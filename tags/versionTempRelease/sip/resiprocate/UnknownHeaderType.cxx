#include "UnknownHeaderType.hxx"
#include "HeaderTypes.hxx"

#include <cassert>
#include <string.h>
#include "sip2/util/ParseBuffer.hxx"

using namespace Vocal2;

UnknownHeaderType::UnknownHeaderType(const char* name)
{
   assert(name);
   ParseBuffer pb(name, strlen(name));
   const char* anchor = pb.skipWhitespace();
   pb.skipNonWhitespace();
   mName = pb.data(anchor);
   assert(!mName.empty());
   assert(Headers::getType(mName.data(), mName.size()) == Headers::UNKNOWN);
}
