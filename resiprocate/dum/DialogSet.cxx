#include "DialogSet.hxx"

using namespace resip;
using namespace std;

DialogSet::DialogSet(const BaseCreator *creator) : 
    mDialogs(),
    mCreator(creator)
{
   Dialog* dialog = new Dialog(*this, request);
   addDialog(dialog);
   dialog->dispatch(request);
}

DialogSet::DialogSet(const SipMessage& request)
   mDialogs(),
   mCreator(NULL),
   mId(request)
{
   assert(request.isRequest());
}

DialogSet::~DialogSet()
{
}

DialogSetId
Dialogset::getId()
{
   return mId;
}

void
DialogSet::addDialog(Dialog *dialog)
{
   mDialogs.push_back(dialog);
}

void 
DialogSet::removeDialog(const Dialog *dialog)
{
    mDialogs.remove(dialog);
}

DialogIdSet 
DialogSet::getDialogs() const
{
    /** @todo Implment this method */
}

Dialog* 
DialogSet::findDialog(const DialogId id)
{
    std::list<Dialog*>::iterator it = mDialogs.start();
    Dialog *dialog;
    while (it != mDialogs.end())
    {
        dialog  = it->next();
        if (dialog->getId() == id)
        {
            return dialog;
        }
    }
    return NULL;
}

Dialog*
Dialogset::findDialog( const Data& otherTag )
{
    std::list<Dialog*>::iterator it = mDialogs.start();
    Dialog *dialog;
    while (it != mDialogs.end())
    {
        dialog  = it->next();
        if (dialog->getId() == otherTag)
        {
            return dialog;
        }
    }
    return NULL;
}

Dialog* findDialog(SipMessage& msg)
{
    std::list<Dialog*>::iterator it = mDialogs.start();
    Dialog *dialog;
    while (it != mDialogs.end())
    {
        dialog = it->next();
        if (dialog->getId() == msg)
        {
            return dialog;
        }
    }
    return NULL;
}

BaseCreator* 
DialogSet::getCreator()
{
    return mCreator;
}
    
void
DialogSet::dispatch(const SipMessage& msg)
{
   assert(msg.isRequest() || msg.isResponse());
   Dialog* dialog = findDialog(msg);
   if (dialog == 0)
   {
      dialog = new Dialog(mDum, msg);
      this->addDialog(dialog);
   }
   dialog->dispatch(msg);
}

bool
DialogSet::mergeRequest(const SipMessage& request)
{
   for (DialogSet::const_iterator i = dialogs.begin(); i != dialogs.end(); ++i)
   {
      if (i->shouldMerge(request))
      {
         InfoLog (<< "Merging request for: " << request.brief());
         return true;
      }
   }
}

