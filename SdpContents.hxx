#if !defined(RESIP_SDPCONTENTS_HXX)
#define RESIP_SDPCONTENTS_HXX 

#include <vector>
#include <list>
#include <iosfwd>

#include "resiprocate/Contents.hxx"
#include "resiprocate/Uri.hxx"
#include "resiprocate/os/Data.hxx"
#include "resiprocate/os/HashMap.hxx"
#include "resiprocate/os/HeapInstanceCounter.hxx"

namespace resip
{

class SdpContents;

class AttributeHelper
{
   public:
      RESIP_HeapCount(AttributeHelper);
      AttributeHelper();
      AttributeHelper(const AttributeHelper& rhs);
      AttributeHelper& operator=(const AttributeHelper& rhs);
      
      bool exists(const Data& key) const;
      const std::vector<Data>& getValues(const Data& key) const;
      std::ostream& encode(std::ostream& s) const;
      void parse(ParseBuffer& pb);
      void addAttribute(const Data& key, const Data& value = Data::Empty);
      void clearAttribute(const Data& key);
   private:
      HashMap< Data, std::vector<Data> > mAttributes;
};

class SdpContents : public Contents
{
   public:
      RESIP_HeapCount(SdpContents);
      typedef enum {IP4=1, IP6} AddrType;
      static const SdpContents Empty;
      
      class Session;

      class Session 
      {
         public:
            class Medium;

            class Codec
            {
               public:
                  Codec() : mName(), mRate(0), mPayloadType(-1) {}
                  Codec(const Data& name, unsigned long rate, const Data& parameters = Data::Empty);
                  Codec(const Data& name, int payloadType, int rate=8000);
                  Codec(const Codec& rhs);
                  Codec& operator=(const Codec& codec);

                  void parse(ParseBuffer& pb, 
                             const SdpContents::Session::Medium& medium, 
                             int payLoadType);

                  const Data& getName() const;
                  int getRate() const;

                  int payloadType() const {return mPayloadType;}
                  int& payloadType() {return mPayloadType;}

                  const Data& parameters() const {return mParameters;}
                  Data& parameters() {return mParameters;}

                  static const Codec ULaw_8000;
                  static const Codec ALaw_8000;
                  static const Codec G729_8000;
                  static const Codec TelephoneEvent;
                  static const Codec FrfDialedDigit;

                  typedef HashMap<int, Codec> CodecMap;
                  // "static" payload types as defined in RFC 3551.
                  // Maps payload type (number) to Codec definition.
                  static CodecMap& getStaticCodecs();

               private:
                  Data mName;
                  unsigned long mRate;
                  int mPayloadType;
                  Data mParameters;

                  static CodecMap sStaticCodecs;
                  static bool sStaticCodecsCreated;
                  friend bool operator==(const Codec&, const Codec&);
                  friend std::ostream& operator<<(std::ostream&, const Codec&);
            };

            class Origin
            {
               public:
                  Origin(const Data& user,
                         const unsigned long long& sessionId,
                         const unsigned long long& version,
                         AddrType addr,
                         const Data& address);
                  Origin(const Origin& rhs);
                  Origin& operator=(const Origin& rhs);

                  void parse(ParseBuffer& pb);
                  std::ostream& encode(std::ostream&) const;

                  const unsigned long long& getSessionId() const {return mSessionId;}
                  unsigned long long& getSessionId() { return mSessionId; }

                  const unsigned long long& getVersion() const {return mVersion;}
                  unsigned long long& getVersion() { return mVersion; }
                  const Data& user() const {return mUser;}
                  Data& user() {return mUser;}
                  AddrType getAddressType() const {return mAddrType;}
                  const Data& getAddress() const {return mAddress;}
                  void setAddress(const Data& host, AddrType type = IP4);

               private:
                  Origin();

                  Data mUser;
                  unsigned long long mSessionId;
                  unsigned long long mVersion;
                  AddrType mAddrType;
                  Data mAddress;

