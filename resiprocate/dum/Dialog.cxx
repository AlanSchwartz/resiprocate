#include "resiprocate/Contents.hxx"
#include "resiprocate/Helper.hxx"
#include "resiprocate/SipMessage.hxx"
#include "resiprocate/dum/AppDialog.hxx"
#include "resiprocate/dum/BaseCreator.hxx"
#include "resiprocate/dum/ClientAuthManager.hxx"
#include "resiprocate/dum/ClientInviteSession.hxx"
#include "resiprocate/dum/ClientSubscription.hxx"
#include "resiprocate/dum/Dialog.hxx"
#include "resiprocate/dum/DialogUsageManager.hxx"
#include "resiprocate/dum/InviteSessionCreator.hxx"
#include "resiprocate/dum/InviteSessionHandler.hxx"
#include "resiprocate/dum/ServerInviteSession.hxx"
#include "resiprocate/dum/ServerSubscription.hxx"
#include "resiprocate/dum/SubscriptionHandler.hxx"
#include "resiprocate/dum/UsageUseException.hxx"
#include "resiprocate/os/Logger.hxx"

#if defined(WIN32) && defined(_DEBUG) && defined(LEAK_CHECK)// Used for tracking down memory leaks in Visual Studio
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new   new( _NORMAL_BLOCK, __FILE__, __LINE__)
#endif // defined(WIN32) && defined(_DEBUG)

#define RESIPROCATE_SUBSYSTEM Subsystem::DUM

using namespace resip;
using namespace std;

