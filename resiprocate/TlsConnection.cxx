#if defined(HAVE_CONFIG_H)
#include "resiprocate/config.hxx"
#endif

#include "resiprocate/TlsConnection.hxx"
#include "resiprocate/Security.hxx"
#include "resiprocate/os/Logger.hxx"
#include "resiprocate/os/Socket.hxx"

#include <openssl/e_os2.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>

using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::TRANSPORT

TlsConnection::TlsConnection( const Tuple& tuple, Socket fd, Security* security, bool server ) :
    Connection(tuple, fd)
{
   DebugLog (<< "Creating TLS connection " << tuple << " on " << fd);

   mSsl = NULL;

   if (server)
   {
      DebugLog( << "Trying to form TLS connection - acting as server" );
   }
   else
   {
      DebugLog( << "Trying to form TLS connection - acting as client" );
   }

   assert( security );
   SSL_CTX* ctx = security->getTlsCtx(server);
   assert(ctx);
   
   mSsl = SSL_new(ctx);
   assert(mSsl);
   
   mBio = BIO_new_socket(fd,0/*close flag*/);
   assert( mBio );
   
   SSL_set_bio( mSsl, mBio, mBio );

   mState = server ? Accepting : Connecting;

   if (checkState() == Broken)
       throw Transport::Exception( Data("TLS setup failed"), __FILE__, __LINE__ );
}


const char*
TlsConnection::fromState(TlsConnection::State s)
{
    switch(s)
    {
        case Handshaking: return "Handshaking"; break;
        case Accepting: return "Accepting"; break;
        case Broken: return "Broken"; break;
        case Connecting: return "Connecting"; break;
        case Up: return "Up"; break;
    }
    return "????";
}

