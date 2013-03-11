#include "Daemon.h"
#include <rct/SocketClient.h>
#include <rct/Config.h>
#include <rct/Messages.h>
#include <rct/Path.h>
#include "Job.h"
#include "Common.h"
#include "Result.h"
#include "GelatoMessage.h"
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
}

Daemon::~Daemon()
{
    Path::rm(socketFile);
}

void Daemon::onTcpClientConnected()
{

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
    if (!mCompilers.contains(job->compiler())) {
        mCompilers[job->compiler()]; // make sure we only get one Process for this. Maybe blocking?
        Process *process = new Process;
        process->setData(CompilerPath, job->compiler());
        process->finished().connect(this, &Daemon::onCompilerInfoProcessFinished);
        process->start(job->compiler(), List<String>() << "-v" << "-E" << "-");
        process->closeStdIn();
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

void Daemon::onCompilerInfoProcessFinished(Process *process)
{
    Compiler &info = mCompilers[process->data(CompilerPath).toString()];

    const List<String> lines = process->readAllStdErr().split('\n');
    for (int i=0; i<lines.size(); ++i) {
        const String &line = lines.at(i);
        if (line.startsWith("COMPILER_PATH=")) {
            const Set<String> paths = line.mid(14).split(':').toSet();
            for (Set<String>::const_iterator it = paths.begin(); it != paths.end(); ++it) {
                const Path p = Path::resolved(*it);
                if (p.isDir()) {
                    p.visit(visitor, &info.files);
                }
            }
        }
    }
    error() << info.files;

    process->deleteLater();
}
