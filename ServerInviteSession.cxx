#include "resiprocate/dum/Dialog.hxx"
#include "resiprocate/dum/DialogUsageManager.hxx"
#include "resiprocate/dum/DumTimeout.hxx"
#include "resiprocate/dum/InviteSessionHandler.hxx"
#include "resiprocate/dum/ServerInviteSession.hxx"
#include "resiprocate/dum/UsageUseException.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/compat.hxx"

using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::DUM

ServerInviteSession::ServerInviteSession(DialogUsageManager& dum, Dialog& dialog, const SipMessage& request)
   : InviteSession(dum, dialog),
     mFirstRequest(request)
{
   assert(request.isRequest());
   mState = UAS_Start;
}

ServerInviteSessionHandle 
ServerInviteSession::getHandle()
{
   return ServerInviteSessionHandle(mDum, getBaseHandle().getId());
}

void 
ServerInviteSession::redirect(const NameAddrs& contacts, int code)
{
   InfoLog (<< toData(mState) << ": redirect(" << code << ")"); // -> " << contacts);

   switch (mState)
   {
      case UAS_EarlyNoOffer:
      case UAS_EarlyOffer:
      case UAS_EarlyProvidedAnswer:
      case UAS_EarlyProvidedOffer:
      case UAS_EarlyReliable:
      case UAS_FirstEarlyReliable:
      case UAS_FirstSentOfferReliable:
      case UAS_NoOffer:
      case UAS_NoOfferReliable:
      case UAS_Offer:
      case UAS_OfferProvidedAnswer:
      case UAS_OfferReliable: 
      case UAS_ProvidedOffer:
      case UAS_ReceivedUpdate:
      case UAS_ReceivedUpdateWaitingAnswer:
      case UAS_SentUpdate:
      {
         // !jf! the cleanup for 3xx may be a bit strange if we are in the middle of
         // an offer/answer exchange with PRACK. 
         // e.g. we sent 183 reliably and then 302 before PRACK was received. Ideally,
         // we should send 200PRACK
         transition(Terminated);

         SipMessage response;
         mDialog.makeResponse(response, mFirstRequest, code);
         response.header(h_Contacts) = contacts;
         mDialog.send(response);
         mDum.destroy(this);
         break;
      }

      case UAS_Accepted:
      case UAS_WaitingToHangup:
      case UAS_WaitingToTerminate:
      case UAS_SentUpdateAccepted:
      case UAS_Start:
      default:
         assert(0);
         throw UsageUseException("Can't redirect after accepted", __FILE__, __LINE__);
         break;
   }
}

void 
ServerInviteSession::provisional(int code)
{
   InfoLog (<< toData(mState) << ": provisional(" << code << ")");

   switch (mState)
   {
      case UAS_Offer:
         transition(UAS_EarlyOffer);
         sendProvisional(code);
         break;

      case UAS_OfferProvidedAnswer:
         transition(UAS_EarlyProvidedAnswer);
         sendProvisional(code);
         break;

      case UAS_ProvidedOffer:
         transition(UAS_EarlyProvidedOffer);
         sendProvisional(code);
         break;
         
      case UAS_EarlyOffer:
      case UAS_EarlyNoOffer:
      case UAS_EarlyProvidedOffer:
         transition(UAS_EarlyOffer);
         sendProvisional(code);
         break;
         
      case UAS_NoOffer:
         transition(UAS_EarlyNoOffer);
         sendProvisional(code);
         break;
         

      case UAS_NoOfferReliable:
      case UAS_EarlyReliable:
         // TBD
         assert(0);
         break;
         
      case UAS_EarlyProvidedAnswer:
      case UAS_Accepted:
      case UAS_FirstEarlyReliable:
      case UAS_FirstSentOfferReliable:
      case UAS_OfferReliable: 
      case UAS_ReceivedUpdate:
      case UAS_ReceivedUpdateWaitingAnswer:
      case UAS_SentUpdate:
      case UAS_SentUpdateAccepted:
      case UAS_Start:
      case UAS_WaitingToHangup:
      case UAS_WaitingToTerminate:
      default:
         assert(0);
         break;
   }
}

