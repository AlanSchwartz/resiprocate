
#include <cassert>

#include "sip2/sipstack/LazyParser.hxx"
#include "sip2/util/ParseBuffer.hxx"
#include "sip2/sipstack/ParserCategories.hxx"
#include "sip2/sipstack/Headers.hxx"

using namespace Vocal2;

LazyParser::LazyParser(HeaderFieldValue* headerFieldValue)
   : mHeaderField(headerFieldValue),
      mIsMine(false),
      mIsParsed(mHeaderField->mField == 0) //although this has a hfv it is
                                           //parsed, as the hfv has no content
{
}
  
LazyParser::LazyParser()
   : mHeaderField(0),
     mIsMine(true),
     mIsParsed(true)
{
}

LazyParser::LazyParser(const LazyParser& rhs)
   : mHeaderField(0),
     mIsMine(true),
     mIsParsed(rhs.mIsParsed)
{
   if (!mIsParsed && rhs.mHeaderField)
   {
      mHeaderField = new HeaderFieldValue(*rhs.mHeaderField);
   }
}

LazyParser::~LazyParser()
{
   clear();
}

LazyParser&
LazyParser::operator=(const LazyParser& rhs)
{
   if (this != &rhs)
   {
      clear();
      mIsParsed = rhs.mIsParsed;
      if (rhs.mIsParsed)
      {
         mHeaderField = 0;
         mIsMine = false;
      }
      else
      {
         mHeaderField = new HeaderFieldValue(*rhs.mHeaderField);
         mIsMine = true;
      }
   }
   return *this;
}

void
LazyParser::checkParsed() const
{
   if (!mIsParsed)
   {
      LazyParser* ncThis = const_cast<LazyParser*>(this);
      ncThis->mIsParsed = true;
      ParseBuffer pb(mHeaderField->mField, mHeaderField->mFieldLength);
      ncThis->parse(pb);
   }
}

void
LazyParser::clear()
{
   if (mIsMine)
   {
      delete mHeaderField;
      mHeaderField = 0;
   }
}

std::ostream&
LazyParser::encode(std::ostream& str) const
{
   if (isParsed())
   {
      return encodeParsed(str);
   }
   else
   {
      assert(mHeaderField);
      return mHeaderField->encode(str);
   }
}

std::ostream&
Vocal2::operator<<(std::ostream& s, const LazyParser& lp)
{
   lp.encode(s);
   return s; 
}
