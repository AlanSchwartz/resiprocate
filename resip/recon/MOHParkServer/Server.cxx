#ifdef WIN32
#include <process.h>
#else
#include <spawn.h>
#endif

#include "Server.hxx"
#include "../UserAgent.hxx"
#include "AppSubsystem.hxx"

#include <resip/stack/Tuple.hxx>
#include <rutil/DnsUtil.hxx>
#include <rutil/Log.hxx>
#include <rutil/Logger.hxx>
#include <rutil/ThreadIf.hxx>
#include <rutil/WinLeakCheck.hxx>

// sipX includes
#include <os/OsSysLog.h>

using namespace recon;
using namespace resip;
using namespace std;

#define RESIPROCATE_SUBSYSTEM AppSubsystem::MOHPARKSERVER

namespace mohparkserver
{
class MyUserAgent : public UserAgent
{
public:
   MyUserAgent(ConversationManager* conversationManager, SharedPtr<UserAgentMasterProfile> profile) :
      UserAgent(conversationManager, profile) {}

   virtual void onApplicationTimer(unsigned int id, unsigned int durationMs, unsigned int seq)
   {
      InfoLog(<< "onApplicationTimeout: id=" << id << " dur=" << durationMs << " seq=" << seq);
   }

   virtual void onSubscriptionTerminated(SubscriptionHandle handle, unsigned int statusCode)
   {
      InfoLog(<< "onSubscriptionTerminated: handle=" << handle << " statusCode=" << statusCode);
   }

