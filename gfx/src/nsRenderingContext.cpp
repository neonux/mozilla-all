/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "nsRenderingContext.h"

// XXXTodo: rename FORM_TWIPS to FROM_APPUNITS
#define FROM_TWIPS(_x)  ((gfxFloat)((_x)/(mP2A)))
#define FROM_TWIPS_INT(_x)  (NSToIntRound((gfxFloat)((_x)/(mP2A))))
#define TO_TWIPS(_x)    ((nscoord)((_x)*(mP2A)))
#define GFX_RECT_FROM_TWIPS_RECT(_r)   (gfxRect(FROM_TWIPS((_r).x), FROM_TWIPS((_r).y), FROM_TWIPS((_r).width), FROM_TWIPS((_r).height)))

// Hard limit substring lengths to 8000 characters ... this lets us statically
// size the cluster buffer array in FindSafeLength
#define MAX_GFX_TEXT_BUF_SIZE 8000

static PRInt32 FindSafeLength(const PRUnichar *aString, PRUint32 aLength,
                              PRUint32 aMaxChunkLength)
{
    if (aLength <= aMaxChunkLength)
        return aLength;

    PRInt32 len = aMaxChunkLength;

    // Ensure that we don't break inside a surrogate pair
    while (len > 0 && NS_IS_LOW_SURROGATE(aString[len])) {
        len--;
    }
    if (len == 0) {
        // We don't want our caller to go into an infinite loop, so don't
        // return zero. It's hard to imagine how we could actually get here
        // unless there are languages that allow clusters of arbitrary size.
        // If there are and someone feeds us a 500+ character cluster, too
        // bad.
        return aMaxChunkLength;
    }
    return len;
}

static PRInt32 FindSafeLength(const char *aString, PRUint32 aLength,
                              PRUint32 aMaxChunkLength)
{
    // Since it's ASCII, we don't need to worry about clusters or RTL
    return PR_MIN(aLength, aMaxChunkLength);
}

//////////////////////////////////////////////////////////////////////
//// nsRenderingContext

nsresult
nsRenderingContext::Init(nsIDeviceContext* aContext,
                         gfxASurface *aThebesSurface)
{
    return Init(aContext, new gfxContext(aThebesSurface));
}

nsresult
nsRenderingContext::Init(nsIDeviceContext* aContext,
                         gfxContext *aThebesContext)
{
    mDeviceContext = aContext;
    mThebes = aThebesContext;

    mThebes->SetLineWidth(1.0);
    mP2A = mDeviceContext->AppUnitsPerDevPixel();

    return NS_OK;
}

already_AddRefed<nsIDeviceContext>
nsRenderingContext::GetDeviceContext()
{
    NS_IF_ADDREF(mDeviceContext);
    return mDeviceContext.get();
}

nsresult
nsRenderingContext::PushState()
{
    mThebes->Save();
    return NS_OK;
}

nsresult
nsRenderingContext::PopState()
{
    mThebes->Restore();
    return NS_OK;
}

//
// clipping
//

nsresult
nsRenderingContext::SetClipRect(const nsRect& aRect, nsClipCombine aCombine)
{
    if (aCombine == nsClipCombine_kReplace) {
        mThebes->ResetClip();
    } else if (aCombine != nsClipCombine_kIntersect) {
        NS_WARNING("Unexpected usage of SetClipRect");
    }

    mThebes->NewPath();
    gfxRect clipRect(GFX_RECT_FROM_TWIPS_RECT(aRect));
    if (mThebes->UserToDevicePixelSnapped(clipRect, PR_TRUE)) {
        gfxMatrix mat(mThebes->CurrentMatrix());
        mThebes->IdentityMatrix();
        mThebes->Rectangle(clipRect);
        mThebes->SetMatrix(mat);
    } else {
        mThebes->Rectangle(clipRect);
    }

    mThebes->Clip();

    return NS_OK;
}

