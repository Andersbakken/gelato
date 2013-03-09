#include "Daemon.h"
#include <rct/LocalClient.h>
#include <rct/Config.h>
#include <rct/Messages.h>
#include "Job.h"

Daemon::Daemon()
    : mLastId(0)
{
    mLocalServer.clientConnected().connect(this, &Daemon::onClientConnected);
}

void Daemon::onClientConnected()
{
    LocalClient *client = mLocalServer.nextClient();
    assert(client);
    client->disconnected().connect(this, &Daemon::onClientDisconnected);
    Conn conn = { client, new Connection(client) };
    conn.connection->newMessage().connect(this, &Daemon::onNewMessage);

    mClients[++mLastId] = conn;
}

bool Daemon::init()
{
    Messages::registerMessage<Job>();
    return mLocalServer.listen(Config::value<String>("socket-name"));
}

void Daemon::onClientDisconnected(LocalClient *client)
{
    int id = client->id().toInteger();
    Conn conn = { 0, 0 };
    if (mClients.remove(id, &conn)) {
        delete conn.connection;
    }
    delete client;
}

void Daemon::onNewMessage(Message *message, Connection *conn)
{
    
}
