#if !defined(RESIP_DIALOGUSAGEMANAGER_HXX)
#define RESIP_DIALOGUSAGEMANAGER_HXX

#include <vector>
#include <set>
#include <map>

#include "resiprocate/Headers.hxx"
#include "resiprocate/dum/DialogSet.hxx"
#include "resiprocate/dum/DumTimeout.hxx"
#include "resiprocate/dum/HandleManager.hxx"
#include "resiprocate/dum/Handles.hxx"
#include "resiprocate/dum/MergedRequestKey.hxx"
#include "resiprocate/os/BaseException.hxx"

namespace resip 
{

class SipStack;
class FdSet;
class Profile;
class RedirectManager;
class ClientAuthManager;
class ServerAuthManager;
class Uri;
class SdpContents;

class ClientRegistrationHandler;
class ServerRegistrationHandler;
class InviteSessionHandler;
class ClientSubscriptionHandler;
class ServerSubscriptionHandler;
class ClientPublicationHandler;
class ServerPublicationHandler;
class OutOfDialogHandler;

class Dialog;
class InviteSessionCreator;

class AppDialogSetFactory;
class DumShutdownHandler;

class DialogUsageManager : public HandleManager
{
   public:
      class Exception : public BaseException
      {
         public:
            Exception(const Data& msg,
                      const Data& file,
                      int line)
               : BaseException(msg, file, line)
            {}
            
            virtual const char* name() const {return "DialogUsageManager::Exception";}
      };
      
      DialogUsageManager(SipStack& stack);
      virtual ~DialogUsageManager();
      
      void shutdown(DumShutdownHandler*);

      void setAppDialogSetFactory(AppDialogSetFactory*);

      // !dcm! -- factory not inhereitance.  virtual AppDialogSet* createAppDialogSet(const SipMessage&);
      // no real reson other than this is the only method that would be overloaded
#if 0
      virtual AppDialog* createAppDialog();
      virtual ClientRegistration* createAppClientRegistration(Dialog& dialog, BaseCreator& creator);
      virtual ClientInviteSession* createAppClientInviteSession(Dialog& dialog, const InviteSessionCreator& creator);
      virtual ClientSubscription* createAppClientSubscription(Dialog& dialog, BaseCreator& creator);
      virtual ClientPublication* createAppClientPublication(Dialog& dialog, BaseCreator& creator);
      virtual ClientOutOfDialogReq* createAppClientOutOfDialogRequest(Dialog& dialog, BaseCreator& creator);

      virtual ServerOutOfDialogReq* createAppServerOutOfDialogRequest();
      virtual ServerSubscription* createAppServerSubscription();
      virtual ServerInviteSession* createAppServerInviteSession();
      virtual ServerRegistration* createAppServerRegistration();
      virtual ServerPublication* createAppServerPublication();
#endif

      void setProfile(Profile* profile);
      Profile* getProfile();
      
      /// There should probably be a default redirect manager
      void setRedirectManager(RedirectManager* redirect);

      /// If there is no ClientAuthManager, when the client receives a 401/407,
      /// pass it up through the normal BaseUsageHandler
      void setClientAuthManager(ClientAuthManager* client);

      /// If there is no ServerAuthManager, the server does not authenticate requests
      void setServerAuthManager(ServerAuthManager* client);

      /// If there is no such handler, calling makeInviteSession will throw and
      /// receiving an INVITE as a UAS will respond with 405 Method Not Allowed
      void setInviteSessionHandler(InviteSessionHandler*);
      
      /// If there is no such handler, calling makeRegistration will throw
      void setClientRegistrationHandler(ClientRegistrationHandler*);

      /// If no such handler, UAS will respond to REGISTER with 405 Method Not Allowed
      void setServerRegistrationHandler(ServerRegistrationHandler*);

      /// If there is no such handler, calling makeSubscription will throw
      void addClientSubscriptionHandler(const Data& eventType, ClientSubscriptionHandler*);

      /// If there is no such handler, calling makePublication will throw
      void addClientPublicationHandler(const Data& eventType, ClientPublicationHandler*);
      
      void addServerSubscriptionHandler(const Data& eventType, ServerSubscriptionHandler*);
      void addServerPublicationHandler(const Data& eventType, ServerPublicationHandler*);
      
      void addOutOfDialogHandler(MethodTypes&, OutOfDialogHandler*);
      
      // The message is owned by the underlying datastructure and may go away in
      // the future. If the caller wants to keep it, it should make a copy. The
      // memory will exist at least up until the point where the application
      // calls DialogUsageManager::send(msg);
      SipMessage& makeInviteSession(const Uri& target, const SdpContents* initialOffer, AppDialogSet* = 0);
      SipMessage& makeSubscription(const Uri& aor, const NameAddr& target, const Data& eventType, AppDialogSet* = 0);
      //unsolicited refer
      SipMessage& makeRefer(const Uri& aor, const H_ReferTo::Type& referTo, AppDialogSet* = 0);

      SipMessage& makePublication(const Uri& targetDocument, const Contents& body, 
                                  const Data& eventType, unsigned expiresSeconds, AppDialogSet* = 0);