nsresult
nsRenderingContext::SetClipRegion(const nsIntRegion& aRegion,
                                  nsClipCombine aCombine)
{
    // Region is in device coords, no transformation.
    // This should only be called when there is no transform in place, when we
    // we just start painting a widget. The region is set by the platform paint
    // routine.
    NS_ASSERTION(aCombine == nsClipCombine_kReplace,
                 "Unexpected usage of SetClipRegion");

    gfxMatrix mat = mThebes->CurrentMatrix();
    mThebes->IdentityMatrix();

    mThebes->ResetClip();

    mThebes->NewPath();
    nsIntRegionRectIterator iter(aRegion);
    const nsIntRect* rect;
    while ((rect = iter.Next())) {
        mThebes->Rectangle(gfxRect(rect->x, rect->y, rect->width, rect->height),
                           PR_TRUE);
    }
    mThebes->Clip();

    mThebes->SetMatrix(mat);

    return NS_OK;
}

//
// other junk
//

nsresult
nsRenderingContext::SetLineStyle(nsLineStyle aLineStyle)
{
    switch (aLineStyle) {
        case nsLineStyle_kSolid:
            mThebes->SetDash(gfxContext::gfxLineSolid);
            break;
        case nsLineStyle_kDashed:
            mThebes->SetDash(gfxContext::gfxLineDashed);
            break;
        case nsLineStyle_kDotted:
            mThebes->SetDash(gfxContext::gfxLineDotted);
            break;
        case nsLineStyle_kNone:
        default:
            // nothing uses kNone
            NS_ERROR("SetLineStyle: Invalid line style");
            break;
    }
    return NS_OK;
}


nsresult
nsRenderingContext::SetColor(nscolor aColor)
{
    /* This sets the color assuming the sRGB color space, since that's
     * what all CSS colors are defined to be in by the spec.
     */
    mThebes->SetColor(gfxRGBA(aColor));
    return NS_OK;
}

nsresult
nsRenderingContext::Translate(const nsPoint& aPt)
{
    mThebes->Translate (gfxPoint(FROM_TWIPS(aPt.x), FROM_TWIPS(aPt.y)));
    return NS_OK;
}

nsresult
nsRenderingContext::Scale(float aSx, float aSy)
{
    mThebes->Scale (aSx, aSy);
    return NS_OK;
}

nsresult
nsRenderingContext::DrawLine(const nsPoint& aStartPt, const nsPoint& aEndPt)
{
    return DrawLine(aStartPt.x, aStartPt.y, aEndPt.x, aEndPt.y);
}

nsresult
nsRenderingContext::DrawLine(nscoord aX0, nscoord aY0,
                             nscoord aX1, nscoord aY1)
{
    gfxPoint p0 = gfxPoint(FROM_TWIPS(aX0), FROM_TWIPS(aY0));
    gfxPoint p1 = gfxPoint(FROM_TWIPS(aX1), FROM_TWIPS(aY1));

    // we can't draw thick lines with gfx, so we always assume we want
    // pixel-aligned lines if the rendering context is at 1.0 scale
    gfxMatrix savedMatrix = mThebes->CurrentMatrix();
    if (!savedMatrix.HasNonTranslation()) {
        p0 = mThebes->UserToDevice(p0);
        p1 = mThebes->UserToDevice(p1);

        p0.Round();
        p1.Round();

        mThebes->IdentityMatrix();

        mThebes->NewPath();

        // snap straight lines
        if (p0.x == p1.x) {
            mThebes->Line(p0 + gfxPoint(0.5, 0),
                          p1 + gfxPoint(0.5, 0));
        } else if (p0.y == p1.y) {
            mThebes->Line(p0 + gfxPoint(0, 0.5),
                          p1 + gfxPoint(0, 0.5));
        } else {
            mThebes->Line(p0, p1);
        }

        mThebes->Stroke();

        mThebes->SetMatrix(savedMatrix);
    } else {
        mThebes->NewPath();
        mThebes->Line(p0, p1);
        mThebes->Stroke();
    }

    return NS_OK;
}

