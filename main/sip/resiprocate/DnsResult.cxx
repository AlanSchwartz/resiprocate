#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include <algorithm>

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifndef __CYGWIN__
#  include <netinet/in.h>
#  include <arpa/nameser.h>
#  include <resolv.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#endif

#if defined(USE_ARES)
extern "C"
{
#include "ares.h"
#include "ares_dns.h"
}
#endif

#include "resiprocate/os/DnsUtil.hxx"
#include "resiprocate/os/Inserter.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/Random.hxx"
#include "resiprocate/os/Tuple.hxx"
#include "resiprocate/os/compat.hxx"

#include "resiprocate/DnsHandler.hxx"
#include "resiprocate/DnsInterface.hxx"
#include "resiprocate/DnsResult.hxx"
#include "resiprocate/ParserCategories.hxx"
#include "resiprocate/Uri.hxx"

#if !defined(USE_ARES)
#warning "ARES is required"
#endif

using namespace resip;

#define RESIPROCATE_SUBSYSTEM resip::Subsystem::DNS

DnsResult::DnsResult(DnsInterface& interfaceObj, DnsHandler* handler) 
   : mInterface(interfaceObj),
     mHandler(handler),
     mSRVCount(0),
     mSips(false),
     mTransport(UNKNOWN_TRANSPORT),
     mPort(-1),
     mType(Pending)
{
}

DnsResult::~DnsResult()
{
   DebugLog (<< "DnsResult::~DnsResult() " << *this);
   assert(mType != Pending);
}

void
DnsResult::destroy()
{
   assert(this);
   DebugLog (<< "DnsResult::destroy() " << *this);
   
   if (mType == Pending)
   {
      mType = Destroyed;
   }
   else
   {
      delete this;
   }
}

DnsResult::Type
DnsResult::available()
{
   assert(mType != Destroyed);
   if (mType == Available)
   {
      if (!mResults.empty())
      {
         return Available;
      }
      else
      {
         primeResults();
         return available(); // recurse
      }
   }
   else
   {
      return mType;
   }
}

Tuple
DnsResult::next() 
{
   assert(available() == Available);
   Tuple next = mResults.front();
   mResults.pop_front();
   DebugLog (<< "Returning next dns entry: " << next);
   return next;
}

void
DnsResult::lookup(const Uri& uri)
{
   DebugLog (<< "DnsResult::lookup " << uri);
   
   assert(uri.scheme() == Symbols::Sips || uri.scheme() == Symbols::Sip);  
   mTarget = uri.exists(p_maddr) ? uri.param(p_maddr) : uri.host();
   mSips = (uri.scheme() == Symbols::Sips);
   bool isNumeric = DnsUtil::isIpAddress(mTarget);

   if (uri.exists(p_transport))
   {
      mTransport = Tuple::toTransport(uri.param(p_transport));

      if (isNumeric) // IP address specified
      {
         mPort = getDefaultPort(mTransport, uri.port());
         Tuple tuple(mTarget, mPort, mTransport);
         DebugLog (<< "Found immediate result: " << tuple);
         mResults.push_back(tuple);
         mType = Available;
      }
      else if (uri.port() != 0)
      {
         mPort = uri.port();
         lookupAAAARecords(mTarget); // for current target and port         
      }
      else 
      { 
         mPort = getDefaultPort(mTransport, uri.port());
         lookupAAAARecords(mTarget); // for current target and port                   
      } 
   }
   else 
   {
      if (isNumeric || uri.port() != 0)
      {
         if (mSips)
         {
            mTransport = TLS;
         }
         else 
         {
            mTransport = UDP;
         }

         if (isNumeric) // IP address specified
         {
            mPort = getDefaultPort(mTransport, uri.port());
            Tuple tuple(mTarget, mPort, mTransport);
            mResults.push_back(tuple);
            mType = Available;
            DebugLog (<< "Numeric result so return immediately: " << tuple);
         }
         else // port specified so we know the transport
         {
            mPort = uri.port();
            lookupAAAARecords(mTarget); // for current target and port         
         }
      }
      else // do NAPTR
      {
         lookupNAPTR(); // for current target
      }
   }
}

