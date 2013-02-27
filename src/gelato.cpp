#include <rct/Path.h>
#include <rct/Set.h>
#include <rct/String.h>
#include <rct/Log.h>
#include <rct/Process.h>

Set<Path> findPathsDpkg(const Path &package, Set<String> &packages)
{
    error() << "package" << package;
    Set<Path> ret;
    if (packages.insert(package)) {
        {
            Process proc;
            printf("[%s] %s:%d: if (proc.start(\"dpkg\", List<String>() << \"-L\" << package) [before]\n", __func__, __FILE__, __LINE__);
            if (proc.start("dpkg", List<String>() << "-L" << package)
                && proc.waitForFinished() && !proc.returnCode()) {
                printf("[%s] %s:%d: && proc.waitForFinished() && !proc.returnCode()) { [after]\n", __func__, __FILE__, __LINE__);
                const List<String> lines = proc.readAllStdOut().split('\n');
                for (int i=0; i<lines.size(); ++i) {
                    const Path p = lines.at(i);
                    if (p.isFile()) {
                        ret.insert(p);
                    }
                }
            }
        }
        printf("[%s] %s:%d: if (proc.start(\"apt-cache\", List<String>() << \"depends\" << package) [before]\n", __func__, __FILE__, __LINE__);
        {
            Process proc;
            if (proc.start("apt-cache", List<String>() << "depends" << package)
                && proc.waitForFinished() && !proc.returnCode()) {
                printf("[%s] %s:%d: && proc.waitForFinished() && !proc.returnCode()) { [after]\n", __func__, __FILE__, __LINE__);
                const List<String> lines = proc.readAllStdOut().split('\n');
                for (int i=0; i<lines.size(); ++i) {
                    const String &line = lines.at(i);
                    if (line.startsWith("  Depends: "))
                        ret += findPathsDpkg(line.mid(11), packages);
                }
            }
        }
    }

    return ret;
}

Set<Path> findPaths(const Path &compiler)
{
    Set<Path> ret;
    Process proc;
    if (proc.start("dpkg", List<String>() << "-S" << compiler)
        && proc.waitForFinished() && !proc.returnCode()) {
        String package = proc.readAllStdOut();
        const int colon = package.indexOf(':');
        if (colon != -1) {
            Set<String> packages;
            return findPathsDpkg(package.left(colon), packages);
        }
    }

    error() << proc.waitForFinished();
    error() << proc.readAllStdOut() << proc.readAllStdErr();
    return ret;
}

int main(int argc, char **argv)
{
    EventLoop loop;
    // Process proc;
    // error() << "Balls" << proc.start("ls");
    // error() << proc.waitForFinished();
    // error() << proc.readAllStdErr() << proc.readAllStdOut();
    // return 0;
    for (int i=1; i<argc; ++i) {
        error() << findPaths(argv[i]);
    }
    return 0;
}
