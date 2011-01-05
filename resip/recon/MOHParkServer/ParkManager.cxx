#include "AppSubsystem.hxx"
#include "ParkManager.hxx"
#include "ParkOrbit.hxx"
#include "Server.hxx"
#include "../UserAgent.hxx"

#include <resip/stack/ExtensionParameter.hxx>
#include <resip/stack/SipMessage.hxx>
#include <rutil/Log.hxx>
#include <rutil/Logger.hxx>
#include <rutil/WinLeakCheck.hxx>

using namespace recon;
using namespace resip;
using namespace std;

#define RESIPROCATE_SUBSYSTEM AppSubsystem::MOHPARKSERVER

static const resip::ExtensionParameter p_orbit("orbit");
static const resip::ExtensionParameter p_automaton("automaton");
static const resip::ExtensionParameter p_byeless("+sip.byeless");
static const resip::ExtensionParameter p_rendering("+sip.rendering");

namespace mohparkserver 
{

ParkManager::ParkManager(Server& server) :
   mServer(server)
{
    // Initialize free orbit list
   for(unsigned long orbit = mServer.mParkOrbitRangeStart; orbit < mServer.mParkOrbitRangeStart + mServer.mParkNumOrbits; orbit++)
   {
      mFreeOrbitList.push_back(orbit);
   }
}

ParkManager::~ParkManager()
{
}

void
ParkManager::startup()
{
   // Setup Park ConversationProfile
   SharedPtr<ConversationProfile> parkConversationProfile = SharedPtr<ConversationProfile>(new ConversationProfile(mServer.mUserAgentMasterProfile));
   parkConversationProfile->setDefaultRegistrationTime(mServer.mParkRegistrationTime);  
   parkConversationProfile->setDefaultRegistrationRetryTime(120);  // 2 mins
   parkConversationProfile->setDefaultFrom(mServer.mParkUri);
   parkConversationProfile->setDigestCredential(mServer.mParkUri.uri().host(), mServer.mParkUri.uri().user(), mServer.mParkPassword);  
   parkConversationProfile->challengeOODReferRequests() = false;
   parkConversationProfile->setExtraHeadersInReferNotifySipFragEnabled(true);  // Enable dialog identifying headers in SipFrag bodies of Refer Notifies
   NameAddr capabilities;
   capabilities.param(p_automaton);
   capabilities.param(p_byeless);
   capabilities.param(p_rendering) = "\"no\"";
   parkConversationProfile->setUserAgentCapabilities(capabilities);  // Same as above
   parkConversationProfile->natTraversalMode() = ConversationProfile::NoNatTraversal;
   parkConversationProfile->secureMediaMode() = ConversationProfile::NoSecureMedia;
   mServer.buildSessionCapabilities(parkConversationProfile->sessionCaps());
   mConversationProfile = parkConversationProfile;
   mServer.mUserAgent->addConversationProfile(mConversationProfile);

   // Create Orbit Profiles
   for(unsigned long orbit = mServer.mParkOrbitRangeStart; orbit < mServer.mParkOrbitRangeStart + mServer.mParkNumOrbits; orbit++)
   {
       SharedPtr<ConversationProfile> orbitConversationProfile = SharedPtr<ConversationProfile>(new ConversationProfile(mConversationProfile));
       Data orbitData(orbit);
       orbitConversationProfile->setDefaultRegistrationTime(mServer.mParkOrbitRegistrationTime);  
       orbitConversationProfile->setDefaultRegistrationRetryTime(120);  // 2 mins
       orbitConversationProfile->setDefaultFrom(mServer.mParkUri);
       orbitConversationProfile->getDefaultFrom().uri().user() = orbitData;
       orbitConversationProfile->setDigestCredential(mServer.mParkUri.uri().host(), orbitData, mServer.mParkOrbitPassword);  
       orbitConversationProfile->challengeOODReferRequests() = false;
       orbitConversationProfile->setExtraHeadersInReferNotifySipFragEnabled(true);  // Enable dialog identifying headers in SipFrag bodies of Refer Notifies
       orbitConversationProfile->setUserAgentCapabilities(capabilities);  // Same as above
       orbitConversationProfile->natTraversalMode() = ConversationProfile::NoNatTraversal;
       orbitConversationProfile->secureMediaMode() = ConversationProfile::NoSecureMedia;
       mServer.buildSessionCapabilities(orbitConversationProfile->sessionCaps());
       mServer.mUserAgent->addConversationProfile(orbitConversationProfile);
       mOrbitProfiles[orbit] = orbitConversationProfile;
   }
}

void 
ParkManager::shutdown()
{
   OrbitMap::iterator it = mOrbits.begin();
   for(;it!=mOrbits.end();it++)
   {
       // Delete all Orbit objects
       delete it->second;
   }
   mOrbits.clear();

   // Destroy main Park profile
   if(mConversationProfile)
   {
       mServer.mUserAgent->destroyConversationProfile(mConversationProfile->getHandle());
       mConversationProfile.reset();
       assert(!mConversationProfile);
   }

   // Destroy Orbit Profiles
   OrbitProfileMap::iterator itOrbit = mOrbitProfiles.begin();
   for(;itOrbit!=mOrbitProfiles.end();itOrbit++)
   {
       mServer.mUserAgent->destroyConversationProfile(itOrbit->second->getHandle());
   }
   mOrbitProfiles.clear();
}

bool 
ParkManager::isMyProfile(recon::ConversationProfile& profile)
{
    if(profile.getHandle() == mConversationProfile->getHandle())
    {
        return true;
    }
    else
    {
        // check orbit profiles
        OrbitProfileMap::iterator it = mOrbitProfiles.begin();
        for(;it!=mOrbitProfiles.end();it++)
        {
            if(profile.getHandle() == it->second->getHandle())
            {
                return true;
            }
        }
    }
    return false;
}

void 
ParkManager::parkParticipant(ParticipantHandle participantHandle, const SipMessage& msg)
{
    unsigned long orbit = 0;

    // Check if Orbit parameter has been specified on the To header
    if(msg.header(h_To).uri().exists(p_orbit))
    {
        orbit = msg.header(h_To).uri().param(p_orbit).convertUnsignedLong();
    }

    if((orbit >= mServer.mParkOrbitRangeStart) && 
       (orbit < (mServer.mParkOrbitRangeStart + mServer.mParkNumOrbits)))
    {
        // Park call at specified orbit

        // Check if Orbit is created or not yet
        OrbitMap::iterator it = mOrbits.find(orbit);
        ParkOrbit* parkOrbit = 0;
        if(it == mOrbits.end())
        {
            // Create Orbit
            parkOrbit = new ParkOrbit(mServer, orbit);
            mOrbits[orbit] = parkOrbit;

            // Remove from free list
            std::deque<unsigned long>::iterator itFree = mFreeOrbitList.begin();
            for(;itFree!=mFreeOrbitList.end();itFree++)
            {
               if(orbit == *itFree)
               {
                   mFreeOrbitList.erase(itFree);
                   break;
               }
            }
        }
        else
        {
            // Use existing
            parkOrbit = it->second;
        }

        assert(parkOrbit);
        if(parkOrbit->addParticipant(participantHandle))
        {
           InfoLog(<< "ParkManager::parkParticipant valid orbit specified (orbit=" << orbit << "), call parked");
        }
    }
    else
    {
        // If no orbit was specified, or specified number is bad - select a free orbit, and redirect request to use newly allocated orbit
        if(mFreeOrbitList.size() > 0)
        {
           unsigned long freeorbit = mFreeOrbitList.front();
           // Move free item to end of list, to reduce chance it will be double allocated
           mFreeOrbitList.pop_front();  
           mFreeOrbitList.push_back(freeorbit);
           InfoLog(<< "ParkManager::parkParticipant no valid orbit specified (orbit=" << orbit << ") redirecting to free orbit=" << freeorbit);
           NameAddr destination(mServer.mParkUri);
           destination.uri().param(p_orbit) = Data(freeorbit);
           mServer.redirectParticipant(participantHandle, destination);
        }
        else
        {
           // No free orbits
           WarningLog(<< "ParkManager::parkParticipant no free orbits available, rejecing with 486 busy.");
           mServer.rejectParticipant(participantHandle, 486 /* Busy */);
        }
    }
}

void 
ParkManager::incomingParticipant(ParticipantHandle participantHandle, const SipMessage& msg)
{
   // Check if To user is an orbit number
   unsigned long orbit = msg.header(h_To).uri().user().convertUnsignedLong();
   if((orbit >= mServer.mParkOrbitRangeStart) && 
      (orbit < (mServer.mParkOrbitRangeStart + mServer.mParkNumOrbits)))
   {
       // Orbit is valid - see if we have a call parked there
       OrbitMap::iterator it = mOrbits.find(orbit);
       ParticipantHandle participantToRetrieve = 0;
       if(it!=mOrbits.end() && (participantToRetrieve = it->second->getNextQueuedParticipant()) != 0)
       {
           // Answer incoming call and then immediately redirect to parked call
           mServer.addParticipant(it->second->getConversationHandle(), participantHandle);
           mServer.modifyParticipantContribution(it->second->getConversationHandle(), participantHandle, 0, 0 /* Mute participant */);
           mServer.answerParticipant(participantHandle);
           mServer.redirectToParticipant(participantHandle, participantToRetrieve);

           InfoLog(<< "ParkManager::incomingParticipant retrieving participant " << participantToRetrieve << " from orbit " << orbit);
       }
       else
       {
          WarningLog(<< "ParkManager::incomingParticipant orbit " << orbit << " has no call to retrieve, rejecting with 404.");
          mServer.rejectParticipant(participantHandle, 404 /* Not Found */);
       }
   }
   else
   {
      WarningLog(<< "ParkManager::incomingParticipant orbit not found in To user (" << msg.header(h_To).uri().user() << "), rejecting with 404.");
      mServer.rejectParticipant(participantHandle, 404 /* Not Found */);
   }
}

bool 
ParkManager::removeParticipant(ParticipantHandle participantHandle)
{
   OrbitMap::iterator it = mOrbits.begin();
   for(;it!=mOrbits.end();it++)
   {
       if(it->second->removeParticipant(participantHandle))
       {
           if(it->second->getNumParticipants() == 0)
           {
               // Last participant just left orbit - destroy it
               mFreeOrbitList.push_back(it->second->getOrbit()); 
               delete it->second;
               mOrbits.erase(it);
           }
           return true;
       }
   }
   return false;
}


}

/* ====================================================================

 Copyright (c) 2011, SIP Spectrum, Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are 
 met:

 1. Redistributions of source code must retain the above copyright 
    notice, this list of conditions and the following disclaimer. 

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution. 

 3. Neither the name of SIP Spectrum nor the names of its contributors 
    may be used to endorse or promote products derived from this 
    software without specific prior written permission. 

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ==================================================================== */