nsresult
nsRenderingContext::DrawRect(const nsRect& aRect)
{
    mThebes->NewPath();
    mThebes->Rectangle(GFX_RECT_FROM_TWIPS_RECT(aRect), PR_TRUE);
    mThebes->Stroke();

    return NS_OK;
}

nsresult
nsRenderingContext::DrawRect(nscoord aX, nscoord aY,
                             nscoord aWidth, nscoord aHeight)
{
    DrawRect(nsRect(aX, aY, aWidth, aHeight));
    return NS_OK;
}


/* Clamp r to (0,0) (2^23,2^23)
 * these are to be device coordinates.
 *
 * Returns PR_FALSE if the rectangle is completely out of bounds,
 * PR_TRUE otherwise.
 *
 * This function assumes that it will be called with a rectangle being
 * drawn into a surface with an identity transformation matrix; that
 * is, anything above or to the left of (0,0) will be offscreen.
 *
 * First it checks if the rectangle is entirely beyond
 * CAIRO_COORD_MAX; if so, it can't ever appear on the screen --
 * PR_FALSE is returned.
 *
 * Then it shifts any rectangles with x/y < 0 so that x and y are = 0,
 * and adjusts the width and height appropriately.  For example, a
 * rectangle from (0,-5) with dimensions (5,10) will become a
 * rectangle from (0,0) with dimensions (5,5).
 *
 * If after negative x/y adjustment to 0, either the width or height
 * is negative, then the rectangle is completely offscreen, and
 * nothing is drawn -- PR_FALSE is returned.
 *
 * Finally, if x+width or y+height are greater than CAIRO_COORD_MAX,
 * the width and height are clamped such x+width or y+height are equal
 * to CAIRO_COORD_MAX, and PR_TRUE is returned.
 */
#define CAIRO_COORD_MAX (double(0x7fffff))

static PRBool
ConditionRect(gfxRect& r) {
    // if either x or y is way out of bounds;
    // note that we don't handle negative w/h here
    if (r.pos.x > CAIRO_COORD_MAX || r.pos.y > CAIRO_COORD_MAX)
        return PR_FALSE;

    if (r.pos.x < 0.0) {
        r.size.width += r.pos.x;
        if (r.size.width < 0.0)
            return PR_FALSE;
        r.pos.x = 0.0;
    }

    if (r.pos.x + r.size.width > CAIRO_COORD_MAX) {
        r.size.width = CAIRO_COORD_MAX - r.pos.x;
    }

    if (r.pos.y < 0.0) {
        r.size.height += r.pos.y;
        if (r.size.height < 0.0)
            return PR_FALSE;

        r.pos.y = 0.0;
    }

    if (r.pos.y + r.size.height > CAIRO_COORD_MAX) {
        r.size.height = CAIRO_COORD_MAX - r.pos.y;
    }
    return PR_TRUE;
}

nsresult
nsRenderingContext::FillRect(const nsRect& aRect)
{
    gfxRect r(GFX_RECT_FROM_TWIPS_RECT(aRect));

    /* Clamp coordinates to work around a design bug in cairo */
    nscoord bigval = (nscoord)(CAIRO_COORD_MAX*mP2A);
    if (aRect.width > bigval ||
        aRect.height > bigval ||
        aRect.x < -bigval ||
        aRect.x > bigval ||
        aRect.y < -bigval ||
        aRect.y > bigval)
    {
        gfxMatrix mat = mThebes->CurrentMatrix();

        r = mat.Transform(r);

        if (!ConditionRect(r))
            return NS_OK;

        mThebes->IdentityMatrix();
        mThebes->NewPath();

        mThebes->Rectangle(r, PR_TRUE);
        mThebes->Fill();
        mThebes->SetMatrix(mat);

        return NS_OK;
    }

    mThebes->NewPath();
    mThebes->Rectangle(r, PR_TRUE);
    mThebes->Fill();

    return NS_OK;
}