   virtual void onSubscriptionNotify(SubscriptionHandle handle, Data& notifyData)
   {
      InfoLog(<< "onSubscriptionNotify: handle=" << handle << " data=" << endl << notifyData);
   }
};

class MOHParkServerLogger : public ExternalLogger
{
public:
   virtual ~MOHParkServerLogger() {}
   /** return true to also do default logging, false to supress default logging. */
   virtual bool operator()(Log::Level level,
                           const Subsystem& subsystem, 
                           const Data& appName,
                           const char* file,
                           int line,
                           const Data& message,
                           const Data& messageWithHeaders)
   {
      // Log any warnings/errors to the screen and all MOHParkServer logging messages
      if(level <= Log::Warning || subsystem.getSubsystem() == AppSubsystem::MOHPARKSERVER.getSubsystem())
      {
         resipCout << messageWithHeaders << endl;
      }
      return true;
   }
};
MOHParkServerLogger g_MOHParkServerLogger;

Server::Server(int argc, char** argv) : 
   ConfigParser(argc, argv),
   ConversationManager(false /* local audio? */, ConversationManager::sipXConversationMediaInterfaceMode),
   mIsV6Avail(false),
   mUserAgent(0),
   mMOHManager(*this),
   mParkManager(*this)
{
   // Initialize loggers
   GenericLogImpl::MaxByteCount = mLogFileMaxBytes; 
   Log::initialize("file", mLogLevel, "", mLogFilename.c_str(), &g_MOHParkServerLogger);
   if(!mSipXLogFilename.empty())
   {
      //enableConsoleOutput(TRUE);  // Allow sipX console output
      OsSysLog::initialize(0, "MOHParkServer");
      OsSysLog::setOutputFile(0, mSipXLogFilename.c_str()) ;
   }

   InfoLog( << "MOHParkServer settings:");
   InfoLog( << "  MOH URI = " << mMOHUri);
   InfoLog( << "  MOH Registration Time = " << mMOHRegistrationTime);
   InfoLog( << "  MOH Filename URL = " << mMOHFilenameUrl);
   InfoLog( << "  Park URI = " << mParkUri);
   InfoLog( << "  Park Registration Time = " << mParkRegistrationTime);
   InfoLog( << "  Park MOH Filename URL = " << mParkMOHFilenameUrl);
   InfoLog( << "  Park Orbit Range Start = " << mParkOrbitRangeStart);
   InfoLog( << "  Park Number of Orbits = " << mParkNumOrbits);
   InfoLog( << "  Park Orbit Registration Time = " << mParkOrbitRegistrationTime);
   InfoLog( << "  Local IP Address = " << mAddress);
   InfoLog( << "  Override DNS Servers = " << mDnsServers);
   InfoLog( << "  UDP Port = " << mUdpPort);
   InfoLog( << "  TCP Port = " << mTcpPort);
   InfoLog( << "  TLS Port = " << mTlsPort);
   InfoLog( << "  TLS Domain = " << mTlsDomain);
   InfoLog( << "  Keepalives = " << (mKeepAlives ? "enabled" : "disabled"));
   InfoLog( << "  Outbound Proxy = " << mOutboundProxy);
   InfoLog( << "  Media Port Range Start = " << mMediaPortRangeStart);
   InfoLog( << "  Media Port Range Size = " << mMediaPortRangeSize);
   InfoLog( << "  Log Level = " << mLogLevel);

   resip::Data ;

   if(!mAddress.empty())
   {
      // If address is specified in config file, then just use this address only
      Tuple myTuple(mAddress, mUdpPort, UDP);
      if(myTuple.ipVersion() == V6)
      {
         mIsV6Avail = true;
      }
   }
   else
   {
      // If address in config is empty - query network interface info
      std::list<std::pair<Data,Data> > interfaces = DnsUtil::getInterfaces();
      std::list<std::pair<Data,Data> >::iterator itInt = interfaces.begin();
      for(;itInt != interfaces.end(); itInt++)
      {
         Tuple myTuple(itInt->second, mUdpPort, UDP);
         if(myTuple.ipVersion() == V6)
         {
            mIsV6Avail = true;
         }
      }
   }

   //////////////////////////////////////////////////////////////////////////////
   // Setup UserAgentMasterProfile
   //////////////////////////////////////////////////////////////////////////////

   SharedPtr<UserAgentMasterProfile> profile(new UserAgentMasterProfile);

   // Add transports
   try
   {
      if(mUdpPort == (unsigned short)-1 
#ifdef USE_SSL
         && mTlsPort == (unsigned short)-1 
#endif
         && mTcpPort == (unsigned short)-1)
      {
         // Ensure there is at least one transport enabled - if all are disabled, then enable UDP on an OS selected port
         mUdpPort = 0;
      }
      if(mUdpPort != (unsigned short)-1)
      {
         profile->addTransport(UDP, mUdpPort, V4, mAddress);
         if(mIsV6Avail)
         {
            profile->addTransport(UDP, mUdpPort, V6, mAddress);
         }
      }
      if(mTcpPort != (unsigned short)-1)
      {
         profile->addTransport(TCP, mTcpPort, V4, mAddress);
         if(mIsV6Avail)
         {
            profile->addTransport(TCP, mTcpPort, V6, mAddress);
         }
      }
#ifdef USE_SSL
      if(mTlsPort != (unsigned short)-1)
      {
         profile->addTransport(TLS, mTlsPort, V4, mAddress, mTlsDomain);
         if(mIsV6Avail)
         {
            profile->addTransport(TLS, mTlsPort, V6, mAddress, mTlsDomain);
         }
      }
#endif
   }
   catch (BaseException& e)
   {
      std::cerr << "Cannot start a transport, likely a port is already in use" << endl;
      InfoLog (<< "Caught: " << e);
      exit(-1);
   }

   // DNS Servers
   ParseBuffer pb(mDnsServers);
   Data dnsServer;
   while(!mDnsServers.empty() && !pb.eof())
   {
      pb.skipWhitespace();
      const char *start = pb.position();
      pb.skipToOneOf(ParseBuffer::Whitespace, ";,");  // allow white space 
      pb.data(dnsServer, start);
      if(DnsUtil::isIpV4Address(dnsServer))
      {
         InfoLog( << "Adding DNS Server: " << dnsServer);
         profile->addAdditionalDnsServer(dnsServer);
      }
      else
      {
         ErrLog( << "Tried to add dns server, but invalid format: " << dnsServer);
      }
      if(!pb.eof())
      {
         pb.skipChar();
      }
   }   

   // Disable Statisitics Manager
   profile->statisticsManagerEnabled() = false;

   if(mKeepAlives)
   {
      profile->setKeepAliveTimeForDatagram(30);
      profile->setKeepAliveTimeForStream(180);
   }

   // Support Methods, etc.
   profile->validateContentEnabled() = false;
   profile->validateContentLanguageEnabled() = false;
   profile->validateAcceptEnabled() = false;

   profile->clearSupportedLanguages();
   profile->addSupportedLanguage(Token("en"));  

   profile->clearSupportedMimeTypes();
   profile->addSupportedMimeType(INVITE, Mime("application", "sdp"));
   profile->addSupportedMimeType(INVITE, Mime("multipart", "mixed"));  
   profile->addSupportedMimeType(INVITE, Mime("multipart", "signed"));  
   profile->addSupportedMimeType(INVITE, Mime("multipart", "alternative"));  
   profile->addSupportedMimeType(OPTIONS,Mime("application", "sdp"));
   profile->addSupportedMimeType(OPTIONS,Mime("multipart", "mixed"));  
   profile->addSupportedMimeType(OPTIONS, Mime("multipart", "signed"));  
   profile->addSupportedMimeType(OPTIONS, Mime("multipart", "alternative"));  
   profile->addSupportedMimeType(NOTIFY, Mime("message", "sipfrag"));  

   profile->clearSupportedMethods();
   profile->addSupportedMethod(INVITE);
   profile->addSupportedMethod(ACK);
   profile->addSupportedMethod(CANCEL);
   profile->addSupportedMethod(OPTIONS);
   profile->addSupportedMethod(BYE);
   profile->addSupportedMethod(REFER);    
   profile->addSupportedMethod(NOTIFY);    
   profile->addSupportedMethod(SUBSCRIBE); 

   profile->clearSupportedOptionTags();
   profile->addSupportedOptionTag(Token(Symbols::Replaces));      
   profile->addSupportedOptionTag(Token(Symbols::Timer)); 
   profile->addSupportedOptionTag(Token(Symbols::NoReferSub));
   profile->addSupportedOptionTag(Token(Symbols::AnswerMode));
   profile->addSupportedOptionTag(Token(Symbols::TargetDialog));

   profile->setUacReliableProvisionalMode(MasterProfile::Never);

   profile->clearSupportedSchemes();
   profile->addSupportedScheme("sip");  
#ifdef USE_SSL
   profile->addSupportedScheme("sips");
#endif

   // Have stack add Allow/Supported/Accept headers to INVITE dialog establishment messages
   profile->clearAdvertisedCapabilities(); // Remove Profile Defaults, then add our preferences
   profile->addAdvertisedCapability(Headers::Allow);  
   //profile->addAdvertisedCapability(Headers::AcceptEncoding);  // This can be misleading - it might specify what is expected in response
   profile->addAdvertisedCapability(Headers::AcceptLanguage);  
   profile->addAdvertisedCapability(Headers::Supported);  
   profile->setMethodsParamEnabled(true);

   //profile->setOverrideHostAndPort(mContact);
   if(!mOutboundProxy.uri().host().empty())
   {
      profile->setOutboundProxy(mOutboundProxy.uri());
   }

   profile->setUserAgent("MOHParkServer");
   profile->rtpPortRangeMin() = mMediaPortRangeStart; 
   profile->rtpPortRangeMax() = mMediaPortRangeStart + mMediaPortRangeSize-1; 

   mUserAgentMasterProfile = profile;

   // Create UserAgent
   mUserAgent = new MyUserAgent(this, profile);
}

Server::~Server()
{
   shutdown();
   delete mUserAgent;
}

void
Server::startup()
{
   assert(mUserAgent);
   mUserAgent->startup();
   mMOHManager.startup();
   mParkManager.startup();
}

void 
Server::process(int timeoutMs)
{
   assert(mUserAgent);
   mUserAgent->process(timeoutMs);
}

void
Server::shutdown()
{
   mMOHManager.shutdown();
   mParkManager.shutdown();
   assert(mUserAgent);
   mUserAgent->shutdown();
   OsSysLog::shutdown();
}

void 
Server::buildSessionCapabilities(resip::SdpContents& sessionCaps)
{
   unsigned int codecIds[] = { SdpCodec::SDP_CODEC_PCMU /* 0 - pcmu */, 
                               SdpCodec::SDP_CODEC_PCMA /* 8 - pcma */, 
                               SdpCodec::SDP_CODEC_SPEEX /* 96 - speex NB 8,000bps */,
                               SdpCodec::SDP_CODEC_SPEEX_15 /* 98 - speex NB 15,000bps */, 
                               SdpCodec::SDP_CODEC_SPEEX_24 /* 99 - speex NB 24,600bps */,
                               SdpCodec::SDP_CODEC_L16_44100_MONO /* PCM 16 bit/sample 44100 samples/sec. */, 
                               SdpCodec::SDP_CODEC_G726_16,
                               SdpCodec::SDP_CODEC_G726_24,
                               SdpCodec::SDP_CODEC_G726_32,
                               SdpCodec::SDP_CODEC_G726_40,
                               SdpCodec::SDP_CODEC_ILBC /* 108 - iLBC */,
                               SdpCodec::SDP_CODEC_ILBC_20MS /* 109 - Internet Low Bit Rate Codec, 20ms (RFC3951) */, 
                               SdpCodec::SDP_CODEC_SPEEX_5 /* 97 - speex NB 5,950bps */,
                               SdpCodec::SDP_CODEC_GSM /* 3 - GSM */,
                               SdpCodec::SDP_CODEC_TONES /* 110 - telephone-event */};
   unsigned int numCodecIds = sizeof(codecIds) / sizeof(codecIds[0]);
   ConversationManager::buildSessionCapabilities(mAddress, numCodecIds, codecIds, sessionCaps);
}

void 
Server::onConversationDestroyed(ConversationHandle convHandle)
{
   InfoLog(<< "onConversationDestroyed: handle=" << convHandle);
}

void 
Server::onParticipantDestroyed(ParticipantHandle partHandle)
{
   InfoLog(<< "onParticipantDestroyed: handle=" << partHandle);
   if(!mMOHManager.removeParticipant(partHandle))
   {
      mParkManager.removeParticipant(partHandle);
   }
}

void 
Server::onDtmfEvent(ParticipantHandle partHandle, int dtmf, int duration, bool up)
{
   InfoLog(<< "onDtmfEvent: handle=" << partHandle << " tone=" << dtmf << " dur=" << duration << " up=" << up);
}

void 
Server::onIncomingParticipant(ParticipantHandle partHandle, const SipMessage& msg, bool autoAnswer, ConversationProfile& conversationProfile)
{
   InfoLog(<< "onIncomingParticipant: handle=" << partHandle << " auto=" << autoAnswer << " msg=" << msg.brief());

   if(mMOHManager.isMyProfile(conversationProfile))
   {
   mMOHManager.addParticipant(partHandle);
   }
   else if(mParkManager.isMyProfile(conversationProfile))
   {
      mParkManager.incomingParticipant(partHandle, msg);      
   }
   else
   {
      rejectParticipant(partHandle, 404);
   }
}

void 
Server::onRequestOutgoingParticipant(ParticipantHandle partHandle, const SipMessage& msg, ConversationProfile& conversationProfile)
{
   InfoLog(<< "onRequestOutgoingParticipant: handle=" << partHandle << " msg=" << msg.brief());
   if(mMOHManager.isMyProfile(conversationProfile))
   {
   mMOHManager.addParticipant(partHandle);
   }
   else if(mParkManager.isMyProfile(conversationProfile))
   {
      mParkManager.parkParticipant(partHandle, msg);      
   }
   else
   {
      rejectParticipant(partHandle, 404);
   }
}
    
void 
Server::onParticipantTerminated(ParticipantHandle partHandle, unsigned int statusCode)
{
   InfoLog(<< "onParticipantTerminated: handle=" << partHandle);
}
    
void 
Server::onParticipantProceeding(ParticipantHandle partHandle, const SipMessage& msg)
{
   InfoLog(<< "onParticipantProceeding: handle=" << partHandle << " msg=" << msg.brief());
}

void 
Server::onRelatedConversation(ConversationHandle relatedConvHandle, ParticipantHandle relatedPartHandle, 
                                      ConversationHandle origConvHandle, ParticipantHandle origPartHandle)
{
   InfoLog(<< "onRelatedConversation: relatedConvHandle=" << relatedConvHandle << " relatedPartHandle=" << relatedPartHandle
           << " origConvHandle=" << origConvHandle << " origPartHandle=" << origPartHandle);
}

void 
Server::onParticipantAlerting(ParticipantHandle partHandle, const SipMessage& msg)
{
   InfoLog(<< "onParticipantAlerting: handle=" << partHandle << " msg=" << msg.brief());
}
    
void 
Server::onParticipantConnected(ParticipantHandle partHandle, const SipMessage& msg)
{
   InfoLog(<< "onParticipantConnected: handle=" << partHandle << " msg=" << msg.brief());
}

void 
Server::onParticipantRedirectSuccess(ParticipantHandle partHandle)
{
   InfoLog(<< "onParticipantRedirectSuccess: handle=" << partHandle);
}

void 
Server::onParticipantRedirectFailure(ParticipantHandle partHandle, unsigned int statusCode)
{
   InfoLog(<< "onParticipantRedirectFailure: handle=" << partHandle << " statusCode=" << statusCode);
}

}

/* ====================================================================

 Copyright (c) 2010, SIP Spectrum, Inc.
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