void 
ServerInviteSession::provideOffer(const SdpContents& offer)
{
   InfoLog (<< toData(mState) << ": provideOffer");
   switch (mState)
   {
      case UAS_NoOffer:
         transition(UAS_ProvidedOffer);
         mProposedLocalSdp = InviteSession::makeSdp(offer);
         break;

      case UAS_EarlyNoOffer:
         transition(UAS_EarlyProvidedOffer);
         mProposedLocalSdp = InviteSession::makeSdp(offer);
         break;
         
      case UAS_NoOfferReliable:
         mProposedLocalSdp = InviteSession::makeSdp(offer);
         // !jf! transition ? 
         break;

      case UAS_EarlyReliable:
         // queue offer
         transition(UAS_SentUpdate);
         mProposedLocalSdp = InviteSession::makeSdp(offer);
         sendUpdate(offer);
         break;
         
      case UAS_Accepted:
      case UAS_EarlyProvidedAnswer:
      case UAS_EarlyProvidedOffer:
      case UAS_FirstEarlyReliable:
      case UAS_FirstSentOfferReliable:
      case UAS_OfferReliable: 
      case UAS_OfferProvidedAnswer:
      case UAS_ProvidedOffer:
      case UAS_ReceivedUpdate:
      case UAS_ReceivedUpdateWaitingAnswer:
      case UAS_SentUpdate:
      case UAS_SentUpdateAccepted:
      case UAS_Start:
      case UAS_WaitingToHangup:
      case UAS_WaitingToTerminate:
         assert(0);
         break;
      default:
         InviteSession::provideOffer(offer);
         break;
   }
}

void 
ServerInviteSession::provideAnswer(const SdpContents& answer)
{
   InviteSessionHandler* handler = mDum.mInviteSessionHandler;
   InfoLog (<< toData(mState) << ": provideAnswer");
   switch (mState)
   {
      case UAS_Offer:
         transition(UAS_OfferProvidedAnswer);
         mCurrentRemoteSdp = mProposedRemoteSdp;
         mCurrentLocalSdp = InviteSession::makeSdp(answer);
         break;
         
      case UAS_EarlyOffer:
         transition(UAS_EarlyProvidedAnswer);
         mCurrentRemoteSdp = mProposedRemoteSdp;
         mCurrentLocalSdp = InviteSession::makeSdp(answer);
         break;
         
      case UAS_OfferReliable: 
         // send1XX-answer, timer::1xx
         transition(UAS_FirstEarlyReliable);
         break;

      case UAS_ReceivedUpdate:
         // send::200U-answer
         transition(UAS_EarlyReliable);
         break;
         
      case UAS_ReceivedUpdateWaitingAnswer:
         // send::2XXU-answer
         // send::2XXI
         handler->onConnected(getSessionHandle(), mInvite200);
         transition(Connected);
         break;

      case UAS_Accepted:
      case UAS_EarlyNoOffer:
      case UAS_EarlyProvidedAnswer:
      case UAS_EarlyProvidedOffer:
      case UAS_EarlyReliable:
      case UAS_FirstEarlyReliable:
      case UAS_FirstSentOfferReliable:
      case UAS_NoOffer:
      case UAS_NoOfferReliable:
      case UAS_OfferProvidedAnswer:
      case UAS_ProvidedOffer:
      case UAS_SentUpdate:
      case UAS_SentUpdateAccepted:
      case UAS_Start:
      case UAS_WaitingToHangup:
      case UAS_WaitingToTerminate:
         assert(0);
         break;
      default:
         InviteSession::provideAnswer(answer);
         break;
   }
}

