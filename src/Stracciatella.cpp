#include <rct/Rct.h>
#include <rct/SocketClient.h>
#include <rct/Connection.h>
#include <rct/Config.h>
#include <rct/Messages.h>
#include <rct/StopWatch.h>
#include "Job.h"
#include "Response.h"
#include "Common.h"
#include <errno.h>

class Conn : public Connection
{
public:
    Conn(int timeout)
        : status(Response::Timeout)
    {
        newMessage().connect(this, &Conn::onNewMessage);
    }

    void onNewMessage(Message *message, Connection *)
    {
        assert(message->messageId() == Response::MessageId);
        Response *response = static_cast<Response*>(message);
        status = response->status();
        errorText = response->errorText();
        stdOut = response->stdOut();
        stdErr = response->stdErr();
        client()->disconnect();
        EventLoop::instance()->exit();
    }

    void timerEvent(TimerEvent *)
    {
        EventLoop::instance()->exit();
    }

    Response::Status status;
    String stdOut, stdErr, errorText;
};

static inline int send(Job *job, bool *ok)
{
    StopWatch watch;
    const int timeout = Config::value<int>("timeout");
    const int connectTimeout = std::min(timeout, Config::value<int>("connect-timeout"));
    shared_ptr<Conn> connection(new Conn(timeout));
    connection->startTimer(timeout, EventReceiver::SingleShot);

    if (!connection->connectToServer(Config::value<String>("socket-name"), connectTimeout)) {
        return false;
    }
    job->setTimeout(timeout - watch.elapsed());
    registerMessages();
    printf("[%s] %s:%d: if (!connection->send(job)) { [before]\n", __func__, __FILE__, __LINE__);
    if (!connection->send(job)) {
        printf("[%s] %s:%d: if (!connection->send(job)) { [after]\n", __func__, __FILE__, __LINE__);
        return false;
    }
    printf("[%s] %s:%d: while (connection->isConnected()) { [before]\n", __func__, __FILE__, __LINE__);
    while (connection->isConnected()) {
        printf("[%s] %s:%d: while (connection->isConnected()) { [after]\n", __func__, __FILE__, __LINE__);
        EventLoop::instance()->run();
    }
    if (!connection->stdOut.isEmpty())
        fprintf(stdout, "%s", connection->stdOut.constData());
    if (!connection->stdErr.isEmpty())
        fprintf(stderr, "%s", connection->stdErr.constData());
    if (ok)
        *ok = connection->status == Response::Success;
    return -1;
}

int main(int argc, char **argv)
{
    Rct::findExecutablePath(argv[0]);
    Config::registerOption<String>("socket-name", "Unix domain socket to use.", 's', Path::home() + ".gelato");
    Config::registerOption("help", "Display this help", 'h');
    Config::registerOption("version", "Display version", 'v');
    Config::registerOption("connect-timeout", "Timeout to connect to daemon", 'c', 1000);
    Config::registerOption("timeout", "Timeout for job to finish", 't', 5000);

    if (Config::isEnabled("h")) {
        Config::showHelp(stdout);
        return 0;
    } else if (Config::isEnabled("V")) {
        printf("Stracciatella version %s", VERSION);
        return 0;
    }

    EventLoop loop;
    Job job;
    if (!job.parse(argc, argv)) {
        String args;
        for (int i=0; i<argc; ++i) {
            if (i)
                args.append(' ');
            args.append(argv[i]);
        }

        error("Can't parse arguments: [%s]", args.constData());
        return 1;
    }

    bool localJob = job.type() != Job::Compile;
    if (!localJob) {
        char *disabled = getenv("STRACCIATELLA_DISABLED");
        if (disabled && !strcmp(disabled, "1")) {
            localJob = true;
        }
    }
    if (!localJob) {
        bool ok;
        printf("[%s] %s:%d: int ret = send(&job, &ok); [before]\n", __func__, __FILE__, __LINE__);
        int ret = send(&job, &ok);
        printf("[%s] %s:%d: int ret = send(&job, &ok); [after]\n", __func__, __FILE__, __LINE__);
        if (ok)
            return ret;
    }
        
    return job.execute();
}
