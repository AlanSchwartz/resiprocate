
#ifndef WIN32
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <memory>
#include "sip2/util/Data.hxx"
#include "sip2/util/Socket.hxx"
#include "sip2/util/Logger.hxx"
#include "sip2/util/compat.hxx"
#include "sip2/sipstack/TlsTransport.hxx"
#include "sip2/sipstack/SipMessage.hxx"
#include "sip2/sipstack/Preparse.hxx"
#include "sip2/sipstack/Security.hxx"

#define VOCAL_SUBSYSTEM Subsystem::TRANSPORT

using namespace std;
using namespace Vocal2;

const size_t TlsTransport::MaxWriteSize = 4096;
const size_t TlsTransport::MaxReadSize = 4096;


TlsTransport::TlsTransport(const Data& sendhost, int portNum, 
                           const Data& nic, Fifo<Message>& fifo,Security* security) : 
   Transport(sendhost, portNum, nic , fifo)
{
   mSendPos = mSendRoundRobin.end();
   mFd = socket(PF_INET, SOCK_STREAM, 0);
   mSecurity = security;
   assert( mSecurity );
   
   if ( mFd == INVALID_SOCKET )
   {
      InfoLog (<< "Failed to open socket: " << portNum);
   }
   
#ifndef WIN32
   int on = 1;
   if ( ::setsockopt ( mFd, SOL_SOCKET, SO_REUSEADDR, // | SO_REUSEPORT,
                       (void*)(&on), sizeof ( on )) )
   {
      int err = errno;
      InfoLog (<< "Couldn't set sockoptions SO_REUSEPORT | SO_REUSEADDR: " 
               << strerror(err));
      throw Exception("Failed setsockopt", __FILE__,__LINE__);
   }
#endif

   //makeSocketNonBlocking(mFd);

   sockaddr_in addr;
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY); // !jf!--change when nic specfied
   addr.sin_port = htons(portNum);
  
   if ( bind( mFd, (struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR )
   {
      int err = errno;
      if ( err == EADDRINUSE )
      {
         ErrLog (<< "Port " << portNum << " already in use");
      }
      else
      {
         ErrLog (<< "Could not bind to port: " << portNum);
      }
      
      throw Exception("Address already in use", __FILE__,__LINE__);
   }

   makeSocketNonBlocking(mFd);

   // do the listen, seting the maximum queue size for compeletly established
   // sockets -- on linux, tcp_max_syn_backlog should be used for the incomplete
   // queue size(see man listen)
   int e = listen(mFd,64 );

   if (e != 0 )
   {
      //int err = errno;
      // !cj! deal with errors
      assert(0);
   }
}


TlsTransport::~TlsTransport()
{
   //::shutdown(mFd, SHUT_RDWR);
   closesocket(mFd);
}


void 
TlsTransport::buildFdSet( FdSet& fdset)
{
   fdset.setRead(mFd);

   for (ConnectionMap::Map::iterator it = mConnectionMap.mConnections.begin();
        it != mConnectionMap.mConnections.end(); it++)
   {
      fdset.setRead(it->second->getSocket());

      // !xx! TODO only should add this if there is data to write
      //fdset.setWrite(it->second->getSocket());
   }
}


void 
TlsTransport::processListen(FdSet& fdset)
{
   if (fdset.readyToRead(mFd))
   {
      struct sockaddr_in peer;
		
      socklen_t peerLen=sizeof(peer);
      Socket sock = accept( mFd, (struct sockaddr*)&peer,&peerLen);
      if ( sock == -1 )
      {
         int err = errno;
         DebugLog( << "Error on accept: " << strerror(err));
         return;
      }

      TlsConnection* tls = new TlsConnection( mSecurity, sock, /*server*/ true );
      assert(tls);
      
      makeSocketNonBlocking(sock);
      
      Transport::Tuple who;
      who.ipv4 = peer.sin_addr;
      who.port = ntohs(peer.sin_port);
      who.transportType = Transport::TLS;
      who.transport = this;
      
      Connection* con = mConnectionMap.add(who, sock);
      assert( con );
      con->mTlsConnection = tls;

      DebugLog( << "Added server connection " << int(con) );
   }
}


bool
TlsTransport::processRead(Connection* c)
{
   std::pair<char* const, size_t> writePair = c->getWriteBuffer();
   size_t bytesToRead = writePair.second;
   if ( bytesToRead > TlsTransport::MaxReadSize)
   {
      bytesToRead = TlsTransport::MaxReadSize;
   }
   
   DebugLog( << "Read from connection " << int(c) );

   assert( c->mTlsConnection );
   int bytesRead = c->mTlsConnection->read( writePair.first, bytesToRead);
   if (bytesRead <= 0)
   {
      DebugLog(<< "bytesRead: " << bytesRead);
      DebugLog(<< "TlsTransport::processRead failed for " << int(c) );
      return false;
   }   
   if (c->process(bytesRead, mStateMachineFifo))
   {
      mConnectionMap.touch(c);
      return true;
   }
   else
   {
      InfoLog (<< "TlsTransport::processRead failed due to bad message " << *c );
      return false;
   }
}


void
TlsTransport::processAllReads(FdSet& fdset)
{
   if (!mConnectionMap.mConnections.empty())
   {
      for (Connection* c = mConnectionMap.mPostOldest.mYounger;
           c != &mConnectionMap.mPreYoungest; c = c->mYounger)
      {
         assert(c);
         assert(c->mTlsConnection);
         
         if ( fdset.readyToRead(c->getSocket()) ||
              c->mTlsConnection->hasDataToRead() )
         {
            if (processRead(c))
            {
               return;
            }
            else
            {
               DebugLog(<< "TlsTransport::processAllReads -- closing: " << *c);
               mConnectionMap.close(c->mWho);
               return;
            }
         }
      }
   }
}


void
TlsTransport::processAllWrites( FdSet& fdset )
{
   if (mTxFifo.messageAvailable())
   {
      SendData* data = mTxFifo.getNext();
      Connection* conn = mConnectionMap.get(data->destination);
        
      if (conn == 0)
      { 
         // attempt to open
         Socket sock = socket( AF_INET, SOCK_STREAM, 0 );

         if ( sock == -1 ) // no socket found - try to free one up and try again
         {
            // !dlb! does the file descriptor become available immediately?
            mConnectionMap.gc(ConnectionMap::MinLastUsed); // free one up 
            sock = socket( AF_INET, SOCK_STREAM, 0 ); // try again 
         }
         
         if ( sock == -1 )
         { 
            DebugLog( << "Error in TLS finding free socket to use" );
         } 
         else
         {
            struct sockaddr_in servaddr;
            memset( &servaddr, sizeof(servaddr), 0 );
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons( data->destination.port);
            servaddr.sin_addr =  data->destination.ipv4;
            
            int e = connect( sock, (struct sockaddr *)&servaddr, sizeof(servaddr) );
            if ( e == -1 ) 
            {
               int err = errno;
               DebugLog( << "Error on TLS connect to " <<  data->destination << ": " << strerror(err));
            }
            else
            {
               // succeeded, add the connection
               TlsConnection* tls = new TlsConnection( mSecurity, sock, /*server*/ false );
               assert(tls);

               mConnectionMap.add( data->destination, sock);

               conn = mConnectionMap.get(data->destination);
               assert( conn );

               conn->mTlsConnection = tls;   

               makeSocketNonBlocking(sock);

               DebugLog( << "Added TLS client connection " << int(conn) );
            }
         }
      }
        
      if (conn == 0)
      {
         DebugLog (<< "Failed to create/get connection: " << data->destination);
         fail(data->transactionId);
         delete data;
         return;
      }

      if (conn->mOutstandingSends.empty())
      {
         DebugLog (<< "Adding " << *conn << " to send roundrobin");
         mSendRoundRobin.push_back(conn);
         conn->mSendPos = 0;
      }

      conn->mOutstandingSends.push_back(data);
   }

   sendFromRoundRobin(fdset);
}         


void
TlsTransport::sendFromRoundRobin(FdSet& fdset)
{
   if (!mSendRoundRobin.empty())
   {
      ConnectionList::iterator rrPos = mSendPos;
      do
      {
         if (mSendPos == mSendRoundRobin.end())
         {
            mSendPos = mSendRoundRobin.begin();
         }
         if (true) // !cj! need to add to fd set - if (fdset.readyToWrite((*mSendPos)->getSocket()))
         {
            if (processWrite(*mSendPos))
            {
               if ((*mSendPos)->mOutstandingSends.empty())
               {
                  DebugLog (<< "Finished send, removing " << **mSendPos << " from roundrobin");
                  mSendPos = mSendRoundRobin.erase(mSendPos);
               }
               else
               {
                  mSendPos++;
               }
               return;
            }
            else
            {
               DebugLog (<< "Failed send, removing " << **mSendPos << " from roundrobin");
               DebugLog(<< "TlsTransport::processAllWrites -- closing: " << **mSendPos);
               mConnectionMap.close((*mSendPos)->mWho);
               mSendPos = mSendRoundRobin.erase(mSendPos);
               return;
            }
         }
         else
         {
            mSendPos++;
         }
      } while(mSendPos != rrPos && !mSendRoundRobin.empty());
   }
}


bool
TlsTransport::processWrite(Connection* c)
{
   assert(c);
   
   assert(!c->mOutstandingSends.empty());
   SendData* data = c->mOutstandingSends.front();
   
   size_t bytesToWrite = data->data.size() - c->mSendPos;
   if ( bytesToWrite > TlsTransport::MaxWriteSize)
   { 
      bytesToWrite = TlsTransport::MaxWriteSize;
   }

   DebugLog( << "Write to connection " << int(c) );

   assert( c->mTlsConnection );
   int bytesWritten = c->mTlsConnection->write( data->data.data() + c->mSendPos, bytesToWrite);
   int err = errno;

   if (bytesWritten <= 0)
   {
      DebugLog (<< "Failed write to " << *c << " :" << strerror(err));
      fail(data->transactionId);
      return false; 
   }
   else if ((size_t)bytesWritten < data->data.size() - c->mSendPos)
   {
      DebugLog (<< "##Wrote " << bytesWritten << " to " << *c);
      c->mSendPos += bytesWritten;
   }
   else
   {
      DebugLog (<< "##Finished write " << bytesWritten << " to " << *c);
      DebugLog (<< "Sent: " << data->data);
      c->mOutstandingSends.pop_front();
      DebugLog (<< "mOutstandingSends.size() " << c->mOutstandingSends.size());
      c->mSendPos = 0;
      ok(data->transactionId);
      delete data;
   }
   mConnectionMap.touch(c);
   return true;
}


void 
TlsTransport::process(FdSet& fdSet)
{
   if ( mTxFifo.messageAvailable() ) 
   {
      DebugLog(<<"TLSTransport mTxFifo:size: " << mTxFifo.size());
   }
   processAllWrites(fdSet);
   processListen(fdSet);
   processAllReads(fdSet);
   //DebugLog(<< "Finished TlsTransport::process");
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
