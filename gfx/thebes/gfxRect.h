/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#ifndef GFX_RECT_H
#define GFX_RECT_H

#include "gfxTypes.h"
#include "gfxPoint.h"
#include "gfxCore.h"
#include "nsDebug.h" 

namespace mozilla {
    namespace css {
        enum Corner {
            // this order is important!
            eCornerTopLeft = 0,
            eCornerTopRight = 1,
            eCornerBottomRight = 2,
            eCornerBottomLeft = 3,
            eNumCorners = 4
        };
    }
}
#define NS_CORNER_TOP_LEFT mozilla::css::eCornerTopLeft
#define NS_CORNER_TOP_RIGHT mozilla::css::eCornerTopRight
#define NS_CORNER_BOTTOM_RIGHT mozilla::css::eCornerBottomRight
#define NS_CORNER_BOTTOM_LEFT mozilla::css::eCornerBottomLeft
#define NS_NUM_CORNERS mozilla::css::eNumCorners

#define NS_FOR_CSS_CORNERS(var_)                         \
    for (mozilla::css::Corner var_ = NS_CORNER_TOP_LEFT; \
         var_ <= NS_CORNER_BOTTOM_LEFT;                  \
         var_++)

static inline mozilla::css::Corner operator++(mozilla::css::Corner& corner, int) {
    NS_PRECONDITION(corner >= NS_CORNER_TOP_LEFT &&
                    corner < NS_NUM_CORNERS, "Out of range corner");
    corner = mozilla::css::Corner(corner + 1);
    return corner;
}

struct THEBES_API gfxRect
{
    gfxFloat x, y;
    gfxFloat width, height;

    gfxRect() {}
    gfxRect(const gfxPoint& _pos, const gfxSize& _size) :
        x(_pos.x), y(_pos.y), width(_size.width), height(_size.height) {}
    gfxRect(gfxFloat _x, gfxFloat _y, gfxFloat _width, gfxFloat _height) :
        x(_x), y(_y), width(_width), height(_height) {}

    int operator==(const gfxRect& s) const {
        return x == s.x && y == s.y && width == s.width && height == s.height;
    }
    PRBool IsEqualEdges(const gfxRect& aRect) const {
      return x == aRect.x && y == aRect.y &&
             width == aRect.width && height == aRect.height;
    }
    // Return true if the rectangles contain the same area of the plane.
    // Use when we do not care about differences in empty rectangles.
    PRBool IsEqualInterior(const gfxRect& aRect) const {
      return IsEqualEdges(aRect) || (IsEmpty() && aRect.IsEmpty());
    }

    void MoveTo(const gfxPoint& aPt) { x = aPt.x; y = aPt.y; }
    const gfxRect& MoveBy(const gfxPoint& aPt) {
        x += aPt.x;
        y += aPt.y;
        return *this;
    }
    void SizeTo(const gfxSize& aSize) { width = aSize.width; height = aSize.height; }

    gfxRect operator+(const gfxPoint& aPt) const {
        return gfxRect(x + aPt.x, y + aPt.y, width, height);
    }
    gfxRect operator-(const gfxPoint& aPt) const {
        return gfxRect(x - aPt.x, y - aPt.y, width, height);
    }
    gfxRect operator+(const gfxSize& aSize) const {
        return gfxRect(x + aSize.width, y + aSize.height, width, height);
    }
    gfxRect operator-(const gfxSize& aSize) const {
        return gfxRect(x - aSize.width, y - aSize.height, width, height);
    }
    gfxRect operator*(const gfxFloat aScale) const {
        return gfxRect(x * aScale, y * aScale, width * aScale, height * aScale);
    }

    const gfxRect& operator+=(const gfxPoint& aPt) {
        x += aPt.x;
        y += aPt.y;
        return *this;
    }
    const gfxRect& operator-=(const gfxPoint& aPt) {
        x -= aPt.x;
        y -= aPt.y;
        return *this;
    }

    gfxFloat Width() const { return width; }
    gfxFloat Height() const { return height; }
    gfxFloat X() const { return x; }
    gfxFloat Y() const { return y; }
    gfxFloat XMost() const { return x + width; }
    gfxFloat YMost() const { return y + height; }

