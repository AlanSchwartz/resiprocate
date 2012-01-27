/* ***********************************************************************
   Copyright 2006-2007 Estacado Systems, LLC. All rights reserved.
 *********************************************************************** */

#ifndef RESIP_DNS_HOST_RECORD
#define RESIP_DNS_HOST_RECORD

#include <iosfwd>

#include "rutil/Data.hxx"
#include "rutil/Socket.hxx"
#include "rutil/dns/DnsResourceRecord.hxx"


namespace resip
{

class DnsResourceRecord;
class RROverlay;

class DnsHostRecord : public DnsResourceRecord
{
   public:
      DnsHostRecord(const RROverlay&);
      DnsHostRecord(const Data name, const in_addr addr): mAddr(addr), mName(name){};
      ~DnsHostRecord() {}

      // accessors.
      Data host() const;
      in_addr addr() const { return mAddr; }
      const Data& name() const { return mName; }
      bool isSameValue(const Data& value) const;
      std::ostream& dump(std::ostream& strm) const;
      
   private:
      in_addr mAddr;
      Data mName;
};

}


#endif
