#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include <set>

#include "resiprocate/os/ParseBuffer.hxx"
#include "resiprocate/os/DataStream.hxx"
#include "resiprocate/Symbols.hxx"
#include "resiprocate/Uri.hxx"
#include "resiprocate/UnknownParameter.hxx"
#include "resiprocate/ParserCategories.hxx" // !dlb! just NameAddr
#include "resiprocate/SipMessage.hxx"
#include "resiprocate/Embedded.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/ParseUtil.hxx"
#include "resiprocate/os/DnsUtil.hxx"

using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::SIP

Uri::Uri() 
   : ParserCategory(),
     mScheme(Data::Share, Symbols::DefaultSipScheme),
     mPort(0),
     mOldPort(0),
     mEmbeddedHeaders(0)
{
}

Uri::Uri(const Data& data)
   : ParserCategory(), 
     mScheme(Symbols::DefaultSipScheme),
     mPort(0),
     mOldPort(0),
     mEmbeddedHeaders(0)
{
   try
   {
      ParseBuffer pb(data.data(), data.size());
      Uri tmp;

      // avoid the destructor/constructor issue
      tmp.parse(pb);
      *this = tmp;
   }
   catch(ParseBuffer::Exception& e)
   {
      DebugLog(<< "Failed trying to construct a Uri from " << data << ": " << e);
      throw;
   }
}

Uri::Uri(const Uri& rhs)
   : ParserCategory(rhs),
     mScheme(rhs.mScheme),
     mHost(rhs.mHost),
     mUser(rhs.mUser),
     mUserParameters(rhs.mUserParameters),
     mPort(rhs.mPort),
     mPassword(rhs.mPassword),
     mOldPort(0),
     mEmbeddedHeadersText(rhs.mEmbeddedHeadersText),
     mEmbeddedHeaders(rhs.mEmbeddedHeaders ? new SipMessage(*rhs.mEmbeddedHeaders) : 0)
{}

Uri::~Uri()
{
   delete mEmbeddedHeaders;
}

// RFC 3261 19.1.6
Uri
Uri::fromTel(const Uri& tel, const Data& host)
{
   assert(tel.scheme() == Symbols::Tel);

   Uri u;
   u.scheme() = Symbols::Sip;
   u.user() = tel.user();
   u.host() = host;
   u.param(p_user) = Symbols::Phone;

   // need to sort the user parameters
   if (!tel.userParameters().empty())
   {
      DebugLog(<< "Uri::fromTel: " << tel.userParameters());
      Data isub;
      Data postd;

      int totalSize  = 0;
      std::set<Data> userParameters;

      ParseBuffer pb(tel.userParameters().data(), tel.userParameters().size());
      while (true)
      {
         const char* anchor = pb.position();
         pb.skipToChar(Symbols::SEMI_COLON[0]);
         Data param = pb.data(anchor);
         // !dlb! not supposed to lowercase extension parameters
         param.lowercase();
         totalSize += param.size() + 1;

         if (param.prefix(Symbols::Isub))
         {
            isub = param;
         }
         else if (param.prefix(Symbols::Postd))
         {
            postd = param;
         }
         else
         {
            userParameters.insert(param);
         }
         if (pb.eof())
         {
            break;
         }
         else
         {
            pb.skipChar();
         }
      }

      u.userParameters().reserve(totalSize);
      if (!isub.empty())
      {
         u.userParameters() = isub;
      }
      if (!postd.empty())
      {
         if (!u.userParameters().empty())
         {
            u.userParameters() += Symbols::SEMI_COLON[0];
         }
         u.userParameters() += postd;
      }
      
      for(std::set<Data>::const_iterator i = userParameters.begin();
          i != userParameters.end(); ++i)
      {
         DebugLog(<< "Adding param: " << *i);
         if (!u.userParameters().empty())
         {
            u.userParameters() += Symbols::SEMI_COLON[0];
         }
         u.userParameters() += *i;
      }
   }

   return u;
}

bool
Uri::hasEmbedded() const
{
   checkParsed(); 
   return !mEmbeddedHeadersText.empty() || mEmbeddedHeaders != 0;
}