Dialog::Dialog(DialogUsageManager& dum, const SipMessage& msg, DialogSet& ds) 
   : mDum(dum),
     mDialogSet(ds),
     mId("INVALID", "INVALID", "INVALID"),
     mClientSubscriptions(),
     mServerSubscriptions(),
     mInviteSession(0),
     mType(Fake),
     mRouteSet(),
     mLocalContact(),
     mLocalCSeq(0),
     mRemoteCSeq(0),
     mRemoteTarget(),
     mLocalNameAddr(),
     mRemoteNameAddr(),
     mCallId(msg.header(h_CallID)),
     mAppDialog(0),
     mDestroying(false)
{
   assert(msg.isExternal());

   
   if (msg.isRequest()) // UAS
   {
      const SipMessage& request = msg;

      switch (request.header(h_CSeq).method())
      {
         case INVITE:
            mType = Invitation;
            break;
            
         case SUBSCRIBE:
         case REFER:
         case NOTIFY:
            //!dcm! -- event header check
            mType = Subscription;
            break;

         default:
            mType = Fake;
      }
      if (request.exists(h_RecordRoutes))
      {
         mRouteSet = request.header(h_RecordRoutes); // !jf! is this right order
      }

      switch (request.header(h_CSeq).method())
      {
         case INVITE:
         case SUBSCRIBE:
         case REFER:
         case NOTIFY:
            DebugLog ( << "UAS dialog ID creation, DS: " << ds.getId());            
            mId = DialogId(ds.getId(), request.header(h_From).param(p_tag));
            mRemoteNameAddr = request.header(h_From);
            mLocalNameAddr = request.header(h_To);
            mLocalNameAddr.param(p_tag) = mId.getLocalTag();
            if (request.exists(h_Contacts) && request.header(h_Contacts).size() == 1)
            {
               const NameAddr& contact = request.header(h_Contacts).front();
               if (isEqualNoCase(contact.uri().scheme(), Symbols::Sips) ||
                   isEqualNoCase(contact.uri().scheme(), Symbols::Sip))
               {
                  mLocalContact = NameAddr(request.header(h_RequestLine).uri()); // update later when send a request 
                  mRemoteTarget = contact;
               }
               else
               {
                  InfoLog(<< "Got an INVITE or SUBSCRIBE with invalid scheme");
                  DebugLog(<< request);
                  throw Exception("Invalid scheme in request", __FILE__, __LINE__);
               }
            }
            else
            {
               InfoLog (<< "Got an INVITE or SUBSCRIBE that doesn't have exactly one contact");
               DebugLog (<< request);
               throw Exception("Too many (or no contact) contacts in request", __FILE__, __LINE__);
            }
            break;
         default:
            break;
      }
      
      mRemoteCSeq = request.header(h_CSeq).sequence();
      mLocalCSeq = 1;

      DebugLog ( << "************** Created Dialog as UAS **************" );      
      DebugLog ( << "mRemoteNameAddr: " << mRemoteNameAddr ); 
      DebugLog ( << "mLocalNameAddr: " << mLocalNameAddr ); 
      DebugLog ( << "mLocalContact: " << mLocalContact );
      DebugLog ( << "mRemoteTarget: " << mRemoteTarget );
   }
   else if (msg.isResponse())
   {
      mId = DialogId(msg);      
      const SipMessage& response = msg;
      mRemoteNameAddr = response.header(h_To);
      mLocalNameAddr = response.header(h_From);

      switch (msg.header(h_CSeq).method())
      {
         case INVITE:
            mType = Invitation;
            break;
            
         case SUBSCRIBE:
         case REFER:
            mType = Subscription;
            break;

         default:
            mType = Fake;
      }

      if (response.exists(h_RecordRoutes))
      {
         mRouteSet = response.header(h_RecordRoutes).reverse();
      }

      switch (response.header(h_CSeq).method())
      {
         case INVITE:
         case SUBSCRIBE:
         case REFER:
            if (response.header(h_StatusLine).statusCode() > 100 &&
                response.header(h_StatusLine).statusCode() < 300)
            {
               
               if  (response.exists(h_Contacts) && response.header(h_Contacts).size() == 1)
               {
                  const NameAddr& contact = response.header(h_Contacts).front();
                  if (isEqualNoCase(contact.uri().scheme(), Symbols::Sips) ||
                      isEqualNoCase(contact.uri().scheme(), Symbols::Sip))
                  {
                     BaseCreator* creator = mDialogSet.getCreator();
                     assert(creator);// !jf! throw or something here
                  
                     mLocalContact = creator->getLastRequest().header(h_Contacts).front();
                     mRemoteTarget = contact;
                  }
                  else
                  {
                     InfoLog (<< "Got an INVITE or SUBSCRIBE with invalid scheme");
                     DebugLog (<< response);
                     throw Exception("Bad scheme in contact in response", __FILE__, __LINE__);
                  }
               }
               else
               {
                  InfoLog (<< "Got an INVITE or SUBSCRIBE that doesn't have exactly one contact");
                  DebugLog (<< response);
                  throw Exception("Too many contacts (or no contact) in response", __FILE__, __LINE__);
               }
               break;
               default:
                  break;
            }
      }
      
      mLocalCSeq = response.header(h_CSeq).sequence();
      mRemoteCSeq = 0;
      DebugLog ( << "************** Created Dialog as UAC **************" );      
      DebugLog ( << "mRemoteNameAddr: " << mRemoteNameAddr ); 
      DebugLog ( << "mLocalNameAddr: " << mLocalNameAddr ); 
      DebugLog ( << "mLocalContact: " << mLocalContact );
      DebugLog ( << "mRemoteTarget: " << mRemoteTarget );

      
   }
   mDialogSet.addDialog(this);
   DebugLog ( <<"Dialog::Dialog " << mId);
}

Dialog::~Dialog()
{
   DebugLog ( <<"Dialog::~Dialog() ");
   
   mDestroying = true;

   while (!mClientSubscriptions.empty())
   {
      delete *mClientSubscriptions.begin();
   }

   while (!mServerSubscriptions.empty())
   {
      delete *mServerSubscriptions.begin();
   }

   delete mInviteSession;

   mDialogSet.mDialogs.erase(this->getId());
   delete mAppDialog;
   mDialogSet.possiblyDie();
}

DialogId
Dialog::getId() const
{
   return mId;
}

