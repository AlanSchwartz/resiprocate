#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include "resiprocate/os/Data.hxx"
#include "resiprocate/Headers.hxx"
#include "resiprocate/Symbols.hxx"
#include "resiprocate/SipMessage.hxx"

// GPERF generated external routines
#include "resiprocate/HeaderHash.hxx"

#include <iostream>
using namespace std;

//int strcasecmp(const char*, const char*);
//int strncasecmp(const char*, const char*, int len);

using namespace resip;

Data Headers::HeaderNames[MAX_HEADERS+1];
bool Headers::CommaTokenizing[] = {false};
bool Headers::CommaEncoding[] = {false};

bool 
Headers::isCommaTokenizing(Type type)
{
   return CommaTokenizing[type+1];
}

bool 
Headers::isCommaEncoding(Type type)
{
   return CommaEncoding[type+1];
}

const Data&
Headers::getHeaderName(int type)
{
   return HeaderNames[type+1];
}

#define defineHeader(_enum, _name, _type)                                                                               \
Headers::Type                                                                                                           \
H_##_enum::getTypeNum() const {return Headers::_enum;}                                                                  \
                                                                                                                        \
void H_##_enum::merge(SipMessage& target, const SipMessage& embedded)                                                   \
{                                                                                                                       \
   if (embedded.exists(*this))                                                                                          \
   {                                                                                                                    \
      target.header(*this) = embedded.header(*this);                                                                    \
   }                                                                                                                    \
}                                                                                                                       \
                                                                                                                        \
H_##_enum::H_##_enum()                                                                                                  \
{                                                                                                                       \
   Headers::CommaTokenizing[Headers::_enum+1] = bool(Type::commaHandling & ParserCategory::CommasAllowedOutputMulti);   \
   Headers::CommaEncoding[Headers::_enum+1] = bool(Type::commaHandling & 2);                                            \
   Headers::HeaderNames[Headers::_enum+1] = _name;                                                                      \
}                                                                                                                       \
                                                                                                                        \
_type&                                                                                                                  \
H_##_enum::knownReturn(ParserContainerBase* container)                                                                  \
{                                                                                                                       \
   return dynamic_cast<ParserContainer<_type>*>(container)->front();                                                    \
}                                                                                                                       \
                                                                                                                        \
H_##_enum resip::h_##_enum

#define defineMultiHeader(_enum, _name, _type)                                                                                         \
   Headers::Type                                                                                                                        \
H_##_enum##s::getTypeNum() const {return Headers::_enum;}                                                                               \
                                                                                                                                        \
void H_##_enum##s::merge(SipMessage& target, const SipMessage& embedded)                                                                \
{                                                                                                                                       \
   if (embedded.exists(*this))                                                                                                          \
   {                                                                                                                                    \
      target.header(*this).append(embedded.header(*this));                                                                              \
   }                                                                                                                                    \
}                                                                                                                                       \
                                                                                                                                        \
H_##_enum##s::H_##_enum##s()                                                                                                            \
{                                                                                                                                       \
   Headers::CommaTokenizing[Headers::_enum+1] = bool(Type::value_type::commaHandling & ParserCategory::CommasAllowedOutputMulti);       \
   Headers::CommaEncoding[Headers::_enum+1] = bool(Type::value_type::commaHandling & 2);                                                \
   Headers::HeaderNames[Headers::_enum+1] = _name;                                                                                      \
}                                                                                                                                       \
                                                                                                                                        \
ParserContainer<_type>&                                                                                                                 \
H_##_enum##s::knownReturn(ParserContainerBase* container)                                                                               \
{                                                                                                                                       \
   return *dynamic_cast<ParserContainer<_type>*>(container);                                                                            \
}                                                                                                                                       \
                                                                                                                                        \
H_##_enum##s resip::h_##_enum##s

defineHeader(ContentDisposition, "Content-Disposition", Token);
defineHeader(ContentEncoding, "Content-Encoding", Token);
defineHeader(ContentTransferEncoding, "Content-Transfer-Encoding", StringCategory);
defineHeader(MIMEVersion, "Mime-Version", Token);
defineHeader(Priority, "Priority", Token);
defineHeader(Event, "Event", Token);
defineHeader(SubscriptionState, "Subscription-State", Token);

defineHeader(SIPETag, "SIP-ETag", Token);
defineHeader(SIPIfMatch, "SIP-If-Match", Token);

defineHeader(Identity, "Identity", Token);

defineMultiHeader(AllowEvents, "Allow-Events", Token);
// explicitly declare to avoid h_AllowEventss, ugh
H_AllowEventss resip::h_AllowEvents;