int
DnsResult::getDefaultPort(TransportType transport, int port)
{
   if (port == 0)
   {
      switch (transport)
      {
         case UDP:
            return 5060;
         case TCP:
            return mSips ? 5061 : 5060;
         default:
            InfoLog( << "Should not get this - unkown transport" );
            return 5060; // !cj! todo - remove 
            assert(0);
      }
   }
   else
   {
      return port;
   }

	assert(0);
	return 0;
}


void
DnsResult::lookupAAAARecords(const Data& target)
{
#if defined(USE_IPV6)
   DebugLog(<< "Doing host (AAAA) lookup: " << target);
   mPassHostFromAAAAtoA = target; // hackage
   ares_query(mInterface.mChannel, target.c_str(), C_IN, T_AAAA, DnsResult::aresAAAACallback, this); 
#else // !USE_IPV6
   lookupARecords(target);
#endif
}

void
DnsResult::lookupARecords(const Data& target)
{
   DebugLog (<< "Doing Host (A) lookup: " << target);
   ares_gethostbyname(mInterface.mChannel, target.c_str(), AF_INET, DnsResult::aresHostCallback, this);
}

void
DnsResult::lookupNAPTR()
{
   DebugLog (<< "Doing NAPTR lookup: " << mTarget);
   ares_query(mInterface.mChannel, mTarget.c_str(), C_IN, T_NAPTR, DnsResult::aresNAPTRCallback, this); 
}

void
DnsResult::lookupSRV(const Data& target)
{
   DebugLog (<< "Doing SRV lookup: " << target);
   ares_query(mInterface.mChannel, target.c_str(), C_IN, T_SRV, DnsResult::aresSRVCallback, this); 
}

void
DnsResult::aresHostCallback(void *arg, int status, struct hostent* result)
{
   DnsResult *thisp = reinterpret_cast<DnsResult*>(arg);
   DebugLog (<< "Received A result for: " << thisp->mTarget);
   thisp->processHost(status, result);
}

void
DnsResult::aresNAPTRCallback(void *arg, int status, unsigned char *abuf, int alen)
{
   DnsResult *thisp = reinterpret_cast<DnsResult*>(arg);
   DebugLog (<< "Received NAPTR result for: " << thisp->mTarget);
   thisp->processNAPTR(status, abuf, alen);
}


void
DnsResult::aresSRVCallback(void *arg, int status, unsigned char *abuf, int alen)
{
   DnsResult *thisp = reinterpret_cast<DnsResult*>(arg);
   DebugLog (<< "Received SRV result for: " << thisp->mTarget);
   thisp->processSRV(status, abuf, alen);
}

void
DnsResult::aresAAAACallback(void *arg, int status, unsigned char *abuf, int alen)
{
   DnsResult *thisp = reinterpret_cast<DnsResult*>(arg);
   DebugLog (<< "Received AAAA result for: " << thisp->mTarget);
   thisp->processAAAA(status, abuf, alen);
}

