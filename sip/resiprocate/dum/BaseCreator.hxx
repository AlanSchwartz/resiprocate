#if !defined(RESIP_BASECREATOR_HXX)
#define RESIP_BASECREATOR_HXX

#include "resiprocate/SipMessage.hxx"

namespace resip
{

class DialogUsageManager;

class BaseCreator
{
   public:
      BaseCreator(DialogUsageManager& dum);
      virtual ~BaseCreator();
      SipMessage& getLastRequest();
      //virtual void dispatch(SipMessage& msg)=0;
      
   protected:
      void makeInitialRequest(const NameAddr& target, MethodTypes method);
      
      // this will get updated when an initial request is challenged. where we
      // store the credentials and last cseq
      SipMessage mLastRequest;
      DialogUsageManager& mDum;
};

}

#endif
