#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/ossl_typ.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include "resiprocate/X509Contents.hxx"
#include "resiprocate/Pkcs8Contents.hxx"
#include "resiprocate/MultipartMixedContents.hxx"
#include "resiprocate/Uri.hxx"
#include "resiprocate/os/Random.hxx"
#include "resiprocate/os/Logger.hxx"

using namespace resip;

#define RESIPROCATE_SUBSYSTEM Subsystem::SIP

int makeSelfCert(X509** selfcert, EVP_PKEY* privkey);

int main(int argc, char* argv[])
{
   int stat;
   Uri aor;
   Data passphrase("password");
   RSA *rsa = NULL;
   EVP_PKEY *privkey = NULL;
   X509 *selfcert = NULL;
   BUF_MEM *bptr = NULL;

   // initilization:  are these needed?
//   CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
//   bio_err=BIO_new_fp(stderr, BIO_NOCLOSE);

 
   Log::initialize(Log::Cerr, Log::Err, argv[0]);
   Log::setLevel(Log::Debug);
   SSL_library_init();
   SSL_load_error_strings();
   OpenSSL_add_ssl_algorithms();
   Random::initialize();

   rsa = RSA_generate_key(1024, RSA_F4, NULL, NULL);
   assert(rsa);    // couldn't make key pair
   
   privkey = EVP_PKEY_new();
   assert(privkey);
   
   stat = EVP_PKEY_set1_RSA(privkey, rsa);
   assert(stat);

   selfcert = X509_new();
   assert(selfcert);
   stat = makeSelfCert(&selfcert, privkey);
   DebugLog ( << "makeSelfCert returned: " << stat );
   assert(stat);   // couldn't make cert
   
   unsigned char* buffer = NULL;     
   int len = i2d_X509(selfcert, &buffer);   // if buffer is NULL, openssl
											// assigns memory for buffer
   assert(buffer);
   Data derData((char *) buffer, len);
   X509Contents *certpart = new X509Contents( derData );
   assert(certpart);
    
   // make an in-memory BIO        [ see  BIO_s_mem(3) ]
   BIO *mbio = BIO_new(BIO_s_mem());
   assert(mbio);

   // encrypt the the private key with the passphrase and put it in the BIO in DER format
   stat = i2d_PKCS8PrivateKey_bio( mbio, privkey, EVP_des_ede3_cbc(), 
      (char *) passphrase.data(), passphrase.size(), NULL, NULL);
   assert(stat);

   // dump the BIO into a Contents
   BIO_get_mem_ptr(mbio, &bptr);
   Data test(bptr->data, bptr->length);
   
   DebugLog( << "Here is the private key unencoded (length " << bptr->length << "bytes): \n" << test );
   
   Pkcs8Contents *keypart = new Pkcs8Contents(test);
   assert(keypart);
   BIO_free(mbio);

   MultipartMixedContents *certsbody = new MultipartMixedContents;
   certsbody->parts().push_back(certpart);
   certsbody->parts().push_back(keypart);
   assert(certsbody);
   
   Data foo;
   DataStream foostr(foo);
   certsbody->encode(foostr);
   foostr.flush();
   
   DebugLog ( << foo );
}


int makeSelfCert(X509 **cert, EVP_PKEY *privkey)   // should include a Uri type at the end of the function call
{
  int stat;
  int serial;
  long err2;
  assert(sizeof(int)==4);
  const long duration = 60*60*24*30;   // make cert valid for 30 days
  X509* selfcert = NULL;
  X509_NAME *subject = NULL;
  X509_EXTENSION *ext = NULL;

  Data domain("example.org");
  Data userAtDomain("user@example.org");

  // Setup the subjectAltName structure here with sip:, im:, and pres: URIs
  // TODO:

  selfcert = *cert;
  subject = X509_NAME_new();
  ext = X509_EXTENSION_new();
  
  X509_set_version(selfcert, 2L);	// set version to X509v3 (starts from 0)

  //  RAND_bytes((char *) serial , 4);
  //serial = 1;
  serial = Random::getRandom();  // get an int worth of randomness
  ASN1_INTEGER_set(X509_get_serialNumber(selfcert),serial);

  stat = X509_NAME_add_entry_by_txt( subject, "O",  MBSTRING_UTF8, (unsigned char *) domain.data(), domain.size(), -1, 0);
  err2 = ERR_get_error();
  DebugLog ( << "SSL Error: " << err2 );
  assert(stat);
  stat = X509_NAME_add_entry_by_txt( subject, "CN", MBSTRING_UTF8, (unsigned char *) userAtDomain.data(), userAtDomain.size(), -1, 0);
  DebugLog ( << "SSL Error: " << err2 );
  assert(stat);
  
  stat = X509_set_issuer_name(selfcert, subject);
  assert(stat);
  stat = X509_set_subject_name(selfcert, subject);
  assert(stat);

  X509_gmtime_adj(X509_get_notBefore(selfcert),0);
  X509_gmtime_adj(X509_get_notAfter(selfcert), duration);

  stat = X509_set_pubkey(selfcert, privkey);
  assert(stat);

  // need to fiddle with this to make this work with lists of IA5 URIs and UTF8
  //ext = X509V3_EXT_conf_nid( NULL , NULL , NID_subject_alt_name, subjectAltNameStr.cstr() );
  //X509_add_ext( selfcert, ext, -1);
  //X509_EXTENSION_free(ext);

  ext = X509V3_EXT_conf_nid(NULL, NULL, NID_basic_constraints, "CA:FALSE");
  stat = X509_add_ext( selfcert, ext, -1);
  assert(stat);  
  X509_EXTENSION_free(ext);

  // add extensions NID_subject_key_identifier and NID_authority_key_identifier

  stat = X509_sign(selfcert, privkey, EVP_sha1());
  assert(stat);
   
  return true; 
}

