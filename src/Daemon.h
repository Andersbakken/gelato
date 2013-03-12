#ifndef Daemon_h
#define Daemon_h

#include <rct/SocketClient.h>
#include <rct/SocketServer.h>
#include <rct/Connection.h>
#include <rct/Process.h>
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
private:
    void onLocalClientConnected();
    void onLocalConnectionDisconnected(Connection *connection);
    void onNewLocalMessage(Message *message, Connection *connection);
    void startJob(Connection *conn, const Job &job);
    bool createCompiler(CompilerMessage *message);
    void onProcessFinished(Process *process);
    void onProcessReadyReadStdOut(Process *process);
    void onProcessReadyReadStdErr(Process *process);
    void onMulticastData(SocketClient *, String host, uint16_t port, String data);
    void onTcpClientConnected();
    void onTcpConnectionDisconnected(Connection *connection);
    void onNewTcpMessage(Message *message, Connection *connection);

    enum { ConnectionPointer = 1, CompilerPath };
    SocketServer mLocalServer, mTcpServer;
    SocketClient mMulticastServer;
    struct ConnectionData {
        Process process;
        String stdOut, stdErr;
        Job job;
    };
    Map<Connection*, ConnectionData> mConnections;
    List<std::pair<Connection*, Job> > mPendingJobs;
    struct Compiler {
        String sha256;
        Set<Path> files;
        // flags, 32-bit, arch
    };
    Map<Path, Compiler> mCompilers;
    List<String> mEnviron;
    Map<String, Connection*> mDaemons;
    int mMaxJobs;

    signalslot::Signal0 mJobFinished;
};

#endif
