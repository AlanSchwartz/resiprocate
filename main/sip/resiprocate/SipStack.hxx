#if !defined(SIPSTACK_HXX)
#define SIPSTACK_HXX

#include <set>

#include "sip2/util/Fifo.hxx"
#include "sip2/util/Socket.hxx"
#include "sip2/util/DataStream.hxx"

#include "sip2/sipstack/Executive.hxx"
#include "sip2/sipstack/TransportSelector.hxx"
#include "sip2/sipstack/TransactionMap.hxx"
#include "sip2/sipstack/TimerQueue.hxx"
#include "sip2/sipstack/Dialog.hxx"
#include "sip2/sipstack/DnsResolver.hxx"
#include "sip2/sipstack/SdpContents.hxx"
#include "sip2/sipstack/SipMessage.hxx"
#include "sip2/sipstack/SipFrag.hxx"
#include "sip2/sipstack/Helper.hxx"

namespace Vocal2
{

class Data;
class Message;
class SipMessage;
class DnsResolver;
class Executive;
class TransportSelector;
class TransactionState;
class TestDnsResolver;
class TestFSM;
class Security;
class Uri;

	
class SipStack
{
   public:
      SipStack(bool multiThreaded=false);

      // Used by the application to add in a new transport
      // by default, you will get UDP and TCP on 5060 (for now)
      // hostname parameter is used to specify the host portion of the uri that
      // describes this sip element (proxy or ua)
      // nic is used to specify an ethernet interface by name. e.g. eth0
      void addTransport( Transport::Type, int port, const Data& hostName="", const Data& nic="");

      // used to add an alias for this sip element. e.g. foobar.com and boo.com
      // are both handled by this proxy. 
      void addAlias(const Data& domain);
      
      // return true if domain is handled by this stack. convenience for
      // Transaction Users. 
      bool isMyDomain(const Data& domain) const;
      
      // get one of the names for this host (calls through to gethostbyname) and
      // is not threadsafe
      static Data getHostname();

      /// interface for the TU to send a message. makes a copy of the
      /// SipMessage. Caller is responsible for deleting the memory and may do
      /// so as soon as it returns. Loose Routing processing as per RFC3261 must
      /// be done before calling send by the TU. See Helper::processStrictRoute
      void send(const SipMessage& msg);

      // this is only if you want to send to a destination not in the route. You
      // probably don't want to use it. 
      void sendTo(const SipMessage& msg, const Uri& uri);

      // caller now owns the memory. returns 0 if nothing there
      SipMessage* receive(); 
      
      // build the FD set to use in a select to find out when process bust be
      // called again. This must be called prior to calling process. 
      void buildFdSet(FdSet& fdset);
	
      // should call buildFdSet before calling process. This allows the
      // executive to give processing time to stack components. 
      void process(FdSet& fdset);

      /// returns time in milliseconds when process next needs to be called 
      int getTimeTillNextProcessMS(); 

#ifdef USE_SSL
      /// if this object exists, it manages advanced security featues
      Security* security;
#endif

private:
      // fifo used to communicate between the TU (Transaction User) and stack 
      Fifo<Message> mTUFifo;

      // fifo used to communicate to the transaction state machine within the
      // stack. Not for external use by the application. May contain, sip
      // messages (requests and responses), timers (used by state machines),
      // asynchronous dns responses, transport errors from the underlying
      // transports, etc. 
      Fifo<Message> mStateMacFifo;

      // Controls the processing of the various stack elements
      Executive mExecutive;

      // Used to decide which transport to send a sip message on. 
      TransportSelector mTransportSelector;

      // stores all of the transactions that are currently active in this stack 
      TransactionMap mTransactionMap;

      // timers associated with the transactions. When a timer fires, it is
      // placed in the mStateMacFifo
      TimerQueue  mTimers;

      // Used to make dns queries by the
      // TransactionState/TransportSelector. Provides an async mechanism for
      // communicating dns responses back to the Transaction. 
      DnsResolver mDnsResolver;
      
      // If true, indicate to the Transaction to ignore responses for which
      // there is no transaction. 
      // !jf! Probably should transmit stray responses statelessly. see RFC3261
      bool mDiscardStrayResponses;

      // store all domains that this stack is responsible for. Controlled by
      // addAlias and addTransport interfaces and checks can be made with isMyDomain()
      std::set<Data> mDomains;

      friend class DnsResolver;
      friend class Executive;
      friend class TransportSelector;
      friend class TransactionState;
      friend class TestDnsResolver;
      friend class TestFSM;
};
 
}

#endif


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
