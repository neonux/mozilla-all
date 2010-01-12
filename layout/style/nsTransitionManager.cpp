/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is nsTransitionManager.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* Code to start and animate CSS transitions. */

#include "nsTransitionManager.h"
#include "nsIContent.h"
#include "nsStyleContext.h"
#include "nsCSSProps.h"
#include "mozilla/TimeStamp.h"
#include "nsRefreshDriver.h"
#include "nsRuleProcessorData.h"
#include "nsIStyleRule.h"
#include "nsRuleWalker.h"
#include "nsRuleData.h"
#include "nsSMILKeySpline.h"
#include "gfxColor.h"
#include "nsCSSPropertySet.h"
#include "nsStyleAnimation.h"
#include "nsCSSDataBlock.h"
#include "nsEventDispatcher.h"
#include "nsGUIEvent.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;

/*****************************************************************************
 * Per-Element data                                                          *
 *****************************************************************************/

struct ElementPropertyTransition
{
  nsCSSProperty mProperty;
  // We need to have mCurrentValue as a member of this structure because
  // the result of the calls to |Interpolate| might hold data that this
  // object's owning style rule needs to keep alive (after calling
  // UncomputeValue on it in MapRuleInfoInto).
  nsStyleAnimation::Value mStartValue, mEndValue, mCurrentValue;
  TimeStamp mStartTime; // actual start plus transition delay

  // data from the relevant nsTransition
  TimeDuration mDuration;
  nsSMILKeySpline mTimingFunction;

  PRBool IsRemovedSentinel() const
  {
    return mStartTime.IsNull();
  }

  void SetRemovedSentinel()
  {
    // assign the null time stamp
    mStartTime = TimeStamp();
  }
};

/**
 * An ElementTransitionsStyleRule overrides style data with the
 * currently-transitioning value for an element that is executing a
 * transition.  It only matches when styling with animation.  When we
 * style without animation, we need to not use it so that we can detect
 * any new changes; if necessary we restyle immediately afterwards with
 * animation.
 */
class ElementTransitionsStyleRule : public nsIStyleRule
{
public:
  // nsISupportsImplementation
  NS_DECL_ISUPPORTS

  // nsIStyleRule implementation
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  ElementTransitionsStyleRule(ElementTransitions *aOwner,
                           TimeStamp aRefreshTime)
    : mElementTransitions(aOwner)
    , mRefreshTime(aRefreshTime)
  {}

  void Disconnect() { mElementTransitions = nsnull; }

  ElementTransitions *ElementData() { return mElementTransitions; }
  TimeStamp RefreshTime() { return mRefreshTime; }

private:
  ElementTransitions *mElementTransitions;
  // The time stamp for which this style rule is valid.
  TimeStamp mRefreshTime;
};

/**
 * A CoverTransitionStyleRule sets any value for which we're starting a
 * transition back to the pre-transition value for the period when we're
 * resolving style on its descendants, so that we have the required
 * behavior for initiating transitions on such descendants.  For more
 * detail, see comment below, above "new CoverTransitionStartStyleRule".
 */
class CoverTransitionStartStyleRule : public nsIStyleRule
{
public:
  // nsISupportsImplementation
  NS_DECL_ISUPPORTS

  // nsIStyleRule implementation
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);
#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  NS_HIDDEN_(void) CoverValue(nsCSSProperty aProperty,
                              nsStyleAnimation::Value &aStartValue)
  {
    CoveredValue v = { aProperty, aStartValue };
    mCoveredValues.AppendElement(v);
  }

  struct CoveredValue {
    nsCSSProperty mProperty;
    nsStyleAnimation::Value mCoveredValue;
  };

private:
  nsTArray<CoveredValue> mCoveredValues;
};

struct ElementTransitions : public PRCList
{
  ElementTransitions(nsIContent *aElement, nsIAtom *aElementProperty,
                     nsTransitionManager *aTransitionManager)
    : mElement(aElement)
    , mElementProperty(aElementProperty)
    , mTransitionManager(aTransitionManager)
  {
    PR_INIT_CLIST(this);
  }
  ~ElementTransitions()
  {
    DropStyleRule();
    PR_REMOVE_LINK(this);
    mTransitionManager->TransitionsRemoved();
  }

