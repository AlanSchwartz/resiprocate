#if !defined(RESIP_HEADERS_HXX)
#define RESIP_HEADERS_HXX 

#include "resiprocate/ParserCategories.hxx"
#include "resiprocate/HeaderTypes.hxx"
#include "resiprocate/Symbols.hxx"
#include "resiprocate/os/Data.hxx"
#include "resiprocate/os/HeapInstanceCounter.hxx"

namespace resip
{
class SipMessage;

//#define PARTIAL_TEMPLATE_SPECIALIZATION
#ifdef PARTIAL_TEMPLATE_SPECIALIZATION
template<bool>
class TypeIf
{
   public:
      template <class T>
      class Resolve
      {
         public:
            typedef T Type;
      };
};

class UnusedHeader
{
};

class TypeIf<false>
{
   public:
      template <class T>
      class Resolve
      {
         public:
            typedef UnusedHeader Type;
      };
};

#define UnusedChecking(_enum)                                           \
      typedef TypeIf<Headers::_enum != Headers::UNKNOWN> TypeIfT;       \
      typedef TypeIfT::Resolve<Type> Resolver;                          \
      typedef Resolver::Type UnknownReturn

#define MultiUnusedChecking(_enum)                                              \
      typedef TypeIf<Headers::_enum != Headers::UNKNOWN> TypeIfT;               \
      typedef TypeIfT::Resolve< ParserContainer<Type> > Resolver;               \
      typedef Resolver::Type UnknownReturn

#else

#define UnusedChecking(_enum) typedef int _dummy
#define MultiUnusedChecking(_enum) typedef int _dummy

#endif

class HeaderBase
{
   public:
      virtual Headers::Type getTypeNum() const = 0;
      virtual void merge(SipMessage&, const SipMessage&)=0;
};

#define defineHeader(_enum, _name, _type, _rfc)                 \
class H_##_enum : public HeaderBase                             \
{                                                               \
   public:                                                      \
      RESIP_HeapCount(H_##_enum);                               \
      enum {Single = true};                                     \
      typedef _type Type;                                       \
      UnusedChecking(_enum);                                    \
      static Type& knownReturn(ParserContainerBase* container); \
      virtual Headers::Type getTypeNum() const;                 \
      virtual void merge(SipMessage&, const SipMessage&);       \
      H_##_enum();                                              \
};                                                              \
extern H_##_enum h_##_enum

#define defineMultiHeader(_enum, _name, _type, _rfc)            \
class H_##_enum##s : public HeaderBase                          \
{                                                               \
   public:                                                      \
      RESIP_HeapCount(H_##_enum##s);                            \
      enum {Single = false};                                    \
      typedef ParserContainer<_type> Type;                      \
      MultiUnusedChecking(_enum);                               \
      static Type& knownReturn(ParserContainerBase* container); \
      virtual Headers::Type getTypeNum() const;                 \
      virtual void merge(SipMessage&, const SipMessage&);       \
      H_##_enum##s();                                           \
};                                                              \
extern H_##_enum##s h_##_enum##s

//====================
// Token:
//====================
typedef ParserContainer<Token> Tokens;

defineHeader(ContentDisposition, "Content-Disposition", Token, "RFC 3261");
defineHeader(ContentEncoding, "Content-Encoding", Token, "RFC 3261");
defineHeader(MIMEVersion, "Mime-Version", Token, "RFC 3261");
defineHeader(Priority, "Priority", Token, "RFC 3261");
defineHeader(Event, "Event", Token, "RFC 3265");
defineHeader(SubscriptionState, "Subscription-State", Token, "RFC 3265");

defineHeader(SIPETag, "SIP-ETag", Token, "RFC 3903");
defineHeader(SIPIfMatch, "SIP-If-Match", Token, "RFC 3903");
defineHeader(ContentId, "Content-ID", Token, "RFC 2045");

defineMultiHeader(AllowEvents, "Allow-Events", Token, "RFC 3265");

defineHeader(Identity, "Identity", StringCategory, "draft-sip-identity-03");
// explicitly declare to avoid h_AllowEventss, ugh
extern H_AllowEventss h_AllowEvents;

defineMultiHeader(AcceptEncoding, "Accept-Encoding", Token, "RFC 3261");
defineMultiHeader(AcceptLanguage, "Accept-Language", Token, "RFC 3261");
defineMultiHeader(Allow, "Allow", Token, "RFC 3261");
defineMultiHeader(ContentLanguage, "Content-Language", Token, "RFC 3261");
defineMultiHeader(ProxyRequire, "Proxy-Require", Token, "RFC 3261");
defineMultiHeader(Require, "Require", Token, "RFC 3261");
defineMultiHeader(Supported, "Supported", Token, "RFC 3261");
defineMultiHeader(Unsupported, "Unsupported", Token, "RFC 3261");
defineMultiHeader(SecurityClient, "Security-Client", Token, "RFC 3329");
defineMultiHeader(SecurityServer, "Security-Server", Token, "RFC 3329");
defineMultiHeader(SecurityVerify, "Security-Verify", Token, "RFC 3329");
// explicitly declare to avoid h_SecurityVerifys, ugh
extern H_SecurityVerifys h_SecurityVerifies;

defineMultiHeader(RequestDisposition, "Request-Disposition", Token, "RFC 3841");
defineMultiHeader(Reason, "Reason", Token, "RFC 3326");
defineMultiHeader(Privacy, "Privacy", Token, "RFC 3323");
// explicitly declare to avoid h_Privacys
extern H_Privacys h_Privacies;

defineMultiHeader(PMediaAuthorization, "P-Media-Authorization", Token, "RFC 3313");

//====================
// Mime
//====================
typedef ParserContainer<Mime> Mimes;

defineMultiHeader(Accept, "Accept", Mime, "RFC 3261");
defineHeader(ContentType, "Content-Type", Mime, "RFC 3261");

//====================
// GenericURIs:
//====================
typedef ParserContainer<GenericURI> GenericURIs;
defineMultiHeader(CallInfo, "Call-Info", GenericURI, "RFC 3261");
defineMultiHeader(AlertInfo, "Alert-Info", GenericURI, "RFC 3261");
defineMultiHeader(ErrorInfo, "Error-Info", GenericURI, "RFC 3261");
defineHeader(IdentityInfo, "Identity-Info", GenericURI, "draft-sip-identity-03");

//====================
// NameAddr:
//====================
typedef ParserContainer<NameAddr> NameAddrs;

defineMultiHeader(RecordRoute, "Record-Route", NameAddr, "RFC 3261");
defineMultiHeader(Route, "Route", NameAddr, "RFC 3261");
defineMultiHeader(Contact, "Contact", NameAddr, "RFC 3261");
defineHeader(From, "From", NameAddr, "RFC 3261");
defineHeader(To, "To", NameAddr, "RFC 3261");
defineHeader(ReplyTo, "Reply-To", NameAddr, "RFC 3261");
defineHeader(ReferTo, "Refer-To", NameAddr, "RFC 3515");
defineHeader(ReferredBy, "Referred-By", NameAddr, "RFC 3892");

defineMultiHeader(Path, "Path", NameAddr, "RFC 3327");
defineMultiHeader(AcceptContact, "Accept-Contact", NameAddr, "RFC 3841");
defineMultiHeader(RejectContact, "Reject-Contact", NameAddr, "RFC 3841");
defineMultiHeader(PPreferredIdentity, "P-Preferred-Identity", NameAddr, "RFC 3325");
// explicitly declare to avoid h_PAssertedIdentitys
extern H_PPreferredIdentitys h_PPreferredIdentities;

defineMultiHeader(PAssertedIdentity, "P-Asserted-Identity", NameAddr, "RFC 3325");
// explicitly declare to avoid h_PAssertedIdentitys
extern H_PAssertedIdentitys h_PAssertedIdentities;

defineHeader(PCalledPartyId, "P-Called-Party-ID", NameAddr, "RFC 3455");
defineMultiHeader(PAssociatedUri, "P-Associated-URI", NameAddr, "RFC 3455");
defineMultiHeader(ServiceRoute, "Service-Route", NameAddr, "RFC 3608");

//====================
// StringCategory:
//====================
typedef ParserContainer<StringCategory> StringCategories;

defineHeader(ContentTransferEncoding, "Content-Transfer-Encoding", StringCategory, "RFC ?");
defineHeader(Organization, "Organization", StringCategory, "RFC 3261");
defineHeader(Server, "Server", StringCategory, "RFC 3261");
defineHeader(Subject, "Subject", StringCategory, "RFC 3261");
defineHeader(UserAgent, "User-Agent", StringCategory, "RFC 3261");
defineHeader(Timestamp, "Timestamp", StringCategory, "RFC 3261");

//====================
// IntegerCategory:
//====================
typedef ParserContainer<IntegerCategory> IntegerCategories;

// !dlb! not clear this needs to be exposed
defineHeader(ContentLength, "Content-Length", IntegerCategory, "RFC 3261");
defineHeader(MaxForwards, "Max-Forwards", IntegerCategory, "RFC 3261");
defineHeader(MinExpires, "Min-Expires", IntegerCategory, "RFC 3261");
defineHeader(RSeq, "RSeq", IntegerCategory, "RFC 3261");

// !dlb! this one is not quite right -- can have (comment) after field value
defineHeader(RetryAfter, "Retry-After", IntegerCategory, "RFC 3261");
defineHeader(Expires, "Expires", ExpiresCategory, "RFC 3261");
defineHeader(SessionExpires, "Session-Expires", ExpiresCategory, "Session Timer draft");
defineHeader(MinSE, "Min-SE", ExpiresCategory, "Session Timer draft");

//====================
// CallId:
//====================
defineHeader(CallID, "Call-ID", CallID, "RFC 3261");
defineHeader(Replaces, "Replaces", CallID, "RFC 3261");
defineHeader(InReplyTo, "In-Reply-To", CallID, "RFC 3261");

typedef H_CallID H_CallId; // code convention compatible
extern H_CallId h_CallId; // code convention compatible

defineHeader(Join, "Join", CallId, "RFC 3911");
defineHeader(TargetDialog, "Target-Dialog", CallId, "Target Dialog draft");


//====================
// Auth:
//====================
typedef ParserContainer<Auth> Auths;
defineHeader(AuthenticationInfo, "Authentication-Info", Auth, "RFC 3261");
defineMultiHeader(Authorization, "Authorization", Auth, "RFC 3261");
defineMultiHeader(ProxyAuthenticate, "Proxy-Authenticate", Auth, "RFC 3261");
defineMultiHeader(ProxyAuthorization, "Proxy-Authorization", Auth, "RFC 3261");
defineMultiHeader(WWWAuthenticate, "Www-Authenticate", Auth, "RFC 3261");

//====================
// CSeqCategory:
//====================
defineHeader(CSeq, "CSeq", CSeqCategory, "RFC 3261");

//====================
// DateCategory:
//====================
defineHeader(Date, "Date", DateCategory, "RFC 3261");

//====================
// WarningCategory:
//====================
defineMultiHeader(Warning, "Warning", WarningCategory, "RFC 3261");

//====================
// Via
//====================
typedef ParserContainer<Via> Vias;
defineMultiHeader(Via, "Via", Via, "RFC 3261");

//====================
// RAckCategory
//====================
defineHeader(RAck, "RAck", RAckCategory, "RFC 3262");

//====================
// special first line accessors
//====================
class RequestLineType {};
extern RequestLineType h_RequestLine;

class StatusLineType {};
extern StatusLineType h_StatusLine;
 
}

#undef defineHeader
#undef defineMultiHeader

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
