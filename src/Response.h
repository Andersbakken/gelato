#ifndef Response_h
#define Response_h

#include <rct/Message.h>

class Response : public Message
{
public:
    enum Status {
        Success,
        Timeout,
        NoDaemons,
        CompilerMissing,
        CompilerPackageError
    };

    enum { MessageId = 2 };
    virtual int messageId() const { return MessageId; }
    
    Response(Status status, const String &error) : mStatus(status), mErrorText(error) {}
    Response(const String &stdOut, const String &stdErr) : mStatus(Success), mStdOut(stdOut), mStdErr(stdErr) {}
    Response() : mStatus(Success) {}

    Status status() const { return mStatus; }
    String errorText() const { return mErrorText; }
    String stdOut() const { return mStdOut; }
    String stdErr() const { return mStdErr; }

    String encode() const
    {
        String out;
        Serializer serializer(out);
        serializer << static_cast<int>(mStatus) << mErrorText << mStdOut << mStdErr;
        return out;
    }
    void fromData(const char *data, int size)
    {
        Deserializer deserializer(data, size);
        int status;
        deserializer >> status >> mErrorText >> mStdOut >> mStdErr;
        mStatus = static_cast<Status>(status);
    }

private:
    Status mStatus;
    String mErrorText, mStdOut, mStdErr;
};

#endif