void
DnsResult::processNAPTR(int status, unsigned char* abuf, int alen)
{
   DebugLog (<< "DnsResult::processNAPTR() " << status);

   // This function assumes that the NAPTR query that caused this
   // callback is the ONLY outstanding query that might cause
   // a callback into this object
   if (mType == Destroyed)
   {
      destroy();
      return;
   }

   if (status == ARES_SUCCESS)
   {
      const unsigned char *aptr = abuf + HFIXEDSZ;
      int qdcount = DNS_HEADER_QDCOUNT(abuf);    /* question count */
      for (int i = 0; i < qdcount && aptr; i++)
      {
         aptr = skipDNSQuestion(aptr, abuf, alen);
      }
      
      int ancount = DNS_HEADER_ANCOUNT(abuf);    /* answer record count */
      for (int i = 0; i < ancount && aptr; i++)
      {
         NAPTR naptr;
         aptr = parseNAPTR(aptr, abuf, alen, naptr);
         
         if (aptr)
         {
            DebugLog (<< "Adding NAPTR record: " << naptr);
            if (mSips && naptr.service.find("SIPS") == 0)
            {
               if (mInterface.isSupported(naptr.service) && naptr < mPreferredNAPTR)
               {
                  mPreferredNAPTR = naptr;
                  DebugLog (<< "Picked preferred: " << mPreferredNAPTR);
               }
            }
            else if (mInterface.isSupported(naptr.service) && naptr < mPreferredNAPTR)
            {
               mPreferredNAPTR = naptr;
               DebugLog (<< "Picked preferred: " << mPreferredNAPTR);
            }
         }
      }

      // This means that dns / NAPTR is misconfigured for this client 
      if (mPreferredNAPTR.key.empty())
      {
         DebugLog (<< "No NAPTR records that are supported by this client");
         mType = Finished;
         mHandler->handle(this);
         return;
      }

      int nscount = DNS_HEADER_NSCOUNT(abuf);    /* name server record count */
      DebugLog (<< "Found " << nscount << " nameserver records");
      for (int i = 0; i < nscount && aptr; i++)
      {
         // this will ignore NS records
         aptr = parseAdditional(aptr, abuf, alen);
      }

      int arcount = DNS_HEADER_ARCOUNT(abuf);    /* additional record count */
      DebugLog (<< "Found " << arcount << " additional records");
      for (int i = 0; i < arcount && aptr; i++)
      {
         // this will store any related SRV and A records
         // don't store SRV records other than the one in the selected NAPTR
         aptr = parseAdditional(aptr, abuf, alen);
      }

      if (ancount == 0) // didn't find any NAPTR records
      {
         DebugLog (<< "There are no NAPTR records so do an SRV lookup instead");
         goto NAPTRFail; // same as if no NAPTR records
      }
      else if (mSRVResults.empty())
      {
         // didn't find any SRV records in Additional, so do another query

         assert(mSRVCount == 0);
         assert(!mPreferredNAPTR.replacement.empty());

         DebugLog (<< "No SRV record for " << mPreferredNAPTR.replacement << " in additional section");
         mType = Pending;
         mSRVCount++;
         lookupSRV(mPreferredNAPTR.replacement);
      }
      else
      {
         // This will fill in mResults based on the DNS result
         std::sort(mSRVResults.begin(),mSRVResults.end()); // !jf! uggh
         primeResults();
      }
   }
   else
   {
      {
         char* errmem=0;
         DebugLog (<< "NAPTR lookup failed: " << ares_strerror(status, &errmem));
         ares_free_errmem(errmem);
      }
      
      // This will result in no NAPTR results. In this case, send out SRV
      // queries for each supported protocol
     NAPTRFail:
      if (mSips)
      {
         lookupSRV("_sips._tcp." + mTarget);
         mSRVCount++;
      }
      else
      {
         // For now, don't add _sips._tcp in this case. 
         lookupSRV("_sip._tcp." + mTarget);
         mSRVCount++;
         lookupSRV("_sip._udp." + mTarget);
         mSRVCount++;
      }
      DebugLog (<< "Doing SRV query " << mSRVCount << " for " << mTarget);
   }
}

void
DnsResult::processSRV(int status, unsigned char* abuf, int alen)
{
   assert(mSRVCount>=0);
   mSRVCount--;
   DebugLog (<< "DnsResult::processSRV() " << mSRVCount << " status=" << status);

   // There could be multiple SRV queries outstanding, but there will be no
   // other DNS queries outstanding that might cause a callback into this
   // object.
   if (mType == Destroyed && mSRVCount == 0)
   {
      destroy();
      return;
   }

   if (status == ARES_SUCCESS)
   {
      const unsigned char *aptr = abuf + HFIXEDSZ;
      int qdcount = DNS_HEADER_QDCOUNT(abuf);    /* question count */
      for (int i = 0; i < qdcount && aptr; i++)
      {
         aptr = skipDNSQuestion(aptr, abuf, alen);
      }
      
      int ancount = DNS_HEADER_ANCOUNT(abuf);    /* answer record count */
      for (int i = 0; i < ancount && aptr; i++)
      {
         SRV srv;
         aptr = parseSRV(aptr, abuf, alen, srv);
         
         if (aptr)
         {
            if (srv.key.find("_sip._udp") == 0)
            {
               srv.transport = UDP;
            }
            else if (srv.key.find("_sip._tcp") == 0)
            {
               srv.transport = TCP;
            }
            else if (srv.key.find("_sips._tcp") == 0)
            {
               srv.transport = TLS;
            }
            else
            {
               DebugLog (<< "Skipping SRV " << srv.key);
               continue;
            }

            DebugLog (<< "Adding SRV record (no NAPTR): " << srv);
            mSRVResults.push_back(srv);
         }
      }

      int nscount = DNS_HEADER_NSCOUNT(abuf);    /* name server record count */
      DebugLog (<< "Found " << nscount << " nameserver records");
      for (int i = 0; i < nscount && aptr; i++)
      {
         // this will ignore NS records
         aptr = parseAdditional(aptr, abuf, alen);
      }
      
      int arcount = DNS_HEADER_ARCOUNT(abuf);    /* additional record count */
      DebugLog (<< "Found " << arcount << " additional records");
      for (int i = 0; i < arcount && aptr; i++)
      {
         // this will store any related A records
         aptr = parseAdditional(aptr, abuf, alen);
      }
   }
   else
   {
      char* errmem=0;
      DebugLog (<< "SRV lookup failed: " << ares_strerror(status, &errmem));
      ares_free_errmem(errmem);
   }

   // no outstanding queries 
   if (mSRVCount == 0) 
   {
      if (mSRVResults.empty())
      {
         if (mSips)
         {
            mTransport = TLS;
            mPort = 5061;
         }
         else
         {
            mTransport = UDP;
            mPort = 5060;
         }
         
         DebugLog (<< "No SRV records for " << mTarget << ". Trying A records");
         lookupAAAARecords(mTarget);
      }
      else
      {
         std::sort(mSRVResults.begin(),mSRVResults.end()); // !jf! uggh
         primeResults();
      }
   }
}

