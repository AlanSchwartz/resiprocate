#if defined(USE_ARES)
extern "C"
{
#include "ares.h"
#include "ares_dns.h"
}
#endif

#ifndef __CYGWIN__
#ifndef RRFIXEDSZ
#define RRFIXEDSZ 10
#endif
#endif

#include <vector>
#include "resiprocate/os/Data.hxx"
#include "resiprocate/os/BaseException.hxx"
#include "RROverlay.hxx"

using namespace resip;
using namespace std;

#define RESIPROCATE_SUBSYSTEM Subsystem::DNS

// aptr - points to the begining of the resource record.
// abuf - points to the begining of the message.
// alen - length of abuf.
RROverlay::RROverlay(const unsigned char *aptr,
                     const unsigned char *abuf,
                     int alen)
{
   char *name;
   int len = 0;

   // Parse the RR name. 
   int status = ares_expand_name(aptr, abuf, alen, &name, &len);
   if (status != ARES_SUCCESS)
   {
      throw OverlayException("Failed parse of RR", __FILE__, __LINE__);
   }
   aptr += len;
   mNameLen = len;
      
   // Make sure there is enough data after the RR name for the fixed
   // part of the RR.
   if (aptr + RRFIXEDSZ > abuf + alen)
   {
      free(name);
      throw OverlayException("Failed parse of RR", __FILE__, __LINE__);
   }
   
   // Parse the fixed part of the RR, and advance to the RR data field. 
   //
   mType = DNS_RR_TYPE(aptr);
   mDataLen = DNS_RR_LEN(aptr);
   mTTL = DNS_RR_TTL(aptr);
   
   aptr += RRFIXEDSZ;
   if (aptr + mDataLen > abuf + alen)
   {
      free(name);
      throw OverlayException("Failed parse of RR", __FILE__, __LINE__);
   }

   mData = aptr;
   mMsgLen = alen;
   mMsg = abuf;
   free(name);
   aptr += mDataLen;
}
