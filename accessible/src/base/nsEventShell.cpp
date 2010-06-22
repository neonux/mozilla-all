/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsEventShell.h"

#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsDocAccessible.h"

////////////////////////////////////////////////////////////////////////////////
// nsEventShell
////////////////////////////////////////////////////////////////////////////////

void
nsEventShell::FireEvent(nsAccEvent *aEvent)
{
  if (!aEvent)
    return;

  nsAccessible *accessible = aEvent->GetAccessible();
  NS_ENSURE_TRUE(accessible,);

  nsINode* node = aEvent->GetNode();
  if (node) {
    sEventTargetNode = node;
    sEventFromUserInput = aEvent->IsFromUserInput();
  }

  accessible->HandleAccEvent(aEvent);

  sEventTargetNode = nsnull;
}

void
nsEventShell::FireEvent(PRUint32 aEventType, nsAccessible *aAccessible,
                        PRBool aIsAsynch, EIsFromUserInput aIsFromUserInput)
{
  NS_ENSURE_TRUE(aAccessible,);

  nsRefPtr<nsAccEvent> event = new nsAccEvent(aEventType, aAccessible,
                                              aIsAsynch, aIsFromUserInput);

  FireEvent(event);
}

void 
nsEventShell::GetEventAttributes(nsINode *aNode,
                                 nsIPersistentProperties *aAttributes)
{
  if (aNode != sEventTargetNode)
    return;

  nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::eventFromInput,
                         sEventFromUserInput ? NS_LITERAL_STRING("true") :
                                               NS_LITERAL_STRING("false"));
}

////////////////////////////////////////////////////////////////////////////////
// nsEventShell: private

PRBool nsEventShell::sEventFromUserInput = PR_FALSE;
nsCOMPtr<nsINode> nsEventShell::sEventTargetNode;


////////////////////////////////////////////////////////////////////////////////
// nsAccEventQueue
////////////////////////////////////////////////////////////////////////////////

nsAccEventQueue::nsAccEventQueue(nsDocAccessible *aDocument):
  mObservingRefresh(PR_FALSE), mDocument(aDocument)
{
}

nsAccEventQueue::~nsAccEventQueue()
{
  NS_ASSERTION(!mDocument, "Queue wasn't shut down!");
}

////////////////////////////////////////////////////////////////////////////////
// nsAccEventQueue: nsISupports and cycle collection

NS_IMPL_CYCLE_COLLECTION_CLASS(nsAccEventQueue)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsAccEventQueue)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsAccEventQueue)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mDocument");
  cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mDocument.get()));

  PRUint32 i, length = tmp->mEvents.Length();
  for (i = 0; i < length; ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvents[i]");
    cb.NoteXPCOMChild(tmp->mEvents[i].get());
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsAccEventQueue)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mEvents)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAccEventQueue)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAccEventQueue)

////////////////////////////////////////////////////////////////////////////////
// nsAccEventQueue: public

void
nsAccEventQueue::Push(nsAccEvent *aEvent)
{
  mEvents.AppendElement(aEvent);
  
  // Filter events.
  CoalesceEvents();
  
  // Process events.
  PrepareFlush();
}

void
nsAccEventQueue::Shutdown()
{
  if (mObservingRefresh) {
    nsCOMPtr<nsIPresShell> shell = mDocument->GetPresShell();
    if (!shell ||
        shell->RemoveRefreshObserver(this, Flush_Display)) {
      mObservingRefresh = PR_FALSE;
    }
  }
  mDocument = nsnull;
  mEvents.Clear();
}

////////////////////////////////////////////////////////////////////////////////
// nsAccEventQueue: private

void
nsAccEventQueue::PrepareFlush()
{
  // If there are pending events in the queue and events flush isn't planed
  // yet start events flush asynchronously.
  if (mEvents.Length() > 0 && !mObservingRefresh) {
    nsCOMPtr<nsIPresShell> shell = mDocument->GetPresShell();
    // Use a Flush_Display observer so that it will get called after
    // style and ayout have been flushed.
    if (shell &&
        shell->AddRefreshObserver(this, Flush_Display)) {
      mObservingRefresh = PR_TRUE;
    }
  }
}

void
nsAccEventQueue::WillRefresh(mozilla::TimeStamp aTime)
{
  // If the document accessible is now shut down, don't fire events in it
  // anymore.
  if (!mDocument)
    return;

  nsCOMPtr<nsIPresShell> presShell = mDocument->GetPresShell();
  if (!presShell)
    return;

  // Process only currently queued events. Newly appended events during events
  // flushing won't be processed.
  nsTArray < nsRefPtr<nsAccEvent> > events;
  events.SwapElements(mEvents);
  PRUint32 length = events.Length();
  NS_ASSERTION(length, "How did we get here without events to fire?");

  for (PRUint32 index = 0; index < length; index ++) {

    // No presshell means the document was shut down during event handling
    // by AT.
    if (!mDocument || !mDocument->HasWeakShell())
      break;

    nsAccEvent *accEvent = events[index];
    if (accEvent->mEventRule != nsAccEvent::eDoNotEmit)
      mDocument->ProcessPendingEvent(accEvent);
  }

  if (mEvents.Length() == 0) {
    nsCOMPtr<nsIPresShell> shell = mDocument->GetPresShell();
    if (!shell ||
        shell->RemoveRefreshObserver(this, Flush_Display)) {
      mObservingRefresh = PR_FALSE;
    }
  }
}

void
nsAccEventQueue::CoalesceEvents()
{
  PRUint32 numQueuedEvents = mEvents.Length();
  PRInt32 tail = numQueuedEvents - 1;
  nsAccEvent* tailEvent = mEvents[tail];

  // No node means this is application accessible (which can be a subject
  // of reorder events), we do not coalesce events for it currently.
  if (!tailEvent->mNode)
    return;

  switch(tailEvent->mEventRule) {
    case nsAccEvent::eCoalesceFromSameSubtree:
    {
      for (PRInt32 index = tail - 1; index >= 0; index--) {
        nsAccEvent* thisEvent = mEvents[index];

        if (thisEvent->mEventType != tailEvent->mEventType)
          continue; // Different type

        // Skip event for application accessible since no coalescence for it
        // is supported. Ignore events unattached from DOM and events from
        // different documents since we can't coalesce them.
        if (!thisEvent->mNode || !thisEvent->mNode->IsInDoc() ||
            thisEvent->mNode->GetOwnerDoc() != tailEvent->mNode->GetOwnerDoc())
          continue;

        // If event queue contains an event of the same type and having target
        // that is sibling of target of newly appended event then apply its
        // event rule to the newly appended event.
        if (thisEvent->mNode->GetNodeParent() ==
            tailEvent->mNode->GetNodeParent()) {
          tailEvent->mEventRule = thisEvent->mEventRule;
          return;
        }

        // Specifies if this event target can be descendant of tail node.
        PRBool thisCanBeDescendantOfTail = PR_FALSE;

        // Coalesce depending on whether this event was coalesced or not.
        if (thisEvent->mEventRule == nsAccEvent::eDoNotEmit) {
          // If this event was coalesced then do not emit tail event iff tail
          // event has the same target or its target is contained by this event
          // target. Note, we don't need to check whether tail event target
          // contains this event target since this event was coalesced already.

          // As well we don't need to apply the calculated rule for siblings of
          // tail node because tail event rule was applied to possible tail
          // node siblings while this event was coalesced.

          if (thisEvent->mNode == tailEvent->mNode) {
            thisEvent->mEventRule = nsAccEvent::eDoNotEmit;
            return;
          }

        } else {
          // If this event wasn't coalesced already then try to coalesce it or
          // tail event. If this event is coalesced by tail event then continue
          // search through events other events that can be coalesced by tail
          // event.

          // If tail and this events have the same target then coalesce tail
          // event because more early event we should fire early and then stop
          // processing.
          if (thisEvent->mNode == tailEvent->mNode) {
            // Coalesce reorder events by special way since reorder events can
            // be conditional events (be or not be fired in the end).
            if (thisEvent->mEventType == nsIAccessibleEvent::EVENT_REORDER) {
              CoalesceReorderEventsFromSameSource(thisEvent, tailEvent);
              if (tailEvent->mEventRule != nsAccEvent::eDoNotEmit)
                continue;
            }
            else {
              tailEvent->mEventRule = nsAccEvent::eDoNotEmit;
            }

            return;
          }

          // This and tail events can be anywhere in the tree, make assumptions
          // for mutation events.

          // More older show event target (thisNode) can't be contained by
          // recent.
          // show event target (tailNode), i.e be a descendant of tailNode.
          // XXX: target of older show event caused by DOM node appending can be
          // contained by target of recent show event caused by style change.
          // XXX: target of older show event caused by style change can be
          // contained by target of recent show event caused by style change.
          thisCanBeDescendantOfTail =
            tailEvent->mEventType != nsIAccessibleEvent::EVENT_SHOW ||
            tailEvent->mIsAsync;
        }

        // Coalesce tail event if tail node is descendant of this node. Stop
        // processing if tail event is coalesced since all possible descendants
        // of this node was coalesced before.
        // Note: more older hide event target (thisNode) can't contain recent
        // hide event target (tailNode), i.e. be ancestor of tailNode. Skip
        // this check for hide events.
        if (tailEvent->mEventType != nsIAccessibleEvent::EVENT_HIDE &&
            nsCoreUtils::IsAncestorOf(thisEvent->mNode, tailEvent->mNode)) {

          if (thisEvent->mEventType == nsIAccessibleEvent::EVENT_REORDER) {
            CoalesceReorderEventsFromSameTree(thisEvent, tailEvent);
            if (tailEvent->mEventRule != nsAccEvent::eDoNotEmit)
              continue;

            return;
          }

          tailEvent->mEventRule = nsAccEvent::eDoNotEmit;
          return;
        }

#ifdef DEBUG
        if (tailEvent->mEventType == nsIAccessibleEvent::EVENT_HIDE &&
            nsCoreUtils::IsAncestorOf(thisEvent->mNode, tailEvent->mNode)) {
          NS_NOTREACHED("More older hide event target is an ancestor of recent hide event target!");
        }
#endif

        // If this node is a descendant of tail node then coalesce this event,
        // check other events in the queue.
        if (thisCanBeDescendantOfTail &&
            nsCoreUtils::IsAncestorOf(tailEvent->mNode, thisEvent->mNode)) {

          if (thisEvent->mEventType == nsIAccessibleEvent::EVENT_REORDER) {
            CoalesceReorderEventsFromSameTree(tailEvent, thisEvent);
            if (tailEvent->mEventRule != nsAccEvent::eDoNotEmit)
              continue;

            return;
          }

          // Do not emit thisEvent, also apply this result to sibling nodes of
          // thisNode.
          thisEvent->mEventRule = nsAccEvent::eDoNotEmit;
          ApplyToSiblings(0, index, thisEvent->mEventType,
                          thisEvent->mNode, nsAccEvent::eDoNotEmit);
          continue;
        }

#ifdef DEBUG
        if (!thisCanBeDescendantOfTail &&
            nsCoreUtils::IsAncestorOf(tailEvent->mNode, thisEvent->mNode)) {
          NS_NOTREACHED("Older event target is a descendant of recent event target!");
        }
#endif

      } // for (index)

    } break; // case eCoalesceFromSameSubtree

    case nsAccEvent::eCoalesceFromSameDocument:
    {
      // Used for focus event, coalesce more older event since focus event
      // for accessible can be duplicated by event for its document, we are
      // interested in focus event for accessible.
      for (PRInt32 index = tail - 1; index >= 0; index--) {
        nsAccEvent* thisEvent = mEvents[index];
        if (thisEvent->mEventType == tailEvent->mEventType &&
            thisEvent->mEventRule == tailEvent->mEventRule &&
            thisEvent->GetDocAccessible() == tailEvent->GetDocAccessible()) {
          thisEvent->mEventRule = nsAccEvent::eDoNotEmit;
          return;
        }
      }
    } break; // case eCoalesceFromSameDocument

    case nsAccEvent::eRemoveDupes:
    {
      // Check for repeat events, coalesce newly appended event by more older
      // event.
      for (PRInt32 index = tail - 1; index >= 0; index--) {
        nsAccEvent* accEvent = mEvents[index];
        if (accEvent->mEventType == tailEvent->mEventType &&
            accEvent->mEventRule == tailEvent->mEventRule &&
            accEvent->mNode == tailEvent->mNode) {
          tailEvent->mEventRule = nsAccEvent::eDoNotEmit;
          return;
        }
      }
    } break; // case eRemoveDupes

    default:
      break; // case eAllowDupes, eDoNotEmit
  } // switch
}

void
nsAccEventQueue::ApplyToSiblings(PRUint32 aStart, PRUint32 aEnd,
                                 PRUint32 aEventType, nsINode* aNode,
                                 nsAccEvent::EEventRule aEventRule)
{
  for (PRUint32 index = aStart; index < aEnd; index ++) {
    nsAccEvent* accEvent = mEvents[index];
    if (accEvent->mEventType == aEventType &&
        accEvent->mEventRule != nsAccEvent::eDoNotEmit &&
        accEvent->mNode->GetNodeParent() == aNode->GetNodeParent()) {
      accEvent->mEventRule = aEventRule;
    }
  }
}

void
nsAccEventQueue::CoalesceReorderEventsFromSameSource(nsAccEvent *aAccEvent1,
                                                     nsAccEvent *aAccEvent2)
{
  // Do not emit event2 if event1 is unconditional.
  nsAccReorderEvent *reorderEvent1 = downcast_accEvent(aAccEvent1);
  if (reorderEvent1->IsUnconditionalEvent()) {
    aAccEvent2->mEventRule = nsAccEvent::eDoNotEmit;
    return;
  }

  // Do not emit event1 if event2 is unconditional.
  nsAccReorderEvent *reorderEvent2 = downcast_accEvent(aAccEvent2);
  if (reorderEvent2->IsUnconditionalEvent()) {
    aAccEvent1->mEventRule = nsAccEvent::eDoNotEmit;
    return;
  }

  // Do not emit event2 if event1 is valid, otherwise do not emit event1.
  if (reorderEvent1->HasAccessibleInReasonSubtree())
    aAccEvent2->mEventRule = nsAccEvent::eDoNotEmit;
  else
    aAccEvent1->mEventRule = nsAccEvent::eDoNotEmit;
}

void
nsAccEventQueue::CoalesceReorderEventsFromSameTree(nsAccEvent *aAccEvent,
                                                   nsAccEvent *aDescendantAccEvent)
{
  // Do not emit descendant event if this event is unconditional.
  nsAccReorderEvent *reorderEvent = downcast_accEvent(aAccEvent);
  if (reorderEvent->IsUnconditionalEvent())
    aDescendantAccEvent->mEventRule = nsAccEvent::eDoNotEmit;
}
