#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#ifndef WIN32
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "resiprocate/os/Data.hxx"
#include "resiprocate/os/Fifo.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/Random.hxx"
#include "resiprocate/os/Socket.hxx"
#include "resiprocate/os/Timer.hxx"

#include "resiprocate/Executive.hxx"
#include "resiprocate/Message.hxx"
#include "resiprocate/Security.hxx"
#include "resiprocate/ShutdownMessage.hxx"
#include "resiprocate/SipMessage.hxx"
#include "resiprocate/SipStack.hxx"



#ifdef WIN32
#pragma warning( disable : 4355 )
#endif

using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::SIP

SipStack::SipStack(bool multiThreaded, Security* pSecurity, bool stateless) : 
   security( pSecurity ),
   mExecutive(*this),
   mTransactionController(multiThreaded, mTUFifo, stateless),
   mStrictRouting(false),
   mShuttingDown(false)
{
   Timer::getTimeMs(); // initalize time offsets
   Random::initialize();
   initNetwork();

#ifdef USE_SSL
   if ( !pSecurity )
   {
      security = new Security( true, true );
   }
#endif
   assert(!mShuttingDown);
}

SipStack::~SipStack()
{
#ifdef USE_SSL
   delete security;
#endif
}

void
SipStack::shutdown()
{
   //InfoLog (<< "Shutting down stack " << this);
   mShuttingDown = true;
   mTUFifo.add(new ShutdownMessage);
}

void 
SipStack::addTransport( TransportType protocol, 
                        int port,
                        IpVersion version,
                        const Data& ipInterface)
{
   assert(!mShuttingDown);
   assert(protocol != TLS);

   mTransactionController.addTransport(protocol, port, version, ipInterface);
   if (!ipInterface.empty()) 
   {
      addAlias(ipInterface, port);
   }
}

void 
SipStack::addTlsTransport( int port, 
                           const Data& keyDir,
                           const Data& privateKeyPassPhrase,
                           const Data& domainname,
                           IpVersion version,
                           const Data& ipInterface)
{
   assert(!mShuttingDown);
   
   try
   {
      mTransactionController.addTlsTransport(port,keyDir,privateKeyPassPhrase, domainname, version, ipInterface);
      if (!ipInterface.empty()) 
      {
         addAlias(ipInterface, port);
      }
   }
   catch(BaseException& e)
   {
      InfoLog (<< "Caught: " << e);
   }
}

void
SipStack::addAlias(const Data& domain, int port)
{
   DebugLog (<< "Adding domain alias: " << domain << ":" << port);
   assert(!mShuttingDown);
   mDomains.insert(domain + Data(":") + Data(port));
}

Data 
SipStack::getHostname()
{
   // if you change this, please #def old version for windows 
   char hostName[1024];
   int err =  gethostname( hostName, sizeof(hostName) );
   assert( err == 0 );
   
   struct hostent* hostEnt = gethostbyname( hostName );
   if ( !hostEnt )
   {
      // this can fail when there is no name server 
      // !cj! - need to decided what error to return 
      ErrLog( << "gethostbyname failed - name server is probably down" );
   }
   assert( hostEnt );
   
   struct in_addr* addr = (struct in_addr*) hostEnt->h_addr_list[0];
   assert( addr );
   
   // if you change this, please #def old version for windows 
   char* addrA = inet_ntoa( *addr );
   Data ret(addrA);

   Data retHost( hostEnt->h_name );
      
   return retHost;
}


Data 
SipStack::getHostAddress()
{
   // if you change this, please #def old version for windows 
   char hostName[1024];
   int err =  gethostname( hostName, sizeof(hostName) );
   assert( err == 0 );
   
   struct hostent* hostEnt = gethostbyname( hostName );
   assert( hostEnt );
   
   struct in_addr* addr = (struct in_addr*) hostEnt->h_addr_list[0];
   assert( addr );
   
   // if you change this, please #def old version for windows 
   char* addrA = inet_ntoa( *addr );
   Data ret(addrA);

   //Data retHost( hostEnt->h_name );
      
   return ret;
}


bool 
SipStack::isMyDomain(const Data& domain, int port) const
{
   return (mDomains.count(domain + Data(":") + 
                          Data(port == 0 ? Symbols::DefaultSipPort : port)) != 0);
}


void 
SipStack::send(const SipMessage& msg)
{
   DebugLog (<< "SEND: " << msg.brief());
   //DebugLog (<< msg);
   assert(!mShuttingDown);
   
   SipMessage* toSend = new SipMessage(msg);
   toSend->setFromTU();

   mTransactionController.send(toSend);
}


// this is only if you want to send to a destination not in the route. You
// probably don't want to use it. 
void 
SipStack::sendTo(const SipMessage& msg, const Uri& uri)
{
   assert(!mShuttingDown);

   SipMessage* toSend = new SipMessage(msg);
   toSend->setTarget(uri);
   toSend->setFromTU();

   mTransactionController.send(toSend);
}

// this is only if you want to send to a destination not in the route. You
// probably don't want to use it. 
void 
SipStack::sendTo(const SipMessage& msg, const Tuple& destination)
{
   assert(!mShuttingDown);
   assert(destination.transport);
   
   SipMessage* toSend = new SipMessage(msg);
   toSend->setDestination(destination);
   toSend->setFromTU();

   mTransactionController.send(toSend);
}



SipMessage* 
SipStack::receive()
{
   // Check to see if a message is available and if it is return the 
   // waiting message. Otherwise, return 0
   if (mTUFifo.messageAvailable())
   {
      // we should only ever have SIP messages on the TU Fifo
      // unless we've registered for termination messages. 
      Message* msg = mTUFifo.getNext();
      SipMessage* sip=0;
      if ((sip=dynamic_cast<SipMessage*>(msg)))
      {
         DebugLog (<< "RECV: " << sip->brief());
         return sip;
      }
      else
      {
         assert(0);
         delete msg;
         return 0;
      }
   }
   else
   {
      return 0;
   }
}

Message*
SipStack::receiveAny()
{
   // Check to see if a message is available and if it is return the 
   // waiting message. Otherwise, return 0
   if (mTUFifo.messageAvailable())
   {
      // we should only ever have SIP messages on the TU Fifo
      // unless we've registered for termination messages. 
      Message* msg = mTUFifo.getNext();
      SipMessage* sip=dynamic_cast<SipMessage*>(msg);
      if (sip)
      {
         DebugLog (<< "RECV: " << sip->brief());
      }
      return msg;
   }
   else
   {
      return 0;
   }
}


void 
SipStack::process(FdSet& fdset)
{
   mExecutive.process(fdset);
}


/// returns time in milliseconds when process next needs to be called 
int 
SipStack::getTimeTillNextProcessMS()
{
   return mExecutive.getTimeTillNextProcessMS();
} 

void
SipStack::registerForTransactionTermination()
{
   mTransactionController.registerForTransactionTermination();
}

void 
SipStack::buildFdSet(FdSet& fdset)
{
   mExecutive.buildFdSet( fdset );
}


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
