#include "JobMessage.h"
#include <rct/Rct.h>
#include <rct/Serializer.h>
#include <rct/Process.h>

void JobMessage::clear()
{
    mCwd.clear();
    mArgs.clear();
    mType = Invalid;
    mCompiler.clear();
    mSourceFiles.clear();
    mPath.clear();
    mOutput.clear();
    mTimeout = -1;
}

bool JobMessage::parse(int argc, char **argv)
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
    bool nextNotSource = false;
    for (int i=1; i<argc; ++i) {
        bool parsed = false;
        if (*argv[i] == '-') {
            parsed = true;
            const char *opt = argv[i] + 1;
            if (!strcmp(opt, "E")) {
                dashE = i - 1;
            } else if (!strcmp(opt, "c")) {
                dashC = i - 1;
            } else if (tolower(*opt) == 'l') {
                if (strlen(opt) == 1)
                    nextNotSource = true;
                dashL = i - 1;
            } else if (strchr("DI", *opt)) {
                if (strlen(opt) == 1)
                    nextNotSource = true;
            } else if (!strncmp(opt, "o", 2)) {
                if (strlen(opt) == 1) {
                    if (i + 1 < argc) {
                        nextNotSource = true;
                        mOutput = argv[i + 1];
                    }
                } else {
                    mOutput = opt + 1;
                }
            } else if (!strcmp(opt, "Xassembler")
                       || !strcmp(opt, "Xlinker")
                       || !strcmp(opt, "wrapper")
                       || !strcmp(opt, "aux-info")
                       || !strcmp(opt, "param")
                       || !strcmp(opt, "idirafter")
                       || !strcmp(opt, "iprefix")
                       || !strcmp(opt, "iwithprefix")
                       || !strcmp(opt, "iwithprefixbefore")
                       || !strcmp(opt, "isysroot")
                       || !strcmp(opt, "imultilib")
                       || !strcmp(opt, "isystem")
                       || !strcmp(opt, "iquote")
                       || !strcmp(opt, "x")
                       || !strcmp(opt, "u")
                       || !strcmp(opt, "G")
                       || !strcmp(opt, "T")) {
                nextNotSource = true;
            } else {
                parsed = false;
            }
        }
        if (!parsed) {
            if (nextNotSource) {
                nextNotSource = false;
            } else if (Path::exists(argv[i])) {
                mSourceFiles.append(i - 1);
            }
        }
        mArgs[i - 1] = argv[i];
    }
    if (dashE == -1 && dashC != -1) {
        mType = Compile;
    } else if (dashL != -1) {
        mType = Link;
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

List<String> JobMessage::arguments() const
{
    if (!isPreprocessed())
        return mArgs;

    assert(mSourceFiles.size() == 1);
    List<String> ret = mArgs;
    ret[mSourceFiles.front()] = "-";
    return ret;
}

List<String> JobMessage::preprocessArguments() const
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

int JobMessage::execute() const
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
}

void JobMessage::encode(Serializer &serializer) const
{
    serializer << mCwd << mPath << mArgs << static_cast<int>(mType) << mCompiler << mTimeout
               << mOutput << mSourceFiles << mPreprocessed;
}

void JobMessage::decode(Deserializer &deserializer)
{
    int type;
    deserializer >> mCwd >> mPath >> mArgs >> type >> mCompiler >> mTimeout
                 >> mOutput >> mSourceFiles >> mPreprocessed;
    mType = static_cast<Type>(type);
}

Path JobMessage::sourceFile(int idx) const
{
    return mArgs.value(mSourceFiles.value(idx, -1));
}

List<Path> JobMessage::sourceFiles() const
{
    List<Path> ret(mSourceFiles.size());
    for (int i=0; i<mSourceFiles.size(); ++i) {
        ret[i] = mArgs.value(mSourceFiles.at(i), -1);
    }
    return ret;
}