    PRBool IsEmpty() const { return width <= 0 || height <= 0; }
    gfxRect Intersect(const gfxRect& aRect) const;
    gfxRect Union(const gfxRect& aRect) const;
    PRBool Contains(const gfxRect& aRect) const;
    PRBool Contains(const gfxPoint& aPoint) const;

    /**
     * Return true if all components of this rect are within
     * aEpsilon of integer coordinates, defined as
     *   |round(coord) - coord| <= |aEpsilon|
     * for x,y,width,height.
     */
    PRBool WithinEpsilonOfIntegerPixels(gfxFloat aEpsilon) const;

    gfxSize Size() const { return gfxSize(width, height); }

    void Inset(gfxFloat k) {
        x += k;
        y += k;
        width = PR_MAX(0.0, width - k * 2.0);
        height = PR_MAX(0.0, height - k * 2.0);
    }

    void Inset(gfxFloat top, gfxFloat right, gfxFloat bottom, gfxFloat left) {
        x += left;
        y += top;
        width = PR_MAX(0.0, width - (right+left));
        height = PR_MAX(0.0, height - (bottom+top));
    }

    void Inset(const gfxFloat *sides) {
        Inset(sides[0], sides[1], sides[2], sides[3]);
    }

    void Inset(const gfxIntSize& aSize) {
        Inset(aSize.height, aSize.width, aSize.height, aSize.width);
    }

    void Outset(gfxFloat k) {
        x -= k;
        y -= k;
        width = PR_MAX(0.0, width + k * 2.0);
        height = PR_MAX(0.0, height + k * 2.0);
    }

    void Outset(gfxFloat top, gfxFloat right, gfxFloat bottom, gfxFloat left) {
        x -= left;
        y -= top;
        width = PR_MAX(0.0, width + (right+left));
        height = PR_MAX(0.0, height + (bottom+top));
    }

    void Outset(const gfxFloat *sides) {
        Outset(sides[0], sides[1], sides[2], sides[3]);
    }

    void Outset(const gfxIntSize& aSize) {
        Outset(aSize.height, aSize.width, aSize.height, aSize.width);
    }

    // Round the rectangle edges to integer coordinates, such that the rounded
    // rectangle has the same set of pixel centers as the original rectangle.
    // Edges at offset 0.5 round up.
    // Suitable for most places where integral device coordinates
    // are needed, but note that any translation should be applied first to
    // avoid pixel rounding errors.
    // Note that this is *not* rounding to nearest integer if the values are negative.
    // They are always rounding as floor(n + 0.5).
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=410748#c14
    // If you need similar method which is using NS_round(), you should create
    // new |RoundAwayFromZero()| method.
    void Round();

    // Snap the rectangle edges to integer coordinates, such that the
    // original rectangle contains the resulting rectangle.
    void RoundIn();
    
    // Snap the rectangle edges to integer coordinates, such that the
    // resulting rectangle contains the original rectangle.
    void RoundOut();

    // grabbing specific points
    gfxPoint TopLeft() const { return gfxPoint(x, y); }
    gfxPoint TopRight() const { return gfxPoint(x, y) + gfxPoint(width, 0.0); }
    gfxPoint BottomLeft() const { return gfxPoint(x, y) + gfxPoint(0.0, height); }
    gfxPoint BottomRight() const { return gfxPoint(x, y) + gfxPoint(width, height); }
    gfxPoint Center() const { return gfxPoint(x, y) + gfxPoint(width, height)/2.0; }

    gfxPoint AtCorner(mozilla::css::Corner corner) const {
        switch (corner) {
            case NS_CORNER_TOP_LEFT: return TopLeft();
            case NS_CORNER_TOP_RIGHT: return TopRight();
            case NS_CORNER_BOTTOM_RIGHT: return BottomRight();
            case NS_CORNER_BOTTOM_LEFT: return BottomLeft();
            default:
                NS_ERROR("Invalid corner!");
                break;
        }
        return gfxPoint(0.0, 0.0);
    }

