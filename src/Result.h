#ifndef Result_h
#define Result_h

#include <rct/Message.h>

class Result : public Message
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

    Result(Status status, const String &error) : Message(MessageId), mStatus(status), mReturnValue(-1), mErrorText(error) {}
    Result(int returnValue, const String &stdOut, const String &stdErr)
        : Message(MessageId), mStatus(Success), mReturnValue(returnValue), mStdOut(stdOut), mStdErr(stdErr)
    {}
    Result() : Message(MessageId), mStatus(Success), mReturnValue(-1) {}

    Status status() const { return mStatus; }
    int returnValue() const { return mReturnValue; }
    String errorText() const { return mErrorText; }
    String stdOut() const { return mStdOut; }
    String stdErr() const { return mStdErr; }

    virtual void encode(Serializer &serializer) const
    {
        serializer << static_cast<int>(mStatus) << mReturnValue << mErrorText << mStdOut << mStdErr;
    }
    virtual void decode(Deserializer &deserializer)
    {
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
