#if defined(HAVE_CONFIG_H)
#include "resip/stack/config.hxx"
#endif

#include <iostream>

#include "repro/Proxy.hxx"
#include "repro/RequestContext.hxx"
#include "resip/stack/ExtensionParameter.hxx"
#include "resip/stack/SipMessage.hxx"
#include "resip/stack/TransactionTerminated.hxx"
#include "rutil/Inserter.hxx"
#include "rutil/Logger.hxx"

#include "repro/Ack200DoneMessage.hxx"
#include "repro/ForkControlMessage.hxx"

// Remove warning about 'this' use in initiator list
#if defined(WIN32)
#pragma warning( disable : 4355 ) // using this in base member initializer list
#endif

#define RESIPROCATE_SUBSYSTEM Subsystem::REPRO

using namespace resip;
using namespace repro;
using namespace std;

RequestContext::RequestContext(Proxy& proxy, 
                               ProcessorChain& requestP,
                               ProcessorChain& responseP,
                               ProcessorChain& targetP) :
   mOriginalRequest(0),
   mCurrentEvent(0),
   mRequestProcessorChain(requestP),
   mResponseProcessorChain(responseP),
   mTargetProcessorChain(targetP),
   mTransactionCount(1),
   mProxy(proxy),
   mHaveSentFinalResponse(false),
   mTargetConnectionId(0),
   mResponseContext(*this),
   mTCSerial(0)
{
   mInitialTimerCSet=false;
}

RequestContext::~RequestContext()
{
   DebugLog (<< "RequestContext::~RequestContext() " << this);
   if (mOriginalRequest != mCurrentEvent)
   {
      delete mOriginalRequest;
      mOriginalRequest = 0;
   }
   delete mCurrentEvent;
   mCurrentEvent = 0;
}


void
RequestContext::process(resip::TransactionTerminated& msg)
{
   InfoLog (<< "RequestContext::process(TransactionTerminated) " 
            << msg.getTransactionId() << " : " << *this);

   if (msg.isClientTransaction())
   {
      mResponseContext.removeClientTransaction(msg.getTransactionId());
   }
   mTransactionCount--;
   if (mTransactionCount == 0)
   {
      delete this;
   }
}

void
RequestContext::process(std::auto_ptr<resip::SipMessage> sipMessage)
{
   DebugLog (<< "process(SipMessage) " << *this);

   if (mCurrentEvent != mOriginalRequest)
   {
      delete mCurrentEvent;
   }
   mCurrentEvent = sipMessage.release();

   
   SipMessage* sip = dynamic_cast<SipMessage*>(mCurrentEvent);
   if (!mOriginalRequest) 
   { 
      assert(sip);
      mOriginalRequest=sip;
	  
	  // RFC 3261 Section 16.4
      fixStrictRouterDamage();
      removeTopRouteIfSelf();
   }


   Processor::processor_action_t ret=Processor::Continue;
   // if it's a CANCEL I need to call processCancel here 
   if (sip->isRequest())
   {
      if (sip->header(h_RequestLine).method() == CANCEL)
      {
         mResponseContext.processCancel(*sip);
      }
      else
      {
         ret = mRequestProcessorChain.process(*this);
      }
      
      if(ret != Processor::WaitingForEvent &&
         !mHaveSentFinalResponse && 
         !mResponseContext.hasActiveTransactions())
      {
         if(mResponseContext.hasPendingTransactions())
         {
            //Someone forgot to start _any_ of the targets they just added.
            ErrLog(<<"In RequestContext, request processor "
            << "chain appears to have added targets, but not processed any of them."
            << " (Bad monkey?)");
         }
         else
         {
            ErrLog(<< "In RequestContext, request processor "
            << "chain appears to have added no targets. Further, there are no active"
            << " transactions. There is no guarantee that any targets will ever "
            << " be added. (Bad monkey?)");
            // TODO make sure to do the right thing here.
         }
      }
      
   }
   else if (sip->isResponse())
   {
      // Do the lemurs if its a response (response processor chain)
      // Call handle Response if its a response
      
      Processor::processor_action_t ret = Processor::Continue;
      ret = mResponseProcessorChain.process(*this);
            
      // this is temporarily not allowed since to allow async requests in the
      // response chain we will need to maintain a collection of all of the
      // outstanding responses that are still processing. 
      assert(ret != Processor::WaitingForEvent);
      if (ret == Processor::Continue) 
      {
         mResponseContext.processResponse(*sip);

         if(!mHaveSentFinalResponse && 
            !mResponseContext.hasActiveTransactions())
         {
            if(mResponseContext.hasPendingTransactions())
            {
               //The last active transaction has ended, and the response processors
               //did not start any of the pending transactions.
               ErrLog(<<"In RequestContext, after processing "
               << "a sip response: The last active transaction has ended, we have "
               << "no final response, and the response processors did not start "
               << "any of the pending transactions. (Bad lemur?)");
            }
            else
            {
               ErrLog(<<"In RequestContext, after processing "
               << "a sip response: The last active transaction has ended, but we"
               << " have not sent a final response. (What happened here?) ");
               // TODO make sure to do the right thing.
            }
         }

         
         return;
      }
   }
   

}