    gfxPoint CCWCorner(mozilla::css::Side side) const {
        switch (side) {
            case NS_SIDE_TOP: return TopLeft();
            case NS_SIDE_RIGHT: return TopRight();
            case NS_SIDE_BOTTOM: return BottomRight();
            case NS_SIDE_LEFT: return BottomLeft();
            default:
                NS_ERROR("Invalid side!");
                break;
        }
        return gfxPoint(0.0, 0.0);
    }

    gfxPoint CWCorner(mozilla::css::Side side) const {
        switch (side) {
            case NS_SIDE_TOP: return TopRight();
            case NS_SIDE_RIGHT: return BottomRight();
            case NS_SIDE_BOTTOM: return BottomLeft();
            case NS_SIDE_LEFT: return TopLeft();
            default:
                NS_ERROR("Invalid side!");
                break;
        }
        return gfxPoint(0.0, 0.0);
    }

    /* Conditions this border to Cairo's max coordinate space.
     * The caller can check IsEmpty() after Condition() -- if it's TRUE,
     * the caller can possibly avoid doing any extra rendering.
     */
    void Condition();

    void Scale(gfxFloat k) {
        NS_ASSERTION(k >= 0.0, "Invalid (negative) scale factor");
        x *= k;
        y *= k;
        width *= k;
        height *= k;
    }

    void Scale(gfxFloat sx, gfxFloat sy) {
        NS_ASSERTION(sx >= 0.0, "Invalid (negative) scale factor");
        NS_ASSERTION(sy >= 0.0, "Invalid (negative) scale factor");
        x *= sx;
        y *= sy;
        width *= sx;
        height *= sy;
    }

    void ScaleInverse(gfxFloat k) {
        NS_ASSERTION(k > 0.0, "Invalid (negative) scale factor");
        x /= k;
        y /= k;
        width /= k;
        height /= k;
    }
};

struct THEBES_API gfxCornerSizes {
    gfxSize sizes[NS_NUM_CORNERS];

    gfxCornerSizes () { }

    gfxCornerSizes (gfxFloat v) {
        for (int i = 0; i < NS_NUM_CORNERS; i++)
            sizes[i].SizeTo(v, v);
    }

    gfxCornerSizes (gfxFloat tl, gfxFloat tr, gfxFloat br, gfxFloat bl) {
        sizes[NS_CORNER_TOP_LEFT].SizeTo(tl, tl);
        sizes[NS_CORNER_TOP_RIGHT].SizeTo(tr, tr);
        sizes[NS_CORNER_BOTTOM_RIGHT].SizeTo(br, br);
        sizes[NS_CORNER_BOTTOM_LEFT].SizeTo(bl, bl);
    }

    gfxCornerSizes (const gfxSize& tl, const gfxSize& tr, const gfxSize& br, const gfxSize& bl) {
        sizes[NS_CORNER_TOP_LEFT] = tl;
        sizes[NS_CORNER_TOP_RIGHT] = tr;
        sizes[NS_CORNER_BOTTOM_RIGHT] = br;
        sizes[NS_CORNER_BOTTOM_LEFT] = bl;
    }

    const gfxSize& operator[] (mozilla::css::Corner index) const {
        return sizes[index];
    }

    gfxSize& operator[] (mozilla::css::Corner index) {
        return sizes[index];
    }

    const gfxSize TopLeft() const { return sizes[NS_CORNER_TOP_LEFT]; }
    gfxSize& TopLeft() { return sizes[NS_CORNER_TOP_LEFT]; }

    const gfxSize TopRight() const { return sizes[NS_CORNER_TOP_RIGHT]; }
    gfxSize& TopRight() { return sizes[NS_CORNER_TOP_RIGHT]; }

    const gfxSize BottomLeft() const { return sizes[NS_CORNER_BOTTOM_LEFT]; }
    gfxSize& BottomLeft() { return sizes[NS_CORNER_BOTTOM_LEFT]; }

    const gfxSize BottomRight() const { return sizes[NS_CORNER_BOTTOM_RIGHT]; }
    gfxSize& BottomRight() { return sizes[NS_CORNER_BOTTOM_RIGHT]; }
};
#endif /* GFX_RECT_H */
