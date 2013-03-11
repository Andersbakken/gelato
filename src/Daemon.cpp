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
    SocketClient *client = mSocketServer.nextClient();
    assert(client);
    Connection *conn = new Connection(client);
    conn->disconnected().connect(this, &Daemon::onConnectionDisconnected);
    mConnections[conn] = ConnectionData();
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
    switch (message->messageId()) {
    case Job::MessageId: {
        Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
        if (it == mConnections.end()) {
            conn->finish();
            return;
        }
        it->second.job = static_cast<Job*>(message);
        break;
    }
}

void Daemon::onConnectionDisconnected(Connection *conn)
{
    mConnections.remove(conn);
    delete conn;
}

void Daemon::handleJob(Job *job, Connection *conn)
{
    Response response(Response::NoDaemons, "No daemons available");
    error() << "Got job" << job->arguments();
    conn->send(&response);
    conn->finish();
}
