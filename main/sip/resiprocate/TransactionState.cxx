#include "sip2/util/Socket.hxx"
#include "sip2/util/Logger.hxx"
#include "sip2/util/MD5Stream.hxx"
#include "sip2/sipstack/TransactionState.hxx"
#include "sip2/sipstack/TransportSelector.hxx"
#include "sip2/sipstack/TransactionTerminated.hxx"
#include "sip2/sipstack/SipStack.hxx"
#include "sip2/sipstack/SipMessage.hxx"
#include "sip2/sipstack/TimerMessage.hxx"
#include "sip2/sipstack/MethodTypes.hxx"
#include "sip2/sipstack/Helper.hxx"
#include "sip2/sipstack/TransportMessage.hxx"
#include "sip2/sipstack/ReliabilityMessage.hxx"
#include "sip2/sipstack/DnsResolver.hxx"


using namespace Vocal2;

#define VOCAL_SUBSYSTEM Subsystem::TRANSACTION

unsigned long TransactionState::StatelessIdCounter = 1;

TransactionState::TransactionState(SipStack& stack, Machine m, State s) : 
   mStack(stack),
   mMachine(m), 
   mState(s),
   mIsReliable(false), // !jf! 
   mCancelStateMachine(0),
   mMsgToRetransmit(0),
   mDnsState(NotStarted),
   mCurrent(mTuples.end()),
   mId()
{
   if (m == ServerNonInvite || m == ServerInvite)
   {
      mDnsState = NoLookupRequired;
   }
}

TransactionState* 
TransactionState::makeCancelTransaction(TransactionState* tr, Machine machine)
{
   TransactionState* cancel = new TransactionState(tr->mStack, machine, Trying);
   cancel->mIsReliable = tr->mIsReliable;
   cancel->mSource = tr->mSource;
   return cancel;
}

TransactionState::~TransactionState()
{
   const Data& id = tid(mMsgToRetransmit);
   
   DebugLog (<< "Deleting TransactionState " << id);
   mStack.mTransactionMap.remove(id);

   // mCancelStateMachine will take care of deleting itself
   
   delete mMsgToRetransmit;
   mMsgToRetransmit = 0;

   mState = Bogus;
}


void
TransactionState::process(SipStack& stack)
{
   Message* message = stack.mStateMacFifo.getNext();
   assert(message);

   DebugLog (<< "TransactionState::process: " << *message);
   
   SipMessage* sip = dynamic_cast<SipMessage*>(message);
   Data tid = message->getTransactionId();

   DebugLog (<< "got message out of state machine fifo: tid='" << tid << "' " << message->brief());
   
   TransactionState* state = stack.mTransactionMap.find(tid);

   // this code makes sure that an ACK to a 200 is going to create a new
   // stateless transaction. In an ACK to a failure response, the mToTag will
   // have been set in the ServerTransaction as the 4xx passes through so it
   // will match. 
   if (state && sip && sip->isRequest() && sip->header(h_RequestLine).getMethod() == ACK)
   {
      if (sip->header(h_To).exists(p_tag) && sip->header(h_To).param(p_tag) != state->mToTag)
      {
         DebugLog (<< "Must have received an ACK to a 200");
         state = 0; // will be sent statelessly, if from TU
      }
   }

   if (state) // found transaction for sip msg
   {
      DebugLog (<< "Found transaction for tid=" << tid << " " << *state);
      
      switch (state->mMachine)
      {
         case ClientNonInvite:
            state->processClientNonInvite(message);
            break;
         case ClientInvite:
            // ACK from TU will be Stateless
            assert (!(state->isFromTU(sip) &&  sip->isRequest() && sip->header(h_RequestLine).getMethod() == ACK));
            state->processClientInvite(message);
            break;
         case ServerNonInvite:
            state->processServerNonInvite(message);
            break;
         case ServerInvite:
            state->processServerInvite(message);
            break;
         case Stateless:
            state->processStateless(message);
            break;
         default:
            assert(0);
      }
   }
   else if (sip)  // new transaction
   {
      if (sip->isRequest())
      {
         // create a new state object and insert in the TransactionMap
               
         if (sip->isExternal()) // new sip msg from transport
         {
            if (sip->header(h_RequestLine).getMethod() == INVITE)
            {
               DebugLog (<< "Create new INVITE transaction for inbound msg " << sip->brief());

               // !rk! This might be needlessly created.  Design issue.
               TransactionState* state = new TransactionState(stack, ServerInvite, Trying);
               state->mMsgToRetransmit = state->make100(sip);
               state->mSource = sip->getSource();
               // since we don't want to reply to the source port unless rport present 
               state->mSource.port = Helper::getSentPort(*sip);
               state->mState = Proceeding;
               stack.mTransactionMap.add(tid,state);
               
               if (Timer::T100 == 0)
               {
                  state->sendToWire(state->mMsgToRetransmit); // will get deleted when this is deleted
               }
               else
               {
                  DebugLog(<<" adding T100 timer (INV)");
                  stack.mTimers.add(Timer::TimerTrying, tid, Timer::T100);
               }
            }
            else if (sip->header(h_RequestLine).getMethod() == ACK)
            {
               // !jf! no transaction need be created here
            }
            else 
            {
               DebugLog(<<"Adding non-INVITE transaction state " << sip->brief());
               TransactionState* state = new TransactionState(stack, ServerNonInvite,Trying);
               state->mSource = sip->getSource();
               // since we don't want to reply to the source port unless rport present 
               state->mSource.port = Helper::getSentPort(*sip);
               stack.mTransactionMap.add(tid,state);
            }

            // Incoming ACK just gets passed to the TU
            DebugLog(<< "Adding incoming message to TU fifo " << tid);
            stack.mTUFifo.add(sip);
         }
         else // new sip msg from the TU
         {
            DebugLog (<< "Create new transaction for msg from TU " << tid);
            if (sip->header(h_RequestLine).getMethod() == INVITE)
            {
               TransactionState* state = new TransactionState(stack, ClientInvite, Calling);
               stack.mTransactionMap.add(tid,state);
               state->processClientInvite(sip);
            }
            else if (sip->header(h_RequestLine).getMethod() == ACK)
            {
               TransactionState* state = new TransactionState(stack, Stateless, Calling);
               state->mId = Data(TransactionState::StatelessIdCounter++);
               stack.mTransactionMap.add(state->mId,state);
               DebugLog (<< "Creating stateless transaction: " << state->mId);
               state->processStateless(sip);
            }
            else 
            {
               TransactionState* state = new TransactionState(stack, ClientNonInvite, Trying);
               stack.mTransactionMap.add(tid,state);
               state->processClientNonInvite(sip);
            }
         }
      }
      else if (sip->isResponse()) // stray response
      {
         if (stack.mDiscardStrayResponses)
         {
            DebugLog (<< "discarding stray response: " << sip->brief());
            delete message;
         }
         else
         {
            DebugLog (<< "forwarding stateless response: " << sip->brief());
            TransactionState* state = new TransactionState(stack, Stateless, Calling);
            state->mId = Data(TransactionState::StatelessIdCounter++);
            DebugLog (<< "Creating stateless transaction: " << state->mId);
            stack.mTransactionMap.add(state->mId,state);
            state->processStateless(sip);
         }
      }
      else // wasn't a request or a response
      {
         DebugLog (<< "discarding unknown message: " << sip->brief());
      }
   } 
   else // timer or other non-sip msg
   {
      DebugLog (<< "discarding non-sip message: " << message->brief());
      delete message;
   }
}