      SipMessage& makeRegistration(const NameAddr& aor, AppDialogSet* = 0);
      SipMessage& makeOutOfDialogRequest(const Uri& aor, const MethodTypes& meth, AppDialogSet* = 0);

      // all can be done inside of INVITE or SUBSCRIBE only; !dcm! -- now live
      // in INVITE or SUBSCRIBE only
//       SipMessage& makeSubscribe(DialogId, const Uri& aor, const Data& eventType);
//       SipMessage& makeRefer(DialogId, const Uri& aor, const H_ReferTo::Type& referTo);
//       SipMessage& makePublish(DialogId, const Uri& targetDocument, const Data& eventType, unsigned expireSeconds); 
//       SipMessage& makeOutOfDialogRequest(DialogId, const Uri& aor, const MethodTypes& meth);
      
      void cancel(DialogSetId invSessionId);
      void send(SipMessage& request); 
      
      void buildFdSet(FdSet& fdset);
      void process(FdSet& fdset);

      /// returns time in milliseconds when process next needs to be called 
      int getTimeTillNextProcessMS(); 

      InviteSessionHandle findInviteSession(DialogId id);
      std::vector<ClientSubscriptionHandle> findClientSubscriptions(DialogId id);
      std::vector<ClientSubscriptionHandle> findClientSubscriptions(DialogSetId id);
      std::vector<ClientSubscriptionHandle> findClientSubscriptions(DialogSetId id, const Data& eventType, const Data& subId);
      ServerSubscriptionHandle findServerSubscription(DialogId id);
      ClientRegistrationHandle findClientRegistration(DialogId id);
      ServerRegistrationHandle findServerRegistration(DialogId id);
      ClientPublicationHandle findClientPublication(DialogId id);
      ServerPublicationHandle findServerPublication(DialogId id);
      std::vector<ClientOutOfDialogReqHandle> findClientOutOfDialog(DialogId id);
      ServerOutOfDialogReqHandle findServerOutOfDialog(DialogId id);
   private:
      friend class Dialog;
      friend class DialogSet;

      friend class ClientInviteSession;
      friend class ClientOutOfDialogReq;
      friend class ClientPublication;
      friend class ClientRegistration;
      friend class ClientSubscription;
      friend class InviteSession;
      friend class ServerInviteSession;
      friend class ServerOutOfDialogReq;
      friend class ServerPublication;
      friend class ServerRegistration;
      friend class ServerSubscription;
      friend class BaseUsage;

      
      SipMessage& makeNewSession(BaseCreator* creator, AppDialogSet* appDs);
      
      // makes a proto response to a request
      void makeResponse(SipMessage& response, 
                        const SipMessage& request, 
                        int responseCode, 
                        const Data& reason = Data::Empty) const;
      // May call a callback to let the app adorn
      void sendResponse(SipMessage& response);

      void addTimer(DumTimeout::Type type,
                    unsigned long duration,
                    BaseUsageHandle target, 
                    int seq, 
                    int altseq=-1);

      Dialog& findOrCreateDialog(const SipMessage* msg);
      Dialog& findDialog(const DialogId& id);
      DialogSet* findDialogSet(const DialogSetId& id);
      
      // return 0, if no matching BaseCreator
      BaseCreator* findCreator(const DialogId& id);

      void prepareInitialRequest(SipMessage& request);
      void processRequest(const SipMessage& request);
      void processResponse(const SipMessage& response);
      bool validateRequest(const SipMessage& request);
      bool validateTo(const SipMessage& request);
      bool mergeRequest(const SipMessage& request);

      void removeDialogSet(const DialogSetId& );      

      typedef std::set<MergedRequestKey> MergedRequests;
      MergedRequests mMergedRequests;
            
      typedef HashMap<DialogSetId, DialogSet*> DialogSetMap;
      DialogSetMap mDialogSetMap;

      Profile* mProfile;
      RedirectManager* mRedirectManager;
      ClientAuthManager* mClientAuthManager;
      ServerAuthManager* mServerAuthManager;  
    
      InviteSessionHandler* mInviteSessionHandler;
      ClientRegistrationHandler* mClientRegistrationHandler;
      ServerRegistrationHandler* mServerRegistrationHandler;      

      ClientSubscriptionHandler* getClientSubscriptionHandler(const Data& eventType);
      ServerSubscriptionHandler* getServerSubscriptionHandler(const Data& eventType);
      
      std::map<Data, ClientSubscriptionHandler*> mClientSubscriptionHandlers;
      std::map<Data, ServerSubscriptionHandler*> mServerSubscriptionHandlers;
      std::map<Data, ClientPublicationHandler*> mClientPublicationHandler;
      std::map<Data, ServerPublicationHandler*> mServerPublicationHandler;
      std::map<MethodTypes, OutOfDialogHandler*> mOutOfDialogHandler;

      AppDialogSetFactory* mAppDialogSetFactory;

      SipStack& mStack;
      DumShutdownHandler* mDumShutdownHandler;       
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
