#if !defined(RESIP_DNSRESULT_HXX)
#define RESIP_DNSRESULT_HXX

#include <iosfwd>
#include <set>
#include <vector>
#include <deque>
#include <map>

#include "resiprocate/os/Tuple.hxx"
#include "resiprocate/Transport.hxx"
#include "resiprocate/Uri.hxx"
#include "resiprocate/os/HeapInstanceCounter.hxx"

struct hostent;

namespace resip
{
class DnsInterface;
class DnsHandler;

class DnsResult
{
   public:
      RESIP_HeapCount(DnsResult);
      DnsResult(DnsInterface& interfaceObj, DnsHandler* handler);
      ~DnsResult();

      typedef enum
      {
         Available, // A result is available now
         Pending,   // More results may be pending 
         Finished,  // No more results available and none pending
         Destroyed  // the associated transaction has been deleted
      } Type;

      // Starts a lookup.  Has the rules for determining the transport
      // from a uri as per rfc3263 and then does a NAPTR lookup or an A
      // lookup depending on the uri
      void lookup(const Uri& uri);

      // Check if there are tuples available now. Will load new tuples in if
      // necessary at a lower priority. 
      Type available();
      
      // return the next tuple available for this query. The caller should have
      // checked available() before calling next()
      Tuple next();

      // return the target of associated query
      Data target() const { return mTarget; }
      
      // Will delete this DnsResult if no pending queries are out there or wait
      // until the pending queries get responses and then delete
      void destroy();
      
      // Used to store a NAPTR result
      class NAPTR
      {
         public:
            NAPTR();
            // As defined by RFC3263
            bool operator<(const NAPTR& rhs) const;

            Data key; // NAPTR record key
            
            int order;
            int pref;
            Data flags;
            Data service;
            Data regex;
            Data replacement;
      };
      
      class SRV
      {
         public:
            SRV();
            // As defined by RFC3263, RFC2782
            bool operator<(const SRV& rhs) const;
            
            Data key; // SRV record key
            
            TransportType transport;
            int priority;
            int weight;
            int cumulativeWeight; // for picking 
            int port;
            Data target;
      };

   private:

      // Given a transport and port from uri, return the default port to use
      int getDefaultPort(TransportType transport, int port);
      
      // performs host record lookups (A and AAAA) on target. 
      // May be asynchronous if ares is used. 
      // Otherwise, load the results into mResults
      void lookupARecords(const Data& target);

      void lookupAAAARecords(const Data& target);

      // performs a NAPTR lookup on mTarget. May be asynchronous if ares is
      // used. Otherwise, load the results into mResults
      void lookupNAPTR();

      void lookupNAPTR(const Data& target);

      // peforms an SRV lookup on target. May be asynchronous if ares is
      // used. Otherwise, load the results into mResults
      void lookupSRV(const Data& target);
    
      // process a NAPTR record as per rfc3263
      void processNAPTR(int status, const unsigned char* abuf, int alen);

      // process an SRV record as per rfc3263 and rfc2782. There may be more
      // than one SRV dns request outstanding at a time
      void processSRV(int status, const unsigned char* abuf, int alen);

      // process the result of a AAAA query
      void processAAAA(int status, const unsigned char* abuf, int alen);
      //
      // process an A record as per rfc3263
      void processHost(int status, const struct hostent* result);
      
      // compute the cumulative weights for the SRV entries with the lowest
      // priority, then randomly pick according to RFC2782 from the entries with
      // the lowest priority based on weights. When all entries at the lowest
      // priority are chosen, pick the next lowest priority and repeat. After an
      // SRV entry is selected, remove it from mSRVResults
      SRV retrieveSRV();
      
      // Will retrieve the next SRV record and compute the prime the mResults
      // with the appropriate Tuples. 
      void primeResults();
      
      // this will parse any ADDITIONAL records from the dns result and stores
      // them in the DnsResult. Only keep additional SRV records where
      // mNAPTRResults contains a record with replacement is that SRV
      // Store all A records for convenience in mARecords 
      // and all AAAA records for convenience in mAAAARecords 
      const unsigned char* parseAdditional(const unsigned char* aptr, 
                                           const unsigned char* abuf,
                                           int alen);


      // Some utilities for parsing dns results
      static const unsigned char* skipDNSQuestion(const unsigned char *aptr,
                                                  const unsigned char *abuf,
                                                  int alen);
      static const unsigned char* parseSRV(const unsigned char *aptr,
                                           const unsigned char *abuf, 
                                           int alen,
                                           SRV& srv);

#ifdef USE_IPV6
      static const unsigned char* parseAAAA(const unsigned char *aptr,
                                           const unsigned char *abuf, 
                                           int alen,
                                           in6_addr * aaaa);
#endif

      static const unsigned char* parseNAPTR(const unsigned char *aptr,
                                             const unsigned char *abuf, 
                                             int alen,
                                             NAPTR& naptr);

    static const unsigned char * parseCNAME(const unsigned char *aptr,
                                            const unsigned char *abuf, 
                                            int alen,
                                            Data& trueName);

   private:
      DnsInterface& mInterface;
      DnsHandler* mHandler;
      int mSRVCount;
      bool mSips;
      Data mTarget;
      Data mSrvKey;
      TransportType mTransport; // current
      int mPort; // current
      Type mType;

      //Ugly hack
      Data mPassHostFromAAAAtoA;

      void transition(Type t);      
      
      // This is where the current pending (ordered) results are stored. As they
      // are retrieved by calling next(), they are popped from the front of the list
      std::deque<Tuple> mResults;
      
      // The best NAPTR record. Only one NAPTR record will be selected
      NAPTR mPreferredNAPTR;

      // used in determining the next SRV record to use as per rfc2782
      int mCumulativeWeight; // for current priority

      // All SRV records sorted in order of preference
      std::vector<SRV> mSRVResults;
      
      // All cached A records associated with this query/queries
	  typedef std::vector<struct in_addr> InAddrList;
      std::map<Data,InAddrList > mARecords;

#ifdef USE_IPV6
      // All cached AAAA records associated with this query/queries
	  typedef std::vector<struct in6_addr> InAddr6List;
      std::map<Data,InAddr6List> mAAAARecords;
#endif

      friend class DnsInterface;
      friend std::ostream& operator<<(std::ostream& strm, const DnsResult&);
      friend std::ostream& operator<<(std::ostream& strm, const DnsResult::SRV&);
      friend std::ostream& operator<<(std::ostream& strm, const DnsResult::NAPTR&);
};

std::ostream& operator<<(std::ostream& strm, const DnsResult&);
std::ostream& operator<<(std::ostream& strm, const DnsResult::SRV&);
std::ostream& operator<<(std::ostream& strm, const DnsResult::NAPTR&);

}

#endif
//  Copyright (c) 2003, Jason Fischl 
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

