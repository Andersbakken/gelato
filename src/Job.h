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
    Job() : Message(MessageId), mType(Invalid), mTimeout(-1) {}

    void clear();
    bool parse(int argc, char **argv);

    int execute() const;

    Path compiler() const { return mCompiler; }
    enum Type {
        Invalid,
        Preprocess,
        Compile,
        Link,
        Other
    };

    Type type() const { return mType; }
    List<String> arguments() const { return mArgs; }
    List<String> preprocessArguments() const;

    List<Path> sourceFiles() const;
    Path sourceFile(int idx = 0) const;

    Path output() const { return mOutput; }

    void setTimeout(int ms) { mTimeout = ms; }
    int timeout() const { return mTimeout; }

    Path cwd() const { return mCwd; }
    String path() const { return mPath; }

    void encode(Serializer &serializer) const;
    void decode(Deserializer &deserializer);
private:
    List<String> mArgs;
    Path mCwd;
    String mPath;
    Type mType;
    Path mCompiler;
    int mTimeout;
    Path mOutput;
    List<int> mSourceFiles;
};

#endif
