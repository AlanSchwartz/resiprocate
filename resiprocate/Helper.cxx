#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include <string.h>
#include <iomanip>

#include "resiprocate/Helper.hxx"
#include "resiprocate/Uri.hxx"
#include "resiprocate/Preparse.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/Random.hxx"
#include "resiprocate/os/Timer.hxx"
#include "resiprocate/os/DataStream.hxx"
#include "resiprocate/os/MD5Stream.hxx"
#include "resiprocate/os/DnsUtil.hxx"

using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::SIP

const int Helper::tagSize = 4;

SipMessage*
Helper::makeRequest(const NameAddr& target, const NameAddr& from, const NameAddr& contact, MethodTypes method)
{
   SipMessage* request = new SipMessage;
   RequestLine rLine(method);
   rLine.uri() = target.uri();
   request->header(h_To) = target;
   request->header(h_RequestLine) = rLine;
   request->header(h_MaxForwards).value() = 70;
   request->header(h_CSeq).method() = method;
   request->header(h_CSeq).sequence() = 1;
   request->header(h_From) = from;
   request->header(h_From).param(p_tag) = Helper::computeTag(Helper::tagSize);
   request->header(h_Contacts).push_front(contact);
   request->header(h_CallId).value() = Helper::computeCallId();
   //request->header(h_ContentLength).value() = 0;
   
   Via via;
   request->header(h_Vias).push_front(via);
   
   return request;
}


SipMessage*
Helper::makeRegister(const NameAddr& to,
                     const NameAddr& from,
                     const NameAddr& contact)
{
   SipMessage* request = new SipMessage;
   RequestLine rLine(REGISTER);

   rLine.uri().scheme() = to.uri().scheme();
   rLine.uri().host() = to.uri().host();
   rLine.uri().port() = to.uri().port();
   if (to.uri().exists(p_transport))
   {
      rLine.uri().param(p_transport) = to.uri().param(p_transport);
   }

   request->header(h_To) = to;
   request->header(h_RequestLine) = rLine;
   request->header(h_MaxForwards).value() = 70;
   request->header(h_CSeq).method() = REGISTER;
   request->header(h_CSeq).sequence() = 1;
   request->header(h_From) = from;
   request->header(h_From).param(p_tag) = Helper::computeTag(Helper::tagSize);
   request->header(h_CallId).value() = Helper::computeCallId();
   assert(!request->exists(h_Contacts) || request->header(h_Contacts).empty());
   request->header(h_Contacts).push_front( contact );
   
   Via via;
   request->header(h_Vias).push_front(via);
   
   return request;
}

SipMessage*
Helper::makeRegister(const NameAddr& to,
                     const Data& transport,
                     const NameAddr& contact)
{
   SipMessage* request = new SipMessage;
   RequestLine rLine(REGISTER);

   rLine.uri().scheme() = to.uri().scheme();
   rLine.uri().host() = to.uri().host();
   rLine.uri().port() = to.uri().port();
   if (!transport.empty())
   {
      rLine.uri().param(p_transport) = transport;
   }

   request->header(h_To) = to;
   request->header(h_RequestLine) = rLine;
   request->header(h_MaxForwards).value() = 70;
   request->header(h_CSeq).method() = REGISTER;
   request->header(h_CSeq).sequence() = 1;
   request->header(h_From) = to;
   request->header(h_From).param(p_tag) = Helper::computeTag(Helper::tagSize);
   request->header(h_CallId).value() = Helper::computeCallId();
   assert(!request->exists(h_Contacts) || request->header(h_Contacts).empty());
   request->header(h_Contacts).push_front( contact );
   
   Via via;
   request->header(h_Vias).push_front(via);
   
   return request;
}


SipMessage*
Helper::makePublish(const NameAddr& target, 
                      const NameAddr& from,
                      const NameAddr& contact)
{
   SipMessage* request = new SipMessage;
   RequestLine rLine(PUBLISH);
   rLine.uri() = target.uri();

   request->header(h_To) = target;
   request->header(h_RequestLine) = rLine;
   request->header(h_MaxForwards).value() = 70;
   request->header(h_CSeq).method() = PUBLISH;
   request->header(h_CSeq).sequence() = 1;
   request->header(h_From) = from;
   request->header(h_From).param(p_tag) = Helper::computeTag(Helper::tagSize);
   request->header(h_CallId).value() = Helper::computeCallId();
   assert(!request->exists(h_Contacts) || request->header(h_Contacts).empty());
   request->header(h_Contacts).push_front( contact );
   Via via;
   request->header(h_Vias).push_front(via);
   
   return request;
}


