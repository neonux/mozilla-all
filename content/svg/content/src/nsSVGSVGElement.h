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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
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

#ifndef __NS_SVGSVGELEMENT_H__
#define __NS_SVGSVGELEMENT_H__

#include "DOMSVGTests.h"
#include "mozilla/dom/FromParser.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsIDOMSVGLocatable.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGZoomAndPan.h"
#include "nsSVGEnum.h"
#include "nsSVGLength2.h"
#include "nsSVGStylableElement.h"
#include "nsSVGViewBox.h"
#include "SVGAnimatedPreserveAspectRatio.h"

class nsIDOMSVGMatrix;
class nsSMILTimeContainer;

typedef nsSVGStylableElement nsSVGSVGElementBase;

class nsSVGSVGElement;

class nsSVGTranslatePoint {
public:
  nsSVGTranslatePoint()
    : mX(0.0f)
    , mY(0.0f)
  {}

  nsSVGTranslatePoint(float aX, float aY)
    : mX(aX)
    , mY(aY)
  {}

  void SetX(float aX)
    { mX = aX; }
  void SetY(float aY)
    { mY = aY; }
  float GetX() const
    { return mX; }
  float GetY() const
    { return mY; }

  nsresult ToDOMVal(nsSVGSVGElement *aElement, nsIDOMSVGPoint **aResult);

  bool operator!=(const nsSVGTranslatePoint &rhs) const {
    return mX != rhs.mX || mY != rhs.mY;
  }

private:

  struct DOMVal : public nsIDOMSVGPoint {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMVal)

    DOMVal(nsSVGTranslatePoint* aVal, nsSVGSVGElement *aElement)
      : mVal(aVal), mElement(aElement) {}

    NS_IMETHOD GetX(float *aValue)
      { *aValue = mVal->GetX(); return NS_OK; }
    NS_IMETHOD GetY(float *aValue)
      { *aValue = mVal->GetY(); return NS_OK; }

    NS_IMETHOD SetX(float aValue);
    NS_IMETHOD SetY(float aValue);

    NS_IMETHOD MatrixTransform(nsIDOMSVGMatrix *matrix,
                               nsIDOMSVGPoint **_retval);

    nsSVGTranslatePoint *mVal; // kept alive because it belongs to mElement
    nsRefPtr<nsSVGSVGElement> mElement;
  };

  float mX;
  float mY;
};

class svgFloatSize {
public:
  svgFloatSize(float aWidth, float aHeight)
    : width(aWidth)
    , height(aHeight)
  {}
  bool operator!=(const svgFloatSize& rhs) {
    return width != rhs.width || height != rhs.height;
  }
  float width;
  float height;
};

class nsSVGSVGElement : public nsSVGSVGElementBase,
                        public nsIDOMSVGSVGElement,
                        public DOMSVGTests,
                        public nsIDOMSVGFitToViewBox,
                        public nsIDOMSVGLocatable,
                        public nsIDOMSVGZoomAndPan
{
  friend class nsSVGOuterSVGFrame;
  friend class nsSVGInnerSVGFrame;
  friend class nsSVGImageFrame;

  friend nsresult NS_NewSVGSVGElement(nsIContent **aResult,
                                      already_AddRefed<nsINodeInfo> aNodeInfo,
                                      mozilla::dom::FromParser aFromParser);
  nsSVGSVGElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                  mozilla::dom::FromParser aFromParser);
  
