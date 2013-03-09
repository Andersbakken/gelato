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
    void onClientDisconnected(LocalClient *client);
    void onNewMessage(Message *message, Connection *connection);

    LocalServer mLocalServer;
    struct Conn
    {
        LocalClient *client;
        Connection *connection;
    };
    Map<int, Conn> mClients;
    int mLastId;
};

#endif
