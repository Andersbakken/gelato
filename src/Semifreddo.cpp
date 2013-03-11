#include <rct/SocketServer.h>
#include <rct/Config.h>
#include <rct/EventLoop.h>
#include <rct/Log.h>
#include "Daemon.h"
#include "Common.h"

int main(int argc, char **argv)
{
    Config::registerOption<String>("socket-name", "Unix domain socket to use.", 's', Path::home() + ".gelato");
    Config::registerOption<String>("log-file", "Log to this file.", 'L');
    Config::registerOption("help", "Display this help", 'h');
    Config::registerOption("version", "Display version", 'V');
    Config::registerOption("verbose", "Be more verbose", 'v');
    Config::registerOption<int>("jobs", "Number of jobs to run locally/serve from other clients concurrently", 'j', -1);
    if (!Config::parse(argc, argv)) {
        Config::showHelp(stderr);
        return 1;
    }


    int level = Error;
    int count;
    if (Config::isEnabled("verbose", &count))
        level += count;
    initLogging(level, Config::value<String>("log-file"));

    if (Config::isEnabled("h")) {
        Config::showHelp(stdout);
        return 0;
    } else if (Config::isEnabled("V")) {
        printf("Semifreddo version %s", VERSION);
        return 0;
    }

    EventLoop loop;
    Daemon daemon;
    if (!daemon.init()) {
        return 1;
    }
    loop.run();
    return 0;
}


