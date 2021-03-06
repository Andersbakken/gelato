#include "Daemon.h"
#include <rct/SocketClient.h>
#include <rct/Config.h>
#include <rct/SHA256.h>
#include <rct/Messages.h>
#include <rct/Path.h>
#include <rct/Rct.h>
#include "JobMessage.h"
#include "Common.h"
#include "ResultMessage.h"
#include "GelatoMessage.h"
#include "CompilerMessage.h"
#include <signal.h>

#define DEBUGMULTICAST

static void *Announce = &Announce;
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
    sInstance = this;
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
    while (!deserializer.atEnd()) {
        String sha256;
        Path path;
        int count;
        uint16_t tcpPort;
        deserializer >> sha256 >> path >> count >> tcpPort;
        error() << "got sha" << sha256 << "with count" << count << "for port" << tcpPort;

        compiler(path); // no need to request the compiler if we have the exact
                        // same one. Create our local one first
        // check if I have the compiler
        shared_ptr<Compiler> compiler = mCompilerBySha.value(sha256);
        if (!compiler) {
            Connection *conn = connection(host, tcpPort);
            if (conn)
                requestCompiler(conn, sha256);
        } else if (mLocalJobs.size() < mMaxJobs) {
            // I have room for more jobs
            const int numJobs = std::max(count, mMaxJobs - mLocalJobs.size());
            Connection *conn = connection(host, tcpPort);
            if (conn)
                requestJobs(conn, sha256, numJobs);
        }
    }
}

Connection *Daemon::connection(const String &host, uint16_t port)
{
    const Map<String, RemoteData>::const_iterator it = mDaemons.find(host);
    if (it != mDaemons.end())
        return it->second.conn;
    SocketClient *client = new SocketClient;
    if (client->connectTcp(host, port)) {
        Connection *conn = new Connection(client);
        conn->disconnected().connect(this, &Daemon::onTcpConnectionDisconnected);
        conn->newMessage().connect(this, &Daemon::onNewMessage);
        RemoteData remote = { conn };
        mDaemons[host] = remote;
        return conn;
    }
    delete client;
    return 0;
}

void Daemon::requestJobs(Connection *conn, const String &sha256, int count)
{
    List<Value> values;
    values << sha256 << count;
    GelatoMessage msg(GelatoMessage::JobRequest, values);
    if (conn->send(&msg)) {
#warning add placeholder to localjobs
    }
}

void Daemon::requestCompiler(Connection *conn, const String &sha256)
{
    GelatoMessage msg(GelatoMessage::CompilerRequest, sha256);
    conn->send(&msg);
}

void Daemon::onTcpConnectionDisconnected(Connection *connection)
{
    const String remote = connection->client()->remoteAddress();
    mDaemons.remove(remote);
    connection->deleteLater();
}

void Daemon::onTcpClientConnected()
{
    error() << "got tcp client";
    SocketClient *client;
    while ((client = mTcpServer.nextClient())) {
        const String remote = client->remoteAddress();
        if (mDaemons.contains(remote)) {
            // we already have a connection from this host, ignore
            // ### allow multiple connections (multiple daemons?) from the same host?
            client->deleteLater();
            return;
        }

        Connection *conn = new Connection(client);
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
        handleJobMessage(conn, *static_cast<JobMessage*>(message));
        break;
    case CompilerMessage::MessageId:
        createCompiler(static_cast<CompilerMessage*>(message));
        break;
    case GelatoMessage::MessageId: {
        const GelatoMessage *gelatoMessage = static_cast<GelatoMessage*>(message);
        switch (gelatoMessage->type()) {
        case GelatoMessage::Quit:
            EventLoop::instance()->exit();
            break;
        case GelatoMessage::Stats:
            // ### add me
            break;
        case GelatoMessage::Invalid:
            break;
        case GelatoMessage::CompilerRequest: {
            shared_ptr<Compiler> compiler = mCompilerBySha.value(gelatoMessage->value().toString());
            if (!compiler) {
                // send an empty compiler message
                CompilerMessage msg(gelatoMessage->value().toString());
                conn->send(&msg);
            } else {
                // load up and send the compiler
                CompilerMessage msg(compiler->path, compiler->files, compiler->sha256);
                conn->send(&msg);
            }
            break; }
        case GelatoMessage::JobRequest: {
            const List<Value> values = gelatoMessage->value().toList();
            assert(values.size() == 2);
            const String sha256 = values.at(0).toString();
            const int count = values.at(1).toInteger();
            handleJobRequest(sha256, count);
            break; }
        case GelatoMessage::Kill:
            break;
        }
        break; }
    }
}

