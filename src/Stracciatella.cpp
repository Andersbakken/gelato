#include <rct/Rct.h>
#include "Job.h"

int main(int argc, char **argv)
{
    Rct::findExecutablePath(argv[0]);
    error() << Rct::executablePath();
    Job job;
    job.parse(argc, argv);
    error() << job.type();
    error() << job.compiler();
    error() << job.arguments();
    error() << job.preprocessArguments();
    return 0;
}
