#ifndef ParserCategory_hxx
#define ParserCategory_hxx

#include <iostream>
#include <list>
#include "sip2/util/Data.hxx"
#include "sip2/util/ParseBuffer.hxx"

#include "sip2/sipstack/ParameterTypes.hxx"
#include "sip2/sipstack/LazyParser.hxx"

namespace Vocal2
{
class UnknownParameter;
class Parameter;

class ParserCategory : public LazyParser
{
    public:
      ParserCategory(HeaderFieldValue* headerFieldValue);
      ParserCategory(const ParserCategory& rhs);
      ParserCategory& operator=(const ParserCategory& rhs);

      virtual ~ParserCategory();

      virtual ParserCategory* clone() const = 0;

      bool exists(const ParamBase& paramType) const;
      void remove(const ParamBase& paramType);
        
      Transport_Param::DType& param(const Transport_Param& paramType) const;
      User_Param::DType& param(const User_Param& paramType) const;
      Method_Param::DType& param(const Method_Param& paramType) const;
      Ttl_Param::DType& param(const Ttl_Param& paramType) const;
      Maddr_Param::DType& param(const Maddr_Param& paramType) const;
      Lr_Param::DType& param(const Lr_Param& paramType) const;
      Q_Param::DType& param(const Q_Param& paramType) const;
      Purpose_Param::DType& param(const Purpose_Param& paramType) const;
      Expires_Param::DType& param(const Expires_Param& paramType) const;
      Handling_Param::DType& param(const Handling_Param& paramType) const;
      Tag_Param::DType& param(const Tag_Param& paramType) const;
      ToTag_Param::DType& param(const ToTag_Param& paramType) const;
      FromTag_Param::DType& param(const FromTag_Param& paramType) const;
      Duration_Param::DType& param(const Duration_Param& paramType) const;
      Branch_Param::DType& param(const Branch_Param& paramType) const;
      Received_Param::DType& param(const Received_Param& paramType) const;
      Mobility_Param::DType& param(const Mobility_Param& paramType) const;
      Comp_Param::DType& param(const Comp_Param& paramType) const;
      Rport_Param::DType& param(const Rport_Param& paramType) const;

      Digest_Algorithm_Param::DType& param(const Digest_Algorithm_Param& paramType) const;
      Digest_Qop_Param::DType& param(const Digest_Qop_Param& paramType) const;
      Digest_Verify_Param::DType& param(const Digest_Verify_Param& paramType) const;

      Data& param(const Data& param) const;
      void remove(const Data& param); 
      bool exists(const Data& param) const;
      
      void parseParameters(ParseBuffer& pb);
      std::ostream& encodeParameters(std::ostream& str) const;
      
      // used to compare 2 parameter lists for equality in an order independent way
      Data commutativeParameterHash() const;
      
   protected:
      ParserCategory();

      Parameter* getParameterByEnum(int type) const;
      void removeParameterByEnum(int type);

      Parameter* getParameterByData(const Data& data) const;
      void removeParameterByData(const Data& data);

      typedef std::list<Parameter*> ParameterList; 
      mutable ParameterList mParameters;
      mutable ParameterList mUnknownParameters;
   private:
      virtual void clear();
      void copyParametersFrom(const ParserCategory& other);
      friend std::ostream& operator<<(std::ostream&, const ParserCategory&);
      friend class NameAddr;
};

std::ostream&
operator<<(std::ostream&, const ParserCategory& category);

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
