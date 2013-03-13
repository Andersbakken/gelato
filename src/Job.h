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

    Job(const JobMessage& message, Connection* source);
    ~Job();

    void startLocal();
    void startRemote(Connection* destination);

    Type type() const { return mType; }

    void kill();
    void setPending();

private:
    void sourceDisconnected(Connection*);

private:
    JobMessage mMsg;
    Connection* mSource;
    Type mType;
    union {
        Process* process;
        Connection* destination;
    } mData;
};

#endif