  void Destroy()
  {
    // This will call our destructor.
    mElement->DeleteProperty(mElementProperty);
  }

  void DropStyleRule();
  PRBool EnsureStyleRuleFor(TimeStamp aRefreshTime);


  // Either zero or one for each CSS property:
  nsTArray<ElementPropertyTransition> mPropertyTransitions;

  // The style rule for the transitions (which contains the time stamp
  // for which it is valid).
  nsRefPtr<ElementTransitionsStyleRule> mStyleRule;

  nsIContent *mElement;

  // the atom we use in mElement's prop table (must be a static atom,
  // i.e., in an atom list)
  nsIAtom *mElementProperty;

  nsTransitionManager *mTransitionManager;
};

static void
ElementTransitionsPropertyDtor(void           *aObject,
                               nsIAtom        *aPropertyName,
                               void           *aPropertyValue,
                               void           *aData)
{
  ElementTransitions *et = static_cast<ElementTransitions*>(aPropertyValue);
  delete et;
}

NS_IMPL_ISUPPORTS1(ElementTransitionsStyleRule, nsIStyleRule)

NS_IMETHODIMP
ElementTransitionsStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  nsStyleContext *contextParent = aRuleData->mStyleContext->GetParent();
  if (contextParent && contextParent->HasPseudoElementData()) {
    // Don't apply transitions to things inside of pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.
    return NS_OK;
  }

  ElementTransitions *et = ElementData();
  NS_ENSURE_TRUE(et, NS_OK); // FIXME (Bug 522597): Why can this be null?
  for (PRUint32 i = 0, i_end = et->mPropertyTransitions.Length();
       i < i_end; ++i)
  {
    ElementPropertyTransition &pt = et->mPropertyTransitions[i];
    if (pt.IsRemovedSentinel()) {
      continue;
    }

    if (aRuleData->mSIDs & nsCachedStyleData::GetBitForSID(
                             nsCSSProps::kSIDTable[pt.mProperty]))
    {
      double timePortion =
        (RefreshTime() - pt.mStartTime).ToSeconds() / pt.mDuration.ToSeconds();
      if (timePortion < 0.0)
        timePortion = 0.0; // use start value during transition-delay
      if (timePortion > 1.0)
        timePortion = 1.0; // we might be behind on flushing

      double valuePortion =
        pt.mTimingFunction.GetSplineValue(timePortion);
#ifdef DEBUG
      PRBool ok =
#endif
        nsStyleAnimation::Interpolate(pt.mProperty,
                                      pt.mStartValue, pt.mEndValue,
                                      valuePortion, pt.mCurrentValue);
      NS_ABORT_IF_FALSE(ok, "could not interpolate values");

      void *prop =
        nsCSSExpandedDataBlock::RuleDataPropertyAt(aRuleData, pt.mProperty);
#ifdef DEBUG
      ok =
#endif
        nsStyleAnimation::UncomputeValue(pt.mProperty, aRuleData->mPresContext,
                                         pt.mCurrentValue, prop);
      NS_ABORT_IF_FALSE(ok, "could not store computed value");
    }
  }

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
ElementTransitionsStyleRule::List(FILE* out, PRInt32 aIndent) const
{
  // WRITE ME?
  return NS_OK;
}
#endif

void
ElementTransitions::DropStyleRule()
{
  if (mStyleRule) {
    mStyleRule->Disconnect();
    mStyleRule = nsnull;
  }
}

PRBool
ElementTransitions::EnsureStyleRuleFor(TimeStamp aRefreshTime)
{
  if (!mStyleRule || mStyleRule->RefreshTime() != aRefreshTime) {
    DropStyleRule();

    ElementTransitionsStyleRule *newRule =
      new ElementTransitionsStyleRule(this, aRefreshTime);
    if (!newRule) {
      NS_WARNING("out of memory");
      return PR_FALSE;
    }

    mStyleRule = newRule;
  }

  return PR_TRUE;
}

NS_IMPL_ISUPPORTS1(CoverTransitionStartStyleRule, nsIStyleRule)