nsresult
nsRenderingContext::FillRect(nscoord aX, nscoord aY,
                             nscoord aWidth, nscoord aHeight)
{
    FillRect(nsRect(aX, aY, aWidth, aHeight));
    return NS_OK;
}

nsresult
nsRenderingContext::InvertRect(const nsRect& aRect)
{
    gfxContext::GraphicsOperator lastOp = mThebes->CurrentOperator();

    mThebes->SetOperator(gfxContext::OPERATOR_XOR);
    nsresult rv = FillRect(aRect);
    mThebes->SetOperator(lastOp);

    return rv;
}

nsresult
nsRenderingContext::InvertRect(nscoord aX, nscoord aY,
                               nscoord aWidth, nscoord aHeight)
{
    return InvertRect(nsRect(aX, aY, aWidth, aHeight));
}

nsresult
nsRenderingContext::DrawEllipse(const nsRect& aRect)
{
    return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

nsresult
nsRenderingContext::DrawEllipse(nscoord aX, nscoord aY,
                                nscoord aWidth, nscoord aHeight)
{
    mThebes->NewPath();
    mThebes->Ellipse(gfxPoint(FROM_TWIPS(aX) + FROM_TWIPS(aWidth)/2.0,
                              FROM_TWIPS(aY) + FROM_TWIPS(aHeight)/2.0),
                     gfxSize(FROM_TWIPS(aWidth),
                             FROM_TWIPS(aHeight)));
    mThebes->Stroke();

    return NS_OK;
}

nsresult
nsRenderingContext::FillEllipse(const nsRect& aRect)
{
    return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

nsresult
nsRenderingContext::FillEllipse(nscoord aX, nscoord aY,
                                nscoord aWidth, nscoord aHeight)
{
    mThebes->NewPath();
    mThebes->Ellipse(gfxPoint(FROM_TWIPS(aX) + FROM_TWIPS(aWidth)/2.0,
                              FROM_TWIPS(aY) + FROM_TWIPS(aHeight)/2.0),
                     gfxSize(FROM_TWIPS(aWidth),
                             FROM_TWIPS(aHeight)));
    mThebes->Fill();

    return NS_OK;
}

nsresult
nsRenderingContext::FillPolygon(const nsPoint twPoints[], PRInt32 aNumPoints)
{
    if (aNumPoints == 0)
        return NS_OK;

    if (aNumPoints == 4) {
    }

    nsAutoArrayPtr<gfxPoint> pxPoints(new gfxPoint[aNumPoints]);

    for (int i = 0; i < aNumPoints; i++) {
        pxPoints[i].x = FROM_TWIPS(twPoints[i].x);
        pxPoints[i].y = FROM_TWIPS(twPoints[i].y);
    }

    mThebes->NewPath();
    mThebes->Polygon(pxPoints, aNumPoints);
    mThebes->Fill();

    return NS_OK;
}

// text

nsresult
nsRenderingContext::SetRightToLeftText(PRBool aIsRTL)
{
    return mFontMetrics->SetRightToLeftText(aIsRTL);
}

void
nsRenderingContext::SetTextRunRTL(PRBool aIsRTL)
{
    mFontMetrics->SetTextRunRTL(aIsRTL);
}

nsresult
nsRenderingContext::SetFont(const nsFont& aFont, nsIAtom* aLanguage,
                            gfxUserFontSet *aUserFontSet)
{
    nsCOMPtr<nsIFontMetrics> newMetrics;
    mDeviceContext->GetMetricsFor(aFont, aLanguage, aUserFontSet,
                                  *getter_AddRefs(newMetrics));
    mFontMetrics = reinterpret_cast<nsIThebesFontMetrics*>(newMetrics.get());
    return NS_OK;
}

nsresult
nsRenderingContext::SetFont(const nsFont& aFont,
                            gfxUserFontSet *aUserFontSet)
{
    nsCOMPtr<nsIFontMetrics> newMetrics;
    mDeviceContext->GetMetricsFor(aFont, nsnull, aUserFontSet,
                                  *getter_AddRefs(newMetrics));
    mFontMetrics = reinterpret_cast<nsIThebesFontMetrics*>(newMetrics.get());
    return NS_OK;
}

nsresult
nsRenderingContext::SetFont(nsIFontMetrics *aFontMetrics)
{
    mFontMetrics = static_cast<nsIThebesFontMetrics*>(aFontMetrics);
    return NS_OK;
}

already_AddRefed<nsIFontMetrics>
nsRenderingContext::GetFontMetrics()
{
    NS_IF_ADDREF(mFontMetrics);
    return mFontMetrics.get();
}

PRInt32
nsRenderingContext::GetMaxChunkLength()
{
    if (!mFontMetrics)
        return 1;
    return PR_MIN(mFontMetrics->GetMaxStringLength(), MAX_GFX_TEXT_BUF_SIZE);
}

nsresult
nsRenderingContext::GetWidth(char aC, nscoord &aWidth)
{
    if (aC == ' ' && mFontMetrics)
        return mFontMetrics->GetSpaceWidth(aWidth);

    return GetWidth(&aC, 1, aWidth);
}

nsresult
nsRenderingContext::GetWidth(PRUnichar aC, nscoord &aWidth, PRInt32 *aFontID)
{
    return GetWidth(&aC, 1, aWidth, aFontID);
}

nsresult
nsRenderingContext::GetWidth(const nsString& aString, nscoord &aWidth,
                                   PRInt32 *aFontID)
{
    return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

nsresult
nsRenderingContext::GetWidth(const char* aString, nscoord& aWidth)
{
    return GetWidth(aString, strlen(aString), aWidth);
}

nsresult
nsRenderingContext::DrawString(const nsString& aString, nscoord aX, nscoord aY,
                               PRInt32 aFontID, const nscoord* aSpacing)
{
    return DrawString(aString.get(), aString.Length(), aX, aY,
                      aFontID, aSpacing);
}

nsresult
nsRenderingContext::GetWidth(const char* aString,
                             PRUint32 aLength,
                             nscoord& aWidth)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    aWidth = 0;
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
        nscoord width;
        nsresult rv = GetWidthInternal(aString, len, width);
        if (NS_FAILED(rv))
            return rv;
        aWidth += width;
        aLength -= len;
        aString += len;
    }
    return NS_OK;
}

nsresult
nsRenderingContext::GetWidth(const PRUnichar *aString,
                             PRUint32 aLength,
                             nscoord &aWidth,
                             PRInt32 *aFontID)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    aWidth = 0;

    if (aFontID) {
        *aFontID = 0;
    }

    while (aLength > 0) {
        PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
        nscoord width;
        nsresult rv = GetWidthInternal(aString, len, width);
        if (NS_FAILED(rv))
            return rv;
        aWidth += width;
        aLength -= len;
        aString += len;
    }
    return NS_OK;
}

