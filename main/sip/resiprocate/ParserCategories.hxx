#ifndef ParserCategories_hxx
#define ParserCategories_hxx

#include "sip2/util/Data.hxx"
#include "sip2/sipstack/ParserCategory.hxx"
#include "sip2/sipstack/ParserContainer.hxx"
#include "sip2/sipstack/HeaderFieldValue.hxx"
#include "sip2/sipstack/MethodTypes.hxx"
#include "sip2/sipstack/Symbols.hxx"
#include "sip2/sipstack/Uri.hxx"


namespace Vocal2
{

class HeaderFieldValueList;

//====================
// Token:
//====================
class Token : public ParserCategory
{
   public:
      enum {isCommaTokenizing = false};

      Token(): ParserCategory(), mValue() {}
      Token(HeaderFieldValue* hfv) : ParserCategory(hfv), mValue() {}
      Token(const Token&);
      Token& operator=(const Token&);

      Data& value() const {checkParsed(); return mValue;}

      virtual void parse(ParseBuffer& pb); // remember to call parseParameters()
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;

   private:
      mutable Data mValue;
};
typedef ParserContainer<Token> Tokens;

//====================
// Mime:
//====================
class Mime : public ParserCategory
{
   public:
      enum {isCommaTokenizing = true};

      Mime() : ParserCategory(), mType(), mSubType() {}
      Mime(HeaderFieldValue* hfv) : ParserCategory(hfv), mType(), mSubType() {}
      Mime(const Mime&);
      Mime& operator=(const Mime&);
      
      Data& type();
      Data& subType();
         
      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;
   private:
      mutable Data mType;
      mutable Data mSubType;
};
typedef ParserContainer<Mime> Mimes;

//====================
// Auth:
//====================
class Auth : public ParserCategory
{
   public:
      enum {isCommaTokenizing = false};

      Auth() : ParserCategory() {}
      Auth(HeaderFieldValue* hfv) : ParserCategory(hfv) {}
      Auth(const Auth&);
      Auth& operator=(const Auth&);

      virtual void parse(ParseBuffer& pb);
      virtual std::ostream& encode(std::ostream& str) const;
      virtual ParserCategory* clone() const;

      void parseAuthParameters(ParseBuffer& pb);
      std::ostream& encodeAuthParameters(std::ostream& str) const;

      Data& scheme();
   private:
      mutable Data mScheme;
};

//====================
// Integer:
//====================
class IntegerCategory : public ParserCategory
{
   public:
      enum {isCommaTokenizing = false};

      IntegerCategory() : ParserCategory(), mValue(0), mComment() {}
      IntegerCategory(HeaderFieldValue* hfv) : ParserCategory(hfv), mValue(0), mComment() {}
      IntegerCategory(const IntegerCategory&);
      IntegerCategory& operator=(const IntegerCategory&);

      virtual void parse(ParseBuffer& pb);
      virtual std::ostream& encode(std::ostream& str) const;
      virtual ParserCategory* clone() const;

      int& value() const {checkParsed(); return mValue;}
      Data& comment() const {checkParsed(); return mComment;}

   private:
      mutable int mValue;
      mutable Data mComment;
};

//====================
// StringCategory
//====================
class StringCategory : public ParserCategory
{
   public:
      enum {isCommaTokenizing = false};
      
      StringCategory() : ParserCategory(), mValue() {}
      StringCategory(HeaderFieldValue* hfv) : ParserCategory(hfv), mValue() {}
      StringCategory(const StringCategory&);
      StringCategory& operator=(const StringCategory&);

      virtual void parse(ParseBuffer& pb);
      virtual std::ostream& encode(std::ostream& str) const;
      virtual ParserCategory* clone() const;

      Data& value() const {checkParsed(); return mValue;}

   private:
      mutable Data mValue;
};
typedef ParserContainer<StringCategory> StringCategories;

//====================
// GenericUri:
//====================
class GenericURI : public ParserCategory
{
   public:
      enum {isCommaTokenizing = false};

      GenericURI() : ParserCategory() {}
      GenericURI(HeaderFieldValue* hfv) : ParserCategory(hfv) {}
      GenericURI(const GenericURI&);
      GenericURI& operator=(const GenericURI&);

      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;

      Data& uri();

   private:
      mutable Data mUri;
};
typedef ParserContainer<GenericURI> GenericURIs;

//====================
// NameAddr:
//====================
class NameAddr : public ParserCategory
{
   public:
      enum {isCommaTokenizing = true};

