#include <rct/SocketServer.h>
#include <rct/Config.h>
#include <rct/EventLoop.h>
#include <rct/Log.h>
#include "Daemon.h"
#include "Common.h"

int main(int argc, char **argv)
{
    Config::registerOption<String>("socket-name", "Unix domain socket to use.", 's', Path::home() + ".gelato");
    Config::registerOption("help", "Display this help", 'h');
    Config::registerOption("version", "Display version", 'V');
    Config::registerOption("verbose", "Be more verbose", 'v');
    initLogging(Error, "log.log", 0);
    if (!Config::parse(argc, argv)) {
        Config::showHelp(stderr);
        return 1;
    }

    if (Config::isEnabled("h")) {
        Config::showHelp(stdout);
        return 0;
    } else if (Config::isEnabled("V")) {
        printf("Semifreddo version %s", VERSION);
        return 0;
    }

    Daemon daemon;
    if (!daemon.init()) {
        return 1;
    }
    EventLoop loop;
    loop.run();
    return 0;
}


