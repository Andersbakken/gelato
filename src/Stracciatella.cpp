#include <rct/Rct.h>
#include "Job.h"
#include <errno.h>

int main(int argc, char **argv)
{
    Rct::findExecutablePath(argv[0]);
    Job job;
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

    const List<String> args = job.arguments();
    const char* arguments[args.size() + 2];
    arguments[args.size() + 1] = 0;
    const Path compiler = job.compiler();
    arguments[0] = compiler.constData();
    extern char **environ;
    int ret;
    eintrwrap(ret, execve(arguments[0], const_cast<char* const*>(arguments), const_cast<char* const*>(environ)));
    error("execve failed somehow %d %d %s", ret, errno, strerror(errno));
    return 1;
}
