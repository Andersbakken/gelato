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
    Job() : mData(new Data) {}

    void clear();
    bool parse(int argc, char **argv);

    virtual int messageId() const { return MessageId; }

    bool execute() const;

    Path compiler() const { return mData->compiler; }
    enum Type {
        Invalid,
        Preprocess,
        Compile,
        Other
    };

    Type type() const { return mData->type; }
    List<String> arguments() const { return mData->args; }
    List<String> preprocessArguments() const;

    void setTimeout(int ms) { mData->ms = ms; }
    int timeout() const { return mData->ms; }

    String encode() const;
    void fromData(const char *data, int size);
private:
    struct Data
    {
        Data()
            : type(Invalid)
        {}
        List<String> args;
        Path cwd;
        Type type;
        Path compiler;
        int ms;
    };
    shared_ptr<Data> mData;
};

#endif
