#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include "resiprocate/os/Socket.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/Connection.hxx"
#include "resiprocate/ConnectionManager.hxx"
#include "resiprocate/SipMessage.hxx"
#include "resiprocate/Security.hxx"
#include "resiprocate/TcpBaseTransport.hxx"

using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::TRANSPORT

char 
Connection::connectionStates[Connection::MAX][32] = { "NewMessage", "ReadingHeaders", "PartialBody" };


Connection::Connection()
   : mSocket(INVALID_SOCKET), // bogus
     mWho(),
     mSendPos(0),
     mMessage(0),
     mBuffer(0),
     mBufferPos(0),
     mBufferSize(0),
     mLastUsed(0),
     mState(NewMessage)
{
}

Connection::Connection(const Tuple& who, Socket socket)
   : mSocket(socket), 
     mWho(who),
     mSendPos(0),
     mMessage(0),
     mBuffer(0),
     mBufferPos(0),
     mBufferSize(Connection::ChunkSize),
     mLastUsed(Timer::getTimeMs()),
     mState(NewMessage)
{
   getConnectionManager().addConnection(this);
}

Connection::~Connection()
{
   if (mSocket != INVALID_SOCKET) // bogus Connections
   {
      //DebugLog (<< "Deleting " << this << " " << mSocket << " with " << mOutstandingSends.size() << " to write");
      while (!mOutstandingSends.empty())
      {
         SendData* sendData = mOutstandingSends.front();
         mWho.transport->fail(sendData->transactionId);
         delete sendData;
         mOutstandingSends.pop_front();
      }
   
      //DebugLog (<< "Shutting down connection " << mSocket);
      closeSocket(mSocket);

      getConnectionManager().removeConnection(this);
   }
}

ConnectionId
Connection::getId() const
{
   return mWho.connectionId;
}

void
Connection::performRead(int bytesRead, Fifo<TransactionMessage>& fifo)
{
   assert(mWho.transport);

   DebugLog(<< "In State: " << connectionStates[mState]);
   getConnectionManager().touch(this);
   
  start:   // If there is an overhang come back here, effectively recursing
   
   switch(mState)
   {
      case NewMessage:
      {
         assert(mWho.transport);
         mMessage = new SipMessage(mWho.transport);
         
         DebugLog(<< "Connection::process setting source " << mWho);
         mMessage->setSource(mWho);
         mMessage->setTlsDomain(mWho.transport->tlsDomain());
         mMsgHeaderScanner.prepareForMessage(mMessage);
         // Fall through to the next case.
      }
      case ReadingHeaders:
      {
         unsigned int chunkLength = mBufferPos + bytesRead;
         char *unprocessedCharPtr;
         MsgHeaderScanner::ScanChunkResult scanChunkResult =
             mMsgHeaderScanner.scanChunk(mBuffer,
                                         chunkLength,
                                         &unprocessedCharPtr);
         if (scanChunkResult == MsgHeaderScanner::scrError)
         {
            //.jacob. Not a terribly informative warning.
            WarningLog(<< "Discarding preparse!");
            delete mBuffer;
            mBuffer = 0;
            delete mMessage;
            mMessage = 0;
            //.jacob. Shouldn't the state also be set here?
            delete this;
            return;
         }
         mMessage->addBuffer(mBuffer);
         unsigned int numUnprocessedChars =
             (mBuffer + chunkLength) - unprocessedCharPtr;
         if (scanChunkResult == MsgHeaderScanner::scrNextChunk)
         {
            // Message header is incomplete...
            if (numUnprocessedChars == 0)
            {
               // ...but the chunk is completely processed.
               //.jacob. I've discarded the "assigned" concept.
               //DebugLog(<< "Data assigned, not fragmented, not complete");
               mBuffer = new char[ChunkSize + 5];
               mBufferPos = 0;
               mBufferSize = ChunkSize;
            }
            else
            {
               // ...but some of the chunk must be shifted into the next one.
               size_t size = numUnprocessedChars*3/2;
               if ( size < Connection::ChunkSize )
               {
                  size = Connection::ChunkSize;
               }
               char* newBuffer = new char[size + 5];
               memcpy(newBuffer, unprocessedCharPtr, numUnprocessedChars);
               mBuffer = newBuffer;
               mBufferPos = numUnprocessedChars;
               mBufferSize = size;
            }
            mState = ReadingHeaders;
         }
         else
         {         
             // The message header is complete.
            size_t contentLength = mMessage->header(h_ContentLength).value();
            
            if (numUnprocessedChars < contentLength)
            {
               // The message body is incomplete.
               char* newBuffer = new char[contentLength + 5];
               memcpy(newBuffer, unprocessedCharPtr, numUnprocessedChars);
               mBufferPos = numUnprocessedChars;
               mBufferSize = contentLength;
               mBuffer = newBuffer;
            
               mState = PartialBody;
            }
            else
            {
               // The message body is complete.
               mMessage->setBody(unprocessedCharPtr, contentLength);
               if (!transport()->basicCheck(*mMessage))
               {
                 delete mMessage;
                 mMessage = 0;
               }
               else
               {
                 Transport::stampReceived(mMessage);
                 DebugLog(<< "##Connection: " << *this << " received: " << *mMessage);
                 fifo.add(mMessage);
               }

               int overHang = numUnprocessedChars - contentLength;

               mState = NewMessage;
               if (overHang > 0) 
               {
                  // The next message has been partially read.
                  size_t size = overHang*3/2;
                  if ( size < Connection::ChunkSize )
                  {
                     size = Connection::ChunkSize;
                  }
                  char* newBuffer = new char[size + 5];
                  memcpy(newBuffer,
                         unprocessedCharPtr + contentLength,
                         overHang);
                  mBuffer = newBuffer;
                  mBufferPos = 0;
                  mBufferSize = size;
                  
                  DebugLog (<< "Extra bytes after message: " << overHang);
                  DebugLog (<< Data(mBuffer, overHang));
                  
                  bytesRead = overHang;
                  goto start;
               }
            }
         }
         break;
      }
      case PartialBody:
      {
         size_t contentLength = mMessage->header(h_ContentLength).value();
         mBufferPos += bytesRead;
         if (mBufferPos == contentLength)
         {
            mMessage->setBody(mBuffer, contentLength);
            if (!transport()->basicCheck(*mMessage))
            {
              delete mMessage;
              mMessage = 0;
            }
            else
            {
              DebugLog(<< "##Connection: " << *this << " received: " << *mMessage);

              Transport::stampReceived(mMessage);
              fifo.add(mMessage);
            }
            mState = NewMessage;
         }
         break;
      }
      default:
         assert(0);
   }
}

