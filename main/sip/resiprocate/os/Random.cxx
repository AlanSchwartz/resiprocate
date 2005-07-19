#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include <cassert>

#ifdef WIN32
#include "resiprocate/os/Socket.hxx"
#include "resiprocate/os/DataStream.hxx"
#include "resiprocate/os/Data.hxx"
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "resiprocate/os/Random.hxx"
#include "resiprocate/os/Timer.hxx"
#include "resiprocate/os/Mutex.hxx"
#include "resiprocate/os/Lock.hxx"
#include "resiprocate/os/Logger.hxx"

#ifdef USE_SSL
#  define USE_OPENSSL 1
#else
#  define USE_OPENSSL 0
#endif

#if ( USE_OPENSSL == 1 )
#  include <openssl/e_os2.h>
#  include <openssl/rand.h>
#  include <openssl/err.h>
#endif

using namespace resip;
#define RESIPROCATE_SUBSYSTEM Subsystem::SIP

#ifdef WIN32
Random::Initializer Random::mInitializer;
#endif

Mutex Random::mMutex;
bool Random::mIsInitialized = false;

void
Random::initialize()
{  
#ifdef WIN32
   if (!Random::mInitializer.isInitialized())
   {
      Lock lock(mMutex);      
      if (!Random::mInitializer.isInitialized())
      {
         Random::mInitializer.setInitialized();
         // !cj! need to find a better way - use pentium random commands?
         Data buffer;
         DataStream strm(buffer);
         strm << GetTickCount() << ":";
         strm << GetCurrentProcessId() << ":";
         strm << GetCurrentThreadId();
         strm.flush();
         unsigned int seed = buffer.hash();

         //InfoLog( << "srand() called with seed=" << seed << " for thread " << GetCurrentThreadId());
         srand(seed);
      }
   }
#endif
   
   if ( !Random::mIsInitialized)
   {
      Lock lock(mMutex);
      if (!Random::mIsInitialized)
      {
         mIsInitialized = true;
         Timer::setupTimeOffsets();

#ifndef WIN32      
         //throwing away first 32 bits
         unsigned int seed = static_cast<unsigned int>(Timer::getTimeMs());
         srandom(seed);

         int fd = open("/dev/urandom", O_RDONLY);
         // !ah! blocks on embedded devices -- not enough entropy.
         if ( fd != -1 )
         {
            int s = read( fd,&seed,sizeof(seed) ); //!ah! blocks if /dev/random on embedded sys

            if ( s != sizeof(seed) )
            {
               ErrLog( << "System is short of randomness" ); // !ah! never prints
            }
         }
         else
         {
            ErrLog( << "Could not open /dev/urandom" );
         }

#endif

#if USE_OPENSSL
         if (fd == -1 )
         {
            // really bad sign - /dev/random does not exist so need to intialize
            // OpenSSL some other way

            // !cj! need to fix         assert(0);
         }
         else
         {
            char buf[1024/8]; // size is number byes used for OpenSSL init 

            int s = read( fd,&buf,sizeof(buf) );

            if ( s != sizeof(buf) )
            {
               ErrLog( << "System is short of randomness" );
            }
         
            RAND_add(buf,sizeof(buf),double(s*8));
         }
#endif
#ifndef WIN32 
         if (fd != -1 )
         {
            ::close(fd);
         }
#endif
      }
   }
}

int
Random::getRandom()
{
   initialize();

#ifdef WIN32
   assert( RAND_MAX == 0x7fff );
   int r1 = rand();
   int r2 = rand();

   int ret = (r1<<16) + r2;

   return ret;
#else
   return random(); 
#endif
}

int
Random::getCryptoRandom()
{
   initialize();

#if USE_OPENSSL
   int ret;
   int e = RAND_bytes( (unsigned char*)&ret , sizeof(ret) );
   if ( e < 0 )
   {
      // error of some type - likely not enough rendomness to dod this 
      long err = ERR_get_error();
      
      char buf[1024];
      ERR_error_string_n(err,buf,sizeof(buf));
      
      ErrLog( << buf );
      assert(0);
   }
   return ret;
#else
   return getRandom();
#endif
}

Data 
Random::getRandom(unsigned int len)
{
   initialize();
   assert(len < Random::maxLength+1);
   
   union 
   {
         char cbuf[Random::maxLength+1];
         unsigned int  ibuf[(Random::maxLength+1)/sizeof(int)];
   };
   
   for (unsigned int count=0; count<(len+sizeof(int)-1)/sizeof(int); ++count)
   {
      ibuf[count] = Random::getRandom();
   }
   return Data(cbuf, len);
}

Data 
Random::getCryptoRandom(unsigned int len)
{
   initialize();
   assert(len < Random::maxLength+1);
   
   union 
   {
         char cbuf[Random::maxLength+1];
         unsigned int  ibuf[(Random::maxLength+1)/sizeof(int)];
   };
   
   for (unsigned int count=0; count<(len+sizeof(int)-1)/sizeof(int); ++count)
   {
      ibuf[count] = Random::getCryptoRandom();
   }
   return Data(cbuf, len);
}

Data 
Random::getRandomHex(unsigned int numBytes)
{
   initialize();
   return Random::getRandom(numBytes).hex();
}

Data 
Random::getRandomBase64(unsigned int numBytes)
{
   initialize();
   return Random::getRandom(numBytes).base64encode();
}

Data 
Random::getCryptoRandomHex(unsigned int numBytes)
{
   initialize();
   return Random::getCryptoRandom(numBytes).hex();
}

Data 
Random::getCryptoRandomBase64(unsigned int numBytes)
{
   initialize();
   return Random::getCryptoRandom(numBytes).base64encode();
}

#ifdef WIN32
Random::Initializer::Initializer()  : mThreadStorage(::TlsAlloc())
{ 
   assert(mThreadStorage != TLS_OUT_OF_INDEXES);
}

Random::Initializer::~Initializer() 
{ 
   ::TlsFree(mThreadStorage); 
}

void 
Random::Initializer::setInitialized() 
{ 
   ::TlsSetValue(mThreadStorage, (LPVOID) TRUE);
}

bool 
Random::Initializer::isInitialized() 
{ 
   // Note:  if value is not set yet then 0 (false) is returned
   return (BOOL) ::TlsGetValue(mThreadStorage) == TRUE; 
}
#endif


/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2005.   All rights reserved.
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

