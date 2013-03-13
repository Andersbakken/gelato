#ifndef GelatoMessage_h
#define GelatoMessage_h

#include <rct/Message.h>
#include <rct/Serializer.h>
#include <rct/String.h>
#include <rct/Value.h>

class GelatoMessage : public Message
{
public:
    enum Type {
        Invalid,
        Quit,
        Stats, // ### need a client or something
        CompilerRequest,
        JobRequest,
        Kill
    };
    enum { MessageId = 3 };
    GelatoMessage(Type type = Invalid, const Value& val = Value()) : Message(MessageId), mType(type), mValue(val) {}
    Type type() const { return mType; }
    Value value() const { return mValue; }

    virtual void encode(Serializer &serializer) const
    {
        serializer << static_cast<int>(mType) << mValue;
    }
    virtual void decode(Deserializer &deserializer)
    {
        int type;
        deserializer >> type >> mValue;
        mType = static_cast<Type>(type);
    }

private:
    Type mType;
    Value mValue;
};
#endif
