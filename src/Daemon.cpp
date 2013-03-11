#include "Daemon.h"
#include <rct/SocketClient.h>
#include <rct/Config.h>
#include <rct/Messages.h>
#include <rct/Path.h>
#include "Job.h"
#include "Common.h"
#include "Result.h"
#include "GelatoMessage.h"
#include <signal.h>

Path socketFile;
static void sigIntHandler(int)
{
    Path::rm(socketFile);
    _exit(1);
}

Daemon::Daemon()
{
    signal(SIGINT, sigIntHandler);
    mSocketServer.clientConnected().connect(this, &Daemon::onClientConnected);
}

Daemon::~Daemon()
{
    Path::rm(socketFile);
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
    socketFile = Config::value<String>("socket-name");
    if (mSocketServer.listenUnix(socketFile))
        return true;
    if (socketFile.exists()) {
        Connection connection;
        if (!connection.connectToServer(socketFile, 1000)) {
            Path::rm(socketFile);
            return mSocketServer.listenUnix(socketFile);
        }

        GelatoMessage msg(GelatoMessage::Quit);
        connection.send(&msg);
        EventLoop::instance()->run(500);
        for (int i=0; i<5; ++i) {
            usleep(100 * 1000);
            if (mSocketServer.listenUnix(socketFile)) {
                return true;
            }
        }

    }
    return false;
}

void Daemon::onNewMessage(Message *message, Connection *conn)
{
    warning() << "onNewMessage" << message->messageId();
    switch (message->messageId()) {
    case Job::MessageId:
        handleJob(static_cast<Job*>(message), conn);
        break;
    case GelatoMessage::MessageId:
        switch (static_cast<GelatoMessage*>(message)->type()) {
        case GelatoMessage::Quit:
            EventLoop::instance()->exit();
            break;
        case GelatoMessage::Invalid:
            break;
        }
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
    data.job = *job;
    data.process.setData(ConnectionPointer, conn);
    data.process.setCwd(job->cwd());
    data.process.finished().connect(this, &Daemon::onProcessFinished);
    data.process.readyReadStdOut().connect(this, &Daemon::onReadyReadStdOut);
    data.process.readyReadStdErr().connect(this, &Daemon::onReadyReadStdErr);
    if (!data.process.start(data.job.compiler(), data.job.arguments())) {
        Result response(Result::CompilerMissing, "Couldn't find compiler: " + data.job.compiler());
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
    warning() << "Finished job" << data.job.compiler() << String::join(data.job.arguments(), " ")
              << process->returnCode() << '\n'
              << data.stdOut << '\n' << data.stdErr;
    Result response(process->returnCode(), data.stdOut, data.stdErr);
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
