#include "resiprocate/AresDns.hxx"

#include "resiprocate/os/WinLeakCheck.hxx"

extern "C"
{
#include "ares.h"
#include "ares_dns.h"
}

#if !defined(USE_ARES)
#error Must have ARES
#endif

#if !defined(WIN32)
#include <arpa/nameser.h>
#endif

using namespace resip;

int 
AresDns::init()
{
   int status;
   if ((status = ares_init(&mChannel)) != ARES_SUCCESS)
   {
      return status;
   }
   else
   {
      return 0;
   }
}

AresDns::~AresDns()
{
   ares_destroy(mChannel);
}

void 
AresDns::lookupARecords(const char* target, ExternalDnsHandler* handler, void* userData)
{
   ares_gethostbyname(mChannel, target, AF_INET, AresDns::aresHostCallback, new Payload(handler, userData));
}

void 
AresDns::lookupAAAARecords(const char* target, ExternalDnsHandler* handler, void* userData)
{
   ares_query(mChannel, target, C_IN, T_AAAA, AresDns::aresAAAACallback, new Payload(handler, userData)); 
}

void 
AresDns::lookupNAPTR(const char* target, ExternalDnsHandler* handler, void* userData)
{
   ares_query(mChannel, target, C_IN, T_NAPTR, AresDns::aresNAPTRCallback, new Payload(handler, userData)); 
}

void 
AresDns::lookupSRV(const char* target, ExternalDnsHandler* handler, void* userData)    
{
   ares_query(mChannel, target, C_IN, T_SRV, AresDns::aresSRVCallback, new Payload(handler, userData)); 
}

ExternalDnsHandler* 
AresDns::getHandler(void* arg)
{
   Payload* p = reinterpret_cast<Payload*>(arg);
   ExternalDnsHandler *thisp = reinterpret_cast<ExternalDnsHandler*>(p->first);
   return thisp;
}

ExternalDnsRawResult 
AresDns::makeRawResult(void *arg, int status, unsigned char *abuf, int alen)
{
   Payload* p = reinterpret_cast<Payload*>(arg);
   void* userArg = reinterpret_cast<void*>(p->second);
   
   if (status != ARES_SUCCESS)
   {
      return ExternalDnsRawResult(status, userArg);
   }
   else
   {
      return ExternalDnsRawResult(abuf, alen, userArg);
   }
}

void
AresDns::aresHostCallback(void *arg, int status, struct hostent* result)
{
   Payload* p = reinterpret_cast<Payload*>(arg);
   ExternalDnsHandler *thisp = reinterpret_cast<ExternalDnsHandler*>(p->first);
   void* userArg = reinterpret_cast<void*>(p->second);

   if (status != ARES_SUCCESS)
   {
      thisp->handle_host(ExternalDnsHostResult(status, userArg));
   }
   else
   {
      thisp->handle_host(ExternalDnsHostResult(result, userArg));
   }
   delete p;   
}

void
AresDns::aresNAPTRCallback(void *arg, int status, unsigned char *abuf, int alen)
{
   getHandler(arg)->handle_NAPTR(makeRawResult(arg, status, abuf, alen));
   Payload* p = reinterpret_cast<Payload*>(arg);
   delete p;
}

void
AresDns::aresSRVCallback(void *arg, int status, unsigned char *abuf, int alen)
{
   getHandler(arg)->handle_SRV(makeRawResult(arg, status, abuf, alen));
   Payload* p = reinterpret_cast<Payload*>(arg);
   delete p;
}

void
AresDns::aresAAAACallback(void *arg, int status, unsigned char *abuf, int alen)
{
   getHandler(arg)->handle_AAAA(makeRawResult(arg, status, abuf, alen));
   Payload* p = reinterpret_cast<Payload*>(arg);
   delete p;
}                             
      
bool 
AresDns::requiresProcess()
{
   return true; 
}

void 
AresDns::buildFdSet(fd_set& read, fd_set& write, int& size)
{
   int newsize = ares_fds(mChannel, &read, &write);
   if ( newsize > size )
   {
      size = newsize;
   }
}

void 
AresDns::process(fd_set& read, fd_set& write)
{
   ares_process(mChannel, &read, &write);
}

char* 
AresDns::errorMessage(long errorCode)
{
   const char* aresMsg = ares_strerror(errorCode);

   int len = strlen(aresMsg);
   char* errorString = new char[len+1];

   strncpy(errorString, aresMsg, len);
   errorString[len] = '\0';
   return errorString;
}

/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000-2005 Vovida Networks, Inc.  All rights reserved.
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
