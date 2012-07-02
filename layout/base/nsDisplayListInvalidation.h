/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSDISPLAYLISTINVALIDATION_H_
#define NSDISPLAYLISTINVALIDATION_H_

/**
 * This stores the geometry of an nsDisplayItem, and the area
 * that will be affected when painting the item.
 *
 * It is used to retain information about display items so they
 * can be compared against new display items in the next paint.
 */
class nsDisplayItemGeometry
{
public:
  nsDisplayItemGeometry()
  {
    MOZ_COUNT_CTOR(nsDisplayItemGeometry);
  }
  virtual ~nsDisplayItemGeometry()
  {
    MOZ_COUNT_DTOR(nsDisplayItemGeometry);
  }
  /**
   * Compute the area required to be invalidated if this
   * display item is removed.
   */
  const nsRect& ComputeInvalidationRegion() { return mBounds; }
  
  /**
   * Shifts all retained areas of the nsDisplayItemGeometry by the given offset.
   * 
   * This is used to compensate for scrolling, since the destination buffer
   * can scroll without requiring a full repaint.
   *
   * @param aOffset Offset to shift by.
   */
  virtual void MoveBy(const nsPoint& aOffset) = 0;

  /**
   * The appunits per dev pixel for the item's frame.
   */
  nscoord mAppUnitsPerDevPixel;

  /**
   * The offset (in pixels) of the TopLeft() of the ThebesLayer
   * this display item was drawn into.
   */
  nsIntPoint mPaintOffset;

  gfxPoint mActiveScrolledRootPosition;
  
  /**
   * Bounds of the display item
   */
  nsRect mBounds;
};

/**
 * A default geometry implementation, used by nsDisplayItem. Retains
 * and compares the bounds, and border rect.
 *
 * This should be sufficient for the majority of display items.
 */
class nsDisplayItemGenericGeometry : public nsDisplayItemGeometry
{
public:
  virtual void MoveBy(const nsPoint& aOffset)
  {
    mBounds.MoveBy(aOffset);
    mBorderRect.MoveBy(aOffset);
  }

  nsRect mBorderRect;
};

class nsDisplayBorderGeometry : public nsDisplayItemGeometry
{
public:
  virtual void MoveBy(const nsPoint& aOffset)
  {
    mBounds.MoveBy(aOffset);
    mPaddingRect.MoveBy(aOffset);
  }

  nsRect mPaddingRect;
};

class nsDisplayBackgroundGeometry : public nsDisplayItemGeometry
{
public:
  virtual void MoveBy(const nsPoint& aOffset)
  {
    mBounds.MoveBy(aOffset);
    mPaddingRect.MoveBy(aOffset);
    mContentRect.MoveBy(aOffset);
  }

  nsRect mPaddingRect;
  nsRect mContentRect;
};

class nsDisplayBoxShadowInnerGeometry : public nsDisplayItemGeometry
{
public:
  virtual void MoveBy(const nsPoint& aOffset)
  {
    mBounds.MoveBy(aOffset);
    mPaddingRect.MoveBy(aOffset);
  }

  nsRect mPaddingRect;
};

#endif /*NSDISPLAYLISTINVALIDATION_H_*/
