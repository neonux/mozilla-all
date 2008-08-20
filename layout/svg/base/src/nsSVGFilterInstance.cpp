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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#include "nsSVGFilterInstance.h"
#include "nsSVGUtils.h"
#include "nsIDOMSVGUnitTypes.h"
#include "nsSVGMatrix.h"

static double Square(double aX)
{
  return aX*aX;
}

float
nsSVGFilterInstance::GetPrimitiveLength(nsSVGLength2 *aLength) const
{
  float value;
  if (mPrimitiveUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
    value = nsSVGUtils::ObjectSpace(mTargetBBox, aLength);
  else
    value = nsSVGUtils::UserSpace(TargetElement(), aLength);

  switch (aLength->GetCtxType()) {
  case nsSVGUtils::X:
    return value * mFilterSpaceSize.width / mFilterRect.Width();
  case nsSVGUtils::Y:
    return value * mFilterSpaceSize.height / mFilterRect.Height();
  case nsSVGUtils::XY:
  default:
    return value *
      sqrt(Square(mFilterSpaceSize.width) + Square(mFilterSpaceSize.height)) /
      sqrt(Square(mFilterRect.Width()) + Square(mFilterRect.Height()));
  }
}

already_AddRefed<gfxImageSurface>
nsSVGFilterInstance::CreateImage()
{
  nsRefPtr<gfxImageSurface> surface =
    new gfxImageSurface(gfxIntSize(mSurfaceRect.width, mSurfaceRect.height),
                        gfxASurface::ImageFormatARGB32);

  if (!surface || surface->CairoStatus())
    return nsnull;

  surface->SetDeviceOffset(gfxPoint(-mSurfaceRect.x, -mSurfaceRect.y));

  gfxImageSurface *retval = nsnull;
  surface.swap(retval);
  return retval;
}

gfxRect
nsSVGFilterInstance::UserSpaceToFilterSpace(const gfxRect& aRect) const
{
  gfxRect r = aRect - mFilterRect.TopLeft();
  r.Scale(mFilterSpaceSize.width / mFilterRect.Width(),
          mFilterSpaceSize.height / mFilterRect.Height());
  return r;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGFilterInstance::GetUserSpaceToFilterSpaceTransform() const
{
  nsCOMPtr<nsIDOMSVGMatrix> filterTransform;
  gfxFloat widthScale = mFilterSpaceSize.width / mFilterRect.Width();
  gfxFloat heightScale = mFilterSpaceSize.height / mFilterRect.Height();
  NS_NewSVGMatrix(getter_AddRefs(filterTransform),
                  widthScale, 0.0f,
                  0.0f, heightScale,
                  -mFilterRect.X() * widthScale,
                  -mFilterRect.Y() * heightScale);
  return filterTransform.forget();
}

void
nsSVGFilterInstance::ComputeFilterPrimitiveSubregion(PrimitiveInfo* aPrimitive)
{
  nsSVGFE* fE = aPrimitive->mFE;

  gfxRect defaultFilterSubregion(0,0,0,0);
  if (fE->SubregionIsUnionOfRegions()) {
    for (PRUint32 i = 0; i < aPrimitive->mInputs.Length(); ++i) {
      defaultFilterSubregion = 
          defaultFilterSubregion.Union(
              aPrimitive->mInputs[i]->mImage.mFilterPrimitiveSubregion);
    }
  } else {
    defaultFilterSubregion =
      gfxRect(0, 0, mFilterSpaceSize.width, mFilterSpaceSize.height);
  }

  nsSVGLength2 *tmpX, *tmpY, *tmpWidth, *tmpHeight;
  tmpX = &fE->mLengthAttributes[nsSVGFE::X];
  tmpY = &fE->mLengthAttributes[nsSVGFE::Y];
  tmpWidth = &fE->mLengthAttributes[nsSVGFE::WIDTH];
  tmpHeight = &fE->mLengthAttributes[nsSVGFE::HEIGHT];

  float x, y, width, height;
  if (mPrimitiveUnits == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    mTargetBBox->GetX(&x);
    x     += nsSVGUtils::ObjectSpace(mTargetBBox, tmpX);
    mTargetBBox->GetY(&y);
    y     += nsSVGUtils::ObjectSpace(mTargetBBox, tmpY);
    width  = nsSVGUtils::ObjectSpace(mTargetBBox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(mTargetBBox, tmpHeight);
  } else {
    nsSVGElement* targetElement = TargetElement();
    x      = nsSVGUtils::UserSpace(targetElement, tmpX);
    y      = nsSVGUtils::UserSpace(targetElement, tmpY);
    width  = nsSVGUtils::UserSpace(targetElement, tmpWidth);
    height = nsSVGUtils::UserSpace(targetElement, tmpHeight);
  }

  gfxRect region = UserSpaceToFilterSpace(gfxRect(x, y, width, height));

  if (!fE->HasAttr(kNameSpaceID_None, nsGkAtoms::x))
    region.pos.x = defaultFilterSubregion.X();
  if (!fE->HasAttr(kNameSpaceID_None, nsGkAtoms::y))
    region.pos.y = defaultFilterSubregion.Y();
  if (!fE->HasAttr(kNameSpaceID_None, nsGkAtoms::width))
    region.size.width = defaultFilterSubregion.Width();
  if (!fE->HasAttr(kNameSpaceID_None, nsGkAtoms::height))
    region.size.height = defaultFilterSubregion.Height();

  // We currently require filter primitive subregions to be pixel-aligned.
  // Following the spec, any pixel partially in the region is included
  // in the region.
  region.RoundOut();
  aPrimitive->mImage.mFilterPrimitiveSubregion = region;
}

nsresult
nsSVGFilterInstance::BuildSources()
{
  gfxRect filterRegion = gfxRect(0, 0, mFilterSpaceSize.width, mFilterSpaceSize.height);
  mSourceColorAlpha.mImage.mFilterPrimitiveSubregion = filterRegion;
  mSourceAlpha.mImage.mFilterPrimitiveSubregion = filterRegion;

  nsIntRect sourceBoundsInt;
  if (mTargetBBox) {
    float x, y, w, h;
    mTargetBBox->GetX(&x);
    mTargetBBox->GetY(&y);
    mTargetBBox->GetWidth(&w);
    mTargetBBox->GetHeight(&h);

    gfxRect sourceBounds = UserSpaceToFilterSpace(gfxRect(x, y, w, h));

    sourceBounds.RoundOut();
    // Detect possible float->int overflow
    if (NS_FAILED(nsSVGUtils::GfxRectToIntRect(sourceBounds, &sourceBoundsInt)))
      return NS_ERROR_FAILURE;
  }

  mSourceColorAlpha.mResultBoundingBox = sourceBoundsInt;
  mSourceAlpha.mResultBoundingBox = sourceBoundsInt;
  return NS_OK;
}

nsresult
nsSVGFilterInstance::BuildPrimitives()
{
  // First build mFilterInfo. It's important that we don't change that
  // array after we start storing pointers to its elements!
  PRUint32 count = mFilterElement->GetChildCount();
  PRUint32 i;
  for (i = 0; i < count; ++i) {
    nsIContent* child = mFilterElement->GetChildAt(i);
    nsRefPtr<nsSVGFE> primitive;
    CallQueryInterface(child, (nsSVGFE**)getter_AddRefs(primitive));
    if (!primitive)
      continue;

    PrimitiveInfo* info = mPrimitives.AppendElement();
    info->mFE = primitive;
  }

  // Now fill in all the links
  nsTHashtable<ImageAnalysisEntry> imageTable;
  imageTable.Init(10);

  for (i = 0; i < mPrimitives.Length(); ++i) {
    PrimitiveInfo* info = &mPrimitives[i];
    nsSVGFE* filter = info->mFE;
    nsAutoTArray<nsSVGString*,2> sources;
    filter->GetSourceImageNames(&sources);
 
    for (PRUint32 j=0; j<sources.Length(); ++j) {
      const nsString& str = sources[j]->GetAnimValue();
      PrimitiveInfo* sourceInfo;

      if (str.EqualsLiteral("SourceGraphic")) {
        sourceInfo = &mSourceColorAlpha;
      } else if (str.EqualsLiteral("SourceAlpha")) {
        sourceInfo = &mSourceAlpha;
      } else if (str.EqualsLiteral("BackgroundImage") ||
                 str.EqualsLiteral("BackgroundAlpha") ||
                 str.EqualsLiteral("FillPaint") ||
                 str.EqualsLiteral("StrokePaint")) {
        return NS_ERROR_NOT_IMPLEMENTED;
      } else if (str.EqualsLiteral("")) {
        sourceInfo = i == 0 ? &mSourceColorAlpha : &mPrimitives[i - 1];
      } else {
        ImageAnalysisEntry* entry = imageTable.GetEntry(str);
        if (!entry)
          return NS_ERROR_FAILURE;
        sourceInfo = entry->mInfo;
      }
      
      ++sourceInfo->mImageUsers;
      info->mInputs.AppendElement(sourceInfo);
    }

    ComputeFilterPrimitiveSubregion(info);

    ImageAnalysisEntry* entry =
      imageTable.PutEntry(filter->GetResultImageName()->GetAnimValue());
    if (entry) {
      entry->mInfo = info;
    }
    
    // The last filter primitive is the filter result, so mark it used
    if (i == mPrimitives.Length() - 1) {
      ++info->mImageUsers;
    }
  }

  return NS_OK;
}

void
nsSVGFilterInstance::ComputeResultBoundingBoxes()
{
  for (PRUint32 i = 0; i < mPrimitives.Length(); ++i) {
    PrimitiveInfo* info = &mPrimitives[i];
    nsAutoTArray<nsIntRect,2> sourceBBoxes;
    for (PRUint32 j = 0; j < info->mInputs.Length(); ++j) {
      sourceBBoxes.AppendElement(info->mInputs[j]->mResultBoundingBox);
    }
    
    nsIntRect resultBBox = info->mFE->ComputeTargetBBox(sourceBBoxes, *this);
    ClipToFilterSpace(&resultBBox);
    nsSVGUtils::ClipToGfxRect(&resultBBox, info->mImage.mFilterPrimitiveSubregion);
    info->mResultBoundingBox = resultBBox;
  }
}

void
nsSVGFilterInstance::ComputeResultChangeBoxes()
{
  for (PRUint32 i = 0; i < mPrimitives.Length(); ++i) {
    PrimitiveInfo* info = &mPrimitives[i];
    nsAutoTArray<nsIntRect,2> sourceChangeBoxes;
    for (PRUint32 j = 0; j < info->mInputs.Length(); ++j) {
      sourceChangeBoxes.AppendElement(info->mInputs[j]->mResultChangeBox);
    }

    nsIntRect resultChangeBox = info->mFE->ComputeChangeBBox(sourceChangeBoxes, *this);
    info->mResultChangeBox.IntersectRect(resultChangeBox, info->mResultBoundingBox);
  }
}

void
nsSVGFilterInstance::ComputeNeededBoxes()
{
  if (mPrimitives.IsEmpty())
    return;

  // In the end, we need whatever the final filter primitive will draw that
  // intersects the destination dirty area.
  mPrimitives[mPrimitives.Length() - 1].mResultNeededBox.IntersectRect(
    mPrimitives[mPrimitives.Length() - 1].mResultBoundingBox, mDirtyOutputRect);

  for (PRInt32 i = mPrimitives.Length() - 1; i >= 0; --i) {
    PrimitiveInfo* info = &mPrimitives[i];
    nsAutoTArray<nsIntRect,2> sourceBBoxes;
    for (PRUint32 j = 0; j < info->mInputs.Length(); ++j) {
      sourceBBoxes.AppendElement(info->mInputs[j]->mResultBoundingBox);
    }
    
    info->mFE->ComputeNeededSourceBBoxes(
      info->mResultNeededBox, sourceBBoxes, *this);
    // Update each source with the rectangle we need
    for (PRUint32 j = 0; j < info->mInputs.Length(); ++j) {
      nsIntRect* r = &info->mInputs[j]->mResultNeededBox;
      r->UnionRect(*r, sourceBBoxes[j]);
      // Keep everything within the filter effects region
      ClipToFilterSpace(r);
      nsSVGUtils::ClipToGfxRect(r, info->mInputs[j]->mImage.mFilterPrimitiveSubregion);
    }
  }
}

nsIntRect
nsSVGFilterInstance::ComputeUnionOfAllNeededBoxes()
{
  nsIntRect r;
  r.UnionRect(mSourceColorAlpha.mResultNeededBox,
              mSourceAlpha.mResultNeededBox);
  for (PRUint32 i = 0; i < mPrimitives.Length(); ++i) {
    r.UnionRect(r, mPrimitives[i].mResultNeededBox);
  }
  return r;
}

nsresult
nsSVGFilterInstance::BuildSourceImages()
{
  if (mSourceColorAlpha.mResultNeededBox.IsEmpty() &&
      mSourceAlpha.mResultNeededBox.IsEmpty())
    return NS_OK;

  nsRefPtr<gfxImageSurface> sourceColorAlpha = CreateImage();
  if (!sourceColorAlpha)
    return NS_ERROR_OUT_OF_MEMORY;

  nsSVGRenderState tmpState(sourceColorAlpha);
  nsresult rv = mTargetFrame->PaintSVG(&tmpState, nsnull);
  if (NS_FAILED(rv))
    return rv;

  if (!mSourceColorAlpha.mResultNeededBox.IsEmpty()) {
    NS_ASSERTION(mSourceColorAlpha.mImageUsers > 0, "Some user must have needed this");
    mSourceColorAlpha.mImage.mImage = sourceColorAlpha;
    // color model is PREMULTIPLIED SRGB by default.
  }

  if (!mSourceAlpha.mResultNeededBox.IsEmpty()) {
    NS_ASSERTION(mSourceAlpha.mImageUsers > 0, "Some user must have needed this");

    mSourceAlpha.mImage.mImage = CreateImage();
    if (!mSourceAlpha.mImage.mImage)
      return NS_ERROR_OUT_OF_MEMORY;
    // color model is PREMULTIPLIED SRGB by default.

    // Clear the color channel
    const PRUint32* src = reinterpret_cast<PRUint32*>(sourceColorAlpha->Data());
    PRUint32* dest = reinterpret_cast<PRUint32*>(mSourceAlpha.mImage.mImage->Data());
    for (PRInt32 y = 0; y < mSurfaceRect.height; y++) {
      PRUint32 rowOffset = (mSourceAlpha.mImage.mImage->Stride()*y) >> 2;
      for (PRInt32 x = 0; x < mSurfaceRect.width; x++) {
        dest[rowOffset + x] = src[rowOffset + x] & 0xFF000000U;
      }
    }
    mSourceAlpha.mImage.mConstantColorChannels = PR_TRUE;
  }
  
  return NS_OK;
}

void
nsSVGFilterInstance::EnsureColorModel(PrimitiveInfo* aPrimitive,
                                      ColorModel aColorModel)
{
  ColorModel currentModel = aPrimitive->mImage.mColorModel;
  if (aColorModel == currentModel)
    return;

  PRUint8* data = aPrimitive->mImage.mImage->Data();
  PRInt32 stride = aPrimitive->mImage.mImage->Stride();

  nsIntRect r = aPrimitive->mResultNeededBox - mSurfaceRect.TopLeft();

  if (currentModel.mAlphaChannel == ColorModel::PREMULTIPLIED) {
    nsSVGUtils::UnPremultiplyImageDataAlpha(data, stride, r);
  }
  if (aColorModel.mColorSpace != currentModel.mColorSpace) {
    if (aColorModel.mColorSpace == ColorModel::LINEAR_RGB) {
      nsSVGUtils::ConvertImageDataToLinearRGB(data, stride, r);
    } else {
      nsSVGUtils::ConvertImageDataFromLinearRGB(data, stride, r);
    }
  }
  if (aColorModel.mAlphaChannel == ColorModel::PREMULTIPLIED) {
    nsSVGUtils::PremultiplyImageDataAlpha(data, stride, r);
  }
  aPrimitive->mImage.mColorModel = aColorModel;
}

nsresult
nsSVGFilterInstance::Render(gfxASurface** aOutput)
{
  *aOutput = nsnull;

  nsresult rv = BuildSources();
  if (NS_FAILED(rv))
    return rv;

  rv = BuildPrimitives();
  if (NS_FAILED(rv))
    return rv;

  if (mPrimitives.IsEmpty()) {
    // Nothing should be rendered.
    return NS_OK;
  }

  ComputeResultBoundingBoxes();
  ComputeNeededBoxes();
  // For now, we make all surface sizes equal to the union of the
  // bounding boxes needed for each temporary image
  mSurfaceRect = ComputeUnionOfAllNeededBoxes();

  rv = BuildSourceImages();
  if (NS_FAILED(rv))
    return rv;

  for (PRUint32 i = 0; i < mPrimitives.Length(); ++i) {
    PrimitiveInfo* primitive = &mPrimitives[i];

    nsIntRect dataRect;
    // Since mResultNeededBox is clipped to the filter primitive subregion,
    // dataRect is also limited to the filter primitive subregion.
    if (!dataRect.IntersectRect(primitive->mResultNeededBox, mSurfaceRect))
      continue;
    dataRect -= mSurfaceRect.TopLeft();

    primitive->mImage.mImage = CreateImage();
    if (!primitive->mImage.mImage)
      return NS_ERROR_OUT_OF_MEMORY;

    nsAutoTArray<const Image*,2> inputs;
    for (PRUint32 j = 0; j < primitive->mInputs.Length(); ++j) {
      PrimitiveInfo* input = primitive->mInputs[j];
      
      if (!input->mImage.mImage) {
        // This image data is not really going to be used, but we'd better
        // have an image object here so the filter primitive doesn't die.
        input->mImage.mImage =
          new gfxImageSurface(gfxIntSize(1, 1), gfxASurface::ImageFormatARGB32);
        if (!input->mImage.mImage)
          return NS_ERROR_OUT_OF_MEMORY;
      }
      
      ColorModel desiredColorModel =
        primitive->mFE->GetInputColorModel(this, j, &input->mImage);
      EnsureColorModel(input, desiredColorModel);
      NS_ASSERTION(input->mImage.mImage->Stride() == primitive->mImage.mImage->Stride(),
                   "stride mismatch");
      inputs.AppendElement(&input->mImage);
    }

    primitive->mImage.mColorModel = primitive->mFE->GetOutputColorModel(this);

    rv = primitive->mFE->Filter(this, inputs, &primitive->mImage, dataRect);
    if (NS_FAILED(rv))
      return rv;

    for (PRUint32 j = 0; j < primitive->mInputs.Length(); ++j) {
      PrimitiveInfo* input = primitive->mInputs[j];
      --input->mImageUsers;
      NS_ASSERTION(input->mImageUsers >= 0, "Bad mImageUsers tracking");
      if (input->mImageUsers == 0) {
        // Release the image, it's no longer needed
        input->mImage.mImage = nsnull;
      }
    }
  }
  
  PrimitiveInfo* result = &mPrimitives[mPrimitives.Length() - 1];
  ColorModel premulSRGB; // default
  EnsureColorModel(result, premulSRGB);
  gfxImageSurface* surf = nsnull;
  result->mImage.mImage.swap(surf);
  *aOutput = surf;
  return NS_OK;
}

nsresult
nsSVGFilterInstance::ComputeOutputDirtyRect(nsIntRect* aDirty)
{
  *aDirty = nsIntRect();

  nsresult rv = BuildSources();
  if (NS_FAILED(rv))
    return rv;

  rv = BuildPrimitives();
  if (NS_FAILED(rv))
    return rv;

  if (mPrimitives.IsEmpty()) {
    // Nothing should be rendered, so nothing can be dirty.
    return NS_OK;
  }

  ComputeResultBoundingBoxes();

  mSourceColorAlpha.mResultChangeBox = mDirtyInputRect;
  mSourceAlpha.mResultChangeBox = mDirtyInputRect;
  ComputeResultChangeBoxes();

  PrimitiveInfo* result = &mPrimitives[mPrimitives.Length() - 1];
  *aDirty = result->mResultChangeBox;
  return NS_OK;
}
