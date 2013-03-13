#include "Common.h"
#include "JobMessage.h"
#include "Result.h"
#include "GelatoMessage.h"
#include <rct/Messages.h>

void registerMessages()
{
    Messages::registerMessage<JobMessage>();
    Messages::registerMessage<Result>();
    Messages::registerMessage<GelatoMessage>();
}
