#include "CompilerMessage.h"
#include <rct/Rct.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

class CompilerPackage
{
public:
    CompilerPackage() {}
    CompilerPackage(const Map<Path, String>& files) : mFiles(files) {}

    static CompilerPackage* loadFromPaths(const Set<Path>& paths);

    bool isEmpty() const { return mFiles.isEmpty(); }
    bool loadFile(const Path& file);

    const Map<Path, String>& files() const { return mFiles; }

private:
    Map<Path, String> mFiles;
};

CompilerPackage* CompilerPackage::loadFromPaths(const Set<Path>& paths)
{
    CompilerPackage* package = new CompilerPackage;
    Set<Path>::const_iterator path = paths.begin();
    const Set<Path>::const_iterator end = paths.end();
    while (path != end) {
        if (!package->loadFile(*path)) {
            delete package;
            return 0;
        }
        ++path;
    }
    if (package->isEmpty()) {
        delete package;
        return 0;
    }
    return package;
}

bool CompilerPackage::loadFile(const Path& file)
{
    String data;
    if (!Rct::readFile(file, data))
        return false;
    mFiles[file] = data;
    return true;
}

Serializer& operator<<(Serializer& s, const CompilerPackage& p)
{
    s << p.files();
    return s;
}

Deserializer& operator>>(Deserializer& s, CompilerPackage& p)
{
    Map<Path, String> files;
    s >> files;
    p = CompilerPackage(files);
    return s;
}

Map<Path, CompilerPackage*> CompilerMessage::sPackages;

CompilerMessage::CompilerMessage(const Path &compiler, const Set<Path> &paths, const String &sha256)
    : Message(MessageId), mCompiler(compiler), mSha256(sha256), mPackage(0)
{
    /*
    for (Set<Path>::const_iterator it = paths.begin(); it != paths.end(); ++it) {
        if (it->isSymLink()) {
            mLinks[*it] = it->followLink();
        } else {
            mFiles[*it] = it->readAll();
        }
    }
    */

    const Map<Path, CompilerPackage*>::const_iterator package = sPackages.find(compiler);
    if (package != sPackages.end())
        mPackage = package->second;
    else {
        mPackage = loadCompiler(paths);
        if (mPackage)
            sPackages[compiler] = mPackage;
    }
}

CompilerMessage::~CompilerMessage()
{
    delete mPackage;
}

CompilerPackage* CompilerMessage::loadCompiler(const Set<Path>& paths)
{
    return CompilerPackage::loadFromPaths(paths);
}

void CompilerMessage::encode(Serializer &serializer) const
{
    serializer << mCompiler << mSha256 << (mPackage != 0);
    if (mPackage)
        serializer << *mPackage;
}

void CompilerMessage::decode(Deserializer &deserializer)
{
    bool hasPackage;
    deserializer >> mCompiler >> mSha256 >> hasPackage;
    if (hasPackage) {
        if (!mPackage)
            mPackage = new CompilerPackage;
        deserializer >> *mPackage;
    }
}

bool CompilerMessage::writeFiles(const Path& path)
{
    if (!mPackage)
        return false;

    const pid_t pid = fork();
    if (pid == -1)
        return false;
    if (pid) {
        int status;
        waitpid(pid, &status, 0);
        return (status == 0);
    } else {
        if (chroot(path.nullTerminated()) == -1) {
            _exit(1);
        }
        int ret = 0;
        const Map<Path, String>& files = mPackage->files();
        Map<Path, String>::const_iterator file = files.begin();
        const Map<Path, String>::const_iterator end = files.end();
        while (file != end) {
            if (!Rct::writeFile(file->first, file->second)) {
                ret = 1;
                break;
            }
            ++file;
        }
        _exit(ret);
    }
    return false;
}