NS_IMETHODIMP
CoverTransitionStartStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  for (PRUint32 i = 0, i_end = mCoveredValues.Length(); i < i_end; ++i) {
    CoveredValue &cv = mCoveredValues[i];
    if (aRuleData->mSIDs & nsCachedStyleData::GetBitForSID(
                             nsCSSProps::kSIDTable[cv.mProperty]))
    {
      void *prop =
        nsCSSExpandedDataBlock::RuleDataPropertyAt(aRuleData, cv.mProperty);
#ifdef DEBUG
      PRBool ok =
#endif
        nsStyleAnimation::UncomputeValue(cv.mProperty, aRuleData->mPresContext,
                                         cv.mCoveredValue, prop);
      NS_ABORT_IF_FALSE(ok, "could not store computed value");
    }
  }

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
CoverTransitionStartStyleRule::List(FILE* out, PRInt32 aIndent) const
{
  // WRITE ME?
  return NS_OK;
}
#endif

/*****************************************************************************
 * nsTransitionManager                                                       *
 *****************************************************************************/

nsTransitionManager::nsTransitionManager(nsPresContext *aPresContext)
  : mPresContext(aPresContext)
{
  PR_INIT_CLIST(&mElementTransitions);
}

nsTransitionManager::~nsTransitionManager()
{
  NS_ABORT_IF_FALSE(!mPresContext, "Disconnect should have been called");
}

void
nsTransitionManager::Disconnect()
{
  // Content nodes might outlive the transition manager.
  while (!PR_CLIST_IS_EMPTY(&mElementTransitions)) {
    ElementTransitions *head = static_cast<ElementTransitions*>(
                                 PR_LIST_HEAD(&mElementTransitions));
    head->Destroy();
  }

  mPresContext = nsnull;
}

static PRBool
TransExtractComputedValue(nsCSSProperty aProperty,
                          nsStyleContext* aStyleContext,
                          nsStyleAnimation::Value& aComputedValue)
{
  PRBool result =
    nsStyleAnimation::ExtractComputedValue(aProperty, aStyleContext,
                                           aComputedValue);
  if (aProperty == eCSSProperty_visibility) {
    NS_ABORT_IF_FALSE(aComputedValue.GetUnit() ==
                        nsStyleAnimation::eUnit_Enumerated,
                      "unexpected unit");
    aComputedValue.SetIntValue(aComputedValue.GetIntValue(),
                               nsStyleAnimation::eUnit_Visibility);
  }
  return result;
}