void 
ServerInviteSession::end()
{
   InfoLog (<< toData(mState) << ": end");
   switch (mState)
   {
      case UAS_EarlyNoOffer:
      case UAS_EarlyOffer:
      case UAS_EarlyProvidedAnswer:
      case UAS_EarlyProvidedOffer:
      case UAS_NoOffer:
      case UAS_Offer:
      case UAS_OfferProvidedAnswer:
      case UAS_ProvidedOffer:
         reject(480);
         break;         
         
      case UAS_OfferReliable: 
      case UAS_ReceivedUpdate:
      case UAS_EarlyReliable:
      case UAS_FirstEarlyReliable:
      case UAS_FirstSentOfferReliable:
      case UAS_NoOfferReliable:
      case UAS_ReceivedUpdateWaitingAnswer:
      case UAS_SentUpdate:
      case UAS_SentUpdateAccepted:
      case UAS_WaitingToHangup:
      case UAS_WaitingToTerminate:
         reject(480);
         break;
         
      case UAS_Start:
         assert(0);
         break;

      case UAS_Accepted:
      default:
         InviteSession::end();
         break;
   }
}

void 
ServerInviteSession::reject(int code)
{
   InfoLog (<< toData(mState) << ": reject(" << code << ")");

   switch (mState)
   {
      case UAS_EarlyNoOffer:
      case UAS_EarlyOffer:
      case UAS_EarlyProvidedAnswer:
      case UAS_EarlyProvidedOffer:
      case UAS_NoOffer:
      case UAS_Offer:
      case UAS_OfferProvidedAnswer:
      case UAS_ProvidedOffer:

      case UAS_EarlyReliable:
      case UAS_FirstEarlyReliable:
      case UAS_FirstSentOfferReliable:
      case UAS_NoOfferReliable:
      case UAS_OfferReliable: 
      case UAS_ReceivedUpdate:
      case UAS_SentUpdate:
      {
         // !jf! the cleanup for 3xx may be a bit strange if we are in the middle of
         // an offer/answer exchange with PRACK. 
         // e.g. we sent 183 reliably and then 302 before PRACK was received. Ideally,
         // we should send 200PRACK
         transition(Terminated);

         SipMessage response;
         mDialog.makeResponse(response, mFirstRequest, code);
         mDialog.send(response);
         mDum.destroy(this);
         break;
      }

      case UAS_Accepted:
      case UAS_ReceivedUpdateWaitingAnswer:
      case UAS_SentUpdateAccepted:
      case UAS_Start:
      case UAS_WaitingToHangup:
      case UAS_WaitingToTerminate:
      default:
         assert(0);
         break;
   }
}

void 
ServerInviteSession::accept(int code)
{
   InfoLog (<< toData(mState) << ": accept(" << code << ")");
   InviteSessionHandler* handler = mDum.mInviteSessionHandler;
   switch (mState)
   {
      case UAS_Offer:
      case UAS_EarlyOffer:
         assert(0);
         break;

      case UAS_OfferProvidedAnswer:
      case UAS_EarlyProvidedAnswer:
         transition(Connected);
         sendAccept(code, mCurrentLocalSdp);
         handler->onConnected(getSessionHandle(), mInvite200);
         break;
         
      case UAS_NoOffer:
      case UAS_EarlyNoOffer:
         assert(0);

      case UAS_ProvidedOffer:
      case UAS_EarlyProvidedOffer:
         transition(UAS_Accepted);
         sendAccept(code, mProposedLocalSdp);
         break;
         
      case UAS_Accepted:
         assert(0);
         break;
         
      case UAS_FirstEarlyReliable:
         // queue 2xx
         // waiting for PRACK
         transition(UAS_Accepted);
         mDialog.makeResponse(mInvite200, mFirstRequest, code);
         handleSessionTimerRequest(mInvite200, mFirstRequest);
         break;
         
      case UAS_EarlyReliable:
         transition(Connected);
         mDialog.makeResponse(mInvite200, mFirstRequest, code);
         handleSessionTimerRequest(mInvite200, mFirstRequest);
         mDialog.send(mInvite200);
         startRetransmitTimer(); // 2xx timer
         handler->onConnected(getSessionHandle(), mInvite200);
         break;

      case UAS_SentUpdate:
         transition(UAS_SentUpdateAccepted);
         mDialog.makeResponse(mInvite200, mFirstRequest, code);
         handleSessionTimerRequest(mInvite200, mFirstRequest);
         mDialog.send(mInvite200);
         startRetransmitTimer(); // 2xx timer
         break;

      case UAS_ReceivedUpdate:
         transition(UAS_ReceivedUpdateWaitingAnswer);
         mDialog.makeResponse(mInvite200, mFirstRequest, code);// queue 2xx
         handleSessionTimerRequest(mInvite200, mFirstRequest);
         break;
         
      case UAS_FirstSentOfferReliable:
      case UAS_NoOfferReliable:
      case UAS_OfferReliable: 
      case UAS_ReceivedUpdateWaitingAnswer:
      case UAS_SentUpdateAccepted:
      case UAS_Start:
      case UAS_WaitingToHangup:
      case UAS_WaitingToTerminate:
      default:
         assert(0);
         break;
   }
}

