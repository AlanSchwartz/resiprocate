#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include "resiprocate/SipMessage.hxx"
#include "resiprocate/ApplicationMessage.hxx"
#include "resiprocate/ShutdownMessage.hxx"
#include "resiprocate/TransactionController.hxx"
#include "resiprocate/TransactionState.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/StatisticsManager.hxx"

using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::TRANSACTION

static unsigned int MaxTUFifoSize = 1000;

#if defined(WIN32)
#pragma warning( disable : 4355 ) // using this in base member initializer list 
#endif


TransactionController::TransactionController(bool multi, 
                                             Fifo<Message>& tufifo, 
                                             bool stateless) : 
   mStateless(stateless),
   mRegisteredForTransactionTermination(false),
   mDiscardStrayResponses(true),
   mTUFifo(tufifo),
   mTransportSelector(multi, mStateMacFifo),
   mStatelessHandler(*this),
   mTimers(mStateMacFifo, mTUFifo),
   StatelessIdCounter(1),
   mShuttingDown(false),
   mStatsManager(0)
{
   RESIP_STATISTICS(mStatsManager = new StatisticsManager(*this));
}

#if defined(WIN32)
#pragma warning( default : 4355 )
#endif

TransactionController::~TransactionController()
{
   delete mStatsManager;
}


bool 
TransactionController::isTUOverloaded() const
{
   return (MaxTUFifoSize && mTUFifo.size() > MaxTUFifoSize);
}

void
TransactionController::shutdown()
{
   mShuttingDown = true;
   mTransportSelector.shutdown();
}

void
TransactionController::process(FdSet& fdset)
{
   if (mShuttingDown && 
       //mTimers.empty() && 
       !mStateMacFifo.messageAvailable() && 
       !mTUFifo.messageAvailable() &&
       mTransportSelector.isFinished())
   {
      mTUFifo.add(new ShutdownMessage);
   }
   else
   {
      mTransportSelector.process(fdset);
      mTimers.process();

      while (mStateMacFifo.messageAvailable())
      {
         if (mStateless)
         {
            mStatelessHandler.process();
         }
         else
         {
            TransactionState::process(*this);
         }
      }
   }
}

unsigned int 
TransactionController::getTimeTillNextProcessMS()
{
   if ( mStateMacFifo.messageAvailable() ) 
   {
      return 0;
   }
   else if ( mTransportSelector.hasDataToSend() )
   {
      return 0;
   }
   
   int ret = mTimers.msTillNextTimer();

   return resipMin(25, ret);
} 
   
void 
TransactionController::buildFdSet( FdSet& fdset)
{
   mTransportSelector.buildFdSet( fdset );
}

bool
TransactionController::addTransport( TransportType protocol, 
                                     int port,
                                     IpVersion version,
                                     const Data& ipInterface)
{
   return mTransportSelector.addTransport(protocol, port, version, ipInterface);
}

bool
TransactionController::addTlsTransport( int port, 
                                        const Data& keyDir,
                                        const Data& privateKeyPassPhrase,
                                        const Data& domainname,
                                        IpVersion version,
                                        const Data& ipInterface,
                                        SecurityTypes::SSLType sslType
                                        )
{
   return mTransportSelector.addTlsTransport(domainname, keyDir, privateKeyPassPhrase, 
                                             port, version, ipInterface, sslType);
}

bool
TransactionController::addTlsTransport( int port, 
                                        const Data& domainname,
					Security& security,
                                        IpVersion version,
                                        const Data& ipInterface
                                        )
{
    return mTransportSelector.addTlsTransport(domainname, security, port,
					      version, ipInterface);
}

void
TransactionController::send(SipMessage* msg)
{
   mStateMacFifo.add(msg);
}

void
TransactionController::post(const ApplicationMessage& message)
{
   assert(!mShuttingDown);
   Message* toPost = message.clone();
   mTUFifo.add(toPost);
}

void
TransactionController::post(const ApplicationMessage& message,
                            unsigned int ms)
{
   assert(!mShuttingDown);
   Message* toPost = message.clone();
   mTimers.add(Timer(ms, toPost));
}

void
TransactionController::registerForTransactionTermination()
{
   mRegisteredForTransactionTermination = true;
}

unsigned int 
TransactionController::getTuFifoSize() const
{
   return mTUFifo.size();
}

unsigned int 
TransactionController::sumTransportFifoSizes() const
{
   return mTransportSelector.sumTransportFifoSizes();
}

unsigned int 
TransactionController::getTransactionFifoSize() const
{
   return mStateMacFifo.size();
}



unsigned int 
TransactionController::getNumClientTransactions() const
{
   return mClientTransactionMap.size();
}

unsigned int 
TransactionController::getNumServerTransactions() const
{
   return mServerTransactionMap.size();
}

unsigned int 
TransactionController::getTimerQueueSize() const
{
   return mTimers.size();
}

StatisticsManager&
TransactionController::getStatisticsManager() const
{
   assert(mStatsManager);
   return *mStatsManager;
}

/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2004 Vovida Networks, Inc.  All rights reserved.
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
