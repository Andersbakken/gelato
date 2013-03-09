#ifndef Daemon_h
#define Daemon_h

#include <rct/LocalServer.h>
#include <rct/Connection.h>

class Daemon
{
public:
    Daemon();
    bool init();

private:
    void onClientConnected();
    void onConnectionDestroyed(Connection *connection);
    void onNewMessage(Message *message, Connection *connection);

    LocalServer mLocalServer;
    Set<Connection*> mConnections;
};

#endif