void
TransactionState::processStateless(Message* message)
{
   // for ACK messages from the TU, there is no transaction, send it directly
   // to the wire // rfc3261 17.1 Client Transaction
   SipMessage* sip = dynamic_cast<SipMessage*>(message);
   TransportMessage* result = dynamic_cast<TransportMessage*>(message);
   DebugLog (<< "TransactionState::processStateless: " << message->brief());
   
   if (sip)
   {
      if (isFromTU(message))
      {
         //assert(sip->header(h_RequestLine).getMethod() == ACK);
         DebugLog (<< "Sending to wire statelessly: " << *sip);
         mMsgToRetransmit = sip;
         sendToWire(sip);
      }
      else
      {
         DebugLog (<< "Sending to TU statelessly: " << *sip);
         sendToTU(sip);
      }
   }
   else if (isDns(message) || isTransportError(message))
   {
      // resend with a different tuple or delete this if no more tuples
      processDns(message);
   }
   else if (result)
   {
      // the pseudotransaction ends on successful transmission
      delete message;
      delete this;
   }
   else
   {
      delete message;
   }
}


// this method will advance the dns state machine using either Sending::Failed
// or DnsMessage inputs
void
TransactionState::processDns(Message* message)
{
   // handle Sending::Failed messages here
   if (isTransportError(message))
   {
      DebugLog (<< "TransactionState::processDns: " << message->brief());
      assert (mDnsState != NotStarted);
      
      DnsResolver::TupleIterator next = mCurrent;
      next++;
      
      if (next != mTuples.end()) // not at end
      {
         mCurrent = next;
         mMsgToRetransmit->header(h_Vias).front().param(p_branch).incrementTransportSequence();
         sendToWire(mMsgToRetransmit);
      }
      else
      {
         // send 503 
         // not for response or ACK !jf!
         if (mMsgToRetransmit->isRequest() && mMsgToRetransmit->header(h_RequestLine).getMethod() != ACK)
         {
            sendToTU(Helper::makeResponse(*mMsgToRetransmit, 503));
         }
         
         DebugLog (<< "Failed to send a request to any tuples");
         delete this;
      }

      delete message;
      return;
   }
   
   DnsResolver::DnsMessage* dns = dynamic_cast<DnsResolver::DnsMessage*>(message);
   if (dns)
   {
      // If dns query failed, the DnsResolver will return an empty list of tuples
      mDnsState = Complete;
      mTuples = dns->mTuples;
      mCurrent = mTuples.begin();
      sendToWire(mMsgToRetransmit);
      delete dns;
   }
}