#ifdef MOZ_MATHML
nsresult
nsRenderingContext::GetBoundingMetrics(const PRUnichar*   aString,
                                       PRUint32           aLength,
                                       nsBoundingMetrics& aBoundingMetrics,
                                       PRInt32*           aFontID)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    if (aLength <= maxChunkLength)
        return GetBoundingMetricsInternal(aString, aLength, aBoundingMetrics,
                                          aFontID);

    if (aFontID) {
        *aFontID = 0;
    }

    PRBool firstIteration = PR_TRUE;
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
        nsBoundingMetrics metrics;
        nsresult rv = GetBoundingMetricsInternal(aString, len, metrics);
        if (NS_FAILED(rv))
            return rv;
        if (firstIteration) {
            // Instead of combining with a Clear()ed nsBoundingMetrics, we
            // assign directly in the first iteration. This ensures that
            // negative ascent/ descent can be returned and the left bearing
            // is properly initialized.
            aBoundingMetrics = metrics;
        } else {
            aBoundingMetrics += metrics;
        }
        aLength -= len;
        aString += len;
        firstIteration = PR_FALSE;
    }
    return NS_OK;
}
#endif

nsresult
nsRenderingContext::DrawString(const char *aString, PRUint32 aLength,
                               nscoord aX, nscoord aY,
                               const nscoord* aSpacing)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
        nsresult rv = DrawStringInternal(aString, len, aX, aY);
        if (NS_FAILED(rv))
            return rv;
        aLength -= len;

        if (aLength > 0) {
            nscoord width;
            rv = GetWidthInternal(aString, len, width);
            if (NS_FAILED(rv))
                return rv;
            aX += width;
            aString += len;
        }
    }
    return NS_OK;
}

