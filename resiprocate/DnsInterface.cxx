#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#if defined(USE_ARES)
extern "C"
{
#include "ares.h"
#include "ares_dns.h"
}
#endif

#include "resiprocate/os/compat.hxx"
#include "resiprocate/os/Logger.hxx"

#include "resiprocate/DnsInterface.hxx"
#include "resiprocate/DnsHandler.hxx"
#include "resiprocate/DnsResult.hxx"

#include "resiprocate/ExternalDnsFactory.hxx"
#include "resiprocate/os/WinLeakCheck.hxx"


using namespace resip;

#define RESIPROCATE_SUBSYSTEM resip::Subsystem::DNS

DnsInterface::DnsInterface() : 
   mDnsProvider(ExternalDnsFactory::createExternalDns()),
   	  mActiveQueryCount(0)
{
   int retCode = mDnsProvider->init();
   if (retCode != 0)
   {
      ErrLog (<< "Failed to initialize async dns library");
      char* errmem = mDnsProvider->errorMessage(retCode);      
      ErrLog (<< errmem);
      delete errmem;
      throw Exception("failed to initialize async dns library", __FILE__,__LINE__);
   }
}

DnsInterface::~DnsInterface()
{
   delete mDnsProvider;
}

void 
DnsInterface::lookupARecords(const Data& target, DnsResult* dres)
{
   mDnsProvider->lookupARecords(target.c_str(), this, dres);
}

void 
DnsInterface::lookupAAAARecords(const Data& target, DnsResult* dres)
{
   mDnsProvider->lookupAAAARecords(target.c_str(), this, dres);
}

void 
DnsInterface::lookupNAPTR(const Data& target, DnsResult* dres)
{
   mDnsProvider->lookupNAPTR(target.c_str(), this, dres);
}

void 
DnsInterface::lookupSRV(const Data& target, DnsResult* dres)
{
   mDnsProvider->lookupSRV(target.c_str(), this, dres);
}

Data 
DnsInterface::errorMessage(int status)
{
   return Data(Data::Take, mDnsProvider->errorMessage(status));
}

void 
DnsInterface::addTransportType(TransportType type, IpVersion version)
{
   static Data Udp("SIP+D2U");
   static Data Tcp("SIP+D2T");
   static Data Tls("SIPS+D2T");
   static Data Dtls("SIPS+D2U");

   mSupportedTransports.push_back(std::make_pair(type, version));
   switch (type)
   {
      case UDP:
         mSupportedNaptrs.insert(Udp);
         break;
      case TCP:
         mSupportedNaptrs.insert(Tcp);
         break;
      case TLS:
         mSupportedNaptrs.insert(Tls);
         break;
      case DTLS:
         mSupportedNaptrs.insert(Dtls);
         break;         
      default:
         assert(0);
         break;
   }
}

bool
DnsInterface::isSupported(const Data& service)
{
   return mSupportedNaptrs.count(service) != 0;
}

bool
DnsInterface::isSupported(TransportType t, IpVersion version)
{
   return std::find(mSupportedTransports.begin(), mSupportedTransports.end(), std::make_pair(t, version)) != mSupportedTransports.end();
}

bool
DnsInterface::isSupportedProtocol(TransportType t)
{
   for (TransportMap::const_iterator i=mSupportedTransports.begin(); i != mSupportedTransports.end(); ++i)
   {
      if (i->first == t)
      {
         return true;
      }
   }
   return false;
}

bool 
DnsInterface::requiresProcess() const
{
   return mDnsProvider->requiresProcess() && mActiveQueryCount;   
}

void 
DnsInterface::buildFdSet(FdSet& fdset) const
{
   mDnsProvider->buildFdSet(fdset.read, fdset.write, fdset.size);
}

void 
DnsInterface::process(FdSet& fdset)
{
   mDnsProvider->process(fdset.read, fdset.write);
}


DnsResult*
DnsInterface::createDnsResult(DnsHandler* handler)
{
   DnsResult* result = new DnsResult(*this, handler);
   mActiveQueryCount++;  
   return result;
}

void 
DnsInterface::lookup(DnsResult* res, const Uri& uri)
{
   res->lookup(uri);   
}



// DnsResult* 
// DnsInterface::lookup(const Via& via, DnsHandler* handler)
// {
//    assert(0);
//    //DnsResult* result = new DnsResult(*this);
//    return NULL;
// }

void 
DnsInterface::handle_NAPTR(ExternalDnsRawResult res)
{
   reinterpret_cast<DnsResult*>(res.userData)->processNAPTR(res.errorCode(), res.abuf, res.alen);
   mDnsProvider->freeResult(res);
}

void 
DnsInterface::handle_SRV(ExternalDnsRawResult res)
{
   reinterpret_cast<DnsResult*>(res.userData)->processSRV(res.errorCode(), res.abuf, res.alen);
   mDnsProvider->freeResult(res);
}

void 
DnsInterface::handle_AAAA(ExternalDnsRawResult res)
{
   reinterpret_cast<DnsResult*>(res.userData)->processAAAA(res.errorCode(), res.abuf, res.alen);
   mDnsProvider->freeResult(res);
}

void 
DnsInterface::handle_host(ExternalDnsHostResult res)
{
   reinterpret_cast<DnsResult*>(res.userData)->processHost(res.errorCode(), res.host);
   mDnsProvider->freeResult(res);
}

//?dcm? -- why is this here?
DnsHandler::~DnsHandler()
{
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