      NameAddr() : 
         ParserCategory(),
         mAllContacts(false),
         mDisplayName()
      {}

      NameAddr(HeaderFieldValue* hfv)
         : ParserCategory(hfv), 
           mAllContacts(false),
           mDisplayName()
      {}

      NameAddr(const Data& unparsed)
         : ParserCategory(unparsed),
           mAllContacts(false),
           mDisplayName()
      {}

      NameAddr(const NameAddr&);
      NameAddr& operator=(const NameAddr&);
      NameAddr(const Uri&);

      virtual ~NameAddr();
      
      Uri& uri() const;
      Data& displayName() const {checkParsed(); return mDisplayName;}
      void setAllContacts() { mAllContacts = true;}
      
      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;

      bool operator<(const NameAddr& other) const;

   protected:
      bool mAllContacts;
      mutable Uri mUri;
      mutable Data mDisplayName;

   private:
      //disallow the following parameters from being accessed in NameAddr
      //this works on gcc 3.2 so far
      using ParserCategory::param;
      Transport_Param::DType& param(const Transport_Param& paramType) const;
      Method_Param::DType& param(const Method_Param& paramType) const;
      Ttl_Param::DType& param(const Ttl_Param& paramType) const;
      Maddr_Param::DType& param(const Maddr_Param& paramType) const;
      Lr_Param::DType& param(const Lr_Param& paramType) const;
      Comp_Param::DType& param(const Comp_Param& paramType) const;
};
typedef ParserContainer<NameAddr> NameAddrs;

//====================
// CallId:
//====================
class CallId : public ParserCategory
{
   public:
      enum {isCommaTokenizing = false};

      CallId() : ParserCategory(), mValue() {}
      CallId(HeaderFieldValue* hfv) : ParserCategory(hfv), mValue() {}
      CallId(const CallId&);
      CallId& operator=(const CallId&);
      bool operator==(const CallId&) const;
      
      Data& value() const {checkParsed(); return mValue;}

      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;

   private:
      mutable Data mValue;
};
typedef ParserContainer<CallId> CallIds;

//====================
// CSeqCategory:
//====================
class CSeqCategory : public ParserCategory
{
   public:
      enum {isCommaTokenizing = false};
      
      CSeqCategory();
      CSeqCategory(HeaderFieldValue* hfv) : ParserCategory(hfv), mMethod(UNKNOWN), mSequence(-1) {}
      CSeqCategory(const CSeqCategory&);
      CSeqCategory& operator=(const CSeqCategory&);

      MethodTypes& method() const {checkParsed(); return mMethod;}
      Data& unknownMethodName() const {checkParsed(); return mUnknownMethodName;}
      int& sequence() const {checkParsed(); return mSequence;}

      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;

   private:
      mutable MethodTypes mMethod;
      mutable Data mUnknownMethodName;
      mutable int mSequence;
};

//====================
// DateCategory:
//====================

enum DayOfWeek { 
   Sun = 0,
   Mon,
   Tue,
   Wed,
   Thu,
   Fri,
   Sat
};

extern Data DayOfWeekData[];
extern Data MonthData[];

enum Month {
   Jan = 0,
   Feb,
   Mar,
   Apr,
   May,
   Jun,
   Jul,
   Aug,
   Sep,
   Oct,
   Nov,
   Dec
};

class DateCategory : public ParserCategory
{
   public:
      enum {isCommaTokenizing = false};

      DateCategory() : 
         ParserCategory(),
         mDayOfWeek(Sun),
         mDayOfMonth(),
         mMonth(Jan),
         mYear(0),
         mHour(0),
         mMin(0),
         mSec(0)
      {}

      DateCategory(HeaderFieldValue* hfv)
         : ParserCategory(hfv),
           mDayOfWeek(Sun),
           mDayOfMonth(),
           mMonth(Jan),
           mYear(0),
           mHour(0),
           mMin(0),
           mSec(0)
      {}

      DateCategory(const DateCategory&);
      DateCategory& operator=(const DateCategory&);

      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;

      virtual std::ostream& encode(std::ostream& str) const;
      
      static DayOfWeek DayOfWeekFromData(const Data&);
      static Month MonthFromData(const Data&);

      DayOfWeek& dayOfWeek() {checkParsed(); return mDayOfWeek;}
      int& dayOfMonth() {checkParsed(); return mDayOfMonth;}
      Month month() {checkParsed(); return mMonth;}
      int& year() {checkParsed(); return mYear;}
      int& hour() {checkParsed(); return mHour;}
      int& minute() {checkParsed(); return mMin;}
      int& second() {checkParsed(); return mSec;}

