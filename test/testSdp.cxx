#include "sip2/util/Logger.hxx"
#include "sip2/util/DataStream.hxx"
#include "sip2/sipstack/SdpContents.hxx"
#include "sip2/sipstack/HeaderFieldValue.hxx"

#include <iostream>
#include "TestSupport.hxx"
#include "tassert.h"

using namespace Vocal2;
using namespace std;

#define VOCAL_SUBSYSTEM Subsystem::APP

int
main(int argc, char* argv[])
{
    Log::Level l = Log::DEBUG;
    
    if (argc > 1)
    {
        switch(*argv[1])
        {
            case 'd': l = Log::DEBUG;
                break;
            case 'i': l = Log::INFO;
                break;
            case 's': l = Log::DEBUG_STACK;
                break;
            case 'c': l = Log::CRIT;
                break;
        }
        
    }
    
    Log::initialize(Log::COUT, l, argv[0]);
    CritLog(<<"Test Driver Starting");
    tassert_init(5);
   {
      Data txt("v=0\r\n"
               "o=alice 53655765 2353687637 IN IP4 pc33.atlanta.com\r\n"
               "s=-\r\n"
               "c=IN IP4 pc33.atlanta.com\r\n"
               "t=0 0\r\n"
               "m=audio 3456 RTP/AVP 0 1 3 99\r\n"
               "a=rtpmap:0 PCMU/8000\r\n");

      HeaderFieldValue hfv(txt.data(), txt.size());
      Mime type("application", "sdp");
      SdpContents sdp(&hfv, type);

      tassert_reset();
      
      tassert(sdp.session().getVersion() == 0);
      tassert(sdp.session().getOrigin().getUser() == "alice");
      tassert(!sdp.session().getMedia().empty());
      
      //this fails, but should probably not parse(t before c not in sdp)
      tassert(sdp.session().getMedia().front().getValue("rtpmap").front() == "0 PCMU/8000");
      tassert_verify(1);
      
   }

   {
      const char* txt = 
         ("v=0\r\n"
          "o=UserA 2890844526 2890844527 IN IP4 here.com\r\n"
          "s=Session SDP\r\n"
          "c=IN IP4 pc33.atlanta.com\r\n"
          "t=5 17\r\n"
          "m=audio 49172 RTP/AVP 0\r\n"
          "a=rtpmap:0 PCMU/8000\r\n"
          "\r\n");

      HeaderFieldValue hfv(txt, strlen(txt));
      Mime type("application", "sdp");
      SdpContents sdp(&hfv, type);
      tassert_reset();
      tassert(sdp.session().getVersion() == 0);
      tassert(sdp.session().getOrigin().getUser() == "UserA");
      tassert(sdp.session().getOrigin().getSessionId() == "2890844526");
      tassert(sdp.session().getOrigin().getVersion() == "2890844527");
      tassert(sdp.session().getOrigin().getAddressType() == SdpContents::IP4);
      tassert(sdp.session().getOrigin().getAddress() == "here.com");

      tassert(sdp.session().getName() == "Session SDP");

      tassert(sdp.session().connection().getAddressType() == SdpContents::IP4);
      tassert(sdp.session().connection().getAddress() == "pc33.atlanta.com");
      tassert(sdp.session().connection().getTTL() == 0);

      tassert(sdp.session().getTimes().front().getStart() == 5);
      tassert(sdp.session().getTimes().front().getStop() == 17);
      tassert(sdp.session().getTimes().front().getRepeats().empty());
      tassert(sdp.session().getTimezones().getAdjustments().empty());

      tassert(sdp.session().getMedia().front().getName() == "audio");
      tassert(sdp.session().getMedia().front().getPort() == 49172);
      tassert(sdp.session().getMedia().front().getMulticast() == 1);
      tassert(sdp.session().getMedia().front().getProtocol() == "RTP/AVP");
      tassert(sdp.session().getMedia().front().getFormats().front() == "0");

      tassert(sdp.session().getMedia().front().getValue("rtpmap").front() == "0 PCMU/8000");
      tassert(sdp.session().getMedia().front().exists("fuzzy") == false);
      tassert_verify(2);
      
   }

   {
      const char* txt = ("v=0\r\n"
                         "o=CiscoSystemsSIP-GW-UserAgent 3559 3228 IN IP4 192.168.2.122\r\n"
                         "s=SIP Call\r\n"
                         "c=IN IP4 192.168.2.122\r\n"
                         "t=0 0\r\n"
                         "m=audio 17124 RTP/AVP 18\r\n"
                         "a=rtpmap:18 G729/8000\r\n");

      HeaderFieldValue hfv(txt, strlen(txt));
      Mime type("application", "sdp");
      SdpContents sdp(&hfv, type);
      tassert_reset();
      tassert(sdp.session().getVersion() == 0);
      tassert(sdp.session().getOrigin().getUser() == "CiscoSystemsSIP-GW-UserAgent");
      tassert(sdp.session().getOrigin().getSessionId() == "3559");
      tassert(sdp.session().getOrigin().getVersion() == "3228");
      tassert(sdp.session().getOrigin().getAddressType() == SdpContents::IP4);
      tassert(sdp.session().getOrigin().getAddress() == "192.168.2.122");

      tassert(sdp.session().getName() == "SIP Call");

      tassert(sdp.session().connection().getAddressType() == SdpContents::IP4);
      tassert(sdp.session().connection().getAddress() == "192.168.2.122");
      tassert(sdp.session().connection().getTTL() == 0);

      tassert(sdp.session().getTimes().front().getStart() == 0);
      tassert(sdp.session().getTimes().front().getStop() == 0);
      tassert(sdp.session().getTimes().front().getRepeats().empty());
      tassert(sdp.session().getTimezones().getAdjustments().empty());

      tassert(sdp.session().getMedia().front().getName() == "audio");
      tassert(sdp.session().getMedia().front().getPort() == 17124);
      tassert(sdp.session().getMedia().front().getMulticast() == 1);
      tassert(sdp.session().getMedia().front().getProtocol() == "RTP/AVP");
      tassert(sdp.session().getMedia().front().getFormats().front() == "18");

      tassert(sdp.session().getMedia().front().getValue("rtpmap").front() == "18 G729/8000");
      tassert(sdp.session().getMedia().front().exists("fuzzy") == false);
      tassert_verify(3);
   }
   
   {
      tassert_reset();
      SdpContents sdp;
      unsigned long tm = 4058038202u;
      Data address("192.168.2.220");
      int port = 5061;
   
      Vocal2::Data sessionId((unsigned long) tm);
   
      SdpContents::Session::Origin origin("-", sessionId, sessionId, SdpContents::IP4, address);
   
      SdpContents::Session session(0, origin, "VOVIDA Session");
      
      session.connection() = SdpContents::Session::Connection(SdpContents::IP4, address);
      session.addTime(SdpContents::Session::Time(tm, 0));
      
      SdpContents::Session::Medium medium("audio", port, 0, "RTP/AVP");
      medium.addFormat("0");
      medium.addFormat("101");
      
      medium.addAttribute("rtpmap", "0 PCMU/8000");
      medium.addAttribute("rtpmap", "101 telephone-event/8000");
      medium.addAttribute("ptime", "160");
      medium.addAttribute("fmtp", "101 0-11");
      
      session.addMedium(medium);
      
      sdp.session() = session;

      Data shouldBeLike("v=0\r\n"
                        "o=- 4058038202 4058038202 IN IP4 192.168.2.220\r\n"
                        "s=VOVIDA Session\r\n"
                        "c=IN IP4 192.168.2.220 /0\r\n"
                        "t=4058038202 0\r\n"
                        "m=audio 5061 RTP/AVP 0 101\r\n"
                        "a=fmtp:101 0-11\r\n"
                        "a=ptime:160\r\n"
                        "a=rtpmap:0 PCMU/8000\r\n"
                        "a=rtpmap:101 telephone-event/8000\r\n");

      Data encoded;
      {
         DataStream s(encoded);
         s << sdp;
      }

      tassert(encoded == shouldBeLike);
      tassert_verify(4);
   }
   
   {
      Data txt("v=0\r\n"
               "o=alice 53655765 2353687637 IN IP4 pc33.atlanta.com\r\n"
               "s=-\r\n"
               "t=0 0\r\n"
               "c=IN IP4 pc33.atlanta.com\r\n"
               "m=audio 3456 RTP/AVP 0 1 3 99\r\n"
               "a=rtpmap:0 PCMU/8000\r\n");

      HeaderFieldValue hfv(txt.data(), txt.size());
      Mime type("application", "sdp");
      SdpContents sdp(&hfv, type);

      tassert(sdp.session().getVersion() == 0);
      tassert(sdp.session().getOrigin().getUser() == "alice");
      tassert(!sdp.session().getMedia().empty());
      //this fails, but should probably not parse(t before c not in sdp)
      tassert(sdp.session().getMedia().front().getValue("rtpmap").front() == "0 PCMU/8000");
      tassert_verify(5);
   }
   tassert_report();
   


}