void Daemon::onLocalConnectionDisconnected(Connection *conn)
{
    printf("%p FISK\n", conn);
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    if (it != mConnections.end()) {
        error() << "onConnectionDisconnected" << it->second.job.sourceFile();
        mConnections.erase(it);
    } else {
        error() << "onConnectionDisconnected no connection data";
    }

    if (!mPendingJobs.isEmpty() && mLocalJobs.size() < mMaxJobs) {
        JobInfo::JobList::iterator jobentry = mPendingJobs.begin();
        shared_ptr<Job> job = jobentry->first;
        LinkedList<JobInfo>::iterator &info = jobentry->second;
        mPendingJobs.erase(jobentry);
        assert(info->type == Job::Pending);
        startLocalJob(job, info);
    }
}

void Daemon::startLocalJob(const shared_ptr<Job> &job, LinkedList<JobInfo>::iterator info)
{
    info->entry = mLocalJobs.insert(mLocalJobs.end(), std::make_pair(job, info));
    info->type = Job::Local;
    job->jobFinished().connect(this, &Daemon::onLocalJobFinished);
    job->startLocal();
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

void Daemon::writeMulticast(const String &data)
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
        for (Map<String, shared_ptr<Compiler> >::const_iterator it = mCompilerBySha.begin();
             it != mCompilerBySha.end(); ++it) {
            const shared_ptr<Compiler> &compiler = it->second;
            if (compiler->preprocessedCount) {
                serializer << compiler->sha256 << compiler->path << compiler->preprocessedCount << port;

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
    }
    if (!data.isEmpty()) {
        writeMulticast(data);
    }
}

void Daemon::onLocalJobFinished(Job *job)
{
    printf("[%s] %s:%d: void Daemon::onLocalJobFinished(Job *job) [after]\n", __func__, __FILE__, __LINE__);
    if (job->type() == Job::Local) {
        printf("[%s] %s:%d: if (job->type() == Job::Local) { [after]\n", __func__, __FILE__, __LINE__);
        assert(job->source());
        ResultMessage response(job->returnCode(), job->stdOut(), job->stdErr());
        Connection *conn = job->source();
        conn->send(&response);
        conn->finish();
    }
}

void Daemon::onJobPreprocessed(Job *job)
{
    Map<int, LinkedList<JobInfo>::iterator>::iterator jobInfo = mIdToJob.find(job->id());
    if (jobInfo == mIdToJob.end()) {
        // already done? throw it away
        job->deleteLater();
        return;
    }
    LinkedList<JobInfo>::iterator &info = jobInfo->second;
    assert(info->type == Job::Pending);
    if (info->type == Job::Pending) {
        assert(mCompilerBySha.contains(job->sha256()));
        ++mCompilerBySha[job->sha256()]->preprocessedCount;

        shared_ptr<Job> job = info->entry->first;
        JobInfo::JobList::iterator newit = mPreprocessed.insert(mPreprocessed.end(), std::make_pair(job, info));
        mPendingJobs.erase(info->entry);
        info->entry = newit;
        info->type = Job::Preprocessed;

        mAnnounceTimer.start(shared_from_this(), 500, SingleShot, Announce);
    }
    if (!mPendingJobs.isEmpty()) {
        // start a new one
        JobInfo::JobList::iterator job = mPendingJobs.begin();
        if (job->first->startPreprocess()) {
            job->first->preprocessed().connect(this, &Daemon::onJobPreprocessed);
        }
    } else {
        --mPreprocessing;
    }

}

void Daemon::handleJobMessage(Connection *conn, const JobMessage &jobMessage) // ### need to do load balancing, max jobs etc
{
    warning() << "handleJob" << jobMessage.sourceFile();
    debug() << jobMessage.compiler() << jobMessage.arguments() << jobMessage.path();

    shared_ptr<Compiler> c = compiler(jobMessage.compiler(), jobMessage.path());
    if (!c) {
        ResultMessage response(-1, String(), String::format<128>("Invalid compiler %s", jobMessage.compiler().constData()));
        conn->send(&response);
        conn->finish();
        // ### what to do here?
        return;
    }
    shared_ptr<Job> job(new Job(jobMessage, c->sha256, conn));
    job->jobFinished().connect(this, &Daemon::onLocalJobFinished);

    if (mLocalJobs.size() >= mMaxJobs) {
        JobInfo info = { Job::Pending };
        LinkedList<JobInfo>::iterator infoEntry = c->jobs.insert(c->jobs.end(), info);
        infoEntry->entry = mPendingJobs.insert(mPendingJobs.end(), std::make_pair(job, infoEntry));
        mIdToJob[job->id()] = infoEntry;

        if (mPreprocessing < mMaxPreprocess && infoEntry->entry->first->startPreprocess()) {
            infoEntry->entry->first->preprocessed().connect(this, &Daemon::onJobPreprocessed);
            ++mPreprocessing;
        }
    } else {
        // Local job
        JobInfo info = { Job::Local };
        LinkedList<JobInfo>::iterator infoEntry = c->jobs.insert(c->jobs.end(), info);
        startLocalJob(job, infoEntry);
    }
}

// ### maybe cache this for a little while
bool Daemon::createCompiler(CompilerMessage *message)
{
    if (!message->isValid()) {
        error() << "No compiler for" << message->sha256();
        return false;
    }
    Path path = String::format<256>("%s/compiler_%s", Rct::executablePath().parentDir().constData(), message->sha256().constData());
    if (path.isDir() || Path::mkdir(path, Path::Recursive)) {
        message->writeFiles(path);
        return true;
    }
    return false;
}

void Daemon::timerEvent(TimerEvent *e)
{
    if (e->userData() == Announce) {
        announceJobs();
    }
}

void Daemon::handleJobRequest(const String &sha, int count)
{

}

shared_ptr<Daemon::Compiler> Daemon::compiler(const Path &compiler, const Path &path)
{
    shared_ptr<Compiler> &c = mCompilerByPath[compiler];
    if (!c) {
        Process process;
        List<String> environment;
        if (!path.isEmpty()) {
            environment = mEnviron;
            environment += ("PATH=" + path);
        }
        if (!process.exec(compiler, List<String>() << "-v" << "-E" << "-", environment))
            return shared_ptr<Compiler>();

        SHA256 sha;
        const List<String> lines = process.readAllStdErr().split('\n');
        String version;
        Set<String> compilerPaths;
        for (int i=0; i<lines.size(); ++i) {
            const String &line = lines.at(i);
            sha.update(line);

            if (compilerPaths.isEmpty() && line.size() > 14 && line.startsWith("COMPILER_PATH=")) {
                compilerPaths = line.mid(14).split(':').toSet();
            } else if (version.isEmpty() && line.size() > 12 && line.startsWith("gcc version")) {
                int space = line.indexOf(' ', 12);
                if (space == -1)
                    space = line.size();

                version = line.mid(12, space - 12);
                error() << version;
            }
        }

        if (!compilerPaths.isEmpty()) {
            c.reset(new Compiler);
            for (Set<String>::const_iterator it = compilerPaths.begin(); it != compilerPaths.end(); ++it) {
                const Path p = Path::resolved(*it);
                if (p.isDir()) {
                    p.visit(visitor, &c->files);
                }
            }

            c->sha256 = sha.hash(SHA256::Hex);
            c->path = compiler;
            mCompilerBySha[c->sha256] = c;
            warning() << "Created package" << compiler << c->files << c->sha256;
            // CompilerMessage msg(compiler, c.files, c.sha256);
            // createCompiler(&msg);
        }
    }
    return c;
}
