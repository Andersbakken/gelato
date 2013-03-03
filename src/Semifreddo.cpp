#include <rct/LocalServer.h>
#include <rct/Config.h>

int main(int argc, char **argv)
{
    Config::registerOption<String>("socket-name", "Unix domain socket to use.", 's', Path::home() + ".gelato");
    Config::registerOption("help", "Display this help", 'h');
    Config::registerOption("version", "Display version", 'v');
    if (!Config::parse(argc, argv)) {
        Config::showHelp(stderr);
        return 1;
    }
    if (Config::isEnabled("h")) {
        Config::showHelp(stdout);
        return 0;
    }
    error() << Config::value<String>("s");
    return 0;
}