void
TransactionState::processClientNonInvite(  Message* msg )
{ 
   DebugLog (<< "TransactionState::processClientNonInvite: " << msg->brief());

   if (isRequest(msg) && !isInvite(msg) && isFromTU(msg))
   {
      DebugLog (<< "received new non-invite request");
      SipMessage* sip = dynamic_cast<SipMessage*>(msg);
      mMsgToRetransmit = sip;
      mStack.mTimers.add(Timer::TimerF, msg->getTransactionId(), 64*Timer::T1 );
      sendToWire(sip);  // don't delete
   }
   else if (isSentReliable(msg))
   {
      DebugLog (<< "received sent reliably message");
      // ignore
      delete msg;
   } 
   else if (isSentUnreliable(msg))
   {
      DebugLog (<< "received sent unreliably message");
      // state might affect this !jf!
      // should we set mIsReliable = false here !jf!
      mStack.mTimers.add(Timer::TimerE1, msg->getTransactionId(), Timer::T1 );
      delete msg;
   }
   else if (isResponse(msg) && isFromWire(msg)) // from the wire
   {
      DebugLog (<< "received response from wire");

      SipMessage* sip = dynamic_cast<SipMessage*>(msg);
      int code = sip->header(h_StatusLine).responseCode();
      if (code >= 100 && code < 200) // 1XX
      {
         if (mState == Trying || mState == Proceeding)
         {
            mState = Proceeding;
            if (!mIsReliable)
            {
               mStack.mTimers.add(Timer::TimerE2, msg->getTransactionId(), Timer::T2 );
            }
            sendToTU(msg); // don't delete            
         }
         else
         {
            // ignore
            delete msg;
         }
      }
      else if (code >= 200)
      {
         if (mIsReliable)
         {
            sendToTU(msg); // don't delete
            terminateClientTransaction(msg->getTransactionId());
            delete this;
         }
         else
         {
            mState = Completed;
            mStack.mTimers.add(Timer::TimerK, msg->getTransactionId(), Timer::T4 );            
            sendToTU(msg); // don't delete            
         }
      }
   }
   else if (isTimer(msg))
   {
      DebugLog (<< "received timer in client non-invite transaction");

      TimerMessage* timer = dynamic_cast<TimerMessage*>(msg);
      switch (timer->getType())
      {
         case Timer::TimerE1:
            if (mState == Trying)
            {
               unsigned long d = timer->getDuration();
               if (d < Timer::T2) d *= 2;
               mStack.mTimers.add(Timer::TimerE1, msg->getTransactionId(), d);
               resendToWire(mMsgToRetransmit);
               delete msg;
            }
            else
            {
               // ignore
               delete msg;
            }
            break;

         case Timer::TimerE2:
            if (mState == Proceeding)
            {
               mStack.mTimers.add(Timer::TimerE2, msg->getTransactionId(), Timer::T2);
               resendToWire(mMsgToRetransmit);
               delete msg;
            }
            else 
            {
               // ignore
               delete msg;
            }
            break;

         case Timer::TimerF:
            // !jf! is this correct
            sendToTU(Helper::makeResponse(*mMsgToRetransmit, 408));
            terminateClientTransaction(msg->getTransactionId());
            delete this;
            break;

         case Timer::TimerK:
            terminateClientTransaction(msg->getTransactionId());
            delete this;
            break;

         default:

            assert(0);
            break;
      }
   }
   else if (isDns(msg))
   {
      processDns(msg);
   }
   else if (isTransportError(msg))
   {
      if (mMsgToRetransmit->isRequest() && mMsgToRetransmit->header(h_RequestLine).getMethod() == CANCEL)
      {
	 // In the case of a client-initiated CANCEL, we don't want to
	 // try other transports in the case of transport error as the
	 // CANCEL MUST be sent to the same IP/PORT as the orig. INVITE.
	 sendToTU(Helper::makeResponse(*mMsgToRetransmit, 503));
	 delete msg;
      }
      else
      {
	 // Try more potential delivery tuples.
	 processDns(msg);
      }
   }
   else
   {
      DebugLog (<< "TransactionState::processClientNonInvite: message unhandled");
      delete msg;
   }
}


