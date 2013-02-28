#include <rct/Rct.h>
#include <rct/LocalClient.h>
#include "Config.h"
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

    bool localJob = job.type() != Job::Compile;
    if (!localJob) {
        char *disabled = getenv("STRACCIATELLA_DISABLED");
        if (disabled && !strcmp(disabled, "1")) {
            localJob = true;
        }
    }
    if (!localJob) {
        LocalClient client;
    }
    if (localJob) {
        job.execute();
        return 1;
    }

}
