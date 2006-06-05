#if defined(HAVE_CONFIG_H)
#include "resip/stack/config.hxx"
#endif

#include <memory>

#include "resip/stack/Helper.hxx"
#include "resip/stack/SendData.hxx"
#include "resip/stack/SipMessage.hxx"
#include "resip/stack/StunMessage.hxx"
#include "resip/stack/UdpTransport.hxx"
#include "rutil/Data.hxx"
#include "rutil/DnsUtil.hxx"
#include "rutil/Logger.hxx"
#include "rutil/Socket.hxx"
#include "rutil/WinLeakCheck.hxx"
#include "rutil/compat.hxx"
#include "rutil/stun/Stun.hxx"

#define RESIPROCATE_SUBSYSTEM Subsystem::TRANSPORT

using namespace std;
using namespace resip;

UdpTransport::UdpTransport(Fifo<TransactionMessage>& fifo,
                           int portNum,  
                           IpVersion version,
                           StunSetting stun,
                           const Data& pinterface,
                           AfterSocketCreationFuncPtr socketFunc) 
   : InternalTransport(fifo, portNum, version, pinterface, socketFunc)
{
   InfoLog (<< "Creating UDP transport host=" << pinterface 
            << " port=" << portNum
            << " ipv4=" << bool(version==V4) );

   mTuple.setType(transport());
   mFd = InternalTransport::socket(transport(), version);
   bind();
}

UdpTransport::~UdpTransport()
{
   DebugLog (<< "Shutting down " << mTuple);
}

