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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

/* representation of simple property values within CSS declarations */

#include "nsCSSValue.h"
#include "nsString.h"
#include "nsCSSProps.h"
#include "nsReadableUtils.h"
#include "imgIRequest.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "nsMathUtils.h"
#include "nsStyleUtil.h"
#include "CSSCalc.h"
#include "prenv.h"  // for paint forcing

namespace css = mozilla::css;

nsCSSValue::nsCSSValue(PRInt32 aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
               aUnit == eCSSUnit_EnumColor, "not an int value");
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
      aUnit == eCSSUnit_EnumColor) {
    mValue.mInt = aValue;
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(float aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(eCSSUnit_Percent <= aUnit, "not a float value");
  if (eCSSUnit_Percent <= aUnit) {
    mValue.mFloat = aValue;
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(const nsString& aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(UnitHasStringValue(), "not a string value");
  if (UnitHasStringValue()) {
    mValue.mString = BufferFromString(aValue);
    if (NS_UNLIKELY(!mValue.mString)) {
      // XXXbz not much we can do here; just make sure that our promise of a
      // non-null mValue.mString holds for string units.
      mUnit = eCSSUnit_Null;
    }
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(nsCSSValue::Array* aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(UnitHasArrayValue(), "bad unit");
  mValue.mArray = aValue;
  mValue.mArray->AddRef();
}

nsCSSValue::nsCSSValue(nsCSSValue::URL* aValue)
  : mUnit(eCSSUnit_URL)
{
  mValue.mURL = aValue;
  mValue.mURL->AddRef();
}

nsCSSValue::nsCSSValue(nsCSSValue::Image* aValue)
  : mUnit(eCSSUnit_Image)
{
  mValue.mImage = aValue;
  mValue.mImage->AddRef();
}

nsCSSValue::nsCSSValue(nsCSSValueGradient* aValue)
  : mUnit(eCSSUnit_Gradient)
{
  mValue.mGradient = aValue;
  mValue.mGradient->AddRef();
}

nsCSSValue::nsCSSValue(const nsCSSValue& aCopy)
  : mUnit(aCopy.mUnit)
{
  if (mUnit <= eCSSUnit_RectIsAuto) {
    // nothing to do, but put this important case first
  }
  else if (eCSSUnit_Percent <= mUnit) {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  else if (UnitHasStringValue()) {
    mValue.mString = aCopy.mValue.mString;
    mValue.mString->AddRef();
  }
  else if (eCSSUnit_Integer <= mUnit && mUnit <= eCSSUnit_EnumColor) {
    mValue.mInt = aCopy.mValue.mInt;
  }
  else if (eCSSUnit_Color == mUnit) {
    mValue.mColor = aCopy.mValue.mColor;
  }
  else if (UnitHasArrayValue()) {
    mValue.mArray = aCopy.mValue.mArray;
    mValue.mArray->AddRef();
  }
  else if (eCSSUnit_URL == mUnit) {
    mValue.mURL = aCopy.mValue.mURL;
    mValue.mURL->AddRef();
  }
  else if (eCSSUnit_Image == mUnit) {
    mValue.mImage = aCopy.mValue.mImage;
    mValue.mImage->AddRef();
  }
  else if (eCSSUnit_Gradient == mUnit) {
    mValue.mGradient = aCopy.mValue.mGradient;
    mValue.mGradient->AddRef();
  }
  else {
    NS_NOTREACHED("unknown unit");
  }
}

nsCSSValue& nsCSSValue::operator=(const nsCSSValue& aCopy)
{
  if (this != &aCopy) {
    Reset();
    new (this) nsCSSValue(aCopy);
  }
  return *this;
}

PRBool nsCSSValue::operator==(const nsCSSValue& aOther) const
{
  if (mUnit == aOther.mUnit) {
    if (mUnit <= eCSSUnit_RectIsAuto) {
      return PR_TRUE;
    }
    else if (UnitHasStringValue()) {
      return (NS_strcmp(GetBufferValue(mValue.mString),
                        GetBufferValue(aOther.mValue.mString)) == 0);
    }
    else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_EnumColor)) {
      return mValue.mInt == aOther.mValue.mInt;
    }
    else if (eCSSUnit_Color == mUnit) {
      return mValue.mColor == aOther.mValue.mColor;
    }
    else if (UnitHasArrayValue()) {
      return *mValue.mArray == *aOther.mValue.mArray;
    }
    else if (eCSSUnit_URL == mUnit) {
      return *mValue.mURL == *aOther.mValue.mURL;
    }
    else if (eCSSUnit_Image == mUnit) {
      return *mValue.mImage == *aOther.mValue.mImage;
    }
    else if (eCSSUnit_Gradient == mUnit) {
      return *mValue.mGradient == *aOther.mValue.mGradient;
    }
    else {
      return mValue.mFloat == aOther.mValue.mFloat;
    }
  }
  return PR_FALSE;
}

double nsCSSValue::GetAngleValueInRadians() const
{
  double angle = GetFloatValue();

  switch (GetUnit()) {
  case eCSSUnit_Radian: return angle;
  case eCSSUnit_Degree: return angle * M_PI / 180.0;
  case eCSSUnit_Grad:   return angle * M_PI / 200.0;

  default:
    NS_NOTREACHED("unrecognized angular unit");
    return 0.0;
  }
}

imgIRequest* nsCSSValue::GetImageValue() const
{
  NS_ASSERTION(mUnit == eCSSUnit_Image, "not an Image value");
  return mValue.mImage->mRequest;
}

nscoord nsCSSValue::GetFixedLength(nsPresContext* aPresContext) const
{
  NS_ASSERTION(IsFixedLengthUnit(), "not a fixed length unit");

  float twips;
  switch (mUnit) {
  case eCSSUnit_Inch:        
    twips = NS_INCHES_TO_TWIPS(mValue.mFloat);
    break;

  case eCSSUnit_Millimeter:
    twips = NS_MILLIMETERS_TO_TWIPS(mValue.mFloat);
    break;

  case eCSSUnit_Centimeter:
    twips = NS_CENTIMETERS_TO_TWIPS(mValue.mFloat);
    break;

  case eCSSUnit_Pica:
    twips = NS_PICAS_TO_TWIPS(mValue.mFloat);
    break;

  default:
    NS_ERROR("should never get here");
    return 0;
  }

  return aPresContext->TwipsToAppUnits(twips);
}

nscoord nsCSSValue::GetPixelLength() const
{
  NS_ASSERTION(IsPixelLengthUnit(), "not a fixed length unit");

  switch (mUnit) {
  case eCSSUnit_Pixel:
    return nsPresContext::CSSPixelsToAppUnits(mValue.mFloat);

  case eCSSUnit_Point:
    return nsPresContext::CSSPixelsToAppUnits(mValue.mFloat*4/3);

  default:
    NS_ERROR("should never get here");
    return 0;
  }
}

void nsCSSValue::DoReset()
{
  if (UnitHasStringValue()) {
    mValue.mString->Release();
  } else if (UnitHasArrayValue()) {
    mValue.mArray->Release();
  } else if (eCSSUnit_URL == mUnit) {
    mValue.mURL->Release();
  } else if (eCSSUnit_Image == mUnit) {
    mValue.mImage->Release();
  } else if (eCSSUnit_Gradient == mUnit) {
    mValue.mGradient->Release();
  }
  mUnit = eCSSUnit_Null;
}

void nsCSSValue::SetIntValue(PRInt32 aValue, nsCSSUnit aUnit)
{
  NS_ASSERTION(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
               aUnit == eCSSUnit_EnumColor, "not an int value");
  Reset();
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
      aUnit == eCSSUnit_EnumColor) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
}

void nsCSSValue::SetPercentValue(float aValue)
{
  Reset();
  mUnit = eCSSUnit_Percent;
  mValue.mFloat = aValue;
}

void nsCSSValue::SetFloatValue(float aValue, nsCSSUnit aUnit)
{
  NS_ASSERTION(eCSSUnit_Number <= aUnit, "not a float value");
  Reset();
  if (eCSSUnit_Number <= aUnit) {
    mUnit = aUnit;
    mValue.mFloat = aValue;
  }
}

void nsCSSValue::SetStringValue(const nsString& aValue,
                                nsCSSUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  NS_ASSERTION(UnitHasStringValue(), "not a string unit");
  if (UnitHasStringValue()) {
    mValue.mString = BufferFromString(aValue);
    if (NS_UNLIKELY(!mValue.mString)) {
      // XXXbz not much we can do here; just make sure that our promise of a
      // non-null mValue.mString holds for string units.
      mUnit = eCSSUnit_Null;
    }
  } else
    mUnit = eCSSUnit_Null;
}

void nsCSSValue::SetColorValue(nscolor aValue)
{
  Reset();
  mUnit = eCSSUnit_Color;
  mValue.mColor = aValue;
}

void nsCSSValue::SetArrayValue(nsCSSValue::Array* aValue, nsCSSUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  NS_ASSERTION(UnitHasArrayValue(), "bad unit");
  mValue.mArray = aValue;
  mValue.mArray->AddRef();
}

void nsCSSValue::SetURLValue(nsCSSValue::URL* aValue)
{
  Reset();
  mUnit = eCSSUnit_URL;
  mValue.mURL = aValue;
  mValue.mURL->AddRef();
}

void nsCSSValue::SetImageValue(nsCSSValue::Image* aValue)
{
  Reset();
  mUnit = eCSSUnit_Image;
  mValue.mImage = aValue;
  mValue.mImage->AddRef();
}

void nsCSSValue::SetGradientValue(nsCSSValueGradient* aValue)
{
  Reset();
  mUnit = eCSSUnit_Gradient;
  mValue.mGradient = aValue;
  mValue.mGradient->AddRef();
}

void nsCSSValue::SetAutoValue()
{
  Reset();
  mUnit = eCSSUnit_Auto;
}

void nsCSSValue::SetInheritValue()
{
  Reset();
  mUnit = eCSSUnit_Inherit;
}

void nsCSSValue::SetInitialValue()
{
  Reset();
  mUnit = eCSSUnit_Initial;
}

void nsCSSValue::SetNoneValue()
{
  Reset();
  mUnit = eCSSUnit_None;
}

void nsCSSValue::SetAllValue()
{
  Reset();
  mUnit = eCSSUnit_All;
}

void nsCSSValue::SetNormalValue()
{
  Reset();
  mUnit = eCSSUnit_Normal;
}

void nsCSSValue::SetSystemFontValue()
{
  Reset();
  mUnit = eCSSUnit_System_Font;
}

void nsCSSValue::SetDummyValue()
{
  Reset();
  mUnit = eCSSUnit_Dummy;
}

void nsCSSValue::SetDummyInheritValue()
{
  Reset();
  mUnit = eCSSUnit_DummyInherit;
}

void nsCSSValue::StartImageLoad(nsIDocument* aDocument) const
{
  NS_PRECONDITION(eCSSUnit_URL == mUnit, "Not a URL value!");
  nsCSSValue::Image* image =
    new nsCSSValue::Image(mValue.mURL->mURI,
                          mValue.mURL->mString,
                          mValue.mURL->mReferrer,
                          mValue.mURL->mOriginPrincipal,
                          aDocument);
  if (image) {
    nsCSSValue* writable = const_cast<nsCSSValue*>(this);
    writable->SetImageValue(image);
  }
}

PRBool nsCSSValue::IsNonTransparentColor() const
{
  // We have the value in the form it was specified in at this point, so we
  // have to look for both the keyword 'transparent' and its equivalent in
  // rgba notation.
  nsDependentString buf;
  return
    (mUnit == eCSSUnit_Color && NS_GET_A(GetColorValue()) > 0) ||
    (mUnit == eCSSUnit_Ident &&
     !nsGkAtoms::transparent->Equals(GetStringValue(buf))) ||
    (mUnit == eCSSUnit_EnumColor);
}

nsCSSValue::Array*
nsCSSValue::InitFunction(nsCSSKeyword aFunctionId, PRUint32 aNumArgs)
{
  nsRefPtr<nsCSSValue::Array> func = Array::Create(aNumArgs + 1);
  if (!func) {
    return nsnull;
  }

  func->Item(0).SetIntValue(aFunctionId, eCSSUnit_Enumerated);
  SetArrayValue(func, eCSSUnit_Function);

  return func;
}

PRBool
nsCSSValue::EqualsFunction(nsCSSKeyword aFunctionId) const
{
  if (mUnit != eCSSUnit_Function) {
    return PR_FALSE;
  }

  nsCSSValue::Array* func = mValue.mArray;
  NS_ABORT_IF_FALSE(func && func->Count() >= 1 &&
                    func->Item(0).GetUnit() == eCSSUnit_Enumerated,
                    "illegally structured function value");

  nsCSSKeyword thisFunctionId =
    static_cast<nsCSSKeyword>(func->Item(0).GetIntValue());
  return thisFunctionId == aFunctionId;
}

// static
nsStringBuffer*
nsCSSValue::BufferFromString(const nsString& aValue)
{
  nsStringBuffer* buffer = nsStringBuffer::FromString(aValue);
  if (buffer) {
    buffer->AddRef();
    return buffer;
  }
  
  PRUnichar length = aValue.Length();
  buffer = nsStringBuffer::Alloc((length + 1) * sizeof(PRUnichar));
  if (NS_LIKELY(buffer != 0)) {
    PRUnichar* data = static_cast<PRUnichar*>(buffer->Data());
    nsCharTraits<PRUnichar>::copy(data, aValue.get(), length);
    // Null-terminate.
    data[length] = 0;
  }

  return buffer;
}

namespace {

struct CSSValueSerializeCalcOps {
  CSSValueSerializeCalcOps(nsCSSProperty aProperty, nsAString& aResult)
    : mProperty(aProperty),
      mResult(aResult)
  {
  }

  typedef nsCSSValue input_type;
  typedef nsCSSValue::Array input_array_type;

  static nsCSSUnit GetUnit(const input_type& aValue) {
    return aValue.GetUnit();
  }

  void Append(const char* aString)
  {
    mResult.AppendASCII(aString);
  }

  void AppendLeafValue(const input_type& aValue)
  {
    NS_ABORT_IF_FALSE(aValue.GetUnit() == eCSSUnit_Percent ||
                      aValue.IsLengthUnit(), "unexpected unit");
    aValue.AppendToString(mProperty, mResult);
  }

  void AppendNumber(const input_type& aValue)
  {
    NS_ABORT_IF_FALSE(aValue.GetUnit() == eCSSUnit_Number, "unexpected unit");
    aValue.AppendToString(mProperty, mResult);
  }

private:
  nsCSSProperty mProperty;
  nsAString &mResult;
};

} // anonymous namespace

void
nsCSSValue::AppendToString(nsCSSProperty aProperty, nsAString& aResult) const
{
  // eCSSProperty_UNKNOWN gets used for some recursive calls below.
  NS_ABORT_IF_FALSE((0 <= aProperty &&
                     aProperty <= eCSSProperty_COUNT_no_shorthands) ||
                    aProperty == eCSSProperty_UNKNOWN,
                    "property ID out of range");

  nsCSSUnit unit = GetUnit();
  if (unit == eCSSUnit_Null) {
    return;
  }

  if (eCSSUnit_String <= unit && unit <= eCSSUnit_Attr) {
    if (unit == eCSSUnit_Attr) {
      aResult.AppendLiteral("attr(");
    }
    nsAutoString  buffer;
    GetStringValue(buffer);
    if (unit == eCSSUnit_String) {
      nsStyleUtil::AppendEscapedCSSString(buffer, aResult);
    } else if (unit == eCSSUnit_Families) {
      // XXX We really need to do *some* escaping.
      aResult.Append(buffer);
    } else {
      nsStyleUtil::AppendEscapedCSSIdent(buffer, aResult);
    }
  }
  else if (eCSSUnit_Array <= unit && unit <= eCSSUnit_Cubic_Bezier) {
    switch (unit) {
      case eCSSUnit_Counter:  aResult.AppendLiteral("counter(");  break;
      case eCSSUnit_Counters: aResult.AppendLiteral("counters("); break;
      case eCSSUnit_Cubic_Bezier: aResult.AppendLiteral("cubic-bezier("); break;
      default: break;
    }

    nsCSSValue::Array *array = GetArrayValue();
    PRBool mark = PR_FALSE;
    for (size_t i = 0, i_end = array->Count(); i < i_end; ++i) {
      if (aProperty == eCSSProperty_border_image && i >= 5) {
        if (array->Item(i).GetUnit() == eCSSUnit_Null) {
          continue;
        }
        if (i == 5) {
          aResult.AppendLiteral(" /");
        }
      }
      if (mark && array->Item(i).GetUnit() != eCSSUnit_Null) {
        if (unit == eCSSUnit_Array &&
            eCSSProperty_transition_timing_function != aProperty)
          aResult.AppendLiteral(" ");
        else
          aResult.AppendLiteral(", ");
      }
      nsCSSProperty prop =
        ((eCSSUnit_Counter <= unit && unit <= eCSSUnit_Counters) &&
         i == array->Count() - 1)
        ? eCSSProperty_list_style_type : aProperty;
      if (array->Item(i).GetUnit() != eCSSUnit_Null) {
        array->Item(i).AppendToString(prop, aResult);
        mark = PR_TRUE;
      }
    }
    if (eCSSUnit_Array == unit &&
        aProperty == eCSSProperty_transition_timing_function) {
      aResult.AppendLiteral(")");
    }
  }
  /* Although Function is backed by an Array, we'll handle it separately
   * because it's a bit quirky.
   */
  else if (eCSSUnit_Function == unit) {
    const nsCSSValue::Array* array = GetArrayValue();
    NS_ASSERTION(array->Count() >= 1, "Functions must have at least one element for the name.");

    /* Append the function name. */
    const nsCSSValue& functionName = array->Item(0);
    if (functionName.GetUnit() == eCSSUnit_Enumerated) {
      // We assume that the first argument is always of nsCSSKeyword type.
      const nsCSSKeyword functionId =
        static_cast<nsCSSKeyword>(functionName.GetIntValue());
      nsStyleUtil::AppendEscapedCSSIdent(
        NS_ConvertASCIItoUTF16(nsCSSKeywords::GetStringValue(functionId)),
        aResult);
    } else {
      functionName.AppendToString(aProperty, aResult);
    }
    aResult.AppendLiteral("(");

    /* Now, step through the function contents, writing each of them as we go. */
    for (size_t index = 1; index < array->Count(); ++index) {
      array->Item(index).AppendToString(aProperty, aResult);

      /* If we're not at the final element, append a comma. */
      if (index + 1 != array->Count())
        aResult.AppendLiteral(", ");
    }

    /* Finally, append the closing parenthesis. */
    aResult.AppendLiteral(")");
  }
  else if (IsCalcUnit()) {
    NS_ABORT_IF_FALSE(GetUnit() == eCSSUnit_Calc ||
                      GetUnit() == eCSSUnit_Calc_Maximum ||
                      GetUnit() == eCSSUnit_Calc_Minimum,
                      "unexpected unit");
    CSSValueSerializeCalcOps ops(aProperty, aResult);
    css::SerializeCalc(*this, ops);
  }
  else if (eCSSUnit_Integer == unit) {
    nsAutoString tmpStr;
    tmpStr.AppendInt(GetIntValue(), 10);
    aResult.Append(tmpStr);
  }
  else if (eCSSUnit_Enumerated == unit) {
    if (eCSSProperty_text_decoration == aProperty) {
      PRInt32 intValue = GetIntValue();
      if (NS_STYLE_TEXT_DECORATION_NONE == intValue) {
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, intValue),
                           aResult);
      } else {
        // Ignore the "override all" internal value.
        // (It doesn't have a string representation.)
        intValue &= ~NS_STYLE_TEXT_DECORATION_OVERRIDE_ALL;
        nsStyleUtil::AppendBitmaskCSSValue(
          aProperty, intValue,
          NS_STYLE_TEXT_DECORATION_UNDERLINE,
          NS_STYLE_TEXT_DECORATION_PREF_ANCHORS,
          aResult);
      }
    }
    else if (eCSSProperty_azimuth == aProperty) {
      PRInt32 intValue = GetIntValue();
      AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, (intValue & ~NS_STYLE_AZIMUTH_BEHIND)), aResult);
      if ((NS_STYLE_AZIMUTH_BEHIND & intValue) != 0) {
        aResult.Append(PRUnichar(' '));
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, NS_STYLE_AZIMUTH_BEHIND), aResult);
      }
    }
    else if (eCSSProperty_marks == aProperty) {
      PRInt32 intValue = GetIntValue();
      if (intValue == NS_STYLE_PAGE_MARKS_NONE) {
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, intValue),
                           aResult);
      } else {
        nsStyleUtil::AppendBitmaskCSSValue(aProperty, intValue,
                                           NS_STYLE_PAGE_MARKS_CROP,
                                           NS_STYLE_PAGE_MARKS_REGISTER,
                                           aResult);
      }
    }
    else {
      const nsAFlatCString& name = nsCSSProps::LookupPropertyValue(aProperty, GetIntValue());
      AppendASCIItoUTF16(name, aResult);
    }
  }
  else if (eCSSUnit_EnumColor == unit) {
    // we can lookup the property in the ColorTable and then
    // get a string mapping the name
    nsCAutoString str;
    if (nsCSSProps::GetColorName(GetIntValue(), str)){
      AppendASCIItoUTF16(str, aResult);
    } else {
      NS_NOTREACHED("bad color value");
    }
  }
  else if (eCSSUnit_Color == unit) {
    nscolor color = GetColorValue();
    if (color == NS_RGBA(0, 0, 0, 0)) {
      // Use the strictest match for 'transparent' so we do correct
      // round-tripping of all other rgba() values.
      aResult.AppendLiteral("transparent");
    } else {
      nsAutoString tmpStr;
      PRUint8 a = NS_GET_A(color);
      if (a < 255) {
        tmpStr.AppendLiteral("rgba(");
      } else {
        tmpStr.AppendLiteral("rgb(");
      }

      NS_NAMED_LITERAL_STRING(comma, ", ");

      tmpStr.AppendInt(NS_GET_R(color), 10);
      tmpStr.Append(comma);
      tmpStr.AppendInt(NS_GET_G(color), 10);
      tmpStr.Append(comma);
      tmpStr.AppendInt(NS_GET_B(color), 10);
      if (a < 255) {
        tmpStr.Append(comma);
        tmpStr.AppendFloat(nsStyleUtil::ColorComponentToFloat(a));
      }
      tmpStr.Append(PRUnichar(')'));

      aResult.Append(tmpStr);
    }
  }
  else if (eCSSUnit_URL == unit || eCSSUnit_Image == unit) {
    aResult.Append(NS_LITERAL_STRING("url("));
    nsStyleUtil::AppendEscapedCSSString(
      nsDependentString(GetOriginalURLValue()), aResult);
    aResult.Append(NS_LITERAL_STRING(")"));
  }
  else if (eCSSUnit_Percent == unit) {
    nsAutoString tmpStr;
    tmpStr.AppendFloat(GetPercentValue() * 100.0f);
    aResult.Append(tmpStr);
  }
  else if (eCSSUnit_Percent < unit) {  // length unit
    nsAutoString tmpStr;
    tmpStr.AppendFloat(GetFloatValue());
    aResult.Append(tmpStr);
  }
  else if (eCSSUnit_Gradient == unit) {
    nsCSSValueGradient* gradient = GetGradientValue();

    if (gradient->mIsRepeating) {
      if (gradient->mIsRadial)
        aResult.AppendLiteral("-moz-repeating-radial-gradient(");
      else
        aResult.AppendLiteral("-moz-repeating-linear-gradient(");
    } else {
      if (gradient->mIsRadial)
        aResult.AppendLiteral("-moz-radial-gradient(");
      else
        aResult.AppendLiteral("-moz-linear-gradient(");
    }

    if (gradient->mBgPosX.GetUnit() != eCSSUnit_None ||
        gradient->mBgPosY.GetUnit() != eCSSUnit_None ||
        gradient->mAngle.GetUnit() != eCSSUnit_None) {
      if (gradient->mBgPosX.GetUnit() != eCSSUnit_None) {
        gradient->mBgPosX.AppendToString(eCSSProperty_background_position,
                                         aResult);
        aResult.AppendLiteral(" ");
      }
      if (gradient->mBgPosY.GetUnit() != eCSSUnit_None) {
        gradient->mBgPosY.AppendToString(eCSSProperty_background_position,
                                         aResult);
        aResult.AppendLiteral(" ");
      }
      if (gradient->mAngle.GetUnit() != eCSSUnit_None) {
        gradient->mAngle.AppendToString(aProperty, aResult);
      }
      aResult.AppendLiteral(", ");
    }

    if (gradient->mIsRadial &&
        (gradient->mRadialShape.GetUnit() != eCSSUnit_None ||
         gradient->mRadialSize.GetUnit() != eCSSUnit_None)) {
      if (gradient->mRadialShape.GetUnit() != eCSSUnit_None) {
        NS_ASSERTION(gradient->mRadialShape.GetUnit() == eCSSUnit_Enumerated,
                     "bad unit for radial gradient shape");
        PRInt32 intValue = gradient->mRadialShape.GetIntValue();
        NS_ASSERTION(intValue != NS_STYLE_GRADIENT_SHAPE_LINEAR,
                     "radial gradient with linear shape?!");
        AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                               nsCSSProps::kRadialGradientShapeKTable),
                           aResult);
        aResult.AppendLiteral(" ");
      }

      if (gradient->mRadialSize.GetUnit() != eCSSUnit_None) {
        NS_ASSERTION(gradient->mRadialSize.GetUnit() == eCSSUnit_Enumerated,
                     "bad unit for radial gradient size");
        PRInt32 intValue = gradient->mRadialSize.GetIntValue();
        AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                               nsCSSProps::kRadialGradientSizeKTable),
                           aResult);
      }
      aResult.AppendLiteral(", ");
    }

    for (PRUint32 i = 0 ;;) {
      gradient->mStops[i].mColor.AppendToString(aProperty, aResult);
      if (gradient->mStops[i].mLocation.GetUnit() != eCSSUnit_None) {
        aResult.AppendLiteral(" ");
        gradient->mStops[i].mLocation.AppendToString(aProperty, aResult);
      }
      if (++i == gradient->mStops.Length()) {
        break;
      }
      aResult.AppendLiteral(", ");
    }

    aResult.AppendLiteral(")");
  }

  switch (unit) {
    case eCSSUnit_Null:         break;
    case eCSSUnit_Auto:         aResult.AppendLiteral("auto");     break;
    case eCSSUnit_Inherit:      aResult.AppendLiteral("inherit");  break;
    case eCSSUnit_Initial:      aResult.AppendLiteral("-moz-initial"); break;
    case eCSSUnit_None:         aResult.AppendLiteral("none");     break;
    case eCSSUnit_Normal:       aResult.AppendLiteral("normal");   break;
    case eCSSUnit_System_Font:  aResult.AppendLiteral("-moz-use-system-font"); break;
    case eCSSUnit_All:          aResult.AppendLiteral("all"); break;
    case eCSSUnit_Dummy:
    case eCSSUnit_DummyInherit:
    case eCSSUnit_RectIsAuto:
      NS_NOTREACHED("should never serialize");
      break;

    case eCSSUnit_String:       break;
    case eCSSUnit_Ident:        break;
    case eCSSUnit_Families:     break;
    case eCSSUnit_URL:          break;
    case eCSSUnit_Image:        break;
    case eCSSUnit_Array:        break;
    case eCSSUnit_Attr:
    case eCSSUnit_Cubic_Bezier:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aResult.Append(PRUnichar(')'));    break;
    case eCSSUnit_Local_Font:   break;
    case eCSSUnit_Font_Format:  break;
    case eCSSUnit_Function:     break;
    case eCSSUnit_Calc:         break;
    case eCSSUnit_Calc_Plus:    break;
    case eCSSUnit_Calc_Minus:   break;
    case eCSSUnit_Calc_Times_L: break;
    case eCSSUnit_Calc_Times_R: break;
    case eCSSUnit_Calc_Divided: break;
    case eCSSUnit_Calc_Minimum: break;
    case eCSSUnit_Calc_Maximum: break;
    case eCSSUnit_Integer:      break;
    case eCSSUnit_Enumerated:   break;
    case eCSSUnit_EnumColor:    break;
    case eCSSUnit_Color:        break;
    case eCSSUnit_Percent:      aResult.Append(PRUnichar('%'));    break;
    case eCSSUnit_Number:       break;
    case eCSSUnit_Gradient:     break;

    case eCSSUnit_Inch:         aResult.AppendLiteral("in");   break;
    case eCSSUnit_Millimeter:   aResult.AppendLiteral("mm");   break;
    case eCSSUnit_Centimeter:   aResult.AppendLiteral("cm");   break;
    case eCSSUnit_Point:        aResult.AppendLiteral("pt");   break;
    case eCSSUnit_Pica:         aResult.AppendLiteral("pc");   break;

    case eCSSUnit_EM:           aResult.AppendLiteral("em");   break;
    case eCSSUnit_XHeight:      aResult.AppendLiteral("ex");   break;
    case eCSSUnit_Char:         aResult.AppendLiteral("ch");   break;
    case eCSSUnit_RootEM:       aResult.AppendLiteral("rem");  break;

    case eCSSUnit_Pixel:        aResult.AppendLiteral("px");   break;

    case eCSSUnit_Degree:       aResult.AppendLiteral("deg");  break;
    case eCSSUnit_Grad:         aResult.AppendLiteral("grad"); break;
    case eCSSUnit_Radian:       aResult.AppendLiteral("rad");  break;

    case eCSSUnit_Hertz:        aResult.AppendLiteral("Hz");   break;
    case eCSSUnit_Kilohertz:    aResult.AppendLiteral("kHz");  break;

    case eCSSUnit_Seconds:      aResult.Append(PRUnichar('s'));    break;
    case eCSSUnit_Milliseconds: aResult.AppendLiteral("ms");   break;
  }
}