void
Dialog::cancel()
{
   if (mInviteSession)
   {
      mInviteSession->send(mInviteSession->end());
   }
   else
   {
      if (mDialogSet.getCreator())
      {
         SipMessage& request = mDialogSet.getCreator()->getLastRequest();
         if (request.header(h_RequestLine).method() == INVITE)
         {
            makeCancel(request);
            mDum.send(request);
            delete this;
         }
         else
         {
            throw new UsageUseException("Can only CANCEL an INVITE", __FILE__, __LINE__);
         }
      }
      else
      {
         throw new UsageUseException("Attempting to cancel UAS dialogSet", __FILE__, __LINE__);
      }
   }
}

void
Dialog::dispatch(const SipMessage& msg)
{
   DebugLog ( << "Dialog::dispatch: " << msg.brief());
   if (msg.isRequest())
   {
      const SipMessage& request = msg;
      switch (request.header(h_CSeq).method())
     {
         case INVITE:  // new INVITE
            if (mInviteSession == 0)
            {
               DebugLog ( << "Dialog::dispatch  --  Created new server invite session" << msg.brief());
               mInviteSession = makeServerInviteSession(request);
            }
            mInviteSession->dispatch(request);
            break;
            //refactor, send bad request for BYE, INFO, CANCEL?
         case BYE:
            if (mInviteSession == 0)
            {
               InfoLog ( << "Spurious BYE" );
               return;               
            }
            else
            {
               mInviteSession->dispatch(request);
            }
            break;
         case INFO:
            if (mInviteSession == 0)
            {
               InfoLog ( << "Spurious INFO" );
               return;               
            }
            else
            {
               mInviteSession->dispatch(request);
            }
            break;
         case ACK:
         case CANCEL:
            if (mInviteSession == 0)
            {
               InfoLog (<< "Drop stray ACK or CANCEL in dialog on the floor");
               DebugLog (<< request);
            }
            else
            {
               mInviteSession->dispatch(request);
            }
            break;
         case SUBSCRIBE:
         {
            ServerSubscription* server = findMatchingServerSub(request);
            if (server)
            {
               server->dispatch(request);
            }
            else
            {
               if (request.header(h_Event).value() == "refer")
               {
                  InfoLog (<< "Received a subscribe to a non-existent refer subscription: " << request.brief());
                  SipMessage failure;
                  makeResponse(failure, request, 403);
                  mDum.sendResponse(failure);
                  return;
               }
               else
               {
                  server = makeServerSubscription(request);
                  mServerSubscriptions.push_back(server);
                  server->dispatch(request);
               }
            }
//            mDum.mInviteSessionHandler->onRefer(mInviteSession->getSessionHandle(), server->getHandle(), msg);
         }
         break;
         case REFER:
         {
            if (mInviteSession == 0)
            {
               InfoLog (<< "Received an in dialog refer in a non-invite dialog: " << request.brief());
               SipMessage failure;
               makeResponse(failure, request, 405);
               mDum.sendResponse(failure);
               return;
            }
            else if  (!request.exists(h_ReferTo))
            {
               InfoLog (<< "Received refer w/out a Refer-To: " << request.brief());
               SipMessage failure;
               makeResponse(failure, request, 400);
               mDum.sendResponse(failure);
               return;
            }
            else
            {
               ServerSubscription* server = findMatchingServerSub(request);
               if (server)
               {
                  server->dispatch(request);
               }
               else
               {
                  server = makeServerSubscription(request);
                  mServerSubscriptions.push_back(server);
                  server->dispatch(request);
               }
               mDum.mInviteSessionHandler->onRefer(mInviteSession->getSessionHandle(), server->getHandle(), msg);    
            }
         }
         break;
         case NOTIFY:
         {
            ClientSubscription* client = findMatchingClientSub(request);
            if (client)
            {
               client->dispatch(request);
            }
            else
            {
               BaseCreator* creator = mDialogSet.getCreator();
               if (creator && (creator->getLastRequest().header(h_RequestLine).method() == SUBSCRIBE ||
                               creator->getLastRequest().header(h_RequestLine).method() == REFER))
               {
                  DebugLog (<< "Making subscription (from creator) request: " << creator->getLastRequest());
                  ClientSubscription* sub = makeClientSubscription(creator->getLastRequest());
                  mClientSubscriptions.push_back(sub);
                  sub->dispatch(request);
               }
               else if (mInviteSession->mLastRequest.header(h_RequestLine).method() == REFER)
               {
                  DebugLog (<< "Making subscription from refer: " << mInviteSession->mLastRequest);
                  ClientSubscription* sub = makeClientSubscription(mInviteSession->mLastRequest);
                  mClientSubscriptions.push_back(sub);
                  sub->dispatch(request);
               }
               else
               {
                  SipMessage failure;
                  makeResponse(failure, request, 481);
                  failure.header(h_To).remove(p_tag); // otherwise it will be INVALID
                  InfoLog (<< "Sending 481 - no dialog created " << endl << failure);
                  mDum.sendResponse(failure);
                  return;
               }
            }
         }
         break;
        default:
           assert(0);
           return;
      }
   }
   else if (msg.isResponse())
   {
      if (!mDialogSet.getCreator() ||
          !(msg.header(h_CSeq) == mDialogSet.getCreator()->getLastRequest().header(h_CSeq)))
      {
         SipMessage* lastRequest = 0;            
         switch (msg.header(h_CSeq).method())
         {
            case INVITE:
            case CANCEL:
            case REFER: 
            case BYE:
               if (mInviteSession == 0)
               {
                  //spurious
                  return;
               }
               else
               {
                  lastRequest = &mInviteSession->mLastRequest;
               }
               break;               
            case INFO:
            {
               if (mInviteSession == 0)
               {
                  //spurious
                  return;
               }
               else
               {
                  lastRequest = &mInviteSession->mLastNit;
               }
               break;               
            }
            default:
               break;
         }
		 if ( lastRequest && mDum.mClientAuthManager.get() && mDum.mClientAuthManager->handle( *lastRequest, msg ) )
         {
            InfoLog( << "about to re-send request with digest credentials" );
            InfoLog( << *lastRequest );
            mLocalCSeq++;            
            mDum.send(*lastRequest);
            return;
         }
      }
      
      const SipMessage& response = msg;
      int code = response.header(h_StatusLine).statusCode();
      if (code >=200 && code < 300)
      {
         if (response.exists(h_RecordRoutes))
         {
            mRouteSet = response.header(h_RecordRoutes).reverse();
         }
      }
      
      // !jf! should this only be for 2xx responses? !jf! Propose no as an
      // answer !dcm! what is he on?
      switch (response.header(h_CSeq).method())
      {
         case INVITE:
            if (mInviteSession == 0)
            {
               // #if!jf! don't think creator needs a dispatch
               //BaseCreator* creator = mDum.findCreator(mId);
               //assert (creator); // stray responses have been rejected already 
               //creator->dispatch(response); 
               // #endif!jf! 
               DebugLog ( << "Dialog::dispatch  --  Created new client invite session" << msg.brief());

               mInviteSession = makeClientInviteSession(response);
               mInviteSession->dispatch(response);
            }
            else
            {
               mInviteSession->dispatch(response);
            }
            break;
         case BYE:
         case ACK:
         case CANCEL:
         case INFO:
            if (mInviteSession != 0)
            {
               mInviteSession->dispatch(response);
            }
            // else drop on the floor
            break;               
         case REFER: 
         {
            int code = response.header(h_StatusLine).statusCode();
            if (code < 300)
            {
               // throw it away
               return;
            }            
            else
            {
               if (mInviteSession && mDum.mInviteSessionHandler)
               {
                  mDum.mInviteSessionHandler->onReferRejected(mInviteSession->getSessionHandle(), response);
               }
            }         
         }
         break;         
         case SUBSCRIBE:
         {
            int code = response.header(h_StatusLine).statusCode();
            if (code < 300)
            {
               // throw it away
               return;
            }            
            else
            {
               ClientSubscription* client = findMatchingClientSub(response);
               if (client)
               {
                  client->dispatch(response);
               }
               else
               {
                  //!dcm! -- can't subscribe in an existing Dialog, this is all 
                  //a bit of a hack; currently, spurious failure messages may cause callbacks
                  BaseCreator* creator = mDialogSet.getCreator();
                  if (!creator || !creator->getLastRequest().exists(h_Event))
                  {
                     return;
                  }
                  else
                  {
                     ClientSubscriptionHandler* handler = 
                        mDum.getClientSubscriptionHandler(creator->getLastRequest().header(h_Event).value());
                     if (handler)
                     {
                        ClientSubscription* sub = makeClientSubscription(creator->getLastRequest());
                        mClientSubscriptions.push_back(sub);
                        sub->dispatch(response);
                     }
                  }
               }
            }
         }
         break;
         case NOTIFY:
         {
            //2xx responses are treated as retransmission quenchers(handled by
            //the stack). Failures are dispatched to all ServerSubsscriptions,
            //which may not be correct.

            int code = msg.header(h_StatusLine).statusCode();
            if (code >= 300)
            {
               //!dcm! -- ick, write guard
               mDestroying = true;               
               for (list<ServerSubscription*>::iterator it = mServerSubscriptions.begin();
                    it != mServerSubscriptions.end(); )
               {
                  ServerSubscription* s = *it;
                  it++;
                  s->dispatch(msg);
               }
               mDestroying = false;
               possiblyDie();
            }
//             ServerSubscription* server = findMatchingServerSub(response);
//             if (server)
//             {
//                server->dispatch(response);
//             }
         }
         break;         
         default:
            assert(0);
            return;
      }
   }
}

