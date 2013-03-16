#ifndef Daemon_h
#define Daemon_h

#include <rct/SocketClient.h>
#include <rct/SocketServer.h>
#include <rct/Connection.h>
#include <rct/Process.h>
#include <rct/LinkedList.h>
#include "JobMessage.h"
#include "Job.h"

class CompilerMessage;
class Daemon : public EventReceiver
{
public:
    Daemon();
    ~Daemon();
    bool init();

    signalslot::Signal0 &jobFinished() { return mJobFinished; }
    void timerEvent(TimerEvent *e);

    static Daemon *instance() { return sInstance; }
    List<String> defaultEnvironment() const { return mEnviron; }
private:
    void onLocalClientConnected();
    void onLocalConnectionDisconnected(Connection *connection);
    void onNewMessage(Message *message, Connection *connection);
    void handleJobMessage(Connection *conn, const JobMessage &job);
    struct JobInfo;
    void startLocalJob(const shared_ptr<Job> &job, LinkedList<JobInfo>::iterator it);
    void handleJobRequest(const String &sha, int count);
    bool createCompiler(CompilerMessage *message);
    void onMulticastData(SocketClient *, String host, uint16_t port, String data);
    void onTcpClientConnected();
    void onTcpConnectionDisconnected(Connection *connection);
    void onLocalJobFinished(Job *job);
    void onJobPreprocessed(Job *job);
    struct Compiler;
    shared_ptr<Compiler> compiler(const Path &compiler, const Path &pathEnv = Path());

    void announceJobs();
    void requestCompiler(Connection*, const String& sha256);
    void requestJobs(Connection*, const String& sha256, int count);
    Connection *connection(const String& host, uint16_t port);

    void writeMulticast(const String& data);

    enum { ConnectionPointer = 1, CompilerPath };
    SocketServer mLocalServer, mTcpServer;
    SocketClient mMulticast;
    struct ConnectionData {
        Process process;
        String stdOut, stdErr;
        JobMessage job;
    };
    Map<Connection*, ConnectionData> mConnections;

    struct PendingJob {
        Connection *conn;
        JobMessage job;
    };

    struct JobInfo
    {
        typedef LinkedList<std::pair<shared_ptr<Job>, LinkedList<JobInfo>::iterator> > JobList;

        Job::Type type;
        JobList::iterator entry;
    };

    JobInfo::JobList mPendingJobs, mPreprocessed, mLocalJobs, mRemoteJobs;
    Map<int, LinkedList<JobInfo>::iterator> mIdToJob;
    int mPreprocessing;

    struct Compiler {
        Compiler() : preprocessedCount(0) {}
        String sha256;
        Path path;
        Set<Path> files;
        LinkedList<JobInfo> jobs;
        int preprocessedCount;
        // flags, 32-bit, arch
    };
    Map<Path, shared_ptr<Compiler> > mCompilerByPath;
    Map<String, shared_ptr<Compiler> > mCompilerBySha;
    List<String> mEnviron;

    struct RemoteData {
        Connection *conn;
    };

    Map<String, RemoteData> mDaemons;
    int mMaxJobs, mMaxPreprocess;

    Timer mAnnounceTimer;

    signalslot::Signal0 mJobFinished;

    static Daemon *sInstance;
};

#endif
