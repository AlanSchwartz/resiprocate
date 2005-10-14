#if defined(HAVE_CONFIG_H)
#include "rutil/config.hxx"
#endif

#include <vector>
#include <list>

#include "rutil/Logger.hxx"
#include "rutil/compat.hxx"
#include "rutil/BaseException.hxx"
#include "rutil/Timer.hxx"
#include "rutil/dns/DnsResourceRecord.hxx"
#include "rutil/dns/RRFactory.hxx"
#include "rutil/dns/RRList.hxx"

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
   if (ttl >= 0)
   {
      mAbsoluteExpiry = ttl * MIN_TO_SEC;
   }
   else
   {
      mAbsoluteExpiry = ULONG_MAX;
   }

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

   if (mAbsoluteExpiry == 0)
   {
      // in case of 0 ttl, giving it 10 seconds to live in the cache.
      // not ideal solution, will fix it properly later.
      mAbsoluteExpiry = 10;
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