SipMessage*
Helper::makeMessage(const NameAddr& target, 
                    const NameAddr& from,
                    const NameAddr& contact)
{
   SipMessage* request = new SipMessage;
   RequestLine rLine(MESSAGE);
   rLine.uri() = target.uri();

   request->header(h_To) = target;
   request->header(h_RequestLine) = rLine;
   request->header(h_MaxForwards).value() = 70;
   request->header(h_CSeq).method() = MESSAGE;
   request->header(h_CSeq).sequence() = 1;
   request->header(h_From) = from;
   request->header(h_From).param(p_tag) = Helper::computeTag(Helper::tagSize);
   request->header(h_CallId).value() = Helper::computeCallId();
   assert(!request->exists(h_Contacts) || request->header(h_Contacts).empty());
   request->header(h_Contacts).push_front( contact );
   Via via;
   request->header(h_Vias).push_front(via);
   
   return request;
}


SipMessage*
Helper::makeSubscribe(const NameAddr& target, 
                      const NameAddr& from,
                      const NameAddr& contact)
{
   SipMessage* request = new SipMessage;
   RequestLine rLine(SUBSCRIBE);
   rLine.uri() = target.uri();

   request->header(h_To) = target;
   request->header(h_RequestLine) = rLine;
   request->header(h_MaxForwards).value() = 70;
   request->header(h_CSeq).method() = SUBSCRIBE;
   request->header(h_CSeq).sequence() = 1;
   request->header(h_From) = from;
   request->header(h_From).param(p_tag) = Helper::computeTag(Helper::tagSize);
   request->header(h_CallId).value() = Helper::computeCallId();
   assert(!request->exists(h_Contacts) || request->header(h_Contacts).empty());
   request->header(h_Contacts).push_front( contact );
   Via via;
   request->header(h_Vias).push_front(via);
   
   return request;
}


SipMessage*
Helper::makeInvite(const NameAddr& target, const NameAddr& from, const NameAddr& contact)
{
   return Helper::makeRequest(target, from, contact, INVITE);
}


SipMessage*
Helper::makeResponse(const SipMessage& request, int responseCode, const Data& reason)
{
   DebugLog(<< "Helper::makeResponse(" << request.brief() << " code=" << responseCode << " reason=" << reason);
   SipMessage* response = new SipMessage;
   response->header(h_StatusLine).responseCode() = responseCode;
   response->header(h_From) = request.header(h_From);
   response->header(h_To) = request.header(h_To);
   response->header(h_CallId) = request.header(h_CallId);
   response->header(h_CSeq) = request.header(h_CSeq);
   response->header(h_Vias) = request.header(h_Vias);
   NameAddr contact;
   response->header(h_Contacts).push_back(contact);
   
   // Only generate a To: tag if one doesn't exist.  Think Re-INVITE.
   // No totag for failure responses or 100s
   if (!response->header(h_To).exists(p_tag) && responseCode < 300 && responseCode > 100)
   {
      response->header(h_To).param(p_tag) = Helper::computeTag(Helper::tagSize);
   }

   response->setRFC2543TransactionId(request.getRFC2543TransactionId());

   //response->header(h_ContentLength).value() = 0;
   
   if (responseCode >= 180 && responseCode < 300 && request.exists(h_RecordRoutes))
   {
      response->header(h_RecordRoutes) = request.header(h_RecordRoutes);
   }

   if (request.isExternal())
   {
       response->setFromTU();
   }
   else
   {
       response->setFromExternal();
   }

   if (reason.size())
   {
      response->header(h_StatusLine).reason() = reason;
   }
   else
   {
      Data &reason(response->header(h_StatusLine).reason());
      switch (responseCode)
      {
         case 100: reason = "Trying"; break;
         case 180: reason = "Ringing"; break;
         case 181: reason = "Call Is Being Forwarded"; break;
         case 182: reason = "Queued"; break;
         case 183: reason = "Session Progress"; break;
         case 200: reason = "OK"; break;
         case 202: reason = "Accepted"; break;
         case 300: reason = "Multiple Choices"; break;
         case 301: reason = "Moved Permanently"; break;
         case 302: reason = "Moved Temporarily"; break;
         case 305: reason = "Use Proxy"; break;
         case 380: reason = "Alternative Service"; break;
         case 400: reason = "Bad Request"; break;
         case 401: reason = "Unauthorized"; break;
         case 402: reason = "Payment Required"; break;
         case 403: reason = "Forbidden"; break;
         case 404: reason = "Not Found"; break;
         case 405: reason = "Method Not Allowed"; break;
         case 406: reason = "Not Acceptable"; break;
         case 407: reason = "Proxy Authentication Required"; break;
         case 408: reason = "Request Timeout"; break;
         case 410: reason = "Gone"; break;
         case 413: reason = "Request Entity Too Large"; break;
         case 414: reason = "Request-URI Too Long"; break;
         case 415: reason = "Unsupported Media Type"; break;
         case 416: reason = "Unsupported URI Scheme"; break;
         case 420: reason = "Bad Extension"; break;
         case 421: reason = "Extension Required"; break;
         case 423: reason = "Interval Too Brief"; break;
         case 480: reason = "Temporarily Unavailable"; break;
         case 481: reason = "Call/Transaction Does Not Exist"; break;
         case 482: reason = "Loop Detected"; break;
         case 483: reason = "Too Many Hops"; break;
         case 484: reason = "Address Incomplete"; break;
         case 485: reason = "Ambiguous"; break;
         case 486: reason = "Busy Here"; break;
         case 487: reason = "Request Terminated"; break;
         case 488: reason = "Not Acceptable Here"; break;
         case 491: reason = "Request Pending"; break;
         case 493: reason = "Undecipherable"; break;
         case 500: reason = "Server Internal Error"; break;
         case 501: reason = "Not Implemented"; break;
         case 502: reason = "Bad Gateway"; break;
         case 503: reason = "Service Unavailable"; break;
         case 504: reason = "Server Time-out"; break;
         case 505: reason = "Version Not Supported"; break;
         case 513: reason = "Message Too Large"; break;
         case 600: reason = "Busy Everywhere"; break;
         case 603: reason = "Decline"; break;
         case 604: reason = "Does Not Exist Anywhere"; break;
         case 606: reason = "Not Acceptable"; break;
      }
   }

   return response;
}