Uri&
Uri::operator=(const Uri& rhs)
{
   if (this != &rhs)
   {
      ParserCategory::operator=(rhs);
      mScheme = rhs.mScheme;
      mHost = rhs.mHost;
      mUser = rhs.mUser;
      mUserParameters = rhs.mUserParameters;
      mPort = rhs.mPort;
      mPassword = rhs.mPassword;
      if (rhs.mEmbeddedHeaders != 0)
      {
         delete mEmbeddedHeaders;
         mEmbeddedHeaders = new SipMessage(*rhs.mEmbeddedHeaders);
      }
      else
      {
         mEmbeddedHeadersText = rhs.mEmbeddedHeadersText;
      }
   }
   return *this;
}

class OrderUnknownParameters
{
   public:
      OrderUnknownParameters() { notUsed=false; };
      ~OrderUnknownParameters() {};

      bool operator()(const Parameter* p1, const Parameter* p2) const
      {
         const UnknownParameter* u1 = dynamic_cast<const UnknownParameter*>(p1);
         const UnknownParameter* u2 = dynamic_cast<const UnknownParameter*>(p2);

         if (u1->getName() < 
             u2->getName())
         {
            return true;
         }

         if (u1->getName() > 
             u2->getName())
         {
            return false;
         }

         // deal with same parameter appearing more than once
         return u1->value() < u2->value();
      }

   private:
      bool notUsed;
};

