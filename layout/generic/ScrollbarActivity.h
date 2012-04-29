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

#ifndef ScrollbarActivity_h___
#define ScrollbarActivity_h___

#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"

class nsIContent;
class nsIScrollbarHolder;
class nsITimer;
class nsIAtom;

namespace mozilla {
namespace layout {

/**
 * ScrollbarActivity
 *
 * This class manages scrollbar behavior that imitates the native Mac OS X
 * Lion overlay scrollbar behavior: Scrollbars are only shown while "scrollbar
 * activity" occurs, and they're hidden with a fade animation after a short
 * delay.
 *
 * Scrollbar activity has these states:
 *  - inactive:
 *      Scrollbars are hidden.
 *  - ongoing activity:
 *      Scrollbars are visible and being operated on in some way, for example
 *      because they're hovered or pressed.
 *  - active, but waiting for fade out
 *      Scrollbars are still completely visible but are about to fade away.
 *  - fading out
 *      Scrollbars are subject to a fade-out animation.
 *
 * Initial scrollbar activity needs to be reported by the scrollbar holder that
 * owns the ScrollbarActivity instance. This needs to happen via a call to
 * ActivityOccurred(), for example when the current scroll position or the size
 * of the scroll area changes.
 *
 * As soon as scrollbars are visible, the ScrollbarActivity class manages the
 * rest of the activity behavior: It ensures that mouse motions inside the
 * scroll area keep the scrollbars visible, and that scrollbars don't fade away
 * while they're being hovered / dragged. It also sets a sticky hover attribute
 * on the most recently hovered scrollbar.
 *
 * ScrollbarActivity falls into hibernation after the scrollbars have faded
 * out. It only starts acting after the next call to ActivityOccurred() /
 * ActivityStarted().
 */

class ScrollbarActivity : public nsIDOMEventListener,
                          public nsARefreshObserver {
public:
  ScrollbarActivity(nsIScrollbarHolder* aScrollableFrame)
   : mScrollableFrame(aScrollableFrame)
   , mNestedActivityCounter(0)
   , mIsActive(false)
   , mIsFading(false)
   , mListeningForEvents(false)
   , mHScrollbarHovered(false)
   , mVScrollbarHovered(false)
  {}

  NS_DECL_ISUPPORTS  
  NS_DECL_NSIDOMEVENTLISTENER

  void Destroy();

  void ActivityOccurred();
  void ActivityStarted();
  void ActivityStopped();

  virtual void WillRefresh(TimeStamp aTime);

  static void FadeBeginTimerFired(nsITimer* aTimer, void* aSelf) {
    reinterpret_cast<ScrollbarActivity*>(aSelf)->BeginFade();
  }

  static const PRUint32 kScrollbarFadeBeginDelay = 450; // milliseconds
  static const PRUint32 kScrollbarFadeDuration = 200; // milliseconds

protected:

  bool IsActivityOngoing()
  { return mNestedActivityCounter > 0; }
  bool IsStillFading(TimeStamp aTime);

  void HandleEventForScrollbar(const nsAString& aType,
                               nsIContent* aTarget,
                               nsIContent* aScrollbar,
                               bool* aStoredHoverState);

  void SetIsActive(bool aNewActive);
  void SetIsFading(bool aNewFading);

  void BeginFade();
  void EndFade();

  void StartFadeBeginTimer();
  void CancelFadeBeginTimer();
  void StartListeningForEvents();
  void StopListeningForEvents();
  void RegisterWithRefreshDriver();
  void UnregisterFromRefreshDriver();

  void UpdateOpacity(TimeStamp aTime);
  void HoveredScrollbar(nsIContent* aScrollbar);

  nsRefreshDriver* GetRefreshDriver();
  nsIContent* GetScrollbarContent(bool aVertical);
  nsIContent* GetHorizontalScrollbar() { return GetScrollbarContent(false); }
  nsIContent* GetVerticalScrollbar() { return GetScrollbarContent(true); }

  static const TimeDuration FadeDuration() {
    return TimeDuration::FromMilliseconds(kScrollbarFadeDuration);
  }

  nsIScrollbarHolder* mScrollableFrame;
  TimeStamp mFadeBeginTime;
  nsCOMPtr<nsITimer> mFadeBeginTimer;
  PRInt32 mNestedActivityCounter;
  bool mIsActive;
  bool mIsFading;
  bool mListeningForEvents;
  bool mHScrollbarHovered;
  bool mVScrollbarHovered;
};

} // namespace layout
} // namespace mozilla

#endif /* nsGfxScrollFrame_h___ */