SipMessage*
Helper::makeResponse(const SipMessage& request, int responseCode, const NameAddr& contact,  const Data& reason)
{

   SipMessage* response = Helper::makeResponse(request, responseCode, reason);
   response->header(h_Contacts).clear();
   response->header(h_Contacts).push_front(contact);
   return response;
}



SipMessage*
Helper::makeCancel(const SipMessage& request)
{
   assert(request.isRequest());
   assert(request.header(h_RequestLine).getMethod() == INVITE);
   SipMessage* cancel = new SipMessage;

   RequestLine rLine(CANCEL, request.header(h_RequestLine).getSipVersion());
   rLine.uri() = request.header(h_RequestLine).uri();
   cancel->header(h_RequestLine) = rLine;
   cancel->header(h_To) = request.header(h_To);
   cancel->header(h_From) = request.header(h_From);
   cancel->header(h_CallId) = request.header(h_CallId);
   if (request.exists(h_ProxyAuthorizations))
   {
      cancel->header(h_ProxyAuthorizations) = request.header(h_ProxyAuthorizations);
   }
   if (request.exists(h_Authorizations))
   {
      cancel->header(h_Authorizations) = request.header(h_Authorizations);
   }
   
   if (request.exists(h_Routes))
   {
      cancel->header(h_Routes) = request.header(h_Routes);
   }
   
   cancel->header(h_CSeq) = request.header(h_CSeq);
   cancel->header(h_CSeq).method() = CANCEL;
   cancel->header(h_Vias).push_back(request.header(h_Vias).front());

   return cancel;
}


// This interface should be used by the stack (TransactionState) to create an
// AckMsg to a failure response
// See RFC3261 section 17.1.1.3
// Note that the branch in this ACK needs to be the 
// For TU generated ACK, see Dialog::makeAck(...)
SipMessage*
Helper::makeFailureAck(const SipMessage& request, const SipMessage& response)
{
   assert (request.header(h_Vias).size() >= 1);
   assert (request.header(h_RequestLine).getMethod() == INVITE);
   
   SipMessage* ack = new SipMessage;

   RequestLine rLine(ACK, request.header(h_RequestLine).getSipVersion());
   rLine.uri() = request.header(h_RequestLine).uri();
   ack->header(h_RequestLine) = rLine;

   ack->header(h_CallId) = request.header(h_CallId);
   ack->header(h_From) = request.header(h_From);
   ack->header(h_To) = response.header(h_To); // to get to-tag
   ack->header(h_Vias).push_back(request.header(h_Vias).front());
   ack->header(h_CSeq) = request.header(h_CSeq);
   ack->header(h_CSeq).method() = ACK;
   if (request.exists(h_Routes))
   {
      ack->header(h_Routes) = request.header(h_Routes);
   }
   
   return ack;
}