void 
ServerInviteSession::dispatch(const SipMessage& msg)
{
   switch (mState)
   {
      case UAS_Start:
         dispatchStart(msg);
         break;

      case UAS_Offer:
      case UAS_EarlyOffer:
      case UAS_EarlyProvidedAnswer:
      case UAS_NoOffer:
      case UAS_ProvidedOffer:
      case UAS_EarlyNoOffer:
      case UAS_EarlyProvidedOffer:
         dispatchOfferOrEarly(msg);
         break;
         
      case UAS_Accepted:
         dispatchAccepted(msg);
         break;
         
      case UAS_EarlyReliable:
         dispatchEarlyReliable(msg);
         break;
      case UAS_FirstEarlyReliable:
         dispatchFirstEarlyReliable(msg);
         break;
      case UAS_FirstSentOfferReliable:
         dispatchFirstSentOfferReliable(msg);
         break;
      case UAS_NoOfferReliable:
         dispatchNoOfferReliable(msg);
         break;
      case UAS_OfferReliable:
         dispatchOfferReliable(msg);
         break;
      case UAS_ReceivedUpdate:
         dispatchReceivedUpdate(msg);
         break;
      case UAS_ReceivedUpdateWaitingAnswer:
         dispatchReceivedUpdateWaitingAnswer(msg);
         break;
      case UAS_SentUpdate:
         dispatchSentUpdate(msg);
         break;
      case UAS_WaitingToHangup:
         dispatchWaitingToHangup(msg);
         break;
      case UAS_WaitingToTerminate:
         dispatchWaitingToTerminate(msg);
         break;
      case UAS_SentUpdateAccepted:
         dispatchSentUpdateAccepted(msg);
         break;
      default:
         InviteSession::dispatch(msg);
         break;
   }
}

void 
ServerInviteSession::dispatch(const DumTimeout& timeout)
{
   if (timeout.type() == DumTimeout::Retransmit1xx)
   {
      mDialog.send(m1xx);
      if (mCurrentRetransmit1xx)
      {
         mCurrentRetransmit1xx *= 2;
         mDum.addTimerMs(DumTimeout::Retransmit1xx, resipMin(Timer::T2, mCurrentRetransmit1xx), getBaseHandle(), timeout.seq());
      }
   }
   else
   {
      InviteSession::dispatch(timeout);
   }
}

void
ServerInviteSession::dispatchStart(const SipMessage& msg)
{
   assert(msg.isRequest());
   assert(msg.header(h_CSeq).method() == INVITE);

   InviteSessionHandler* handler = mDum.mInviteSessionHandler;
   const SdpContents* sdp = InviteSession::getSdp(msg);

   switch (toEvent(msg, sdp))
   {
      case OnInviteOffer:
         transition(UAS_Offer);
         mProposedRemoteSdp = InviteSession::makeSdp(*sdp);
         handler->onNewSession(getHandle(), Offer, msg);
         handler->onOffer(getSessionHandle(), msg, sdp);
         break;
      case OnInvite:
         transition(UAS_NoOffer);
         handler->onNewSession(getHandle(), None, msg);
         break;
      case OnInviteReliableOffer:
         transition(UAS_OfferReliable);
         mProposedRemoteSdp = InviteSession::makeSdp(*sdp);
         handler->onNewSession(getHandle(), Offer, msg);
         handler->onOffer(getSessionHandle(), msg, sdp);
         break;
      case OnInviteReliable:
         transition(UAS_NoOfferReliable);
         handler->onNewSession(getHandle(), None, msg);
         break;
      default:
         assert(0);
         break;
   }
}