void
DnsResult::processAAAA(int status, unsigned char* abuf, int alen)
{
#ifdef USE_IPV6
   DebugLog (<< "DnsResult::processAAAA() " << status);
   // This function assumes that the AAAA query that caused this callback
   // is the _only_ outstanding DNS query that might result in a
   // callback into this function
   if ( mType == Destroyed )
   {
      destroy();
      return;
   }
   if (status == ARES_SUCCESS)
   {
     const unsigned char *aptr = abuf + HFIXEDSZ;

     int qdcount = DNS_HEADER_QDCOUNT(abuf); /*question count*/
     for (int i=0; i < qdcount && aptr; i++)
     {
       aptr = skipDNSQuestion(aptr, abuf, alen);
     }

     int ancount = DNS_HEADER_ANCOUNT(abuf); /*answer count*/
     for (int i=0; i < ancount && aptr ; i++)
     {
       struct in6_addr aaaa;
       aptr = parseAAAA(aptr,abuf,alen,&aaaa);
       if (aptr)
       {
         Tuple tuple(aaaa,mPort,mTransport);
         DebugLog (<< "Adding " << tuple << " to result set");

         // !jf! Should this be going directly into mResults or into mARecords
         // !jf! Check for duplicates? 
         mResults.push_back(tuple);
       }
     }
   }
   else
   {
      char* errmem=0;
      DebugLog (<< "Failed async dns query: " << ares_strerror(status, &errmem));
      ares_free_errmem(errmem);
   }
   lookupARecords(mPassHostFromAAAAtoA);
#else
	assert(0);
#endif
}

void
DnsResult::processHost(int status, struct hostent* result)
{
   DebugLog (<< "DnsResult::processHost() " << status);
   
   // This function assumes that the A query that caused this callback
   // is the _only_ outstanding DNS query that might result in a
   // callback into this function
   if ( mType == Destroyed )
   {
      destroy();
      return;
   }

   if (status == ARES_SUCCESS)
   {
      DebugLog (<< "DNS A lookup canonical name: " << result->h_name);
      for (char** pptr = result->h_addr_list; *pptr != 0; pptr++)
      {
         in_addr addr;
         addr.s_addr = *((u_int32_t*)(*pptr));
         Tuple tuple(addr, mPort, mTransport);
         DebugLog (<< "Adding " << tuple << " to result set");
         // !jf! Should this be going directly into mResults or into mARecords
         // !jf! Check for duplicates? 
         mResults.push_back(tuple);
      }
   }
   else
   {
      char* errmem=0;
      DebugLog (<< "Failed async A query: " << ares_strerror(status, &errmem));
      ares_free_errmem(errmem);
   }

   if (mSRVCount == 0)
   {
      bool changed = (mType == Pending);
      if (mResults.empty())
      {
         mType = Finished;
      }
      else 
      {
         mType = Available;
      }
      if (changed) mHandler->handle(this);
   }
}

