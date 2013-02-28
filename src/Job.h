#ifndef Job_h
#define Job_h

#include <rct/List.h>
#include <rct/String.h>
#include <rct/Path.h>
#include <rct/Serializer.h>

class Job
{
public:
    Job() : mData(new Data) {}

    void clear();
    bool parse(int argc, char **argv);

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

    void encode(Serializer &serializer);
    void decode(Deserializer &deserializer);
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
    };
    shared_ptr<Data> mData;
};

#endif
