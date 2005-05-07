#include <vector>
#include <list>

#include "resiprocate/os/compat.hxx"
#include "resiprocate/os/BaseException.hxx"
#include "resiprocate/os/Timer.hxx"
#include "resiprocate/dns/DnsResourceRecord.hxx"
#include "resiprocate/dns/RRFactory.hxx"
#include "resiprocate/dns/RRList.hxx"

using namespace resip;
using namespace std;

RRList::RRList() : mRRType(0), mStatus(0), mAbsoluteRetryAfter(0), mAbsoluteExpiry(ULONG_MAX) {}

RRList::RRList(const Data& key, 
                const int rrtype, 
                int ttl, 
                int status)
   : mKey(key), mRRType(rrtype), mStatus(status), mAbsoluteRetryAfter(0)
{
   mAbsoluteExpiry = ttl + Timer::getTimeMs()/1000;
}

RRList::RRList(const Data& key, int rrtype)
   : mKey(key), mRRType(rrtype), mStatus(0), mAbsoluteRetryAfter(0), mAbsoluteExpiry(ULONG_MAX)
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
   : mKey(key), mRRType(rrType), mStatus(0), mAbsoluteRetryAfter(0)
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
      RecordItem item;
      item.record = factory->create(*it);
      mRecords.push_back(item);
      if ((UInt64)it->ttl() < mAbsoluteExpiry)
      {
         mAbsoluteExpiry = it->ttl();
      }
   }
   mAbsoluteExpiry += Timer::getTimeMs()/1000;
}

RRList::Records RRList::records(const int protocol, int& retryAfter, bool& allBlacklisted)
{
   Records records;
   retryAfter = 0;
   allBlacklisted = false;
   if (mRecords.empty()) return records;

   retryAfter  = (int)(mAbsoluteRetryAfter - Timer::getTimeMs()/1000);
   if (retryAfter < 0) 
   {
      retryAfter = 0;
      mAbsoluteRetryAfter = 0;
   }

   bool retry = false;
   for (std::vector<RecordItem>::iterator it = mRecords.begin(); it != mRecords.end(); ++it)
   {
      if ((*it).states.empty())
      {
         records.push_back((*it).record);
      }
      else if (!(*it).states[protocol].blacklisted)
      {
         if (mAbsoluteRetryAfter == 0)
         {
            records.push_back((*it).record);
            (*it).states[protocol].retryAfter = false;
         }
         else if (!(*it).states[protocol].retryAfter)
         {
            records.push_back((*it).record);
         }
         else
         {
            retry = true;
         }
      }
   }
   if (records.empty())
   {
      if (!retry)
      {
         // every record is blacklisted.
         // two options:
         //    1. reset the states and return all the records.
         //    2. get caller to remove the cache.
         // Option 2 is used in this implementation.
         allBlacklisted = true;
         retryAfter = 0;
      }
   }
   else
   {
      retryAfter = 0;
   }

   return records;
}

void RRList::blacklist(const int protocol,
                       const DataArr& targets)
{
   for (DataArr::const_iterator it = targets.begin(); it != targets.end(); ++it)
   {
      RecordItr recordItr = find(*it);
      if (recordItr != mRecords.end())
      {
         if ((*recordItr).states.empty())
         {
            initStates((*recordItr).states);
         }
         (*recordItr).states[protocol].blacklisted = true;
      }
   }
}

void RRList::retryAfter(const int protocol,
                        const int retryAfter,
                        const DataArr& targets)
{
   bool retry = false;
   for (DataArr::const_iterator it = targets.begin(); it != targets.end(); ++it)
   {
      RecordItr recordItr = find(*it);
      if (recordItr != mRecords.end())
      {
         if ((*recordItr).states.empty())
         {
            initStates((*recordItr).states);
         }
         (*recordItr).states[protocol].retryAfter = true;
         retry = true;
      }
   }
   if (retry) mAbsoluteRetryAfter = retryAfter +  Timer::getTimeMs()/1000;
}

void RRList::initStates(States& states)
{
   RecordState state;
   state.blacklisted = false;
   state.retryAfter = false;
   for (int i = 0; i < Protocol::Total; ++i)
   {
      states.push_back(state);
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
