#ifndef Daemon_h
#define Daemon_h

#include <rct/SocketServer.h>
#include <rct/Connection.h>
#include <rct/Process.h>

class Job;
class Daemon
{
public:
    Daemon();
    bool init();

private:
    void onClientConnected();
    void onConnectionDisconnected(Connection *connection);
    void onNewMessage(Message *message, Connection *connection);
    void onProcessFinished();

    SocketServer mSocketServer;
    struct ConnectionData {
        ConnectionData() : job(0) {}
        shared_ptr<Process> process;
        Job *job;
    };
    Map<Connection*, ConnectionData> mConnections;
};

#endif
