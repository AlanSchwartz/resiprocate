#include "PagerMessageCreator.hxx"

using namespace resip;

PagerMessageCreator::PagerMessageCreator(DialogUsageManager& dum, const NameAddr& target, Identity& identity)
   : BaseCreator(dum, identity)
{
   makeInitialRequest(target, MESSAGE);
   //rfc3428 section 9 - remove the header that may have been added by the BaseCreator and are not allowed
   mLastRequest.remove(h_Supporteds);
   mLastRequest.remove(h_AcceptEncodings);
   mLastRequest.remove(h_AcceptLanguages);
   mLastRequest.remove(h_Contacts);
}