void 
UdpTransport::process(FdSet& fdset)
{
   // pull buffers to send out of TxFifo
   // receive datagrams from fd
   // preparse and stuff into RxFifo

   if (mTxFifo.messageAvailable() && fdset.readyToWrite(mFd))
   {
      std::auto_ptr<SendData> sendData = std::auto_ptr<SendData>(mTxFifo.getNext());
      //DebugLog (<< "Sent: " <<  sendData->data);
      //DebugLog (<< "Sending message on udp.");

      assert( &(*sendData) );
      assert( sendData->destination.getPort() != 0 );
      
      const sockaddr& addr = sendData->destination.getSockaddr();
      int count = sendto(mFd, 
                         sendData->data.data(), sendData->data.size(),  
                         0, // flags
                         &addr, sendData->destination.length());
      
      if ( count == SOCKET_ERROR )
      {
         int e = getErrno();
         error(e);
         InfoLog (<< "Failed (" << e << ") sending to " << sendData->destination);
         fail(sendData->transactionId);
      }
      else
      {
         if (count != int(sendData->data.size()) )
         {
            ErrLog (<< "UDPTransport - send buffer full" );
            fail(sendData->transactionId);
         }
      }
   }
   
   // !jf! this may have to change - when we read a message that is too big
   if ( fdset.readyToRead(mFd) )
   {
      //should this buffer be allocated on the stack and then copied out, as it
      //needs to be deleted every time EWOULDBLOCK is encountered
      // .dlb. can we determine the size of the buffer before we allocate?
      // something about MSG_PEEK|MSG_TRUNC in Stevens..
      // .dlb. RFC3261 18.1.1 MUST accept 65K datagrams. would have to attempt to
      // adjust the UDP buffer as well...
      char* buffer = MsgHeaderScanner::allocateBuffer(MaxBufferSize);      

      // !jf! how do we tell if it discarded bytes 
      // !ah! we use the len-1 trick :-(
      Tuple tuple(mTuple);
      socklen_t slen = tuple.length();
      int len = recvfrom( mFd,
                          buffer,
                          MaxBufferSize,
                          0 /*flags */,
                          &tuple.getMutableSockaddr(), 
                          &slen);
      if ( len == SOCKET_ERROR )
      {
         int err = getErrno();
         if ( err != EWOULDBLOCK  )
         {
            error( err );
         }
      }

      if (len == 0 || len == SOCKET_ERROR)
      {
         delete[] buffer; 
         buffer=0;
         return;
      }

      if (len+1 >= MaxBufferSize)
      {
         InfoLog(<<"Datagram exceeded max length "<<MaxBufferSize);
         delete [] buffer; buffer=0;
         return;
      }

      //handle incoming CRLFCRLF keep-alive packets
      if ( len == 4 && strncmp(buffer, Symbols::CRLFCRLF, 4) == 0)
      {
         delete[] buffer;
         buffer = 0;
         StackLog(<<"Throwing away incoming firewall keep-alive");
         return;
      }

#if 0 
      // this must be a STUN response (or garbage)
      if (buffer[0] == 1 && buffer[1] == 1 && ipVersion() == V4)
      {
         resip::Lock lock(myMutex);
	     StunMessage resp;
	     memset(&resp, 0, sizeof(StunMessage));
		
	     if (stunParseMessage(buffer, len, resp, false))
		 {
			 in_addr sin_addr;
#if defined(WIN32)
			 sin_addr.S_un.S_addr = htonl(resp.mappedAddress.ipv4.addr);
#else
			 sin_addr.s_addr = htonl(resp.mappedAddress.ipv4.addr);
#endif
			 mStunMappedAddress = Tuple(sin_addr,resp.mappedAddress.ipv4.port, UDP);
			 mStunSuccess = true;
		 }
		 return;
	  }

      // this must be a STUN request (or garbage)
      if (buffer[0] == 0 && buffer[1] == 1 && ipVersion() == V4)
      {
         bool changePort = false;
         bool changeIp = false;
         
         StunAddress4 myAddr;
         const sockaddr_in& bi = (const sockaddr_in&)boundInterface();
         myAddr.addr = ntohl(bi.sin_addr.s_addr);
         myAddr.port = ntohs(bi.sin_port);
         
         StunAddress4 from; // packet source
         const sockaddr_in& fi = (const sockaddr_in&)tuple.getSockaddr();
         from.addr = ntohl(fi.sin_addr.s_addr);
         from.port = ntohs(fi.sin_port);
         
         StunMessage resp;
         StunAddress4 dest;
         StunAtrString hmacPassword;  
         hmacPassword.sizeValue = 0;
         
         StunAddress4 secondary;
         secondary.port = 0;
         secondary.addr = 0;
         
         bool ok = stunServerProcessMsg( buffer, len, // input buffer
                                         from,  // packet source
                                         secondary, // not used
                                         myAddr, // address to fill into response
                                         myAddr, // not used
                                         &resp, // stun response
                                         &dest, // where to send response
                                         &hmacPassword, // not used
                                         &changePort, // not used
                                         &changeIp, // not used
                                         false ); // logging
         
         if (ok)
         {
            char* response = new char[STUN_MAX_MESSAGE_SIZE];
            int rlen = stunEncodeMessage( resp, 
                                          response, 
                                          STUN_MAX_MESSAGE_SIZE, 
                                          hmacPassword,
                                          false );
            SendData* stunResponse = new SendData(tuple, response, rlen);
            mTxFifo.add(stunResponse);
         }
         delete[] buffer;
         buffer = 0;
         return;
      }
#endif


      buffer[len]=0; // null terminate the buffer string just to make debug easier and reduce errors

      //DebugLog ( << "UDP Rcv : " << len << " b" );
      //DebugLog ( << Data(buffer, len).escaped().c_str());

      TransactionMessage* message;
      if ( ( buffer[0] == 0 || buffer[0] == 1 )  
           && ( buffer[1] >= 1 && buffer[1] <= 0x12 ) )
      {
         message = new StunMessage(this);
         // Tell the StunMessage about this datagram buffer.
         message->addBuffer(buffer, len);
      }
      else
      {
         message = new SipMessage(this);
         // Tell the SipMessage about this datagram buffer.
         message->addBuffer(buffer);
      }
 
      // It is presumed that UDP Datagrams are arriving atomically and that
      // each one is a unique message

      // Save all the info where this message came from
      tuple.transport = this;
      message->setSource(tuple);   
      //DebugLog (<< "Received from: " << tuple);
   

      if ( dynamic_cast<StunMessage*>( message ) )
      {
         StunMessage* msg = dynamic_cast<StunMessage*>(message);
         assert(msg);
         
         if (!basicCheck(*msg))
         {
            delete msg; // cannot use it, so, punt on it...
            msg = 0;
            message =0;
            return;
         }
      }
      else
      {
         SipMessage* msg = dynamic_cast<SipMessage*>(message);
         assert(msg);
         
         mMsgHeaderScanner.prepareForMessage(msg);

         char *unprocessedCharPtr;
         if (mMsgHeaderScanner.scanChunk(buffer,
                                         len,
                                         &unprocessedCharPtr) !=
             MsgHeaderScanner::scrEnd)
         {
            StackLog(<<"Scanner rejecting datagram as unparsable / fragmented from " << tuple);
            StackLog(<< Data(buffer, len));
            delete msg; 
            msg=0; message=0;
            return;
         }
         
         // no pp error
         int used = unprocessedCharPtr - buffer;
         
         if (used < len)
         {
            // body is present .. add it up.
            // NB. The Sip Message uses an overlay (again)
            // for the body. It ALSO expects that the body
            // will be contiguous (of course).
            // it doesn't need a new buffer in UDP b/c there
            // will only be one datagram per buffer. (1:1 strict)
            
            msg->setBody(buffer+used,len-used);
            //DebugLog(<<"added " << len-used << " byte body");
         }
         
         if (!basicCheck(*msg))
         {
            // basicCheck queued any response required
            delete msg; // cannot use it, so, punt on it...
            msg = 0;
            message =0;
            return;
         }
         
         stampReceived(msg);
      }
      
      mStateMachineFifo.add(message);
   }
}


void 
UdpTransport::buildFdSet( FdSet& fdset )
{
   fdset.setRead(mFd);
    
   if (mTxFifo.messageAvailable())
   {
     fdset.setWrite(mFd);
   }
}

#if 0 
bool 
UdpTransport::stunSendTest(const Tuple&  dest)
{
   resip::Lock lock(myMutex);

   bool changePort=false;
   bool changeIP=false;

   StunAtrString username;
   StunAtrString password;

   username.sizeValue = 0;
   password.sizeValue = 0;
	
   StunMessage req;
   memset(&req, 0, sizeof(StunMessage));
	
   stunBuildReqSimple(&req, username, changePort , changeIP , 1);
	
   char* buf = new char[STUN_MAX_MESSAGE_SIZE];
   int len = STUN_MAX_MESSAGE_SIZE;
	
   int rlen = stunEncodeMessage(req, buf, len, password, false);
	
   SendData* stunRequest = new SendData(dest, buf, rlen);
   mTxFifo.add(stunRequest);

   mStunSuccess = false;

   return true;
}

bool
UdpTransport::stunResult(Tuple& mappedAddress)
{
	resip::Lock lock(myMutex);

	if (mStunSuccess)
	{
		mappedAddress = mStunMappedAddress;
	}
	return mStunSuccess;
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