already_AddRefed<nsIStyleRule>
nsTransitionManager::StyleContextChanged(nsIContent *aElement,
                                         nsStyleContext *aOldStyleContext,
                                         nsStyleContext *aNewStyleContext)
{
  NS_PRECONDITION(aOldStyleContext->GetPseudo() ==
                      aNewStyleContext->GetPseudo(),
                  "pseudo type mismatch");
  // If we were called from ReParentStyleContext, this assertion would
  // actually fire.  If we need to be called from there, we can probably
  // just remove it; the condition probably isn't critical, although
  // it's worth thinking about some more.
  NS_PRECONDITION(aOldStyleContext->HasPseudoElementData() ==
                      aNewStyleContext->HasPseudoElementData(),
                  "pseudo type mismatch");

  // NOTE: Things in this function (and ConsiderStartingTransition)
  // should never call PeekStyleData because we don't preserve gotten
  // structs across reframes.

  // Return sooner (before the startedAny check below) for the most
  // common case: no transitions specified or running.
  const nsStyleDisplay *disp = aNewStyleContext->GetStyleDisplay();
  nsCSSPseudoElements::Type pseudoType = aNewStyleContext->GetPseudoType();
  ElementTransitions *et =
      GetElementTransitions(aElement, pseudoType, PR_FALSE);
  if (!et &&
      disp->mTransitionPropertyCount == 1 &&
      disp->mTransitions[0].GetDelay() == 0.0f &&
      disp->mTransitions[0].GetDuration() == 0.0f) {
    return nsnull;
  }      


  if (aNewStyleContext->PresContext()->IsProcessingAnimationStyleChange()) {
    return nsnull;
  }
  
  if (pseudoType != nsCSSPseudoElements::ePseudo_NotPseudoElement &&
      pseudoType != nsCSSPseudoElements::ePseudo_before &&
      pseudoType != nsCSSPseudoElements::ePseudo_after) {
    return nsnull;
  }
  if (aNewStyleContext->GetParent() &&
      aNewStyleContext->GetParent()->HasPseudoElementData()) {
    // Ignore transitions on things that inherit properties from
    // pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.
    return nsnull;
  }

  // Per http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html
  // I'll consider only the transitions from the number of items in
  // 'transition-property' on down, and later ones will override earlier
  // ones (tracked using |whichStarted|).
  PRBool startedAny = PR_FALSE;
  nsCSSPropertySet whichStarted;
  for (PRUint32 i = disp->mTransitionPropertyCount; i-- != 0; ) {
    const nsTransition& t = disp->mTransitions[i];
    // Check delay and duration first, since they default to zero, and
    // when they're both zero, we can ignore the transition.
    if (t.GetDelay() != 0.0f || t.GetDuration() != 0.0f) {
      // We might have something to transition.  See if any of the
      // properties in question changed and are animatable.
      // FIXME: Would be good to find a way to share code between this
      // interpretation of transition-property and the one below.
      nsCSSProperty property = t.GetProperty();
      if (property == eCSSPropertyExtra_no_properties ||
          property == eCSSProperty_UNKNOWN) {
        // Nothing to do, but need to exclude this from cases below.
      } else if (property == eCSSPropertyExtra_all_properties) {
        for (nsCSSProperty p = nsCSSProperty(0); 
             p < eCSSProperty_COUNT_no_shorthands;
             p = nsCSSProperty(p + 1)) {
          ConsiderStartingTransition(p, t, aElement, et,
                                     aOldStyleContext, aNewStyleContext,
                                     &startedAny, &whichStarted);
        }
      } else if (nsCSSProps::IsShorthand(property)) {
        CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, property) {
          ConsiderStartingTransition(*subprop, t, aElement, et,
                                     aOldStyleContext, aNewStyleContext,
                                     &startedAny, &whichStarted);
        }
      } else {
        ConsiderStartingTransition(property, t, aElement, et,
                                   aOldStyleContext, aNewStyleContext,
                                   &startedAny, &whichStarted);
      }
    }
  }

  // Stop any transitions for properties that are no longer in
  // 'transition-property'.
  // Also stop any transitions for properties that just changed (and are
  // still in the set of properties to transition), but we didn't just
  // start the transition because delay and duration are both zero.
  if (et) {
    PRBool checkProperties =
      disp->mTransitions[0].GetProperty() != eCSSPropertyExtra_all_properties;
    nsCSSPropertySet allTransitionProperties;
    if (checkProperties) {
      for (PRUint32 i = disp->mTransitionPropertyCount; i-- != 0; ) {
        const nsTransition& t = disp->mTransitions[i];
        // FIXME: Would be good to find a way to share code between this
        // interpretation of transition-property and the one above.
        nsCSSProperty property = t.GetProperty();
        if (property == eCSSPropertyExtra_no_properties ||
            property == eCSSProperty_UNKNOWN) {
          // Nothing to do, but need to exclude this from cases below.
        } else if (property == eCSSPropertyExtra_all_properties) {
          for (nsCSSProperty p = nsCSSProperty(0); 
               p < eCSSProperty_COUNT_no_shorthands;
               p = nsCSSProperty(p + 1)) {
            allTransitionProperties.AddProperty(p);
          }
        } else if (nsCSSProps::IsShorthand(property)) {
          CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, property) {
            allTransitionProperties.AddProperty(*subprop);
          }
        } else {
          allTransitionProperties.AddProperty(property);
        }
      }
    }

    nsTArray<ElementPropertyTransition> &pts = et->mPropertyTransitions;
    PRUint32 i = pts.Length();
    NS_ABORT_IF_FALSE(i != 0, "empty transitions list?");
    nsStyleAnimation::Value currentValue;
    do {
      --i;
      ElementPropertyTransition &pt = pts[i];
          // properties no longer in 'transition-property'
      if ((checkProperties &&
           !allTransitionProperties.HasProperty(pt.mProperty)) ||
          // properties whose computed values changed but delay and
          // duration are both zero
          !TransExtractComputedValue(pt.mProperty, aNewStyleContext,
                                     currentValue) ||
          currentValue != pt.mEndValue) {
        // stop the transition
        pts.RemoveElementAt(i);
      }
    } while (i != 0);

    if (pts.IsEmpty()) {
      et->Destroy();
      et = nsnull;
    }
  }

  if (!startedAny) {
    return nsnull;
  }

  NS_ABORT_IF_FALSE(et, "must have element transitions if we started "
                        "any transitions");

  // In the CSS working group discussion (2009 Jul 15 telecon,
  // http://www.w3.org/mid/4A5E1470.4030904@inkedblade.net ) of
  // http://lists.w3.org/Archives/Public/www-style/2009Jun/0121.html ,
  // the working group decided that a transition property on an
  // element should not cause any transitions if the property change
  // is itself inheriting a value that is transitioning on an
  // ancestor.  So, to get the correct behavior, we continue the
  // restyle that caused this transition using a "covering" rule that
  // covers up any changes on which we started transitions, so that
  // descendants don't start their own transitions.  (In the case of
  // negative transition delay, this covering rule produces different
  // results than applying the transition rule immediately would).
  // Our caller is responsible for restyling again using this covering
  // rule.

  nsRefPtr<CoverTransitionStartStyleRule> coverRule =
    new CoverTransitionStartStyleRule;
  if (!coverRule) {
    NS_WARNING("out of memory");
    return nsnull;
  }
  
  nsTArray<ElementPropertyTransition> &pts = et->mPropertyTransitions;
  for (PRUint32 i = 0, i_end = pts.Length(); i < i_end; ++i) {
    ElementPropertyTransition &pt = pts[i];
    if (whichStarted.HasProperty(pt.mProperty)) {
      coverRule->CoverValue(pt.mProperty, pt.mStartValue);
    }
  }

  return already_AddRefed<nsIStyleRule>(
           static_cast<nsIStyleRule*>(coverRule.forget().get()));
}

void
nsTransitionManager::ConsiderStartingTransition(nsCSSProperty aProperty,
                       const nsTransition& aTransition,
                       nsIContent *aElement,
                       ElementTransitions *&aElementTransitions,
                       nsStyleContext *aOldStyleContext,
                       nsStyleContext *aNewStyleContext,
                       PRBool *aStartedAny,
                       nsCSSPropertySet *aWhichStarted)
{
  // IsShorthand itself will assert if aProperty is not a property.
  NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(aProperty),
                    "property out of range");

  if (aWhichStarted->HasProperty(aProperty)) {
    // A later item in transition-property already started a
    // transition for this property, so we ignore this one.
    // See comment above and
    // http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html .
    return;
  }

  if (nsCSSProps::kAnimTypeTable[aProperty] == eStyleAnimType_None) {
    return;
  }

  ElementPropertyTransition pt;
  nsStyleAnimation::Value dummyValue;
  PRBool shouldAnimate =
    TransExtractComputedValue(aProperty, aOldStyleContext, pt.mStartValue) &&
    TransExtractComputedValue(aProperty, aNewStyleContext, pt.mEndValue) &&
    pt.mStartValue != pt.mEndValue &&
    // Check that we can interpolate between these values
    // (If this is ever a performance problem, we could add a
    // CanInterpolate method, but it seems fine for now.)
    nsStyleAnimation::Interpolate(aProperty, pt.mStartValue, pt.mEndValue,
                                  0.5, dummyValue);

  PRUint32 currentIndex = nsTArray<ElementPropertyTransition>::NoIndex;
  if (aElementTransitions) {
    nsTArray<ElementPropertyTransition> &pts =
      aElementTransitions->mPropertyTransitions;
    for (PRUint32 i = 0, i_end = pts.Length(); i < i_end; ++i) {
      if (pts[i].mProperty == aProperty) {
        currentIndex = i;
        break;
      }
    }
  }

  nsPresContext *presContext = aNewStyleContext->PresContext();

  if (!shouldAnimate) {
    if (currentIndex != nsTArray<ElementPropertyTransition>::NoIndex) {
      // We're in the middle of a transition, but just got a
      // non-transition style change changing to exactly the
      // current in-progress value.   (This is quite easy to cause
      // using 'transition-delay'.)
      nsTArray<ElementPropertyTransition> &pts =
        aElementTransitions->mPropertyTransitions;
      pts.RemoveElementAt(currentIndex);
      if (pts.IsEmpty()) {
        aElementTransitions->Destroy();
        // |aElementTransitions| is now a dangling pointer!
        aElementTransitions = nsnull;
      }
      // WalkTransitionRule already called RestyleForAnimation.
    }
    return;
  }

  // When we interrupt a running transition, we want to reduce the
  // duration of the new transition *if* the new transition would have
  // been longer had it started from the endpoint of the currently
  // running transition.
  double durationFraction = 1.0;

  // We need to check two things if we have a currently running
  // transition for this property:  see durationFraction comment above
  // and the endpoint check below.
  if (currentIndex != nsTArray<ElementPropertyTransition>::NoIndex) {
    const ElementPropertyTransition &oldPT =
      aElementTransitions->mPropertyTransitions[currentIndex];

    if (oldPT.mEndValue == pt.mEndValue) {
      // If we got a style change that changed the value to the endpoint
      // of the currently running transition, we don't want to interrupt
      // its timing function.
      // WalkTransitionRule already called RestyleForAnimation.
      return;
    }

    double fullDistance, remainingDistance;
#ifdef DEBUG
    PRBool ok =
#endif
      nsStyleAnimation::ComputeDistance(aProperty, pt.mStartValue,
                                        pt.mEndValue, fullDistance);
    NS_ABORT_IF_FALSE(ok, "could not compute distance");
    NS_ABORT_IF_FALSE(fullDistance >= 0.0, "distance must be positive");

    if (!oldPT.IsRemovedSentinel() &&
        nsStyleAnimation::ComputeDistance(aProperty, oldPT.mEndValue,
                                          pt.mEndValue, remainingDistance)) {
      NS_ABORT_IF_FALSE(remainingDistance >= 0.0, "distance must be positive");
      durationFraction = fullDistance / remainingDistance;
      if (durationFraction > 1.0) {
        durationFraction = 1.0;
      }
    }
  }


  nsRefreshDriver *rd = presContext->RefreshDriver();

  pt.mProperty = aProperty;
  float delay = aTransition.GetDelay();
  float duration = aTransition.GetDuration();
  if (durationFraction != 1.0) {
    // Negative delays are essentially part of the transition
    // function, so reduce them along with the duration, but don't
    // reduce positive delays.  (See comment above about
    // durationFraction.)
    if (delay < 0.0f)
        delay *= durationFraction;
    duration *= durationFraction;
  }
  pt.mStartTime = rd->MostRecentRefresh() +
                  TimeDuration::FromMilliseconds(delay);
  pt.mDuration = TimeDuration::FromMilliseconds(duration);
  const nsTimingFunction &tf = aTransition.GetTimingFunction();
  pt.mTimingFunction.Init(tf.mX1, tf.mY1, tf.mX2, tf.mY2);

  if (!aElementTransitions) {
    aElementTransitions =
      GetElementTransitions(aElement, aNewStyleContext->GetPseudoType(),
                            PR_TRUE);
    if (!aElementTransitions) {
      NS_WARNING("allocating ElementTransitions failed");
      return;
    }
  }
  
  nsTArray<ElementPropertyTransition> &pts =
    aElementTransitions->mPropertyTransitions;