nsresult
nsRenderingContext::DrawString(const PRUnichar *aString, PRUint32 aLength,
                               nscoord aX, nscoord aY,
                               PRInt32 aFontID,
                               const nscoord* aSpacing)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    if (aLength <= maxChunkLength) {
        return DrawStringInternal(aString, aLength, aX, aY, aFontID, aSpacing);
    }

    PRBool isRTL = mFontMetrics->GetRightToLeftText();

    if (isRTL) {
        nscoord totalWidth = 0;
        if (aSpacing) {
            for (PRUint32 i = 0; i < aLength; ++i) {
                totalWidth += aSpacing[i];
            }
        } else {
            nsresult rv = GetWidth(aString, aLength, totalWidth);
            if (NS_FAILED(rv))
                return rv;
        }
        aX += totalWidth;
    }

    while (aLength > 0) {
        PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
        nscoord width = 0;
        if (aSpacing) {
            for (PRInt32 i = 0; i < len; ++i) {
                width += aSpacing[i];
            }
        } else {
            nsresult rv = GetWidthInternal(aString, len, width);
            if (NS_FAILED(rv))
                return rv;
        }

        if (isRTL) {
            aX -= width;
        }
        nsresult rv = DrawStringInternal(aString, len, aX, aY, aFontID, aSpacing);
        if (NS_FAILED(rv))
            return rv;
        aLength -= len;
        if (!isRTL) {
            aX += width;
        }
        aString += len;
        if (aSpacing) {
            aSpacing += len;
        }
    }
    return NS_OK;
}

nsresult
nsRenderingContext::GetWidthInternal(const char* aString, PRUint32 aLength,
                                     nscoord& aWidth)
{
    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    return mFontMetrics->GetWidth(aString, aLength, aWidth, this);
}

nsresult
nsRenderingContext::GetWidthInternal(const PRUnichar *aString, PRUint32 aLength,
                                     nscoord &aWidth, PRInt32 *aFontID)
{
    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    return mFontMetrics->GetWidth(aString, aLength, aWidth, aFontID, this);
}

#ifdef MOZ_MATHML
nsresult
nsRenderingContext::GetBoundingMetricsInternal(const PRUnichar*   aString,
                                               PRUint32           aLength,
                                               nsBoundingMetrics& aBoundingMetrics,
                                               PRInt32*           aFontID)
{
    return mFontMetrics->GetBoundingMetrics(aString, aLength, this,
                                            aBoundingMetrics);
}
#endif // MOZ_MATHML

nsresult
nsRenderingContext::DrawStringInternal(const char *aString, PRUint32 aLength,
                                       nscoord aX, nscoord aY,
                                       const nscoord* aSpacing)
{
    return mFontMetrics->DrawString(aString, aLength, aX, aY, aSpacing, this);
}

nsresult
nsRenderingContext::DrawStringInternal(const PRUnichar *aString,
                                       PRUint32 aLength,
                                       nscoord aX, nscoord aY,
                                       PRInt32 aFontID,
                                       const nscoord* aSpacing)
{
    return mFontMetrics->DrawString(aString, aLength, aX, aY, aFontID,
                                    aSpacing, this);
}