TlsConnection::State
TlsConnection::checkState()
{
   //DebugLog(<<"state is " << fromState(mState));

   if (mState == Up || mState == Broken)
     return mState;

   int ok=0;

   if (mState != Handshaking)
   {
       if (mState == Accepting)
       {
	   ok = SSL_accept(mSsl);
       }
       else
       {
	   ok = SSL_connect(mSsl);
       }

       if ( ok <= 0 )
       {
	   int err = SSL_get_error(mSsl,ok);
	   char buf[256];
	   ERR_error_string_n(err,buf,sizeof(buf));
	   DebugLog( << "TLS error ok=" << ok << " err=" << err << " " << buf );
          
	   switch (err)
	   {
	       case SSL_ERROR_WANT_READ:
		   DebugLog( << "TLS connection want read" );
		   return mState;
	       case SSL_ERROR_WANT_WRITE:
		   DebugLog( << "TLS connection want write" );
		   return mState;
	       case SSL_ERROR_WANT_CONNECT:
		   DebugLog( << "TLS connection want connect" );
		   return mState;
#if 0
	       case SSL_ERROR_WANT_ACCEPT:
		   DebugLog( << "TLS connection want accept" );
		   return mState;
#endif
	   }
	   
	   ErrLog( << "TLS connection failed "
		   << "ok=" << ok << " err=" << err << " " << buf );
         
	   switch (err)
	   {
	       case SSL_ERROR_NONE: 
		   ErrLog( <<" (SSL Error none)" );
		   break;
	       case SSL_ERROR_SSL: 
		   ErrLog( <<" (SSL Error ssl)" );
		   break;
	       case SSL_ERROR_WANT_READ: 
		   ErrLog( <<" (SSL Error want read)" ); break;
	       case SSL_ERROR_WANT_WRITE: 
		   ErrLog( <<" (SSL Error want write)" ); 
		   break;
	       case SSL_ERROR_WANT_X509_LOOKUP: 
		   ErrLog( <<" (SSL Error want x509 lookup)" ); 
		   break;
	       case SSL_ERROR_SYSCALL: 
		   ErrLog( <<" (SSL Error want syscall)" ); 
		   ErrLog( <<"Error may be because trying ssl connection to tls server" ); 
		   break;
	       case SSL_ERROR_WANT_CONNECT: 
		   ErrLog( <<" (SSL Error want connect)" ); 
		   break;
#if ( OPENSSL_VERSION_NUMBER >= 0x0090702fL )
	       case SSL_ERROR_WANT_ACCEPT: 
		   ErrLog( <<" (SSL Error want accept)" ); 
		   break;
#endif
		   while (true)
		   {
		       const char* file;
		       int line;
                  
		       unsigned long code = ERR_get_error_line(&file,&line);
		       if ( code == 0 )
		       {
			   break;
		       }
                  
		       char buf[256];
		       ERR_error_string_n(code,buf,sizeof(buf));
		       ErrLog( << buf  );
		       InfoLog( << "Error code = " 
				<< code << " file=" << file << " line=" << line );
		   }
	   }

	   mState = Broken;
	   mBio = 0;
	   ErrLog (<< "Couldn't TLS connect");
	   return mState;
       }

       InfoLog( << "TLS connected" ); 
       mState = Handshaking;
   }

   InfoLog( << "TLS handshake starting" ); 

   ok = SSL_do_handshake(mSsl);
      
   if ( ok <= 0 )
   {
       int err = SSL_get_error(mSsl,ok);
       char buf[256];
       ERR_error_string_n(err,buf,sizeof(buf));
         
       switch (err)
       {
	   case SSL_ERROR_WANT_READ:
               DebugLog( << "TLS handshake want read" );
	       return mState;
	   case SSL_ERROR_WANT_WRITE:
               DebugLog( << "TLS handshake want write" );
	       return mState;
	   default:
               ErrLog( << "TLS handshake failed "
                       << "ok=" << ok << " err=" << err << " " << buf );
               mBio = NULL;
	       mState = Broken;
	       return mState;
       }
   }
   
   InfoLog( << "TLS handshake done" ); 
   mState = Up;
   return mState;
}

      
int 
TlsConnection::read(char* buf, int count )
{
   assert( mSsl ); 
   assert( buf );

   switch(checkState())
   {
       case Broken:
           return -1;
           break;
       case Up:
           break;
       default:
       return 0;
           break;
   }

   if (!mBio)
   {
      DebugLog( << "Got TLS read bad bio  " );
      return 0;
   }
      
   if ( !isGood() )
   {
      return -1;
   }
   
   int bytesRead = SSL_read(mSsl,buf,count);
   if (bytesRead <= 0 )
   {
      int err = SSL_get_error(mSsl,bytesRead);
      switch (err)
      {
         case SSL_ERROR_WANT_READ:
         case SSL_ERROR_WANT_WRITE:
         case SSL_ERROR_NONE:
         {
            DebugLog( << "Got TLS read got condition of " << err  );
            return 0;
         }
         break;
         default:
         {
            char buf[256];
            ERR_error_string_n(err,buf,sizeof(buf));
            ErrLog( << "Got TLS read ret=" << bytesRead << " error=" << err  << " " << buf  );
            return -1;
         }
         break;
      }
      assert(0);
   }
   //DebugLog(<<"SSL bytesRead="<<bytesRead);
   return bytesRead;
}


int 
TlsConnection::write( const char* buf, int count )
{
   assert( mSsl );
   assert( buf );
   int ret;
 
   switch(checkState())
   {
       case Broken:
           return -1;
           break;
       case Up:
           break;
       default:
       return 0;
           break;
   }

   if (!mBio)
   {
      DebugLog( << "Got TLS write bad bio "  );
      return 0;
   }
        
   ret = SSL_write(mSsl,(const char*)buf,count);
   if (ret < 0 )
   {
      int err = SSL_get_error(mSsl,ret);
      switch (err)
      {
         case SSL_ERROR_WANT_READ:
         case SSL_ERROR_WANT_WRITE:
         case SSL_ERROR_NONE:
         {
            DebugLog( << "Got TLS write got condition of " << err  );
            return 0;
         }
         break;
         default:
         {
            while (true)
            {
               const char* file;
               int line;
               
               unsigned long code = ERR_get_error_line(&file,&line);
               if ( code == 0 )
               {
                  break;
               }
               
               char buf[256];
               ERR_error_string_n(code,buf,sizeof(buf));
               ErrLog( << buf  );
               DebugLog( << "Error code = " << code << " file=" << file << " line=" << line );
            }
            ErrLog( << "Got TLS write error=" << err << " ret=" << ret  );
            return -1;
         }
         break;
      }
   }

   DebugLog( << "Did TLS write"  );

   return ret;
}