void
RequestContext::process(std::auto_ptr<ApplicationMessage> app)
{
   DebugLog (<< "process(ApplicationMessage) " << *this);

   if (mCurrentEvent != mOriginalRequest)
   {
      delete mCurrentEvent;
   }
   mCurrentEvent = app.release();


   Ack200DoneMessage* ackDone = dynamic_cast<Ack200DoneMessage*>(mCurrentEvent);
   if (ackDone)
     {
       delete this;
       return;
     }

   TimerCMessage* tc = dynamic_cast<TimerCMessage*>(mCurrentEvent);

   if(tc)
   {
      if(tc->mSerial == mTCSerial)
      {
         mResponseContext.processTimerC();
      }

      return;
   }

   Processor::processor_action_t ret=Processor::Continue;
   ret = mRequestProcessorChain.process(*this);

   if(ret != Processor::WaitingForEvent &&
      !mHaveSentFinalResponse && 
      !mResponseContext.hasActiveTransactions())
   {
      if(mResponseContext.hasPendingTransactions())
      {
         //Someone forgot to start _any_ of the targets they just added.
         ErrLog(<<"In RequestContext, request processor "
         << "chain appears to have added targets, but not processed any of them."
         << " (Bad monkey?)");
      }
      else
      {
         ErrLog(<< "In RequestContext, request processor "
         << "chain appears to have added no targets. Further, there are no active"
         << " transactions. There is no guarantee that any targets will ever "
         << " be added. (Bad monkey?)");
         // TODO make sure to do the right thing here.
      }
   }
}

resip::SipMessage& 
RequestContext::getOriginalRequest()
{
   return *mOriginalRequest;
}

const resip::SipMessage& 
RequestContext::getOriginalRequest() const
{
   return *mOriginalRequest;
}

const resip::Data&
RequestContext::getTransactionId() const
{
   return mOriginalRequest->getTransactionId();
}

resip::Message* 
RequestContext::getCurrentEvent()
{
   return mCurrentEvent;
}

const resip::Message* 
RequestContext::getCurrentEvent() const
{
   return mCurrentEvent;
}

void 
RequestContext::setDigestIdentity (const resip::Data& data)
{
   mDigestIdentity = data;
}

const resip::Data& 
RequestContext::getDigestIdentity() const
{
   return mDigestIdentity;
}

resip::Data
RequestContext::addTarget(const NameAddr& addr, bool beginImmediately)
{
   InfoLog (<< "Adding candidate " << addr);
   repro::Target target(addr);
   mResponseContext.addTarget(target,beginImmediately);
   return target.tid();
}


