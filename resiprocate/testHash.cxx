#include <iostream>
#ifndef WIN32
#include <unistd.h>
#endif

#include <limits.h>
#include <signal.h>
#include <sys/fcntl.h>
#include <util/Logger.hxx>
#include <sipstack/HeaderTypes.hxx>

using namespace Vocal2;
using namespace std;

#define VOCAL_SUBSYSTEM Vocal2::Subsystem::SIP

volatile bool signalled = false;

void tick(int sig)
{
   signalled = true;
}


static 
struct {
      char * keyword;
      Headers::Type type;
      int len;
} headerInfo[(int)(Headers::MAX_HEADERS-Headers::CSeq+1)];

unsigned int  InitHeaderInfo()
{
   int i = 0;
   
   for(Headers::Type t = Headers::CSeq;
       t < Headers::UNKNOWN;
       ++t,++i)
   {
      char* p = strdup(Headers::HeaderNames[t].c_str());
      
      headerInfo[i].len = Headers::HeaderNames[t].length();
      headerInfo[i].keyword = p;
      headerInfo[i].type = t;
      
      DebugLog(<< headerInfo[i].keyword << " ["
               << headerInfo[i].type <<"]");
   }

   return i;
   
}


unsigned short randomUShort()
{
   static int fd = 0;
   static bool init = false;
   if (!init)
   {
      fd = open("/dev/urandom",O_RDONLY);
      if (fd < 0)
      {
 cerr << "randomShort(): unable to open /dev/urandom -- degraded mode"
      << endl;
      }
      init = true;
   }
   if (init && fd >= 0)
   {
      unsigned short r;
      int n = 0;
      if ((n=read(fd,&r,sizeof(r))) == sizeof(r))
      {
 return r;
      }
   }
   // degraded mode
   return (unsigned short)((USHRT_MAX + 1.0) * rand() / (RAND_MAX+1.0));
}


int
main()
{
   int randomList[100*1024];


   unsigned int nRandom = sizeof(randomList)/sizeof(*randomList);
   
   register unsigned int i = 0;

   // Load up the main table
   unsigned short nKeywords =    InitHeaderInfo();
   

   // Verify that the hash function works.
   InfoLog(<<"Checking that hash function works for all known headers");
   
   for(i=0;i<nKeywords;i++)
   {
#if defined(GPERF)
      siphdrhash_s* hash = 
         Perfect_Hash::in_word_set(headerInfo[i].keyword,
                                   headerInfo[i].len);
      bool ok = headerInfo[i].type == hash->type;
#else
      Headers::Type t = Headers::getHeaderType(headerInfo[i].keyword,
                                               headerInfo[i].len);
      bool ok = headerInfo[i].type == t;
      
#endif
      InfoLog(<< headerInfo[i].keyword << " " << (ok?"OK":"FAIL"));
      if (!ok)
      {
         ErrLog(<<headerInfo[i].keyword << "["
                <<headerInfo[i].type << "] hashed to "
                << t << " ["
                << Headers::HeaderNames[t] << "]");
      }
      
   }

   
   // Make a large random list so we don't take a hit with 
   // random() calcs during the hash.

   InfoLog(<< "Pre-loading random list of " << nRandom << " entries");
   
   for(i = 0; i < nRandom ;  i++)
   {
      short r = randomUShort()%nKeywords;
      randomList[i] = r;
   }
   
   i=0;
   InfoLog(<<"Starting timing loop");

   register int totalTime=30;
   register int interval=5;
   register int timer=totalTime;
   register int elapsed=0;
   
   signal(SIGALRM, tick);
   register unsigned long long counter = 0;

   assert(totalTime/interval*interval == totalTime);
   
   while (timer>0)
   {
      alarm(interval);
      while (!signalled)
      {
#if defined(GPERF_HASH)
         volatile register siphdrhash_s* hash = 
            Perfect_Hash::in_word_set(headerInfo[randomList[i]].keyword,
                                      headerInfo[randomList[i]].len);
#else
         volatile register Headers::Type hdr = 
            Headers::getHeaderType(headerInfo[randomList[i]].keyword,
                                   headerInfo[randomList[i]].len);
#endif         
         counter++;
         i++;
         if (i >= nRandom) i = 0;
      }
      timer-=interval;
      elapsed+=interval;
      InfoLog(<< timer <<" sec rem: " 
              << counter << " hashes "
              << counter/elapsed << " h/sec");
      signalled=false;
      
   }
}
/*

         volatile register Headers::Type hdr = 
            Headers::getHeaderType(randomList[i].word,
                                   randomList[i].len);

*/


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
