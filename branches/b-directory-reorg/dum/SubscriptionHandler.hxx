#if !defined(RESIP_SUBSCRIPTIONHANDLER_HXX)
#define RESIP_SUBSCRIPTIONHANDLER_HXX

#include "dum/Handles.hxx"
#include "resiprocate/Mime.hxx"
#include "resiprocate/contents/Contents.hxx"

namespace resip
{
class SipMessage;
class SecurityAttributes;

class ClientSubscriptionHandler
{
  public:
      virtual ~ClientSubscriptionHandler() { }

      virtual void onRefreshRejected(ClientSubscriptionHandle, const SipMessage& rejection)=0;

      //Client must call acceptUpdate or rejectUpdate for any onUpdateFoo
      virtual void onUpdatePending(ClientSubscriptionHandle, const SipMessage& notify)=0;
      virtual void onUpdateActive(ClientSubscriptionHandle, const SipMessage& notify)=0;
      //unknown Subscription-State value
      virtual void onUpdateExtension(ClientSubscriptionHandle, const SipMessage& notify)=0;      

      virtual int onRequestRetry(ClientSubscriptionHandle, int retrySeconds, const SipMessage& notify)=0;
      
      //subscription can be ended through a notify or a failure response.
      virtual void onTerminated(ClientSubscriptionHandle, const SipMessage& msg)=0;   
      //not sure if this has any value.
      virtual void onNewSubscription(ClientSubscriptionHandle, const SipMessage& notify)=0;
};

class ServerSubscriptionHandler
{
  public:   
      virtual ~ServerSubscriptionHandler() {}

      virtual void onNewSubscription(ServerSubscriptionHandle, const SipMessage& sub)=0;
      virtual void onRefresh(ServerSubscriptionHandle, const SipMessage& sub);
      virtual void onPublished(ServerSubscriptionHandle associated, 
                               ServerPublicationHandle publication, 
                               const Contents* contents,
                               const SecurityAttributes* attrs);

      virtual void onNotifyRejected(ServerSubscriptionHandle, const SipMessage& msg);      

      //called when this usage is destroyed for any reason. One of the following
      //three methods will always be called before this, but this is the only
      //method that MUST be implemented by a handler
      virtual void onTerminated(ServerSubscriptionHandle)=0;

      //will be called when a NOTIFY is not delivered(with a usage terminating
      //statusCode), or the Dialog is destroyed
      virtual void onError(ServerSubscriptionHandle, const SipMessage& msg);      

      //app can synchronously decorate terminating NOTIFY messages. The only
      //graceful termination mechanism is expiration, but the client can
      //explicity end a subscription with an Expires header of 0.
      virtual void onExpiredByClient(ServerSubscriptionHandle, const SipMessage& sub, SipMessage& notify);
      virtual void onExpired(ServerSubscriptionHandle, SipMessage& notify);

      virtual bool hasDefaultExpires() const;
      virtual int getDefaultExpires() const;

      const Mimes& getSupportedMimeTypes() const;
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