void
RequestContext::updateTimerC()
{
   InfoLog(<<"Updating timer C.");
   mTCSerial++;
   TimerCMessage* tc = new TimerCMessage(this->getTransactionId(),mTCSerial);
   mProxy.postTimerC(std::auto_ptr<TimerCMessage>(tc));
}

void
RequestContext::postTimedMessage(std::auto_ptr<resip::ApplicationMessage> msg,int seconds)
{
   mProxy.postTimedMessage(msg,seconds);
}


void
RequestContext::sendResponse(const SipMessage& msg)
{
   assert (msg.isResponse());
   mProxy.send(msg);
   if (msg.header(h_StatusLine).statusCode()>199)
   {
      mHaveSentFinalResponse=true;
   }
}

//      This function assumes that if ;lr shows up in the
//      RURI, that it's a URI we put in a Record-Route header
//      earlier. It will do the wrong thing if some other 
//      malbehaving implementation lobs something at us with
//      ;lr in the RURI and it wasn't us.
//		(from Section 16.4 of RFC 3261)
void
RequestContext::fixStrictRouterDamage()
{
   if (mOriginalRequest->header(h_RequestLine).uri().exists(p_lr))
   {
      if (    mOriginalRequest->exists(h_Routes)
              && !mOriginalRequest->header(h_Routes).empty())
      {
         mOriginalRequest->header(h_RequestLine).uri()=
            mOriginalRequest->header(h_Routes).back().uri();
         mOriginalRequest->header(h_Routes).pop_back();
      }
      else
      {
         //!RjS! When we wire this class for logging, here's a
         //      place to log a warning
      }
   }
}

/** @brief Pops the topmost route if it's us */
void
RequestContext::removeTopRouteIfSelf()
{
   if (    mOriginalRequest->exists(h_Routes)
           && !mOriginalRequest->header(h_Routes).empty()
           &&  mProxy.isMyUri(mOriginalRequest->header(h_Routes).front().uri())
      )
   {
      // !jf! this is to get tcp connections to go over the correct connection id
      const Uri& route = mOriginalRequest->header(h_Routes).front().uri();

      static ExtensionParameter p_cid("cid");
      static ExtensionParameter p_cid1("cid1");
      static ExtensionParameter p_cid2("cid2");

      ConnectionId cid1 = route.exists(p_cid1) ? route.param(p_cid1).convertUnsignedLong() : 0;
      ConnectionId cid2 = route.exists(p_cid2) ? route.param(p_cid2).convertUnsignedLong() : 0;
      if (mOriginalRequest->getSource().connectionId != 0 && 
          mOriginalRequest->getSource().connectionId == cid1)
      {
         mTargetConnectionId = cid2;
      }
      else if (mOriginalRequest->getSource().connectionId != 0 && 
               mOriginalRequest->getSource().connectionId == cid2)
      {
         mTargetConnectionId = cid1;
      }
         
      mOriginalRequest->header(h_Routes).pop_front();
   }

}

void
RequestContext::pushChainIterator(ProcessorChain::Chain::iterator& i)
{
   mChainIteratorStack.push_back(i);
}

ProcessorChain::Chain::iterator
RequestContext::popChainIterator()
{
   ProcessorChain::Chain::iterator i;
   i = mChainIteratorStack.back();
   mChainIteratorStack.pop_back();
   return i;
}

bool
RequestContext::chainIteratorStackIsEmpty()
{
   return mChainIteratorStack.empty();
}

Proxy& 
RequestContext::getProxy()
{
   return mProxy;
}

ResponseContext&
RequestContext::getResponseContext()
{
   return mResponseContext;
}

std::ostream&
repro::operator<<(std::ostream& strm, const RequestContext& rc)
{
   strm << "RequestContext: "
        << " identity=" << rc.mDigestIdentity
        << " count=" << rc.mTransactionCount
        << " final=" << rc.mHaveSentFinalResponse;

   if (rc.mOriginalRequest) strm << " orig requri=" << rc.mOriginalRequest->brief();
   //if (rc.mCurrentEvent) strm << " current=" << rc.mCurrentEvent->brief();
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