void
DnsResult::primeResults()
{
#if !defined(WIN32) && !defined(__SUNPRO_CC) && !defined(__INTEL_COMPILER)
   DebugLog(<< "Priming " << Inserter(mSRVResults));
#endif

   //assert(mType != Pending);
   //assert(mType != Finished);
   assert(mResults.empty());

   if (!mSRVResults.empty())
   {
      SRV next = retrieveSRV();
      DebugLog (<< "Primed with SRV=" << next);
      if ( mARecords.count(next.target) 
#ifdef USE_IPV6 
           + mAAAARecords.count(next.target)
#endif
         )
      {
#ifdef USE_IPV6 
         std::list<struct in6_addr>& aaaarecs = mAAAARecords[next.target];
         for (std::list<struct in6_addr>::const_iterator i=aaaarecs.begin();
	         i!=aaaarecs.end(); i++)
         {
            Tuple tuple(*i,next.port,next.transport);
            DebugLog (<< "Adding " << tuple << " to result set");
            mResults.push_back(tuple);
         }
#endif
         std::list<struct in_addr>& arecs = mARecords[next.target];
         for (std::list<struct in_addr>::const_iterator i=arecs.begin(); i!=arecs.end(); i++)
         {
            Tuple tuple(*i, next.port, next.transport);
            DebugLog (<< "Adding " << tuple << " to result set");
            mResults.push_back(tuple);
         }
#if !defined(WIN32) && !defined(__SUNPRO_CC) && !defined(__INTEL_COMPILER)
         DebugLog (<< "Try: " << Inserter(mResults));
#endif


         bool changed = (mType == Pending);
         mType = Available;
         if (changed) mHandler->handle(this);

         // recurse if there are no results. This could happen if the DNS SRV
         // target record didn't have any A/AAAA records. This will terminate if we
         // run out of SRV results. 
         if (mResults.empty())
         {
            primeResults();
         }
      }
      else
      {
         // !jf! not going to test this for now
         // this occurs when the A records were not provided in the Additional
         // Records of the DNS result
         // we will need to store the SRV record that is being looked up so we
         // can populate the resulting Tuples 
         mType = Pending;
         mPort = next.port;
         mTransport = next.transport;
         DebugLog (<< "No A or AAAA record for " << next.target << " in additional records");
         lookupAAAARecords(next.target);
         // don't call primeResults since we need to wait for the response to
         // AAAA/A query first
      }
   }
   else
   {
      bool changed = (mType == Pending);
      mType = Finished;
      if (changed) mHandler->handle(this);
   }

   // Either we are finished or there are results primed
   assert(mType == Finished || 
          mType == Pending || 
          (mType == Available && !mResults.empty()));
}

// implement the selection algorithm from rfc2782 (SRV records)
DnsResult::SRV 
DnsResult::retrieveSRV()
{
   assert(!mSRVResults.empty());

   const SRV& srv = *mSRVResults.begin();
   if (srv.cumulativeWeight == 0)
   {
      int priority = srv.priority;
   
      mCumulativeWeight=0;
      for (std::vector<SRV>::iterator i=mSRVResults.begin(); 
           i!=mSRVResults.end() && i->priority == priority; i++)
      {
         mCumulativeWeight += i->weight;
         i->cumulativeWeight = mCumulativeWeight;
      }
   }
   
   int selected = Random::getRandom() % (mCumulativeWeight+1);

   DebugLog (<< "cumulative weight = " << mCumulativeWeight << " selected=" << selected);

   std::vector<SRV>::iterator i;
   for (i=mSRVResults.begin(); i!=mSRVResults.end(); i++)
   {
      if (i->cumulativeWeight >= selected)
      {
         break;
      }
   }
   
   if (i == mSRVResults.end())
   {
      InfoLog (<< "SRV Results problem selected=" << selected << " cum=" << mCumulativeWeight);
#if !defined(WIN32) && !defined(__SUNPRO_CC) && !defined(__INTEL_COMPILER)
      InfoLog (<< "SRV: " << Inserter(mSRVResults) ); // !cj! does not work in windows 
#endif
   }
   assert(i != mSRVResults.end());
   SRV next = *i;
   mCumulativeWeight -= next.cumulativeWeight;
   mSRVResults.erase(i);
   
#if !defined(WIN32) && !defined(__SUNPRO_CC) && !defined(__INTEL_COMPILER)
   DebugLog (<< "SRV: " << Inserter(mSRVResults));
#endif

   return next;
}


