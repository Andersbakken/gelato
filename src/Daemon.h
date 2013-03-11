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
    bool init();

private:
    void onClientConnected();
    void onConnectionDisconnected(Connection *connection);
    void onNewMessage(Message *message, Connection *connection);
    void handleJob(Job *job, Connection *conn);
    void onProcessFinished(Process *process);
    void onReadyReadStdOut(Process *process);
    void onReadyReadStdErr(Process *process);

    enum { ConnectionPointer = 1 };
    SocketServer mSocketServer;
    struct ConnectionData {
        Process process;
        String stdOut, stdErr;
        Job job;
    };
    Map<Connection*, ConnectionData> mConnections;
};

#endif