void
TransactionState::processClientInvite(  Message* msg )
{
   DebugLog(<< "TransactionState::processClientInvite: " << *msg);
   
   if (isRequest(msg) && isFromTU(msg))
   {
      SipMessage* sip = dynamic_cast<SipMessage*>(msg);
      switch (sip->header(h_RequestLine).getMethod())
      {
         /* Received INVITE request from TU="Transaction User", Fire Timer B which controls
            transaction timeouts. alsoHandle CANCEL , To Handle Response to the CANCEL use
            processClientNonInvite 
         */

         case INVITE:
            mMsgToRetransmit = sip;
            mStack.mTimers.add(Timer::TimerB, msg->getTransactionId(), 64*Timer::T1 );
            sendToWire(msg); // don't delete msg
            break;
            
         case CANCEL:
	    if (!mCancelStateMachine)
	    {
	       //mCancelStateMachine = new TransactionState(mStack, ClientNonInvite, Trying);
               mCancelStateMachine = TransactionState::makeCancelTransaction(this, ClientNonInvite);
	    }
	    // It would seem more logical to pass sip to processClientNonInvite
	    // directly but this would be wrong as it would redo the DNS
	    // query and might end up different.  The CANCEL MUST be sent to
	    // the same coordinates as the INVITE it cancels.
	    delete mCancelStateMachine->mMsgToRetransmit;
	    mCancelStateMachine->mMsgToRetransmit = sip;
	    mStack.mTimers.add(Timer::TimerF, sip->getTransactionId(), 64*Timer::T1);
            resendToWire(sip);
            break;
            
         default:
            delete msg;
            break;
      }
   }
   else if (isSentUnreliable(msg))
   {
      switch (mMsgToRetransmit->header(h_RequestLine).getMethod())
      {
         /* Unreliable transport is used start TIMER A 
            (Timer A controls request retransmissions) 
         */
         case INVITE:
            if (isSentReliable(msg))
            {
               mStack.mTimers.add(Timer::TimerA, msg->getTransactionId(), Timer::T1 );
            }
            delete msg;
            break;
            
         case CANCEL:
            mCancelStateMachine->processClientNonInvite(msg);
            // !jf! memory mgmt? 
            break;
            
         default:
            delete msg;
            break;
      }
   }
   else if (isResponse(msg) && isFromWire(msg))
   {
      SipMessage* sip = dynamic_cast<SipMessage*>(msg);
      int code = sip->header(h_StatusLine).responseCode();
      switch (sip->header(h_CSeq).method())
      {
         case INVITE:
            /* If the client transaction receives a provisional response while in
               the "Calling" state, it transitions to the "Proceeding" state. In the
               "Proceeding" state, the client transaction SHOULD NOT retransmit the
               request any longer (this will be Handled in  "else if (isTimer(msg))")
               The Retransmissions will be stopped, Not by Cancelling Timers but
               by Ignoring the fired Timers depending upon the State which stack is in.   
            */
            if (code >= 100 && code < 200) // 1XX
            {
               if (mState == Calling || mState == Proceeding)
               {
                  mState = Proceeding;
                  sendToTU(sip); // don't delete msg
               }
               else
               {
                  delete msg;
               }
            }

	    /* When in either the "Calling" or "Proceeding" states, reception of a
               2xx response MUST cause the client transaction to enter the
               "Terminated" state, and the response MUST be passed up to the TU 
               State Machine is changed to Stale since, we wanted to ensure that 
               all 2xx gets to TU
	    */
              
	    else if (code >= 200 && code < 300)
            {
               sendToTU(sip); // don't delete msg               
               terminateClientTransaction(msg->getTransactionId());
               delete this;
            }
            else if (code >= 300)
            {
               /* When in either the "Calling" or "Proceeding" states, reception of a
                  response with status code from 300-699 MUST cause the client
                  transaction to transition to "Completed".
               */
                 
               if (mIsReliable)
               {
                  /*  Stack MUST pass the received response up to the TU, and the client
                      transaction MUST generate an ACK request, even if the transport is
                      reliable
                  */
                  SipMessage* invite = mMsgToRetransmit;
                  mMsgToRetransmit = Helper::makeFailureAck(*invite, *sip);
                  delete invite;
                  
                  // want to use the same transport as was selected for Invite
                  resendToWire(mMsgToRetransmit);
                  sendToTU(msg); // don't delete msg
                  terminateClientTransaction(msg->getTransactionId());
                  delete this;
               }
               else
               {
                  if (mState == Calling || mState == Proceeding)
                  {
                     /*  MUST pass the received response up to the TU, and the client
                         transaction MUST generate an ACK request, even if the transport is
                         reliable, if transport is Unreliable then Fire the Timer D which 
                         take care of re-Transmission of ACK 
                     */
                     mState = Completed;
                     mStack.mTimers.add(Timer::TimerD, msg->getTransactionId(), Timer::TD );
                     SipMessage* ack;
                     ack = Helper::makeFailureAck(*mMsgToRetransmit, *sip);
                     resendToWire(ack);
                     sendToTU(msg); // don't delete msg
                  }
                  else if (mState == Completed)
                  {
                     /* Any retransmissions of the final response that are received while in
                        the "Completed" state MUST cause the ACK to be re-passed to the
                        transport layer for retransmission
		     */
                     SipMessage* ack;
                     ack = Helper::makeFailureAck(*mMsgToRetransmit, *sip);
                     resendToWire(ack);
                     sendToTU(msg); // don't delete msg
                  }
                  else
                  {
                     /* This should never Happen if it happens we should have a plan
                        what to do here?? for now assert will work
                     */
                     assert(0);
                  }
               }
            }
            break;
            
         case CANCEL:
            /*
              Let processClientNonInvite Handle the CANCEL
	    */
            mCancelStateMachine->processClientNonInvite(msg);
            // !jf! memory mgmt? 
            break;

         default:
            delete msg;
            break;
      }
   }
   else if (isTimer(msg))
   {
      /* Handle Transaction Timers , Retransmission Timers which were set and Handle
         Cancellation of Timers for Re-transmissions here */

      TimerMessage* timer = dynamic_cast<TimerMessage*>(msg);
      DebugLog (<< "timer fired: " << *timer);
      
      switch (timer->getType())
      {
         case Timer::TimerA:
            if (mState == Calling)
            {
               unsigned long d = timer->getDuration();
               if (d < Timer::T2) d *= 2;

               mStack.mTimers.add(Timer::TimerA, msg->getTransactionId(), d);
               resendToWire(mMsgToRetransmit);
            }
            delete msg;
            break;

         case Timer::TimerB:
	    if (mState == Calling)
	    {
               sendToTU(Helper::makeResponse(*mMsgToRetransmit, 408));
               terminateClientTransaction(msg->getTransactionId());
               delete this;
	    }
            delete msg;
            break;

         case Timer::TimerD:
            terminateClientTransaction(msg->getTransactionId());
            delete msg;
            delete this;
            break;

         default:
            assert(mCancelStateMachine);
            mCancelStateMachine->processClientNonInvite(msg);
            break;
      }
   }
   else if (isDns(msg))
   {
      if (mCancelStateMachine &&
	  mCancelStateMachine->mMsgToRetransmit &&
	  mCancelStateMachine->mMsgToRetransmit->getTransactionId() ==
	  msg->getTransactionId())
      {
	 DebugLog (<< "TransactionState::processClientInvite: passing DNS to CANCEL transaction");
	 mCancelStateMachine->processDns(msg);
      }
      else
      {
	 processDns(msg);
      }
   }
   else
   {
      DebugLog (<< "TransactionState::processClientInvite: message unhandled");
      delete msg;
   }
}