bool 
Uri::operator==(const Uri& other) const
{
   checkParsed();
   other.checkParsed();

   // compare hosts
   if (DnsUtil::isIpV6Address(mHost) &&
       DnsUtil::isIpV6Address(other.mHost))
   {

      // compare canonicalized IPV6 addresses

      // update canonicalized if host changed
      if (mOldHost != mHost)
      {
         mOldHost = mHost;
         mCanonicalHost = DnsUtil::canonicalizeIpV6Address(mHost);
      }

      // update canonicalized if host changed      
      if (other.mOldHost != other.mHost)
      {
         other.mOldHost = other.mHost;
         other.mCanonicalHost = DnsUtil::canonicalizeIpV6Address(other.mHost);
      }

      if (mCanonicalHost != other.mCanonicalHost)
      {
         return false;
      }
   }
   else
   {
      if (!isEqualNoCase(mHost, other.mHost))
      {
         return false;
      }
   }
   
   if (isEqualNoCase(mScheme, other.mScheme) &&
       ((isEqualNoCase(mScheme, Symbols::Sip) || isEqualNoCase(mScheme, Symbols::Sips)) ? mUser == other.mUser : isEqualNoCase(mUser, other.mUser)) &&
       isEqualNoCase(mUserParameters,other.mUserParameters) &&
       mPassword == other.mPassword &&
       mPort == other.mPort)
   {
      for (ParameterList::iterator it = mParameters.begin(); it != mParameters.end(); ++it)
      {
         Parameter* otherParam = other.getParameterByEnum((*it)->getType());

         switch ((*it)->getType())
         {
            case ParameterTypes::user:
            {
               if (!(otherParam &&
                     isEqualNoCase(dynamic_cast<DataParameter*>(*it)->value(),
                                   dynamic_cast<DataParameter*>(otherParam)->value())))
               {
                  return false;
               }
            }
            break;
            case ParameterTypes::ttl:
            {
               if (!(otherParam &&
                     (dynamic_cast<IntegerParameter*>(*it)->value() ==
                      dynamic_cast<IntegerParameter*>(otherParam)->value())))
               {
                  return false;
               }
               break;
            }
            case ParameterTypes::method:
            {
               // this should possilby be case sensitive, but is allowed to be
               // case insensitive for robustness.  
               
               if (otherParam)
               {
                  DataParameter* dp1 = dynamic_cast<DataParameter*>(*it);
                  DataParameter* dp2 = dynamic_cast<DataParameter*>(otherParam);
                  assert(dp1);
                  assert(dp2);
               }
               if (!(otherParam &&
                     isEqualNoCase(dynamic_cast<DataParameter*>(*it)->value(),
                                   dynamic_cast<DataParameter*>(otherParam)->value())))
               {
                  return false;
               }
               break;
            }
            case ParameterTypes::maddr:
            {               
               if (!(otherParam &&
                     isEqualNoCase(dynamic_cast<DataParameter*>(*it)->value(),
                                   dynamic_cast<DataParameter*>(otherParam)->value())))
               {
                  return false;
               }
            }
            break;
            case ParameterTypes::transport:
            {
               if (!(otherParam &&
                     isEqualNoCase(dynamic_cast<DataParameter*>(*it)->value(),
                                   dynamic_cast<DataParameter*>(otherParam)->value())))
               {
                  return false;
               }
            }
            break;
            // the parameters that follow don't affect comparison if only present
            // in one of the URI's
            case ParameterTypes::lr:
               break;
            default:
               break;
               //treat as unknown parameter?
         }
      }         

      // now check the other way, sigh
      for (ParameterList::iterator it = other.mParameters.begin(); it != other.mParameters.end(); ++it)
      {
         Parameter* param = getParameterByEnum((*it)->getType());
         switch ((*it)->getType())
         {
            case ParameterTypes::user:
            {
               if (!(param &&
                     isEqualNoCase(dynamic_cast<DataParameter*>(*it)->value(),
                                   dynamic_cast<DataParameter*>(param)->value())))
               {
                  return false;
               }
            }
            break;
            case ParameterTypes::ttl:
            {
               if (!(param &&
                     (dynamic_cast<IntegerParameter*>(*it)->value() == 
                      dynamic_cast<IntegerParameter*>(param)->value())))
               {
                  return false;
               }
               break;
            }
            case ParameterTypes::method:
            {
               // this should possilby be case sensitive, but is allowed to be
               // case insensitive for robustness.  
               if (!(param &&
                     isEqualNoCase(dynamic_cast<DataParameter*>(*it)->value(),
                                   dynamic_cast<DataParameter*>(param)->value())))
               {
                  return false;
               }
            }
            break;
            case ParameterTypes::maddr:
            {               
               if (!(param &&
                     isEqualNoCase(dynamic_cast<DataParameter*>(*it)->value(),
                                   dynamic_cast<DataParameter*>(param)->value())))
               {
                  return false;
               }
            }
            break;
            case ParameterTypes::transport:
            {
               if (!(param &&
                     isEqualNoCase(dynamic_cast<DataParameter*>(*it)->value(),
                                   dynamic_cast<DataParameter*>(param)->value())))
               {
                  return false;
               }
            }
            break;
            // the parameters that follow don't affect comparison if only present
            // in one of the URI's
            case ParameterTypes::lr:
               break;
            default:
               break;
               //treat as unknown parameter?
         }
      }
   }
   else
   {
      return false;
   }

   OrderUnknownParameters orderUnknown;

#if defined(__SUNPRO_CC) || defined(WIN32)
   // The Solaris Forte STL implementation does not support the
   // notion of a list.sort() function taking a BinaryPredicate.
   // The hacky workaround is to load the Parameter pointers into
   // an STL set which does support an ordering function.

   typedef std::set<Parameter*, OrderUnknownParameters> ParameterSet;
   ParameterSet unA, unB;

   for (ParameterList::iterator i = mUnknownParameters.begin();
        i != mUnknownParameters.end(); ++i)
   {
      unA.insert(*i);
   }
   for (ParameterList::iterator i = other.mUnknownParameters.begin();
        i != other.mUnknownParameters.end(); ++i)
   {
      unB.insert(*i);
   }

   ParameterSet::iterator a = unA.begin();
   ParameterSet::iterator b = unB.begin();
#else
   // .dlb. more efficient to copy to vector for sorting?
   // Uri comparison is expensive; consider caching? ugh
   ParameterList unA = mUnknownParameters;
   ParameterList unB = other.mUnknownParameters;

   sort(unA.begin(), unA.end(), orderUnknown);
   sort(unB.begin(), unB.end(), orderUnknown);
 
   ParameterList::iterator a = unA.begin();
   ParameterList::iterator b = unB.begin();
#endif

   while(a != unA.end() && b != unB.end())
   {
      const UnknownParameter* u1 = dynamic_cast<const UnknownParameter*>(*a);
      const UnknownParameter* u2 = dynamic_cast<const UnknownParameter*>(*b);

      if (u1->getName() < 
          u2->getName())
      {
         ++a;
      }
      else if (u1->getName() >
               u2->getName())
      {
         ++b;
      }
      else
      {
         if (!isEqualNoCase(u1->value(),
                            u2->value()))
         {
            return false;
         }
         ++a;
         ++b;
      }
   }
   return true;
}

