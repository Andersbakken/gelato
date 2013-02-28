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
    enum Mode {
        Invalid,
        Preprocess,
        Compile
    };
    Mode mode() const { return mData->mode; }
    List<String> arguments(Mode mode) const;

    void encode(Serializer &serializer);
    bool decode(Deserializer &deserializer);
private:
    struct Data
    {
        Data()
            : argc(0), argv(0), mode(Invalid), dashO(-1), dashE(-1)
        {}
        int argc;
        char **argv;
        Mode mode;
        Path compiler;
        int dashO, dashE;
    };
    shared_ptr<Data> mData;
};

#endif
