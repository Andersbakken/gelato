#include "Daemon.h"
#include <rct/SocketClient.h>
#include <rct/Config.h>
#include <rct/SHA256.h>
#include <rct/Messages.h>
#include <rct/Path.h>
#include <rct/Rct.h>
#include "Job.h"
#include "Common.h"
#include "Result.h"
#include "GelatoMessage.h"
#include "CompilerMessage.h"
#include <signal.h>

Path socketFile;
static void sigIntHandler(int)
{
    Path::rm(socketFile);
    _exit(1);
}

Daemon::Daemon()
{
    signal(SIGINT, sigIntHandler);
    mLocalServer.clientConnected().connect(this, &Daemon::onLocalClientConnected);
    mTcpServer.clientConnected().connect(this, &Daemon::onTcpClientConnected);
    mMulticastServer.dataAvailable().connect(this, &Daemon::onMulticastData);
}

Daemon::~Daemon()
{
    Path::rm(socketFile);
}

void Daemon::onMulticastData(SocketClient *)
{
    error() << "got multicast data" << mMulticastServer.readAll();
}

void Daemon::onTcpClientConnected()
{
    error() << "got tcp client";
}

void Daemon::onLocalClientConnected()
{
    warning() << "onLocalClientConnected";
    SocketClient *client = mLocalServer.nextClient();
    assert(client);
    Connection *conn = new Connection(client);
    conn->disconnected().connect(this, &Daemon::onLocalConnectionDisconnected);
    mConnections[conn] = ConnectionData();
    conn->newMessage().connect(this, &Daemon::onNewLocalMessage);
}

bool Daemon::init()
{
    mEnviron = Process::environment();
    for (int i = 0; i < mEnviron.size(); ++i) {
        if (mEnviron.at(i).startsWith("PATH=")) {
            mEnviron.removeAt(i);
            break;
        }
    }

    if (!mMulticastServer.receiveFrom(Config::value<int>("multicast-port"))) {
        error() << "Can't listen for multicast on" << Config::value<int>("multicast-port");
        return false;
    }
    if (!mMulticastServer.addMulticast(Config::value<String>("multicast-address"))) {
        error() << "Unable to subscribe to" << Config::value<String>("multicast-address");
        return false;
    }
    if (!mTcpServer.listenTcp(Config::value<int>("daemon-port"))) {
        error() << "Can't listen for daemon on" << Config::value<int>("daemon-port");
        return false;
    }

    registerMessages();
    socketFile = Config::value<String>("socket-name");
    if (mLocalServer.listenUnix(socketFile))
        return true;
    if (socketFile.exists()) {
        Connection connection;
        if (!connection.connectToServer(socketFile, 1000)) {
            Path::rm(socketFile);
            return mLocalServer.listenUnix(socketFile);
        }

        GelatoMessage msg(GelatoMessage::Quit);
        connection.send(&msg);
        EventLoop::instance()->run(500);
        for (int i=0; i<5; ++i) {
            usleep(100 * 1000);
            if (mLocalServer.listenUnix(socketFile)) {
                return true;
            }
        }

    }

    return false;
}

void Daemon::onNewLocalMessage(Message *message, Connection *conn)
{
    warning() << "onNewMessage" << message->messageId();
    switch (message->messageId()) {
    case Job::MessageId:
        startJob(static_cast<Job*>(message), conn);
        break;
    case CompilerMessage::MessageId:
        createCompiler(static_cast<CompilerMessage*>(message));
        break;
    case GelatoMessage::MessageId:
        switch (static_cast<GelatoMessage*>(message)->type()) {
        case GelatoMessage::Quit:
            EventLoop::instance()->exit();
            break;
        case GelatoMessage::Stats:
            // ### add me
            break;
        case GelatoMessage::Invalid:
            break;
        }
        break;
    }
}

void Daemon::onLocalConnectionDisconnected(Connection *conn)
{
    warning() << "onConnectionDisconnected";
    mConnections.remove(conn);
    conn->deleteLater();
}

static Path::VisitResult visitor(const Path &path, void *userData)
{
    Set<Path> &files = *reinterpret_cast<Set<Path> *>(userData);
    // if (path.isSymLink()) {
    // error() << path << "isSymLink" << path.isSymLink() << path.isDir();
    if (path.isSymLink()) {
        files.insert(path);
        if (path.isFile()) {
            Path link = path.followLink();
            link.resolve(Path::RealPath, path.parentDir());
            visitor(link, userData);
        }
    } else if (path.isDir()) {
        if (!path.isSymLink()) {
            return Path::Recurse;
        } else {
            files.insert(path);
        }
    } else if (path.mode() & 0111) {
        char *buf = 0;
        const int read = path.readAll(buf, 3);
        if (read != 3 || strncmp(buf, "#!/", read))
            files.insert(path);
        delete[] buf;
    } else if (path.contains(".so")) {
        files.insert(path);
    }
    return Path::Continue;
}

