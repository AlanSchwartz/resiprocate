
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

#if !defined(DIALOG_HXX)
#define DIALOG_HXX

#include <iostream>
#include <sipstack/SipMessage.hxx>

namespace Vocal2
{

class Dialog
{
   public:
      class Exception : public VException 
      {
         public:
            Exception( const Vocal2::Data& msg,
                       const Vocal2::Data& file,
                       const int line): VException(msg,file,line){}
            const char* name() const { return "Dialog::Exception"; }
      };
      
      
      // pass in a contact for this location e.g. "sip:local@domain:5060"
      Dialog(const NameAddr& localContact);
      
      // Used by the UAS to create a dialog or early dialog, will return a 1xx(code)
      // UAS should call this upon receiving the invite/subscribe. 
      // This should not be called twice if the UAS sends back a 180 and a 200,
      // it should call it for the 180 and then use Dialog::makeResponse to make
      // the 200
      SipMessage* createDialogAsUAS(const SipMessage& request, int code=200);
      
      // This happens when a dialog gets created on a UAC when 
      // a UAC receives a response that creates a dialog
      void createDialogAsUAC(const SipMessage& request, const SipMessage& response);

      // Called when a 2xx response is received in an existing dialog
      // Replace the _remoteTarget with uri from Contact header in response
      void targetRefreshResponse(const SipMessage& response);

      // Called when a request is received in an existing dialog
      // return status code of response to generate - 0 if ok
      int targetRefreshRequest(const SipMessage& request);

      const Data& dialogId() const { return mDialogId; }
      const Data& getLocalTag() const { return mLocalTag; }
      const NameAddr& getRemoteTarget() const { return mRemoteTarget; }
      const CallId& getCallId() const { return mCallId; }
      

      // For creating requests within a dialog
      SipMessage* makeInvite();
      SipMessage* makeBye();
      SipMessage* makeRefer(const NameAddr& referTo);
      SipMessage* makeNotify();
      SipMessage* makeOptions();
      SipMessage* makeAck(const SipMessage& request);
      SipMessage* makeCancel(const SipMessage& request);
      SipMessage* makeResponse(const SipMessage& request, int code);
      
      // resets to an empty dialog with no state
      void clear();
      
   private:
      SipMessage* makeRequest(MethodTypes method);
      void incrementCSeq(SipMessage& request);
      void copyCSeq(SipMessage& request);

      Via mVia;          // for this UA
      NameAddr mContact;  // for this UA

      // Dialog State
      bool mCreated;
      NameAddrs mRouteSet;
      NameAddr mRemoteTarget;
      unsigned long mRemoteSequence;
      bool mRemoteEmpty;
      unsigned long mLocalSequence;
      bool mLocalEmpty;
      CallId mCallId;
      Data mLocalTag;
      Data mRemoteTag;
      NameAddr mRemoteUri;
      NameAddr mLocalUri;
      Data mDialogId;

      friend std::ostream& operator<<(std::ostream& strm, Dialog& d);
};

std::ostream&
operator<<(std::ostream& strm, Dialog& d);
 
} // namespace Cathay

#endif

