#ifndef Daemon_h
#define Daemon_h

#include <rct/SocketServer.h>
#include <rct/Connection.h>

class Job;
class Daemon
{
public:
    Daemon();
    bool init();

private:
    void onClientConnected();
    void onConnectionDestroyed(Connection *connection);
    void onNewMessage(Message *message, Connection *connection);
    void handleJob(Job *job, Connection *conn);

    SocketServer mSocketServer;
    Set<Connection*> mConnections;
};

#endif
