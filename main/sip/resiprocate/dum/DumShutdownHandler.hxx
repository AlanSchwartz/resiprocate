#if !defined(RESIP_DUMSHUTDOWNHANDLER_HXX)
#define RESIP_DUMSHUTDOWNHANDLER_HXX

namespace resip
{

class DumShutdownHandler
{
   public:
      virtual void dumDestroyed()=0;
};

}


#endif
