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
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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

/* representation of a value for a SMIL-animated CSS property */

#include "nsSMILCSSValueType.h"
#include "nsString.h"
#include "nsStyleAnimation.h"
#include "nsStyleCoord.h"
#include "nsSMILParserUtils.h"
#include "nsSMILValue.h"
#include "nsCSSValue.h"
#include "nsCSSDeclaration.h"
#include "nsColor.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsDebug.h"

/*static*/ nsSMILCSSValueType nsSMILCSSValueType::sSingleton;

struct ValueWrapper {
  ValueWrapper() : mCSSValue(), mPropID(eCSSProperty_UNKNOWN),
                   mPresContext(nsnull) {}

  nsStyleCoord   mCSSValue;
  nsCSSProperty  mPropID;
  nsPresContext* mPresContext;
};

// Helper "zero" values of various types
// -------------------------------------
static const nsStyleCoord sZeroCoord(0);
static const nsStyleCoord sZeroPercent(0.0f, eStyleUnit_Percent);
static const nsStyleCoord sZeroFactor(0.0f,  eStyleUnit_Factor);
static const nsStyleCoord sZeroColor(NS_RGB(0,0,0));

// Helper Methods
// --------------
static const nsStyleCoord*
GetZeroValueForUnit(nsStyleUnit aUnit)
{
  NS_ABORT_IF_FALSE(aUnit != eStyleUnit_Null,
                    "Need non-null unit for a zero value.");
  switch (aUnit) {
    case eStyleUnit_Coord:
      return &sZeroCoord;
    case eStyleUnit_Percent:
      return &sZeroPercent;
    case eStyleUnit_Factor:
      return &sZeroFactor;
    case eStyleUnit_Color:
      return &sZeroColor;
    default:
      NS_NOTREACHED("Calling GetZeroValueForUnit with an unsupported unit");
      return nsnull;
  }
}

static void
InvertStyleCoordSign(nsStyleCoord& aStyleCoord)
{
  switch (aStyleCoord.GetUnit()) {
    case eStyleUnit_Coord:
      aStyleCoord.SetCoordValue(-aStyleCoord.GetCoordValue());
      break;
    case eStyleUnit_Percent:
      aStyleCoord.SetPercentValue(-aStyleCoord.GetPercentValue());
      break;
    case eStyleUnit_Factor:
      aStyleCoord.SetFactorValue(-aStyleCoord.GetFactorValue());
      break;
    default:
      NS_NOTREACHED("Calling InvertStyleCoordSign with an unsupported unit");
      break;
  }
}

static ValueWrapper*
ExtractValueWrapper(nsSMILValue& aValue)
{
  return static_cast<ValueWrapper*>(aValue.mU.mPtr);
}

static const ValueWrapper*
ExtractValueWrapper(const nsSMILValue& aValue)
{
  return static_cast<const ValueWrapper*>(aValue.mU.mPtr);
}

// Class methods
// -------------
nsresult
nsSMILCSSValueType::Init(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(),
                    "Unexpected value type");

  aValue.mU.mPtr = new ValueWrapper();
  if (!aValue.mU.mPtr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aValue.mType = this;
  return NS_OK;
}

void
nsSMILCSSValueType::Destroy(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.mType == this, "Unexpected SMIL value type");
  NS_ABORT_IF_FALSE(aValue.mU.mPtr,
                    "nsSMILValue for CSS val should have non-null pointer");

  delete static_cast<ValueWrapper*>(aValue.mU.mPtr);
  aValue.mU.mPtr    = nsnull;
  aValue.mType      = &nsSMILNullType::sSingleton;
}

nsresult
nsSMILCSSValueType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_ABORT_IF_FALSE(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_ABORT_IF_FALSE(aDest.mType == this, "Unexpected SMIL value type");
  NS_ABORT_IF_FALSE(aDest.mU.mPtr,
                    "nsSMILValue for CSS val should have non-null pointer");
  NS_ABORT_IF_FALSE(aSrc.mU.mPtr,
                    "nsSMILValue for CSS val should have non-null pointer");
  const ValueWrapper* srcWrapper = ExtractValueWrapper(aSrc);
  ValueWrapper* destWrapper = ExtractValueWrapper(aDest);
  *destWrapper = *srcWrapper; // Directly copy prop ID & CSS value

  return NS_OK;
}

