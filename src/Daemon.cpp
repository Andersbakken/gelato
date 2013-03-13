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
Daemon *Daemon::sInstance = 0;

Path socketFile;
static void sigIntHandler(int)
{
    Path::rm(socketFile);
    _exit(1);
}

Daemon::Daemon()
    : mPreprocessing(0)
{
    assert(!sInstance);
    sInstance = 0;
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
    assert(sInstance == this);
    sInstance = 0;
    Path::rm(socketFile);
}

void Daemon::onMulticastData(SocketClient *, String host, uint16_t port, String data)
{
    error() << "got multicast data" << host << port << data;
    Deserializer deserializer(data.data(), data.size());
    uint16_t tcpPort;
    deserializer >> tcpPort;
    while (!deserializer.atEnd()) {
        String sha256;
        int count;
        deserializer >> sha256 >> count;
        error() << "got sha" << sha256 << "with count" << count << "for port" << tcpPort;

        // check if I have the compiler
        const Map<String, Path>::const_iterator compiler = mShaToCompiler.find(sha256);
        if (compiler == mShaToCompiler.end()) { // no, request it
            Connection* conn = connection(host, tcpPort);
            if (conn)
                requestCompiler(conn, sha256);
        } else if (mLocalJobs.size() < mMaxJobs) {
            // I have room for more jobs
            const int numJobs = std::max(count, mMaxJobs - mLocalJobs.size());
#warning reserve placeholder jobs in mLocalJobs
            Connection* conn = connection(host, tcpPort);
            if (conn)
                requestJobs(conn, sha256, numJobs);
        }
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

void Daemon::requestJobs(Connection* conn, const String& sha256, int count)
{
    // ### should probably use a more efficient message here
    const String data = sha256 + ":" + String::number(count);
    GelatoMessage msg(GelatoMessage::JobRequest, data);
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
    mMaxPreprocess = Config::value<int>("preprocess");
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
        case GelatoMessage::Kill:
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
    if (!mPendingJobs.isEmpty() && mLocalJobs.size() < mMaxJobs) {
        JobInfo::JobList::iterator jobentry = mPendingJobs.begin();
        Job& job = jobentry->first;
        LinkedList<JobInfo>::iterator& info = jobentry->second;
        assert(info->type == Job::Pending);
        job.startLocal();
        info->entry = mLocalJobs.insert(mLocalJobs.end(), std::make_pair(job, info));
        info->type = Job::Local;
        mPendingJobs.erase(jobentry);
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
    enum { DatagramSize = 1400 };

    String data;
    {
        int currentPosition = 0, previousPosition = 0;
        static const uint16_t port = Config::value<int>("daemon-port");

        Serializer serializer(data);
        serializer << port;

        Map<String, int>::const_iterator it = mPreprocessedCount.begin();
        const Map<String, int>::const_iterator end = mPreprocessedCount.end();
        while (it != end) {
            serializer << it->first << it->second;
            ++it;

            if (data.size() - currentPosition >= DatagramSize) {
                // flush
                assert(data.size() - previousPosition < DatagramSize);
                assert(previousPosition > 0);
                writeMulticast(data.left(previousPosition));
                data = data.mid(previousPosition);
                currentPosition = 0;
            }
            previousPosition = currentPosition;
            currentPosition = data.size();
            assert(currentPosition > previousPosition);
            assert(data.size() - currentPosition < DatagramSize);
        }
    }
    if (!data.isEmpty()) {
        writeMulticast(data);
    }
}

void Daemon::onJobFinished(Job* job)
{
}

void Daemon::onJobPreprocessed(Job* job)
{
    Map<int, LinkedList<JobInfo>::iterator>::iterator jobInfo = mIdToJob.find(job->id());
    if (jobInfo == mIdToJob.end()) {
        // already done? throw it away
        job->deleteLater();
        return;
    }
    LinkedList<JobInfo>::iterator& info = jobInfo->second;
    assert(info->type == Job::Pending);
    if (info->type == Job::Pending) {
        ++mPreprocessedCount[job->sha256()];

        Job& job = info->entry->first;
        JobInfo::JobList::iterator newit = mPreprocessed.insert(mPreprocessed.end(), std::make_pair(job, info));
        mPendingJobs.erase(info->entry);
        info->entry = newit;
        info->type = Job::Preprocessed;

        mAnnounceTimer.start(shared_from_this(), 500, SingleShot, Announce);
    }
    if (!mPendingJobs.isEmpty()) {
        // start a new one
        JobInfo::JobList::iterator job = mPendingJobs.begin();
        if (job->first.startPreprocess()) {
            job->first.preprocessed().connect(this, &Daemon::onJobPreprocessed);
        }
    } else {
        --mPreprocessing;
    }

}

void Daemon::startJob(Connection *conn, const JobMessage &jobMessage) // ### need to do load balancing, max jobs etc
{
    warning() << "handleJob" << jobMessage.sourceFile();
    debug() << jobMessage.compiler() << jobMessage.arguments() << jobMessage.path();
    const Path compiler = jobMessage.compiler();
    List<String> environ = mEnviron;
    environ += ("PATH=" + jobMessage.path());

    String sha256;
    const Map<Path, Compiler>::const_iterator compilerEntry = mCompilers.find(compiler);
    if (compilerEntry == mCompilers.end()) {
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
            sha256 = c.sha256 = sha.hash(SHA256::Hex);
            fclose(f);

            mShaToCompiler[c.sha256] = compiler;
        }

        warning() << "Created package" << compiler << c.files << c.sha256;
        // CompilerMessage msg(compiler, c.files, c.sha256);
        // createCompiler(&msg);
    } else {
        sha256 = compilerEntry->second.sha256;
    }

    Job job(jobMessage, sha256, conn);
    job.jobFinished().connect(this, &Daemon::onJobFinished);

    LinkedList<JobInfo>& infos = mShaToJob[sha256];
    if (mLocalJobs.size() >= mMaxJobs) {
        JobInfo info = { Job::Pending };
        LinkedList<JobInfo>::iterator infoEntry = infos.insert(infos.end(), info);
        infoEntry->entry = mPendingJobs.insert(mPendingJobs.end(), std::make_pair(job, infoEntry));
        mIdToJob[job.id()] = infoEntry;

        if (mPreprocessing < mMaxPreprocess) {
            if (infoEntry->entry->first.startPreprocess()) {
                infoEntry->entry->first.preprocessed().connect(this, &Daemon::onJobPreprocessed);
                ++mPreprocessing;
            }
        }

        mAnnounceTimer.start(shared_from_this(), 500, SingleShot, Announce);
        return;
    }

    // Local job
    JobInfo info = { Job::Local };
    LinkedList<JobInfo>::iterator infoEntry = infos.insert(infos.end(), info);
    infoEntry->entry = mLocalJobs.insert(mLocalJobs.end(), std::make_pair(job, infoEntry));
    infoEntry->entry->first.startLocal();
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

// ### maybe cache this for a little while
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
