#if !defined(ConnectionMap_hxx)
#define ConnectionMap_hxx

#include <map>
#include <list>

#include "sip2/util/Socket.hxx"
#include "sip2/util/Timer.hxx"
#include "sip2/sipstack/Transport.hxx"
#include "sip2/sipstack/SipMessage.hxx"

namespace Vocal2
{

class Preparse;

class ConnectionMap
{
   public:
      // smallest time to reuse
      static const UInt64 MinLastUsed;
      // largest unused time to reclaim
      static const UInt64 MaxLastUsed;
      enum {MaxAttempts = 7};


      ConnectionMap();
      
      class Connection
      {
         public:
            enum State
            {
               NewMessage,
               PartialHeaderRead,
               PartialBodyRead
            };
            
         
            Connection(Transport::Tuple who, Socket socket);
            Socket getSocket() {return mSocket;}
            
            void allocateBuffer(int maxBufferSize);
            bool process(int bytesRead, Fifo<Message>& fifo, Preparse& preparse, int maxBufferSize);
            bool prepNextMessage(int bytesUsed, int bytesRead, Fifo<Message>& fifo, Preparse& preparse, int maxBufferSize);
            bool readAnyBody(int bytesUsed, int bytesRead, Fifo<Message>& fifo, Preparse& preparse, int maxBufferSize);
            
            
            Data::size_type mSendPos;     //position in current message being sent
            std::list<SendData*> mOutstandingSends;
            SendData* mCurrent;
            
            SipMessage* mMessage;
            char* mBuffer;
            int mBytesRead;
            int mBufferSize;
            
            Connection();
            Connection* remove(); // return next youngest
            ~Connection();

            Connection* mYounger;
            Connection* mOlder;

            Transport::Tuple mWho;


         private:
            Socket mSocket;
            UInt64 mLastUsed;
            
            State mState;

            friend class ConnectionMap;
      };

      Connection* add(Transport::Tuple who, Socket s);
      Connection* get(Transport::Tuple who, int attempt = 1);
      void close(Transport::Tuple who);

      // release excessively old connections
      void gc(UInt64 threshhold = ConnectionMap::MaxLastUsed);

      typedef std::map<Transport::Tuple, Connection*> Map;
      Map mConnections;
      
      // move to youngest
      void touch(Connection* connection);
      
      Connection mPreYoungest;
      Connection mPostOldest;
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