// Offer, Early, EarlyNoOffer, NoOffer
void
ServerInviteSession::dispatchOfferOrEarly(const SipMessage& msg)
{
   const SdpContents* sdp = InviteSession::getSdp(msg);
   switch (toEvent(msg, sdp))
   {
      case OnCancel:
         dispatchCancel(msg);
         break;
      case OnBye:
         dispatchBye(msg);
         break;
      default:
         assert(msg.isRequest());
         dispatchUnknown(msg);
         break;
   }
}

void
ServerInviteSession::dispatchAccepted(const SipMessage& msg)
{
   InviteSessionHandler* handler = mDum.mInviteSessionHandler;
   const SdpContents* sdp = InviteSession::getSdp(msg);

   switch (toEvent(msg, sdp))
   {
      case OnAckAnswer:
         transition(Connected);
         handler->onAnswer(getSessionHandle(), msg, sdp);
         handler->onConnected(getSessionHandle(), msg);
         break;

      case OnCancel:
      {
         SipMessage c200;
         mDialog.makeResponse(c200, msg, 200);
         mDialog.send(c200);
         break;
      }

      case OnBye:
      {
         SipMessage b200;
         mDialog.makeResponse(b200, msg, 200);
         mDialog.send(b200);
         break;
      }
         
      case OnAck:
      {
         transition(Terminated);
         SipMessage bye;
         mDialog.makeRequest(bye, BYE);
         mDialog.send(bye);
         handler->onTerminated(getSessionHandle(), InviteSessionHandler::GeneralFailure, &msg);  // !slg!
         break;
      }
      
      default:
         if(msg.isRequest())
         {
            dispatchUnknown(msg);
         }
         break;
   }
}

void
ServerInviteSession::dispatchAcceptedWaitingAnswer(const SipMessage& msg)
{
   InviteSessionHandler* handler = mDum.mInviteSessionHandler;
   const SdpContents* sdp = InviteSession::getSdp(msg);

   switch (toEvent(msg, sdp))
   {
      case OnAckAnswer:
         transition(Connected);
         mCurrentLocalSdp = mProposedLocalSdp;
         mCurrentRemoteSdp = InviteSession::makeSdp(*sdp);
         handler->onAnswer(getSessionHandle(), msg, sdp);
         handler->onConnected(getSessionHandle(), msg);
         break;
         
      case OnCancel:
      {
         // no transition

         SipMessage c200;
         mDialog.makeResponse(c200, msg, 200);
         mDialog.send(c200);
         break;
      }

      case OnPrack: // broken
      {
         // no transition

         SipMessage p200;
         mDialog.makeResponse(p200, msg, 200);
         mDialog.send(p200);
         
         mDum.makeResponse(mInvite200, mFirstRequest, 200);
         startRetransmitTimer(); // make 2xx timer
         mDialog.send(mInvite200);  
         
         break;
      }

      default:
         if(msg.isRequest())
         {
            dispatchUnknown(msg);
         }
         break;
   }
}

