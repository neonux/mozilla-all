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
 * The Original Code is nsStyleAnimation.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>, Mozilla Corporation
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
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

/* Utilities for animation of computed style values */

#ifndef nsStyleAnimation_h_
#define nsStyleAnimation_h_

#include "prtypes.h"
#include "nsAString.h"
#include "nsCSSProperty.h"
#include "nsCoord.h"
#include "nsColor.h"

class nsCSSDeclaration;
class nsIContent;
class nsPresContext;
class nsStyleContext;

/**
 * Utility class to handle animated style values
 */
class nsStyleAnimation {
public:
  class Value;

  // Mathematical methods
  // --------------------
  /**
   * Adds |aCount| copies of |aValueToAdd| to |aDest|.  The result of this
   * addition is stored in aDest.
   *
   * Note that if |aCount| is 0, then |aDest| will be unchanged.  Also, if
   * this method fails, then |aDest| will be unchanged.
   *
   * @param aDest       The value to add to.
   * @param aValueToAdd The value to add.
   * @param aCount      The number of times to add aValueToAdd.
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool Add(Value& aDest, const Value& aValueToAdd,
                    PRUint32 aCount);

  /**
   * Calculates a measure of 'distance' between two values.
   *
   * If this method succeeds, the returned distance value is guaranteed to be
   * non-negative.
   *
   * @param aStartValue The start of the interval for which the distance
   *                    should be calculated.
   * @param aEndValue   The end of the interval for which the distance
   *                    should be calculated.
   * @param aDistance   The result of the calculation.
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool ComputeDistance(const Value& aStartValue,
                                const Value& aEndValue,
                                double& aDistance);

  /**
   * Calculates an interpolated value that is the specified |aPortion| between
   * the two given values.
   *
   * This really just does the following calculation:
   *   aResultValue = (1.0 - aPortion) * aStartValue + aPortion * aEndValue
   *
   * @param aStartValue The value defining the start of the interval of
   *                    interpolation.
   * @param aEndValue   The value defining the end of the interval of
   *                    interpolation.
   * @param aPortion    A number in the range [0.0, 1.0] defining the
   *                    distance of the interpolated value in the interval.
   * @param [out] aResultValue The resulting interpolated value.
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool Interpolate(const Value& aStartValue,
                            const Value& aEndValue,
                            double aPortion,
                            Value& aResultValue);

  // Type-conversion methods
  // -----------------------
  /**
   * Creates a computed value for the given specified value
   * (property ID + string).  A style context is needed in case the
   * specified value depends on inherited style or on the values of other
   * properties.
   * 
   * NOTE: This method uses GetPrimaryShell() to access the style system,
   * so it should only be used for style that applies to all presentations,
   * rather than for style that only applies to a particular presentation.
   * XXX Once we get rid of multiple presentations, we can remove the above
   * note.
   *
   * @param aProperty       The property whose value we're computing.
   * @param aTargetElement  The content node to which our computed value is
   *                        applicable.
   * @param aSpecifiedValue The specified value, from which we'll build our
   *                        computed value.
   * @param [out] aComputedValue The resulting computed value.
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool ComputeValue(nsCSSProperty aProperty,
                             nsIContent* aElement,
                             const nsAString& aSpecifiedValue,
                             Value& aComputedValue);

  /**
   * Creates a specified value for the given computed value.
   *
   * The first form fills in one of the nsCSSType types into the void*;
   * for some types this means that the void* is pointing to memory
   * owned by the nsStyleAnimation::Value.  (For all complex types, the
   * nsStyleAnimation::Value owns the necessary objects so that the
   * caller does not need to do anything to free them.)
   *
   * @param aProperty      The property whose value we're uncomputing.
   * @param aPresContext   The presentation context for the document in
   *                       which we're working.
   * @param aComputedValue The computed value to be converted.
   * @param [out] aSpecifiedValue The resulting specified value.
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool UncomputeValue(nsCSSProperty aProperty,
                               nsPresContext* aPresContext,
                               const Value& aComputedValue,
                               void* aSpecifiedValue);
  static PRBool UncomputeValue(nsCSSProperty aProperty,
                               nsPresContext* aPresContext,
                               const Value& aComputedValue,
                               nsAString& aSpecifiedValue);

  /**
   * Gets the computed value for the given property from the given style
   * context.
   *
   * @param aProperty     The property whose value we're looking up.
   * @param aStyleContext The style context to check for the computed value.
   * @param [out] aComputedValue The resulting computed value.
   * @return PR_TRUE on success, PR_FALSE on failure.
   */
  static PRBool ExtractComputedValue(nsCSSProperty aProperty,
                                     nsStyleContext* aStyleContext,
                                     Value& aComputedValue);

  /**
   * The types and values for the values that we extract and animate.
   */
  enum Unit {
    eUnit_Null, // not initialized
    eUnit_Normal,
    eUnit_Auto,
    eUnit_None,
    eUnit_Coord,
    eUnit_Percent,
    eUnit_Float,
    eUnit_Color
  };

  class Value {
  private:
    Unit mUnit;
    union {
      nscoord mCoord;
      float mFloat;
      nscolor mColor;
    } mValue;
  public:
    Unit GetUnit() const {
      NS_ASSERTION(mUnit != eUnit_Null, "uninitialized");
      return mUnit;
    }

    // Accessor to let us verify assumptions about presence of null unit,
    // without tripping the assertion in GetUnit().
    PRBool IsNull() const {
      return mUnit == eUnit_Null;
    }

    nscoord GetCoordValue() const {
      NS_ASSERTION(mUnit == eUnit_Coord, "unit mismatch");
      return mValue.mCoord;
    }
    float GetPercentValue() const {
      NS_ASSERTION(mUnit == eUnit_Percent, "unit mismatch");
      return mValue.mFloat;
    }
    float GetFloatValue() const {
      NS_ASSERTION(mUnit == eUnit_Float, "unit mismatch");
      return mValue.mFloat;
    }
    nscolor GetColorValue() const {
      NS_ASSERTION(mUnit == eUnit_Color, "unit mismatch");
      return mValue.mColor;
    }

    explicit Value(Unit aUnit = eUnit_Null) : mUnit(aUnit) {
      NS_ASSERTION(aUnit == eUnit_Null || aUnit == eUnit_Normal ||
                   aUnit == eUnit_Auto || aUnit == eUnit_None,
                   "must be valueless unit");
    }
    Value(const Value& aOther) : mUnit(eUnit_Null) { *this = aOther; }
    enum CoordConstructorType { CoordConstructor };
    Value(nscoord aLength, CoordConstructorType);
    enum PercentConstructorType { PercentConstructor };
    Value(float aPercent, PercentConstructorType);
    enum FloatConstructorType { FloatConstructor };
    Value(float aFloat, FloatConstructorType);
    enum ColorConstructorType { ColorConstructor };
    Value(nscolor aColor, ColorConstructorType);

    ~Value() { FreeValue(); }

    void SetNormalValue();
    void SetAutoValue();
    void SetNoneValue();
    void SetCoordValue(nscoord aCoord);
    void SetPercentValue(float aPercent);
    void SetFloatValue(float aFloat);
    void SetColorValue(nscolor aColor);

    Value& operator=(const Value& aOther);

    PRBool operator==(const Value& aOther) const;
    PRBool operator!=(const Value& aOther) const
      { return !(*this == aOther); }

  private:
    void FreeValue();
  };
};

#endif