Data 
Helper::computeUniqueBranch()
{
   static const Data cookie("z9hG4bK"); // magic cookie per rfc2543bis-09    

   Data result(16, true);
   result += cookie;
   result += Random::getRandomHex(4);
   result += "C1";
   result += Random::getRandomHex(2);
   return result;
}


Data
Helper::computeCallId()
{
   // !jf! need to include host as well (should cache it)
   return Random::getRandomHex(8);
}


Data
Helper::computeTag(int numBytes)
{
   return Random::getRandomHex(4);
}

//this should not be in source, it should be configurable !dcm!
static Data privateKey("lw4j5owG9A");

Data
Helper::makeNonce(const SipMessage& request, const Data& timestamp)
{
   Data nonce(100, true);
   nonce += timestamp;
   nonce += Symbols::COLON;
   Data noncePrivate(100, true);
   noncePrivate += timestamp;
   noncePrivate += Symbols::COLON;
   //!jf! don't include the Call-Id since it might not be the same. 
   //noncePrivate += request.header(h_CallId).value(); 
   noncePrivate += request.header(h_From).uri().user();
   noncePrivate += privateKey;
   nonce += noncePrivate.md5();
   return nonce;
}

//RFC 2617 3.2.2.1
Data 
Helper::makeResponseMD5(const Data& username, const Data& password, const Data& realm, 
                        const Data& method, const Data& digestUri, const Data& nonce,
                        const Data& qop, const Data& cnonce, const Data& cnonceCount)
{
   MD5Stream a1;
   a1 << username
      << Symbols::COLON
      << realm
      << Symbols::COLON
      << password;

   MD5Stream a2;
   a2 << method
      << Symbols::COLON
      << digestUri;
   
   MD5Stream r;
   r << a1.getHex()
     << Symbols::COLON
     << nonce
     << Symbols::COLON;

   if (!qop.empty()) 
   {
      r << cnonceCount
        << Symbols::COLON
        << cnonce
        << Symbols::COLON
        << qop
        << Symbols::COLON;
   }
   r << a2.getHex();

   return r.getHex();
}

// !jf! note that this only authenticates a ProxyAuthenticate header, need to
// add support for WWWAuthenticate as well!!
Helper::AuthResult
Helper::authenticateRequest(const SipMessage& request, 
                            const Data& realm,
                            const Data& password,
                            int expiresDelta)
{
   DebugLog(<< "Authenticating: realm=" << realm << " expires=" << expiresDelta);
   //DebugLog(<< request);
   
   if (request.exists(h_ProxyAuthorizations))
   {
      const ParserContainer<Auth>& auths = request.header(h_ProxyAuthorizations);
      for (ParserContainer<Auth>::const_iterator i = auths.begin(); i != auths.end(); i++)
      {
         if (i->exists(p_realm) && 
             i->exists(p_nonce) &&
             i->exists(p_response) &&
             i->param(p_realm) == realm)
         {
            ParseBuffer pb(i->param(p_nonce).data(), i->param(p_nonce).size());
            if (!pb.eof() && !isdigit(*pb.position()))
            {
               DebugLog(<< "Invalid nonce; expected timestamp.");
               return BadlyFormed;
            }
            const char* anchor = pb.position();
            pb.skipToChar(Symbols::COLON[0]);

            if (pb.eof())
            {
               DebugLog(<< "Invalid nonce; expected timestamp terminator.");
               return BadlyFormed;
            }

            Data then;
            pb.data(then, anchor);
            if (expiresDelta > 0)
            {
               int now = (int)(Timer::getTimeMs()/1000);
               if (then.convertInt() + expiresDelta < now)
               {
                  DebugLog(<< "Nonce has expired.");
                  return Expired;
               }
            }
            if (i->param(p_nonce) != makeNonce(request, then))
            {
               InfoLog(<< "Not my nonce.");
               return Failed;
            }
         
            if (i->exists(p_qop))
            {
               if (i->param(p_qop) == Symbols::auth)
               {
                  if (i->param(p_response) == makeResponseMD5(i->param(p_username), 
                                                              password,
                                                              realm, 
                                                              MethodNames[request.header(h_RequestLine).getMethod()],
                                                              i->param(p_uri),
                                                              i->param(p_nonce),
                                                              i->param(p_qop),
                                                              i->param(p_cnonce),
                                                              i->param(p_nc)))
                  {
                     return Authenticated;
                  }
                  else
                  {
                     return Failed;
                  }
               }
               else
               {
                  InfoLog (<< "Unsupported qop=" << i->param(p_qop));
                  return Failed;
               }
            }
            else if (i->param(p_response) == makeResponseMD5(i->param(p_username), 
                                                             password,
                                                             realm, 
                                                             MethodNames[request.header(h_RequestLine).getMethod()],
                                                             i->param(p_uri),
                                                             i->param(p_nonce)))
            {
               return Authenticated;
            }
            else
            {
               return Failed;
            }
         }
         else
         {
            return BadlyFormed;
         }
      }
      return BadlyFormed;
   }
   DebugLog (<< "No authentication headers. Failing request.");
   return Failed;
}

