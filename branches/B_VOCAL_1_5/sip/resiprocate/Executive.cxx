#include "sip2/sipstack/Executive.hxx"
#include "sip2/sipstack/SipStack.hxx"
#include "sip2/sipstack/TransactionState.hxx"
#include "sip2/util/Logger.hxx"

using namespace Vocal2;
#define VOCAL_SUBSYSTEM Subsystem::SIP

Executive::Executive( SipStack& stack)
  : mStack(stack)
{

}


void
Executive::process(FdSet& fdset)
{
   processTransports(fdset);
   processTimer();
   processDns(fdset);
   
   while( processStateMachine() );
}

 
bool 
Executive::processTransports(FdSet& fdset) 
{
   mStack.mTransportSelector.process(fdset);
   return false;
}


bool 
Executive::processStateMachine()
{
   if ( mStack.mStateMacFifo.size() == 0 ) 
   {
      return false;
   }
   
   TransactionState::process(mStack);
   
   return true;
}

bool 
Executive::processTimer()
{
   mStack.mTimers.process();
   return false;
}

bool
Executive::processDns(FdSet& fdset)
{
   mStack.mDnsResolver.process(fdset);
   return false;
}


/// returns time in milliseconds when process next needs to be called 
int 
Executive::getTimeTillNextProcessMS()
{
   if ( mStack.mStateMacFifo.size() != 0 ) 
   {
      return 0;
   }

   if ( mStack.mTransportSelector.hasDataToSend() )
   {
      return 0;
   }
   
   int ret = mStack.mTimers.msTillNextTimer();

#if 1 // !cj! just keep a max of 500ms for good luck - should not be needed   
   if ( ret > 500 )
   {
      ret = 500;
   }
#endif

   return ret;
} 


void 
Executive::buildFdSet( FdSet& fdset)
{
   mStack.mTransportSelector.buildFdSet( fdset );
   mStack.mDnsResolver.buildFdSet( fdset );
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
