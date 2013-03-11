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
    GelatoMessage(Type type) : Message(MessageId), mType(type) {}
    Type type() const { return mType; }

    virtual void encode(Serializer &serializer) const
    {
        serializer << static_cast<int>(mType);
    }
    virtual void decode(Deserializer &deserializer)
    {
        int type;
        deserializer >> type;
        mType = static_cast<Type>(type);
    }
    
private:
    Type mType;
};
#endif
