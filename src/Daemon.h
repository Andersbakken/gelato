#ifndef Daemon_h
#define Daemon_h

#include <rct/SocketClient.h>
#include <rct/SocketServer.h>
#include <rct/Connection.h>
#include <rct/Process.h>
#include "JobMessage.h"

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
    void startJob(Connection *conn, const JobMessage &job);
    bool createCompiler(CompilerMessage *message);
    void onProcessFinished(Process *process);
    void onProcessReadyReadStdOut(Process *process);
    void onProcessReadyReadStdErr(Process *process);
    void onMulticastData(SocketClient *, String host, uint16_t port, String data);
    void onTcpClientConnected();
    void onTcpConnectionDisconnected(Connection *connection);

    void announceJobs();
    void requestCompiler(Connection*, const String& sha256);
    void requestJob(Connection*, const String& sha256);
    Connection* connection(const String& host, uint16_t port);

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
        Connection* conn;
        JobMessage job;
    };
    Map<String, List<PendingJob> > mPendingJobs;

    struct Compiler {
        String sha256;
        Set<Path> files;
        // flags, 32-bit, arch
    };
    Map<Path, Compiler> mCompilers;
    Map<String, Path> mShaToCompiler;
    List<String> mEnviron;

    struct RemoteData {
        Connection* conn;
    };

    Map<String, RemoteData> mDaemons;
    int mMaxJobs;

    Timer mAnnounceTimer;

    signalslot::Signal0 mJobFinished;

    static Daemon *sInstance;
};

#endif
