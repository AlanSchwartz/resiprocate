#if !defined(RESIP_TCPBASETRANSPORT_HXX)
#define RESIP_TCPBASETRANSPORT_HXX

#include "resiprocate/InternalTransport.hxx"
#include "resiprocate/ConnectionManager.hxx"

namespace resip
{

class SipMessage;

class TcpBaseTransport : public InternalTransport
{
   public:
//      enum {MaxFileDescriptors = 100000};

      TcpBaseTransport(Fifo<TransactionMessage>& fifo, int portNum, const Data& sendhost, bool ipv4);
      virtual  ~TcpBaseTransport();
      
      void process(FdSet& fdset);
      void buildFdSet( FdSet& fdset);
      bool isReliable() const { return true; }
//      int maxFileDescriptors() const { return MaxFileDescriptors; }

      ConnectionManager& getConnectionManager() {return mConnectionManager;}

   protected:
      virtual Connection* createConnection(Tuple& who, Socket fd, bool server=false)=0;
      
      void processSomeWrites(FdSet& fdset);
      void processSomeReads(FdSet& fdset);
      void processAllWriteRequests(FdSet& fdset);
      void sendFromRoundRobin(FdSet& fdset);
      void processListen(FdSet& fdSet);

      static const size_t MaxWriteSize;
      static const size_t MaxReadSize;

   private:
      static const int MaxBufferSize;
      ConnectionManager mConnectionManager;
};

}


#endif
