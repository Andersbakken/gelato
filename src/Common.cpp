#include "Common.h"
#include "Job.h"
#include "Result.h"
#include <rct/Messages.h>

void registerMessages()
{
    Messages::registerMessage<Job>();
    Messages::registerMessage<Result>();
}