void
TransactionState::processServerNonInvite(  Message* msg )
{
   DebugLog (<< "TransactionState::processServerNonInvite: " << msg->brief());

   if (isRequest(msg) && !isInvite(msg) && isFromWire(msg)) // from the wire
   {
      if (mState == Trying)
      {
         // ignore
         delete msg;
      }
      else if (mState == Proceeding || mState == Completed)
      {
         resendToWire(mMsgToRetransmit);
         delete msg;
      }
      else
      {
         assert(0);
      }
   }
   else if (isResponse(msg) && isFromTU(msg))
   {
      SipMessage* sip = dynamic_cast<SipMessage*>(msg);
      int code = sip->header(h_StatusLine).responseCode();
      if (code >= 100 && code < 200) // 1XX
      {
         if (mState == Trying || mState == Proceeding)
         {
            delete mMsgToRetransmit;
            mMsgToRetransmit = sip;
            mState = Proceeding;
            sendToWire(sip); // don't delete msg
         }
         else
         {
            // ignore
            delete msg;
         }
      }
      else if (code >= 200 && code <= 699)
      {
         if (mIsReliable)
         {
            mMsgToRetransmit = sip;
            sendToWire(sip); // don't delete msg
            delete this;
            terminateServerTransaction(msg->getTransactionId());
         }
         else
         {
            if (mState == Trying || mState == Proceeding)
            {
               mState = Completed;
               mStack.mTimers.add(Timer::TimerJ, msg->getTransactionId(), 64*Timer::T1 );
               mMsgToRetransmit = sip;
               sendToWire(sip); // don't delete msg
            }
            else if (mState == Completed)
            {
               // ignore
               delete msg;               
            }
            else
            {
               assert(0);
            }
         }
      }
      else
      {
         // ignore
         delete msg;               
      }
   }
   else if (isTimer(msg))
   {
      assert (mState == Completed);
      assert(dynamic_cast<TimerMessage*>(msg)->getType() == Timer::TimerJ);
      terminateServerTransaction(msg->getTransactionId());
      delete msg;
      delete this;
   }
   else if (isDns(msg))
   {
      processDns(msg);
   }
   else
   {
      DebugLog (<< "TransactionState::processServerNonInvite: message unhandled");
      delete msg;
   }
}