ServerSubscription* 
Dialog::findMatchingServerSub(const SipMessage& msg)
{
   for (std::list<ServerSubscription*>::iterator i=mServerSubscriptions.begin(); 
        i != mServerSubscriptions.end(); ++i)
   {
      if ((*i)->matches(msg))
      {
         return *i;
      }
   }
   return 0;
}

ClientSubscription* 
Dialog::findMatchingClientSub(const SipMessage& msg)
{
   for (std::list<ClientSubscription*>::iterator i=mClientSubscriptions.begin(); 
        i != mClientSubscriptions.end(); ++i)
   {
      if ((*i)->matches(msg))
      {
         return *i;
      }
   }
   return 0;
}

InviteSessionHandle
Dialog::getInviteSession()
{
   if (mInviteSession)
   {
      return mInviteSession->getSessionHandle();
   }
   else
   {
      return InviteSessionHandle::NotValid();
   }
}

std::vector<ClientSubscriptionHandle> 
Dialog::findClientSubscriptions(const Data& event)
{
   std::vector<ClientSubscriptionHandle> handles;
   
   for (std::list<ClientSubscription*>::const_iterator i = mClientSubscriptions.begin();
        i != mClientSubscriptions.end(); ++i)
   {
      if ( (*i)->getEventType() == event)
      {
         handles.push_back((*i)->getHandle());
      }
   }
   return handles;
}

