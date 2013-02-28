#include "Job.h"
#include <rct/Rct.h>

void Job::clear()
{
    mData->cwd.clear();
    mData->args.clear();
    mData->type = Invalid;
    mData->compiler.clear();
}

bool Job::parse(int argc, char **argv)
{
    if (!argc || !argv) {
        clear();
        printf("[%s] %s:%d: clear(); [after]\n", __func__, __FILE__, __LINE__);
        return false;
    }

    int dashE = -1, dashO = -1;
    mData->args.resize(argc - 1);
    for (int i=1; i<argc; ++i) {
        if (!strncmp(argv[i], "-E", 2)) {
            dashE = i;
        } else if (!strncmp(argv[i], "-o", 2)) {
            dashO = i;
        }
        mData->args[i - 1] = argv[i];
    }
    if (dashE == -1 && dashO != -1) {
        mData->type = Compile;
    } else if (dashE != -1 && dashO == -1) {
        mData->type = Preprocess;
    } else {
        mData->type = Other;
    }
    mData->cwd = Path::pwd();
    mData->type = dashE != -1 ? Preprocess : Compile;
    Path self = Rct::executablePath();
    const Path fileName = self.fileName();
    self.resolve();
    const List<String> split = String(getenv("PATH")).split(':');
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
                mData->compiler = p;
                break;
            }
        }
    }
    if (mData->compiler.isEmpty()) {
        clear();
        return false;
    }
    return true;
}

List<String> Job::preprocessArguments() const
{
    if (mData->type == Preprocess)
        return mData->args;

    List<String> ret = mData->args;
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

void Job::encode(Serializer &serializer)
{
    serializer << mData->cwd << mData->args << static_cast<int>(mData->type) << mData->compiler;
}

void Job::decode(Deserializer &deserializer)
{
    int type;
    deserializer >> mData->cwd >> mData->args >> type >> mData->compiler;
    mData->type = static_cast<Type>(type);
}
