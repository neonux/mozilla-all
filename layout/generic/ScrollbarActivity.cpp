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
 * Markus Stange.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "ScrollbarActivity.h"
#include "nsIScrollbarHolder.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIFrame.h"
#include "nsContentUtils.h"
#include "nsAString.h"

namespace mozilla {
namespace layout {

NS_IMPL_ISUPPORTS1(ScrollbarActivity, nsIDOMEventListener)

void
ScrollbarActivity::Destroy()
{
  StopListeningForEvents();
  UnregisterFromRefreshDriver();
  CancelFadeBeginTimer();
}

void
ScrollbarActivity::ActivityOccurred()
{
  ActivityStarted();
  ActivityStopped();
}

void
ScrollbarActivity::ActivityStarted()
{
  mNestedActivityCounter++;
  CancelFadeBeginTimer();
  SetIsFading(false);
  UnregisterFromRefreshDriver();
  StartListeningForEvents();
  SetIsActive(true);

  NS_ASSERTION(mIsActive, "need to be active during activity");
  NS_ASSERTION(!mIsFading, "must not be fading during activity");
  NS_ASSERTION(!mFadeBeginTimer, "fade begin timer shouldn't be running");
}

void
ScrollbarActivity::ActivityStopped()
{
  NS_ASSERTION(IsActivityOngoing(), "activity stopped while none was going on");
  NS_ASSERTION(mIsActive, "need to be active during activity");
  NS_ASSERTION(!mIsFading, "must not be fading during ongoing activity");
  NS_ASSERTION(!mFadeBeginTimer, "must not be waiting for fade during ongoing activity");

  mNestedActivityCounter--;

  if (!IsActivityOngoing()) {
    StartFadeBeginTimer();

    NS_ASSERTION(mIsActive, "need to be active right after activity");
    NS_ASSERTION(!mIsFading, "must not be fading right after activity");
    NS_ASSERTION(mFadeBeginTimer, "fade begin timer should be running");
  }
}

NS_IMETHODIMP
ScrollbarActivity::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!mIsActive)
    return NS_OK;

  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("mousemove")) {
    // Mouse motions anywhere in the scrollable frame should keep the
    // scrollbars visible.
    ActivityOccurred();
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNSEvent> event = do_QueryInterface(aEvent);
  if (!event)
    return NS_OK;

  nsCOMPtr<nsIDOMEventTarget> target;
  event->GetOriginalTarget(getter_AddRefs(target));
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);

  HandleEventForScrollbar(type, targetContent, GetHorizontalScrollbar(),
                          &mHScrollbarHovered);
  HandleEventForScrollbar(type, targetContent, GetVerticalScrollbar(),
                          &mVScrollbarHovered);

  return NS_OK;
}

void
ScrollbarActivity::WillRefresh(TimeStamp aTime)
{
  NS_ASSERTION(mIsActive, "should only fade while scrollbars are visible");
  NS_ASSERTION(!IsActivityOngoing(), "why weren't we unregistered from the refresh driver when scrollbar activity started?");
  NS_ASSERTION(mIsFading, "should only animate fading during fade");

  UpdateOpacity(aTime);

  if (!IsStillFading(aTime)) {
    EndFade();
  }
}

bool
ScrollbarActivity::IsStillFading(TimeStamp aTime)
{
  return !mFadeBeginTime.IsNull() && (aTime - mFadeBeginTime < FadeDuration());
}

void
ScrollbarActivity::HandleEventForScrollbar(const nsAString& aType,
                                           nsIContent* aTarget,
                                           nsIContent* aScrollbar,
                                           bool* aStoredHoverState)
{
  if (!aTarget || !aScrollbar ||
      !nsContentUtils::ContentIsDescendantOf(aTarget, aScrollbar))
    return;

  if (aType.EqualsLiteral("mousedown")) {
    ActivityStarted();
  } else if (aType.EqualsLiteral("mouseup")) {
    ActivityStopped();
  } else if (aType.EqualsLiteral("mouseover") ||
             aType.EqualsLiteral("mouseout")) {
    bool newHoveredState = aType.EqualsLiteral("mouseover");
    if (newHoveredState && !*aStoredHoverState) {
      ActivityStarted();
      HoveredScrollbar(aScrollbar);
    } else if (*aStoredHoverState && !newHoveredState) {
      ActivityStopped();
      // Don't call HoveredScrollbar(nsnull) here because we want the hover
      // attribute to stick until the scrollbars are hidden.
    }
    *aStoredHoverState = newHoveredState;
  }
}

void
ScrollbarActivity::StartListeningForEvents()
{
  if (mListeningForEvents)
    return;

  nsCOMPtr<nsIDOMEventTarget> target = mScrollableFrame->GetEventTarget();
  if (!target)
    return;

  target->AddEventListener(NS_LITERAL_STRING("mousemove"), this, true);
  target->AddEventListener(NS_LITERAL_STRING("mousedown"), this, true);
  target->AddEventListener(NS_LITERAL_STRING("mouseup"), this, true);
  target->AddEventListener(NS_LITERAL_STRING("mouseover"), this, true);
  target->AddEventListener(NS_LITERAL_STRING("mouseout"), this, true);
  mListeningForEvents = true;
}

void
ScrollbarActivity::StopListeningForEvents()
{
  if (!mListeningForEvents)
    return;

  nsCOMPtr<nsIDOMEventTarget> target = mScrollableFrame->GetEventTarget();
  if (!target) {
    NS_WARNING("scrollable frame doesn't provide an event target");
    return;
  }

  target->RemoveEventListener(NS_LITERAL_STRING("mousemove"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("mousedown"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("mouseup"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("mouseover"), this, true);
  target->RemoveEventListener(NS_LITERAL_STRING("mouseout"), this, true);
  mListeningForEvents = false;
}

void
ScrollbarActivity::BeginFade()
{
  NS_ASSERTION(mIsActive, "can't begin fade when we're already inactive");
  NS_ASSERTION(!IsActivityOngoing(), "why wasn't the fade begin timer cancelled when scrollbar activity started?");
  NS_ASSERTION(!mIsFading, "shouldn't be fading just yet");

  CancelFadeBeginTimer();
  mFadeBeginTime = TimeStamp::Now();
  SetIsFading(true);
  RegisterWithRefreshDriver();

  NS_ASSERTION(mIsActive, "only fade while scrollbars are visible");
  NS_ASSERTION(mIsFading, "should be fading now");
}

void
ScrollbarActivity::EndFade()
{
  NS_ASSERTION(mIsActive, "still need to be active at this point");
  NS_ASSERTION(!IsActivityOngoing(), "why wasn't the fade end timer cancelled when scrollbar activity started?");

  SetIsFading(false);
  SetIsActive(false);
  UnregisterFromRefreshDriver();
  StopListeningForEvents();

  NS_ASSERTION(!mIsActive, "should have gone inactive after fade end");
  NS_ASSERTION(!mIsFading, "shouldn't be fading anymore");
  NS_ASSERTION(!mFadeBeginTimer, "fade begin timer shouldn't be running");
}

void
ScrollbarActivity::RegisterWithRefreshDriver()
{
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (refreshDriver) {
    refreshDriver->AddRefreshObserver(this, Flush_Style);
  }
}

void
ScrollbarActivity::UnregisterFromRefreshDriver()
{
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (refreshDriver) {
    refreshDriver->RemoveRefreshObserver(this, Flush_Style);
  }
}

static void
SetBooleanAttribute(nsIContent* aContent, nsIAtom* aAttribute, bool aValue)
{
  if (aContent) {
    if (aValue) {
      aContent->SetAttr(kNameSpaceID_None, aAttribute,
                        NS_LITERAL_STRING("true"), true);
    } else {
      aContent->UnsetAttr(kNameSpaceID_None, aAttribute, true);
    }
  }
}

void
ScrollbarActivity::SetIsActive(bool aNewActive)
{
  if (mIsActive == aNewActive)
    return;

  mIsActive = aNewActive;
  if (!mIsActive) {
    // Clear sticky scrollbar hover status.
    HoveredScrollbar(nsnull);
  }

  SetBooleanAttribute(GetHorizontalScrollbar(), nsGkAtoms::active, mIsActive);
  SetBooleanAttribute(GetVerticalScrollbar(), nsGkAtoms::active, mIsActive);
}

static void
SetOpacityOnElement(nsIContent* aContent, double aOpacity)
{
  nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyleContent =
    do_QueryInterface(aContent);
  if (inlineStyleContent) {
    nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
    inlineStyleContent->GetStyle(getter_AddRefs(decl));
    if (decl) {
      nsAutoString str;
      str.AppendFloat(aOpacity);
      decl->SetProperty(NS_LITERAL_STRING("opacity"), str, EmptyString());
    }
  }
}

void
ScrollbarActivity::UpdateOpacity(TimeStamp aTime)
{
  double progress = (aTime - mFadeBeginTime) / FadeDuration();
  double opacity = 1.0 - NS_MAX(0.0, NS_MIN(1.0, progress));
  SetOpacityOnElement(GetHorizontalScrollbar(), opacity);
  SetOpacityOnElement(GetVerticalScrollbar(), opacity);
}

static void
UnsetOpacityOnElement(nsIContent* aContent)
{
  nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyleContent =
    do_QueryInterface(aContent);
  if (inlineStyleContent) {
    nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
    inlineStyleContent->GetStyle(getter_AddRefs(decl));
    if (decl) {
      nsAutoString dummy;
      decl->RemoveProperty(NS_LITERAL_STRING("opacity"), dummy);
    }
  }
}

void
ScrollbarActivity::SetIsFading(bool aNewFading)
{
  if (mIsFading == aNewFading)
    return;

  mIsFading = aNewFading;
  if (!mIsFading) {
    mFadeBeginTime = TimeStamp();
    UnsetOpacityOnElement(GetHorizontalScrollbar());
    UnsetOpacityOnElement(GetVerticalScrollbar());
  }
}

void
ScrollbarActivity::StartFadeBeginTimer()
{
  NS_ASSERTION(!mFadeBeginTimer, "timer already alive!");
  mFadeBeginTimer = do_CreateInstance("@mozilla.org/timer;1");
  mFadeBeginTimer->InitWithFuncCallback(FadeBeginTimerFired, this,
                                        kScrollbarFadeBeginDelay,
                                        nsITimer::TYPE_ONE_SHOT);
}

void
ScrollbarActivity::CancelFadeBeginTimer()
{
  if (mFadeBeginTimer) {
    mFadeBeginTimer->Cancel();
    mFadeBeginTimer = nsnull;
  }
}

void
ScrollbarActivity::HoveredScrollbar(nsIContent* aScrollbar)
{
  SetBooleanAttribute(GetHorizontalScrollbar(), nsGkAtoms::hover, false);
  SetBooleanAttribute(GetVerticalScrollbar(), nsGkAtoms::hover, false);
  SetBooleanAttribute(aScrollbar, nsGkAtoms::hover, true);
}

nsRefreshDriver*
ScrollbarActivity::GetRefreshDriver()
{
  nsIFrame* box = mScrollableFrame->GetScrollbarBox(false);
  if (!box) {
    box = mScrollableFrame->GetScrollbarBox(true);
  }
  return box ? box->PresContext()->RefreshDriver() : nsnull;
}

nsIContent*
ScrollbarActivity::GetScrollbarContent(bool aVertical)
{
  nsIFrame* box = mScrollableFrame->GetScrollbarBox(aVertical);
  return box ? box->GetContent() : nsnull;
}

} // namespace layout
} // namespace mozilla
