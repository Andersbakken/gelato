#include "Job.h"
#include "GelatoMessage.h"
#include "Daemon.h"
#include <rct/Process.h>
#include <rct/Connection.h>

Job::Job(const JobMessage& message, const String& sha256, Connection* source)
    : mMsg(message), mSha256(sha256), mSource(source), mType(Pending), mReturnCode(-1), mFinished(false), mPreprocessing(false)
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

    } else {
        // write preprocessed data
        if (mMsg.isPreprocessed()) {
            mData.process->write(mMsg.preprocessed());
            mData.process->closeStdIn();
        }
    }
}

void Job::startRemote(Connection* conn)
{
    if (mType != Pending)
        kill();
    // ...
    mType = Remote;
}

bool Job::startPreprocess()
{
    if (mType != Pending)
        kill();
    // ...
    if (!mMsg.canPreprocess())
        return false;
    mPreprocessing = true;
    mData.process = new Process;
    mData.process->setCwd(mMsg.cwd());
    mData.process->finished().connect(this, &Job::onProcessFinished);
    List<String> environ = Daemon::instance()->defaultEnvironment();
    environ += ("PATH=" + mMsg.path());
    if (!mData.process->start(mMsg.compiler(), mMsg.preprocessArguments(), environ)) {
        return false;
    }
    return true;
}

bool Job::isPreprocessed() const
{
    return (mType == Pending && !mPreprocessing && mMsg.isPreprocessed());
}

void Job::deleteProcess()
{
    mData.process->stop();
    delete mData.process;
    mData.process = 0;
}

void Job::kill()
{
    if (mPreprocessing) {
        deleteProcess();
        mPreprocessing = false;
        mType = Pending;
        return;
    }

    switch (mType) {
    case Pending:
    case Preprocessed:
        return;
    case Local:
        deleteProcess();
        break;
    case Remote: {
        GelatoMessage msg(GelatoMessage::Kill, static_cast<int>(mId));
        mData.destination->send(&msg);
        // do NOT delete the connection here
        break; }
    }
    mType = Pending;
}

void Job::onProcessFinished(Process *process)
{
    assert(process == mData.process);
    mType = Pending;
    mStdErr += process->readAllStdErr();
    mReturnCode = process->returnCode();
    if (mPreprocessing) {
        mPreprocessing = false;
        mMsg.setPreprocessed(process->readAllStdOut());
        mPreprocessed(this);
    } else { // assume local
        mStdOut += process->readAllStdOut();
    }
    process->deleteLater();
    mData.process = 0;
    // warning() << "Finished job" << data.job.compiler() << String::join(data.job.arguments(), " ")
    //           << process->returnCode();
    // Result response(process->returnCode(), data.stdOut, data.stdErr);
    // conn->send(&response);
    // conn->finish();
    // error() << "finished" << data.job.sourceFile();
}
