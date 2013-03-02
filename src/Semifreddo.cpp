#include <rct/LocalServer.h>
#include <rct/Config.h>

int main(int argc, char **argv)
{
    Config::registerOption<String>("socket-name", 's', "Unix domain socket to use (default ~/.gelato)", Path::home() + ".gelato");
    Config::registerOption<String>("help", 'h', "Display this help");
    Config::registerOption<String>("version", 'v', "Display version");
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