// adapted from the adig.c example in ares
const unsigned char* 
DnsResult::parseAdditional(const unsigned char *aptr,
                           const unsigned char *abuf, 
                           int alen)
{
   char *name=0;
   int len=0;
   int status=0;

   // Parse the RR name. 
   status = ares_expand_name(aptr, abuf, alen, &name, &len);
   if (status != ARES_SUCCESS)
   {
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
   aptr += len;

   /* Make sure there is enough data after the RR name for the fixed
    * part of the RR.
    */
   if (aptr + RRFIXEDSZ > abuf + alen)
   {
      DebugLog (<< "Failed parse of RR");
      free(name);
      return NULL;
   }
  
   // Parse the fixed part of the RR, and advance to the RR data
   // field. 
   int type = DNS_RR_TYPE(aptr);
   int dlen = DNS_RR_LEN(aptr);
   aptr += RRFIXEDSZ;
   if (aptr + dlen > abuf + alen)
   {
      DebugLog (<< "Failed parse of RR");
      free(name);
      return NULL;
   }

   // Display the RR data.  Don't touch aptr. 
   Data key = name;
   free(name);

   if (type == T_SRV)
   {
      SRV srv;
      
      // The RR data is three two-byte numbers representing the
      // priority, weight, and port, followed by a domain name.
      srv.key = key;
      srv.priority = DNS__16BIT(aptr);
      srv.weight = DNS__16BIT(aptr + 2);
      srv.port = DNS__16BIT(aptr + 4);
      status = ares_expand_name(aptr + 6, abuf, alen, &name, &len);
      if (status != ARES_SUCCESS)
      {
         return NULL;
      }
      srv.target = name;
      free(name);
      
      // Only add SRV results for the preferred NAPTR target as per rfc3263
      // (section 4.1). 
      assert(!mPreferredNAPTR.key.empty());
      if (srv.key == mPreferredNAPTR.replacement && srv.target != Symbols::DOT)
      {
         if (srv.key.find("_sip._udp") == 0)
         {
            srv.transport = UDP;
         }
         else if (srv.key.find("_sip._tcp") == 0)
         {
            srv.transport = TCP;
         }
         else if (srv.key.find("_sips._tcp") == 0)
         {
            srv.transport = TLS;
         }
         else
         {
            DebugLog (<< "Skipping SRV " << srv.key);
            return aptr + dlen;
         }
         
         DebugLog (<< "Inserting SRV: " << srv);
         mSRVResults.push_back(srv);
      }

      return aptr + dlen;

   }
   else if (type == T_A)
   {
      // The RR data is a four-byte Internet address. 
      if (dlen != 4)
      {
         DebugLog (<< "Failed parse of RR");
         return NULL;
      }

      struct in_addr addr;
      memcpy(&addr, aptr, sizeof(struct in_addr));
      DebugLog (<< "From additional: " << key << ":" << DnsUtil::inet_ntop(addr));

      // Only add the additional records if they weren't already there from
      // another query
      if (mARecords.count(key) == 0)
      {
         mARecords[key].push_back(addr);
      }
      return aptr + dlen;
   }
#ifdef USE_IPV6
   else if (type == T_AAAA)
   {
      if (dlen != 16) // The RR is 128 bits of ipv6 address
      {
         DebugLog (<< "Failed parse of RR");
         return NULL;
      }
      struct in6_addr addr;
      memcpy(&addr, aptr, sizeof(struct in6_addr));
      DebugLog (<< "From additional: " << key << ":" << DnsUtil::inet_ntop(addr));
      // Only add the additional records if they weren't already there from
      // another query
      if (mAAAARecords.count(key) == 0)
      {
         mAAAARecords[key].push_back(addr);
      }
      return aptr + dlen;
   }
#endif
   else // just skip it (we don't care :)
   {
      //DebugLog (<< "Skipping: " << key);
      return aptr + dlen;
   }
}

// adapted from the adig.c example in ares
const unsigned char* 
DnsResult::skipDNSQuestion(const unsigned char *aptr,
                           const unsigned char *abuf,
                           int alen)
{
   char *name=0;
   int status=0;
   int len=0;
   
   // Parse the question name. 
   status = ares_expand_name(aptr, abuf, alen, &name, &len);
   if (status != ARES_SUCCESS)
   {
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
   aptr += len;

   // Make sure there's enough data after the name for the fixed part
   // of the question.
   if (aptr + QFIXEDSZ > abuf + alen)
   {
      free(name);
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }

   // Parse the question type and class. 
   //int type = DNS_QUESTION_TYPE(aptr);
   //int dnsclass = DNS_QUESTION_CLASS(aptr);
   aptr += QFIXEDSZ;
   
   free(name);
   return aptr;
}

// adapted from the adig.c example in ares
const unsigned char* 
DnsResult::parseSRV(const unsigned char *aptr,
                    const unsigned char *abuf, 
                    int alen,
                    SRV& srv)
{
   char *name;
   int len=0;
   int status=0;

   // Parse the RR name. 
   status = ares_expand_name(aptr, abuf, alen, &name, &len);
   if (status != ARES_SUCCESS)
   {
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
   aptr += len;

   /* Make sure there is enough data after the RR name for the fixed
    * part of the RR.
    */
   if (aptr + RRFIXEDSZ > abuf + alen)
   {
      free(name);
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
  
   // Parse the fixed part of the RR, and advance to the RR data
   // field. 
   int type = DNS_RR_TYPE(aptr);
   int dlen = DNS_RR_LEN(aptr);
   aptr += RRFIXEDSZ;
   if (aptr + dlen > abuf + alen)
   {
      free(name);
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
   Data key = name;
   free(name);

   // Display the RR data.  Don't touch aptr. 
   if (type == T_SRV)
   {
      // The RR data is three two-byte numbers representing the
      // priority, weight, and port, followed by a domain name.
      srv.key = key;
      srv.priority = DNS__16BIT(aptr);
      srv.weight = DNS__16BIT(aptr + 2);
      srv.port = DNS__16BIT(aptr + 4);
      status = ares_expand_name(aptr + 6, abuf, alen, &name, &len);
      if (status != ARES_SUCCESS)
      {
         DebugLog (<< "Failed parse of RR");
         return NULL;
      }
      srv.target = name;
      free(name);

      return aptr + dlen;

   }
   else
   {
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
}
      

#ifdef USE_IPV6
// adapted from the adig.c example in ares
const unsigned char* 
DnsResult::parseAAAA(const unsigned char *aptr,
                     const unsigned char *abuf, 
                     int alen,
                     in6_addr * aaaa)
{
   char *name;
   int len=0;
   int status=0;

   // Parse the RR name. 
   status = ares_expand_name(aptr, abuf, alen, &name, &len);
   if (status != ARES_SUCCESS)
   {
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
   aptr += len;

   /* Make sure there is enough data after the RR name for the fixed
    * part of the RR.
    */
   if (aptr + RRFIXEDSZ > abuf + alen)
   {
      free(name);
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
  
   // Parse the fixed part of the RR, and advance to the RR data
   // field. 
   int type = DNS_RR_TYPE(aptr);
   int dlen = DNS_RR_LEN(aptr);
   aptr += RRFIXEDSZ;
   if (aptr + dlen > abuf + alen)
   {
      free(name);
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
   Data key = name;
   free(name);

   // Display the RR data.  Don't touch aptr. 

   if (type == T_AAAA)
   {
      // The RR data is 128 bits of ipv6 address in network byte
      // order
      memcpy(aaaa, aptr, sizeof(in6_addr));
      return aptr + dlen;
   }
   else

   {
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
}
#endif


// adapted from the adig.c example in ares
const unsigned char* 
DnsResult::parseNAPTR(const unsigned char *aptr,
                      const unsigned char *abuf, 
                      int alen,
                      NAPTR& naptr)
{
   const unsigned char* p=0;
   char* name=0;
   int len=0;
   int status=0;

   // Parse the RR name. 
   status = ares_expand_name(aptr, abuf, alen, &name, &len);
   if (status != ARES_SUCCESS)
   {
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
   aptr += len;

   // Make sure there is enough data after the RR name for the fixed
   // part of the RR.
   if (aptr + RRFIXEDSZ > abuf + alen)
   {
      free(name);
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
  
   // Parse the fixed part of the RR, and advance to the RR data
   // field. 
   int type = DNS_RR_TYPE(aptr);
   int dlen = DNS_RR_LEN(aptr);
   aptr += RRFIXEDSZ;
   if (aptr + dlen > abuf + alen)
   {
      free(name);
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }

   Data key = name;
   free(name);

   // Look at the RR data.  Don't touch aptr. 
   if (type == T_NAPTR)
   {
      // The RR data is two two-byte numbers representing the
      // order and preference, followed by three character strings
      // representing flags, services, a regex, and a domain name.

      naptr.key = key;
      naptr.order = DNS__16BIT(aptr);
      naptr.pref = DNS__16BIT(aptr + 2);
      p = aptr + 4;
      len = *p;
      if (p + len + 1 > aptr + dlen)
      {
         DebugLog (<< "Failed parse of RR");
         return NULL;
      }
      naptr.flags = Data(p+1, len);
      
      p += len + 1;
      len = *p;
      if (p + len + 1 > aptr + dlen)
      {
         DebugLog (<< "Failed parse of RR");
         return NULL;
      }
      naptr.service = Data(p+1, len);

      p += len + 1;
      len = *p;
      if (p + len + 1 > aptr + dlen)
      {
         DebugLog (<< "Failed parse of RR");
         return NULL;
      }
      naptr.regex = Data(p+1, len);

      p += len + 1;
      status = ares_expand_name(p, abuf, alen, &name, &len);
      if (status != ARES_SUCCESS)
      {
         DebugLog (<< "Failed parse of RR");
         return NULL;
      }
      naptr.replacement = name;
      
      free(name);
      return aptr + dlen;
   }
   else
   {
      DebugLog (<< "Failed parse of RR");
      return NULL;
   }
}

DnsResult::NAPTR::NAPTR() : order(0), pref(0)
{
}

bool 
DnsResult::NAPTR::operator<(const DnsResult::NAPTR& rhs) const
{
   if (key.empty()) // default value
   {
      return false;
   }
   else if (rhs.key.empty()) // default value
   {
      return true;
   }
   else if (order < rhs.order)
   {
      return true;
   }
   else if (order == rhs.order)
   {
      if (pref < rhs.pref)
      {
         return true;
      }
      else if (pref == rhs.pref)
      {
         return replacement < rhs.replacement;
      }
   }
   return false;
}

DnsResult::SRV::SRV() : priority(0), weight(0), cumulativeWeight(0), port(0)
{
}

bool 
DnsResult::SRV::operator<(const DnsResult::SRV& rhs) const
{
   if (priority < rhs.priority)
   {
      return true;
   }
   else if (priority == rhs.priority)
   {
      if (weight < rhs.weight)
      {
         return true;
      }
      else if (weight == rhs.weight)
      {
         if (transport < rhs.transport)
         {
            return true;
         }
         else if (transport == rhs.transport)
         {
            if (target < rhs.target)
            {
               return true;
            }
         }
      }
   }
   return false;
}

std::ostream& 
resip::operator<<(std::ostream& strm, const resip::DnsResult& result)
{
#if !defined(WIN32) && !defined(__SUNPRO_CC) && !defined(__INTEL_COMPILER)
   strm << result.mTarget << " --> " << Inserter(result.mResults);
#endif
   return strm;
}


std::ostream& 
resip::operator<<(std::ostream& strm, const resip::DnsResult::NAPTR& naptr)
{
   strm << "key=" << naptr.key
        << " order=" << naptr.order
        << " pref=" << naptr.pref
        << " flags=" << naptr.flags
        << " service=" << naptr.service
        << " regex=" << naptr.regex
        << " replacement=" << naptr.replacement;
   return strm;
}

std::ostream& 
resip::operator<<(std::ostream& strm, const resip::DnsResult::SRV& srv)
{
   strm << "key=" << srv.key
        << " t=" << Tuple::toData(srv.transport) 
        << " p=" << srv.priority
        << " w=" << srv.weight
        << " c=" << srv.cumulativeWeight
        << " port=" << srv.port
        << " target=" << srv.target;
   return strm;
}


//  Copyright (c) 2003, Jason Fischl 
/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */

