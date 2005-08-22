#include <cppunit/TestCase.h>
#include <boost/lexical_cast.hpp>
#include <memory>

#include "rutil/Logger.hxx"
#include "tfm/TestEndPoint.hxx"
#include "tfm/Sequence.hxx"
#include "tfm/SequenceSet.hxx"
#include "tfm/ExpectActionEvent.hxx"

using namespace resip;
using namespace boost;
using namespace std;

#define RESIPROCATE_SUBSYSTEM resip::Subsystem::TEST


TestEndPoint::AlwaysTrue* TestEndPoint::AlwaysTruePred = new TestEndPoint::AlwaysTrue();


TestEndPoint::GlobalFailure::GlobalFailure(const resip::Data& msg,
                                           const resip::Data& file, 
                                           int line)
   : TestException(msg, file, line)
{
}

resip::Data 
TestEndPoint::GlobalFailure::getName() const 
{
   return "TestEndPoint::GlobalFailure"; 
}

TestEndPoint::TestEndPoint() 
   : mSequenceSet(0)
{
}

TestEndPoint::~TestEndPoint() 
{
}

int
TestEndPoint::DebugTimeMult() 
{
   return (Log::level() >= Log::Debug) ? 4 : 1;
}

SequenceSet* 
TestEndPoint::getSequenceSet() const 
{
   return mSequenceSet;
}

void
TestEndPoint::setSequenceSet(SequenceSet* set)
{
   //DebugLog(<< this << "->TestEndPoint::setSequenceSet(" << set << ")" << "(was " << mSequenceSet << ")");
   assert(set == 0 || mSequenceSet == 0 || mSequenceSet == set);
   mSequenceSet = set;
}

bool 
TestEndPoint::operator==(const TestEndPoint& endPoint) const
{
   return this == &endPoint;
}

TestEndPoint::Action::Action() 
{
}

TestEndPoint::Action::~Action() 
{
}

void 
TestEndPoint::Action::operator()(boost::shared_ptr<Event> event)
{
   (*this)();
}

TestEndPoint::EndPointAction::EndPointAction(TestEndPoint* endPoint)
   : mEndPoint(endPoint)
{
}           

TestEndPoint::EndPointAction::~EndPointAction()
{
}

void 
TestEndPoint::EndPointAction::operator()()
{
   return (*this)(*mEndPoint);
}

TestEndPoint::NoSeqAction::NoSeqAction() : EndPointAction(0)
{
}

void 
TestEndPoint::NoSeqAction::operator()(TestEndPoint& user) 
{
}

resip::Data 
TestEndPoint::NoSeqAction::toString() const 
{
   return "NoSeqAction";
}

ExpectAction* 
TestEndPoint::noAction()
{
   return new NoAction();
}


TestEndPoint::Note::Note(TestEndPoint* endPoint, const char* message) 
   : mEndPoint(endPoint), mMessage(message) 
{
}

void 
TestEndPoint::Note::operator()()
{
   InfoLog(<<"##Note: " << mEndPoint->getName() << " " << mMessage);
}

void 
TestEndPoint::Note::operator()(shared_ptr<Event> event)
{
   InfoLog(<<"##Note: " << mEndPoint->getName() << " " << mMessage << ", " << event->briefString());
}

resip::Data
TestEndPoint::Note::toString() const
{
   return mEndPoint->getName() + ".Note(" + mMessage + ")";
}

ActionBase* 
TestEndPoint::note(const char* msg)
{
   return new Note(this, msg);
}


void
TestEndPoint::clear()
{
   DebugLog(<< "clearing " << getName());
   mSequenceSet = 0;
}


TestEndPoint::Execute::Execute(TestEndPoint* from, 
                               ExecFn fn, 
                               const resip::Data& label) 
   : mEndPoint(*from), mLabel(label), mExec(fn) 
{
}

void
TestEndPoint::Execute::operator()(shared_ptr<Event> event)
{
   mExec(event);
}

resip::Data
TestEndPoint::Execute::toString() const
{
   return mEndPoint.getName() + ".execute(" + mLabel + ")";
}

TestEndPoint::And::And(const list<SequenceClass*>& sequences,
                       ActionBase* aferAction)
   : mActive(false),
     mAfterAction(aferAction)
{
   assert(sequences.size() > 1);

   mSequences = sequences;
}

bool
TestEndPoint::And::queue(SequenceClass* parent)
{
   DebugLog(<<"!*!And::queue");
   DebugLog(<<"# sequences: " << mSequences.size());

   parent->setBranchCount(mSequences.size());
   parent->setAfterAction(mAfterAction);

   for(list<SequenceClass*>::iterator seqit =  mSequences.begin();
       seqit != mSequences.end(); seqit++)
   {
      DebugLog(<< "Adding in queue: " << **seqit);
      (*seqit)->addToActiveSet();
      (*seqit)->setParent(parent);
   }
   return true;
}

void
TestEndPoint::And::setSequenceSet(SequenceSet* set)
{
   for(list<SequenceClass*>::iterator seqit =  mSequences.begin();
       seqit != mSequences.end(); seqit++)
   {
      (*seqit)->setSequenceSet(set);
   }
}

bool
TestEndPoint::And::isMatch(shared_ptr<Event> event) const
{
   assert(0);
}

resip::Data
TestEndPoint::And::explainMismatch(shared_ptr<Event> event) const
{
   assert(0);
}

void
TestEndPoint::And::onEvent(TestEndPoint& user, shared_ptr<Event> event)
{
   assert(0);
}

TestEndPoint::And::~And()
{
   for(list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      delete *i;
   }
}

ostream& 
TestEndPoint::And::output(ostream& s) const
{
   s << "And(";
   for(list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      s << "Sub" << **i << ", ";
   }
   s << ")";
   return s;
}

void
TestEndPoint::And::prettyPrint(ostream& str, bool& previousActive, int ind) const
{
   str << "And(";
   ind += 4;

   bool recPreviousActive = previousActive;
   bool returnPreviousActive = previousActive;
   for (list<SequenceClass*>::const_iterator i = mSequences.begin();
        i != mSequences.end(); i++)
   {
      recPreviousActive = previousActive;
      if (i == mSequences.begin())
      {
         (*i)->prettyPrint(str, recPreviousActive, ind);
      }
      else
      {
         str << "," << endl;
         str << indent(ind);
         (*i)->prettyPrint(str, recPreviousActive, ind); 
      }
      returnPreviousActive |= recPreviousActive;
   }
   str << ")";
   previousActive = returnPreviousActive;
}

TestEndPoint::Pause::Pause(int msec, TestEndPoint* endPoint) 
   : mMsec(msec), mEndPoint(endPoint) 
{
}

void TestEndPoint::Pause::exec(shared_ptr<Event> event)
{
   assert(mNext);
   assert(mEndPoint->getSequenceSet());
   mEndPoint->getSequenceSet()->enqueue(shared_ptr<Event>(new ExpectActionEvent(mNext, event)), mMsec);
}

void TestEndPoint::Pause::exec()
{
   assert(mNext);
   assert(mEndPoint->getSequenceSet());
   mEndPoint->getSequenceSet()->enqueue(shared_ptr<Event>(new ExpectActionEvent(mNext, mEndPoint)), mMsec);
}

resip::Data
TestEndPoint::Pause::toString() const
{
   return "Pause(" + resip::Data(mMsec) + ")";
}

ActionBase* 
TestEndPoint::pause(int msec)
{
   return new Pause(msec, this);
}


TestEndPoint::ExpectBase::Exception::Exception(const resip::Data& msg,
                                               const resip::Data& file,
                                               const int line)
   : TestException(msg, file, line)
{
}

resip::Data 
TestEndPoint::ExpectBase::Exception::getName() const 
{
   return "TestEndPoint::ExpectBase::Exception";
}

TestEndPoint::ExpectBase::ExpectBase() 
   : mIsOptional(false) 
{
}

TestEndPoint::ExpectBase::~ExpectBase() 
{
}

bool 
TestEndPoint::ExpectBase::queue(SequenceClass* parent) 
{
   return false; 
}

void 
TestEndPoint::ExpectBase::setSequenceSet(SequenceSet* set)
{
   getEndPoint()->setSequenceSet(set);
}

std::ostream& 
TestEndPoint::ExpectBase::output(std::ostream& s) const
{
   s << getMsgTypeString();
   return s;
}

void 
TestEndPoint::ExpectBase::prettyPrint(std::ostream& str, bool& previousActive, int ind) const
{
   output(str);
}

bool 
TestEndPoint::ExpectBase::isOptional() const 
{
   return mIsOptional;
}


TestEndPoint::AssertException::AssertException(const resip::Data& msg,
                                               const resip::Data& file,
                                               const int line)
   : resip::BaseException(msg, file, line)
{
}

resip::Data 
TestEndPoint::AssertException::getName() const 
{
   return "TestEndPoint::AssertException";
}

const char* 
TestEndPoint::AssertException::name() const 
{
   return "TestEndPoint::AssertException";
}

TestEndPoint::Assert::Assert(TestEndPoint* from, 
                             PredicateFn fn, 
                             const resip::Data& label) 
   : mEndPoint(*from), mLabel(label), mPredicate(fn) 
{
}

void
TestEndPoint::Assert::operator()(shared_ptr<Event> event)
{
   if (!mPredicate(event))
   {
      throw AssertException("failed: " + mLabel, __FILE__, __LINE__);
   }
}

resip::Data
TestEndPoint::Assert::toString() const
{
   return mEndPoint.getName() + ".check(" + mLabel + ")";
}

ExpectAction* 
TestEndPoint::check1(PredicateFn fn, resip::Data label)
{
   return new Assert(this, fn, label);
}


std::ostream& 
operator<<(ostream& s, const TestEndPoint& endPoint)
{
   s << endPoint.getName();
   return s;
}

std::ostream& 
operator<<(std::ostream& s, const TestEndPoint::ExpectBase& eb)
{
   return eb.output(s);
}


ExpectAction*
noAction()
{
   return new TestEndPoint::NoAction;
}


TestEndPoint::And*
And(SequenceClass* sequence1,
    SequenceClass* sequence2,
    ActionBase* afterAction)
{
   list<SequenceClass*> sequences;
   sequences.push_front(sequence1);
   sequences.push_front(sequence2);
   return new TestEndPoint::And(sequences, afterAction);
}

TestEndPoint::And*
And(SequenceClass* sequence1,
    SequenceClass* sequence2,
    SequenceClass* sequence3,
    ActionBase* afterAction)
{
   list<SequenceClass*> sequences;
   sequences.push_front(sequence1);
   sequences.push_front(sequence2);
   sequences.push_front(sequence3);
   return new TestEndPoint::And(sequences, afterAction);
}

TestEndPoint::And*
And(SequenceClass* sequence1,
    SequenceClass* sequence2,
    SequenceClass* sequence3,
    SequenceClass* sequence4,
    ActionBase* afterAction)
{
   list<SequenceClass*> sequences;
   sequences.push_front(sequence1);
   sequences.push_front(sequence2);
   sequences.push_front(sequence3);
   sequences.push_front(sequence4);
   return new TestEndPoint::And(sequences, afterAction);
}

TestEndPoint::And*
And(SequenceClass* sequence1,
    SequenceClass* sequence2,
    SequenceClass* sequence3,
    SequenceClass* sequence4,
    SequenceClass* sequence5,
    ActionBase* afterAction)
{
   list<SequenceClass*> sequences;
   sequences.push_front(sequence1);
   sequences.push_front(sequence2);
   sequences.push_front(sequence3);
   sequences.push_front(sequence4);
   sequences.push_front(sequence5);
   return new TestEndPoint::And(sequences, afterAction);
}

TestEndPoint::And*
And(SequenceClass* sequence1,
    SequenceClass* sequence2,
    SequenceClass* sequence3,
    SequenceClass* sequence4,
    SequenceClass* sequence5,
    SequenceClass* sequence6,
    ActionBase* afterAction)
{
   list<SequenceClass*> sequences;
   sequences.push_front(sequence1);
   sequences.push_front(sequence2);
   sequences.push_front(sequence3);
   sequences.push_front(sequence4);
   sequences.push_front(sequence5);
   sequences.push_front(sequence6);
   return new TestEndPoint::And(sequences, afterAction);
}

