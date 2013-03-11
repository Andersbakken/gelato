#include "Job.h"
#include <rct/Rct.h>
#include <rct/Serializer.h>

void Job::clear()
{
    mCwd.clear();
    mArgs.clear();
    mType = Invalid;
    mCompiler.clear();
    mTimeout = -1;
}

bool Job::parse(int argc, char **argv)
{
    // mArgs = String("-DGELATO_DEBUG -DOS_Darwin -g -I/Users/abakken/dev/gelato/src/rct/src -I/Users/abakken/dev/gelato/src/rct/include -I/Users/abakken/dev/gelato/src    -o CMakeFiles/stracciatella.dir/Stracciatella.cpp.o -c /Users/abakken/dev/gelato/src/Stracciatella.cpp").split(' ');
    // mType = Compile;
    // mCompiler = "/usr/bin/c++";
    // mCwd = Path::pwd();
    // return true;
    if (!argc || !argv) {
        clear();
        return false;
    }

    int dashE = -1, dashC = -1;
    mArgs.resize(argc - 1);
    for (int i=1; i<argc; ++i) {
        if (!strcmp(argv[i], "-E")) {
            dashE = i;
        } else if (!strcmp(argv[i], "-c")) {
            dashC = i;
        }
        mArgs[i - 1] = argv[i];
    }
    if (dashE == -1 && dashC != -1) {
        mType = Compile;
    } else if (dashE != -1 && dashC == -1) {
        mType = Preprocess;
    } else {
        mType = Other;
    }
    mCwd = Path::pwd();
    mType = dashE != -1 ? Preprocess : Compile;
    Path self = Rct::executablePath();
    const Path fileName = self.fileName();
    self.resolve();
    const List<String> split = String(getenv("PATH")).split(':');
    // error() << split;
    for (int i=0; i<split.size(); ++i) {
        Path p = split.at(i);
        if (p.isEmpty())
            continue;
        if (!p.endsWith('/'))
            p.append('/');
        p.append(fileName);
        if (p.isFile()) {
            const Path resolved = p.resolved();
            if (resolved != self) {
                mCompiler = p;
                break;
            }
        }
    }
    if (mCompiler.isEmpty()) {
        clear();
        return false;
    }
    return true;
}

List<String> Job::preprocessArguments() const
{
    if (mType == Preprocess)
        return mArgs;

    List<String> ret = mArgs;
    for (int i=0; i<ret.size(); ++i) {
        if (ret.at(i).startsWith("-o")) {
            if (ret[i].size() == 2 && i + 1 < ret.size())
                ret.removeAt(i + 1);
            ret[i] = "-E";
            break;
        }
    }
    return ret;
}

int Job::execute() const
{
    String command = mCompiler;
    command += " " + String::join(mArgs, " ");
    const int ret = system(command.constData());
    const int status = WEXITSTATUS(ret);
    return status;
}

String Job::encode() const
{
    String ret;
    Serializer serializer(ret);
    serializer << mCwd << mArgs << static_cast<int>(mType) << mCompiler << mTimeout;
    return ret;
}

void Job::fromData(const char *data, int length)
{
    Deserializer deserializer(data, length);
    int type;
    deserializer >> mCwd >> mArgs >> type >> mCompiler >> mTimeout;
    mType = static_cast<Type>(type);
}
