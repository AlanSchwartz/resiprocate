#include <cassert>
#include "DumTimeout.hxx"

using namespace resip;

const unsigned long DumTimeout::StaleCallTimeout = 3600*2; //2 hrs

DumTimeout::DumTimeout(Type type, unsigned long duration, BaseUsageHandle targetBu, int seq, int altSeq)
    : mType(type),
      mDuration(duration),
      mUsageHandle(targetBu),
      mSeq(seq),
      mSecondarySeq(altSeq)
{
}

DumTimeout::DumTimeout(const DumTimeout& source)
    : mType(source.mType),
      mDuration(source.mDuration),
      mUsageHandle(source.mUsageHandle),
      mSeq(source.mSeq),
      mSecondarySeq(source.mSecondarySeq)
{
}

DumTimeout::~DumTimeout()
{
}

Message*
DumTimeout::clone() const
{
   return new DumTimeout(*this);
}

      
DumTimeout::Type 
DumTimeout::type() const
{
   return mType;
}

int 
DumTimeout::seq() const
{
   return mSeq;
}

int 
DumTimeout::secondarySeq() const
{
   return mSecondarySeq;
}
      
const Data& 
DumTimeout::getTransactionId() const
{
   assert(0);
   return Data::Empty;
}

bool 
DumTimeout::isClientTransaction() const
{
   assert(0);
   return false;
}
      
Data 
DumTimeout::brief() const
{
   Data data;
   DataStream strm(data);
   strm << "DumTimeout: " << mType << " : " << mDuration << "," << mSeq << "," << mSecondarySeq ;
   return data;
}

std::ostream& 
DumTimeout::encode(std::ostream& strm) const
{
   strm << brief();
   return strm;
}

BaseUsageHandle 
DumTimeout::getBaseUsage() const
{
   return mUsageHandle;
}

   

