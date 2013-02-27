#include <rct/Path.h>
#include <rct/Set.h>
#include <rct/String.h>
#include <rct/Log.h>
#include <rct/Process.h>

Set<Path> findPaths(const Path &compiler)
{
    Set<Path> ret;
    Process proc;
    // EventLoop loop;
    // proc.finished().connect(EventLoop::instance(), &EventLoop::exit);
    proc.start("dpkg", List<String>() << "-S" << compiler);
    error() << proc.waitForFinished();
    error() << proc.readAllStdOut() << proc.readAllStdErr();
    return ret;
}

int main(int argc, char **argv)
{
    EventLoop loop;
    Process proc;
    error() << "Balls" << proc.start("ls");
    error() << proc.waitForFinished();
    error() << proc.readAllStdErr() << proc.readAllStdOut();
    return 0;
    for (int i=1; i<argc; ++i) {
        error() << findPaths(argv[i]);
    }
    return 0;
}
