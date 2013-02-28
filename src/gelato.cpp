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
            if (proc.exec("dpkg", List<String>() << "-L" << package) == Process::Done
                && !proc.returnCode()) {
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
            if (proc.exec("apt-cache", List<String>() << "depends" << package) == Process::Done
                && !proc.returnCode()) {
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

static Path::VisitResult visitor(const Path &path, void *userData)
{
    Set<Path> &files = *reinterpret_cast<Set<Path> *>(userData);
    // if (path.isSymLink()) {
    // error() << path << "isSymLink" << path.isSymLink() << path.isDir();
    if (path.isSymLink()) {
        files.insert(path);
        if (path.isFile()) {
            Path link = path.followLink();
            link.resolve(Path::RealPath, path.parentDir());
            visitor(link, userData);
        }
    } else if (path.isDir()) {
        if (!path.isSymLink()) {
            return Path::Recurse;
        } else {
            files.insert(path);
        }
    } else if (path.mode() & 0111) {
        char *buf = 0;
        const int read = path.readAll(buf, 3);
        if (read != 3 || strncmp(buf, "#!/", read))
            files.insert(path);
        delete[] buf;
    } else if (path.contains(".so")) {
        files.insert(path);
    }
    return Path::Continue;
}

Set<Path> findPaths(const Path &compiler)
{
    Set<Path> ret;
    ret.insert(compiler);
#if 0
    Process proc;
    if (proc.exec("dpkg", List<String>() << "-S" << compiler) == Process::Done
        && !proc.returnCode()) {
        String package = proc.readAllStdOut();
        const int colon = package.indexOf(':');
        if (colon != -1) {
            Set<String> packages;
            packages.insert("tzdata");
            return findPathsDpkg(package.left(colon), packages);
        }
    }
#endif
    Process proc;
    proc.exec(compiler, List<String>() << "-v" << "-E" << "-");
    const List<String> lines = proc.readAllStdErr().split('\n');
    for (int i=0; i<lines.size(); ++i) {
        const String &line = lines.at(i);
        if (line.startsWith("COMPILER_PATH=")) {
            const Set<String> paths = line.mid(14).split(':').toSet();
            for (Set<String>::const_iterator it = paths.begin(); it != paths.end(); ++it) {
                const Path p = Path::resolved(*it);
                if (p.isDir()) {
                    p.visit(visitor, &ret);
                }
            }
        }
    }
    // ### todo, tar.gz them up
    return ret;
}

int main(int argc, char **argv)
{
    EventLoop loop;
    /*
    Process proc;
    error() << "Balls" << proc.exec("ls");
    error() << proc.readAllStdErr() << proc.readAllStdOut();
    error() << "--==stuff==--";
    error() << "Balls again" << proc.exec("find", List<String>() << "/usr/lib/" << "-name" << "*.so");
    error() << proc.readAllStdErr() << proc.readAllStdOut();
    return 0;
    */
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