defineMultiHeader(AcceptEncoding, "Accept-Encoding", Token);
defineMultiHeader(AcceptLanguage, "Accept-Language", Token);
defineMultiHeader(Allow, "Allow", Token);
defineMultiHeader(ContentLanguage, "Content-Language", Token);
defineMultiHeader(ProxyRequire, "Proxy-Require", Token);
defineMultiHeader(Require, "Require", Token);
defineMultiHeader(Supported, "Supported", Token);
defineMultiHeader(Unsupported, "Unsupported", Token);
defineMultiHeader(SecurityClient, "Security-Client", Token);
defineMultiHeader(SecurityServer, "Security-Server", Token);
defineMultiHeader(SecurityVerify, "Security-Verify", Token);
// explicitly declare to avoid h_SecurityVerifys, ugh
H_SecurityVerifys resip::h_SecurityVerifies;

//====================
// Mime
//====================
typedef ParserContainer<Mime> Mimes;

defineMultiHeader(Accept, "Accept", Mime);
defineHeader(ContentType, "Content-Type", Mime);

//====================
// GenericURIs:
//====================
defineHeader(IdentityInfo, "Identity-Info", GenericURI);

typedef ParserContainer<GenericURI> GenericURIs;
defineMultiHeader(CallInfo, "Call-Info", GenericURI);
defineMultiHeader(AlertInfo, "Alert-Info", GenericURI);
defineMultiHeader(ErrorInfo, "Error-Info", GenericURI);

//====================
// NameAddr:
//====================
typedef ParserContainer<NameAddr> NameAddrs;

defineMultiHeader(RecordRoute, "Record-Route", NameAddr);
defineMultiHeader(Route, "Route", NameAddr);
defineMultiHeader(Contact, "Contact", NameAddr);
defineHeader(From, "From", NameAddr);
defineHeader(To, "To", NameAddr);
defineHeader(ReplyTo, "Reply-To", NameAddr);
defineHeader(ReferTo, "Refer-To", NameAddr);
defineHeader(ReferredBy, "Referred-By", NameAddr);

//====================
// String:
//====================
typedef ParserContainer<StringCategory> StringCategories;

defineHeader(Organization, "Organization", StringCategory);
defineHeader(Server, "Server", StringCategory);
defineHeader(Subject, "Subject", StringCategory);
defineHeader(UserAgent, "User-Agent", StringCategory);
defineHeader(Timestamp, "Timestamp", StringCategory);

//====================
// Integer:
//====================
typedef ParserContainer<IntegerCategory> IntegerCategories;

// !dlb! not clear this needs to be exposed
defineHeader(ContentLength, "Content-Length", IntegerCategory);
defineHeader(MaxForwards, "Max-Forwards", IntegerCategory);
defineHeader(MinExpires, "Min-Expires", IntegerCategory);
defineHeader(RSeq, "RSeq", IntegerCategory);

// !dlb! this one is not quite right -- can have (comment) after field value
defineHeader(RetryAfter, "Retry-After", IntegerCategory);
defineHeader(Expires, "Expires", ExpiresCategory);
defineHeader(SessionExpires, "Session-Expires", ExpiresCategory);
defineHeader(MinSE, "Min-SE", ExpiresCategory);

//====================
// CallId:
//====================
defineHeader(CallID, "Call-ID", CallID);
H_CallId resip::h_CallId; // code convention compatible
defineHeader(Replaces, "Replaces", CallID);
defineHeader(InReplyTo, "In-Reply-To", CallID);

//====================
// Auth:
//====================
defineHeader(AuthenticationInfo, "Authentication-Info", Auth);
defineMultiHeader(Authorization, "Authorization", Auth);
defineMultiHeader(ProxyAuthenticate, "Proxy-Authenticate", Auth);
defineMultiHeader(ProxyAuthorization, "Proxy-Authorization", Auth);
defineMultiHeader(WWWAuthenticate, "Www-Authenticate", Auth);

//====================
// CSeqCategory:
//====================
defineHeader(CSeq, "CSeq", CSeqCategory);

//====================
// DateCategory:
//====================
defineHeader(Date, "Date", DateCategory);

//====================
// WarningCategory:
//====================
defineMultiHeader(Warning, "Warning", WarningCategory);

//====================
// RAckCategory
//====================
defineHeader(RAck, "RAck", RAckCategory);

defineMultiHeader(Via, "Via", Via);

RequestLineType resip::h_RequestLine;
StatusLineType resip::h_StatusLine;

Headers::Type
Headers::getType(const char* name, int len)
{
   struct headers* p;
   p = HeaderHash::in_word_set(name, len);
   return p ? Headers::Type(p->type) : Headers::UNKNOWN;
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
