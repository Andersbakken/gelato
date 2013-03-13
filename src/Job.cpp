#include "Job.h"
#include "GelatoMessage.h"
#include "Daemon.h"
#include <rct/Process.h>
#include <rct/Connection.h>

Job::Job(const JobMessage& message, const String& sha256, Connection* source)
    : mMsg(message), mSha256(sha256), mSource(source), mType(Pending), mReturnCode(-1), mFinished(false)
{
    static unsigned int currentId = 0;
    mId = ++currentId;
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
    mData.process = new Process;
    mData.process->setCwd(mMsg.cwd());
    mData.process->finished().connect(this, &Job::onProcessFinished);
    List<String> environ = Daemon::instance()->defaultEnvironment();
    environ += ("PATH=" + mMsg.path());
    if (!mData.process->start(mMsg.compiler(), mMsg.arguments(), environ)) {

    }
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

void Job::onProcessFinished(Process *process)
{
    assert(process == mData.process);
    mStdOut += process->readAllStdOut();
    mStdErr += process->readAllStdErr();
    mReturnCode = process->returnCode();
    mData.process = 0;
    mData.process->deleteLater();
    // warning() << "Finished job" << data.job.compiler() << String::join(data.job.arguments(), " ")
    //           << process->returnCode();
    // Result response(process->returnCode(), data.stdOut, data.stdErr);
    // conn->send(&response);
    // conn->finish();
    // error() << "finished" << data.job.sourceFile();
}
