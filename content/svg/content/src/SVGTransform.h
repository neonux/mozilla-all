/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#ifndef MOZILLA_SVGTRANSFORM_H__
#define MOZILLA_SVGTRANSFORM_H__

#include "gfxMatrix.h"
#include "nsDebug.h"
#include "nsIDOMSVGTransform.h"

namespace mozilla {

/*
 * The DOM wrapper class for this class is DOMSVGTransformMatrix.
 */
class SVGTransform
{
public:
  // Default ctor initialises to matrix type with identity matrix
  SVGTransform()
    : mMatrix() // Initialises to identity
    , mAngle(0.f)
    , mOriginX(0.f)
    , mOriginY(0.f)
    , mType(nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX)
  { }

  SVGTransform(const gfxMatrix& aMatrix)
    : mMatrix(aMatrix)
    , mAngle(0.f)
    , mOriginX(0.f)
    , mOriginY(0.f)
    , mType(nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX)
  { }

  bool operator==(const SVGTransform& rhs) const {
    return mType == rhs.mType &&
      MatricesEqual(mMatrix, rhs.mMatrix) &&
      mAngle == rhs.mAngle &&
      mOriginX == rhs.mOriginX &&
      mOriginY == rhs.mOriginY;
  }

  void GetValueAsString(nsAString& aValue) const;

  float Angle() const {
    return mAngle;
  }
  void GetRotationOrigin(float& aOriginX, float& aOriginY) const {
    aOriginX = mOriginX;
    aOriginY = mOriginY;
  }
  PRUint16 Type() const {
    return mType;
  }

  const gfxMatrix& Matrix() const { return mMatrix; }
  void SetMatrix(const gfxMatrix& aMatrix);
  void SetTranslate(float aTx, float aTy);
  void SetScale(float aSx, float aSy);
  void SetRotate(float aAngle, float aCx, float aCy);
  nsresult SetSkewX(float aAngle);
  nsresult SetSkewY(float aAngle);

  static bool MatricesEqual(const gfxMatrix& a, const gfxMatrix& b)
  {
    return a.xx == b.xx &&
           a.yx == b.yx &&
           a.xy == b.xy &&
           a.yy == b.yy &&
           a.x0 == b.x0 &&
           a.y0 == b.y0;
  }

protected:
  gfxMatrix mMatrix;
  float mAngle, mOriginX, mOriginY;
  PRUint16 mType;
};

/*
 * A slightly more light-weight version of SVGTransform for SMIL animation.
 *
 * Storing the parameters in an array (rather than a matrix) also allows simpler
 * (transform type-agnostic) interpolation and addition.
 *
 * The meaning of the mParams array depends on the transform type as follows:
 *
 * Type                | mParams[0], mParams[1], mParams[2], ...
 * --------------------+-----------------------------------------
 * translate           | tx, ty
 * scale               | sx, sy
 * rotate              | rotation-angle (in degrees), cx, cy
 * skewX               | skew-angle (in degrees)
 * skewY               | skew-angle (in degrees)
 * matrix              | a, b, c, d, e, f
 *
 * The matrix type is never generated by animation code (it is only produced
 * when the user inserts one via the DOM) and often requires special handling
 * when we do encounter it. Therefore many users of this class are only
 * interested in the first three parameters and so we provide a special
 * constructor for setting those parameters only.
 */
class SVGTransformSMILData
{
public:
  // Number of float-params required in constructor, if constructing one of the
  // 'simple' transform types (all but matrix type)
  static const PRUint32 NUM_SIMPLE_PARAMS = 3;

  // Number of float-params required in constructor for matrix type.
  // This is also the number of params we actually store, regardless of type.
  static const PRUint32 NUM_STORED_PARAMS = 6;

  explicit SVGTransformSMILData(PRUint16 aType)
  : mTransformType(aType)
  {
    NS_ABORT_IF_FALSE(aType >= nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX &&
                      aType <= nsIDOMSVGTransform::SVG_TRANSFORM_SKEWY,
                      "Unexpected transform type");
    for (PRUint32 i = 0; i < NUM_STORED_PARAMS; ++i) {
      mParams[i] = 0.f;
    }
  }

  SVGTransformSMILData(PRUint16 aType, float (&aParams)[NUM_SIMPLE_PARAMS])
  : mTransformType(aType)
  {
    NS_ABORT_IF_FALSE(aType >= nsIDOMSVGTransform::SVG_TRANSFORM_TRANSLATE &&
                      aType <= nsIDOMSVGTransform::SVG_TRANSFORM_SKEWY,
                      "Expected 'simple' transform type");
    for (PRUint32 i = 0; i < NUM_SIMPLE_PARAMS; ++i) {
      mParams[i] = aParams[i];
    }
    for (PRUint32 i = NUM_SIMPLE_PARAMS; i < NUM_STORED_PARAMS; ++i) {
      mParams[i] = 0.f;
    }
  }

  // Conversion to/from a fully-fledged SVGTransform
  SVGTransformSMILData(const SVGTransform& aTransform);
  SVGTransform ToSVGTransform() const;

  bool operator==(const SVGTransformSMILData& aOther) const
  {
    if (mTransformType != aOther.mTransformType)
      return false;

    for (PRUint32 i = 0; i < NUM_STORED_PARAMS; ++i) {
      if (mParams[i] != aOther.mParams[i]) {
        return false;
      }
    }

    return true;
  }

  bool operator!=(const SVGTransformSMILData& aOther) const
  {
    return !(*this == aOther);
  }

  PRUint16 mTransformType;
  float    mParams[NUM_STORED_PARAMS];
};

} // namespace mozilla

#endif // MOZILLA_SVGTRANSFORM_H__