                  friend class Session;
            };

            class Email
            {
               public:
                  Email(const Data& address,
                        const Data& freeText);

                  Email(const Email& rhs);
                  Email& operator=(const Email& rhs);

                  void parse(ParseBuffer& pb);
                  std::ostream& encode(std::ostream&) const;

                  const Data& getAddress() const {return mAddress;}
                  const Data& getFreeText() const {return mFreeText;}

               private:
                  Email() {}

                  Data mAddress;
                  Data mFreeText;

                  friend class Session;
            };

            class Phone
            {
               public:
                  Phone(const Data& number,
                        const Data& freeText);
                  Phone(const Phone& rhs);
                  Phone& operator=(const Phone& rhs);

                  void parse(ParseBuffer& pb);
                  std::ostream& encode(std::ostream&) const;

                  const Data& getNumber() const {return mNumber;}
                  const Data& getFreeText() const {return mFreeText;}

               private:
                  Phone() {}

                  Data mNumber;
                  Data mFreeText;

                  friend class Session;
            };

            class Connection
            {
               public:
                  Connection(AddrType addType,
                             const Data& address,
                             unsigned long ttl = 0);
                  Connection(const Connection& rhs);
                  Connection& operator=(const Connection& rhs);
                  
                  void parse(ParseBuffer& pb);
                  std::ostream& encode(std::ostream&) const;

                  AddrType getAddressType() const {return mAddrType;}
                  const Data& getAddress() const {return mAddress;}
                  void setAddress(const Data& host, AddrType type = IP4);
                  unsigned long ttl() const {return mTTL;}
                  unsigned long& ttl() {return mTTL;}

               private:
                  Connection();

                  AddrType mAddrType;
                  Data mAddress;
                  unsigned long mTTL;

                  friend class Session;
                  friend class Medium;
            };

            class Bandwidth
            {
               public:
                  Bandwidth(const Data& modifier,
                            unsigned long kbPerSecond);
                  Bandwidth(const Bandwidth& rhs);
                  Bandwidth& operator=(const Bandwidth& rhs);
                  
                  void parse(ParseBuffer& pb);
                  std::ostream& encode(std::ostream&) const;

                  const Data& modifier() const {return mModifier;}
                  Data modifier() {return mModifier;}
                  unsigned long kbPerSecond() const {return mKbPerSecond;}
                  unsigned long& kbPerSecond() {return mKbPerSecond;}

               private:
                  Bandwidth() {}
                  Data mModifier;
                  unsigned long mKbPerSecond;

                  friend class Session;
                  friend class Medium;
            };

            class Time
            {
               public:
                  Time(unsigned long start,
                       unsigned long stop);
                  Time(const Time& rhs);
                  Time& operator=(const Time& rhs);
                  
                  void parse(ParseBuffer& pb);
                  std::ostream& encode(std::ostream&) const;

                  class Repeat
                  {
                     public:
                        Repeat(unsigned long interval,
                               unsigned long duration,
                               std::vector<int> offsets);
                        void parse(ParseBuffer& pb);
                        std::ostream& encode(std::ostream&) const;

                        unsigned long getInterval() const {return mInterval;}
                        unsigned long getDuration() const {return mDuration;}
                        const std::vector<int> getOffsets() const {return mOffsets;}
                        
                     private:
                        Repeat() {}
                        unsigned long mInterval;
                        unsigned long mDuration;
                        std::vector<int> mOffsets;

                        friend class Time;
                  };

                  void addRepeat(const Repeat& repeat);

                  unsigned long getStart() const {return mStart;}
                  unsigned long getStop() const {return mStop;}
                  const std::vector<Repeat>& getRepeats() const {return mRepeats;}

               private:
                  Time() {}
                  unsigned long mStart;
                  unsigned long mStop;
                  std::vector<Repeat> mRepeats;

                  friend class Session;
            };

