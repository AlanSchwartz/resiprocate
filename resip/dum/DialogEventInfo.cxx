#include "resip/dum/DialogEventInfo.hxx"
#include "resip/dum/InviteSession.hxx"

using namespace resip;

DialogEventInfo::DialogEventInfo()
: mDialogId(Data::Empty, Data::Empty, Data::Empty),
  mState(DialogEventInfo::Trying),
  mDirection(DialogEventInfo::Initiator),
  mCreationTime(0),
  mInviteSession(InviteSessionHandle::NotValid()),
  mReplaced(false)
{
}

DialogEventInfo::DialogEventInfo(const DialogEventInfo& rhs)
: mDialogId(rhs.mDialogId),
  mState(rhs.mState),
  mDialogEventId(rhs.mDialogEventId),
  mDirection(rhs.mDirection),
  mInviteSession(rhs.mInviteSession),
  mReferredBy(rhs.mReferredBy.get() ? new NameAddr(*rhs.mReferredBy) : 0),
  mRouteSet(rhs.mRouteSet),
  mLocalIdentity(rhs.mLocalIdentity),
  mRemoteIdentity(rhs.mRemoteIdentity),
  mLocalTarget(rhs.mLocalTarget),
  mRemoteTarget(rhs.mRemoteTarget.get() ? new Uri(*rhs.mRemoteTarget) : 0),
  mCreationTime(rhs.mCreationTime),
  mReplaced(rhs.mReplaced)
{
   if (rhs.mReplacesId.get())
   {
      mReplacesId = std::auto_ptr<DialogId>(new DialogId(rhs.mReplacesId->getCallId(),
                                                         rhs.mReplacesId->getLocalTag(),
                                                         rhs.mReplacesId->getRemoteTag()));
   }
   if (rhs.mLocalSdp.get())
   {
      mLocalSdp = std::auto_ptr<SdpContents>(static_cast<SdpContents*>(rhs.mLocalSdp->clone()));
   }
   if (rhs.mRemoteSdp.get())
   {
      mRemoteSdp = std::auto_ptr<SdpContents>(static_cast<SdpContents*>(rhs.mRemoteSdp->clone()));
   }
}

DialogEventInfo&
DialogEventInfo::operator=(const DialogEventInfo& dialogEventInfo)
{
   if (this != &dialogEventInfo)
   {
      mDialogId = dialogEventInfo.mDialogId;
      mState = dialogEventInfo.mState;
      mCreationTime = dialogEventInfo.mCreationTime;
      mDialogEventId = dialogEventInfo.mDialogEventId;
      mDirection = dialogEventInfo.mDirection;
      mInviteSession = dialogEventInfo.mInviteSession;
      mLocalIdentity = dialogEventInfo.mLocalIdentity;
      mLocalSdp = std::auto_ptr<SdpContents>(static_cast<SdpContents*>(dialogEventInfo.mLocalSdp->clone()));
      mLocalTarget = dialogEventInfo.mLocalTarget;
      mReferredBy = std::auto_ptr<NameAddr>(static_cast<NameAddr*>(dialogEventInfo.mReferredBy->clone()));
      mRemoteIdentity = dialogEventInfo.mRemoteIdentity;
      mRemoteSdp = std::auto_ptr<SdpContents>(static_cast<SdpContents*>(dialogEventInfo.mRemoteSdp->clone()));
      mRemoteTarget = std::auto_ptr<Uri>(static_cast<Uri*>(dialogEventInfo.mRemoteTarget->clone()));
      mReplacesId = std::auto_ptr<DialogId>(new DialogId(dialogEventInfo.mReplacesId->getDialogSetId(), 
         dialogEventInfo.mReplacesId->getRemoteTag()));
      mRouteSet = dialogEventInfo.mRouteSet;
      mReplaced = dialogEventInfo.mReplaced;
   }
   return *this;
}

bool 
DialogEventInfo::operator==(const DialogEventInfo& rhs) const
{
   return (mDialogEventId == rhs.mDialogEventId);
}

bool 
DialogEventInfo::operator!=(const DialogEventInfo& rhs) const
{
   return (mDialogEventId != rhs.mDialogEventId);
}

bool 
DialogEventInfo::operator<(const DialogEventInfo& rhs) const
{
   return (mDialogEventId < rhs.mDialogEventId);
}

const DialogEventInfo::State& 
DialogEventInfo::getState() const
{
   return mState;
}

const Data& 
DialogEventInfo::getDialogEventId() const
{
   return mDialogEventId;
}

const Data& 
DialogEventInfo::getCallId() const
{
   return mDialogId.getCallId();
}

const Data& 
DialogEventInfo::getLocalTag() const
{
   return mDialogId.getLocalTag();
}

bool 
DialogEventInfo::hasRemoteTag() const
{
   return (mDialogId.getRemoteTag() != Data::Empty);
}

const Data& 
DialogEventInfo::getRemoteTag() const
{
   return mDialogId.getRemoteTag();
}

bool 
DialogEventInfo::hasRefferedBy() const
{
   return (mReferredBy.get() != NULL);
}

const NameAddr& 
DialogEventInfo::getRefferredBy() const
{
   return *mReferredBy;
}

const NameAddr& 
DialogEventInfo::getLocalIdentity() const
{
   return mLocalIdentity;
}

const NameAddr& 
DialogEventInfo::getRemoteIdentity() const
{
   return mRemoteIdentity;
}

const Uri& 
DialogEventInfo::getLocalTarget() const
{
   return mLocalTarget;   
}

bool
DialogEventInfo::hasRouteSet() const
{
   return false;
}

const NameAddrs& 
DialogEventInfo::getRouteSet() const
{
   return mRouteSet;
}

bool
DialogEventInfo::hasRemoteTarget() const
{
   return (mRemoteTarget.get() != NULL);
}

const Uri&
DialogEventInfo::getRemoteTarget() const
{
   return *mRemoteTarget;
}

const SdpContents&
DialogEventInfo::getLocalSdp() const
{
   if (mInviteSession.isValid())
   {
      if (mInviteSession->hasLocalSdp())
      {
         return mInviteSession->getLocalSdp();
      }
   }
   assert(mLocalSdp.get() != NULL);
   return *mLocalSdp;
}

const SdpContents&
DialogEventInfo::getRemoteSdp() const
{
   if (mInviteSession.isValid())
   {
      if (mInviteSession->hasRemoteSdp())
      {
         return mInviteSession->getRemoteSdp();
      }
   }
   assert(mRemoteSdp.get() != NULL);
   return *mRemoteSdp;
}

bool
DialogEventInfo::hasLocalSdp() const
{
   return (mInviteSession.isValid() ? mInviteSession->hasLocalSdp() : mLocalSdp.get() != 0);
}

bool
DialogEventInfo::hasRemoteSdp() const
{
   return (mInviteSession.isValid() ? mInviteSession->hasRemoteSdp() : mRemoteSdp.get() != 0);
}

UInt64
DialogEventInfo::getDuration() const
{
   UInt64 delta = Timer::getTimeSecs() - mCreationTime;
   return delta;
}

bool 
DialogEventInfo::hasReplacesId() const
{
   return (mReplacesId.get() != 0);
}

const DialogId& 
DialogEventInfo::getReplacesId() const
{
   return *mReplacesId;
}

DialogEventInfo::Direction
DialogEventInfo::getDirection() const
{
   return mDirection;
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