bool 
Uri::operator!=(const Uri& other) const
{
   return !(*this == other);
}

bool
Uri::operator<(const Uri& other) const
{
   other.checkParsed();
   checkParsed();
   if (mUser < other.mUser)
   {
      return true;
   }

   if (mUser > other.mUser)
   {
      return false;
   }

   if (mUserParameters < other.mUserParameters)
   {
      return true;
   }

   if (mUserParameters > other.mUserParameters)
   {
      return false;
   }

   if (mHost < other.mHost)
   {
      return true;
   }

   if (mHost > other.mHost)
   {
      return false;
   }

   return mPort < other.mPort;
}

bool 
Uri::GreaterQ::operator()(const Uri& lhs, const Uri& rhs) const
{
   if (lhs.exists(p_q) && rhs.exists(p_q))
   {
      return lhs.param(p_q) > rhs.param(p_q);
   }

   if (lhs.exists(p_q))
   {
      return lhs.param(p_q) > 1.0;
   }

   if (rhs.exists(p_q))
   {
      return rhs.param(p_q) < 1.0;
   }

   return false;
}

const Data
Uri::getAorNoPort() const
{
   checkParsed();
   getAor();

   Data aor;
   aor.reserve(mUser.size() + mCanonicalHost.size() + 2 /* for @ */ );
   
   if (mUser.empty())
   {
      aor += mCanonicalHost;
   }
   else
   {
      aor += mUser;
      if (!mCanonicalHost.empty())
      {
         aor += Symbols::AT_SIGN;
         aor += mCanonicalHost;
      }
   }
   
   return aor;
}

const Data&
Uri::getAor() const
{
   checkParsed();
   // did anything change?
   if (mOldUser != mUser ||
       mOldHost != mHost ||
       mOldPort != mPort)
   {
      mOldHost = mHost;
      // canonicalize host
      if (DnsUtil::isIpV6Address(mOldHost))
      {
         mCanonicalHost = DnsUtil::canonicalizeIpV6Address(mHost);
      }
      else
      {
         mCanonicalHost = mHost;
         mCanonicalHost.lowercase();
      }

      mOldUser = mUser;
      mOldPort = mPort;
      mAor.clear();

      //                                                  @:10000
      mAor.reserve(mUser.size() + mCanonicalHost.size() + 10);
      if (mOldUser.empty())
      {
         mAor += mCanonicalHost;
      }
      else
      {
         mAor += mOldUser;
         if (!mCanonicalHost.empty())
         {
            mAor += Symbols::AT_SIGN;
            mAor += mCanonicalHost;
         }
      }

      if (mPort != 0)
      {
         mAor += Symbols::COLON;
         mAor += Data(mPort);
      }
   }
   return mAor;
}

void
Uri::parse(ParseBuffer& pb)
{
   pb.skipWhitespace();
   const char* start = pb.position();
   pb.skipToOneOf(":@"); // make sure the colon precedes

   pb.assertNotEof();

   pb.data(mScheme, start);
   pb.skipChar(Symbols::COLON[0]);
   mScheme.lowercase();

   if (isEqualNoCase(mScheme, Symbols::Tel))
   {
      const char* anchor = pb.position();
      pb.skipToOneOf(ParseBuffer::Whitespace, ";>");
      pb.data(mUser, anchor);
      if (!pb.eof() && *pb.position() == Symbols::SEMI_COLON[0])
      {
         anchor = pb.skipChar();
         pb.skipToOneOf(ParseBuffer::Whitespace, Symbols::RA_QUOTE);
         pb.data(mUserParameters, anchor);
      }
      return;
   }
   
   start = pb.position();
   pb.skipToChar(Symbols::AT_SIGN[0]);
   if (!pb.eof())
   {
      pb.reset(start);
      start = pb.position();
      pb.skipToOneOf(":@");
      pb.data(mUser, start);
      if (!pb.eof() && *pb.position() == Symbols::COLON[0])
      {
         start = pb.skipChar();
         pb.skipToChar(Symbols::AT_SIGN[0]);
         pb.data(mPassword, start);
      }
      start = pb.skipChar();
   }
   else
   {
      pb.reset(start);
   }
   
   if (*start == '[')
   {
      start = pb.skipChar();
      pb.skipToChar(']');
      pb.data(mHost, start);
      DnsUtil::canonicalizeIpV6Address(mHost);
      pb.skipChar();
   }
   else
   {
      pb.skipToOneOf(ParseBuffer::Whitespace, ":;?>");
      pb.data(mHost, start);
   }

   pb.skipToOneOf(ParseBuffer::Whitespace, ":;?>");
   if (!pb.eof() && *pb.position() == ':')
   {
      start = pb.skipChar();
      mPort = pb.integer();
      pb.skipToOneOf(ParseBuffer::Whitespace, ";?>");
   }
   else
   {
      mPort = 0;
   }
   
   parseParameters(pb);

   if (!pb.eof() && *pb.position() == Symbols::QUESTION[0])
   {
      const char* anchor = pb.position();
      pb.skipToOneOf(Symbols::RA_QUOTE, Symbols::SEMI_COLON);
      pb.data(mEmbeddedHeadersText, anchor);
   }
}

