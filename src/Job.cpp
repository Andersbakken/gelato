#include "Job.h"
#include <rct/Rct.h>
#include <rct/Serializer.h>
#include <rct/Process.h>

void Job::clear()
{
    mCwd.clear();
    mArgs.clear();
    mType = Invalid;
    mCompiler.clear();
    mPath.clear();
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

    int dashE = -1, dashC = -1, dashL = -1;
    mArgs.resize(argc - 1);
    for (int i=1; i<argc; ++i) {
        if (!strcmp(argv[i], "-E")) {
            dashE = i;
        } else if (!strcmp(argv[i], "-c")) {
            dashC = i;
        } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "-L")) {
            dashL = i;
        }
        mArgs[i - 1] = argv[i];
    }
    if (dashL != -1) {
        mType = Link;
    } else if (dashE == -1 && dashC != -1) {
        mType = Compile;
    } else if (dashE != -1 && dashC == -1) {
        mType = Preprocess;
    } else {
        mType = Other;
    }
    mCwd = Path::pwd();
    Path self = Rct::executablePath();
    const Path fileName = self.fileName();
    self.resolve();
    const List<String> split = String(getenv("PATH")).split(':');
    mPath.clear();
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
                if (mCompiler.isEmpty())
                    mCompiler = p;
            } else {
                continue;
            }
        }
        if (!mPath.isEmpty())
            mPath += ':';
        mPath += split.at(i);
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
    Process process;
    process.setCwd(mCwd);
    process.exec(mCompiler, mArgs);
    const String out = process.readAllStdOut();
    if (!out.isEmpty())
        fprintf(stdout, "%s", out.constData());
    const String err = process.readAllStdErr();
    if (!err.isEmpty())
        fprintf(stderr, "%s", err.constData());
    return process.returnCode();
    // String command = mCompiler;
    // for (int i=0; i<mArgs.size(); ++i) {
    //     command += ' ';
    //     const String &arg = mArgs.at(i);
    //     const bool hasSpace = arg.contains(' ');
    //     if (hasSpace)
    //         command += '"';

    //     for (int j=0; j<arg.size(); ++j) {
    //         if (arg.at(j) == '"')
    //             command.append('\\');
    //         command.append(arg.at(j));
    //     }
    //     if (hasSpace)
    //         command += '"';
    // }

    // const int ret = system(command.constData());
    // const int status = WEXITSTATUS(ret);
    // return status;
}

void Job::encode(Serializer &serializer) const
{
    serializer << mCwd << mPath << mArgs << static_cast<int>(mType) << mCompiler << mTimeout;
}

void Job::decode(Deserializer &deserializer)
{
    int type;
    deserializer >> mCwd >> mPath >> mArgs >> type >> mCompiler >> mTimeout;
    mType = static_cast<Type>(type);
}
