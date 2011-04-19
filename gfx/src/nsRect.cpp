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

#include "nsRect.h"
#include "nsString.h"
#include "nsIDeviceContext.h"
#include "prlog.h"
#include <limits.h>

// the mozilla::css::Side sequence must match the nsMargin nscoord sequence
PR_STATIC_ASSERT((NS_SIDE_TOP == 0) && (NS_SIDE_RIGHT == 1) && (NS_SIDE_BOTTOM == 2) && (NS_SIDE_LEFT == 3));

/* static */
const nsIntRect nsIntRect::kMaxSizedIntRect(0, 0, INT_MAX, INT_MAX);

#ifdef DEBUG
static bool IsFloatInteger(float aFloat)
{
  return fabs(aFloat - NS_round(aFloat)) < 1e-6;
}
#endif

nsRect& nsRect::ExtendForScaling(float aXMult, float aYMult)
{
  NS_ASSERTION((IsFloatInteger(aXMult) || IsFloatInteger(1/aXMult)) &&
               (IsFloatInteger(aYMult) || IsFloatInteger(1/aYMult)),
               "Multiplication factors must be integers or 1/integer");

  // Scale rect by multiplier, snap outwards to integers and then unscale.
  // We round the results to the nearest integer to prevent floating point errors.
  if (aXMult < 1) {
    nscoord right = NSToCoordRound(ceil(float(XMost()) * aXMult) / aXMult);
    x = NSToCoordRound(floor(float(x) * aXMult) / aXMult);
    width = right - x;
  }
  if (aYMult < 1) {
    nscoord bottom = NSToCoordRound(ceil(float(YMost()) * aYMult) / aYMult);
    y = NSToCoordRound(floor(float(y) * aYMult) / aYMult);
    height = bottom - y;
  }
  return *this;
}

#ifdef DEBUG
// Diagnostics

FILE* operator<<(FILE* out, const nsRect& rect)
{
  nsAutoString tmp;

  // Output the coordinates in fractional pixels so they're easier to read
  tmp.AppendLiteral("{");
  tmp.AppendFloat(NSAppUnitsToFloatPixels(rect.x,
                       nsIDeviceContext::AppUnitsPerCSSPixel()));
  tmp.AppendLiteral(", ");
  tmp.AppendFloat(NSAppUnitsToFloatPixels(rect.y,
                       nsIDeviceContext::AppUnitsPerCSSPixel()));
  tmp.AppendLiteral(", ");
  tmp.AppendFloat(NSAppUnitsToFloatPixels(rect.width,
                       nsIDeviceContext::AppUnitsPerCSSPixel()));
  tmp.AppendLiteral(", ");
  tmp.AppendFloat(NSAppUnitsToFloatPixels(rect.height,
                       nsIDeviceContext::AppUnitsPerCSSPixel()));
  tmp.AppendLiteral("}");
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
  return out;
}

#endif // DEBUG
