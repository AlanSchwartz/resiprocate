#if !defined(TESTSUPPORT_HXX)
#define TESTSUPPORT_HXX
#include <sys/types.h>
#include "sip2/util/Data.hxx"
#include "sip2/sipstack/SipMessage.hxx"

namespace Vocal2 {

class TestSupport
{
   public:
      static void prettyPrint(const char *buffer, size_t length);
      static Data showN(const char *buffer, size_t length);
      static SipMessage* makeMessage(const Data& data, bool isExternal = false);
};

/// Allows sending of arbitrarily badly formed messages.
class SipRawMessage : public SipMessage
{
   public:
      SipRawMessage(const SipMessage& carrier,
                    const Data& rawMessage)
         : SipMessage(carrier),
           mRawMessage(rawMessage)
      {}

      Data& raw() const {return mRawMessage;}

      virtual std::ostream& encode(std::ostream& str) const
      {
         str << mRawMessage;
         return str;
      }

   private:
      mutable Data mRawMessage;
};

};
#endif