#ifdef DEBUG
  for (PRUint32 i = 0, i_end = pts.Length(); i < i_end; ++i) {
    NS_ABORT_IF_FALSE(i == currentIndex ||
                      pts[i].mProperty != aProperty,
                      "duplicate transitions for property");
  }
#endif
  if (currentIndex != nsTArray<ElementPropertyTransition>::NoIndex) {
    pts[currentIndex] = pt;
  } else {
    if (!pts.AppendElement(pt)) {
      NS_WARNING("out of memory");
      return;
    }
  }

  presContext->PresShell()->RestyleForAnimation(aElement);

  *aStartedAny = PR_TRUE;
  aWhichStarted->AddProperty(aProperty);
}

ElementTransitions*
nsTransitionManager::GetElementTransitions(nsIContent *aElement,
                                           nsCSSPseudoElements::Type aPseudoType,
                                           PRBool aCreateIfNeeded)
{
  if (!aCreateIfNeeded && PR_CLIST_IS_EMPTY(&mElementTransitions)) {
    // Early return for the most common case.
    return nsnull;
  }

  nsIAtom *propName;
  if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    propName = nsGkAtoms::transitionsProperty;
  } else if (aPseudoType == nsCSSPseudoElements::ePseudo_before) {
    propName = nsGkAtoms::transitionsOfBeforeProperty;
  } else if (aPseudoType == nsCSSPseudoElements::ePseudo_after) {
    propName = nsGkAtoms::transitionsOfAfterProperty;
  } else {
    NS_ASSERTION(!aCreateIfNeeded,
                 "should never try to create transitions for pseudo "
                 "other than :before or :after");
    return nsnull;
  }
  ElementTransitions *et = static_cast<ElementTransitions*>(
                             aElement->GetProperty(propName));
  if (!et && aCreateIfNeeded) {
    // FIXME: Consider arena-allocating?
    et = new ElementTransitions(aElement, propName, this);
    if (!et) {
      NS_WARNING("out of memory");
      return nsnull;
    }
    nsresult rv = aElement->SetProperty(propName, et,
                                        ElementTransitionsPropertyDtor, nsnull);
    if (NS_FAILED(rv)) {
      NS_WARNING("SetProperty failed");
      delete et;
      return nsnull;
    }

    AddElementTransitions(et);
  }

  return et;
}

