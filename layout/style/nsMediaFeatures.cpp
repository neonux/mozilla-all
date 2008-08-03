/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is nsMediaFeatures.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

/* the features that media queries can test */

#include "nsMediaFeatures.h"
#include "nsGkAtoms.h"
#include "nsCSSKeywords.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDeviceContext.h"
#include "nsCSSValue.h"
#include "nsIDocShell.h"
#include "nsLayoutUtils.h"

static const PRInt32 kOrientationKeywords[] = {
  eCSSKeyword_portrait,                 NS_STYLE_ORIENTATION_PORTRAIT,
  eCSSKeyword_landscape,                NS_STYLE_ORIENTATION_LANDSCAPE,
  eCSSKeyword_UNKNOWN,                  -1
};

static const PRInt32 kScanKeywords[] = {
  eCSSKeyword_progressive,              NS_STYLE_SCAN_PROGRESSIVE,
  eCSSKeyword_interlace,                NS_STYLE_SCAN_INTERLACE,
  eCSSKeyword_UNKNOWN,                  -1
};

PR_STATIC_CALLBACK(nsresult)
GetWidth(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    nscoord width = aPresContext->GetVisibleArea().width;
    float pixelWidth = aPresContext->AppUnitsToFloatCSSPixels(width);
    aResult.SetFloatValue(pixelWidth, eCSSUnit_Pixel);
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetHeight(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    nscoord height = aPresContext->GetVisibleArea().height;
    float pixelHeight = aPresContext->AppUnitsToFloatCSSPixels(height);
    aResult.SetFloatValue(pixelHeight, eCSSUnit_Pixel);
    return NS_OK;
}

static nsIDeviceContext*
GetDeviceContextFor(nsPresContext* aPresContext)
{
  // Do this dance rather than aPresContext->DeviceContext() to get
  // things right in multi-monitor situations.
  // (It's not clear if this is really needed for GetDepth and GetColor,
  // but do it anyway.)
  return nsLayoutUtils::GetDeviceContextForScreenInfo(
    nsCOMPtr<nsIDocShell>(do_QueryInterface(
      nsCOMPtr<nsISupports>(aPresContext->GetContainer()))));
}

PR_STATIC_CALLBACK(nsresult)
GetDeviceWidth(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    // XXX: I'm not sure if this is really the right thing for print:
    // do we want to include unprintable areas / page margins?
    nsIDeviceContext *dx = GetDeviceContextFor(aPresContext);
    nscoord width, height;
    dx->GetDeviceSurfaceDimensions(width, height);
    float pixelWidth = aPresContext->AppUnitsToFloatCSSPixels(width);
    aResult.SetFloatValue(pixelWidth, eCSSUnit_Pixel);
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetDeviceHeight(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    // XXX: I'm not sure if this is really the right thing for print:
    // do we want to include unprintable areas / page margins?
    nsIDeviceContext *dx = GetDeviceContextFor(aPresContext);
    nscoord width, height;
    dx->GetDeviceSurfaceDimensions(width, height);
    float pixelHeight = aPresContext->AppUnitsToFloatCSSPixels(height);
    aResult.SetFloatValue(pixelHeight, eCSSUnit_Pixel);
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetOrientation(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    nsSize size = aPresContext->GetVisibleArea().Size();
    PRInt32 orientation;
    if (size.width > size.height) {
        orientation = NS_STYLE_ORIENTATION_LANDSCAPE;
    } else {
        // Per spec, square viewports should be 'portrait'
        orientation = NS_STYLE_ORIENTATION_PORTRAIT;
    }

    aResult.SetIntValue(orientation, eCSSUnit_Enumerated);
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetAspectRatio(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    nsRefPtr<nsCSSValue::Array> a = nsCSSValue::Array::Create(2);
    NS_ENSURE_TRUE(a, NS_ERROR_OUT_OF_MEMORY);

    nsSize size = aPresContext->GetVisibleArea().Size();
    a->Item(0).SetIntValue(size.width, eCSSUnit_Integer);
    a->Item(1).SetIntValue(size.height, eCSSUnit_Integer);

    aResult.SetArrayValue(a, eCSSUnit_Array);
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetDeviceAspectRatio(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    nsRefPtr<nsCSSValue::Array> a = nsCSSValue::Array::Create(2);
    NS_ENSURE_TRUE(a, NS_ERROR_OUT_OF_MEMORY);

    // XXX: I'm not sure if this is really the right thing for print:
    // do we want to include unprintable areas / page margins?
    nsIDeviceContext *dx = GetDeviceContextFor(aPresContext);
    nscoord width, height;
    dx->GetDeviceSurfaceDimensions(width, height);
    a->Item(0).SetIntValue(width, eCSSUnit_Integer);
    a->Item(1).SetIntValue(height, eCSSUnit_Integer);

    aResult.SetArrayValue(a, eCSSUnit_Array);
    return NS_OK;
}


PR_STATIC_CALLBACK(nsresult)
GetColor(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    // FIXME:  This implementation is bogus.  nsThebesDeviceContext
    // doesn't provide reliable information (should be fixed in bug
    // 424386).
    // FIXME: On a monochrome device, return 0!
    nsIDeviceContext *dx = GetDeviceContextFor(aPresContext);
    PRUint32 depth;
    dx->GetDepth(depth);
    // Some graphics backends may claim 32-bit depth when it's really 24
    // (because they're counting the Alpha component).
    if (depth == 32) {
        depth = 24;
    }
    // The spec says to use bits *per color component*, so divide by 3,
    // and round down, since the spec says to use the smallest when the
    // color components differ.
    depth /= 3;
    aResult.SetIntValue(PRInt32(depth), eCSSUnit_Integer);
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetColorIndex(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    // We should return zero if the device does not use a color lookup
    // table.  Stuart says that our handling of displays with 8-bit
    // color is bad enough that we never change the lookup table to
    // match what we're trying to display, so perhaps we should always
    // return zero.  Given that there isn't any better information
    // exposed, we don't have much other choice.
    aResult.SetIntValue(0, eCSSUnit_Integer);
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetMonochrome(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    // For color devices we should return 0.
    // FIXME: On a monochrome device, return the actual color depth, not
    // 0!
    aResult.SetIntValue(0, eCSSUnit_Integer);
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetResolution(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    // Resolution values are in device pixels, not CSS pixels.
    nsIDeviceContext *dx = GetDeviceContextFor(aPresContext);
    float dpi = float(dx->AppUnitsPerInch()) / float(dx->AppUnitsPerDevPixel());
    aResult.SetFloatValue(dpi, eCSSUnit_Inch);
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetScan(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    // Since Gecko doesn't support the 'tv' media type, the 'scan'
    // feature is never present.
    aResult.Reset();
    return NS_OK;
}

PR_STATIC_CALLBACK(nsresult)
GetGrid(nsPresContext* aPresContext, nsCSSValue& aResult)
{
    // Gecko doesn't support grid devices (e.g., ttys), so the 'grid'
    // feature is always 0.
    aResult.SetIntValue(0, eCSSUnit_Integer);
    return NS_OK;
}

/*
 * Adding new media features requires (1) adding the new feature to this
 * array, with appropriate entries (and potentially any new code needed
 * to support new types in these entries and (2) ensuring that either
 * nsPresContext::MediaFeatureValuesChanged or
 * nsPresContext::PostMediaFeatureValuesChangedEvent is called when the
 * value that would be returned by the entry's mGetter changes.
 */

/* static */ const nsMediaFeature
nsMediaFeatures::features[] = {
    {
        &nsGkAtoms::width,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eLength,
        nsnull,
        GetWidth
    },
    {
        &nsGkAtoms::height,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eLength,
        nsnull,
        GetHeight
    },
    {
        &nsGkAtoms::deviceWidth,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eLength,
        nsnull,
        GetDeviceWidth
    },
    {
        &nsGkAtoms::deviceHeight,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eLength,
        nsnull,
        GetDeviceHeight
    },
    {
        &nsGkAtoms::orientation,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eEnumerated,
        kOrientationKeywords,
        GetOrientation
    },
    {
        &nsGkAtoms::aspectRatio,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eIntRatio,
        nsnull,
        GetAspectRatio
    },
    {
        &nsGkAtoms::deviceAspectRatio,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eIntRatio,
        nsnull,
        GetDeviceAspectRatio
    },
    {
        &nsGkAtoms::color,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eInteger,
        nsnull,
        GetColor
    },
    {
        &nsGkAtoms::colorIndex,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eInteger,
        nsnull,
        GetColorIndex
    },
    {
        &nsGkAtoms::monochrome,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eInteger,
        nsnull,
        GetMonochrome
    },
    {
        &nsGkAtoms::resolution,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eResolution,
        nsnull,
        GetResolution
    },
    {
        &nsGkAtoms::scan,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eEnumerated,
        kScanKeywords,
        GetScan
    },
    {
        &nsGkAtoms::grid,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eInteger,
        nsnull,
        GetGrid
    },
    // Null-mName terminator:
    {
        nsnull,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eInteger,
        nsnull,
        nsnull
    },
};