public:
  typedef mozilla::SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio;
  typedef mozilla::SVGPreserveAspectRatio SVGPreserveAspectRatio;

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsSVGSVGElement, nsSVGSVGElementBase)
  NS_DECL_NSIDOMSVGSVGELEMENT
  NS_DECL_NSIDOMSVGFITTOVIEWBOX
  NS_DECL_NSIDOMSVGLOCATABLE
  NS_DECL_NSIDOMSVGZOOMANDPAN
  
  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE(nsSVGSVGElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGSVGElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGSVGElementBase::)

  /**
   * For use by zoom controls to allow currentScale, currentTranslate.x and
   * currentTranslate.y to be set by a single operation that dispatches a
   * single SVGZoom event (instead of one SVGZoom and two SVGScroll events).
   */
  NS_IMETHOD SetCurrentScaleTranslate(float s, float x, float y);

  /**
   * For use by pan controls to allow currentTranslate.x and currentTranslate.y
   * to be set by a single operation that dispatches a single SVGScroll event
   * (instead of two).
   */
  NS_IMETHOD SetCurrentTranslate(float x, float y);

  /**
   * Retrieve the value of currentScale and currentTranslate.
   */
  const nsSVGTranslatePoint& GetCurrentTranslate() { return mCurrentTranslate; }
  float GetCurrentScale() { return mCurrentScale; }

  /**
   * Retrieve the value of currentScale, currentTranslate.x or
   * currentTranslate.y prior to the last change made to any one of them.
   */
  const nsSVGTranslatePoint& GetPreviousTranslate() { return mPreviousTranslate; }
  float GetPreviousScale() { return mPreviousScale; }

  nsSMILTimeContainer* GetTimedDocumentRoot();

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);

  // nsSVGElement specializations:
  virtual gfxMatrix PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                      TransformTypes aWhich = eAllTransforms) const;
  virtual bool HasValidDimensions() const;
 
  // nsSVGSVGElement methods:
  float GetLength(PRUint8 mCtxType);
  // Copy our width or height to the target
  void SyncWidthOrHeight(nsIAtom* aName, nsSVGElement *aTarget) const;

  // public helpers:

  /**
   * Returns true if this element has a base/anim value for its "viewBox"
   * attribute that defines a viewBox rectangle with finite values.
   *
   * Note that this does not check whether we need to synthesize a viewBox,
   * so you must call ShouldSynthesizeViewBox() if you need to check that too.
   *
   * Note also that this method does not pay attention to whether the width or
   * height values of the viewBox rect are positive!
   */
  bool HasViewBox() const {
    return mViewBox.IsExplicitlySet();
  }

  /**
   * Returns true if we should synthesize a viewBox for ourselves (that is, if
   * we're the root element in an image document, and we're not currently being
   * painted for an <svg:image> element).
   *
   * Only call this method if HasViewBox() returns false.
   */
  bool ShouldSynthesizeViewBox() const;

  bool HasViewBoxOrSyntheticViewBox() const {
    return HasViewBox() || ShouldSynthesizeViewBox();
  }

  gfxMatrix GetViewBoxTransform() const;

  bool HasChildrenOnlyTransform() const {
    return mHasChildrenOnlyTransform;
  }

  /**
   * This method notifies the style system that the overflow rects of our
   * immediate childrens' frames need to be updated. It is called by our own
   * frame when changes (e.g. to currentScale) cause our children-only
   * transform to change.
   *
   * The reason we have this method instead of overriding
   * GetAttributeChangeHint is because we need to act on non-attribute (e.g.
   * currentScale) changes in addition to attribute (e.g. viewBox) changes.
   */
  void ChildrenOnlyTransformChanged();

  // This services any pending notifications for the transform on on this root
  // <svg> node needing to be recalculated.  (Only applicable in
  // SVG-as-an-image documents.)
  virtual void FlushImageTransformInvalidation();

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  svgFloatSize GetViewportSize() const {
    return svgFloatSize(mViewportWidth, mViewportHeight);
  }

  void SetViewportSize(const svgFloatSize& aSize) {
    mViewportWidth  = aSize.width;
    mViewportHeight = aSize.height;
  }

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