void
TransactionState::processServerInvite(  Message* msg )
{
   DebugLog (<< "TransactionState::processServerInvite: " << msg->brief());
   if (isRequest(msg) && isFromWire(msg))
   {
      SipMessage* sip = dynamic_cast<SipMessage*>(msg);
      switch (sip->header(h_RequestLine).getMethod())
      {
         case INVITE:
            if (mState == Proceeding || mState == Completed)
            {
               /*
                 The server transaction has already been constructed so this
                 message is a retransmission.  The server transaction must
                 respond with a 100 Trying _or_ the last provisional response
                 passed from the TU for this transaction.
               */
               DebugLog (<< "Received invite from wire - forwarding to TU state=" << mState);
	       if (!mMsgToRetransmit)
	       {
	          mMsgToRetransmit = make100(sip); // for when TimerTrying fires
               }
               delete msg;
	       sendToWire(mMsgToRetransmit);
            }
            else
            {
               DebugLog (<< "Received invite from wire - ignoring state=" << mState);
               delete msg;
            }
            break;
            
         case ACK:
            /*
              If an ACK is received while the server transaction is in the
              "Completed" state, the server transaction MUST transition to the
              "Confirmed" state.
	    */
            if (mState == Completed)
            {
               if (mIsReliable)
               {
                  DebugLog (<< "Received ACK in Completed (reliable) - delete transaction");
                  terminateServerTransaction(msg->getTransactionId());
                  delete this; 
                  delete msg;
               }
               else
               {
                  DebugLog (<< "Received ACK in Completed (unreliable) - confirmed, start Timer I");
                  mState = Confirmed;
                  mStack.mTimers.add(Timer::TimerI, msg->getTransactionId(), Timer::T4 );
                  delete msg;
               }
            }
            else
            {
               DebugLog (<< "Ignore ACK not in Completed state");
               delete msg;
            }
            break;

         case CANCEL:
            DebugLog (<< "Received Cancel, create Cancel transaction and process as server non-invite and send to TU");
	    if (!mCancelStateMachine)
	    {
               // new TransactionState(mStack, ServerNonInvite, Trying);
	       mCancelStateMachine = TransactionState::makeCancelTransaction(this, ServerNonInvite);
	    }
            // Copy the message so that it will still be valid for sendToTU().
            mCancelStateMachine->processServerNonInvite(new SipMessage(*sip));
            sendToTU(msg); // don't delete msg
            break;

         default:
            DebugLog (<< "Received unexpected request. Ignoring message");
            delete msg;
            break;
      }
   }
   else if (isResponse(msg, 100, 699) && isFromTU(msg))
   {
      SipMessage* sip = dynamic_cast<SipMessage*>(msg);
      int code = sip->header(h_StatusLine).responseCode();
      switch (sip->header(h_CSeq).method())
      {
         case INVITE:
            if (code == 100)
            {
               if (mState == Trying || mState == Proceeding)
               {
                  DebugLog (<< "Received 100 in Trying or Proceeding. Send over wire");
                  delete mMsgToRetransmit; // may be replacing the 100
                  mMsgToRetransmit = sip;
                  mState = Proceeding;
                  sendToWire(msg); // don't delete msg
               }
               else
               {
                  DebugLog (<< "Ignoring 100 - not in Trying or Proceeding.");
                  delete msg;
               }
            }
            else if (code > 100 && code < 200)
            {
               if (mState == Trying || mState == Proceeding)
               {
                  DebugLog (<< "Received 1xx in Trying or Proceeding. Send over wire");
                  delete mMsgToRetransmit; // may be replacing the 100
                  mMsgToRetransmit = sip;
                  mState = Proceeding;
                  sendToWire(msg); // don't delete msg
               }
               else
               {
                  DebugLog (<< "Received 100 when not in Trying State. Ignoring");
                  delete msg;
               }
            }
            else if (code >= 200 && code < 300)
            {
               if (mState == Trying || mState == Proceeding)
               {
                  DebugLog (<< "Received 2xx when in Trying or Proceeding State. Start Stale Timer, move to terminated.");
                  
                  terminateServerTransaction(msg->getTransactionId());
                  sendToWire(msg); // don't delete
                  delete this; // !jf! don't delete since still need confirmation
               }
               else
               {
                  DebugLog (<< "Received 2xx when not in Trying or Proceeding State. Ignoring");
                  delete msg;
               }
            }
            else if (code >= 300)
            {
               /*
                 While in the "Proceeding" state, if the TU passes a response with
                 status code from 300 to 699 to the server transaction, For unreliable 
                 transports,timer G is set to fire in T1 seconds, and is not set to 
                 fire for reliable transports.when the "Completed" state is entered, 
                 timer H MUST be set to fire in 64*T1 seconds for all transports.  
                 Timer H determines when the server transaction abandons retransmitting 
                 the response
	       */

               if (mState == Trying || mState == Proceeding)
               {
                  DebugLog (<< "Received failed response in Trying or Proceeding. Start Timer H, move to completed.");
                  delete mMsgToRetransmit; 
                  mMsgToRetransmit = sip; 
                  mState = Completed;
                  if (sip->header(h_To).exists(p_tag))
                  {
                     mToTag = sip->header(h_To).param(p_tag);
                  }
                  mStack.mTimers.add(Timer::TimerH, msg->getTransactionId(), 64*Timer::T1 );
                  if (!mIsReliable)
                  {
                     mStack.mTimers.add(Timer::TimerG, msg->getTransactionId(), Timer::T1 );
                  }
                  sendToWire(msg); // don't delete msg
               }
               else
               {
                  DebugLog (<< "Received Final response when not in Trying or Proceeding State. Ignoring");
                  delete msg;
               }
            }
            else
            {
               DebugLog (<< "Received Invalid response line. Ignoring");
               delete msg;
            }
            break;
            
         case CANCEL:
            DebugLog (<< "Received response to Cancel, process as server non-invite if we are expecting this");
            
	    if (!mCancelStateMachine)
	    {
	       // mCancelStateMachine should already exist by this point but
	       // we shouldn't assert or this creates a DoS.
	       delete msg;
	       break;
	    }
            mCancelStateMachine->processServerNonInvite(new SipMessage(*sip));
            sendToTU(msg); // don't delete
            break;
            
         default:
            DebugLog (<< "Received response to non invite or cancel. Ignoring");
            delete msg;
            break;
      }
   }
   else if (isTimer(msg))
   {
      TimerMessage* timer = dynamic_cast<TimerMessage*>(msg);
      switch (timer->getType())
      {
         case Timer::TimerG:
            if (mState == Completed)
            {
               DebugLog (<< "TimerG fired. retransmit, and readd TimerG");
               resendToWire(mMsgToRetransmit);
               mStack.mTimers.add(Timer::TimerG, msg->getTransactionId(), timer->getDuration()*2 );
            }
            else
            {
               delete msg;
            }
            break;
	    /*
	      If timer H fires while in the "Completed" state, it implies that the
	      ACK was never received.  In this case, the server transaction MUST
	      transition to the "Terminated" state, and MUST indicate to the TU
	      that a transaction failure has occurred. WHY we need to inform TU
              for Failure cases ACK ? do we really need to do this ???       
            */
         case Timer::TimerH:
	 case Timer::TimerI:
            DebugLog (<< "TimerH or TimerI fired. Delete this");
            terminateServerTransaction(msg->getTransactionId());
            delete this;
            delete msg;
            break;
            
         case Timer::TimerJ:
            DebugLog (<< "TimerJ fired. Delete state of cancel");
            mCancelStateMachine = 0;
            delete mCancelStateMachine;
            delete msg;
            break;
            
         case Timer::TimerTrying:
            if (mState == Trying)
            {
               DebugLog (<< "TimerTrying fired. Send a 100");
               sendToWire(mMsgToRetransmit); // will get deleted when this is deleted
	       mState = Proceeding;
               delete msg;
            }
            else
            {
               DebugLog (<< "TimerTrying fired. Not in Trying state. Ignoring");
               delete msg;
            }
            break;
            
         default:
            assert(0); // programming error if any other timer fires
            break;
      }
   }
   else if (isDns(msg))
   {
      processDns(msg);
   }
   else
   {
      DebugLog (<< "TransactionState::processServerInvite: message unhandled");
      delete msg;
   }
}