std::vector<ServerSubscriptionHandle> 
Dialog::findServerSubscriptions(const Data& event)
{
   std::vector<ServerSubscriptionHandle> handles;
   
   for (std::list<ServerSubscription*>::const_iterator i = mServerSubscriptions.begin();
        i != mServerSubscriptions.end(); ++i)
   {
      if ( (*i)->getEventType() == event)
      {
         handles.push_back((*i)->getHandle());
      }
   }
   return handles;
}


std::vector<ClientSubscriptionHandle> 
Dialog::getClientSubscriptions()
{
   std::vector<ClientSubscriptionHandle> handles;
   
   for (std::list<ClientSubscription*>::const_iterator i = mClientSubscriptions.begin();
        i != mClientSubscriptions.end(); ++i)
   {
      handles.push_back((*i)->getHandle());
   }

   return handles;
}

std::vector<ServerSubscriptionHandle> 
Dialog::getServerSubscriptions()
{
   std::vector<ServerSubscriptionHandle> handles;
   
   for (std::list<ServerSubscription*>::const_iterator i = mServerSubscriptions.begin();
        i != mServerSubscriptions.end(); ++i)
   {
      handles.push_back((*i)->getHandle());
   }

   return handles;
}

void 
Dialog::redirected(const SipMessage& msg)
{
   //Established dialogs are not destroyed by a redirect
   if (!mClientSubscriptions.empty() || !mServerSubscriptions.empty())
   {
      return;
   }
   if (mInviteSession)
   {
      ClientInviteSession* cInv = dynamic_cast<ClientInviteSession*>(mInviteSession);
      if (cInv)
      {
         cInv->redirected(msg);         
      }
   }
}


