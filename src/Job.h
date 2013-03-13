#ifndef Job_h
#define Job_h

#include "JobMessage.h"
#include <rct/EventReceiver.h>

class Connection;
class Process;

class Job : public EventReceiver
{
public:
    enum Type { Pending, Local, Remote };

    Job(const JobMessage& message, const String& sha256, Connection* source);
    ~Job();

    String sha256() const { return mSha256; }
    unsigned int id() const { return mId; }

    void startLocal();
    void startRemote(Connection* destination);

    Type type() const { return mType; }

    void kill();
    void setPending();

    bool isFinished() const { return mFinished; }

    signalslot::Signal1<Job*>& jobFinished() { return mJobFinished; }

private:
    void sourceDisconnected(Connection*);
    void onProcessFinished(Process *process);

private:
    JobMessage mMsg;
    String mSha256;
    Connection* mSource;
    Type mType;
    union {
        Process* process;
        Connection* destination;
    } mData;

    String mStdOut, mStdErr;
    int mReturnCode;
    bool mFinished;
    signalslot::Signal1<Job*> mJobFinished;

    unsigned int mId;
};

#endif
