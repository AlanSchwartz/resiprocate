#if !defined(RESIP_PIDF_HXX)
#define RESIP_PIDF_HXX 

#include <vector>

#include "resiprocate/Contents.hxx"
#include "resiprocate/os/Data.hxx"
#include "resiprocate/os/HashMap.hxx"

namespace resip
{

class Pidf : public Contents
{
   public:
      Pidf();
      Pidf(const Mime& contentType);
      Pidf(HeaderFieldValue* hfv, const Mime& contentType);
      Pidf(const Pidf& rhs);
      virtual ~Pidf();

      Pidf& operator=(const Pidf& rhs);
      virtual Contents* clone() const;
      static const Mime& getStaticType() ;
      virtual std::ostream& encodeParsed(std::ostream& str) const;
      virtual void parse(ParseBuffer& pb);

      void setSimpleId( const Data& id );
      void setEntity( const Data& d) { mEntity=d; };
      const Data& getEntity() const;
      void setSimpleStatus(bool online, const Data& note=Data::Empty, const Data& contact=Data::Empty);
      bool getSimpleStatus(Data* note=NULL) const;
      
      class Tuple
      {
         public:
            bool status;
            Data id;
            Data contact;
            float contactPriority;
            Data note;
            Data timeStamp;
            HashMap<Data, Data> attributes;
      };

      std::vector<Tuple>& getTuples();
      const std::vector<Tuple>& getTuples() const;
      // ?dlb? consider returning an XML cursor as well?

      int getNumTuples() const;

      void merge(const Pidf& other);

      static bool init();   
   
   private:
      Data mEntity;
      Data mNote;
      std::vector<Tuple> mTuples;
};

std::ostream& operator<<(std::ostream& strm, const Pidf::Tuple& tuple);
static bool invokePidfInit = Pidf::init();

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
