#if defined(HAVE_CONFIG_H)
#include "rutil/config.hxx"
#endif

#include <vector>
#include <list>

#include "ares.h"

#include "rutil/Logger.hxx"
#include "rutil/compat.hxx"
#include "rutil/BaseException.hxx"
#include "rutil/Timer.hxx"
#include "rutil/DnsUtil.hxx"
#include "rutil/dns/DnsResourceRecord.hxx"
#include "rutil/dns/RRFactory.hxx"
#include "rutil/dns/RRList.hxx"
#include "rutil/dns/DnsAAAARecord.hxx"
#include "rutil/dns/DnsHostRecord.hxx"
#include "rutil/dns/DnsNaptrRecord.hxx"
#include "rutil/dns/DnsSrvRecord.hxx"
#include "rutil/dns/DnsCnameRecord.hxx"

using namespace resip;
using namespace std;

#define RESIPROCATE_SUBSYSTEM resip::Subsystem::DNS

RRList::RRList() : mRRType(0), mStatus(0), mAbsoluteExpiry(ULONG_MAX) {}

RRList::RRList(const Data& key, 
               const int rrtype, 
               int ttl, 
               int status)
   : mKey(key), mRRType(rrtype), mStatus(status)
{
   mAbsoluteExpiry = ttl + Timer::getTimeMs()/1000;
}

RRList::RRList(const Data& key, int rrtype)
   : mKey(key), mRRType(rrtype), mStatus(0), mAbsoluteExpiry(ULONG_MAX)
{}

RRList::~RRList()
{
   this->clear();
}

RRList::RRList(const RRFactoryBase* factory, 
               const Data& key,
               const int rrType,
               Itr begin,
               Itr end, 
               int ttl)
   : mKey(key), mRRType(rrType), mStatus(0)
{
   update(factory, begin, end, ttl);
}
      
void RRList::update(const RRFactoryBase* factory, Itr begin, Itr end, int ttl)
{
   this->clear();
   mAbsoluteExpiry = ULONG_MAX;
   
   for (Itr it = begin; it != end; it++)
   {
      try
      {
         RecordItem item;
         item.record = factory->create(*it);
         mRecords.push_back(item);
         if ((UInt64)it->ttl() < mAbsoluteExpiry)
         {
            mAbsoluteExpiry = it->ttl();
         }
      }
      catch (BaseException& e)
      {
         ErrLog(<< e.getMessage() << endl);
      }
   }

   if (mAbsoluteExpiry < (UInt64)ttl)
   {
      mAbsoluteExpiry = ttl;
   }

   mAbsoluteExpiry += Timer::getTimeMs()/1000;
}

RRList::Records RRList::records(const int protocol, bool& allBlacklisted)
{
   Records records;
   allBlacklisted = false;
   if (mRecords.empty()) return records;

   for (std::vector<RecordItem>::iterator it = mRecords.begin(); it != mRecords.end(); ++it)
   {
      if (!isBlacklisted(*it, protocol))
      {
         records.push_back((*it).record);
      }
   }
   if (records.empty())
   {
      // every record is blacklisted.
      // two options:
      //    1. reset the states and return all the records.
      //    2. get caller to remove the cache and requery.
      // Option 2 is used in this implementation.
      allBlacklisted = true;
   }
   return records;
}

void RRList::blacklist(const int protocol,
                       const DataArr& targets)
{
   if (protocol == Protocol::Reserved) return;

   for (DataArr::const_iterator it = targets.begin(); it != targets.end(); ++it)
   {
      RecordItr recordItr = find(*it);
      if (recordItr != mRecords.end())
      {
         blacklist(*recordItr, protocol);
      }
   }
}

RRList::RecordItr RRList::find(const Data& value)
{
   for (RecordItr it = mRecords.begin(); it != mRecords.end(); ++it)
   {
      if ((*it).record->isSameValue(value))
      {
         return it;
      }
   }
   return mRecords.end();
}

void RRList::clear()
{
   for (RecordArr::iterator it = mRecords.begin(); it != mRecords.end(); ++it)
   {
      delete (*it).record;
   }
   mRecords.clear();
}

bool RRList::isBlacklisted(RecordItem& item, int protocol)
{
   if (protocol == Protocol::Reserved) return false;

   vector<int>::iterator it;
   for (it = item.blacklistedProtocols.begin(); it != item.blacklistedProtocols.end(); ++it)
   {
      if (protocol == *it)
      {
         break;
      }
   }

   return item.blacklistedProtocols.end()!=it;
}

void RRList::blacklist(RecordItem& item, int protocol)
{
   if (Protocol::Reserved == protocol) return;

   vector<int>::iterator it;
   for (it = item.blacklistedProtocols.begin(); it != item.blacklistedProtocols.end(); ++it)
   {
      if (protocol == *it)
      {
         break;
      }
   }

   if (item.blacklistedProtocols.end() == it)
   {
      item.blacklistedProtocols.push_back(protocol);
   }
}

void RRList::log()
{
   for (RecordArr::iterator it = mRecords.begin(); it != mRecords.end(); ++it)
   {
      Data buffer;
      DataStream strm(buffer);

      strm << "DNSCACHE: Type=";

      switch(mRRType)
      {
      case T_CNAME:
      {
         DnsCnameRecord* record = dynamic_cast<DnsCnameRecord*>((*it).record);
         assert(record);
         strm << "CNAME: " << record->name() << " -> " << record->cname();
         break;
      }

      case T_NAPTR:
      {
         DnsNaptrRecord* record = dynamic_cast<DnsNaptrRecord*>((*it).record);
         assert(record);
         strm << "NAPTR: " << record->name() << " -> repl=" << record->replacement() << " service=" << record->service() 
              << " order=" << record->order() << " pref=" << record->preference() << " flags=" << record->flags() 
              << " regexp=" << record->regexp().regexp();
         break;
      }

      case T_SRV:
      {
         DnsSrvRecord* record = dynamic_cast<DnsSrvRecord*>((*it).record);
         assert(record);
         strm << "SRV: " << record->name() << " -> " << record->target() << ":" << record->port() 
              << " priority=" << record->priority() << " weight=" << record->weight();               
         break;
      }

#ifdef USE_IPV6
      case T_AAAA:
      {
         DnsAAAARecord* record = dynamic_cast<DnsAAAARecord*>((*it).record);
         assert(record);
         strm << "AAAA(Host): " << record->name() << " -> " << DnsUtil::inet_ntop(record->v6Address());
         break;
      }
#endif

      case T_A:
      {
         DnsHostRecord* record = dynamic_cast<DnsHostRecord*>((*it).record);
         assert(record);
         strm << "A(Host): " << record->name() << " -> " << record->host();
         break;
      }
      default:
         strm << "UNKNOWN(" << mRRType << ")" << " key=" << mKey << " name=" << (*it).record->name();
         break;
      }
      
      if((*it).blacklistedProtocols.size() > 0)
      {
          strm << " **blacklisted for protocols=";
          for(std::vector<int>::iterator it2 = (*it).blacklistedProtocols.begin(); 
              it2 != (*it).blacklistedProtocols.end(); it2++)
          {
              switch((*it2))
              {
              case Protocol::Sip:
                strm << "SIP ";
                break;
              case Protocol::Stun:
                strm << "STUN ";
                break;
              case Protocol::Http:
                strm << "HTTP ";
                break;
              case Protocol::Enum:
                strm << "ENUM ";
                break;
              default:
                strm << "UNKNOWN(" << (*it2) << ") ";
                break;
              }
          }
      }
      strm.flush();
      WarningLog( << buffer);
   }
}