void
Connection::requestWrite(SendData* sendData)
{
    assert(mWho.transport);

    if (mOutstandingSends.empty())
   {
      getConnectionManager().addToWritable(this);
   }

   mOutstandingSends.push_back(sendData);
   
}

void
Connection::performWrite()
{
   //assert(hasDataToWrite());

   assert(!mOutstandingSends.empty());
   const Data& data = mOutstandingSends.front()->data;
   DebugLog (<< "Sending " << data.size() - mSendPos << " bytes");
   int nBytes = write(data.data() + mSendPos,data.size() - mSendPos);

   if (nBytes < 0)
   {
      //fail(data.transactionId);
      InfoLog(<< "Write failed on socket: " << this->getSocket() << ", closing connection");
      delete this;
   }
   else
   {
     // Safe because of the conditional above ( < 0 ).
     Data::size_type bytesWritten = static_cast<Data::size_type>(nBytes);
     mSendPos += bytesWritten;
      if (mSendPos == data.size())
      {
         mSendPos = 0;
         delete mOutstandingSends.front();
         mOutstandingSends.pop_front();

         if (mOutstandingSends.empty())
         {
            getConnectionManager().removeFromWritable();
         }
      }
   }
}
            
ConnectionManager&
Connection::getConnectionManager() const
{
   assert(mWho.transport);
   TcpBaseTransport* transport = static_cast<TcpBaseTransport*>(mWho.transport);
   
   return transport->getConnectionManager();
}

std::pair<char*, size_t> 
Connection::getWriteBuffer()
{
   if (mState == NewMessage)
   {
      DebugLog (<< "Creating buffer for " << *this);

      // .dlb. room for sentinels
      mBuffer = new char [Connection::ChunkSize+5];
      mBufferSize = Connection::ChunkSize;
      mBufferPos = 0;
   }
   return std::make_pair(mBuffer + mBufferPos, mBufferSize - mBufferPos);
}
            
std::ostream& 
resip::operator<<(std::ostream& strm, const resip::Connection& c)
{
   strm << "CONN: " << &c << " " << int(c.getSocket()) << " " << c.mWho;
   return strm;
}

Transport* 
Connection::transport()
{
   assert(this);
   return mWho.transport;
}


bool 
Connection::hasDataToRead()
{
   return true;
}


bool 
Connection::isGood()
{
   return true;
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

