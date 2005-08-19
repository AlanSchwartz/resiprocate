#if !defined(RESIP_DumFeatureMessage_hxx)
#define RESIP_DumFeatureMessage_hxx 

#include <iosfwd>
#include "resip/sip/ApplicationMessage.hxx"

namespace resip
{

//!dcm! -- what is the intent of ApplicationMessage, especially as used in
//repro? Is this really what ApplicationMessage should be(always has tid)

class DumFeatureMessage : public ApplicationMessage
{
   public:
      DumFeatureMessage(const Data& tid);      
      DumFeatureMessage(const DumFeatureMessage&);      
      ~DumFeatureMessage();

      Message* clone() const;

      virtual std::ostream& encode(std::ostream& strm) const;
      /// output a brief description to stream
      virtual std::ostream& encodeBrief(std::ostream& str) const;

      virtual const Data& getTransactionId() const { return mTransactionId; }
   private:
      Data mTransactionId;      
};

}

#endif
