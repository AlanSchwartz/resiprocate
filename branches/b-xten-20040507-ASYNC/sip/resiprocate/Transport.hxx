#if !defined(RESIP_TRANSPORT_HXX)
#define RESIP_TRANSPORT_HXX

//#include <exception>

#include "resiprocate/os/Fifo.hxx"
#include "resiprocate/os/BaseException.hxx"
#include "resiprocate/os/Data.hxx"
#include "resiprocate/os/Tuple.hxx"

namespace resip
{

class TransactionMessage;
class SipMessage;
class SendData;
class Connection;

class Transport
{
   public:
      class Exception : public BaseException
      {
         public:
            Exception(const Data& msg, const Data& file, const int line);
            const char* name() const { return "TransportException"; }
      };
      
      // sendHost what to put in the Via:sent-by
      // portNum is the port to receive and/or send on
      Transport(Fifo<TransactionMessage>& rxFifo, const GenericIPAddress& address);
      Transport(Fifo<TransactionMessage>& rxFifo, int portNum, const Data& interfaceObj, bool ipv4);
      virtual ~Transport();

      virtual bool isFinished() const=0;
      
      virtual void send( const Tuple& tuple, const Data& data, const Data& tid);

      //only call buildFdSet and process if requiresProcess is true. This won't
      //be true for most external transports, or transports that have their own
      //threading model. 
      virtual void process(FdSet& fdset) = 0;
      virtual void buildFdSet( FdSet& fdset) =0;

      virtual bool hasDataToSend() const = 0;
      
      
//  virtual int maxFileDescriptors() const = 0; !dcm! -- not used anywhere


      virtual TransportType transport() const = 0 ;
      virtual bool isReliable() const = 0;

      // void shutdown();
      
      void fail(const Data& tid); // called when transport failed
      
      static void error(int e);
      
      // These methods are used by the TransportSelector
      const Data& interfaceName() const { return mInterface; } 
      int port() const { return mTuple.getPort(); } 
      bool isV4() const { return mTuple.isV4(); }
      
      const Data& tlsDomain() const { return Data::Empty; }
      const sockaddr& boundInterface() const { return mTuple.getSockaddr(); }
      const Tuple& getTuple() const { return mTuple; }

      // Perform basic sanity checks on message. Return false
      // if there is a problem eg) no Vias. --SIDE EFFECT--
      // This will queue a response if it CAN for a via-less 
      // request. Response will go straight into the TxFifo
      bool basicCheck(const SipMessage& msg);

      void makeFailedResponse(const SipMessage& msg,
                              int responseCode = 400,
                              const char * warning = 0);

      // mark the received= and rport parameters if necessary
      static void stampReceived(SipMessage* request);


      // also used by the TransportSelector. 
      // requires that the two transports be 
      bool operator==(const Transport& rhs) const;

   protected:
      Data mInterface;
      Tuple mTuple;

      Fifo<TransactionMessage>& mStateMachineFifo; // passed in
      bool mShuttingDown;

      //not a great name, just adds the message to the fifo in the synchronous(default) case,
      //actually transmits in the asyncronous case.  Don't make a SendData because asynchronous
      //transports would require another copy.
      virtual void transmit(const Tuple& dest, const Data& pdata, const Data& tid) = 0;
      
   private:
      static const Data transportNames[MAX_TRANSPORT];
      friend std::ostream& operator<<(std::ostream& strm, const Transport& rhs);
};

//!dcm! -- move into own header
class SendData
{
   public:
      SendData(const Tuple& dest, const Data& pdata, const Data& tid): 
         destination(dest),
         data(pdata),
         transactionId(tid) 
      {}
      Tuple destination;
      const Data data;
      const Data transactionId;
};

std::ostream& operator<<(std::ostream& strm, const Transport& rhs);
      
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
