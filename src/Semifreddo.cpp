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
        printf("[%s] %s:%d: if (!Config::parse(argc, argv)) { [after]\n", __func__, __FILE__, __LINE__);
        Config::showHelp(stderr);
        return 1;
    }

    if (Config::isEnabled("h")) {
        printf("[%s] %s:%d: if (Config::isEnabled(\"h\")) { [after]\n", __func__, __FILE__, __LINE__);
        Config::showHelp(stdout);
        return 0;
    } else if (Config::isEnabled("V")) {
        printf("[%s] %s:%d: } else if (Config::isEnabled(\"V\")) { [after]\n", __func__, __FILE__, __LINE__);
        printf("Semifreddo version %s", VERSION);
        return 0;
    }

    EventLoop loop;
    Daemon daemon;
    if (!daemon.init()) {
        printf("[%s] %s:%d: if (!daemon.init()) { [after]\n", __func__, __FILE__, __LINE__);
        return 1;
    }
    printf("[%s] %s:%d: EventLoop loop; [after]\n", __func__, __FILE__, __LINE__);
    loop.run();
    printf("[%s] %s:%d: loop.run(); [after]\n", __func__, __FILE__, __LINE__);
    return 0;
}