SipMessage*
Helper::make405(const SipMessage& request,
                const int* allowedMethods,
                int len )
{
    SipMessage* resp = Helper::makeResponse(request, 405);

    if (len < 0)
    {
        int upperBound = static_cast<int>(UNKNOWN);
	// The UNKNOWN method name marks the end of the enum
        
        for (int i = 0 ; i < upperBound; i ++)
        {
            int last = 0;
            // ENUMS must be contiguous in order for this to work.
            assert( i - last <= 1);
            //MethodTypes type = static_cast<MethodTypes>(i);
            Token t;
            t.value() = MethodNames[i];
            resp->header(h_Allows).push_back(t);
            last = i;
        }
    }
    else
    {
        // use user's list
        for ( int i = 0 ; i < len ; i++)
        {
            Token t;
            t.value() = MethodNames[allowedMethods[i]];
            resp->header(h_Allows).push_back(t);
        }
    }
    return resp;
}


SipMessage*
Helper::makeProxyChallenge(const SipMessage& request, const Data& realm, bool useAuth)
{
   Auth auth;
   auth.scheme() = "Digest";
   Data timestamp((int)(Timer::getTimeMs()/1000));
   auth.param(p_nonce) = makeNonce(request, timestamp);
   auth.param(p_algorithm) = "MD5";
   auth.param(p_realm) = realm;
   if (useAuth)
   {
      auth.param(p_qopOptions) = "auth,auth-int";
   }
   SipMessage *response = Helper::makeResponse(request, 407);
   response->header(h_ProxyAuthenticates).push_back(auth);
   return response;
}

void updateNonceCount(unsigned int& nonceCount, Data& nonceCountString)
{
   if (!nonceCountString.empty())
   {
      return;
   }
   nonceCount++;
   {
      DataStream s(nonceCountString);
      
      s << std::setw(8) << std::setfill('0') << std::hex << nonceCount;
   }
   DebugLog(<< "nonceCount is now: [" << nonceCountString << "]");
}

Auth makeChallengeResponseAuth(SipMessage& request,
                               const Data& username,
                               const Data& password,
                               const Auth& challenge,
                               const Data& cnonce,
                               unsigned int& nonceCount,
                               Data& nonceCountString)
{
   Auth auth;
   auth.scheme() = "Digest";
   auth.param(p_username) = username;
   assert(challenge.exists(p_realm));
   auth.param(p_realm) = challenge.param(p_realm);
   assert(challenge.exists(p_nonce));
   auth.param(p_nonce) = challenge.param(p_nonce);
   Data digestUri;
   {
      DataStream s(digestUri);
      s << request.header(h_RequestLine).uri();
   }
   auth.param(p_uri) = digestUri;

   bool useAuthQop = false;
   if (challenge.exists(p_qopOptions) && !challenge.param(p_qopOptions).empty())
   {
      ParseBuffer pb(challenge.param(p_qopOptions).data(), challenge.param(p_qopOptions).size());
      do
      {
         const char* anchor = pb.skipWhitespace();
         pb.skipToChar(Symbols::COMMA[0]);
         Data q;
         pb.data(q, anchor);
         if (q == Symbols::auth)
         {
            useAuthQop = true;
            break;
         }
      }
      while(!pb.eof());
   }
   if (useAuthQop)
   {
      updateNonceCount(nonceCount, nonceCountString);
      auth.param(p_response) = Helper::makeResponseMD5(username, 
                                                       password,
                                                       challenge.param(p_realm), 
                                                       MethodNames[request.header(h_RequestLine).getMethod()], 
                                                       digestUri, 
                                                       challenge.param(p_nonce),
                                                       Symbols::auth,
                                                       cnonce,
                                                       nonceCountString);
      auth.param(p_cnonce) = cnonce;
      auth.param(p_nc) = nonceCountString;
      auth.param(p_qop) = Symbols::auth;
   }
   else
   {
      assert(challenge.exists(p_realm));
      auth.param(p_response) = Helper::makeResponseMD5(username, 
                                                       password,
                                                       challenge.param(p_realm), 
                                                       MethodNames[request.header(h_RequestLine).getMethod()], 
                                                       digestUri, 
                                                       challenge.param(p_nonce));
   }
   
   if (challenge.exists(p_algorithm))
   {
      auth.param(p_algorithm) = challenge.param(p_algorithm);
   }
   else
   {
      auth.param(p_algorithm) = "MD5";
   }

   if (challenge.exists(p_opaque))
   {
      auth.param(p_opaque) = challenge.param(p_opaque);
   }
   
   return auth;
}
   