bool 
TlsConnection::hasDataToRead() // has data that can be read 
{
    if (checkState() != Up)
	return false;

   int p = SSL_pending(mSsl);
   //DebugLog(<<"hasDataToRead(): "<<p);
   return (p>0);
}


bool 
TlsConnection::isGood() // has data that can be read 
{
   if ( mBio == 0 )
   {
      return false;
   }

   int mode = SSL_get_shutdown(mSsl);
   if ( mode != 0 ) 
   {
      return false;
   }
      
   return true;
}


Data 
TlsConnection::peerName()
{
   assert(mSsl);
   Data ret = Data::Empty;

   if (checkState() != Up)
       return Data::Empty;

   if (!mBio)
   {
      DebugLog( << "bad bio" );
      return Data::Empty;
   }

#if 1 // print session infor       
   SSL_CIPHER *ciph;
   
   ciph=SSL_get_current_cipher(mSsl);
   InfoLog( << "TLS sessions set up with " 
            <<  SSL_get_version(mSsl) << " "
            <<  SSL_CIPHER_get_version(ciph) << " "
            <<  SSL_CIPHER_get_name(ciph) << " " );
#endif

#ifdef WIN32
   assert(0);
#else
   ErrLog(<< "request peer certificate" );
   X509* cert = SSL_get_peer_certificate(mSsl);
   if ( !cert )
   {
      ErrLog(<< "No peer certifiace in TLS connection" );
      return ret;
   }
   ErrLog(<< "Got peer certificate" );

   X509_NAME* subject = X509_get_subject_name(cert);
   assert(subject);
   
   int i =-1;
   while( true )
   {
      i = X509_NAME_get_index_by_NID(subject, NID_commonName,i);
      if ( i == -1 )
      {
         break;
      }
      assert( i != -1 );
      X509_NAME_ENTRY* entry = X509_NAME_get_entry(subject,i);
      assert( entry );
      
      ASN1_STRING*	s = X509_NAME_ENTRY_get_data(entry);
      assert( s );
      
      int t = M_ASN1_STRING_type(s);
      int l = M_ASN1_STRING_length(s);
      unsigned char* d = M_ASN1_STRING_data(s);
      
      ErrLog( << "got string type=" << t << " len="<<l );
      ErrLog( << "data=<" << d << ">" );

      ret = Data(d);
   }
   
   STACK* sk = X509_get1_email(cert);
   if (sk)
   {
      ErrLog( << "Got an email" );
      
      for( int i=0; i<sk_num(sk); i++ )
      {
         char* v = sk_value(sk,i);
         ErrLog( << "Got an email value of " << v );

         ret = Data(v);
      }
   }
   
   int numExt = X509_get_ext_count(cert);
   ErrLog(<< "Got peer certificate with " << numExt << " extentions" );

   for ( int i=0; i<numExt; i++ )
   {
      X509_EXTENSION* ext = X509_get_ext(cert,i);
      assert( ext );
      
      const char* str = OBJ_nid2sn(OBJ_obj2nid(X509_EXTENSION_get_object(ext)));
      assert(str);
      
      ErrLog(<< "Got certificate extention" << str );
   }
   
   X509_free(cert); cert=NULL;
#endif
   return ret;
}

