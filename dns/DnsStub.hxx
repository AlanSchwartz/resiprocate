#ifndef RESIP_DNS_STUB_HXX
#define RESIP_DNS_STUB_HXX

#include "resiprocate/os/Inserter.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/Fifo.hxx"

#include "resiprocate/dns/RROverlay.hxx"
#include "resiprocate/dns/RRList.hxx"
#include "resiprocate/dns/RRCache.hxx"
#include "resiprocate/dns/DnsResourceRecord.hxx"
#include "resiprocate/dns/DnsNaptrRecord.hxx"
#include "resiprocate/dns/DnsAAAARecord.hxx"
#include "resiprocate/dns/DnsHostRecord.hxx"
#include "resiprocate/dns/DnsSrvRecord.hxx"
#include "resiprocate/dns/DnsHostRecord.hxx"
#include "resiprocate/dns/DnsCnameRecord.hxx"

namespace resip
{

class DnsInterface;

template<typename T>
class DNSResult
{
   public:
      Data domain;
      int status;
      int retryAfter; // in seconds.
      Data msg;
      std::vector<T> records;
};

class DnsResultSink
{
   public:
      virtual void onDnsResult(const DNSResult<DnsHostRecord>&) = 0;

#ifdef USE_IPV6
      virtual void onDnsResult(const DNSResult<DnsAAAARecord>&) = 0;
#endif

      virtual void onDnsResult(const DNSResult<DnsSrvRecord>&) = 0;
      virtual void onDnsResult(const DNSResult<DnsNaptrRecord>&) = 0;
      virtual void onDnsResult(const DNSResult<DnsCnameRecord>&) = 0;
};

class DnsRawSink
{
   public:
      virtual void onDnsRaw(int statuts, const unsigned char* abuf, int len) = 0;
};

class DnsStub
{
   public:
      typedef RRCache::Protocol Protocol;
      typedef std::vector<Data> DataArr;

      typedef std::vector<DnsResourceRecord*> DnsResourceRecordsByPtr;
      class ResultTransform
      {
         public:
            virtual ~ResultTransform() {}
            virtual void transform(const Data& target, int rrType, DnsResourceRecordsByPtr& src) = 0;
      };

      

      class DnsStubException : public BaseException
      {
         public:
            DnsStubException(const Data& msg, const Data& file, const int line)
               : BaseException(msg, file, line) 
            {
            }
            
            const char* name() const { return "DnsStubException"; }
      };

      DnsStub(DnsInterface* dns);
      ~DnsStub();

      void setResultTransform(ResultTransform*);
      void removeResultTransform();

      template<class QueryType> void lookup(const Data& target, int proto, DnsResultSink* sink)
      {
         QueryCommand<QueryType>* command = new QueryCommand<QueryType>(target, proto, sink, *this);
         mCommandFifo.add(command);
      }

      void blacklist(const Data& target, int rrType, const int proto, const DataArr& targetsToBlacklist)
      {
         BlacklistingCommand* command = new BlacklistingCommand(target, rrType, proto, *this, targetsToBlacklist);
         mCommandFifo.add(command);
      }

      void retryAfter(const Data& target, int rrType, const int proto, const int retryAfter, const DataArr& targetsToRetryAfter)
      {
         RetryAfterCommand* command = new RetryAfterCommand(target, rrType, proto, *this, retryAfter, targetsToRetryAfter);
         mCommandFifo.add(command);
      }

      void setTTL(int ttl) // in minute. 
      {
         RRCache::instance()->setTTL(ttl);
      }

      void setCacheSize(int size)
      {
         if (size > 0) RRCache::instance()->setSize(size);
      }

      void process();
      
   protected:
      void cache(const Data& key, const unsigned char* abuf, int alen);
      void cacheTTL(const Data& key, int rrType, int status, const unsigned char* abuf, int alen);

   private:
      class ResultConverter //.dcm. -- flyweight?
      {
         public:
            virtual void notifyUser(const Data& target, 
                                    int status, 
                                    int retryAfter, 
                                    const Data& msg,
                                    const DnsResourceRecordsByPtr& src,
                                    DnsResultSink* sink) = 0;
            virtual ~ResultConverter() {}
      };
      
      template<class QueryType>  
      class ResultConverterImpl : public ResultConverter
      {
         public:
            virtual void notifyUser(const Data& target, 
                                    int status, 
                                    int retryAfter, 
                                    const Data& msg,
                                    const DnsResourceRecordsByPtr& src,
                                    DnsResultSink* sink)
            {
               assert(sink);
               DNSResult<typename QueryType::Type>  result;               
               for (unsigned int i = 0; i < src.size(); ++i)
               {
                  result.records.push_back(*(dynamic_cast<typename QueryType::Type*>(src[i])));
               }
               result.domain = target;
               result.status = status;
               result.retryAfter = retryAfter;
               result.msg = msg;
               sink->onDnsResult(result);
            }
      };