nsresult
nsSMILCSSValueType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                        PRUint32 aCount) const
{
  NS_ABORT_IF_FALSE(aValueToAdd.mType == aDest.mType,
                    "Trying to add invalid types");
  NS_ABORT_IF_FALSE(aValueToAdd.mType == this, "Unexpected source type");

  ValueWrapper* destWrapper = ExtractValueWrapper(aDest);
  const ValueWrapper* valueToAddWrapper = ExtractValueWrapper(aValueToAdd);

  NS_ABORT_IF_FALSE(destWrapper && valueToAddWrapper,
                    "these pointers shouldn't be null");

  // At most one of our two inputs might be "unknown" (zero) values.
  // If so, replace with an actual zero value
  const nsStyleCoord* valueToAddCSSValue;
  if (valueToAddWrapper->mPropID == eCSSProperty_UNKNOWN) {
    NS_ABORT_IF_FALSE(destWrapper->mPropID != eCSSProperty_UNKNOWN,
                      "At least one of our inputs should have known value");
    NS_ABORT_IF_FALSE(valueToAddWrapper->mCSSValue.GetUnit() == eStyleUnit_Null,
                      "If property ID is unset, then the unit should be, too");
    valueToAddCSSValue = GetZeroValueForUnit(destWrapper->mCSSValue.GetUnit());
  } else {
    valueToAddCSSValue = &valueToAddWrapper->mCSSValue;
  }
  if (destWrapper->mPropID == eCSSProperty_UNKNOWN) {
    NS_ABORT_IF_FALSE(destWrapper->mCSSValue.IsNull(),
                      "If property ID is unset, then the unit should be, too");
    // We need to update destWrapper, since it's part of an outparam.
    destWrapper->mCSSValue =
      *GetZeroValueForUnit(valueToAddWrapper->mCSSValue.GetUnit());
    destWrapper->mPropID = valueToAddWrapper->mPropID;
    destWrapper->mPresContext = valueToAddWrapper->mPresContext;
  }

  // Special case: font-size-adjust is explicitly non-additive
  if (destWrapper->mPropID == eCSSProperty_font_size_adjust) {
    return NS_ERROR_FAILURE;
  }
  return nsStyleAnimation::Add(destWrapper->mCSSValue,
                               *valueToAddCSSValue, aCount) ?
    NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsSMILCSSValueType::ComputeDistance(const nsSMILValue& aFrom,
                                    const nsSMILValue& aTo,
                                    double& aDistance) const
{
  NS_ABORT_IF_FALSE(aFrom.mType == aTo.mType,
                    "Trying to compare different types");
  NS_ABORT_IF_FALSE(aFrom.mType == this, "Unexpected source type");

  const ValueWrapper* fromWrapper = ExtractValueWrapper(aFrom);
  const ValueWrapper* toWrapper = ExtractValueWrapper(aTo);
  NS_ABORT_IF_FALSE(fromWrapper && toWrapper,
                    "These pointers shouldn't be null");

  const nsStyleCoord* fromCSSValue;
  if (fromWrapper->mPropID == eCSSProperty_UNKNOWN) {
    NS_ABORT_IF_FALSE(fromWrapper->mCSSValue.IsNull(),
                      "If property ID is unset, then the unit should be, too");
    fromCSSValue = GetZeroValueForUnit(toWrapper->mCSSValue.GetUnit());
  } else {
    fromCSSValue = &fromWrapper->mCSSValue;
  }
  NS_ABORT_IF_FALSE(toWrapper->mPropID != eCSSProperty_UNKNOWN &&
                    !toWrapper->mCSSValue.IsNull(),
                    "ComputeDistance endpoint should be a parsed value");

  PRBool success = nsStyleAnimation::ComputeDistance(*fromCSSValue,
                                                     toWrapper->mCSSValue,
                                                     aDistance);
  return success ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsSMILCSSValueType::Interpolate(const nsSMILValue& aStartVal,
                                const nsSMILValue& aEndVal,
                                double aUnitDistance,
                                nsSMILValue& aResult) const
{
  NS_ABORT_IF_FALSE(aStartVal.mType == aEndVal.mType,
                    "Trying to interpolate different types");
  NS_ABORT_IF_FALSE(aStartVal.mType == this,
                    "Unexpected types for interpolation");
  NS_ABORT_IF_FALSE(aResult.mType == this, "Unexpected result type");
  NS_ABORT_IF_FALSE(aUnitDistance >= 0.0 && aUnitDistance <= 1.0,
                    "unit distance value out of bounds");

  const ValueWrapper* startWrapper = ExtractValueWrapper(aStartVal);
  const ValueWrapper* endWrapper = ExtractValueWrapper(aEndVal);
  ValueWrapper* resultWrapper = ExtractValueWrapper(aResult);

  NS_ABORT_IF_FALSE(startWrapper && endWrapper && resultWrapper,
                    "These pointers shouldn't be null");

  const nsStyleCoord* startCSSValue;
  if (startWrapper->mPropID == eCSSProperty_UNKNOWN) {
    NS_ABORT_IF_FALSE(startWrapper->mCSSValue.IsNull(),
                      "If property ID is unset, then the unit should be, too");
    startCSSValue = GetZeroValueForUnit(endWrapper->mCSSValue.GetUnit());
  } else {
    startCSSValue = &startWrapper->mCSSValue;
  }
  NS_ABORT_IF_FALSE(endWrapper->mPropID != eCSSProperty_UNKNOWN &&
                    !endWrapper->mCSSValue.IsNull(),
                    "Interpolate endpoint should be a parsed value");

  if (nsStyleAnimation::Interpolate(*startCSSValue,
                                    endWrapper->mCSSValue,
                                    aUnitDistance,
                                    resultWrapper->mCSSValue)) {
    resultWrapper->mPropID = endWrapper->mPropID;
    resultWrapper->mPresContext = endWrapper->mPresContext;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

PRBool
nsSMILCSSValueType::ValueFromString(nsCSSProperty aPropID,
                                    nsIContent* aTargetElement,
                                    const nsAString& aString,
                                    nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.mType == &nsSMILCSSValueType::sSingleton,
                    "Passed-in value is wrong type");
  NS_ABORT_IF_FALSE(aPropID < eCSSProperty_COUNT_no_shorthands,
                    "nsSMILCSSValueType shouldn't be used with "
                    "shorthand properties");

  // If value is negative, we'll strip off the "-" so the CSS parser won't
  // barf, and then manually make the parsed value negative. (This is a partial
  // solution to let us accept some otherwise out-of-bounds CSS values -- bug
  // 501188 will provide a more complete fix.)
  PRBool isNegative = PR_FALSE;
  PRUint32 subStringBegin = 0;
  PRInt32 absValuePos = nsSMILParserUtils::CheckForNegativeNumber(aString);
  if (absValuePos > 0) {
    subStringBegin = (PRUint32)absValuePos;
    isNegative = PR_TRUE;
  }
  nsDependentSubstring subString(aString, subStringBegin);
  ValueWrapper* wrapper = ExtractValueWrapper(aValue);
  NS_ABORT_IF_FALSE(wrapper, "Wrapper should be non-null if type is set.");

  if (nsStyleAnimation::ComputeValue(aPropID, aTargetElement,
                                     subString, wrapper->mCSSValue)) {
    wrapper->mPropID = aPropID;
    if (isNegative) {
      InvertStyleCoordSign(wrapper->mCSSValue);
    }
    // Cache a reference to the PresContext, if we've got one
    nsIDocument* doc = aTargetElement->GetCurrentDoc();
    NS_ABORT_IF_FALSE(doc, "active animations should only be able to "
                      "target elements that are in a document");
    nsIPresShell* shell = doc->GetPrimaryShell();
    if (shell) {
      wrapper->mPresContext = shell->GetPresContext();
    }
    if (wrapper->mPresContext) {
      // Divide out text-zoom, since SVG is supposed to ignore it
      if (aPropID == eCSSProperty_font_size) {
        NS_ABORT_IF_FALSE(wrapper->mCSSValue.GetUnit() == eStyleUnit_Coord,
                          "'font-size' value with unexpected style unit");
        wrapper->mCSSValue.SetCoordValue(wrapper->mCSSValue.GetCoordValue() /
                                         wrapper->mPresContext->TextZoom());
      }
      return PR_TRUE;
    }
    // Crap! We lack a PresContext or a PresShell
    NS_NOTREACHED("Not parsing animation value; unable to get PresContext");
    // Destroy & re-initialize aValue to make sure we leave it in a
    // consistent state
    Destroy(aValue);
    Init(aValue);
  }
  return PR_FALSE;
}

PRBool
nsSMILCSSValueType::ValueToString(const nsSMILValue& aValue,
                                  nsAString& aString) const
{
  NS_ABORT_IF_FALSE(aValue.mType == &nsSMILCSSValueType::sSingleton,
                    "Passed-in value is wrong type");

  const ValueWrapper* wrapper = ExtractValueWrapper(aValue);
  return nsStyleAnimation::UncomputeValue(wrapper->mPropID,
                                          wrapper->mPresContext,
                                          wrapper->mCSSValue, aString);
}
