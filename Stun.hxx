#if !defined(RESIP_STUN_HXX)
#define RESIP_STUN_HXX


namespace resip
{

class Stun
{
   public:

      // Define enum with different types of NAT 
      typedef enum 
      {
         NatTypeUnknown=0,
         NatTypeOpen,
         NatTypeConeNat,         
         NatTypeRestrictedNat,
         NatTypePortRestrictedNat,
         NatTypeSymNat,
         NatTypeSymFirewall,
         NatTypeBlocked,
         NatTypeNonBlocked, // this is the result of testI quick test.
         NatTypeNonSymNat,  // this is the result of symmetric firewall quick test.
         NatTypeOpenUnknown,// on open internet, but no idea if blocked by symmetric firewall. this is the result of symmetric nat quick test.
         NatTypeFailure
      } NatType;

      typedef struct
      {
         NatType type;
         unsigned int ip;
         unsigned short port;
         std::string msg;
      } StunResult;

      Stun() : mQuickTest(false), mSymNatTest(true) {}

      void quickTest(bool quickTest) { mQuickTest = quickTest; }
      void symNatTest(bool test) { mSymNatTest = test; }

      StunResult NATType(unsigned int destAddr,
                         unsigned short destPort,
                         bool* preservePort=0, // if set, is return for if NAT preservers ports or not
                         bool* hairpin=0 ,  // if set, is the return for if NAT will hairpin packets
                         unsigned short port=0, // port to use for the test, 0 to choose random port
                         unsigned int addr=0 // NIC to use
                         );

   private:

      static const UInt16 STUN_MAX_STRING = 256;
      static const UInt16 STUN_MAX_UNKNOWN_ATTRIBUTES = 8;

      typedef enum
      {
         TestI = 1,
         TestII = 2,
         TestIII = 3,
         TestIWithChangedIp = 10
      } StunTests;

      typedef struct 
      {
         UInt16 msgType;
         UInt16 msgLength;
         UInt128 id;
      } StunMsgHdr;

      typedef struct
      {
         UInt16 type;
         UInt16 length;
      } StunAtrHdr;
      
      typedef struct
      {
         UInt16 port;
         UInt32 addr;
      } StunAddress4;

      typedef struct
      {
         UInt8 pad;
         UInt8 family;
         StunAddress4 ipv4;
      } StunAtrAddress4;
      
      typedef struct
      {
         UInt32 value;
      } StunAtrChangeRequest;

      typedef struct
      {
         UInt16 pad; // all 0
         UInt8 errorClass;
         UInt8 number;
         char reason[STUN_MAX_STRING];
         UInt16 sizeReason;
      } StunAtrError;

      typedef struct
      {
         UInt16 attrType[STUN_MAX_UNKNOWN_ATTRIBUTES];
         UInt16 numAttributes;
      } StunAtrUnknown;

      typedef struct
      {
         char value[STUN_MAX_STRING];      
         UInt16 sizeValue;
      } StunAtrString;

      typedef struct
      {
         char hash[20];
      } StunAtrIntegrity;

      typedef enum 
      {
         HmacUnkown=0,
         HmacOK,
         HmacBadUserName,
         HmacUnkownUserName,
         HmacFailed,
      } StunHmacStatus;

      typedef struct
      {
         StunMsgHdr msgHdr;

         bool hasMappedAddress;
         StunAtrAddress4  mappedAddress;

         bool hasResponseAddress;
         StunAtrAddress4  responseAddress;

         bool hasChangeRequest;
         StunAtrChangeRequest changeRequest;

         bool hasSourceAddress;
         StunAtrAddress4 sourceAddress;
         
         bool hasChangedAddress;
         StunAtrAddress4 changedAddress;
         
         bool hasUsername;
         StunAtrString username;

         bool hasPassword;
         StunAtrString password;

         bool hasMessageIntegrity;
         StunAtrIntegrity messageIntegrity;         

         bool hasErrorCode;
         StunAtrError errorCode;

         bool hasUnknownAttributes;
         StunAtrUnknown unknownAttributes;
         
         bool hasReflectedFrom;
         StunAtrAddress4 reflectedFrom;

         bool hasXorMappedAddress;
         StunAtrAddress4  xorMappedAddress;

         bool xorOnly;
         
         bool hasServerName;
         StunAtrString serverName;
         
         bool hasSecondaryAddress;
         StunAtrAddress4 secondaryAddress;
      } StunMessage; 

      bool mQuickTest;
      bool mSymNatTest;

      bool  parseMessage(char* buf, 
                         unsigned int bufLen, 
                         StunMessage& message);      

      StunResult result(NatType type, StunAddress4 addr, char* error);
      int stunRand();
      int randomPort();
      void sendTest(Socket fd, StunAddress4& dest, const StunAtrString& username,
                    const StunAtrString& password, int testNum);
      void buildReqSimple(StunMessage* msg, const StunAtrString& username, bool changePort, bool changeIp, unsigned int id=0);
      unsigned int encodeMessage(const StunMessage& message, char* buf, unsigned int bufLen, const StunAtrString& password);
      char* encode16(char* buf, UInt16 data);
      char* encode32(char* buf, UInt32 data);
      char* encode(char* buf, const char* data, unsigned int length);
      void computeHmac(char* hmac, const char* input, int length, const char* key, int sizeKey);
      char* encodeAtrAddress4(char* ptr, UInt16 type, const StunAtrAddress4& atr);
      char* encodeAtrChangeRequest(char* ptr, const StunAtrChangeRequest& atr);
      char* encodeAtrError(char* ptr, const StunAtrError& atr);
      char* encodeAtrUnknown(char* ptr, const StunAtrUnknown& atr);
      char* encodeXorOnly(char* ptr);
      char* encodeAtrString(char* ptr, UInt16 type, const StunAtrString& atr);
      char* encodeAtrIntegrity(char* ptr, const StunAtrIntegrity& atr);
      bool parseAtrAddress( char* body, unsigned int hdrLen,  StunAtrAddress4& result );
      bool parseAtrChangeRequest( char* body, unsigned int hdrLen,  StunAtrChangeRequest& result );
      bool parseAtrError( char* body, unsigned int hdrLen,  StunAtrError& result );
      bool parseAtrUnknown( char* body, unsigned int hdrLen,  StunAtrUnknown& result );
      bool parseAtrString( char* body, unsigned int hdrLen,  StunAtrString& result );
      bool parseAtrIntegrity( char* body, unsigned int hdrLen,  StunAtrIntegrity& result );

      // udp
      Socket openPort(unsigned short port, unsigned int interfaceIp);
      bool getMessage(Socket fd, char* buf, int* len, 
                      unsigned int* srcIp, unsigned short* srcPort);
      bool sendMessage(Socket fd, char* msg, int len, unsigned int dstIp, unsigned short dstPort);

};

}


#endif
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
