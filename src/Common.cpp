#include "Common.h"
#include "Job.h"
#include "Response.h"
#include <rct/Messages.h>

void registerMessages()
{
    Messages::registerMessage<Job>();
    Messages::registerMessage<Response>();
}