nsCSSValue::URL::URL(nsIURI* aURI, nsStringBuffer* aString, nsIURI* aReferrer,
                     nsIPrincipal* aOriginPrincipal)
  : mURI(aURI),
    mString(aString),
    mReferrer(aReferrer),
    mOriginPrincipal(aOriginPrincipal)
{
  NS_PRECONDITION(aOriginPrincipal, "Must have an origin principal");
  mString->AddRef();
}

nsCSSValue::URL::~URL()
{
  mString->Release();
}

PRBool
nsCSSValue::URL::operator==(const URL& aOther) const
{
  PRBool eq;
  return NS_strcmp(GetBufferValue(mString),
                   GetBufferValue(aOther.mString)) == 0 &&
          (mURI == aOther.mURI || // handles null == null
           (mURI && aOther.mURI &&
            NS_SUCCEEDED(mURI->Equals(aOther.mURI, &eq)) &&
            eq)) &&
          (mOriginPrincipal == aOther.mOriginPrincipal ||
           (NS_SUCCEEDED(mOriginPrincipal->Equals(aOther.mOriginPrincipal,
                                                  &eq)) && eq));
}

PRBool
nsCSSValue::URL::URIEquals(const URL& aOther) const
{
  PRBool eq;
  // Worth comparing mURI to aOther.mURI and mOriginPrincipal to
  // aOther.mOriginPrincipal, because in the (probably common) case when this
  // value was one of the ones that in fact did not change this will be our
  // fast path to equality
  return (mURI == aOther.mURI ||
          (NS_SUCCEEDED(mURI->Equals(aOther.mURI, &eq)) && eq)) &&
         (mOriginPrincipal == aOther.mOriginPrincipal ||
          (NS_SUCCEEDED(mOriginPrincipal->Equals(aOther.mOriginPrincipal,
                                                 &eq)) && eq));
}

