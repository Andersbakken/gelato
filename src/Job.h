#ifndef Job_h
#define Job_h

#include "JobMessage.h"
#include <rct/EventReceiver.h>

class Connection;
class Process;

class Job : public EventReceiver
{
public:
    enum Type { Pending, Preprocessed, Local, Remote };

    Job(const JobMessage& message, const String& sha256, Connection* source);
    ~Job();

    String sha256() const { return mSha256; }
    unsigned int id() const { return mId; }

    void startLocal();
    void startRemote(Connection* destination);
    bool startPreprocess();

    Type type() const { return mType; }

    void kill();
    bool isPreprocessed() const;

    bool isFinished() const { return mFinished; }

    signalslot::Signal1<Job*>& jobFinished() { return mJobFinished; }
    signalslot::Signal1<Job*>& preprocessed() { return mPreprocessed; }

    Connection *source() const { return mSource; }

    int returnCode() const { return mReturnCode; }
    String stdOut() const { return mStdOut; }
    String stdErr() const { return mStdErr; }
private:
    void sourceDisconnected(Connection*);
    void onProcessFinished(Process *process);

    void deleteProcess();

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
    bool mFinished, mPreprocessing;
    signalslot::Signal1<Job*> mJobFinished;
    signalslot::Signal1<Job*> mPreprocessed;

    unsigned int mId;
};

#endif
