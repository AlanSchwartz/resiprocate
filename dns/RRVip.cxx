extern "C"
{
#include "ares.h"
#include "ares_dns.h"
}

#include <map>
#include <list>
#include <vector>

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

#include "resiprocate/os/BaseException.hxx"
#include "resiprocate/dns/DnsResourceRecord.hxx"
#include "resiprocate/dns/DnsAAAARecord.hxx"
#include "resiprocate/dns/DnsHostRecord.hxx"
#include "resiprocate/dns/DnsNaptrRecord.hxx"
#include "resiprocate/dns/DnsSrvRecord.hxx"
#include "resiprocate/dns/RRVip.hxx"

using namespace resip;
using namespace std;

RRVip::RRVip()
{
   mFactories[T_A] = new HostTransformFactory;
   mFactories[T_AAAA] = new HostTransformFactory;
   mFactories[T_NAPTR] = new NaptrTransformFactroy;
   mFactories[T_SRV] = new SrvTransformFactory;
}

RRVip::~RRVip()
{
   for (map<MapKey, Transform*>::iterator it = mTransforms.begin(); it != mTransforms.end(); ++it)
   {
      delete (*it).second;
   }

   for (TransformFactoryMap::iterator it = mFactories.begin(); it != mFactories.end(); ++it)
   {
      delete (*it).second;
   }
}

void RRVip::addListener(int rrType, const Listener* listener)
{
   ListenerMap::iterator it = mListenerMap.find(rrType);
   if (it == mListenerMap.end())
   {
      Listeners lst;
      lst.push_back(listener);
      mListenerMap.insert(ListenerMap::value_type(rrType, lst));
   }
   else
   {
      for (Listeners::iterator itr = it->second.begin(); itr != it->second.end(); ++itr)
      {
         if ((*itr) == listener) return;
      }
      it->second.push_back(listener);
   }
}

void RRVip::removeListener(int rrType, const Listener* listener)
{
   ListenerMap::iterator it = mListenerMap.find(rrType);
   if (it != mListenerMap.end())
   {
      for (Listeners::iterator itr = it->second.begin(); itr != it->second.end(); ++itr)
      {
         if ((*itr) == listener)
         {
            it->second.erase(itr);
            break;
         }
      }
   }
}

void RRVip::vip(const Data& target,
                int rrType,
                const Data& vip)
{
   RRVip::MapKey key(target, rrType);
   TransformMap::iterator it = mTransforms.find(key);
   if (it != mTransforms.end())
   {
      it->second->updateVip(vip);
   }
   else
   {
      TransformFactoryMap::iterator it = mFactories.find(rrType);
      assert(it != mFactories.end());
      Transform* transform = it->second->createTransform(vip);
      mTransforms.insert(TransformMap::value_type(key, transform));
   }
}

void RRVip::transform(const Data& target,
                      int rrType,
                      std::vector<DnsResourceRecord*>& src)
{
   RRVip::MapKey key(target, rrType);
   TransformMap::iterator it = mTransforms.find(key);
   if (it != mTransforms.end())
   {
      bool invalidVip = false;
      it->second->transform(src, invalidVip);
      if (invalidVip) 
      {
         Data vip = it->second->vip();
         mTransforms.erase(it);
         ListenerMap::iterator it = mListenerMap.find(rrType);
         if (it != mListenerMap.end())
         {
            for (Listeners::const_iterator itL = (*it).second.begin(); itL != (*it).second.end(); ++itL)
            {
               (*itL)->onVipInvalidated(rrType, vip);
            }
         }
      }
   }
}

RRVip::Transform::Transform(const Data& vip) 
   : mVip(vip)
{
}

RRVip::Transform::~Transform()
{
}

void RRVip::Transform::updateVip(const Data& vip)
{
   mVip = vip;
}

void RRVip::Transform::transform(RRVector& src,
                                 bool& invalidVip)
{
   invalidVip = true;
   for (RRVector::iterator it = src.begin(); it != src.end(); ++it)
   {
      if ((*it)->isSameValue(mVip))
      {
         invalidVip = false;
         break;
      }
   }
   if(!invalidVip)
   {
      if (src.begin() != it)
      {
         DnsResourceRecord* vip = *it;
         src.erase(it);
         src.insert(src.begin(), vip);
      }
   }
}

RRVip::NaptrTransform::NaptrTransform(const Data& vip)
   : Transform(vip)
{
}

void RRVip::NaptrTransform::transform(RRVector& src,
                                      bool& invalidVip)
{
   invalidVip = true;
   RRVector::iterator vip;
   for (RRVector::iterator it = src.begin(); it != src.end(); ++it)
   {
      if ((*it)->isSameValue(mVip))
      {
         invalidVip = false;
         vip = it;
         break;
      }
   }
   if(!invalidVip)
   {
      int min = dynamic_cast<DnsNaptrRecord*>(*(src.begin()))->order();
      for (RRVector::iterator it = src.begin(); it != src.end(); ++it)
      {
         int order = ((dynamic_cast<DnsNaptrRecord*>(*it))->order())++;
         if (order < min) min = order;
      }
      dynamic_cast<DnsNaptrRecord*>((*vip))->order() = min;
   }
}

RRVip::SrvTransform::SrvTransform(const Data& vip)
   : Transform(vip)
{
}

void RRVip::SrvTransform::transform(RRVector& src,
                                    bool& invalidVip)
{
   invalidVip = true;
   RRVector::iterator vip;
   for (RRVector::iterator it = src.begin(); it != src.end(); ++it)
   {
      if ((*it)->isSameValue(mVip))
      {
         invalidVip = false;
         vip = it;
         break;
      }
   }
   if(!invalidVip)
   {
      int min = dynamic_cast<DnsSrvRecord*>(*(src.begin()))->priority();
      for (RRVector::iterator it = src.begin(); it != src.end(); ++it)
      {
         int priority = ((dynamic_cast<DnsSrvRecord*>(*it))->priority())++;
         if (priority < min) min = priority;
      }
      dynamic_cast<DnsSrvRecord*>((*vip))->priority() = min;
   }
}

RRVip::MapKey::MapKey()
{
}

RRVip::MapKey::MapKey(const Data& target, int rrType)
   : mTarget(target), 
     mRRType(rrType)
{
}

bool RRVip::MapKey::operator<(const MapKey& rhs) const
{
   if (mRRType < rhs.mRRType)
   {
      return true;
   }
   else if (mRRType > rhs.mRRType)
   {
      return false;
   }
   else
   {
      return mTarget < rhs.mTarget;
   }
}
