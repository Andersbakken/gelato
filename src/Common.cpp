#include "Common.h"
#include "Job.h"
#include <rct/Messages.h>

void registerMessages()
{
    Messages::registerMessage<Job>();
}
