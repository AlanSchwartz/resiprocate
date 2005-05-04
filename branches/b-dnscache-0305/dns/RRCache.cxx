extern "C"
{
#include "ares.h"
#include "ares_dns.h"
}

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
#else
#include <Winsock2.h>
#include <svcguid.h>
#ifdef USE_IPV6
#include <ws2tcpip.h>
#endif
#endif

#include <set>
#include <vector>
#include <list>
#include <map>
#include <cassert>
#include "resiprocate/os/BaseException.hxx"
#include "resiprocate/os/Data.hxx"
#include "resiprocate/os/Timer.hxx"
#include "resiprocate/dns/RRFactory.hxx"
#include "resiprocate/dns/RROverlay.hxx"
#include "resiprocate/dns/RRFactory.hxx"
#include "resiprocate/dns/DnsResourceRecord.hxx"
#include "resiprocate/dns/DnsAAAARecord.hxx"
#include "resiprocate/dns/DnsHostRecord.hxx"
#include "resiprocate/dns/DnsNaptrRecord.hxx"
#include "resiprocate/dns/DnsSrvRecord.hxx"
#include "resiprocate/dns/DnsCnameRecord.hxx"
#include "resiprocate/dns/RRCache.hxx"

using namespace resip;
using namespace std;

RRCache::RRCache() 
   : mHead(),
     mLruHead(LruListType::makeList(&mHead)),
     mUserDefinedTTL(-1),
     mSize(DEFAULT_SIZE)
{
   mFactoryMap[T_CNAME] = &mCnameRecordFactory;
   mFactoryMap[T_NAPTR] = &mNaptrRecordFacotry;
   mFactoryMap[T_SRV] = &mSrvRecordFactory;
#ifdef USE_IPV6
   mFactoryMap[T_AAAA] = &mAAAARecordFactory;
#endif
   mFactoryMap[T_A] = &mHostRecordFactory;
}

RRCache::~RRCache()
{
   cleanup();
}

void RRCache::updateCache(const Data& target,
                          const int rrType,
                          Itr begin, 
                          Itr end)
{
   Data domain = (*begin).domain();
   FactoryMap::iterator it = mFactoryMap.find(rrType);
   assert(it != mFactoryMap.end());
   RRList* key = new RRList(domain, rrType);         
   RRSet::iterator lb = mRRSet.lower_bound(key);
   if (lb != mRRSet.end() &&
       !(mRRSet.key_comp()(key, *lb)))
   {
      (*lb)->update(it->second, begin, end, mUserDefinedTTL);
      touch(*lb);
   }
   else
   {
      RRList* val = new RRList(it->second, domain, rrType, begin, end, mUserDefinedTTL);
      mRRSet.insert(val);
      mLruHead->push_back(val);
      purge();
   }
   delete key;
}

void RRCache::cacheTTL(const Data& target,                    
                       const int rrType,
                       const int status,
                       RROverlay overlay)
{
   int ttl = getTTL(overlay);
   if (ttl < 0) return; // not soa.
   if (mUserDefinedTTL > 0)
   {
      ttl = ttl < mUserDefinedTTL? ttl : mUserDefinedTTL;
   }
   RRList* val = new RRList(target, rrType, ttl, status);
   RRSet::iterator it = mRRSet.find(val);
   if (it != mRRSet.end())
   {
      (*it)->remove();
      delete *it;
      mRRSet.erase(it);
   }
   mRRSet.insert(val);
   mLruHead->push_back(val);
   purge();
}

bool RRCache::lookup(const Data& target, 
                     const int type, 
                     const int protocol,
                     Result& records, 
                     int& status,
                     int& retryAfter)
{
   retryAfter = 0;
   records.empty();
   status = 0;
   RRList* key = new RRList(target, type);
   RRSet::iterator it = mRRSet.find(key);
   delete key;
   if (it == mRRSet.end())
   {
      return false;
   }
   else
   {
      if (Timer::getTimeMs()/1000 >= (*it)->absoluteExpiry())
      {
         delete *it;
         mRRSet.erase(it);
         return false;
      }
      else
      {
         bool allBlacklisted = false;
         records = (*it)->records(protocol, retryAfter, allBlacklisted);
         if (allBlacklisted)
         {
            assert(records.empty());
            (*it)->remove();
            delete *it;
            mRRSet.erase(it);
            return false;
         }
         else
         {
            status = (*it)->status();
            touch(*it);
            return true;
         }
      }
   }
}

void RRCache::blacklist(const Data& target, 
                        const int type,
                        const int protocol,
                        const DataArr& targetsToBlacklist)
{
   RRList* key = new RRList(target, type);
   RRSet::iterator it = mRRSet.find(key);
   delete key;
   if (it == mRRSet.end()) return;
   (*it)->blacklist(protocol, targetsToBlacklist);
}

void RRCache::retryAfter(const Data& target,
                         const int type,
                         const int protocol,
                         const int retryAfter,
                         const DataArr& targetsToRetryAfter)
{
   RRList* key = new RRList(target, type);
   RRSet::iterator it = mRRSet.find(key);
   delete key;
   if (it == mRRSet.end()) return;
   (*it)->retryAfter(protocol, retryAfter, targetsToRetryAfter);
}

void RRCache::touch(RRList* node)
{
   node->remove();
   mLruHead->push_back(node);
}

void RRCache::cleanup()
{
   for (std::set<RRList*, CompareT>::iterator it = mRRSet.begin(); it != mRRSet.end(); it++)
   {
      (*it)->remove();
      delete *it;
   }
   mRRSet.clear();
}

int RRCache::getTTL(const RROverlay& overlay)
{
   // overlay is a soa answer.
   if (overlay.type() != T_SOA) return -1;
   char* name = 0;
   int len = 0;
   ares_expand_name(overlay.data(), overlay.msg(), overlay.msgLength(), &name, &len);
   const unsigned char* pPos = overlay.data() + len;
   free(name);
   ares_expand_name(pPos, overlay.msg(), overlay.msgLength(), &name, &len);
   free(name);
   pPos += len;
   pPos += 16; // skip four 32 bit entities.
   return DNS__32BIT(pPos);         
}

void RRCache::purge()
{
   if (mRRSet.size() < mSize) return;
   RRList* lst = *(mLruHead->begin());
   RRSet::iterator it = mRRSet.find(lst);
   assert(it != mRRSet.end());
   lst->remove();
   delete *it;
   mRRSet.erase(it);
}
