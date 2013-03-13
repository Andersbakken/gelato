#include "Daemon.h"
#include <rct/SocketClient.h>
#include <rct/Config.h>
#include <rct/SHA256.h>
#include <rct/Messages.h>
#include <rct/Path.h>
#include <rct/Rct.h>
#include "JobMessage.h"
#include "Common.h"
#include "Result.h"
#include "GelatoMessage.h"
#include "CompilerMessage.h"
#include <signal.h>

#define DEBUGMULTICAST

static void* Announce = &Announce;

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
    mMulticast.udpDataAvailable().connect(this, &Daemon::onMulticastData);
#ifdef DEBUGMULTICAST
    mMulticast.setMulticastLoop();
#endif
}

Daemon::~Daemon()
{
    Path::rm(socketFile);
}

void Daemon::onMulticastData(SocketClient *, String host, uint16_t port, String data)
{
    error() << "got multicast data" << host << port << data;
    Deserializer deserializer(data.data(), data.size());
    String sha256;
    uint16_t tcpPort;
    deserializer >> sha256 >> tcpPort;
    error() << "got sha" << sha256 << tcpPort;

    // check if I have the compiler
    const Map<String, Path>::const_iterator compiler = mShaToCompiler.find(sha256);
    if (compiler == mShaToCompiler.end()) { // no, request it
        Connection* conn = connection(host, tcpPort);
        if (conn)
            requestCompiler(conn, sha256);
    } else if (mConnections.size() < mMaxJobs) {
        // yes, check if I have room to run a job
        Connection* conn = connection(host, tcpPort);
        if (conn)
            requestJob(conn, sha256);
    }
}

Connection* Daemon::connection(const String& host, uint16_t port)
{
    const Map<String, RemoteData>::const_iterator it = mDaemons.find(host);
    if (it != mDaemons.end())
        return it->second.conn;
    SocketClient* client = new SocketClient;
    if (client->connectTcp(host, port)) {
        Connection* conn = new Connection(client);
        conn->disconnected().connect(this, &Daemon::onTcpConnectionDisconnected);
        conn->newMessage().connect(this, &Daemon::onNewMessage);
        RemoteData remote = { conn };
        mDaemons[host] = remote;
        return conn;
    }
    delete client;
    return 0;
}

void Daemon::requestJob(Connection* conn, const String& sha256)
{
    GelatoMessage msg(GelatoMessage::JobRequest, sha256);
    conn->send(&msg);
}

void Daemon::requestCompiler(Connection* conn, const String& sha256)
{
    GelatoMessage msg(GelatoMessage::CompilerRequest, sha256);
    conn->send(&msg);
}

void Daemon::onTcpConnectionDisconnected(Connection* connection)
{
    const String remote = connection->client()->remoteAddress();
    mDaemons.remove(remote);
    connection->deleteLater();
}

void Daemon::onTcpClientConnected()
{
    error() << "got tcp client";
    SocketClient* client;
    while ((client = mTcpServer.nextClient())) {
        const String remote = client->remoteAddress();
        if (mDaemons.contains(remote)) {
            // we already have a connection from this host, ignore
            // ### allow multiple connections (multiple daemons?) from the same host?
            client->deleteLater();
            return;
        }

        Connection* conn = new Connection(client);
        conn->disconnected().connect(this, &Daemon::onTcpConnectionDisconnected);
        conn->newMessage().connect(this, &Daemon::onNewMessage);
        RemoteData remoteData = { conn };
        mDaemons[remote] = remoteData;
    }
}

void Daemon::onLocalClientConnected()
{
    while (SocketClient *client = mLocalServer.nextClient()) {
        warning() << "onLocalClientConnected";
        assert(client);
        Connection *conn = new Connection(client);
        conn->disconnected().connect(this, &Daemon::onLocalConnectionDisconnected);
        conn->newMessage().connect(this, &Daemon::onNewMessage);
    }
}

