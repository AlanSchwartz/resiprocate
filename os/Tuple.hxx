#if !defined(RESIP_TUPLE_HXX)
#define RESIP_TUPLE_HXX

#include "resiprocate/os/compat.hxx"
#include "resiprocate/os/Socket.hxx"

#include "resiprocate/os/HashMap.hxx"
#include "resiprocate/os/TransportType.hxx"
#include "resiprocate/os/HeapInstanceCounter.hxx"
#include "resiprocate/os/Data.hxx"

#if !defined(WIN32)
#include <netinet/in.h>
#endif

namespace resip
{

class Transport;
struct GenericIPAddress;

// WARNING!!
// When you change this structure, make sure to update the hash function,
// operator== and operator< to be consistent with the new structure. For
// instance, the Connection* and Transport* change value in the Tuple over
// its lifetime so they must not be included in the hash or comparisons. 

typedef unsigned long ConnectionId;

class Tuple
{
   public:
      RESIP_HeapCount(Tuple);
      Tuple();
      explicit Tuple(const GenericIPAddress& genericAddress, TransportType type=UNKNOWN_TRANSPORT, 
                     const Data& targetDomain = Data::Empty);
      Tuple(const Data& printableAddress, int port, IpVersion ipVer, TransportType type=UNKNOWN_TRANSPORT, const Data& targetDomain = Data::Empty);
      Tuple(const Data& printableAddress, int port, TransportType type, const Data& targetDomain = Data::Empty);
      Tuple(const in_addr& pipv4, int pport, TransportType ptype, const Data& targetDomain = Data::Empty);
      Tuple(const sockaddr& addr, TransportType ptype, const Data& targetDomain = Data::Empty);
#ifdef USE_IPV6
      Tuple(const in6_addr& pipv6,  int pport, TransportType ptype, const Data& targetDomain = Data::Empty);
#endif
      
      // convert from a tuple to a sockaddr structure
      const sockaddr& getSockaddr() const { return mSockaddr; }
      sockaddr& getMutableSockaddr() { return mSockaddr; }

      TransportType getType() const { return mTransportType; }
      void setType(TransportType type) { mTransportType = type ;}
      void setPort(int port);
      int getPort() const;
      bool isV4() const; //!dcm! -- should deprecate asap
      IpVersion ipVersion() const;     
      bool isAnyInterface() const;
      socklen_t length() const; // of sockaddr
      
      bool operator<(const Tuple& rhs) const;
      bool operator==(const Tuple& rhs) const;
           
      Data presentationFormat() const;
      
      static TransportType toTransport( const Data& );
      static const Data& toData( TransportType );

      GenericIPAddress toGenericIPAddress() const;

      Transport* transport;
      ConnectionId connectionId;

      // special comparitors
      class AnyInterfaceCompare
      {
         public:
            bool operator()(const Tuple& x,
                            const Tuple& y) const;
      };
      friend class AnyInterfaceCompare;

      class AnyPortCompare
      {
         public:
            bool operator()(const Tuple& x,
                            const Tuple& y) const;
      };
      friend class AnyPortCompare;

      class AnyPortAnyInterfaceCompare
      {
         public:
            bool operator()(const Tuple& x,
                            const Tuple& y) const;
      };
      friend class AnyPortAnyInterfaceCompare;

      const Data& getTargetDomain() const
      {
         return mTargetDomain;
      }
      
   private:
      // !ah! needs to big enough for the IPv6 address.
      union {
            sockaddr mSockaddr;
            sockaddr_in m_anonv4;
#ifdef USE_IPV6
            sockaddr_in6 m_anonv6;
#endif
      };
      TransportType mTransportType;
      Data mTargetDomain; 

      friend std::ostream& operator<<(std::ostream& strm, const Tuple& tuple);
      friend class DnsResult;
};

}

#if  defined(__INTEL_COMPILER )

namespace std
{
size_t hash_value(const resip::Tuple& tuple);
}

#elif defined(HASH_MAP_NAMESPACE)

namespace __gnu_cxx
{

template<>
struct hash<resip::Tuple>
{
      size_t operator()(const resip::Tuple& addrPort) const;
};
 
}

#endif // hash stuff


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
