#include <rct/Rct.h>
#include <rct/SocketClient.h>
#include <rct/Connection.h>
#include <rct/Config.h>
#include <rct/Messages.h>
#include <rct/StopWatch.h>
#include "JobMessage.h"
#include "ResultMessage.h"
#include "Common.h"
#include <errno.h>

class Conn : public Connection
{
public:
    Conn(int timeout)
        : status(ResultMessage::Timeout), returnValue(-1)
    {
        newMessage().connect(this, &Conn::onNewMessage);
    }

    void onNewMessage(Message *message, Connection *)
    {
        assert(message->messageId() == ResultMessage::MessageId);
        ResultMessage *response = static_cast<ResultMessage*>(message);
        status = response->status();
        errorText = response->errorText();
        stdOut = response->stdOut();
        stdErr = response->stdErr();
        client()->disconnect();
        returnValue = response->returnValue();
        EventLoop::instance()->exit();
    }

    void timerEvent(TimerEvent *)
    {
        EventLoop::instance()->exit();
    }

    ResultMessage::Status status;
    String stdOut, stdErr, errorText;
    int returnValue;
};

static inline bool send(JobMessage *job, int &returnCode)
{
    returnCode = -1;
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
    if (!connection->send(job)) {
        return false;
    }
    while (connection->isConnected()) {
        // ### timeout here?
        EventLoop::instance()->run();
    }
    if (!connection->stdOut.isEmpty())
        fprintf(stdout, "%s", connection->stdOut.constData());
    if (!connection->stdErr.isEmpty())
        fprintf(stderr, "%s", connection->stdErr.constData());
    returnCode = connection->returnValue;
    return connection->status == ResultMessage::Success;
}

int main(int argc, char **argv)
{
    Rct::findExecutablePath(argv[0]);
    Config::registerOption<String>("socket-name", "Unix domain socket to use.", 's', Path::home() + ".gelato");
    Config::registerOption("help", "Display this help", 'h');
    Config::registerOption("version", "Display version", 'v');
    Config::registerOption("connect-timeout", "Timeout to connect to daemon", 'c', 1000);
    Config::registerOption("timeout", "Timeout for job to finish", 't', 5000);
    Config::parse(1, argv);

    if (Config::isEnabled("h")) {
        Config::showHelp(stdout);
        return 0;
    } else if (Config::isEnabled("V")) {
        printf("Stracciatella version %s", VERSION);
        return 0;
    }

    EventLoop loop;

    /*
    SocketClient client;
    if (!client.writeTo("231.232.232.231", 8485, "some data")) {
        error() << "unable to write multicast";
    }
    fflush(stdout);
    loop.run(5000);
    */

    JobMessage job;
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

    bool localJob = job.type() != JobMessage::Compile;
    if (!localJob) {
        char *disabled = getenv("GELATO_DISABLED");
        if (disabled && !strcmp(disabled, "1")) {
            localJob = true;
        }
    }
    int ret;
    if (!localJob && send(&job, ret)) {
        return ret;
    }

    return job.execute();
}