void
TransactionState::processStale(  Message* msg )
{
   DebugLog (<< "TransactionState::processStale: " << msg->brief());

   SipMessage* sip = dynamic_cast<SipMessage*>(msg);
   if ( (sip &&
	 sip->isRequest() &&
	 sip->header(h_RequestLine).getMethod() == ACK ) ||
        isResponse(msg, 200, 299 ) )
   {
      if (isFromTU(msg))
      { 
	 mMsgToRetransmit = sip;
	 sendToWire(sip);
      }
      else if (isFromWire(msg))
      {
	 sendToTU(msg);
      }
      else 
      {
	 // !rk! what about 3xx - 6xx from processServerInvite?
	 delete msg;
      }
   }
   else if (isTimer(msg))
   {
      DebugLog (<< "received timer in stale transaction");
      
      TimerMessage* timer = dynamic_cast<TimerMessage*>(msg);
      switch (timer->getType())
      {
	 case Timer::TimerStaleServer:
            assert(0);
            terminateServerTransaction(msg->getTransactionId());
	    delete this;
	    delete msg;
	    break;
	    
	 case Timer::TimerStaleClient:
            assert(0);
            terminateClientTransaction(msg->getTransactionId());
	    delete this;
	    delete msg;
	    break;

	 default:
	    DebugLog (<< "ignoring timer " << timer->brief());
	    delete msg;
	    break;
      }
   }
   else if (isDns(msg) || isTransportError(msg))
   {
      // resend with a different tuple or delete this if no more tuples
      processDns(msg);
   }
   else
   {
      DebugLog (<< "TransactionState::processStale: message unhandled");
      delete msg;
   }
}

bool
TransactionState::isRequest(Message* msg) const
{
   SipMessage* sip = dynamic_cast<SipMessage*>(msg);   
   return sip && sip->isRequest();
}

bool
TransactionState::isInvite(Message* msg) const
{
   if (isRequest(msg))
   {
      SipMessage* sip = dynamic_cast<SipMessage*>(msg);
      return (sip->header(h_RequestLine).getMethod()) == INVITE;
   }
   return false;
}

bool
TransactionState::isResponse(Message* msg, int lower, int upper) const
{
   SipMessage* sip = dynamic_cast<SipMessage*>(msg);
   if (sip && sip->isResponse())
   {
      int c = sip->header(h_StatusLine).responseCode();
      return (c >= lower && c <= upper);
   }
   return false;
}

bool
TransactionState::isTimer(Message* msg) const
{
   return dynamic_cast<TimerMessage*>(msg) != 0;
}

bool
TransactionState::isDns(Message* msg) const
{
   return dynamic_cast<DnsResolver::DnsMessage*>(msg) != 0;
}