private:
  // Methods for <image> elements to override my "PreserveAspectRatio" value.
  // These are private so that only our friends (nsSVGImageFrame in
  // particular) have access.
  void SetImageOverridePreserveAspectRatio(const SVGPreserveAspectRatio& aPAR);
  void ClearImageOverridePreserveAspectRatio();
  const SVGPreserveAspectRatio* GetImageOverridePreserveAspectRatio() const;

  // nsSVGElement overrides
  bool IsEventName(nsIAtom* aName);

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);

  // implementation helpers:

  bool IsRoot() const {
    NS_ASSERTION((IsInDoc() && !GetParent()) ==
                 (OwnerDoc() && (OwnerDoc()->GetRootElement() == this)),
                 "Can't determine if we're root");
    return IsInDoc() && !GetParent();
  }

  /**
   * Returns true if this is an SVG <svg> element that is the child of
   * another non-foreignObject SVG element.
   */
  bool IsInner() const {
    const nsIContent *parent = GetFlattenedTreeParent();
    return parent && parent->IsSVG() &&
           parent->Tag() != nsGkAtoms::foreignObject;
  }

  /* 
   * While binding to the tree we need to determine if we will be the outermost
   * <svg> element _before_ the children are bound (as they want to know what
   * timed document root to register with) and therefore _before_ our parent is
   * set (both actions are performed by nsGenericElement::BindToTree) so we
   * can't use GetOwnerSVGElement() as it relies on GetParent(). This code is
   * basically a simplified version of GetOwnerSVGElement that uses the parent
   * parameters passed in instead.
   */
  bool WillBeOutermostSVG(nsIContent* aParent,
                            nsIContent* aBindingParent) const;

  // invalidate viewbox -> viewport xform & inform frames
  void InvalidateTransformNotifyFrame();

  // Returns true if we have at least one of the following:
  // - a (valid or invalid) value for the preserveAspectRatio attribute
  // - a SMIL-animated value for the preserveAspectRatio attribute
  bool HasPreserveAspectRatio();

  virtual LengthAttributesInfo GetLengthInfo();

  enum { X, Y, WIDTH, HEIGHT };
  nsSVGLength2 mLengthAttributes[4];
  static LengthInfo sLengthInfo[4];

  virtual EnumAttributesInfo GetEnumInfo();

  enum { ZOOMANDPAN };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sZoomAndPanMap[];
  static EnumInfo sEnumInfo[1];

  virtual nsSVGViewBox *GetViewBox();
  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();

  nsSVGViewBox                   mViewBox;
  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;

  nsSVGSVGElement               *mCoordCtx;

  // The size of the rectangular SVG viewport into which we render. This is
  // not (necessarily) the same as the content area. See:
  //
  //   http://www.w3.org/TR/SVG11/coords.html#ViewportSpace
  //
  // XXXjwatt Currently only used for outer <svg>, but maybe we could use -1 to
  // flag this as an inner <svg> to save the overhead of GetCtx calls?
  // XXXjwatt our frame should probably reset these when it's destroyed.
  float mViewportWidth, mViewportHeight;

  // The time container for animations within this SVG document fragment. Set
  // for all outermost <svg> elements (not nested <svg> elements).
  nsAutoPtr<nsSMILTimeContainer> mTimedDocumentRoot;

  // zoom and pan
  // IMPORTANT: see the comment in RecordCurrentScaleTranslate before writing
  // code to change any of these!
  nsSVGTranslatePoint               mCurrentTranslate;
  float                             mCurrentScale;
  nsSVGTranslatePoint               mPreviousTranslate;
  float                             mPreviousScale;
  PRInt32                           mRedrawSuspendCount;

  // For outermost <svg> elements created from parsing, animation is started by
  // the onload event in accordance with the SVG spec, but for <svg> elements
  // created by script or promoted from inner <svg> to outermost <svg> we need
  // to manually kick off animation when they are bound to the tree.
  bool                              mStartAnimationOnBindToTree;
  bool                              mImageNeedsTransformInvalidation;
  bool                              mIsPaintingSVGImageElement;
  bool                              mHasChildrenOnlyTransform;
};

#endif