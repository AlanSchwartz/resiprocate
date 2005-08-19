#include "rutil/FlowId.hxx"
#include "rutil/ParseBuffer.hxx"
#include "resip/sip/Symbols.hxx"
#include "resip/sip/Transport.hxx"
#include <iostream>

using namespace resip;
using namespace std;

FlowId::FlowId(const Tuple& t) :
   transport(t.transport),
   connectionId(t.connectionId)
{
   assert(transport);
   assert(connectionId);
}

FlowId::FlowId(const Data& d)
{
//   ParseBuffer pb(d.base64decode());
   ParseBuffer pb(d);

   const char* anchor = pb.position();
   pb.skipToChar(Symbols::COLON[0]);

   {
      Data intData;
      pb.data(intData, anchor);
      DataStream str(intData);

      // !kh!
      // needs a reference for stream extraction operator
      // str >> (void*) transport;
      void* p = 0;
      str >> p;
      transport = reinterpret_cast<Transport*>(p);

      cerr << "IntData is: " << "[" << intData << "]" << endl;

      cerr << "Transport now: " << transport << endl;
   }

   pb.skipChar();

   anchor = pb.position();
   if (pb.eof())
   {
      pb.fail(__FILE__, __LINE__, "missing connectionId component of FlowId");
   }
   pb.skipToEnd();
   {
      Data intData;
      pb.data(intData, anchor);
      connectionId = intData.convertInt();
   }
}

Tuple 
FlowId::makeConnectionTuple() const
{
   Tuple tup = transport->getTuple();
   //.dcm.  Transports tuple doesn't point to transport 
   tup.transport = transport;
   tup.connectionId = connectionId;
   return tup;   
}

Tuple&
FlowId::pointTupleToFlow(Tuple& t) const
{
   t.transport = transport;
   t.connectionId = connectionId;
   return t;
}

std::ostream&
resip::operator<<(std::ostream& ostrm, const FlowId& f)
{
   Data res;
   {
      DataStream ds(res);
      ds << std::dec << f.transport << ":" << f.connectionId;
   }
//   ostrm << res.base64encode();
   ostrm << res;
   return ostrm;
}

bool 
FlowId::operator==(const FlowId& rhs) const
{
   return transport == rhs.transport && connectionId == rhs.connectionId;
}

bool 
FlowId::operator<(const FlowId& rhs) const
{
   if (transport < rhs.transport)
   {
      return true;
   }
   else if (transport > rhs.transport)
   {
      return true;
   }
   else
   {
      return connectionId < rhs.connectionId;
   }
}

