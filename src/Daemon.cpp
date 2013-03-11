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
    case Job::MessageId:
        handleJob(static_cast<Job*>(message), conn);
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
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    if (it == mConnections.end()) {
        conn->finish();
        return;
    }
    ConnectionData &data = it->second;
    data.job = job;
    data.process.setData(ConnectionPointer, conn);
    data.process.finished().connect(this, &Daemon::onProcessFinished);
    data.process.readyReadStdOut().connect(this, &Daemon::onReadyReadStdOut);
    data.process.readyReadStdErr().connect(this, &Daemon::onReadyReadStdErr);
    if (!data.process.start(data.job->compiler(), data.job->arguments())) {
        Response response(Response::CompilerMissing, "Couldn't find compiler: " + data.job->compiler());
        conn->send(&response);
        conn->finish();
        return;
    }
}

void Daemon::onProcessFinished(Process *process)
{
    Connection *conn = static_cast<Connection*>(process->data(ConnectionPointer).toPointer());
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    assert(it != mConnections.end());
    ConnectionData &data = it->second;

}

void Daemon::onReadyReadStdOut(Process *process)
{
    Connection *conn = static_cast<Connection*>(process->data(ConnectionPointer).toPointer());
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    assert(it != mConnections.end());
    ConnectionData &data = it->second;
    data.stdOut += process->readAllStdOut();
}

void Daemon::onReadyReadStdErr(Process *process)
{
    Connection *conn = static_cast<Connection*>(process->data(ConnectionPointer).toPointer());
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    assert(it != mConnections.end());
    ConnectionData &data = it->second;
    data.stdOut += process->readAllStdErr();
}
