#if !defined(RESIP_PROFILE_HXX)
#define RESIP_PROFILE_HXX

#include <iosfwd>
#include <set>
#include "resiprocate/Headers.hxx"
#include "resiprocate/MethodTypes.hxx"

namespace resip
{

class Data;

// !jf! will want to provide a default subclass of Profile that provides some
// good defaults for the Profile
class Profile
{
   public:  
      
      struct DigestCredential
      {
            DigestCredential();
            DigestCredential(const Data& r, const Data& u, const Data& p);
            Data realm;
            Data user;
            Data password;

            bool operator<(const DigestCredential& rhs) const;
            std::ostream& operator<<(std::ostream&) const;
      };
      
      Profile();
      
      void setDefaultAor(const NameAddr& from);
      void setDefaultRegistrationTime(int secs);

      void addSupportedScheme(const Data& scheme);
      void addSupportedMethod(const MethodTypes& method);
      void addSupportedOptionTags(const Token& tag);
      void addSupportedMimeType(const Mime& mimeType);
      void addSupportedEncoding(const Token& encoding);
      void addSupportedLanguage(const Token& lang);

      NameAddr& getDefaultAor();
      int getDefaultRegistrationTime();
      int getDefaultSubscriptionTime();

      bool isSchemeSupported(const Data& scheme);
      bool isMethodSupported(MethodTypes method);
      bool isMimeTypeSupported(const Mime& mimeType);
      bool isContentEncodingSupported(const Token& contentEncoding);
      bool isLanguageSupported(const Tokens& lang);
      
      // return the list of unsupported tokens from set of requires tokens
      Tokens isSupported(const Tokens& requires);
      Tokens getSupportedOptionTags();
      Tokens getAllowedMethods();
      Mimes getSupportedMimeTypes();
      Tokens getSupportedEncodings();
      Tokens getSupportedLanguages();
      
      void addGruu(const Data& aor, const NameAddr& contact);
      bool hasGruu(const Data& aor);
      bool hasGruu(const Data& aor, const Data& instance);
      NameAddr& getGruu(const Data& aor);
      NameAddr& getGruu(const Data& aor, const NameAddr& contact);
      
      const NameAddr& getDefaultAor() const;

      void setOutboundProxy( const Uri& uri );
      const NameAddr& getOutboundProxy() const;
      bool hasOutboundProxy() const;
      
      void disableGruu();

      /// The following functions deal with getting digest credentals 
      //@{ 

      void addDigestCredential( const Data& realm, const Data& user, const Data& password);
      
      /** This class is used as a callback to get digest crednetials. The
       * derived class must define one of computeA1 or getPaswword. computeA1 is
       * tried first and it it returns an empty string, then getPassword is
       * tried. */
      class DigestCredentialHandler
      {
         public:
            virtual Data computeA1( const Data& realm, const Data& users );
            virtual Data getPassword( const Data& realm, const Data& users );
      };
      
      void setDigestHandler( DigestCredentialHandler* handler );
      //@}
      
      DigestCredentialHandler* getDigestHandler();
      const DigestCredential& getDigestCredential( const Data& realm );
      
   private:
      NameAddr mAor;
      int mDefaultRegistrationExpires;

      bool mHasOutboundProxy;
      NameAddr mOutboundProxy;
      
      std::set<Data> mSupportedSchemes;
      std::set<MethodTypes> mSupportedMethodTypes;
      Tokens mSupportedMethods;
      Tokens mSupportedOptionTags;
      Mimes mSupportedMimeTypes;
      Tokens mSupportedEncodings;
      Tokens mSupportedLanguages;

      DigestCredentialHandler* mDigestCredentialHandler;
      
      typedef std::set<DigestCredential> DigestCredentials;
      DigestCredentials mDigestCredentials;
};
   

 
}

#endif
