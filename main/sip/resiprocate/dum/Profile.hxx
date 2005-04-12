#if !defined(RESIP_PROFILE_HXX)
#define RESIP_PROFILE_HXX

#include <iosfwd>
#include <set>
#include "resiprocate/Headers.hxx"
#include "resiprocate/MethodTypes.hxx"

namespace resip
{

class Data;

class Profile
{
   public:        
      Profile(Profile *baseProfile = 0);  // Default to no base profile
      virtual ~Profile();

      // This default is used if no value is passed in when creating a registration
      virtual void setDefaultRegistrationTime(int secs);
      virtual int getDefaultRegistrationTime() const;

      // If a registration gets rejected with a 423, then we with the MinExpires value - if it is less than this
      // Set to 0 to disable this check and accept any time suggested by the server.
      virtual void setDefaultMaxRegistrationTime(int secs);
      virtual int getDefaultMaxRegistrationTime() const;

      // The time to retry registrations on error responses (if Retry-After header is not present in error)
      // Set to 0 to never retry on errors
      virtual void setDefaultRegistrationRetryTime(int secs);
      virtual int getDefaultRegistrationRetryTime() const;

      // This default is used if no value is passed in when creating a subscription
      virtual void setDefaultSubscriptionTime(int secs);
      virtual int getDefaultSubscriptionTime() const;

      // This default is used if no value is passed in when creating a publication
      virtual void setDefaultPublicationTime(int secs);
      virtual int getDefaultPublicationTime() const;

      /// Call is stale if UAC gets no final response within the stale call timeout (default 3 minutes)
      virtual void setDefaultStaleCallTime(int secs);
      virtual int getDefaultStaleCallTime() const;

      // Only used if timer option tag is set in MasterProfile.
      // Note:  Value must be higher than 90 (as specified in session-timer draft)
      virtual void setDefaultSessionTime(int secs); 
      virtual int getDefaultSessionTime() const;

      // Only used if timer option tag is set in MasterProfile.
      // Set to PreferLocalRefreshes if you prefer that the local UA performs the refreshes.  
      // Set to PreferRemoteRefreshes if you prefer that the remote UA peforms the refreshes.
      // Set to PreferUACRefreshes if you prefer that the UAC performs the refreshes.
      // Set to PreferUASRefreshes if you prefer that the UAS performs the refreshes.
      // Note: determining the refresher is a negotiation, so despite this setting the remote 
      // end may end up enforcing their preference.  Also if the remote end doesn't support 
      // SessionTimers then the refresher will always be local.
      // This implementation follows the RECOMMENDED practices from section 7.1 of the draft 
      // and does not specify a refresher parameter as in UAC requests.
      typedef enum
      {
         PreferLocalRefreshes,
         PreferRemoteRefreshes,
         PreferUACRefreshes,
         PreferUASRefreshes
      } SessionTimerMode;
      virtual void setDefaultSessionTimerMode(Profile::SessionTimerMode mode);
      virtual Profile::SessionTimerMode getDefaultSessionTimerMode() const;

      // The amount of time that can pass before dum will resubmit an unreliable provisional response
      virtual void set1xxRetransmissionTime(int secs);
      virtual int get1xxRetransmissionTime() const;

      //overrides the value used to populate the contact
      //?dcm? -- also change via entries? Also, dum currently uses(as a uas)
      //the request uri of the dialog constructing request for the local contact
      //within that dialog. A transport paramter here could also be used to
      //force tcp vs udp vs tls?
      virtual void setOverrideHostAndPort(const Uri& hostPort);
      virtual bool hasOverrideHostAndPort() const;
      virtual const Uri& getOverrideHostAndPort() const;      
      
      //enable/disable sending of Allow/Supported/Accept-Language/Accept-Encoding headers 
      //on initial outbound requests (ie. Initial INVITE, REGISTER, etc.) and Invite 200 responses
      //Note:  Default is to advertise Headers::Allow and Headers::Supported, use clearAdvertisedCapabilities to remove these
      //       Currently implemented header values are: Headers::Allow, 
      //       Headers::AcceptEncoding, Headers::AcceptLanguage, Headers::Supported
      virtual void addAdvertisedCapability(const Headers::Type header);
      virtual bool isAdvertisedCapability(const Headers::Type header) const;
      virtual void clearAdvertisedCapabilities();
      
      // Use to route all outbound requests through a particular proxy
      virtual void setOutboundProxy( const Uri& uri );
      virtual const NameAddr& getOutboundProxy() const;
      virtual bool hasOutboundProxy() const;
      
      //enable/disable rport for requests. rport is enabled by default
      virtual void setRportEnabled(bool enabled);
      virtual bool getRportEnabled() const;      

      //if set then UserAgent header is added to outbound messages
      virtual void setUserAgent( const Data& userAgent );
      virtual const Data& getUserAgent() const;
      virtual bool hasUserAgent() const;

      //time between CR/LF keepalive messages in seconds.  Set to 0 to disable.  Default is 30.
      //Note:  You must set a KeepAliveManager on DUM for this to work.
      virtual void setKeepAliveTime(int keepAliveTime);
      virtual int getKeepAliveTime() const;      

   private:
      // !slg! - should we provide a mechanism to clear the mHasxxxxx members to re-enable fall through after setting?
      bool mHasDefaultRegistrationExpires;
      int mDefaultRegistrationExpires;
      
      bool mHasDefaultMaxRegistrationExpires;
      int mDefaultMaxRegistrationExpires;

      bool mHasDefaultRegistrationRetryInterval;
      int  mDefaultRegistrationRetryInterval;

      bool mHasDefaultSubscriptionExpires;
      int mDefaultSubscriptionExpires;

      bool mHasDefaultPublicationExpires;
      int mDefaultPublicationExpires;

      bool mHasDefaultStaleCallTime;
      int mDefaultStaleCallTime;

      bool mHasDefaultSessionExpires;
      int mDefaultSessionExpires;

      bool mHasDefaultSessionTimerMode;
      SessionTimerMode mDefaultSessionTimerMode;

      bool mHas1xxRetransmissionTime;
      int m1xxRetransmissionTime;

      bool mHasOutboundProxy;
      NameAddr mOutboundProxy;
      
      bool mHasAdvertisedCapabilities;
      std::set<Headers::Type> mAdvertisedCapabilities;
      
      bool mHasRportEnabled;
      bool mRportEnabled;
      
      bool mHasUserAgent;            
      Data mUserAgent;
      
      bool mHasOverrideHostPort;
      Uri  mOverrideHostPort;
      
      bool mHasKeepAliveTime;
      int  mKeepAliveTime;
      
      Profile *mBaseProfile;  // All non-set settings will fall through to this Profile (if set)
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