   private:
      mutable enum DayOfWeek mDayOfWeek;
      mutable int mDayOfMonth;
      mutable enum Month mMonth;
      mutable int mYear;
      mutable int mHour;
      mutable int mMin;
      mutable int mSec;
};

//====================
// WarningCategory:
//====================
class WarningCategory : public ParserCategory
{
   public:
      enum {isCommaTokenizing = true};

      WarningCategory() : ParserCategory() {}
      WarningCategory(HeaderFieldValue* hfv) : ParserCategory(hfv) {}
      WarningCategory(const WarningCategory&);
      WarningCategory& operator=(const WarningCategory&);

      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;

      int& code();
      Data& hostname();
      Data& text();

   private:
      mutable int mCode;
      mutable Data mHostname;
      mutable Data mText;
};

//====================
// Via:
//====================
class Via : public ParserCategory
{
   public:
      enum {isCommaTokenizing = true};

      Via() : ParserCategory(), 
              mProtocolName(Symbols::ProtocolName),
              mProtocolVersion(Symbols::ProtocolVersion),
              mTransport(Symbols::UDP),
              mSentHost(),
              mSentPort(0) 
      {
         // insert a branch in all Vias (default constructor)
         this->param(p_branch);
      }

      Via(HeaderFieldValue* hfv) : ParserCategory(hfv),
                                   mProtocolName(Symbols::ProtocolName),
                                   mProtocolVersion(Symbols::ProtocolVersion),
                                   mTransport(Symbols::UDP), // !jf! 
                                   mSentHost(),
                                   mSentPort(-1) {}
      Via(const Via&);
      Via& operator=(const Via&);

      Data& protocolName() const {checkParsed(); return mProtocolName;}
      Data& protocolVersion() const {checkParsed(); return mProtocolVersion;}
      Data& transport() const {checkParsed(); return mTransport;}
      Data& sentHost() const {checkParsed(); return mSentHost;}
      int& sentPort() const {checkParsed(); return mSentPort;}

      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;

   private:
      mutable Data mProtocolName;
      mutable Data mProtocolVersion;
      mutable Data mTransport;
      mutable Data mSentHost;
      mutable int mSentPort;
};
typedef ParserContainer<Via> Vias;
 
//====================
// RequestLine:
//====================
class RequestLine : public ParserCategory
{
   public:
      RequestLine(MethodTypes method, const Data& sipVersion = Symbols::DefaultSipVersion)
         : mMethod(method),
           mUnknownMethodName(),
           mSipVersion(sipVersion)
      {}

      RequestLine(HeaderFieldValue* hfv) 
         : ParserCategory(hfv),
           mMethod(UNKNOWN),
           mUnknownMethodName(MethodNames[UNKNOWN]),
           mSipVersion(Symbols::DefaultSipVersion)
      {}
      
      RequestLine(const RequestLine&);
      RequestLine& operator=(const RequestLine&);

      virtual ~RequestLine();

      Uri& uri() const;
      
      MethodTypes getMethod() const {checkParsed(); return mMethod;}
      Data& unknownMethodName() const {checkParsed(); return mUnknownMethodName;}
      const Data& getSipVersion() const {checkParsed(); return mSipVersion;}

      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;

   private:
      mutable Uri mUri;
      mutable MethodTypes mMethod;
      mutable Data mUnknownMethodName;
      mutable Data mSipVersion;
};

//====================
// StatusLine:
//====================
class StatusLine : public ParserCategory
{
   public:
      StatusLine() : ParserCategory(), mResponseCode(-1), mSipVersion(Symbols::DefaultSipVersion), mReason() {}
      StatusLine(HeaderFieldValue* hfv) : ParserCategory(hfv), mResponseCode(-1), mSipVersion(Symbols::DefaultSipVersion), mReason() {}
      StatusLine(const StatusLine&);
      StatusLine& operator=(const StatusLine&);

      int& responseCode() const {checkParsed(); return mResponseCode;}
      const Data& getSipVersion() const {checkParsed(); return mSipVersion;}
      Data& reason() const {checkParsed(); return mReason;}

      virtual void parse(ParseBuffer& pb);
      virtual ParserCategory* clone() const;
      virtual std::ostream& encode(std::ostream& str) const;

   private:
      mutable int mResponseCode;
      Data mSipVersion;
      mutable Data mReason;
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

/* Local Variables: */
/* c-file-style: "ellemtel" */
/* End: */
