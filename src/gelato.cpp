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
            if (proc.start("dpkg", List<String>() << "-L" << package)
                && proc.waitForFinished() && !proc.returnCode()) {
                const List<String> lines = proc.readAllStdOut().split('\n');
                for (int i=0; i<lines.size(); ++i) {
                    const Path p = lines.at(i);
                    if (p.isFile()) {
                        if ((p.mode() & 0111) || p.contains(".so")) { // || p.isHeader()) {
                            ret.insert(p);
                        // } else {
                        //     error() << "ditched file" << p;
                        }
                    }
                }
            }
        }
        {
            Process proc;
            if (proc.start("apt-cache", List<String>() << "depends" << package)
                && proc.waitForFinished() && !proc.returnCode()) {
                const List<String> lines = proc.readAllStdOut().split('\n');
                for (int i=0; i<lines.size(); ++i) {
                    const String &line = lines.at(i);
                    if (line.startsWith("  Depends: ")) {
                        const String package = line.mid(11);
                        if (!package.startsWith("lib"))
                            ret += findPathsDpkg(line.mid(11), packages);
                    }
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
            packages.insert("tzdata");
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
        Set<Path> ret = findPaths(argv[i]);
        error() << ret.size();
        int tot = 0;
        for (Set<Path>::const_iterator it = ret.begin(); it != ret.end(); ++it) {
            tot += it->fileSize();
            error() << " " << *it;
        }
        error() << ret.size() << "files" << tot << "bytes";
    }
    return 0;
}
