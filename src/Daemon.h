#ifndef Daemon_h
#define Daemon_h

#include <rct/SocketServer.h>
#include <rct/Connection.h>
#include <rct/Process.h>
#include "Job.h"

class Daemon
{
public:
    Daemon();
    ~Daemon();
    bool init();

private:
    void onLocalClientConnected();
    void onLocalConnectionDisconnected(Connection *connection);
    void onNewLocalMessage(Message *message, Connection *connection);
    void startJob(Job *job, Connection *conn);
    void onProcessFinished(Process *process);
    void onProcessReadyReadStdOut(Process *process);
    void onProcessReadyReadStdErr(Process *process);

    void onTcpClientConnected();

    enum { ConnectionPointer = 1 };
    SocketServer mLocalServer, mTcpServer;
    struct ConnectionData {
        Process process;
        String stdOut, stdErr;
        Job job;
    };
    Map<Connection*, ConnectionData> mConnections;
};

#endif