void
nsTransitionManager::AddElementTransitions(ElementTransitions* aElementTransitions)
{
  if (PR_CLIST_IS_EMPTY(&mElementTransitions)) {
    // We need to observe the refresh driver.
    nsRefreshDriver *rd = mPresContext->RefreshDriver();
    rd->AddRefreshObserver(this, Flush_Style);
  }

  PR_INSERT_BEFORE(aElementTransitions, &mElementTransitions);
}

/*
 * nsISupports implementation
 */

NS_IMPL_ISUPPORTS1(nsTransitionManager, nsIStyleRuleProcessor)

/*
 * nsIStyleRuleProcessor implementation
 */

nsresult
nsTransitionManager::WalkTransitionRule(RuleProcessorData* aData,
                                        nsCSSPseudoElements::Type aPseudoType)
{
  if (!aData->mContent) {
    return NS_OK;
  }

  ElementTransitions *et =
    GetElementTransitions(aData->mContent, aPseudoType, PR_FALSE);
  if (!et) {
    return NS_OK;
  }

  if (!aData->mPresContext->IsProcessingAnimationStyleChange()) {
    // If we're processing a normal style change rather than one from
    // animation, don't add the transition rule.  This allows us to
    // compute the new style value rather than having the transition
    // override it, so that we can start transitioning differently.

    // We need to immediately restyle with animation
    // after doing this.
    if (et) {
      mPresContext->PresShell()->RestyleForAnimation(aData->mContent);
    }
    return NS_OK;
  }

  if (!et->EnsureStyleRuleFor(
        aData->mPresContext->RefreshDriver()->MostRecentRefresh())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aData->mRuleWalker->Forward(et->mStyleRule);

  return NS_OK;
}

NS_IMETHODIMP
nsTransitionManager::RulesMatching(ElementRuleProcessorData* aData)
{
  NS_ABORT_IF_FALSE(aData->mPresContext == mPresContext,
                    "pres context mismatch");
  return WalkTransitionRule(aData,
			    nsCSSPseudoElements::ePseudo_NotPseudoElement);
}

NS_IMETHODIMP
nsTransitionManager::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  NS_ABORT_IF_FALSE(aData->mPresContext == mPresContext,
                    "pres context mismatch");

  // Note:  If we're the only thing keeping a pseudo-element frame alive
  // (per ProbePseudoStyleContext), we still want to keep it alive, so
  // this is ok.
  return WalkTransitionRule(aData, aData->mPseudoType);
}

NS_IMETHODIMP
nsTransitionManager::RulesMatching(AnonBoxRuleProcessorData* aData)
{
  return NS_OK;
}

#ifdef MOZ_XUL
NS_IMETHODIMP
nsTransitionManager::RulesMatching(XULTreeRuleProcessorData* aData)
{
  return NS_OK;
}
#endif

nsReStyleHint
nsTransitionManager::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  return nsReStyleHint(0);
}

nsReStyleHint
nsTransitionManager::HasAttributeDependentStyle(AttributeRuleProcessorData* aData)
{
  return nsReStyleHint(0);
}

