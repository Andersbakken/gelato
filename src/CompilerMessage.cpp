#include "CompilerMessage.h"
#include <rct/Rct.h>

CompilerMessage::CompilerMessage(const Path &compiler, const Set<Path> &paths, const String &sha256)
    : Message(MessageId), mCompiler(compiler), mSha256(sha256)
{
    for (Set<Path>::const_iterator it = paths.begin(); it != paths.end(); ++it) {
        mPaths[*it] = it->readAll();
    }
}

void CompilerMessage::encode(Serializer &serializer) const
{
    serializer << mCompiler << mSha256 << mPaths;
}

void CompilerMessage::decode(Deserializer &deserializer)
{
    deserializer >> mCompiler >> mSha256 >> mPaths;
}
