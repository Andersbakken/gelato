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
    
    Response(Status status, const String &error) : mStatus(status), mReturnValue(-1), mErrorText(error) {}
    Response(int returnValue, const String &stdOut, const String &stdErr)
        : mStatus(Success), mReturnValue(returnValue), mStdOut(stdOut), mStdErr(stdErr)
    {}
    Response() : mStatus(Success), mReturnValue(-1) {}

    Status status() const { return mStatus; }
    int returnValue() const { return mReturnValue; }
    String errorText() const { return mErrorText; }
    String stdOut() const { return mStdOut; }
    String stdErr() const { return mStdErr; }

    String encode() const
    {
        String out;
        Serializer serializer(out);
        serializer << static_cast<int>(mStatus) << mReturnValue << mErrorText << mStdOut << mStdErr;
        return out;
    }
    void fromData(const char *data, int size)
    {
        Deserializer deserializer(data, size);
        int status;
        deserializer >> status >> mReturnValue >> mErrorText >> mStdOut >> mStdErr;
        mStatus = static_cast<Status>(status);
    }

private:
    Status mStatus;
    int mReturnValue;
    String mErrorText, mStdOut, mStdErr;
};

#endif
