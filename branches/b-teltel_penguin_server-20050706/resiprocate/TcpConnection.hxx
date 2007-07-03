#if !defined(TcpConnection_hxx)
#define TcpConnection_hxx

#include "resiprocate/Connection.hxx"

namespace resip
{

class Tuple;

class TcpConnection : public Connection
{
   public:
      TcpConnection( const Tuple& who, Socket fd );
      
      int read( char* buf, const int count );
      int write( const char* buf, const int count );
      virtual bool hasDataToRead(); // has data that can be read 
      virtual bool isGood(); // is good to read/write
      Data peerName();      
};
 
}

#endif
