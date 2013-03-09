#include "Daemon.h"
#include <rct/LocalClient.h>
#include <rct/Config.h>
#include <rct/Messages.h>
#include "Job.h"

Daemon::Daemon()
{
    mLocalServer.clientConnected().connect(this, &Daemon::onClientConnected);
}

void Daemon::onClientConnected()
{
    LocalClient *client = mLocalServer.nextClient();
    assert(client);
    Connection *conn = new Connection(client);
    conn->destroyed().connect(this, &Daemon::onConnectionDestroyed);
    mConnections.insert(conn);
    conn->newMessage().connect(this, &Daemon::onNewMessage);
}

bool Daemon::init()
{
    Messages::registerMessage<Job>();
    return mLocalServer.listen(Config::value<String>("socket-name"));
}

void Daemon::onNewMessage(Message *message, Connection *conn)
{
    
}

void Daemon::onConnectionDestroyed(Connection *conn)
{

}