bool
TransactionState::isFromTU(Message* msg) const
{
   SipMessage* sip = dynamic_cast<SipMessage*>(msg);
   return sip && !sip->isExternal();
}

bool
TransactionState::isFromWire(Message* msg) const
{
   SipMessage* sip = dynamic_cast<SipMessage*>(msg);
   return sip && sip->isExternal();
}

bool
TransactionState::isTransportError(Message* msg) const
{
   TransportMessage* t = dynamic_cast<TransportMessage*>(msg);
   return (t && t->isFailed());
}

bool
TransactionState::isReliabilityIndication(Message* msg) const
{
   return ( dynamic_cast<ReliabilityMessage*>(msg) != 0);
}

bool
TransactionState::isSentReliable(Message* msg) const
{
   ReliabilityMessage* r = dynamic_cast<ReliabilityMessage*>(msg);
   return (r && r->isReliable());
}

bool
TransactionState::isSentUnreliable(Message* msg) const
{
   ReliabilityMessage* r = dynamic_cast<ReliabilityMessage*>(msg);
   return (r && !r->isReliable());
}

const Data&
TransactionState::tid(SipMessage* sip) const
{
   assert((mMachine == Stateless && !mId.empty()) || sip);
   return (mId.empty() && sip) ? sip->getTransactionId() : mId;
}

void
TransactionState::sendToWire(Message* msg) 
{
   SipMessage* sip=dynamic_cast<SipMessage*>(msg);
   assert(sip);

   if (mDnsState == NotStarted)
   {
      DebugLog(<< "TransactionState::sendToWire pausing for DNS lookup");
      mDnsState = Waiting;
      mStack.mTransportSelector.dnsResolve(sip, tid(sip));
   }
   else if (mDnsState == NoLookupRequired)
   {
      assert(sip->isResponse());
      mStack.mTransportSelector.send(sip, mSource, tid(sip));
   }
   else if (mDnsState == Complete)
   {
      assert (mCurrent != mTuples.end());
      mStack.mTransportSelector.send(sip, *mCurrent, tid(sip));	  
   }
}

void
TransactionState::resendToWire(Message* msg) const
{
   SipMessage* sip=dynamic_cast<SipMessage*>(msg);
   assert(sip);

   assert (mDnsState != NotStarted); 
   assert (mDnsState != Waiting);  // !jf! can this happen? 
   
   if (mDnsState == NoLookupRequired)
   {
      assert(sip->isResponse());
      mStack.mTransportSelector.send(sip, mSource, tid(sip));
   }
   else
   {
      assert (mCurrent != mTuples.end());
      mStack.mTransportSelector.send(sip, *mCurrent, tid(sip), true);
   }
}

void
TransactionState::sendToTU(Message* msg) const
{
   SipMessage* sip=dynamic_cast<SipMessage*>(msg);
   DebugLog(<< "Send to TU: " << *msg);
   assert(sip);
   mStack.mTUFifo.add(sip);
}

SipMessage*
TransactionState::make100(SipMessage* request) const
{
   SipMessage* sip=Helper::makeResponse(*request, 100);
   return sip;
}

void
TransactionState::terminateClientTransaction(const Data& tid)
{
   mState = Terminated;
   if (mStack.mRegisteredForTransactionTermination)
   {
      DebugLog (<< "Terminate transaction " << tid);
      mStack.mTUFifo.add(new TransactionTerminated(tid));
   }
}

void
TransactionState::terminateServerTransaction(const Data& tid)
{
   mState = Terminated;
   if (mStack.mRegisteredForTransactionTermination)
   {
      DebugLog (<< "Terminate transaction " << tid);
      mStack.mTUFifo.add(new TransactionTerminated(tid));
   }
}


std::ostream& 
Vocal2::operator<<(std::ostream& strm, const Vocal2::TransactionState& state)
{
   strm << "Tstate[ mMach=";
   switch (state.mMachine)
   {
      case TransactionState::ClientNonInvite:
         strm << "ClientNonInvite";
         break;
      case TransactionState::ClientInvite:
         strm << "ClientInvite";
         break;
      case TransactionState::ServerNonInvite:
         strm << "ServerNonInvite";
         break;
      case TransactionState::ServerInvite:
         strm << "ServerInvite";
         break;
      case TransactionState::Stateless:
         strm << "Stateless";
         break;
   }
   
   strm << " state=";
   switch (state.mState)
   {
      case TransactionState::Calling:
         strm << "Calling";
         break;
      case TransactionState::Trying:
         strm << "Trying";
         break;
      case TransactionState::Proceeding:
         strm << "Proceeding";
         break;
      case TransactionState::Completed:
         strm << "Completed";
         break;
      case TransactionState::Confirmed:
         strm << "Confirmed";
         break;
      case TransactionState::Terminated:
         strm << "Terminated";
         break;
      case TransactionState::Bogus:
         strm << "Bogus";
         break;
   }

   strm << (state.mIsReliable ? " reliable" : " unreliable");
   return strm;
}

/* Local Variables: */
/* c-file-style: "ellemtel" */
/* End: */

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
