#ifndef Job_h
#define Job_h

#include <rct/List.h>
#include <rct/String.h>
#include <rct/Path.h>
#include <rct/Serializer.h>
#include <rct/Message.h>

class Job : public Message
{
public:
    enum { MessageId = 1 };
    Job() : mType(Invalid), mTimeout(-1) {}

    void clear();
    bool parse(int argc, char **argv);

    virtual int messageId() const { return MessageId; }

    bool execute() const;

    Path compiler() const { return mCompiler; }
    enum Type {
        Invalid,
        Preprocess,
        Compile,
        Other
    };

    Type type() const { return mType; }
    List<String> arguments() const { return mArgs; }
    List<String> preprocessArguments() const;

    void setTimeout(int ms) { mTimeout = ms; }
    int timeout() const { return mTimeout; }

    String encode() const;
    void fromData(const char *data, int size);
private:
    List<String> mArgs;
    Path mCwd;
    Type mType;
    Path mCompiler;
    int mTimeout;
};

#endif
