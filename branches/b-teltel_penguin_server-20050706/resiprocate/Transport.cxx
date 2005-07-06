#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include <iostream>

#if defined(HAVE_SYS_SOCKIO_H)
#include <sys/sockio.h>
#endif

#include "resiprocate/os/compat.hxx"
#include "resiprocate/os/DnsUtil.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/Socket.hxx"

#include "resiprocate/Transport.hxx"
#include "resiprocate/SipMessage.hxx"
#include "resiprocate/TransportMessage.hxx"
#include "resiprocate/Helper.hxx"
#include "resiprocate/os/WinLeakCheck.hxx"

using namespace resip;
using namespace std;

#define RESIPROCATE_SUBSYSTEM Subsystem::TRANSPORT

Transport::Exception::Exception(const Data& msg, const Data& file, const int line) :
   BaseException(msg,file,line)
{
}

Transport::Transport(Fifo<TransactionMessage>& rxFifo,
                     int portNum, 
                     IpVersion version,
                     const Data& intfc) :
   mFd(-1),
   mInterface(intfc),
   mTuple(intfc, portNum, version==V4 ),
   mStateMachineFifo(rxFifo),
   mShuttingDown(false)
{
}

Transport::~Transport()
{
   if (mFd != -1)
   {
      //DebugLog (<< "Closing " << mFd);
      closeSocket(mFd);
   }
   mFd = -2;
}

bool
Transport::isFinished() const
{
   return !mTxFifo.messageAvailable();
}

void
Transport::shutdown()
{
   // !jf! should use the fifo to pass this in
   mShuttingDown = true;
}

