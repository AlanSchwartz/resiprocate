
#include <cassert>
#include <fstream>
#include <ostream>

#include "rutil/Logger.hxx"
#include "resip/stack/Security.hxx"

#include "TestSupport.hxx"


using namespace resip;
using namespace std;

#define RESIPROCATE_SUBSYSTEM Subsystem::TEST


int
main(int argc, char* argv[])
{
   Log::initialize(Log::Cout, Log::Debug, Data::Empty);

#ifdef USE_SSL
   Security* security=0;
   try
   {
      security = new Security;
   }
   catch( ... )
   {
      security = 0;
      ErrLog( << "Got a exception setting up Security" );
      return -1;
   }

   try
   {
      assert(security != 0);
      security->preload();
   }
   catch( ... )
   {
      ErrLog( << "Got a exception loading certificates" );
      return -1;
   }

   assert( security );

#if 0
   Data in1("sip:alice@atlanta.example.com"
            "|sip:bob@biloxi.example.org"
            "|a84b4c76e66710"
            "|314159 INVITE"
            //":314159 INVITE"
            "|Thu, 21 Feb 2002 13:02:03 GMT"
            "|sip:alice@pc33.atlanta.example.com"
            "|v=0\r\n"
            "o=UserA 2890844526 2890844526 IN IP4 pc33.atlanta.example.com\r\n"
            "s=Session SDP\r\n"
            "c=IN IP4 pc33.atlanta.example.com\r\n"
            "t=0 0\r\n"
            "m=audio 49172 RTP/AVP 0\r\n"
            "a=rtpmap:0 PCMU/8000\r\n");

   Data in2("sip:bob@biloxi.example.org"
            "|sip:alice@atlanta.example.com"
            "|a84b4c76e66710"
            "|231 BYE"
            "|Thu, 21 Feb 2002 14:19:51 GMT"
            "|"
            "|" 
            //"|\r\n" 
      );

   Data& in=in1;
   
   ofstream strm("identity-in", std::ios_base::trunc);
   strm.write( in.data(), in.size() );
   strm.flush();
   
   Data res = security->computeIdentity( Data("atlanta.example.com"), in );

   ErrLog( << "input is encoded " << in.charEncoded()  );
   ErrLog( << "input is hex " << in.hex() );
   ErrLog( << "input is  " << in );
   ErrLog( << "identity is " << res  );

   if (true)
   {
      bool c  = security->checkIdentity( Data("atlanta.example.com"), in , res );
      
      if ( !c )
      {
         ErrLog( << "Identity check failed" << res  );
         return -1;
      }
   }
#endif

   // start the second test 
     Data txt1 = 
        "INVITE sip:bob@biloxi.exmple.org SIP/2.0\r\n"
        "Via: SIP/2.0/TLS pc33.atlanta.example.com;branch=z9hG4bKnashds8\r\n"
        "To: Bob <sip:bob@biloxi.example.org>\r\n"
        "From: Alice <sip:alice@atlanta.example.com>;tag=1928301774\r\n"
        "Call-ID: a84b4c76e66710\r\n"
        "CSeq: 314159 INVITE\r\n"
        "Max-Forwards: 70\r\n"
        "Date: Thu, 21 Feb 2002 13:02:03 GMT\r\n"
        "Contact: <sip:alice@pc33.atlanta.example.com>\r\n"
        "Content-Type: application/sdp\r\n"
        "Content-Length: 147\r\n"
        "\r\n"
        "v=0\r\n"
        "o=UserA 2890844526 2890844526 IN IP4 pc33.atlanta.example.com\r\n"
        "s=Session SDP\r\n"
        "c=IN IP4 pc33.atlanta.example.com\r\n"
        "t=0 0\r\n"
        "m=audio 49172 RTP/AVP 0\r\n"
        "a=rtpmap:0 PCMU/8000\r\n";
     
     Data txt2 = 
        "BYE sip:alice@pc33.atlanta.example.com SIP/2.0\r\n"
        "Via: SIP/2.0/TLS 192.0.2.4;branch=z9hG4bKnashds10\r\n"
        "Max-Forwards: 70\r\n"
        "From: Bob <sip:bob@biloxi.example.org>;tag=a6c85cf\r\n"
        "To: Alice <sip:alice@atlanta.example.com>;tag=1928301774\r\n"
        "Date: Thu, 21 Feb 2002 14:19:51 GMT\r\n"
        "Call-ID: a84b4c76e66710\r\n"
        "CSeq: 231 BYE\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
     
      auto_ptr<SipMessage> msg(TestSupport::makeMessage(txt1));

      try
      {
         const Data& domain = msg->header(h_From).uri().host();
         msg->header(h_Identity).value() = security->computeIdentity( domain,
                                                                       msg->getCanonicalIdentityString());
      }
      catch (Security::Exception& e)
      {
         ErrLog (<< "Couldn't add identity header: " << e);
         msg->remove(h_Identity);
      }
      
      ErrLog( << "identity is " <<  msg->header(h_Identity).value() );
#endif // use_ssl 

   return 0;
}

/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */
