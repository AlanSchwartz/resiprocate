#if !defined(RESIP_CLIENTAUTHMANAGER_HXX)
#define RESIP_CLIENTAUTHMANAGER_HXX

#include "resiprocate/dum/DialogSetId.hxx"
#include "resiprocate/dum/UserProfile.hxx"

#include <map>
#include <functional>

namespace resip
{

class Auth;
class SipMessage;

class ClientAuthManager
{
   public:
      ClientAuthManager();

      // return true if request is authorized
      bool handle(UserProfile& userProfile, SipMessage& origRequest, const SipMessage& response);
      void addAuthentication(SipMessage& origRequest);
      
   private:
      friend class DialogSet;
      void dialogSetDestroyed(const DialogSetId& dsId);      

      class CompareAuth  : public std::binary_function<const Auth&, const Auth&, bool>
      {
         public:
            bool operator()(const Auth& lhs, const Auth& rhs) const;
      };      
         
      typedef enum
      {
         Invalid,
         Cached,
         Current,
         Failed
      } State;      

      class AuthState
      {
         public:     
            AuthState();
            
            void clear();

            //could work to iterator pointing to digest credential
            typedef std::map<Auth, UserProfile::DigestCredential, CompareAuth > CredentialMap;            
            CredentialMap proxyCredentials;
            CredentialMap wwwCredentials;            

            State state;            
            unsigned int cnonceCount;
            Data cnonce;
            Data cnonceCountString;            
      };      
         

      typedef std::map<DialogSetId, AuthState> AttemptedAuthMap;

      bool handleAuthHeader(UserProfile& userProfile,
                            const Auth& auth, 
                            AttemptedAuthMap::iterator authState, 
                            SipMessage& origRequest, 
                            const SipMessage& response, 
                            bool proxy);      

      AttemptedAuthMap mAttemptedAuths;      
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