Socket
Transport::socket(TransportType type, bool ipv4)
{
   Socket fd;
   switch (type)
   {
      case UDP:
#ifdef USE_IPV6
         fd = ::socket(ipv4 ? PF_INET : PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
#else
         fd = ::socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif
         break;
      case TCP:
      case TLS:
#ifdef USE_IPV6
         fd = ::socket(ipv4 ? PF_INET : PF_INET6, SOCK_STREAM, 0);
#else
         fd = ::socket(PF_INET, SOCK_STREAM, 0);
#endif
         break;
      default:
         InfoLog (<< "Try to create an unsupported socket type: " << Tuple::toData(type));
         assert(0);
         throw Exception("Unsupported transport", __FILE__,__LINE__);
   }
   
   if ( fd == INVALID_SOCKET )
   {
      int e = getErrno();
      error(e);
      throw Exception("Can't create TcpBaseTransport", __FILE__,__LINE__);
   }

   DebugLog (<< "Creating fd=" << fd << (ipv4 ? " V4/" : " V6/") << (type == UDP ? "UDP" : "TCP"));
   
   return fd;
}

void 
Transport::bind()
{
   DebugLog (<< "Binding to " << DnsUtil::inet_ntop(mTuple));
   
   if ( ::bind( mFd, &mTuple.getMutableSockaddr(), mTuple.length()) == SOCKET_ERROR )
   {
      int e = getErrno();
      if ( e == EADDRINUSE )
      {
         error(e);
         ErrLog (<< mTuple << " already in use ");
         throw Exception("port already in use", __FILE__,__LINE__);
      }
      else
      {
         error(e);
         ErrLog (<< "Could not bind to " << mTuple);
         throw Exception("Could not use port", __FILE__,__LINE__);
      }
   }
   
   bool ok = makeSocketNonBlocking(mFd);
   if ( !ok )
   {
      ErrLog (<< "Could not make socket non-blocking " << port());
      throw Exception("Failed making socket non-blocking", __FILE__,__LINE__);
   }
}

void
Transport::error(int e)
{
   switch (e)
   {
      case EAGAIN:
         //InfoLog (<< "No data ready to read" << strerror(e));
         break;
      case EINTR:
         InfoLog (<< "The call was interrupted by a signal before any data was read : " << strerror(e));
         break;
      case EIO:
         InfoLog (<< "I/O error : " << strerror(e));
         break;
      case EBADF:
         InfoLog (<< "fd is not a valid file descriptor or is not open for reading : " << strerror(e));
         break;
      case EINVAL:
         InfoLog (<< "fd is attached to an object which is unsuitable for reading : " << strerror(e));
         break;
      case EFAULT:
         InfoLog (<< "buf is outside your accessible address space : " << strerror(e));
         break;

#if defined(WIN32)
      case WSAENETDOWN: 
         InfoLog (<<" The network subsystem has failed.  ");
         break;
      case WSAEFAULT:
         InfoLog (<<" The buf or from parameters are not part of the user address space, "
                   "or the fromlen parameter is too small to accommodate the peer address.  ");
         break;
      case WSAEINTR: 
         InfoLog (<<" The (blocking) call was canceled through WSACancelBlockingCall.  ");
         break;
      case WSAEINPROGRESS: 
         InfoLog (<<" A blocking Windows Sockets 1.1 call is in progress, or the "
                   "service provider is still processing a callback function.  ");
         break;
      case WSAEINVAL: 
         InfoLog (<<" The socket has not been bound with bind, or an unknown flag was specified, "
                   "or MSG_OOB was specified for a socket with SO_OOBINLINE enabled, "
                   "or (for byte stream-style sockets only) len was zero or negative.  ");
         break;
      case WSAEISCONN : 
         InfoLog (<<"The socket is connected. This function is not permitted with a connected socket, "
                  "whether the socket is connection-oriented or connectionless.  ");
         break;
      case WSAENETRESET:
         InfoLog (<<" The connection has been broken due to the keep-alive activity "
                  "detecting a failure while the operation was in progress.  ");
         break;
      case WSAENOTSOCK :
         InfoLog (<<"The descriptor is not a socket.  ");
         break;
      case WSAEOPNOTSUPP:
         InfoLog (<<" MSG_OOB was specified, but the socket is not stream-style such as type "
                   "SOCK_STREAM, OOB data is not supported in the communication domain associated with this socket, "
                   "or the socket is unidirectional and supports only send operations.  ");
         break;
      case WSAESHUTDOWN:
         InfoLog (<<"The socket has been shut down; it is not possible to recvfrom on a socket after "
                  "shutdown has been invoked with how set to SD_RECEIVE or SD_BOTH.  ");
         break;
      case WSAEMSGSIZE:
         InfoLog (<<" The message was too large to fit into the specified buffer and was truncated.  ");
         break;
      case WSAETIMEDOUT: 
         InfoLog (<<" The connection has been dropped, because of a network failure or because the "
                  "system on the other end went down without notice.  ");
         break;
      case WSAECONNRESET : 
         InfoLog (<<"Connection reset ");
         break;

	  case WSAEWOULDBLOCK:
         DebugLog (<<"Would Block ");
         break;
#endif

      default:
         InfoLog (<< "Some other error (" << e << "): " << strerror(e));
         break;
   }
}


void
Transport::thread()
{
   InfoLog (<< "Starting transport thread for " << mTuple);
#if defined(USE_EPOLL)
   int epollfd = ::open("/dev/epoll", O_RDWR);
   if (epollfd < 0)
   {
      int e = getErrno();
      Transport::error( e );
      ErrLog (<< "Can't find epoll on this system");
      assert(0);
   }
   
   int ret = ::ioctl(epollfd, EP_ALLOC, maxFileDescriptors());
   if (ret != 0)
   {
      int e = getErrno();
      Transport::error( e );
      ErrLog (<< "Failed to define for " << maxFileDescriptors() << " descriptors");
      assert(0);
   }

   char *map = (char *)mmap(NULL, EP_MAP_SIZE(maxFileDescriptors(), 
                                              PROT_READ | PROT_WRITE, MAP_PRIVATE, n
                                              epollfd, 0));
   if (map <= 0)
   {
      ErrLog (<< "Failed to allocate space for epoll in kernel for " << maxFileDescriptors() << " descriptors");
      assert(0);
   }
   
   while (!mShutdown)
   {
      struct pollfd pfd;
      pfd.fd = fd;
      pfd.events = POLLIN | POLLOUT | POLLERR | POLLHUP;
      pfd.revents = 0;
      
      if (write(kdpfd, &pfd, sizeof(pfd)) != sizeof(pfd)) 
      {
         
         /* report error */
      }
      



      FdSet fdset; 
      buildFdSet(fdset);
      int  err = fdset.selectMilliSeconds(100);
      if (err >= 0)
      {
         try
         {
            process(fdset);
         }
         catch (BaseException& e)
         {
            InfoLog (<< "Uncaught exception: " << e);
         }
      }
   }
#else // !USE_EPOLL
   while (!mShutdown)
   {
      FdSet fdset; 
      buildFdSet(fdset);
      int  err = fdset.selectMilliSeconds(100);
      if (err >= 0)
      {
         try
         {
            process(fdset);
         }
         catch (BaseException& e)
         {
            InfoLog (<< "Uncaught exception: " << e);
         }
      }
   }
#endif

}

void
Transport::fail(const Data& tid)
{
   if (!tid.empty())
   {
      mStateMachineFifo.add(new TransportMessage(tid, true));
   }
}

bool 
Transport::hasDataToSend() const
{
   return mTxFifo.messageAvailable();
}


void 
Transport::send( const Tuple& dest, const Data& d, const Data& tid)
{
   SendData* data = new SendData(dest, d, tid);
   assert(dest.getPort() != -1);
   DebugLog (<< "Adding message to tx buffer to: " << dest); // << " " << d.escaped());
   mTxFifo.add(data); // !jf!
}

SendData*
Transport::makeFailedResponse(const SipMessage& msg,
                              int responseCode,
                              const char * warning)
{
  if (msg.isResponse()) return 0;

  const Tuple& dest = msg.getSource();

  std::auto_ptr<SipMessage> errMsg(Helper::makeResponse(msg, 
                                                        responseCode, 
                                                        warning ? warning : "Original request had no Vias"));
  
  // make send data here w/ blank tid and blast it out.
  // encode message
  Data encoded;
  encoded.clear();
  DataStream encodeStream(encoded);
  errMsg->encode(encodeStream);
  encodeStream.flush();
  assert(!encoded.empty());

  InfoLog(<<"Sending response directly to " << dest << " : " << errMsg->brief() );
  SendData * sd = new SendData( dest, encoded, Data::Empty );
  
  return sd;
}


void
Transport::stampReceived(SipMessage* message)
{
   // set the received= and rport= parameters in the message if necessary !jf!
   if (message->isRequest() && message->exists(h_Vias) && !message->header(h_Vias).empty())
   {
      const Tuple& tuple = message->getSource();
      message->header(h_Vias).front().param(p_received) = DnsUtil::inet_ntop(tuple);
      if (message->header(h_Vias).front().exists(p_rport))
      {
         message->header(h_Vias).front().param(p_rport).port() = tuple.getPort();
      }
   }
   DebugLog (<< "incoming from: " << message->getSource());
   StackLog (<< endl << *message);
}


bool
Transport::basicCheck(const SipMessage& msg)
{
   if (msg.isExternal())
   {
      if (!Helper::validateMessage(msg))
      {
         InfoLog(<<"Message Failed basicCheck :" << msg.brief());
         if (msg.isRequest())
         {
            // this is VERY low-level b/c we don't have a transaction...
            // here we make a response to warn the offending party.
            SendData * sd = makeFailedResponse(msg);
            assert(sd);
            mTxFifo.add(sd);
         }
         return false;
      }
      else if (mShuttingDown && msg.isRequest())
      {
         InfoLog (<< "Server has been shutdown, reject message with 503");
         // this is VERY low-level b/c we don't have a transaction...
         // here we make a response to warn the offending party.
         SendData * sd = makeFailedResponse(msg, 503, "Server has been shutdown");
         assert(sd);
         mTxFifo.add(sd);
      }
   }
   return true;
}

bool 
Transport::operator==(const Transport& rhs) const
{
   return ( ( mTuple.isV4() == rhs.isV4()) &&
            ( port() == rhs.port()) &&
            ( memcmp(&boundInterface(),&rhs.boundInterface(),mTuple.length()) == 0) );
}

unsigned int 
Transport::getFifoSize() const
{
   return mTxFifo.size();
}
    
std::ostream& 
resip::operator<<(std::ostream& strm, const resip::Transport& rhs)
{
   strm << "Transport: " << rhs.mTuple;
   if (!rhs.mInterface.empty()) strm << " on " << rhs.mInterface;
   return strm;
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