#if 0
void
Dialog::processNotify(const SipMessage& notify)
{
   if (notify.isRequest())
   {
      if (findSubscriptions().empty())
      {
         SubscriptionCreator* creator = dynamic_cast<SubscriptionCreator*>(DialogSetId(notify).getCreator());
         if (creator)
         {
            creator->makeNewSubscription(notify);
         }
      }
      else
      {
         for (std::list<BaseUsage*>::iterator i=mUsages.begin(); i!=mUsages.end(); i++)
         {
            ClientSubscription* sub = dynamic_cast<ClientSubscription*>(*i);
            if (sub && sub->matches(notify))
            {
               sub->process(notify);
               break;
            }
         }
      }
   }
}
#endif


void 
Dialog::makeRequest(SipMessage& request, MethodTypes method)
{
   RequestLine rLine(method);

   rLine.uri() = mRemoteTarget.uri();
   
   request.header(h_RequestLine) = rLine;
   request.header(h_To) = mRemoteNameAddr;   
//   request.header(h_To).param(p_tag) = mId.getRemoteTag();
   request.header(h_From) = mLocalNameAddr;   
//   request.header(h_From).param(p_tag) = mId.getLocalTag(); 

   request.header(h_CallId) = mCallId;

   request.remove(h_RecordRoutes);  //!dcm! -- all of this is rather messy

   request.remove(h_Contacts);   
   request.header(h_Contacts).push_front(mLocalContact);   
   request.header(h_CSeq).method() = method;
   request.header(h_MaxForwards).value() = 70;

   //must keep old via for cancel
   if (method != CANCEL)
   {
      request.header(h_Routes) = mRouteSet;
      request.remove(h_Vias);      
      Via via;
      via.param(p_branch); // will create the branch
      request.header(h_Vias).push_front(via);
   }
   else
   {
      assert(request.exists(h_Vias));
   }
   //don't increment CSeq for ACK or CANCEL
   if (method != ACK && method != CANCEL)
   {
      request.header(h_CSeq).sequence() = ++mLocalCSeq;
   }
   DebugLog ( << "Dialog::makeRequest: " << request );
}

void 
Dialog::makeCancel(SipMessage& request)
{
   //minimal for cancel
   request.header(h_RequestLine).method() = CANCEL;   
   request.header(h_CSeq).method() = CANCEL;
   request.remove(h_Requires);
   request.remove(h_ProxyRequires);
   assert(request.exists(h_Vias));

   //not sure of these
   request.header(h_To).remove(p_tag);   
   request.remove(h_RecordRoutes);
   request.remove(h_Contacts);   
   request.header(h_Contacts).push_front(mLocalContact);   
   request.header(h_MaxForwards).value() = 70;
}

