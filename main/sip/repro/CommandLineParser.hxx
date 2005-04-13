#if !defined(DUM_CommandLineParser_hxx)
#define DUM_CommandLineParser_hxx

#include <vector>
#include "resiprocate/Uri.hxx"
#include "resiprocate/os/Data.hxx"

namespace resip
{

class CommandLineParser
{
   public:
      CommandLineParser(int argc, char** argv);
      static resip::Uri toUri(const char* input, const char* description);
      static std::vector<resip::Uri> toUriVector(const char* input, const char* description);
      
      Data mLogType;
      Data mLogLevel;
      Data mTlsDomain;
      int mUdpPort;
      int mTcpPort;
      int mTlsPort;
      int mDtlsPort;
      bool mUseV4;
      bool mUseV6;
      std::vector<Uri> mDomains;
      Data mCertPath;
      bool mNoChallenge;
      bool mNoRegistrar;
      bool mCertServer;
      Data mRequestProcessorChainName;
};
 
}

#endif