            class Timezones
            {
               public:
                  class Adjustment
                  {
                     public:
                        Adjustment(unsigned long time,
                                   int offset);
                        Adjustment(const Adjustment& rhs);
                        Adjustment& operator=(const Adjustment& rhs);
                        
                        unsigned long time;
                        int offset;
                  };

                  Timezones();
                  Timezones(const Timezones& rhs);
                  Timezones& operator=(const Timezones& rhs);
                  
                  void parse(ParseBuffer& pb);
                  std::ostream& encode(std::ostream&) const;

                  void addAdjustment(const Adjustment& adjusment);
                  const std::vector<Adjustment>& getAdjustments() const {return mAdjustments; }
               private:
                  std::vector<Adjustment> mAdjustments;
            };

            class Encryption
            {
               public:
                  typedef enum {NoEncryption = 0, Prompt, Clear, Base64, UriKey} KeyType;
                  Encryption(const KeyType& method,
                             const Data& key);
                  Encryption(const Encryption& rhs);
                  Encryption& operator=(const Encryption& rhs);
                  
                  
                  void parse(ParseBuffer& pb);
                  std::ostream& encode(std::ostream&) const;

                  const KeyType& getMethod() const {return mMethod;}
                  const KeyType& method() const {return mMethod;}
                  KeyType& method() {return mMethod;}
                  const Data& getKey() const {return mKey;}
                  const Data& key() const {return mKey;}
                  Data& key() {return mKey;}

                  Encryption();
               private:
                  KeyType mMethod;
                  Data mKey;
            };

            class Medium
            {
               public:
                  Medium();
                  Medium(const Medium& rhs);
                  Medium(const Data& name,
                         unsigned long port,
                         unsigned long multicast,
                         const Data& protocol);
                  Medium& operator=(const Medium& rhs);
                  
                  void parse(ParseBuffer& pb);
                  std::ostream& encode(std::ostream&) const;
                  
                  void addFormat(const Data& format);
                  void setConnection(const Connection& connection);
                  void addConnection(const Connection& connection);
                  void setBandwidth(const Bandwidth& bandwidth);
                  void addBandwidth(const Bandwidth& bandwidth);
                  void addAttribute(const Data& key, const Data& value = Data::Empty);

                  const Data& name() const {return mName;}
                  Data name() {return mName;}

                  int port() const {return mPort;}
                  unsigned long& port() {return mPort;}
                  int multicast() const {return mMulticast;}
                  unsigned long& multicast() {return mMulticast;}
                  const Data& protocol() const {return mProtocol;}
                  Data& protocol() {return mProtocol;}

                  // preferred codec/format interface
                  const std::vector<Codec>& codecs() const;
                  std::vector<Codec>& codecs();
                  void clearCodecs();
                  void addCodec(const Codec& codec);

                  const std::vector<Data>& getFormats() const {return mFormats;}
                  const Data& information() const {return mInformation;}
                  Data& information() {return mInformation;}
                  const std::vector<Bandwidth>& bandwidths() const {return mBandwidths;}
                  std::vector<Bandwidth>& bandwidths() {return mBandwidths;}

                  // from session if empty
                  const std::vector<Connection> getConnections() const;
                  // does not include session connections
                  std::vector<Connection>& getMediumConnections() {return mConnections;}
                  const Encryption& getEncryption() const {return mEncryption;}
                  const Encryption& encryption() const {return mEncryption;}
                  Encryption& encryption() {return mEncryption;}
                  bool exists(const Data& key) const;
                  const std::vector<Data>& getValues(const Data& key) const;
                  void clearAttribute(const Data& key);

               private:
                  void setSession(Session* session);
                  Session* mSession;

                  Data mName;
                  unsigned long mPort;
                  unsigned long mMulticast;
                  Data mProtocol;
                  mutable std::vector<Data> mFormats;
                  mutable std::vector<Codec> mCodecs;
                  Data mTransport;
                  Data mInformation;
                  std::vector<Connection> mConnections;
                  std::vector<Bandwidth> mBandwidths;
                  Encryption mEncryption;
                  mutable AttributeHelper mAttributeHelper;

