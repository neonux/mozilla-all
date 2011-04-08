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
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef NSRENDERINGCONTEXT__H__
#define NSRENDERINGCONTEXT__H__

#include "nsCOMPtr.h"
#include "nsIDeviceContext.h"
#include "nsIThebesFontMetrics.h"
#include "nsIRegion.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "nsColor.h"
#include "nsRect.h"
#include "gfxContext.h"

typedef enum {
    nsLineStyle_kNone   = 0,
    nsLineStyle_kSolid  = 1,
    nsLineStyle_kDashed = 2,
    nsLineStyle_kDotted = 3
} nsLineStyle;

#ifdef MOZ_MATHML
/* Struct used for accurate measurements of a string in order
 * to allow precise positioning when processing MathML.
 */
struct nsBoundingMetrics {

    ///////////
    // Metrics that _exactly_ enclose the text:

    // The character coordinate system is the one used on X Windows:
    // 1. The origin is located at the intersection of the baseline
    //    with the left of the character's cell.
    // 2. All horizontal bearings are oriented from left to right.
    // 2. All horizontal bearings are oriented from left to right.
    // 3. The ascent is oriented from bottom to top (being 0 at the orgin).
    // 4. The descent is oriented from top to bottom (being 0 at the origin).

    // Note that Win32/Mac/PostScript use a different convention for
    // the descent (all vertical measurements are oriented from bottom
    // to top on these palatforms). Make sure to flip the sign of the
    // descent on these platforms for cross-platform compatibility.

    // Any of the following member variables listed here can have
    // positive or negative value.

    nscoord leftBearing;
    /* The horizontal distance from the origin of the drawing
       operation to the left-most part of the drawn string. */

    nscoord rightBearing;
    /* The horizontal distance from the origin of the drawing
       operation to the right-most part of the drawn string.
       The _exact_ width of the string is therefore:
       rightBearing - leftBearing */

    nscoord ascent;
    /* The vertical distance from the origin of the drawing
       operation to the top-most part of the drawn string. */

    nscoord descent;
    /* The vertical distance from the origin of the drawing
       operation to the bottom-most part of the drawn string.
       The _exact_ height of the string is therefore:
       ascent + descent */

    nscoord width;
    /* The horizontal distance from the origin of the drawing
       operation to the correct origin for drawing another string
       to follow the current one. Depending on the font, this
       could be greater than or less than the right bearing. */

    nsBoundingMetrics() : leftBearing(0), rightBearing(0),
                          ascent(0), descent(0), width(0)
    {}

    void
    operator += (const nsBoundingMetrics& bm) {
        if (ascent + descent == 0 && rightBearing - leftBearing == 0) {
            ascent = bm.ascent;
            descent = bm.descent;
            leftBearing = width + bm.leftBearing;
            rightBearing = width + bm.rightBearing;
        }
        else {
            if (ascent < bm.ascent) ascent = bm.ascent;
            if (descent < bm.descent) descent = bm.descent;
            leftBearing = PR_MIN(leftBearing, width + bm.leftBearing);
            rightBearing = PR_MAX(rightBearing, width + bm.rightBearing);
        }
        width += bm.width;
    }
};
#endif // MOZ_MATHML


class nsRenderingContext
{
public:
    nsRenderingContext() : mP2A(0.) {}
    // ~nsRenderingContext() {}

    NS_INLINE_DECL_REFCOUNTING(nsRenderingContext)

    void Init(nsIDeviceContext* aContext, gfxASurface* aThebesSurface);
    void Init(nsIDeviceContext* aContext, gfxContext* aThebesContext);

    // These accessors will never return null.
    gfxContext *ThebesContext() { return mThebes; }
    nsIDeviceContext *DeviceContext() { return mDeviceContext; }
    PRInt32 AppUnitsPerDevPixel() { return mP2A; }

    // Graphics state

    void PushState(void);
    void PopState(void);
    void IntersectClip(const nsRect& aRect);
    void SetClip(const nsIntRegion& aRegion);
    void SetLineStyle(nsLineStyle aLineStyle);
    void SetColor(nscolor aColor);
    void Translate(const nsPoint& aPt);
    void Scale(float aSx, float aSy);

    class AutoPushTranslation {
        nsRenderingContext* mCtx;
    public:
        AutoPushTranslation(nsRenderingContext* aCtx, const nsPoint& aPt)
            : mCtx(aCtx) {
            mCtx->PushState();
            mCtx->Translate(aPt);
        }
        ~AutoPushTranslation() {
            mCtx->PopState();
        }
    };

    // Shapes

    void DrawLine(const nsPoint& aStartPt, const nsPoint& aEndPt);
    void DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1);
    void DrawRect(const nsRect& aRect);
    void DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    void DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    void DrawEllipse(const nsRect& aRect);

    void FillRect(const nsRect& aRect);
    void FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    void FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints);

    void FillEllipse(const nsRect& aRect);
    void FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

    void InvertRect(const nsRect& aRect);
    void InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

    // Text

    void SetFont(const nsFont& aFont, nsIAtom* aLanguage,
                 gfxUserFontSet *aUserFontSet);
    void SetFont(const nsFont& aFont, gfxUserFontSet *aUserFontSet);
    void SetFont(nsIFontMetrics *aFontMetrics);
    nsIThebesFontMetrics *FontMetrics() { return mFontMetrics; } // may be null

    void SetRightToLeftText(PRBool aIsRTL);
    void SetTextRunRTL(PRBool aIsRTL);

    nscoord GetWidth(char aC);
    nscoord GetWidth(PRUnichar aC);
    nscoord GetWidth(const nsString& aString);
    nscoord GetWidth(const char* aString);
    nscoord GetWidth(const char* aString, PRUint32 aLength);
    nscoord GetWidth(const PRUnichar *aString, PRUint32 aLength);

#ifdef MOZ_MATHML
    nsBoundingMetrics GetBoundingMetrics(const PRUnichar *aString,
                                         PRUint32 aLength);
#endif

    void DrawString(const nsString& aString, nscoord aX, nscoord aY);
    void DrawString(const char *aString, PRUint32 aLength,
                    nscoord aX, nscoord aY);
    void DrawString(const PRUnichar *aString, PRUint32 aLength,
                    nscoord aX, nscoord aY);

protected:
    PRInt32 GetMaxChunkLength();
    nscoord GetWidthInternal(const char *aString, PRUint32 aLength);
    nscoord GetWidthInternal(const PRUnichar *aString, PRUint32 aLength);

    nsRefPtr<gfxContext> mThebes;
    nsCOMPtr<nsIDeviceContext> mDeviceContext;
    nsCOMPtr<nsIThebesFontMetrics> mFontMetrics;

    double mP2A; // cached app units per device pixel value
};

#endif  // NSRENDERINGCONTEXT__H__