NS_IMETHODIMP
nsTransitionManager::MediumFeaturesChanged(nsPresContext* aPresContext,
                                           PRBool* aRulesChanged)
{
  *aRulesChanged = PR_FALSE;
  return NS_OK;
}

struct TransitionEventInfo {
  nsCOMPtr<nsIContent> mElement;
  nsTransitionEvent mEvent;

  TransitionEventInfo(nsIContent *aElement, nsCSSProperty aProperty,
                      TimeDuration aDuration)
    : mElement(aElement),
      mEvent(PR_TRUE, NS_TRANSITION_END,
             NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aProperty)),
             aDuration.ToSeconds())
  {
  }

  // nsTransitionEvent doesn't support copy-construction, so we need
  // to ourselves in order to work with nsTArray
  TransitionEventInfo(const TransitionEventInfo &aOther)
    : mElement(aOther.mElement),
      mEvent(PR_TRUE, NS_TRANSITION_END,
             aOther.mEvent.propertyName, aOther.mEvent.elapsedTime)
  {
  }
};

/* virtual */ void
nsTransitionManager::WillRefresh(mozilla::TimeStamp aTime)
{
  NS_ABORT_IF_FALSE(mPresContext,
                    "refresh driver should not notify additional observers "
                    "after pres context has been destroyed");

  nsTArray<TransitionEventInfo> events;

  // Trim transitions that have completed, and post restyle events for
  // frames that are still transitioning.
  {
    PRCList *next = PR_LIST_HEAD(&mElementTransitions);
    while (next != &mElementTransitions) {
      ElementTransitions *et = static_cast<ElementTransitions*>(next);
      next = PR_NEXT_LINK(next);

      NS_ABORT_IF_FALSE(et->mElement->GetCurrentDoc() ==
                          mPresContext->Document(),
                        "nsGenericElement::UnbindFromTree should have "
                        "destroyed the element transitions object");

      PRUint32 i = et->mPropertyTransitions.Length();
      NS_ABORT_IF_FALSE(i != 0, "empty transitions list?");
      do {
        --i;
        ElementPropertyTransition &pt = et->mPropertyTransitions[i];
        if (pt.IsRemovedSentinel()) {
          // Actually remove transitions one cycle after their
          // completion.  See comment below.
          et->mPropertyTransitions.RemoveElementAt(i);
        } else if (pt.mStartTime + pt.mDuration <= aTime) {
          // This transition has completed.
          nsCSSProperty prop = pt.mProperty;
          if (nsCSSProps::PropHasFlags(prop, CSS_PROPERTY_REPORT_OTHER_NAME)) {
            prop = nsCSSProps::OtherNameFor(prop);
          }
          events.AppendElement(
            TransitionEventInfo(et->mElement, prop, pt.mDuration));

          // Leave this transition in the list for one more refresh
          // cycle, since we haven't yet processed its style change, and
          // if we also have (already, or will have from processing
          // transitionend events or other refresh driver notifications)
          // a non-animation style change that would affect it, we need
          // to know not to start a new transition for the transition
          // from the almost-completed value to the final value.
          pt.SetRemovedSentinel();
        }
      } while (i != 0);

      // We need to restyle even if the transition rule no longer
      // applies (in which case we just made it not apply).
      mPresContext->PresShell()->RestyleForAnimation(et->mElement);

      if (et->mPropertyTransitions.IsEmpty()) {
        et->Destroy();
        // |et| is now a dangling pointer!
        et = nsnull;
      }
    }
  }

  // We might have removed transitions above.
  TransitionsRemoved();

  for (PRUint32 i = 0, i_end = events.Length(); i < i_end; ++i) {
    TransitionEventInfo &info = events[i];
    nsEventDispatcher::Dispatch(info.mElement, mPresContext, &info.mEvent);

    if (!mPresContext) {
      break;
    }
  }
}

void
nsTransitionManager::TransitionsRemoved()
{
  // If we have no transitions left, remove ourselves from the refresh
  // driver.
  if (PR_CLIST_IS_EMPTY(&mElementTransitions)) {
    mPresContext->RefreshDriver()->RemoveRefreshObserver(this, Flush_Style);
  }
}