void Daemon::startJob(Job *job, Connection *conn) // ### need to do load balancing, max jobs etc
{
    warning() << "handleJob" << job->compiler() << job->arguments() << job->path();
    ConnectionData &data = mConnections[conn];
    data.job = *job;
    data.process.setData(ConnectionPointer, conn);
    data.process.setCwd(job->cwd());
    data.process.finished().connect(this, &Daemon::onProcessFinished);
    data.process.readyReadStdOut().connect(this, &Daemon::onProcessReadyReadStdOut);
    data.process.readyReadStdErr().connect(this, &Daemon::onProcessReadyReadStdErr);

    List<String> environ = mEnviron;
    environ += ("PATH=" + job->path());

    if (!data.process.start(data.job.compiler(), data.job.arguments(), environ)) {
        const Result response(Result::CompilerMissing, "Couldn't find compiler: " + data.job.compiler());
        conn->send(&response);
        conn->finish();
    }
    const Path compiler = job->compiler();
    if (!mCompilers.contains(compiler)) {
        Compiler &c = mCompilers[compiler];
        Process process;
        process.exec(compiler, List<String>() << "-v" << "-E" << "-", environ);

        const List<String> lines = process.readAllStdErr().split('\n');
        for (int i=0; i<lines.size(); ++i) {
            const String &line = lines.at(i);
            if (line.startsWith("COMPILER_PATH=")) {
                const Set<String> paths = line.mid(14).split(':').toSet();
                for (Set<String>::const_iterator it = paths.begin(); it != paths.end(); ++it) {
                    const Path p = Path::resolved(*it);
                    if (p.isDir()) {
                        p.visit(visitor, &c.files);
                    }
                }
            }
        }
        FILE *f = fopen(compiler.constData(), "r");
        if (f) {
            char buf[1024];
            int r;
            SHA256 sha;
            while ((r = fread(buf, sizeof(char), sizeof(buf), f)) > 0) {
                sha.update(buf, r);
            }
            c.sha256 = sha.hash(SHA256::Hex);
            fclose(f);
        }

        warning() << "package" << compiler << c.files << c.sha256;
        CompilerMessage msg(compiler, c.files, c.sha256);
        createCompiler(&msg);
    }
}

void Daemon::onProcessFinished(Process *process)
{
    Connection *conn = static_cast<Connection*>(process->data(ConnectionPointer).toPointer());
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    assert(it != mConnections.end());
    ConnectionData &data = it->second;
    data.stdOut += process->readAllStdOut();
    data.stdErr += process->readAllStdErr();
    warning() << "Finished job" << data.job.compiler() << String::join(data.job.arguments(), " ")
              << process->returnCode();
    if (!data.stdOut.isEmpty())
        warning() << data.stdOut;
    if (!data.stdErr.isEmpty())
        warning() << data.stdErr;
    Result response(process->returnCode(), data.stdOut, data.stdErr);
    conn->send(&response);
    conn->finish();
}

void Daemon::onProcessReadyReadStdOut(Process *process)
{
    Connection *conn = static_cast<Connection*>(process->data(ConnectionPointer).toPointer());
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    assert(it != mConnections.end());
    ConnectionData &data = it->second;
    data.stdOut += process->readAllStdOut();
}

void Daemon::onProcessReadyReadStdErr(Process *process)
{
    Connection *conn = static_cast<Connection*>(process->data(ConnectionPointer).toPointer());
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    assert(it != mConnections.end());
    ConnectionData &data = it->second;
    data.stdErr += process->readAllStdErr();
}
bool Daemon::createCompiler(CompilerMessage *message)
{
    Path path = String::format<256>("%s/compiler_%s", Rct::executablePath().parentDir().constData(), message->sha256().constData());
    Path::mkdir(path, Path::Recursive);
    const Map<Path, String> &paths = message->paths();

    for (Map<Path, String>::const_iterator it = paths.begin(); it != paths.end(); ++it) {
        Path abs = path + it->first;
        if (!Path::mkdir(abs.parentDir(), Path::Recursive)) {
            error("Couldn't create directory %s", abs.parentDir().constData());
        }
        FILE *f = fopen(abs.constData(), "w");
        if (!f) {
            error("Couldn't create file %s", abs.constData());
            return false;
        }
        if (!fwrite(it->second.constData(), it->second.size(), 1, f)) {
            error() << "Write error" << abs;
        }
        fclose(f);
    }
    return true;
}
