#if defined(USE_ARES)
extern "C"
{
#include "ares.h"
#include "ares_dns.h"
}
#endif

#include "resiprocate/os/Data.hxx"
#include "resiprocate/os/BaseException.hxx"
#include "RROverlay.hxx"
#include "DnsResourceRecord.hxx"
#include "DnsSrvRecord.hxx"

using namespace resip;

DnsSrvRecord::DnsSrvRecord(const RROverlay& overlay)
{
   char* name = 0;
   int len = 0;
   ares_expand_name(overlay.data()-overlay.nameLength()-RRFIXEDSZ, overlay.msg(), overlay.msgLength(), &name, &len);
   mName = name;
   free(name);

   mPriority = DNS__16BIT(overlay.data());
   mWeight = DNS__16BIT(overlay.data() + 2);
   mPort = DNS__16BIT(overlay.data() + 4);

   if (ARES_SUCCESS != ares_expand_name(overlay.data() + 6, overlay.msg(), overlay.msgLength(), &name, &len))
   {
      throw SrvException("Failed parse of SRV record", __FILE__, __LINE__);
   }
   mTarget = name;
   free(name);
}
