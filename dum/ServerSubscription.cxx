#include "resiprocate/dum/Dialog.hxx"
#include "resiprocate/dum/DialogUsageManager.hxx"
#include "resiprocate/dum/ServerSubscription.hxx"
#include "resiprocate/dum/SubscriptionHandler.hxx"
#include "resiprocate/dum/UsageUseException.hxx"

using namespace resip;

ServerSubscriptionHandle 
ServerSubscription::getHandle()
{
   return ServerSubscriptionHandle(mDum, getBaseHandle().getId());
}

ServerSubscription::ServerSubscription(DialogUsageManager& dum,
                                       Dialog& dialog,
                                       const SipMessage& req)
   : BaseSubscription(dum, dialog, req),
     mExpires(60)
{
   mLastRequest = req;
}

ServerSubscription::~ServerSubscription()
{
   mDialog.mServerSubscriptions.remove(this);
}

SipMessage& 
ServerSubscription::accept(int statusCode)
{
   mDialog.makeResponse(mLastResponse, mLastRequest, statusCode);
   mLastResponse.header(h_Expires).value() = mExpires;   
   return mLastResponse;
}

SipMessage& 
ServerSubscription::reject(int statusCode)
{
   if (statusCode < 400)
   {
      throw new UsageUseException("Must reject with a 4xx", __FILE__, __LINE__);
   }
   mDialog.makeResponse(mLastResponse, mLastRequest, statusCode);
   return mLastResponse;
}


void 
ServerSubscription::send(SipMessage& msg)
{
   if (msg.isResponse())
   {
      int code = msg.header(h_StatusLine).statusCode();
      if (code < 100)
      {
         mDum.send(msg);
      }
      else if (code < 300)
      {
         if(msg.exists(h_Expires))
         {
            mDum.addTimer(DumTimeout::Subscription, msg.header(h_Expires).value(), getBaseHandle(), ++mTimerSeq);
            mDum.send(msg);
         }
         else
         {
            throw new UsageUseException("2xx to a Subscribe MUST contain an Expires header", __FILE__, __LINE__);
         }
      }
      else
      {
         mDum.send(msg);
         delete this;
         return;
      }
   }
   else
   {
      mDum.send(msg);
      msg.releaseContents();
   }
}

void 
ServerSubscription::setSubscriptionState(SubscriptionState state)
{
   mSubscriptionState = state;
}

void 
ServerSubscription::dispatch(const SipMessage& msg)
{
   ServerSubscriptionHandler* handler = mDum.getServerSubscriptionHandler(mEventType);
   assert(handler);

   if (msg.isRequest())
   {
      //!dcm! -- need to have a mechanism to retrieve default & acceptable
      //expiration times for an event package--part of handler API?
      if (msg.exists(h_Expires))
      {         
         mExpires = msg.header(h_Expires).value();
         if (mExpires == 0)
         {
            makeNotifyExpires();
            handler->onExpiredByClient(getHandle(), msg, mLastNotify);
            send(mLastNotify);
            return;
         }
         if (mSubscriptionState == Invalid)
         {
            //!dcm! -- should initial state be pending?
            mSubscriptionState = Init;
            handler->onNewSubscription(getHandle(), msg);            
         }
         else
         {
            handler->onRefresh(getHandle(), msg);            
         }
      }
   }
   else
   {
      int code = msg.header(h_StatusLine).statusCode();
      if (code == 200 && mSubscriptionState == Terminated)
      {
         handler->onTerminated(getHandle());
         delete this;         
      }
      else if (code > 399)
      {
         handler->onError(getHandle(), msg);
         handler->onTerminated(getHandle());
         delete this;         
      }
   }
}

void
ServerSubscription::makeNotifyExpires()
{
   mSubscriptionState = Terminated;
   makeNotify();
   mLastNotify.header(h_SubscriptionState).param(p_reason) = getTerminateReasonString(Timeout);   
}

void
ServerSubscription::makeNotify()
{
   mDialog.makeRequest(mLastNotify, NOTIFY);
   mLastNotify.header(h_SubscriptionState).value() = getSubscriptionStateString(mSubscriptionState);
   mLastNotify.header(h_Event).value() = mEventType;   
   if (!mSubscriptionId.empty())
   {
      mLastNotify.header(h_Event).param(p_id) = mSubscriptionId;
   }
}


SipMessage& 
ServerSubscription::end(TerminateReason reason, const Contents* document)
{
   makeNotify();
   mLastNotify.header(h_SubscriptionState).param(p_reason) = getTerminateReasonString(reason);   
   if (document)
   {
      mLastNotify.setContents(document);
   }
   return mLastNotify;
}

void
ServerSubscription::dispatch(const DumTimeout& timeout)
{
   assert(timeout.type() == DumTimeout::Subscription);
   if (timeout.seq() == mTimerSeq)
   {
      ServerSubscriptionHandler* handler = mDum.getServerSubscriptionHandler(mEventType);
      assert(handler);
      makeNotifyExpires();
      handler->onExpired(getHandle(), mLastNotify);
      send(mLastNotify);
   }
}

SipMessage& 
ServerSubscription::update(const Contents* document)
{
   makeNotify();
   mLastNotify.setContents(document);
   return mLastNotify;
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