SipMessage& 
Helper::addAuthorization(SipMessage& request,
                         const SipMessage& challenge,
                         const Data& username,
                         const Data& password,
                         const Data& cnonce,
                         unsigned int& nonceCount)
{
   Data nonceCountString = Data::Empty;
   
   assert(challenge.isResponse());
   assert(challenge.header(h_StatusLine).responseCode() == 401 ||
          challenge.header(h_StatusLine).responseCode() == 407);

   if (challenge.exists(h_ProxyAuthenticates))
   {
      const ParserContainer<Auth>& auths = challenge.header(h_ProxyAuthenticates);
      for (ParserContainer<Auth>::const_iterator i = auths.begin();
           i != auths.end(); i++)
      {
         request.header(h_ProxyAuthorizations).push_back(makeChallengeResponseAuth(request, username, password, *i, 
                                                                                    cnonce, nonceCount, nonceCountString));
      }
   }
   if (challenge.exists(h_WWWAuthenticates))
   {
      const ParserContainer<Auth>& auths = challenge.header(h_WWWAuthenticates);
      for (ParserContainer<Auth>::const_iterator i = auths.begin();
           i != auths.end(); i++)
      {
         request.header(h_Authorizations).push_back(makeChallengeResponseAuth(request, username, password, *i,
                                                                               cnonce, nonceCount, nonceCountString));
      }
   }
   return request;
}
      
Uri
Helper::makeUri(const Data& aor, const Data& scheme)
{
   Data tmp(aor.size() + scheme.size() + 1, true);
   tmp += scheme;
   tmp += Symbols::COLON;
   tmp += aor;
   Uri uri(tmp);
   return uri;
}

void
Helper::processStrictRoute(SipMessage& request)
{
   if (request.exists(h_Routes) && 
       !request.header(h_Routes).empty() &&
       !request.header(h_Routes).front().exists(p_lr))
   {
      // The next hop is a strict router.  Move the next hop into the
      // Request-URI and move the ultimate destination to the end of the
      // route list.  Force the message target to be the next hop router.
      request.header(h_Routes).push_back(NameAddr(request.header(h_RequestLine).uri()));
      request.header(h_RequestLine).uri() = request.header(h_Routes).front().uri();
      request.header(h_Routes).pop_front();
      request.setForceTarget(request.header(h_RequestLine).uri());
   }
}

int
Helper::getPortForReply(SipMessage& request, bool sym)
{
   assert(request.isRequest());
   int port = -1;
   if (sym || request.header(h_Vias).front().exists(p_rport))
   {
       port = request.getSource().getPort();
   }
   else
   {
      port = request.header(h_Vias).front().sentPort();
      if (port <= 0 || port > 65535) 
      {
         port = Symbols::DefaultSipPort;
      }
   }
   assert(port != -1);

   // DebugLog(<<"getSentPort(): sym=" << (sym?'t':'f') << " returning: " << port);

   return port;
}

Uri 
Helper::fromAor(const Data& aor, const Data& scheme)
{
   return Uri(scheme + Symbols::COLON + aor);
}

bool
Helper::validateMessage(const SipMessage& message)
{
   if (!message.exists(h_To) || 
       !message.exists(h_From) || 
       !message.exists(h_CSeq) || 
       !message.exists(h_CallId) || 
       !message.exists(h_Vias) ||
       message.header(h_Vias).empty())
   {
      InfoLog(<< "Missing mandatory header fields (To, From, CSeq, Call-Id or Via)");
      DebugLog(<< message);
      return false;
   }
   else
   {
      return true;
   }
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