nsCSSValue::Image::Image(nsIURI* aURI, nsStringBuffer* aString,
                         nsIURI* aReferrer, nsIPrincipal* aOriginPrincipal,
                         nsIDocument* aDocument)
  : URL(aURI, aString, aReferrer, aOriginPrincipal)
{
  if (mURI &&
      nsContentUtils::CanLoadImage(mURI, aDocument, aDocument,
                                   aOriginPrincipal)) {
    nsContentUtils::LoadImage(mURI, aDocument, aOriginPrincipal, aReferrer,
                              nsnull, nsIRequest::LOAD_NORMAL,
                              getter_AddRefs(mRequest));
  }
}

nsCSSValue::Image::~Image()
{
}

nsCSSValueGradientStop::nsCSSValueGradientStop()
  : mLocation(eCSSUnit_None),
    mColor(eCSSUnit_Null)
{
  MOZ_COUNT_CTOR(nsCSSValueGradientStop);
}

nsCSSValueGradientStop::nsCSSValueGradientStop(const nsCSSValueGradientStop& aOther)
  : mLocation(aOther.mLocation),
    mColor(aOther.mColor)
{
  MOZ_COUNT_CTOR(nsCSSValueGradientStop);
}

nsCSSValueGradientStop::~nsCSSValueGradientStop()
{
  MOZ_COUNT_DTOR(nsCSSValueGradientStop);
}

nsCSSValueGradient::nsCSSValueGradient(PRBool aIsRadial,
                                       PRBool aIsRepeating)
  : mIsRadial(aIsRadial),
    mIsRepeating(aIsRepeating),
    mBgPosX(eCSSUnit_None),
    mBgPosY(eCSSUnit_None),
    mAngle(eCSSUnit_None),
    mRadialShape(eCSSUnit_None),
    mRadialSize(eCSSUnit_None)
{
}
