#if !defined(RESIP_TRANSPORTSELECTOR_HXX)
#define RESIP_TRANSPORTSELECTOR_HXX

#ifndef WIN32
#include <sys/select.h>
#endif

#include <map>

#include "resiprocate/os/Data.hxx"
#include "resiprocate/os/Fifo.hxx"
#include "resiprocate/DnsInterface.hxx"
#include "resiprocate/SipMessage.hxx"
#include "resiprocate/Transport.hxx"

class TestTransportSelector;

namespace resip
{

class DnsHandler;
class Message;
class SipMessage;
class TlsTransport;
class TransactionController;

class TransportSelector 
{
   public:
      TransportSelector(bool multithreaded, Fifo<Message>& fifo);
      virtual ~TransportSelector();
      bool hasDataToSend() const;
      
      void process(FdSet& fdset);
      void buildFdSet(FdSet& fdset);
     
      void addTransport( TransportType,
                         int port,
                         IpVersion version,
                         const Data& ipInterface=Data::Empty);
      void addTlsTransport(const Data& domainName, 
                           const Data& keyDir,
                           const Data& privateKeyPassPhrase,
                           int port, 
                           IpVersion version,
                           const Data& ipInterface=Data::Empty);

      DnsResult* dnsResolve(SipMessage* msg, DnsHandler* handler);

      // this will result in msg->resolve() being called to either
      // kick off dns resolution or to pick the next tuple , will cause the
      // message to be encoded and via updated
      void transmit( SipMessage* msg, Tuple& destination );
      
      // just resend to the same transport as last time
      void retransmit(SipMessage* msg, Tuple& destination );
      
   private:
      Transport* findTransport(const Tuple& src);
      Transport* findTlsTransport(const Data& domain);
      Tuple determineSourceInterface(SipMessage* msg, const Tuple& dest) const;

      bool mMultiThreaded;
      DnsInterface mDns;
      Fifo<Message>& mStateMacFifo;

      // specific port and interface
      typedef HashMap<Tuple, Transport*> ExactTupleMap;
      ExactTupleMap mExactTransports;

      // specific port, ANY interface
      typedef map<Tuple, Transport*, Tuple::AnyInterfaceCompare> AnyInterfaceTupleMap;
      AnyInterfaceTupleMap mAnyInterfaceTransports;

      // ANY port, specific interface
      typedef map<Tuple, Transport*, Tuple::AnyPortCompare> AnyPortTupleMap;
      AnyPortTupleMap mAnyPortTransports;

      // ANY port, ANY interface
      typedef map<Tuple, Transport*, Tuple::AnyPortAnyInterfaceCompare> AnyPortAnyInterfaceTupleMap;
      AnyPortAnyInterfaceTupleMap mAnyPortAnyInterfaceTransports;

      HashMap<Data, TlsTransport*> mTlsTransports;      // domain name -> Transport
      
      // fake socket for connect() and route table lookups
      mutable Socket mSocket;
      mutable Socket mSocket6;
      
      // An AF_UNSPEC addr_in for rapid unconnect
      struct sockaddr_in mUnspecified;
#ifdef USE_IPV6
	  struct sockaddr_in6 mUnspecified6;
#endif

      friend class TestTransportSelector;
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