void
ServerInviteSession::dispatchOfferReliable(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchNoOfferReliable(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchFirstSentOfferReliable(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchFirstEarlyReliable(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchEarlyReliable(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchSentUpdate(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchSentUpdateAccepted(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchReceivedUpdate(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchReceivedUpdateWaitingAnswer(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchWaitingToTerminate(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchWaitingToHangup(const SipMessage& msg)
{
}

void
ServerInviteSession::dispatchCancel(const SipMessage& msg)
{
   transition(Terminated);
   InviteSessionHandler* handler = mDum.mInviteSessionHandler;

   SipMessage c200;
   mDialog.makeResponse(c200, msg, 200);
   mDialog.send(c200);

   SipMessage i487;
   mDialog.makeResponse(i487, mFirstRequest, 487);
   mDialog.send(i487);

   handler->onTerminated(getSessionHandle(), InviteSessionHandler::PeerEnded, &msg);
   mDum.destroy(this);
}

void
ServerInviteSession::dispatchBye(const SipMessage& msg)
{
   transition(Terminated);
   InviteSessionHandler* handler = mDum.mInviteSessionHandler;

   SipMessage b200;
   mDialog.makeResponse(b200, msg, 200);
   mDialog.send(b200);

   SipMessage i487;
   mDialog.makeResponse(i487, mFirstRequest, 487);
   mDialog.send(i487);

   handler->onTerminated(getSessionHandle(), InviteSessionHandler::PeerEnded, &msg);
   mDum.destroy(this);
}

void
ServerInviteSession::dispatchUnknown(const SipMessage& msg)
{
   transition(Terminated);
   InviteSessionHandler* handler = mDum.mInviteSessionHandler;

   SipMessage r481; // !jf! what should we send here? 
   mDialog.makeResponse(r481, msg, 481);
   mDialog.send(r481);
   
   SipMessage i400;
   mDialog.makeResponse(i400, mFirstRequest, 400);
   mDialog.send(i400);

   handler->onTerminated(getSessionHandle(), InviteSessionHandler::GeneralFailure, &msg);
   mDum.destroy(this);
}

void
ServerInviteSession::targetRefresh (const NameAddr& localUri)
{
   WarningLog (<< "Can't refresh before Connected");
   assert(0);
   throw UsageUseException("Can't refresh before Connected", __FILE__, __LINE__);
}

void 
ServerInviteSession::refer(const NameAddr& referTo)
{
   if (isConnected())
   {
      InviteSession::refer(referTo);
   }
   else
   {
      WarningLog (<< "Can't refer before Connected");
      assert(0);
      throw UsageUseException("REFER not allowed in this context", __FILE__, __LINE__);
   }
}

void 
ServerInviteSession::refer(const NameAddr& referTo, InviteSessionHandle sessionToReplace)
{
   if (isConnected())
   {
      InviteSession::refer(referTo, sessionToReplace);
   }
   else
   {
      WarningLog (<< "Can't refer before Connected");
      assert(0);
      throw UsageUseException("REFER not allowed in this context", __FILE__, __LINE__);
   }
}

void 
ServerInviteSession::info(const Contents& contents)
{
   if (isConnected())
   {
      InviteSession::info(contents);
   }
   else
   {
      WarningLog (<< "Can't send INFO before Connected");
      assert(0);
      throw UsageUseException("Can't send INFO before Connected", __FILE__, __LINE__);
   }
}

void
ServerInviteSession::sendProvisional(int code)
{
   mDialog.makeResponse(m1xx, mFirstRequest, code);
   if (mProposedLocalSdp.get()) // early media
   {
      setSdp(m1xx, *mProposedLocalSdp);
   }
   // !jf! start 1xx timer
   mDialog.send(m1xx);
}

void
ServerInviteSession::sendAccept(int code, std::auto_ptr<SdpContents> sdp)
{
   mDialog.makeResponse(mInvite200, mFirstRequest, code);
   handleSessionTimerRequest(mInvite200, mFirstRequest);
   if (sdp.get())
   {
      setSdp(mInvite200, *sdp);
   }

   // make timer::2xx
   // make timer::NoAck
   mDialog.send(mInvite200);
}

void
ServerInviteSession::sendUpdate(const SdpContents& sdp)
{
   if (peerSupportsUpdateMethod())
   {
      SipMessage update;
      mDialog.makeRequest(update, UPDATE);
      InviteSession::setSdp(update, sdp);
      mDialog.send(update);
   }
   else
   {
      throw UsageUseException("Can't send UPDATE to peer", __FILE__, __LINE__);
   }
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
