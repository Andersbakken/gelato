#include "Daemon.h"
#include <rct/SocketClient.h>
#include <rct/Config.h>
#include <rct/Messages.h>
#include <rct/Path.h>
#include "Job.h"
#include "Common.h"
#include "Response.h"

Daemon::Daemon()
{
    mSocketServer.clientConnected().connect(this, &Daemon::onClientConnected);
}

void Daemon::onClientConnected()
{
    printf("[%s] %s:%d: void Daemon::onClientConnected() [after]\n", __func__, __FILE__, __LINE__);
    SocketClient *client = mSocketServer.nextClient();
    assert(client);
    Connection *conn = new Connection(client);
    conn->disconnected().connect(this, &Daemon::onConnectionDisconnected);
    mConnections.insert(conn);
    conn->newMessage().connect(this, &Daemon::onNewMessage);
}

bool Daemon::init()
{
    registerMessages();
    const Path file = Config::value<String>("socket-name");
    if (mSocketServer.listenUnix(file))
        return true;
    if (file.exists()) {
        Path::rm(file);
        return mSocketServer.listenUnix(file);
    }
    // ### should send a quit command and shit
    return false;
}

void Daemon::onNewMessage(Message *message, Connection *conn)
{
    printf("[%s] %s:%d: void Daemon::onNewMessage(Message *message, Connection *conn) [after]\n", __func__, __FILE__, __LINE__);
    switch (message->messageId()) {
    case Job::MessageId:
        handleJob(static_cast<Job*>(message), conn);
        break;
    }
}

void Daemon::onConnectionDisconnected(Connection *conn)
{
    mConnections.remove(conn);
    delete conn;
    printf("[%s] %s:%d: void Daemon::onConnectionDestroyed(Connection *conn) [after]\n", __func__, __FILE__, __LINE__);
}

void Daemon::handleJob(Job *job, Connection *conn)
{
    printf("[%s] %s:%d: void Daemon::handleJob(Job *job, Connection *conn) [after]\n", __func__, __FILE__, __LINE__);
    Response response(Response::NoDaemons, "No daemons available");
    error() << "Got job" << job->arguments();
    conn->send(&response);
    conn->finish();
}