      class Query : public DnsRawSink
      {
         public:
            Query(DnsStub& stub, ResultTransform* transform, ResultConverter* resultConv, 
                  const Data& target, int rrType, bool followCname, int proto, DnsResultSink* s);
            virtual ~Query();

            enum {MAX_REQUERIES = 5};

            void go(DnsInterface* dns);
            void process(int status, const unsigned char* abuf, const int alen);
            void onDnsRaw(int status, const unsigned char* abuf, int alen);
            void followCname(const unsigned char* aptr, const unsigned char*abuf, const int alen, bool& bGotAnswers, bool& bDeleteThis);

         private:
            static DnsResourceRecordsByPtr Empty;
            int mRRType;
            DnsStub& mStub;
            ResultTransform* mTransform;
            ResultConverter* mResultConverter;
            Data mTarget;
            int mProto;
            int mReQuery;
            DnsResultSink* mSink;
            DnsInterface* mDns;
            bool mFollowCname;
      };

   private:
      DnsStub(const DnsStub&);   // disable copy ctor.
      DnsStub& operator=(const DnsStub&);

      template<class QueryType>
      void query(const Data& target, int proto, DnsResultSink* sink)
      {
         Query* query = new Query(*this, mTransform, 
                                  new ResultConverterImpl<QueryType>(), 
                                  target, QueryType::getRRType(),
                                  QueryType::SupportsCName, proto, sink);
         mQueries.insert(query);
         query->go(mDns);
      }
      void doBlacklisting(const Data& target, int rrType, 
                          int protocol, const DataArr& targetsToBlacklist);
      void doRetryAfter(const Data& target, int rrType, int protocol,
                        int retryAfter, const DataArr& targetsToRetryAfter);
                          

      class Command
      {
         public:
            virtual ~Command() {}
            virtual void execute() = 0;
      };

      template<class QueryType>
      class QueryCommand : public Command
      {
         public:
            QueryCommand(const Data& target, 
                         int proto,
                         DnsResultSink* sink,
                         DnsStub& stub)
               : mTarget(target),
                 mProto(proto),
                 mSink(sink),
                 mStub(stub)
            {}

            ~QueryCommand() {}

            void execute()
            {
               mStub.query<QueryType>(mTarget, mProto, mSink);
            }

         private:
            Data mTarget;
            int mProto;
            DnsResultSink* mSink;
            DnsStub& mStub;
      };

      class BlacklistingCommand : public Command
      {
         public:
            BlacklistingCommand(const Data& target,
                                int rrType,
                                int proto,
                                DnsStub& stub,
                                const DataArr& targetToBlacklist)
               : mTarget(target),
                 mRRType(rrType),
                 mProto(proto),
                 mStub(stub),
                 mTargetsToBlacklist(targetToBlacklist)
            {}             
            ~BlacklistingCommand() {}
            void execute()
            {
               mStub.doBlacklisting(mTarget, mRRType, mProto, mTargetsToBlacklist);
            }

         private:
            Data mTarget;
            int mRRType;
            int mProto;
            DnsStub& mStub;
            DataArr mTargetsToBlacklist;
      };

      class RetryAfterCommand: public Command
      {
         public:
            RetryAfterCommand(const Data& target,
                              int rrType,
                              int proto,
                              DnsStub& stub,
                              const int retryAfter,
                              const DataArr& targetsToRetryAfter)
               : mTarget(target),
                 mRRType(rrType),
                 mProto(proto),
                 mStub(stub),
                 mRetryAfter(retryAfter),
                 mTargetsToRetryAfter(targetsToRetryAfter)
            {}
            ~RetryAfterCommand() {}
            void execute()
            {
               mStub.doRetryAfter(mTarget, mRRType, mProto, mRetryAfter, mTargetsToRetryAfter);
            }

         private:
            Data mTarget;
            int mRRType;
            int mProto;
            DnsStub& mStub;
            int mRetryAfter;
            DataArr mTargetsToRetryAfter;
      };

      resip::Fifo<Command> mCommandFifo;

      const unsigned char* skipDNSQuestion(const unsigned char *aptr,
                                           const unsigned char *abuf,
                                           int alen);
      bool supportedType(int);
      const unsigned char* createOverlay(const unsigned char* abuf, 
                                         const int alen, 
                                         const unsigned char* aptr, 
                                         std::vector<RROverlay>&,
                                         bool discard=false);
      void removeQuery(Query*);
      DnsInterface* mDns;
      ResultTransform* mTransform;
      std::set<Query*> mQueries;
};

typedef DnsStub::Protocol Protocol;

}

#endif
 
