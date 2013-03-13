#include "Job.h"
#include "GelatoMessage.h"
#include <rct/Process.h>
#include <rct/Connection.h>

Job::Job(const JobMessage& message, Connection* source)
    : mMsg(message), mSource(source), mType(Pending)
{
    source->connected().connect(this, &Job::sourceDisconnected);
}

Job::~Job()
{
    kill();
}

void Job::sourceDisconnected(Connection*)
{
    kill();
    deleteLater();
}

void Job::startLocal()
{
    if (mType != Pending)
        kill();
    // ...
    mType = Local;
}

void Job::startRemote(Connection* conn)
{
    if (mType != Pending)
        kill();
    // ...
    mType = Remote;
}

void Job::kill()
{
    switch (mType) {
    case Pending:
        return;
    case Local:
        mData.process->stop();
        delete mData.process;
        break;
    case Remote: {
        GelatoMessage msg(GelatoMessage::Kill);
        mData.destination->send(&msg);
        // do NOT delete the connection here
        break; }
    }
    mType = Pending;
}
