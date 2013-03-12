#ifndef CompilerMessage_h
#define CompilerMessage_h

#include <rct/Message.h>
#include <rct/Path.h>
#include <rct/Set.h>

class CompilerMessage : public Message
{
public:
    enum { MessageId = 4 };
    CompilerMessage(const Path &compiler, const Set<Path> &paths, const String &sha256);

    virtual void encode(Serializer &serializer) const;
    virtual void decode(Deserializer &deserializer);
    Path compiler() const { return mCompiler; }
    String sha256() const { return mSha256; }
    const Map<Path, String> &paths() const { return mPaths; }
private:
    Path mCompiler;
    Map<Path, String> mPaths;
    String mSha256;
};

#endif