ParserCategory*
Uri::clone() const
{
   return new Uri(*this);
}
 
// should not encode user parameters unless its a tel?
std::ostream& 
Uri::encodeParsed(std::ostream& str) const
{
   str << mScheme << Symbols::COLON; 
   if (!mUser.empty())
   {
      str << mUser;
      if (!mUserParameters.empty())
      {
         str << Symbols::SEMI_COLON[0] << mUserParameters;
      }
      if (!mPassword.empty())
      {
         str << Symbols::COLON << mPassword;
      }
   }
   if (!mHost.empty())
   {
     if (!mUser.empty())
     {
       str << Symbols::AT_SIGN;
     }
     if (ParseUtil::fastRejectIsIpV6Address(mHost))
     {
        str << '[' << mHost << ']';
     }
     else
     {
        str << mHost;
     }
   }
   if (mPort != 0)
   {
      str << Symbols::COLON << mPort;
   }
   encodeParameters(str);
   encodeEmbeddedHeaders(str);

   return str;
}

SipMessage&
Uri::embedded() const
{
   checkParsed();
   if (mEmbeddedHeaders == 0)
   {
      Uri* ncthis = const_cast<Uri*>(this);
      ncthis->mEmbeddedHeaders = new SipMessage();
      if (!mEmbeddedHeadersText.empty())
      {
         ParseBuffer pb(mEmbeddedHeadersText.data(), mEmbeddedHeadersText.size());
         ncthis->parseEmbeddedHeaders(pb);
      }
   }

   return *mEmbeddedHeaders;
}

void
Uri::parseEmbeddedHeaders(ParseBuffer& pb)
{
   DebugLog(<< "Uri::parseEmbeddedHeaders");
   if (!pb.eof() && *pb.position() == Symbols::QUESTION[0])
   {
      pb.skipChar();
   }

   const char* anchor;
   Data headerName;
   Data headerContents;

   bool first = true;
   while (!pb.eof())
   {
      if (first)
      {
         first = false;
      }
      else
      {
         pb.skipChar(Symbols::AMPERSAND[0]);
      }

      anchor = pb.position();
      pb.skipToChar(Symbols::EQUALS[0]);
      pb.data(headerName, anchor);
      // .dlb. in theory, need to decode header name

      anchor = pb.skipChar(Symbols::EQUALS[0]);
      pb.skipToChar(Symbols::AMPERSAND[0]);
      pb.data(headerContents, anchor);

      unsigned int len;
      char* decodedContents = Embedded::decode(headerContents, len);
      mEmbeddedHeaders->addBuffer(decodedContents);

      static const Data body("Body");
      if (isEqualNoCase(body, headerName))
      {
         mEmbeddedHeaders->setBody(decodedContents, len); 
      }
      else
      {
         DebugLog(<< "Uri::parseEmbeddedHeaders(" << headerName << ", " << Data(decodedContents, len) << ")");
         mEmbeddedHeaders->addHeader(Headers::getType(headerName.data(), headerName.size()),
                                     headerName.data(), headerName.size(),
                                     decodedContents, len);
      }
   }
}

std::ostream& 
Uri::encodeEmbeddedHeaders(std::ostream& str) const
{
   if (mEmbeddedHeaders)
   {
      mEmbeddedHeaders->encodeEmbedded(str);
   }
   else
   {
      // never decoded
      str << mEmbeddedHeadersText;
   }
   return str;
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