TestEndPoint::And*
And(SequenceClass* sequence1,
    SequenceClass* sequence2,
    SequenceClass* sequence3,
    SequenceClass* sequence4,
    SequenceClass* sequence5,
    SequenceClass* sequence6,
    SequenceClass* sequence7,
    ActionBase* afterAction)
{
   list<SequenceClass*> sequences;
   sequences.push_front(sequence1);
   sequences.push_front(sequence2);
   sequences.push_front(sequence3);
   sequences.push_front(sequence4);
   sequences.push_front(sequence5);
   sequences.push_front(sequence6);
   sequences.push_front(sequence7);
   return new TestEndPoint::And(sequences, afterAction);
}

TestEndPoint::And*
And(const list<SequenceClass*>& sequences,
    ActionBase* afterAction)
{
   return new TestEndPoint::And(sequences, afterAction);
}

Box
TestEndPoint::And::layout() const
{
   //                 |
   //     +-----------+------------+      // branch start 1
   //     |           |            |      // branch start 2
   // ext1->100   ext17->100   ext9->100
   //     |           |            |
   //     |        ext3->100   ext4->100
   //     |           |            |      // branch end 1
   //     +-----------+------------+      // branch end 2

   //DebugLog(<< "TestEndPoint::And::layout");

   // pad
   mBox.mX = 1;
   mBox.mY = 0;

   for (list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      // layout alone (0, 0) corner
      Box sub = (*i)->layout(); 
      // displace to the right for preceding sub-sequences
      sub = (*i)->displaceRight(mBox);
      //DebugLog(<< "after displace right: " << sub);
      // offset down for And branch start
      sub = (*i)->displaceDown(Box(0, 0, 0, 2));
      
      mBox.boxUnion(sub);
   }

   /// tell each child sequence its container height so it can pad | down
   for (list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      (*i)->mContainerBottom = mBox.mY + mBox.mHeight;
   }

   // offset down for And branch end
   mBox.mHeight += 2;

   // pad
   mBox.mWidth += 2;
   //DebugLog(<< "TestEndPoint::And::layout: " << mBox);
   return mBox;
}

void
TestEndPoint::And::render(CharRaster& out) const
{
   out[mBox.mY][mBox.mX + mBox.mWidth/2] = '|';
   
   // draw branch start line
   for (unsigned int i = mSequences.front()->mBox.mX+mSequences.front()->mBox.mWidth/2;
        i < mSequences.back()->mBox.mX+mSequences.back()->mBox.mWidth/2; i++)
   {
      out[mBox.mY+1][i+1] = '-';
   }

   // draw branch start joins
   for(list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      out[mBox.mY+1][(*i)->mBox.mX + (*i)->mBox.mWidth/2+1] = '+';
   }

   // render sub-sequences
   for(list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      (*i)->render(out);
   }

   // draw sequence terminal |
   for(list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      out[mBox.mY + mBox.mHeight-2][(*i)->mBox.mX + (*i)->mBox.mWidth/2+1] = '|';
   }
   
   // draw branch end line
   for (unsigned int i = mSequences.front()->mBox.mX+mSequences.front()->mBox.mWidth/2;
        i < mSequences.back()->mBox.mX+mSequences.back()->mBox.mWidth/2; i++)
   {
      out[mBox.mY + mBox.mHeight-1][i+1] = '-';
   }

   // draw branch end joins
   for(list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      out[mBox.mY + mBox.mHeight-1][(*i)->mBox.mX + (*i)->mBox.mWidth/2+1] = '+';
   }
}

Box&
TestEndPoint::And::displaceDown(const Box &box)
{
   for(list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      (*i)->displaceDown(box);
   }

   AsciiGraphic::displaceDown(box);
   return mBox;
}

Box&
TestEndPoint::And::displaceRight(const Box &box)
{
   for(list<SequenceClass*>::const_iterator i = mSequences.begin();
       i != mSequences.end(); i++)
   {
      (*i)->displaceRight(box);
   }

   AsciiGraphic::displaceRight(box);
   return mBox;
}

TestEndPoint::ExpectBase*
optional(TestEndPoint::ExpectBase* expect)
{
   expect->mIsOptional = true;
   return expect;
}
/*
  Copyright (c) 2005, PurpleComm, Inc. 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  * Neither the name of PurpleComm, Inc. nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
