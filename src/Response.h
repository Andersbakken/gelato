#ifndef Response_h
#define Response_h

#include <rct/Message.h>

class Response : public Message
{
public:
    Response(Status status, const String &error) : mStatus(status), mErrorText(error) {}
    Response(const String &stdOut, const String &stdErr) : mStatus(Success), mStdOut(stdOut), mStdErr(stdErr) {}
    Response() : mStatus(Success) {}

    enum Status {
        Success,
        Timeout,
        NoDaemons,
        CompilerMissing,
        CompilerPackageError
    };

    Status status() cons { return mStatus; }
    String errorText() const { return mErrorText; }
    String stdOut() const { return mStdOut; }
    String stdErr() const { return mStdErr; }

private:
    Status mStatus;
    String mErrorText, mStdOut, mStdErr;
};

#endif