bool Daemon::init()
{
    mMaxJobs = Config::value<int>("jobs");
    error() << "Running with" << mMaxJobs << "jobs";
    mEnviron = Process::environment();
    for (int i = 0; i < mEnviron.size(); ++i) {
        if (mEnviron.at(i).startsWith("PATH=")) {
            mEnviron.removeAt(i);
            break;
        }
    }

    if (!mMulticast.receiveFrom(Config::value<int>("multicast-port"))) {
        error() << "Can't listen for multicast on" << Config::value<int>("multicast-port");
        return false;
    }
    if (!mMulticast.addMulticast(Config::value<String>("multicast-address"))) {
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

void Daemon::onNewMessage(Message *message, Connection *conn)
{
    warning() << "onNewMessage" << message->messageId();
    switch (message->messageId()) {
    case JobMessage::MessageId:
        startJob(conn, *static_cast<JobMessage*>(message));
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
        case GelatoMessage::CompilerRequest:
            break;
        case GelatoMessage::JobRequest:
            break;
        }
        break;
    }
}

void Daemon::onLocalConnectionDisconnected(Connection *conn)
{
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    if (it != mConnections.end()) {
        error() << "onConnectionDisconnected" << it->second.job.sourceFile();
        mConnections.erase(it);
    } else {
        error() << "onConnectionDisconnected no connection data";
    }

    conn->deleteLater();
    if (!mPendingJobs.isEmpty() && mConnections.size() < mMaxJobs) {
#warning fixme
        //        List<std::pair<Connection*, JobMessage> >::iterator it = mPendingJobs.begin();
        //startJob((*it).first, (*it).second);
        //mPendingJobs.erase(it);
    }
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

void Daemon::writeMulticast(const String& data)
{
    static String addr = Config::value<String>("multicast-address");
    static uint16_t port = Config::value<int>("multicast-port");
    mMulticast.writeTo(addr, port, data);
}

void Daemon::announceJobs()
{
    String data;
    {
#warning fix me as well
        /*
        const Map<Path, Compiler>::const_iterator it = mCompilers.find(job.compiler());
        if (it == mCompilers.end())
            return;
        static const uint16_t port = Config::value<int>("daemon-port");
        Serializer serializer(data);
        serializer << it->second.sha256 << port;
        */
    }
    writeMulticast(data);
}

void Daemon::startJob(Connection *conn, const JobMessage &job) // ### need to do load balancing, max jobs etc
{
    warning() << "handleJob" << job.sourceFile();
    debug() << job.compiler() << job.arguments() << job.path();
    const Path compiler = job.compiler();
    List<String> environ = mEnviron;
    environ += ("PATH=" + job.path());

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

            mShaToCompiler[c.sha256] = compiler;
        }

        warning() << "Created package" << compiler << c.files << c.sha256;
        // CompilerMessage msg(compiler, c.files, c.sha256);
        // createCompiler(&msg);
    }

    if (mConnections.size() >= mMaxJobs) {
#warning and fix me here too
        //mPendingJobs.append(std::make_pair(conn, job));
        mAnnounceTimer.start(shared_from_this(), 500, SingleShot, Announce);
        return;
    }
    ConnectionData &data = mConnections[conn];
    data.job = job;
    data.process.setData(ConnectionPointer, conn);
    data.process.setCwd(job.cwd());
    data.process.finished().connect(this, &Daemon::onProcessFinished);
    data.process.readyReadStdOut().connect(this, &Daemon::onProcessReadyReadStdOut);
    data.process.readyReadStdErr().connect(this, &Daemon::onProcessReadyReadStdErr);

    if (!data.process.start(job.compiler(), job.arguments(), environ)) {
        const Result response(Result::CompilerMissing, "Couldn't find compiler: " + data.job.compiler());
        conn->send(&response);
        conn->finish();
    } else {
        error() << "Compiling" << job.sourceFile();
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
    error() << "finished" << data.job.sourceFile();
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

void Daemon::timerEvent(TimerEvent *e)
{
    if (e->userData() == Announce) {
        announceJobs();
    }
}
