#if !defined(RESIP_CONNECTION_BASE_HXX)
#define RESIP_CONNECTION_BASE_HXX 

/**
   !dlb!
   Relationship between buffer allocation and parsing is broken.

   If the read returns with a few bytes, a new buffer is allocated in performRead.

   performRead should handle allocation, read, and parsing. it should be the only
   public read accessor in Connection. read should be protected.

   !dcm! -- reworking Connection.  Friendship is gone unless a
   compelling arguement can be made, as ConnectionManager will now
   have subclasses.  Accessors now instead of direct access to private
   memebers.  All buffer stuff is hidden in read for synchronous
   connections and handleRead for async connections.
*/

#include <list>

#include "resiprocate/os/Timer.hxx"
#include "resiprocate/os/Fifo.hxx"
#include "resiprocate/Transport.hxx"
#include "resiprocate/MsgHeaderScanner.hxx"
#include "resiprocate/SendData.hxx"
#include "resiprocate/os/Win32Export.hxx"

namespace resip
{

class TransactionMessage;

class RESIP_API ConnectionBase
{
      friend RESIP_API  std::ostream& operator<<(std::ostream& strm, const resip::ConnectionBase& c);
   public:
      ConnectionBase(const Tuple& who);
      ConnectionId getId() const;

      Transport* transport();

      Tuple& who() { return mWho; }
      const UInt64& whenLastUsed() { return mLastUsed; }

      enum { ChunkSize = 2048 }; // !jf! what is the optimal size here? 

   protected:
      enum State
      {
         NewMessage = 0,
         ReadingHeaders,
         PartialBody,
         MAX
      };
	
      State getCurrentState() const { return mState; }
      void preparseNewBytes(int bytesRead, Fifo<TransactionMessage>& fifo);
      std::pair<char*, size_t> getWriteBuffer();
	 
      // for avoiding copies in external transports--not used in core resip
      void setBuffer(char* bytes, int count);

      Data::size_type mSendPos;
      std::list<SendData*> mOutstandingSends; // !jacob! intrusive queue?

      ConnectionBase();
      virtual ~ConnectionBase();
      // no value semantics
      ConnectionBase(const Connection&);
      ConnectionBase& operator=(const Connection&);
   protected:
      Tuple mWho;
   private:
      SipMessage* mMessage;
      char* mBuffer;
      size_t mBufferPos;
      size_t mBufferSize;

      static char connectionStates[MAX][32];
      UInt64 mLastUsed;
      State mState;
      MsgHeaderScanner mMsgHeaderScanner;
};

//RESIP_API std::ostream& operator<<(std::ostream& strm, const resip::ConnectionBase& c);

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