                  mutable bool mRtpMapDone;
                  typedef HashMap<int, Codec> RtpMap;
                  mutable RtpMap mRtpMap;

                  friend class Session;
            };

            Session(int version,
                    const Origin& origin,
                    const Data& name);

            Session() : mVersion(0) {}
            Session(const Session& rhs);
            Session& operator=(const Session& rhs);

            void parse(ParseBuffer& pb);
            std::ostream& encode(std::ostream&) const;

            int version() const {return mVersion;}
            int& version() {return mVersion;}
            const Origin& origin() const {return mOrigin;}
            Origin& origin() {return mOrigin;}
            const Data& name() const {return mName;}
            Data& name() {return mName;}
            const Data& information() const {return mInformation;}
            Data& information() {return mInformation;}
            const Uri& uri() const {return mUri;}
            Uri& uri() {return mUri;}
            const std::vector<Email>& getEmails() const {return mEmails;}
            const std::vector<Phone>& getPhones() const {return mPhones;}
            const Connection& connection() const {return mConnection;}
            Connection& connection() {return mConnection;} // !dlb! optional?
            bool isConnection() { return mConnection.mAddress != Data::Empty; }
            const std::vector<Bandwidth>& bandwidths() const {return mBandwidths;}
            std::vector<Bandwidth>& bandwidths() {return mBandwidths;}
            const std::vector<Time>& getTimes() const {return mTimes;}
            const Timezones& getTimezones() const {return mTimezones;}
            const Encryption& getEncryption() const {return mEncryption;}
            const Encryption& encryption() const {return mEncryption;}
           Encryption& encryption() {return mEncryption;}
            const std::vector<Medium>& media() const {return mMedia;}
            std::vector<Medium>& media() {return mMedia;}
            
            void addEmail(const Email& email);
            void addPhone(const Phone& phone);
            void addBandwidth(const Bandwidth& bandwidth);
            void addTime(const Time& t);
            void addMedium(const Medium& medium);
            void clearAttribute(const Data& key);
            void addAttribute(const Data& key, const Data& value = Data::Empty);
            bool exists(const Data& key) const;
            const std::vector<Data>& getValues(const Data& key) const;

         private:            
            int mVersion;
            Origin mOrigin;
            Data mName;
            std::vector<Medium> mMedia;

            // applies to all Media where unspecified
            Data mInformation;
            Uri mUri;            
            std::vector<Email> mEmails;
            std::vector<Phone> mPhones;
            Connection mConnection;
            std::vector<Bandwidth> mBandwidths;
            std::vector<Time> mTimes;
            Timezones mTimezones;
            Encryption mEncryption;
            AttributeHelper mAttributeHelper;

            friend class SdpContents;
      };

      SdpContents();
      SdpContents(HeaderFieldValue* hfv, const Mime& contentTypes);
      ~SdpContents();
      
      SdpContents(const SdpContents& rhs);
      SdpContents& operator=(const SdpContents& rhs);

      virtual Contents* clone() const;

      Session& session() {checkParsed(); return mSession;}
      const Session& session() const {checkParsed(); return mSession;}

      virtual std::ostream& encodeParsed(std::ostream& str) const;
      virtual void parse(ParseBuffer& pb);
      static const Mime& getStaticType() ;

      static bool init();

   private:
      SdpContents(const Data& data, const Mime& contentTypes);
      Session mSession;
};

static bool invokeSdpContentsInit = SdpContents::init();

typedef SdpContents::Session::Codec Codec;

bool operator==(const SdpContents::Session::Codec& lhs, 
                const SdpContents::Session::Codec& rhs);

std::ostream& operator<<(std::ostream& str, const SdpContents::Session::Codec& codec);

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
 *    notice, this std::vector of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this std::vector of conditions and the following disclaimer in
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
