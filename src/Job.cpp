#include "Job.h"
#include <rct/Rct.h>

bool Job::parse(int argc, char **argv)
{
    int dashE = -1, dashO = -1;
    for (int i=1; i<argc; ++i) {
        if (!strncmp(argv[i], "-E", 2)) {
            dashE = i;
        } else if (!strncmp(argv[i], "-o", 2)) {
            dashO = i;
        }
    }
    if ((dashE == -1) == (dashO == -1)) {
        clear();
        return false;
    }
    mData->mode = dashE != -1 ? Preprocess : Compile;
    mData->dashE = dashE;
    mData->dashO = dashO;
    Rct::findApplicationDirPath(argv[0]);
    Path self = argv[0];
    mData->compiler = argv[0]; // gotta resolve
    char *path = getenv("PATH");
    mData->argc = argc;
    mData->argv = argv;
}

List<String> Job::arguments(Mode mode) const
{
    
}

void Job::encode(Serializer &serializer)
{
    
}

bool Job::decode(Deserializer &deserializer)
{
    
}
