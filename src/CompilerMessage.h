#ifndef CompilerMessage_h
#define CompilerMessage_h

#include <rct/Map.h>
#include <rct/Message.h>
#include <rct/Path.h>
#include <rct/Set.h>

class CompilerPackage;

class CompilerMessage : public Message
{
public:
    enum { MessageId = 4 };
    CompilerMessage(const String& sha256) : Message(MessageId), mPackage(0) { mSha256 = sha256; }
    CompilerMessage(const Path &compiler, const Set<Path> &paths, const String &sha256);
    ~CompilerMessage();

    virtual void encode(Serializer &serializer) const;
    virtual void decode(Deserializer &deserializer);
    Path compiler() const { return mCompiler; }
    String sha256() const { return mSha256; }

    bool isValid() const { return mPackage != 0; }

    bool writeFiles(const Path& path);

private:
    CompilerPackage* loadCompiler(const Set<Path> &paths);

private:
    static Map<Path, CompilerPackage*> sPackages;

    Path mCompiler;
    String mSha256;
    CompilerPackage* mPackage;
};

#endif
