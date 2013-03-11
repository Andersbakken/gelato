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
    warning() << "onClientConnected";
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
    warning() << "onNewMessage" << message->messageId();
    switch (message->messageId()) {
    case Job::MessageId:
        handleJob(static_cast<Job*>(message), conn);
        break;
    }
}

void Daemon::onConnectionDisconnected(Connection *conn)
{
    warning() << "onConnectionDisconnected";
    mConnections.remove(conn);
    conn->deleteLater();
}

void Daemon::handleJob(Job *job, Connection *conn)
{
    warning() << "handleJob" << job->compiler() << job->arguments();
    ConnectionData &data = mConnections[conn];
    data.job = job;
    data.process.setData(ConnectionPointer, conn);
    data.process.setCwd(job->cwd());
    data.process.finished().connect(this, &Daemon::onProcessFinished);
    data.process.readyReadStdOut().connect(this, &Daemon::onReadyReadStdOut);
    data.process.readyReadStdErr().connect(this, &Daemon::onReadyReadStdErr);
    if (!data.process.start(data.job->compiler(), data.job->arguments())) {
        Response response(Response::CompilerMissing, "Couldn't find compiler: " + data.job->compiler());
        conn->send(&response);
        conn->finish();
    }
}

void Daemon::onProcessFinished(Process *process)
{
    Connection *conn = static_cast<Connection*>(process->data(ConnectionPointer).toPointer());
    Map<Connection*, ConnectionData>::iterator it = mConnections.find(conn);
    assert(it != mConnections.end());
    ConnectionData &data = it->second;
    data.stdOut += process->readAllStdOut();
    data.stdErr += process->readAllStdErr();
    Response response(process->returnCode(), data.stdOut, data.stdErr);
    conn->send(&response);
    conn->finish();
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
    data.stdErr += process->readAllStdErr();
}
