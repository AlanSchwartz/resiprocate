#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include "resiprocate/Pidf.hxx"
#include "resiprocate/SipMessage.hxx"
#include "resiprocate/Symbols.hxx"
#include "resiprocate/os/Logger.hxx"

using namespace resip;
using namespace std;

#define RESIPROCATE_SUBSYSTEM Subsystem::SIP

ContentsFactory<Pidf> Pidf::Factory;

Pidf::Pidf()
   : Contents(getStaticType())
{}

#if 0
Pidf::Pidf(const Data& txt)
   : Contents(getStaticType())
{}
#endif

Pidf::Pidf(HeaderFieldValue* hfv, const Mime& contentsType)
   : Contents(hfv, contentsType)
{
}
 
#if 0
Pidf::Pidf(const Data& txt, const Mime& contentsType)
   : Contents(contentsType)
{
}
#endif


Pidf::Pidf(const Pidf& rhs)
   : Contents(rhs),
     mEntity(rhs.mEntity),
     mNote(rhs.mNote)
{
   for( unsigned int i=0; i < rhs.mTuple.size(); i++)
   {
      Tuple t = rhs.mTuple[i];
      mTuple.push_back( t ); // !ah! gratuitous temporary
   }
   assert(  mTuple.size() ==  rhs.mTuple.size() );

}

Pidf::~Pidf()
{
}

Pidf&
Pidf::operator=(const Pidf& rhs)
{
   if (this != &rhs)
   {
      Contents::operator=(rhs);
      clear();
      
      mNote = rhs.mNote;
      mEntity = rhs.mEntity;
      for( unsigned int i=0; i < rhs.mTuple.size(); i++)
      {
         Tuple t = rhs.mTuple[i];
         mTuple.push_back( t );
      }
   }
   return *this;
}

Contents* 
Pidf::clone() const
{
   return new Pidf(*this);
}

const Mime& 
Pidf::getStaticType() 
{
   static Mime type("application","pidf+xml");
   return type;
}


std::ostream& 
Pidf::encodeParsed(std::ostream& str) const
{
   //DebugLog(<< "Pidf::encodeParsed " << mText);
   //str << mText;

   str       << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << Symbols::CRLF;;
   str       << "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\"" << Symbols::CRLF;;
   str       << "           entity=\"pres:"<<mEntity<<"\">" << Symbols::CRLF;;
   for( unsigned int i=0; i<mTuple.size(); i++)
   {
      Data status( (char*)( (mTuple[i].status)?"open":"close" ) );
      str    << "  <tuple id=\""<<mTuple[i].id<<"\">" << Symbols::CRLF;;
      str    << "     <status><basic>"<<status<<"</basic></status>" << Symbols::CRLF;;
      if ( !mTuple[i].contact.empty() )
      {
         str << "     <contact priority=\""<<mTuple[i].contactPriority<<"\">"<< "sip:" << mTuple[i].contact<<"</contact>" << Symbols::CRLF;;
      }
      if ( !mTuple[i].timeStamp.empty() )
      {
         str << "     <timestamp>"<<mTuple[i].timeStamp<<"</timestamp>" << Symbols::CRLF;;
      }
      if ( !mTuple[i].note.empty() )
      {
         str << "     <note>"<<mTuple[i].note<<"</note>" << Symbols::CRLF;;
      }
      str    << "  </tuple>" << Symbols::CRLF;;
   }
   str       << "</presence>" << Symbols::CRLF;;
   
   return str;
}


void 
Pidf::parse(ParseBuffer& pb)
{
   const char* anchor = pb.position();

   Tuple t;
   
   mTuple.push_back( t );
   mTuple[0].status = true;
   mTuple[0].note = Data::Empty;

   const char* close = pb.skipToChars("close");
   if ( close != pb.end() )
   {
      DebugLog ( << "found a close" );
      mTuple[0].status = false;
   }

   pb.reset(anchor);
   const char* open = pb.skipToChars("open");
   if ( open != pb.end() )
   {
      DebugLog ( << "found an open" );
      mTuple[0].status = true;
   }

   pb.reset(anchor);
   pb.skipToChars("<note");
   pb.skipToChars(">");
   if (!pb.eof() )
   {
      const char* startNote = pb.skipChar();
      pb.skipToChars("</note");
      pb.data(mTuple[0].note, startNote);
      DebugLog ( << "found a note of" << mTuple[0].note);
   }
}


void 
Pidf::setSimpleId( const Data& id )
{
   if ( mTuple.size() == 0 )
   {
      Tuple t;
      mTuple.push_back( t );
   }
   assert(mTuple.size() > 0 );
   mTuple[0].id = id;
}


void 
Pidf::setSimpleStatus( bool online, const Data& note, const Data& contact )
{
   if ( mTuple.size() == 0 )
   {
      Tuple t;
      mTuple.push_back( t );
   }

   mTuple[0].status = online;
   mTuple[0].contact = contact;
   mTuple[0].contactPriority = 1.0;
   mTuple[0].note = note;
   mTuple[0].timeStamp = Data::Empty;
}


bool 
Pidf::getSimpleStatus( Data* note )
{
   checkParsed();

   assert(mTuple.size() > 0);

   if ( note )                  // !ass! this function appears naive.
                                // why only the 1st mTuple ?
   {
      *note = mTuple[0].note;
   }
   
   return mTuple[0].status;
}

void
Pidf::clear()
{
   mTuple.clear();
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
