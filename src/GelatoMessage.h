#ifndef GelatoMessage_h
#define GelatoMessage_h

#include <rct/Message.h>
#include <rct/Serializer.h>
#include <rct/String.h>

class GelatoMessage : public Message
{
public:
    enum Type {
        Quit
    };
    enum { MessageId = 3 };
    GelatorMessage(Type type) : mType(type) {}
    Type type() const { return mType; }

    virtual int messageId() const { return MessageId; }

    String encode() const
    {
        String out;
        Serializer serializer(out);
        serializer << static_cast<int>(mType);
        return out;
    }
    void fromData(const char *data, int size)
    {
        Deserializer deserializer(data, size);
        int type;
        deserializer >> type;
        mType = static_cast<Type>(type);
    }
    
private:
    Type mType;
};
#endif
