#if !defined(RESIP_DATASTREAM_HXX)
#define RESIP_DATASTREAM_HXX 

#include <iostream>

namespace resip
{

class Data;

class DataBuffer : public std::streambuf
{
   public:
      DataBuffer(Data& str);
      virtual ~DataBuffer();

   protected:
      virtual int sync();
      virtual int overflow(int c = -1);

   private:
      Data& mStr;
      DataBuffer(const DataBuffer&);
      DataBuffer& operator=(const DataBuffer&);
};

// To use:
// Data result(4096, true); // size zero, capacity 4096
// {
//    DataStream ds(result);
//    msg->encode(ds);
// }
//
// result contains the encoded message
// -- may be larger than initially allocated

class DataStream : private DataBuffer, public std::iostream
{
   public:
      DataStream(Data& str);
      ~DataStream();

   private:
      DataStream(const DataStream&);
      DataStream& operator=(const DataStream&);
};

class iDataStream : private DataBuffer, public std::istream
{
   public:
      iDataStream(Data& str);
      ~iDataStream();
      
   private:
      iDataStream(const iDataStream&);
      iDataStream& operator=(const iDataStream&);

};

class oDataStream : private DataBuffer, public std::ostream {
   public:
      oDataStream(Data& str);
      ~oDataStream();

   private:
      oDataStream(const oDataStream&);
      oDataStream& operator=(const oDataStream&);
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