void 
Dialog::makeResponse(SipMessage& response, const SipMessage& request, int code)
{
   assert( code >= 100 );
   response.remove(h_Contacts);   
   if (code < 300 && code > 100)
   {
      assert(request.isRequest());
      assert(request.header(h_RequestLine).getMethod() == INVITE ||
             request.header(h_RequestLine).getMethod() == SUBSCRIBE ||
             request.header(h_RequestLine).getMethod() == BYE ||
             request.header(h_RequestLine).getMethod() == CANCEL ||
             request.header(h_RequestLine).getMethod() == REFER ||
             request.header(h_RequestLine).getMethod() == MESSAGE ||
             request.header(h_RequestLine).getMethod() == NOTIFY ||
             request.header(h_RequestLine).getMethod() == INFO ||
             request.header(h_RequestLine).getMethod() == OPTIONS 
             );
      
//      assert (request.header(h_RequestLine).getMethod() == CANCEL ||  // Contact header is not required for Requests that do not form a dialog
//		      request.header(h_RequestLine).getMethod() == BYE ||
//		      request.header(h_Contacts).size() == 1);
      Helper::makeResponse(response, request, code, mLocalContact);
      response.header(h_To).param(p_tag) = mId.getLocalTag();
   }
   else
   {
      Helper::makeResponse(response, request, code);
      response.header(h_To).param(p_tag) = mId.getLocalTag();

   }
   DebugLog ( << "Dialog::makeResponse: " << response);   
}


ClientInviteSession*
Dialog::makeClientInviteSession(const SipMessage& response)
{
   InviteSessionCreator* creator = dynamic_cast<InviteSessionCreator*>(mDialogSet.getCreator());
   assert(creator); // !jf! this maybe can assert by evil UAS
   //return mDum.createAppClientInviteSession(*this, *creator);
   return new ClientInviteSession(mDum, *this, creator->getLastRequest(), 
                                  creator->getInitialOffer(), creator->getServerSubscription());
}



ClientSubscription*
Dialog::makeClientSubscription(const SipMessage& request)
{
   return new ClientSubscription(mDum, *this, request);
}


ServerInviteSession*
Dialog::makeServerInviteSession(const SipMessage& request)
{
   return new ServerInviteSession(mDum, *this, request);
}

ServerSubscription* 
Dialog::makeServerSubscription(const SipMessage& request)
{
   return new ServerSubscription(mDum, *this, request);
}

Dialog::Exception::Exception(const Data& msg, const Data& file, int line)
   : BaseException(msg, file, line)
{
}

void
Dialog::update(const SipMessage& msg)
{
}

#if 0
void 
Dialog::setLocalContact(const NameAddr& localContact)
{
   mLocalContact = localContact;
}

void 
Dialog::setRemoteTarget(const NameAddr& remoteTarget)
{
   mRemoteTarget = remoteTarget;
}
#endif

void Dialog::possiblyDie()
{
   if (!mDestroying)
   {
      if (mClientSubscriptions.empty() &&
          mServerSubscriptions.empty() &&
          !mInviteSession)
      {
         delete this;
      }
   }   
}

bool
Dialog::matches(const SipMessage& msg)
{
   //currently only responses are passed to this method
   if (msg.isRequest())
   {
      return false;
   }

   switch (msg.header(h_CSeq).method())
   {
      case INVITE:
      case CANCEL:
      case REFER: 
      case BYE:
         if (mInviteSession == 0)
         {
            return false;
         }
         else
         {
            return msg.getTransactionId() == mInviteSession->mLastRequest.getTransactionId();
         }
         break;               
      case INFO:
         if (mInviteSession == 0)
         {
            return false;
         }
         else
         {
            return msg.getTransactionId() == mInviteSession->mLastNit.getTransactionId();
         }
         break;
      case SUBSCRIBE:
         if (mClientSubscriptions.empty())
         {
            return false;
         }
         else
         {
            for (std::list<ClientSubscription*>::iterator it = mClientSubscriptions.begin();
                 it != mClientSubscriptions.end(); it++)
            {
               return msg.getTransactionId() == (*it)->mLastRequest.getTransactionId();
            }
         }
         break;
      case NOTIFY:
         if (mServerSubscriptions.empty())
         {
            return false;
         }
         else
         {
            for (std::list<ServerSubscription*>::iterator it = mServerSubscriptions.begin();
                 it != mServerSubscriptions.end(); it++)
            {
               return msg.getTransactionId() == (*it)->mLastNotify.getTransactionId();
            }
         }
         break;
      default:
         return false;
   }
   return false;   
}

ostream& 
resip::operator<<(ostream& strm, const Dialog& dialog)
{

   return strm;
}

