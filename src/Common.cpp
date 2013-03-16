#include "Common.h"
#include "JobMessage.h"
#include "ResultMessage.h"
#include "GelatoMessage.h"
#include <rct/Messages.h>

void registerMessages()
{
    Messages::registerMessage<JobMessage>();
    Messages::registerMessage<ResultMessage>();
    Messages::registerMessage<GelatoMessage>();
}
