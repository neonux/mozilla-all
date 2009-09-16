/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Takeshi Ichimaru <ayakawa.m@gmail.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Rob Arnold <robarnold@mozilla.com>
 *   Jeff Walden <jwalden+code@mit.edu>
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

/* utility functions for drawing borders and backgrounds */

#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIFrame.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsIViewManager.h"
#include "nsIPresShell.h"
#include "nsFrameManager.h"
#include "nsStyleContext.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsTransform2D.h"
#include "nsIDeviceContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIScrollableFrame.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsCSSRendering.h"
#include "nsCSSColorUtils.h"
#include "nsITheme.h"
#include "nsThemeConstants.h"
#include "nsIServiceManager.h"
#include "nsIHTMLDocument.h"
#include "nsLayoutUtils.h"
#include "nsINameSpaceManager.h"
#include "nsBlockFrame.h"
#include "gfxContext.h"
#include "nsIInterfaceRequestorUtils.h"
#include "gfxPlatform.h"
#include "gfxImageSurface.h"
#include "nsStyleStructInlines.h"
#include "nsCSSFrameConstructor.h"

#include "nsCSSRenderingBorders.h"

/**
 * This is a small wrapper class to encapsulate image drawing that can draw an
 * nsStyleImage image, which may internally be a real image, a sub image, or a
 * CSS gradient.
 *
 * @note Always call the member functions in the order of PrepareImage(),
 * ComputeSize(), and Draw().
 */
class ImageRenderer {
public:
  enum {
    FLAG_SYNC_DECODE_IMAGES = 0x01
  };
  ImageRenderer(nsIFrame* aForFrame, const nsStyleImage& aImage, PRUint32 aFlags);
  ~ImageRenderer();
  /**
   * Populates member variables to get ready for rendering.
   * @return PR_TRUE iff the image is ready, and there is at least a pixel to
   * draw.
   */
  PRBool PrepareImage();
  /**
   * @return the image size in appunits. CSS gradient images don't have an
   * intrinsic size so we have to pass in a default that they will use.
   */
  nsSize ComputeSize(const nsSize& aDefault);
  /**
   * Draws the image to the target rendering context.
   * @param aRepeat indicates whether the image is to be repeated (tiled)
   * @see nsLayoutUtils::DrawImage() for other parameters
   */
  void Draw(nsPresContext*       aPresContext,
            nsIRenderingContext& aRenderingContext,
            const nsRect&        aDest,
            const nsRect&        aFill,
            const nsPoint&       aAnchor,
            const nsRect&        aDirty,
            PRBool               aRepeat);

private:
  nsIFrame*                 mForFrame;
  nsStyleImage              mImage;
  nsStyleImageType          mType;
  nsCOMPtr<imgIContainer>   mImageContainer;
  nsRefPtr<nsStyleGradient> mGradientData;
  PRBool                    mIsReady;
  nsSize                    mSize;
  PRUint32                  mFlags;
};

// To avoid storing this data on nsInlineFrame (bloat) and to avoid
// recalculating this for each frame in a continuation (perf), hold
// a cache of various coordinate information that we need in order
// to paint inline backgrounds.
struct InlineBackgroundData
{
  InlineBackgroundData()
      : mFrame(nsnull), mBlockFrame(nsnull)
  {
  }

  ~InlineBackgroundData()
  {
  }

  void Reset()
  {
    mBoundingBox.SetRect(0,0,0,0);
    mContinuationPoint = mLineContinuationPoint = mUnbrokenWidth = 0;
    mFrame = mBlockFrame = nsnull;
  }

  nsRect GetContinuousRect(nsIFrame* aFrame)
  {
    SetFrame(aFrame);

    nscoord x;
    if (mBidiEnabled) {
      x = mLineContinuationPoint;

      // Scan continuations on the same line as aFrame and accumulate the widths
      // of frames that are to the left (if this is an LTR block) or right 
      // (if it's RTL) of the current one.
      PRBool isRtlBlock = (mBlockFrame->GetStyleVisibility()->mDirection ==
                           NS_STYLE_DIRECTION_RTL);      
      nscoord curOffset = aFrame->GetOffsetTo(mBlockFrame).x;

      // No need to use our GetPrevContinuation/GetNextContinuation methods
      // here, since ib special siblings are certainly not on the same line.
      
      nsIFrame* inlineFrame = aFrame->GetPrevContinuation();
      // If the continuation is fluid we know inlineFrame is not on the same line.
      // If it's not fluid, we need to test further to be sure.
      while (inlineFrame && !inlineFrame->GetNextInFlow() &&
             AreOnSameLine(aFrame, inlineFrame)) {
        nscoord frameXOffset = inlineFrame->GetOffsetTo(mBlockFrame).x;
        if(isRtlBlock == (frameXOffset >= curOffset)) {
          x += inlineFrame->GetSize().width;
        }
        inlineFrame = inlineFrame->GetPrevContinuation();
      }

      inlineFrame = aFrame->GetNextContinuation();
      while (inlineFrame && !inlineFrame->GetPrevInFlow() &&
             AreOnSameLine(aFrame, inlineFrame)) {
        nscoord frameXOffset = inlineFrame->GetOffsetTo(mBlockFrame).x;
        if(isRtlBlock == (frameXOffset >= curOffset)) {
          x += inlineFrame->GetSize().width;
        }
        inlineFrame = inlineFrame->GetNextContinuation();
      }
      if (isRtlBlock) {
        // aFrame itself is also to the right of its left edge, so add its width.
        x += aFrame->GetSize().width;
        // x is now the distance from the left edge of aFrame to the right edge
        // of the unbroken content. Change it to indicate the distance from the
        // left edge of the unbroken content to the left edge of aFrame.
        x = mUnbrokenWidth - x;
      }
    } else {
      x = mContinuationPoint;
    }

    // Assume background-origin: border and return a rect with offsets
    // relative to (0,0).  If we have a different background-origin,
    // then our rect should be deflated appropriately by our caller.
    return nsRect(-x, 0, mUnbrokenWidth, mFrame->GetSize().height);
  }

  nsRect GetBoundingRect(nsIFrame* aFrame)
  {
    SetFrame(aFrame);

    // Move the offsets relative to (0,0) which puts the bounding box into
    // our coordinate system rather than our parent's.  We do this by
    // moving it the back distance from us to the bounding box.
    // This also assumes background-origin: border, so our caller will
    // need to deflate us if needed.
    nsRect boundingBox(mBoundingBox);
    nsPoint point = mFrame->GetPosition();
    boundingBox.MoveBy(-point.x, -point.y);

    return boundingBox;
  }

protected:
  nsIFrame*     mFrame;
  nsBlockFrame* mBlockFrame;
  nsRect        mBoundingBox;
  nscoord       mContinuationPoint;
  nscoord       mUnbrokenWidth;
  nscoord       mLineContinuationPoint;
  PRBool        mBidiEnabled;
  
  void SetFrame(nsIFrame* aFrame)
  {
    NS_PRECONDITION(aFrame, "Need a frame");

    nsIFrame *prevContinuation = GetPrevContinuation(aFrame);

    if (!prevContinuation || mFrame != prevContinuation) {
      // Ok, we've got the wrong frame.  We have to start from scratch.
      Reset();
      Init(aFrame);
      return;
    }

    // Get our last frame's size and add its width to our continuation
    // point before we cache the new frame.
    mContinuationPoint += mFrame->GetSize().width;

    // If this a new line, update mLineContinuationPoint.
    if (mBidiEnabled &&
        (aFrame->GetPrevInFlow() || !AreOnSameLine(mFrame, aFrame))) {
       mLineContinuationPoint = mContinuationPoint;
    }
    
    mFrame = aFrame;
  }

  nsIFrame* GetPrevContinuation(nsIFrame* aFrame)
  {
    nsIFrame* prevCont = aFrame->GetPrevContinuation();
    if (!prevCont && (aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL)) {
      nsIFrame* block =
        static_cast<nsIFrame*>
                   (aFrame->GetProperty(nsGkAtoms::IBSplitSpecialPrevSibling));
      if (block) {
        // The {ib} properties are only stored on first continuations
        block = block->GetFirstContinuation();
        prevCont =
          static_cast<nsIFrame*>
                     (block->GetProperty(nsGkAtoms::IBSplitSpecialPrevSibling));
        NS_ASSERTION(prevCont, "How did that happen?");
      }
    }
    return prevCont;
  }

  nsIFrame* GetNextContinuation(nsIFrame* aFrame)
  {
    nsIFrame* nextCont = aFrame->GetNextContinuation();
    if (!nextCont && (aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL)) {
      // The {ib} properties are only stored on first continuations
      aFrame = aFrame->GetFirstContinuation();
      nsIFrame* block =
        static_cast<nsIFrame*>
                   (aFrame->GetProperty(nsGkAtoms::IBSplitSpecialSibling));
      if (block) {
        nextCont =
          static_cast<nsIFrame*>
                     (block->GetProperty(nsGkAtoms::IBSplitSpecialSibling));
        NS_ASSERTION(nextCont, "How did that happen?");
      }
    }
    return nextCont;
  }

  void Init(nsIFrame* aFrame)
  {    
    // Start with the previous flow frame as our continuation point
    // is the total of the widths of the previous frames.
    nsIFrame* inlineFrame = GetPrevContinuation(aFrame);

    while (inlineFrame) {
      nsRect rect = inlineFrame->GetRect();
      mContinuationPoint += rect.width;
      mUnbrokenWidth += rect.width;
      mBoundingBox.UnionRect(mBoundingBox, rect);
      inlineFrame = GetPrevContinuation(inlineFrame);
    }

    // Next add this frame and subsequent frames to the bounding box and
    // unbroken width.
    inlineFrame = aFrame;
    while (inlineFrame) {
      nsRect rect = inlineFrame->GetRect();
      mUnbrokenWidth += rect.width;
      mBoundingBox.UnionRect(mBoundingBox, rect);
      inlineFrame = GetNextContinuation(inlineFrame);
    }

    mFrame = aFrame;

    mBidiEnabled = aFrame->PresContext()->BidiEnabled();
    if (mBidiEnabled) {
      // Find the containing block frame
      nsIFrame* frame = aFrame;
      do {
        frame = frame->GetParent();
        mBlockFrame = do_QueryFrame(frame);
      }
      while (frame && frame->IsFrameOfType(nsIFrame::eLineParticipant));

      NS_ASSERTION(mBlockFrame, "Cannot find containing block.");

      mLineContinuationPoint = mContinuationPoint;
    }
  }
  
  PRBool AreOnSameLine(nsIFrame* aFrame1, nsIFrame* aFrame2) {
    // Assumes that aFrame1 and aFrame2 are both decsendants of mBlockFrame.
    PRBool isValid1, isValid2;
    nsBlockInFlowLineIterator it1(mBlockFrame, aFrame1, &isValid1);
    nsBlockInFlowLineIterator it2(mBlockFrame, aFrame2, &isValid2);
    return isValid1 && isValid2 && it1.GetLine() == it2.GetLine();
  }
};

/* Local functions */
static void PaintBackgroundLayer(nsPresContext* aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsIFrame* aForFrame,
                                 PRUint32 aFlags,
                                 const nsRect& aDirtyRect,
                                 const nsRect& aBorderArea,
                                 const nsRect& aBGClipRect,
                                 const nsStyleBackground& aBackground,
                                 const nsStyleBackground::Layer& aLayer);

static void DrawBorderImage(nsPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            nsIFrame* aForFrame,
                            const nsRect& aBorderArea,
                            const nsStyleBorder& aBorderStyle,
                            const nsRect& aDirtyRect);

static void DrawBorderImageComponent(nsIRenderingContext& aRenderingContext,
                                     nsIFrame* aForFrame,
                                     imgIContainer* aImage,
                                     const nsRect& aDirtyRect,
                                     const nsRect& aFill,
                                     const nsIntRect& aSrc,
                                     PRUint8 aHFill,
                                     PRUint8 aVFill,
                                     const nsSize& aUnitSize);

static nscolor MakeBevelColor(PRIntn whichSide, PRUint8 style,
                              nscolor aBackgroundColor,
                              nscolor aBorderColor);

static InlineBackgroundData* gInlineBGData = nsnull;

// Initialize any static variables used by nsCSSRendering.
nsresult nsCSSRendering::Init()
{  
  NS_ASSERTION(!gInlineBGData, "Init called twice");
  gInlineBGData = new InlineBackgroundData();
  if (!gInlineBGData)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

// Clean up any global variables used by nsCSSRendering.
void nsCSSRendering::Shutdown()
{
  delete gInlineBGData;
  gInlineBGData = nsnull;
}

/**
 * Make a bevel color
 */
static nscolor
MakeBevelColor(PRIntn whichSide, PRUint8 style,
               nscolor aBackgroundColor, nscolor aBorderColor)
{

  nscolor colors[2];
  nscolor theColor;

  // Given a background color and a border color
  // calculate the color used for the shading
  NS_GetSpecial3DColors(colors, aBackgroundColor, aBorderColor);
 
  if ((style == NS_STYLE_BORDER_STYLE_OUTSET) ||
      (style == NS_STYLE_BORDER_STYLE_RIDGE)) {
    // Flip colors for these two border styles
    switch (whichSide) {
    case NS_SIDE_BOTTOM: whichSide = NS_SIDE_TOP;    break;
    case NS_SIDE_RIGHT:  whichSide = NS_SIDE_LEFT;   break;
    case NS_SIDE_TOP:    whichSide = NS_SIDE_BOTTOM; break;
    case NS_SIDE_LEFT:   whichSide = NS_SIDE_RIGHT;  break;
    }
  }

  switch (whichSide) {
  case NS_SIDE_BOTTOM:
    theColor = colors[1];
    break;
  case NS_SIDE_RIGHT:
    theColor = colors[1];
    break;
  case NS_SIDE_TOP:
    theColor = colors[0];
    break;
  case NS_SIDE_LEFT:
  default:
    theColor = colors[0];
    break;
  }
  return theColor;
}

//----------------------------------------------------------------------
// Thebes Border Rendering Code Start

// helper function to convert a nsRect to a gfxRect
static gfxRect
RectToGfxRect(const nsRect& rect, nscoord twipsPerPixel)
{
  return gfxRect(gfxFloat(rect.x) / twipsPerPixel,
                 gfxFloat(rect.y) / twipsPerPixel,
                 gfxFloat(rect.width) / twipsPerPixel,
                 gfxFloat(rect.height) / twipsPerPixel);
}

/*
 * Compute the float-pixel radii that should be used for drawing
 * this border/outline, given the various input bits.
 *
 * If a side is skipped via skipSides, its corners are forced to 0.
 * All corner radii are then adjusted so they do not require more
 * space than outerRect, according to the algorithm in css3-background.
 */
static void
ComputePixelRadii(const nscoord *aTwipsRadii,
                  const nsRect& outerRect,
                  PRIntn skipSides,
                  nscoord twipsPerPixel,
                  gfxCornerSizes *oBorderRadii)
{
  nscoord twipsRadii[8];
  memcpy(twipsRadii, aTwipsRadii, sizeof twipsRadii);

  if (skipSides & SIDE_BIT_TOP) {
    twipsRadii[NS_CORNER_TOP_LEFT_X] = 0;
    twipsRadii[NS_CORNER_TOP_LEFT_Y] = 0;
    twipsRadii[NS_CORNER_TOP_RIGHT_X] = 0;
    twipsRadii[NS_CORNER_TOP_RIGHT_Y] = 0;
  }

  if (skipSides & SIDE_BIT_RIGHT) {
    twipsRadii[NS_CORNER_TOP_RIGHT_X] = 0;
    twipsRadii[NS_CORNER_TOP_RIGHT_Y] = 0;
    twipsRadii[NS_CORNER_BOTTOM_RIGHT_X] = 0;
    twipsRadii[NS_CORNER_BOTTOM_RIGHT_Y] = 0;
  }

  if (skipSides & SIDE_BIT_BOTTOM) {
    twipsRadii[NS_CORNER_BOTTOM_RIGHT_X] = 0;
    twipsRadii[NS_CORNER_BOTTOM_RIGHT_Y] = 0;
    twipsRadii[NS_CORNER_BOTTOM_LEFT_X] = 0;
    twipsRadii[NS_CORNER_BOTTOM_LEFT_Y] = 0;
  }

  if (skipSides & SIDE_BIT_LEFT) {
    twipsRadii[NS_CORNER_BOTTOM_LEFT_X] = 0;
    twipsRadii[NS_CORNER_BOTTOM_LEFT_Y] = 0;
    twipsRadii[NS_CORNER_TOP_LEFT_X] = 0;
    twipsRadii[NS_CORNER_TOP_LEFT_Y] = 0;
  }

  gfxFloat radii[8];
  NS_FOR_CSS_HALF_CORNERS(corner)
    radii[corner] = twipsRadii[corner] / twipsPerPixel;

  // css3-background specifies this algorithm for reducing
  // corner radii when they are too big.
  gfxFloat maxWidth = outerRect.width / twipsPerPixel;
  gfxFloat maxHeight = outerRect.height / twipsPerPixel;
  gfxFloat f = 1.0f;
  NS_FOR_CSS_SIDES(side) {
    PRUint32 hc1 = NS_SIDE_TO_HALF_CORNER(side, PR_FALSE, PR_TRUE);
    PRUint32 hc2 = NS_SIDE_TO_HALF_CORNER(side, PR_TRUE, PR_TRUE);
    gfxFloat length = NS_SIDE_IS_VERTICAL(side) ? maxHeight : maxWidth;
    gfxFloat sum = radii[hc1] + radii[hc2];
    // avoid floating point division in the normal case
    if (length < sum)
      f = NS_MIN(f, length/sum);
  }
  if (f < 1.0) {
    NS_FOR_CSS_HALF_CORNERS(corner) {
      radii[corner] *= f;
    }
  }

  (*oBorderRadii)[C_TL] = gfxSize(radii[NS_CORNER_TOP_LEFT_X],
                                  radii[NS_CORNER_TOP_LEFT_Y]);
  (*oBorderRadii)[C_TR] = gfxSize(radii[NS_CORNER_TOP_RIGHT_X],
                                  radii[NS_CORNER_TOP_RIGHT_Y]);
  (*oBorderRadii)[C_BR] = gfxSize(radii[NS_CORNER_BOTTOM_RIGHT_X],
                                  radii[NS_CORNER_BOTTOM_RIGHT_Y]);
  (*oBorderRadii)[C_BL] = gfxSize(radii[NS_CORNER_BOTTOM_LEFT_X],
                                  radii[NS_CORNER_BOTTOM_LEFT_Y]);
}

void
nsCSSRendering::PaintBorder(nsPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            nsIFrame* aForFrame,
                            const nsRect& aDirtyRect,
                            const nsRect& aBorderArea,
                            const nsStyleBorder& aBorderStyle,
                            nsStyleContext* aStyleContext,
                            PRIntn aSkipSides)
{
  nsMargin            border;
  nscoord             twipsRadii[8];
  nsCompatibility     compatMode = aPresContext->CompatibilityMode();

  SN("++ PaintBorder");

  // Check to see if we have an appearance defined.  If so, we let the theme
  // renderer draw the border.  DO not get the data from aForFrame, since the passed in style context
  // may be different!  Always use |aStyleContext|!
  const nsStyleDisplay* displayData = aStyleContext->GetStyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, aForFrame, displayData->mAppearance))
      return; // Let the theme handle it.
  }

  if (aBorderStyle.IsBorderImageLoaded()) {
    DrawBorderImage(aPresContext, aRenderingContext, aForFrame,
                    aBorderArea, aBorderStyle, aDirtyRect);
    return;
  }
  
  // Get our style context's color struct.
  const nsStyleColor* ourColor = aStyleContext->GetStyleColor();

  // in NavQuirks mode we want to use the parent's context as a starting point
  // for determining the background color
  nsStyleContext* bgContext = nsCSSRendering::FindNonTransparentBackground
    (aStyleContext, compatMode == eCompatibility_NavQuirks ? PR_TRUE : PR_FALSE);
  const nsStyleBackground* bgColor = bgContext->GetStyleBackground();

  border = aBorderStyle.GetComputedBorder();
  if ((0 == border.left) && (0 == border.right) &&
      (0 == border.top) && (0 == border.bottom)) {
    // Empty border area
    return;
  }

  GetBorderRadiusTwips(aBorderStyle.mBorderRadius, aForFrame->GetSize().width,
                       twipsRadii);

  // Turn off rendering for all of the zero sized sides
  if (aSkipSides & SIDE_BIT_TOP) border.top = 0;
  if (aSkipSides & SIDE_BIT_RIGHT) border.right = 0;
  if (aSkipSides & SIDE_BIT_BOTTOM) border.bottom = 0;
  if (aSkipSides & SIDE_BIT_LEFT) border.left = 0;

  // get the inside and outside parts of the border
  nsRect outerRect(aBorderArea);

  SF(" outerRect: %d %d %d %d\n", outerRect.x, outerRect.y, outerRect.width, outerRect.height);

  // we can assume that we're already clipped to aDirtyRect -- I think? (!?)

  // Get our conversion values
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  // convert outer and inner rects
  gfxRect oRect(RectToGfxRect(outerRect, twipsPerPixel));

  // convert the border widths
  gfxFloat borderWidths[4] = { border.top / twipsPerPixel,
                               border.right / twipsPerPixel,
                               border.bottom / twipsPerPixel,
                               border.left / twipsPerPixel };

  // convert the radii
  gfxCornerSizes borderRadii;
  ComputePixelRadii(twipsRadii, outerRect, aSkipSides, twipsPerPixel,
                    &borderRadii);

  PRUint8 borderStyles[4];
  nscolor borderColors[4];
  nsBorderColors *compositeColors[4];

  // pull out styles, colors, composite colors
  NS_FOR_CSS_SIDES (i) {
    PRBool foreground;
    borderStyles[i] = aBorderStyle.GetBorderStyle(i);
    aBorderStyle.GetBorderColor(i, borderColors[i], foreground);
    aBorderStyle.GetCompositeColors(i, &compositeColors[i]);

    if (foreground)
      borderColors[i] = ourColor->mColor;
  }

  SF(" borderStyles: %d %d %d %d\n", borderStyles[0], borderStyles[1], borderStyles[2], borderStyles[3]);

  // start drawing
  gfxContext *ctx = aRenderingContext.ThebesContext();

  ctx->Save();

#if 0
  // this will draw a transparent red backround underneath the oRect area
  ctx->Save();
  ctx->Rectangle(oRect);
  ctx->SetColor(gfxRGBA(1.0, 0.0, 0.0, 0.5));
  ctx->Fill();
  ctx->Restore();
#endif

  //SF ("borderRadii: %f %f %f %f\n", borderRadii[0], borderRadii[1], borderRadii[2], borderRadii[3]);

  nsCSSBorderRenderer br(twipsPerPixel,
                         ctx,
                         oRect,
                         borderStyles,
                         borderWidths,
                         borderRadii,
                         borderColors,
                         compositeColors,
                         aSkipSides,
                         bgColor->mBackgroundColor);
  br.DrawBorders();

  ctx->Restore();

  SN();
}

static nsRect
GetOutlineInnerRect(nsIFrame* aFrame)
{
  nsRect* savedOutlineInnerRect = static_cast<nsRect*>
    (aFrame->GetProperty(nsGkAtoms::outlineInnerRectProperty));
  if (savedOutlineInnerRect)
    return *savedOutlineInnerRect;
  return aFrame->GetOverflowRect();
}

void
nsCSSRendering::PaintOutline(nsPresContext* aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             nsIFrame* aForFrame,
                             const nsRect& aDirtyRect,
                             const nsRect& aBorderArea,
                             const nsStyleBorder& aBorderStyle,
                             const nsStyleOutline& aOutlineStyle,
                             nsStyleContext* aStyleContext)
{
  nscoord             twipsRadii[8];

  // Get our style context's color struct.
  const nsStyleColor* ourColor = aStyleContext->GetStyleColor();

  nscoord width;
  aOutlineStyle.GetOutlineWidth(width);

  if (width == 0) {
    // Empty outline
    return;
  }

  nsStyleContext* bgContext = nsCSSRendering::FindNonTransparentBackground
    (aStyleContext, PR_FALSE);
  const nsStyleBackground* bgColor = bgContext->GetStyleBackground();

  // get the radius for our outline
  GetBorderRadiusTwips(aOutlineStyle.mOutlineRadius, aBorderArea.width,
                       twipsRadii);

  // When the outline property is set on :-moz-anonymous-block or
  // :-moz-anonyomus-positioned-block pseudo-elements, it inherited that
  // outline from the inline that was broken because it contained a
  // block.  In that case, we don't want a really wide outline if the
  // block inside the inline is narrow, so union the actual contents of
  // the anonymous blocks.
  nsIFrame *frameForArea = aForFrame;
  do {
    nsIAtom *pseudoType = frameForArea->GetStyleContext()->GetPseudoType();
    if (pseudoType != nsCSSAnonBoxes::mozAnonymousBlock &&
        pseudoType != nsCSSAnonBoxes::mozAnonymousPositionedBlock)
      break;
    // If we're done, we really want it and all its later siblings.
    frameForArea = frameForArea->GetFirstChild(nsnull);
    NS_ASSERTION(frameForArea, "anonymous block with no children?");
  } while (frameForArea);
  nsRect innerRect; // relative to aBorderArea.TopLeft()
  if (frameForArea == aForFrame) {
    innerRect = GetOutlineInnerRect(aForFrame);
  } else {
    for (; frameForArea; frameForArea = frameForArea->GetNextSibling()) {
      // The outline has already been included in aForFrame's overflow
      // area, but not in those of its descendants, so we have to
      // include it.  Otherwise we'll end up drawing the outline inside
      // the border.
      nsRect r(GetOutlineInnerRect(frameForArea) +
               frameForArea->GetOffsetTo(aForFrame));
      innerRect.UnionRect(innerRect, r);
    }
  }

  innerRect += aBorderArea.TopLeft();
  nscoord offset = aOutlineStyle.mOutlineOffset;
  innerRect.Inflate(offset, offset);
  // If the dirty rect is completely inside the border area (e.g., only the
  // content is being painted), then we can skip out now
  // XXX this isn't exactly true for rounded borders, where the inside curves may
  // encroach into the content area.  A safer calculation would be to
  // shorten insideRect by the radius one each side before performing this test.
  if (innerRect.Contains(aDirtyRect))
    return;

  nsRect outerRect = innerRect;
  outerRect.Inflate(width, width);

  // Get our conversion values
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  // get the outer rectangles
  gfxRect oRect(RectToGfxRect(outerRect, twipsPerPixel));

  // convert the radii
  nsMargin outlineMargin(width, width, width, width);
  gfxCornerSizes outlineRadii;
  ComputePixelRadii(twipsRadii, outerRect, 0, twipsPerPixel,
                    &outlineRadii);

  PRUint8 outlineStyle = aOutlineStyle.GetOutlineStyle();
  PRUint8 outlineStyles[4] = { outlineStyle,
                               outlineStyle,
                               outlineStyle,
                               outlineStyle };

  nscolor outlineColor;
  // PR_FALSE means use the initial color; PR_TRUE means a color was
  // set.
  if (!aOutlineStyle.GetOutlineColor(outlineColor))
    outlineColor = ourColor->mColor;
  nscolor outlineColors[4] = { outlineColor,
                               outlineColor,
                               outlineColor,
                               outlineColor };

  // convert the border widths
  gfxFloat outlineWidths[4] = { width / twipsPerPixel,
                                width / twipsPerPixel,
                                width / twipsPerPixel,
                                width / twipsPerPixel };

  // start drawing
  gfxContext *ctx = aRenderingContext.ThebesContext();

  ctx->Save();

  nsCSSBorderRenderer br(twipsPerPixel,
                         ctx,
                         oRect,
                         outlineStyles,
                         outlineWidths,
                         outlineRadii,
                         outlineColors,
                         nsnull, 0,
                         bgColor->mBackgroundColor);
  br.DrawBorders();

  ctx->Restore();

  SN();
}

void
nsCSSRendering::PaintFocus(nsPresContext* aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect& aFocusRect,
                           nscolor aColor)
{
  nscoord oneCSSPixel = nsPresContext::CSSPixelsToAppUnits(1);
  nscoord oneDevPixel = aPresContext->DevPixelsToAppUnits(1);

  gfxRect focusRect(RectToGfxRect(aFocusRect, oneDevPixel));

  gfxCornerSizes focusRadii;
  {
    nscoord twipsRadii[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    ComputePixelRadii(twipsRadii, aFocusRect, 0, oneDevPixel, &focusRadii);
  }
  gfxFloat focusWidths[4] = { oneCSSPixel / oneDevPixel,
                              oneCSSPixel / oneDevPixel,
                              oneCSSPixel / oneDevPixel,
                              oneCSSPixel / oneDevPixel };

  PRUint8 focusStyles[4] = { NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED };
  nscolor focusColors[4] = { aColor, aColor, aColor, aColor };

  gfxContext *ctx = aRenderingContext.ThebesContext();

  ctx->Save();

  // Because this renders a dotted border, the background color
  // should not be used.  Therefore, we provide a value that will
  // be blatantly wrong if it ever does get used.  (If this becomes
  // something that CSS can style, this function will then have access
  // to a style context and can use the same logic that PaintBorder
  // and PaintOutline do.)
  nsCSSBorderRenderer br(oneDevPixel,
                         ctx,
                         focusRect,
                         focusStyles,
                         focusWidths,
                         focusRadii,
                         focusColors,
                         nsnull, 0,
                         NS_RGB(255, 0, 0));
  br.DrawBorders();

  ctx->Restore();

  SN();
}

// Thebes Border Rendering Code End
//----------------------------------------------------------------------


//----------------------------------------------------------------------

/**
 * Computes the placement of a background image.
 *
 * @param aOriginBounds is the box to which the tiling position should be
 * relative
 * This should correspond to 'background-origin' for the frame,
 * except when painting on the canvas, in which case the origin bounds
 * should be the bounds of the root element's frame.
 * @param aTopLeft the top-left corner where an image tile should be drawn
 * @param aAnchorPoint a point which should be pixel-aligned by
 * nsLayoutUtils::DrawImage. This is the same as aTopLeft, unless CSS
 * specifies a percentage (including 'right' or 'bottom'), in which case
 * it's that percentage within of aOriginBounds. So 'right' would set
 * aAnchorPoint.x to aOriginBounds.XMost().
 * 
 * Points are returned relative to aOriginBounds.
 */
static void
ComputeBackgroundAnchorPoint(const nsStyleBackground::Layer& aLayer,
                             const nsSize& aOriginBounds,
                             const nsSize& aImageSize,
                             nsPoint* aTopLeft,
                             nsPoint* aAnchorPoint)
{
  if (!aLayer.mPosition.mXIsPercent) {
    aTopLeft->x = aAnchorPoint->x = aLayer.mPosition.mXPosition.mCoord;
  }
  else {
    double percent = aLayer.mPosition.mXPosition.mFloat;
    aAnchorPoint->x = NSToCoordRound(percent*aOriginBounds.width);
    aTopLeft->x = NSToCoordRound(percent*(aOriginBounds.width - aImageSize.width));
  }

  if (!aLayer.mPosition.mYIsPercent) {
    aTopLeft->y = aAnchorPoint->y = aLayer.mPosition.mYPosition.mCoord;
  }
  else {
    double percent = aLayer.mPosition.mYPosition.mFloat;
    aAnchorPoint->y = NSToCoordRound(percent*aOriginBounds.height);
    aTopLeft->y = NSToCoordRound(percent*(aOriginBounds.height - aImageSize.height));
  }
}

nsStyleContext*
nsCSSRendering::FindNonTransparentBackground(nsStyleContext* aContext,
                                             PRBool aStartAtParent /*= PR_FALSE*/)
{
  NS_ASSERTION(aContext, "Cannot find NonTransparentBackground in a null context" );
  
  nsStyleContext* context = nsnull;
  if (aStartAtParent) {
    context = aContext->GetParent();
  }
  if (!context) {
    context = aContext;
  }
  
  while (context) {
    const nsStyleBackground* bg = context->GetStyleBackground();
    if (NS_GET_A(bg->mBackgroundColor) > 0)
      break;

    const nsStyleDisplay* display = context->GetStyleDisplay();
    if (display->mAppearance)
      break;

    nsStyleContext* parent = context->GetParent();
    if (!parent)
      break;

    context = parent;
  }
  return context;
}

// Returns true if aFrame is a canvas frame.
// We need to treat the viewport as canvas because, even though
// it does not actually paint a background, we need to get the right
// background style so we correctly detect transparent documents.
PRBool
nsCSSRendering::IsCanvasFrame(nsIFrame* aFrame)
{
  nsIAtom* frameType = aFrame->GetType();
  return frameType == nsGkAtoms::canvasFrame ||
         frameType == nsGkAtoms::rootFrame ||
         frameType == nsGkAtoms::pageFrame ||
         frameType == nsGkAtoms::pageContentFrame ||
         frameType == nsGkAtoms::viewportFrame;
}

nsIFrame*
nsCSSRendering::FindRootFrame(nsIFrame* aForFrame)
{
  const nsStyleBackground* result = aForFrame->GetStyleBackground();

  // Check if we need to do propagation from BODY rather than HTML.
  if (result->IsTransparent()) {
    nsIContent* content = aForFrame->GetContent();
    // The root element content can't be null. We wouldn't know what
    // frame to create for aFrame.
    // Use |GetOwnerDoc| so it works during destruction.
    if (content) {
      nsIDocument* document = content->GetOwnerDoc();
      nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document);
      if (htmlDoc) {
        nsIContent* bodyContent = htmlDoc->GetBodyContentExternal();
        // We need to null check the body node (bug 118829) since
        // there are cases, thanks to the fix for bug 5569, where we
        // will reflow a document with no body.  In particular, if a
        // SCRIPT element in the head blocks the parser and then has a
        // SCRIPT that does "document.location.href = 'foo'", then
        // nsParser::Terminate will call |DidBuildModel| methods
        // through to the content sink, which will call |StartLayout|
        // and thus |InitialReflow| on the pres shell.  See bug 119351
        // for the ugly details.
        if (bodyContent) {
          nsIFrame *bodyFrame = aForFrame->PresContext()->GetPresShell()->
            GetPrimaryFrameFor(bodyContent);
          if (bodyFrame) {
            return bodyFrame;
          }
        }
      }
    }
  }

  return aForFrame;
}

/**
 * |FindBackground| finds the correct style data to use to paint the
 * background.  It is responsible for handling the following two
 * statements in section 14.2 of CSS2:
 *
 *   The background of the box generated by the root element covers the
 *   entire canvas.
 *
 *   For HTML documents, however, we recommend that authors specify the
 *   background for the BODY element rather than the HTML element. User
 *   agents should observe the following precedence rules to fill in the
 *   background: if the value of the 'background' property for the HTML
 *   element is different from 'transparent' then use it, else use the
 *   value of the 'background' property for the BODY element. If the
 *   resulting value is 'transparent', the rendering is undefined.
 *
 * Thus, in our implementation, it is responsible for ensuring that:
 *  + we paint the correct background on the |nsCanvasFrame|,
 *    |nsRootBoxFrame|, or |nsPageFrame|,
 *  + we don't paint the background on the root element, and
 *  + we don't paint the background on the BODY element in *some* cases,
 *    and for SGML-based HTML documents only.
 *
 * |FindBackground| returns true if a background should be painted, and
 * the resulting style context to use for the background information
 * will be filled in to |aBackground|.
 */
const nsStyleBackground*
nsCSSRendering::FindRootFrameBackground(nsIFrame* aForFrame)
{
  return FindRootFrame(aForFrame)->GetStyleBackground();
}

inline void
FindCanvasBackground(nsIFrame* aForFrame, nsIFrame* aRootElementFrame,
                     const nsStyleBackground** aBackground)
{
  if (aRootElementFrame) {
    *aBackground = nsCSSRendering::FindRootFrameBackground(aRootElementFrame);
  } else {
    // This should always give transparent, so we'll fill it in with the
    // default color if needed.  This seems to happen a bit while a page is
    // being loaded.
    *aBackground = aForFrame->GetStyleBackground();
  }
}

inline PRBool
FindElementBackground(nsIFrame* aForFrame, nsIFrame* aRootElementFrame,
                      const nsStyleBackground** aBackground)
{
  if (aForFrame == aRootElementFrame) {
    // We must have propagated our background to the viewport or canvas. Abort.
    return PR_FALSE;
  }

  *aBackground = aForFrame->GetStyleBackground();

  // Return true unless the frame is for a BODY element whose background
  // was propagated to the viewport.

  nsIContent* content = aForFrame->GetContent();
  if (!content || content->Tag() != nsGkAtoms::body)
    return PR_TRUE; // not frame for a "body" element
  // It could be a non-HTML "body" element but that's OK, we'd fail the
  // bodyContent check below

  if (aForFrame->GetStyleContext()->GetPseudoType())
    return PR_TRUE; // A pseudo-element frame.

  // We should only look at the <html> background if we're in an HTML document
  nsIDocument* document = content->GetOwnerDoc();
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document);
  if (!htmlDoc)
    return PR_TRUE;

  nsIContent* bodyContent = htmlDoc->GetBodyContentExternal();
  if (bodyContent != content)
    return PR_TRUE; // this wasn't the background that was propagated

  // This can be called even when there's no root element yet, during frame
  // construction, via nsLayoutUtils::FrameHasTransparency and
  // nsContainerFrame::SyncFrameViewProperties.
  if (!aRootElementFrame)
    return PR_TRUE;

  const nsStyleBackground* htmlBG = aRootElementFrame->GetStyleBackground();
  return !htmlBG->IsTransparent();
}

PRBool
nsCSSRendering::FindBackground(nsPresContext* aPresContext,
                               nsIFrame* aForFrame,
                               const nsStyleBackground** aBackground)
{
  nsIFrame* rootElementFrame =
    aPresContext->PresShell()->FrameConstructor()->GetRootElementStyleFrame();
  if (IsCanvasFrame(aForFrame)) {
    FindCanvasBackground(aForFrame, rootElementFrame, aBackground);
    return PR_TRUE;
  } else {
    return FindElementBackground(aForFrame, rootElementFrame, aBackground);
  }
}

void
nsCSSRendering::DidPaint()
{
  gInlineBGData->Reset();
}

PRBool
nsCSSRendering::GetBorderRadiusTwips(const nsStyleCorners& aBorderRadius,
                                     const nscoord& aFrameWidth,
                                     nscoord aRadii[8])
{
  PRBool result = PR_FALSE;
  
  // Convert percentage values
  NS_FOR_CSS_HALF_CORNERS(i) {
    const nsStyleCoord c = aBorderRadius.Get(i);

    switch (c.GetUnit()) {
      case eStyleUnit_Percent:
        aRadii[i] = (nscoord)(c.GetPercentValue() * aFrameWidth);
        break;

      case eStyleUnit_Coord:
        aRadii[i] = c.GetCoordValue();
        break;

      default:
        NS_NOTREACHED("GetBorderRadiusTwips: bad unit");
        aRadii[i] = 0;
        break;
    }

    if (aRadii[i])
      result = PR_TRUE;
  }
  return result;
}

void
nsCSSRendering::PaintBoxShadowOuter(nsPresContext* aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    nsIFrame* aForFrame,
                                    const nsRect& aFrameArea,
                                    const nsRect& aDirtyRect)
{
  nsCSSShadowArray* shadows = aForFrame->GetEffectiveBoxShadows();
  if (!shadows)
    return;
  const nsStyleBorder* styleBorder = aForFrame->GetStyleBorder();
  PRIntn sidesToSkip = aForFrame->GetSkipSides();

  // Get any border radius, since box-shadow must also have rounded corners if the frame does
  nscoord twipsRadii[8];
  PRBool hasBorderRadius = GetBorderRadiusTwips(styleBorder->mBorderRadius,
                                                aFrameArea.width, twipsRadii);
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  gfxCornerSizes borderRadii;
  ComputePixelRadii(twipsRadii, aFrameArea, sidesToSkip,
                    twipsPerPixel, &borderRadii);

  gfxRect frameGfxRect = RectToGfxRect(aFrameArea, twipsPerPixel);
  frameGfxRect.Round();
  gfxRect dirtyGfxRect = RectToGfxRect(aDirtyRect, twipsPerPixel);

  for (PRUint32 i = shadows->Length(); i > 0; --i) {
    nsCSSShadowItem* shadowItem = shadows->ShadowAt(i - 1);
    if (shadowItem->mInset)
      continue;

    gfxRect shadowRect(aFrameArea.x, aFrameArea.y, aFrameArea.width, aFrameArea.height);
    shadowRect.MoveBy(gfxPoint(shadowItem->mXOffset, shadowItem->mYOffset));
    shadowRect.Outset(shadowItem->mSpread);

    gfxRect shadowRectPlusBlur = shadowRect;
    shadowRect.ScaleInverse(twipsPerPixel);
    shadowRect.Round();

    // shadowRect won't include the blur, so make an extra rect here that includes the blur
    // for use in the even-odd rule below.
    nscoord blurRadius = shadowItem->mRadius;
    shadowRectPlusBlur.Outset(blurRadius);
    shadowRectPlusBlur.ScaleInverse(twipsPerPixel);
    shadowRectPlusBlur.RoundOut();

    gfxContext* renderContext = aRenderingContext.ThebesContext();
    nsRefPtr<gfxContext> shadowContext;
    nsContextBoxBlur blurringArea;

    // shadowRect is already in device pixels, pass 1 as the appunits/pixel value
    blurRadius /= twipsPerPixel;
    shadowContext = blurringArea.Init(shadowRect, blurRadius, 1, renderContext, dirtyGfxRect);
    if (!shadowContext)
      continue;

    // Set the shadow color; if not specified, use the foreground color
    nscolor shadowColor;
    if (shadowItem->mHasColor)
      shadowColor = shadowItem->mColor;
    else
      shadowColor = aForFrame->GetStyleColor()->mColor;

    renderContext->Save();
    renderContext->SetColor(gfxRGBA(shadowColor));

    // Clip out the area of the actual frame so the shadow is not shown within
    // the frame
    renderContext->NewPath();
    renderContext->Rectangle(shadowRectPlusBlur);
    if (hasBorderRadius)
      renderContext->RoundedRectangle(frameGfxRect, borderRadii);
    else
      renderContext->Rectangle(frameGfxRect);
    renderContext->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
    renderContext->Clip();

    // Draw the shape of the frame so it can be blurred. Recall how nsContextBoxBlur
    // doesn't make any temporary surfaces if blur is 0 and it just returns the original
    // surface? If we have no blur, we're painting this fill on the actual content surface
    // (renderContext == shadowContext) which is why we set up the color and clip
    // before doing this.
    shadowContext->NewPath();
    if (hasBorderRadius) {
      gfxCornerSizes clipRectRadii;
      gfxFloat spreadDistance = -shadowItem->mSpread / twipsPerPixel;
      gfxFloat borderSizes[4] = {
        spreadDistance, spreadDistance,
        spreadDistance, spreadDistance
      };
      nsCSSBorderRenderer::ComputeInnerRadii(borderRadii, borderSizes,
                                             &clipRectRadii);
      shadowContext->RoundedRectangle(shadowRect, clipRectRadii);
    } else {
      shadowContext->Rectangle(shadowRect);
    }
    shadowContext->Fill();

    blurringArea.DoPaint();
    renderContext->Restore();
  }
}

void
nsCSSRendering::PaintBoxShadowInner(nsPresContext* aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    nsIFrame* aForFrame,
                                    const nsRect& aFrameArea,
                                    const nsRect& aDirtyRect)
{
  nsCSSShadowArray* shadows = aForFrame->GetEffectiveBoxShadows();
  if (!shadows)
    return;
  const nsStyleBorder* styleBorder = aForFrame->GetStyleBorder();

  // Get any border radius, since box-shadow must also have rounded corners if the frame does
  nscoord twipsRadii[8];
  PRBool hasBorderRadius = GetBorderRadiusTwips(styleBorder->mBorderRadius,
                                                aFrameArea.width, twipsRadii);
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  nsRect paddingRect = aFrameArea;
  nsMargin border = aForFrame->GetUsedBorder();
  aForFrame->ApplySkipSides(border);
  paddingRect.Deflate(border);

  gfxCornerSizes innerRadii;
  if (hasBorderRadius) {
    gfxCornerSizes borderRadii;
    PRIntn sidesToSkip = aForFrame->GetSkipSides();

    ComputePixelRadii(twipsRadii, aFrameArea, sidesToSkip,
                      twipsPerPixel, &borderRadii);
    gfxFloat borderSizes[4] = {
      border.top / twipsPerPixel, border.right / twipsPerPixel,
      border.bottom / twipsPerPixel, border.left / twipsPerPixel
    };
    nsCSSBorderRenderer::ComputeInnerRadii(borderRadii, borderSizes,
                                           &innerRadii);
  }

  gfxRect dirtyGfxRect = RectToGfxRect(aDirtyRect, twipsPerPixel);
  for (PRUint32 i = shadows->Length(); i > 0; --i) {
    nsCSSShadowItem* shadowItem = shadows->ShadowAt(i - 1);
    if (!shadowItem->mInset)
      continue;

    /*
     * shadowRect: the frame's padding rect
     * shadowPaintRect: the area to paint on the temp surface, larger than shadowRect
     *                  so that blurs still happen properly near the edges
     * shadowClipRect: the area on the temporary surface within shadowPaintRect
     *                 that we will NOT paint in
     */
    nscoord blurRadius = shadowItem->mRadius;
    gfxRect shadowRect(paddingRect.x, paddingRect.y, paddingRect.width, paddingRect.height);
    gfxRect shadowPaintRect = shadowRect;
    shadowPaintRect.Outset(blurRadius);

    gfxRect shadowClipRect = shadowRect;
    shadowClipRect.MoveBy(gfxPoint(shadowItem->mXOffset, shadowItem->mYOffset));
    shadowClipRect.Inset(shadowItem->mSpread);

    shadowRect.ScaleInverse(twipsPerPixel);
    shadowRect.Round();
    shadowPaintRect.ScaleInverse(twipsPerPixel);
    shadowPaintRect.RoundOut();
    shadowClipRect.ScaleInverse(twipsPerPixel);
    shadowClipRect.Round();

    gfxContext* renderContext = aRenderingContext.ThebesContext();
    nsRefPtr<gfxContext> shadowContext;
    nsContextBoxBlur blurringArea;

    // shadowPaintRect is already in device pixels, pass 1 as the appunits/pixel value
    blurRadius /= twipsPerPixel;
    shadowContext = blurringArea.Init(shadowPaintRect, blurRadius, 1, renderContext, dirtyGfxRect);
    if (!shadowContext)
      continue;

    // Set the shadow color; if not specified, use the foreground color
    nscolor shadowColor;
    if (shadowItem->mHasColor)
      shadowColor = shadowItem->mColor;
    else
      shadowColor = aForFrame->GetStyleColor()->mColor;

    renderContext->Save();
    renderContext->SetColor(gfxRGBA(shadowColor));

    // Clip the context to the area of the frame's padding rect, so no part of the
    // shadow is painted outside
    renderContext->NewPath();
    if (hasBorderRadius)
      renderContext->RoundedRectangle(shadowRect, innerRadii, PR_FALSE);
    else
      renderContext->Rectangle(shadowRect);
    renderContext->Clip();

    // Fill the temporary surface minus the area within the frame that we should
    // not paint in, and blur and apply it
    shadowContext->NewPath();
    shadowContext->Rectangle(shadowPaintRect);
    if (hasBorderRadius) {
      // Calculate the radii the inner clipping rect will have
      gfxCornerSizes clipRectRadii;
      gfxFloat spreadDistance = shadowItem->mSpread / twipsPerPixel;
      gfxFloat borderSizes[4] = {
        spreadDistance, spreadDistance,
        spreadDistance, spreadDistance
      };
      nsCSSBorderRenderer::ComputeInnerRadii(innerRadii, borderSizes,
                                             &clipRectRadii);
      shadowContext->RoundedRectangle(shadowClipRect, clipRectRadii, PR_FALSE);
    } else {
      shadowContext->Rectangle(shadowClipRect);
    }
    shadowContext->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
    shadowContext->Fill();

    blurringArea.DoPaint();
    renderContext->Restore();
  }
}

void
nsCSSRendering::PaintBackground(nsPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsIFrame* aForFrame,
                                const nsRect& aDirtyRect,
                                const nsRect& aBorderArea,
                                PRUint32 aFlags,
                                nsRect* aBGClipRect)
{
  NS_PRECONDITION(aForFrame,
                  "Frame is expected to be provided to PaintBackground");

  const nsStyleBackground *background;
  if (!FindBackground(aPresContext, aForFrame, &background)) {
    // We don't want to bail out if moz-appearance is set on a root
    // node. If it has a parent content node, bail because it's not
    // a root, other wise keep going in order to let the theme stuff
    // draw the background. The canvas really should be drawing the
    // bg, but there's no way to hook that up via css.
    if (!aForFrame->GetStyleDisplay()->mAppearance) {
      return;
    }

    nsIContent* content = aForFrame->GetContent();
    if (!content || content->GetParent()) {
      return;
    }

    background = aForFrame->GetStyleBackground();
  }

  PaintBackgroundWithSC(aPresContext, aRenderingContext, aForFrame,
                        aDirtyRect, aBorderArea, *background,
                        *aForFrame->GetStyleBorder(), aFlags,
                        aBGClipRect);
}

static PRBool
IsSolidBorderEdge(const nsStyleBorder& aBorder, PRUint32 aSide)
{
  if (aBorder.GetActualBorder().side(aSide) == 0)
    return PR_TRUE;
  if (aBorder.GetBorderStyle(aSide) != NS_STYLE_BORDER_STYLE_SOLID)
    return PR_FALSE;

  // If we're using a border image, assume it's not fully opaque,
  // because we may not even have the image loaded at this point, and
  // even if we did, checking whether the relevant tile is fully
  // opaque would be too much work.
  if (aBorder.GetBorderImage())
    return PR_FALSE;

  nscolor color;
  PRBool isForeground;
  aBorder.GetBorderColor(aSide, color, isForeground);

  // We don't know the foreground color here, so if it's being used
  // we must assume it might be transparent.
  if (isForeground)
    return PR_FALSE;

  return NS_GET_A(color) == 255;
}

/**
 * Returns true if all border edges are either missing or opaque.
 */
static PRBool
IsSolidBorder(const nsStyleBorder& aBorder)
{
  if (aBorder.mBorderColors)
    return PR_FALSE;
  for (PRUint32 i = 0; i < 4; ++i) {
    if (!IsSolidBorderEdge(aBorder, i))
      return PR_FALSE;
  }
  return PR_TRUE;
}

static inline void
SetupDirtyRects(const nsRect& aBGClipArea, const nsRect& aCallerDirtyRect,
                nscoord aAppUnitsPerPixel,
                /* OUT: */
                nsRect* aDirtyRect, gfxRect* aDirtyRectGfx)
{
  aDirtyRect->IntersectRect(aBGClipArea, aCallerDirtyRect);

  // Compute the Thebes equivalent of the dirtyRect.
  *aDirtyRectGfx = RectToGfxRect(*aDirtyRect, aAppUnitsPerPixel);
  NS_WARN_IF_FALSE(aDirtyRect->IsEmpty() || !aDirtyRectGfx->IsEmpty(),
                   "converted dirty rect should not be empty");
  NS_ABORT_IF_FALSE(!aDirtyRect->IsEmpty() || aDirtyRectGfx->IsEmpty(),
                    "second should be empty if first is");
}

static void
SetupBackgroundClip(gfxContext *aCtx, PRUint8 aBackgroundClip,
                    nsIFrame* aForFrame, const nsRect& aBorderArea,
                    const nsRect& aCallerDirtyRect, PRBool aHaveRoundedCorners,
                    const gfxCornerSizes& aBGRadii, nscoord aAppUnitsPerPixel,
                    gfxContextAutoSaveRestore* aAutoSR,
                    /* OUT: */
                    nsRect* aBGClipArea, nsRect* aDirtyRect,
                    gfxRect* aDirtyRectGfx)
{
  *aBGClipArea = aBorderArea;
  PRBool radiiAreOuter = PR_TRUE;
  gfxCornerSizes clippedRadii = aBGRadii;
  if (aBackgroundClip != NS_STYLE_BG_CLIP_BORDER) {
    NS_ASSERTION(aBackgroundClip == NS_STYLE_BG_CLIP_PADDING,
                 "unexpected background-clip");
    nsMargin border = aForFrame->GetUsedBorder();
    aForFrame->ApplySkipSides(border);
    aBGClipArea->Deflate(border);

    if (aHaveRoundedCorners) {
      gfxFloat borderSizes[4] = {
        border.top / aAppUnitsPerPixel, border.right / aAppUnitsPerPixel,
        border.bottom / aAppUnitsPerPixel, border.left / aAppUnitsPerPixel
      };
      nsCSSBorderRenderer::ComputeInnerRadii(aBGRadii, borderSizes,
                                             &clippedRadii);
      radiiAreOuter = PR_FALSE;
    }
  }

  SetupDirtyRects(*aBGClipArea, aCallerDirtyRect, aAppUnitsPerPixel,
                  aDirtyRect, aDirtyRectGfx);

  if (aDirtyRectGfx->IsEmpty()) {
    // Our caller won't draw anything under this condition, so no need
    // to set more up.
    return;
  }

  // If we have rounded corners, clip all subsequent drawing to the
  // rounded rectangle defined by bgArea and bgRadii (we don't know
  // whether the rounded corners intrude on the dirtyRect or not).
  // Do not do this if we have a caller-provided clip rect --
  // as above with bgArea, arguably a bug, but table painting seems
  // to depend on it.

  if (aHaveRoundedCorners) {
    gfxRect bgAreaGfx(RectToGfxRect(*aBGClipArea, aAppUnitsPerPixel));
    bgAreaGfx.Round();
    bgAreaGfx.Condition();

    if (bgAreaGfx.IsEmpty()) {
      // I think it's become possible to hit this since
      // http://hg.mozilla.org/mozilla-central/rev/50e934e4979b landed.
      NS_WARNING("converted background area should not be empty");
      // Make our caller not do anything.
      aDirtyRectGfx->size.SizeTo(0.0, 0.0);
      return;
    }

    aAutoSR->Reset(aCtx);
    aCtx->NewPath();
    aCtx->RoundedRectangle(bgAreaGfx, clippedRadii, radiiAreOuter);
    aCtx->Clip();
  }
}

static nscolor
DetermineBackgroundColorInternal(nsPresContext* aPresContext,
                                 const nsStyleBackground& aBackground,
                                 nsIFrame* aFrame,
                                 PRBool& aDrawBackgroundImage,
                                 PRBool& aDrawBackgroundColor)
{
  aDrawBackgroundImage = PR_TRUE;
  aDrawBackgroundColor = PR_TRUE;

  if (aFrame->HonorPrintBackgroundSettings()) {
    aDrawBackgroundImage = aPresContext->GetBackgroundImageDraw();
    aDrawBackgroundColor = aPresContext->GetBackgroundColorDraw();
  }

  nscolor bgColor;
  if (aDrawBackgroundColor) {
    bgColor = aBackground.mBackgroundColor;
    if (NS_GET_A(bgColor) == 0)
      aDrawBackgroundColor = PR_FALSE;
  } else {
    // If GetBackgroundColorDraw() is false, we are still expected to
    // draw color in the background of any frame that's not completely
    // transparent, but we are expected to use white instead of whatever
    // color was specified.
    bgColor = NS_RGB(255, 255, 255);
    if (aDrawBackgroundImage || !aBackground.IsTransparent())
      aDrawBackgroundColor = PR_TRUE;
    else
      bgColor = NS_RGBA(0,0,0,0);
  }

  return bgColor;
}

nscolor
nsCSSRendering::DetermineBackgroundColor(nsPresContext* aPresContext,
                                         const nsStyleBackground& aBackground,
                                         nsIFrame* aFrame)
{
  PRBool drawBackgroundImage;
  PRBool drawBackgroundColor;
  return DetermineBackgroundColorInternal(aPresContext,
                                          aBackground,
                                          aFrame,
                                          drawBackgroundImage,
                                          drawBackgroundColor);
}

static gfxFloat
ConvertGradientValueToPixels(const nsStyleCoord& aCoord,
                             nscoord aFillLength,
                             nscoord aAppUnitsPerPixel)
{
  switch (aCoord.GetUnit()) {
    case eStyleUnit_Percent:
      return aCoord.GetPercentValue() * aFillLength / aAppUnitsPerPixel;
    case eStyleUnit_Coord:
      return aCoord.GetCoordValue() / aAppUnitsPerPixel;
    default:
      NS_WARNING("Unexpected coord unit");
      return 0;
  }
}

void
nsCSSRendering::PaintGradient(nsPresContext* aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsStyleGradient* aGradient,
                              const nsRect& aDirtyRect,
                              const nsRect& aOneCellArea,
                              const nsRect& aFillArea,
                              PRBool aRepeat)
{
  gfxContext *ctx = aRenderingContext.ThebesContext();
  nscoord appUnitsPerPixel = aPresContext->AppUnitsPerDevPixel();

  gfxRect dirtyRect = RectToGfxRect(aDirtyRect, appUnitsPerPixel);
  gfxRect areaToFill = RectToGfxRect(aFillArea, appUnitsPerPixel);
  gfxRect oneCellArea = RectToGfxRect(aOneCellArea, appUnitsPerPixel);
  gfxPoint fillOrigin = oneCellArea.TopLeft();

  areaToFill = areaToFill.Intersect(dirtyRect);
  if (areaToFill.IsEmpty())
    return;

  gfxFloat gradX0 = ConvertGradientValueToPixels(aGradient->mStartX,
                        aOneCellArea.width, appUnitsPerPixel);
  gfxFloat gradY0 = ConvertGradientValueToPixels(aGradient->mStartY,
                        aOneCellArea.height, appUnitsPerPixel);
  gfxFloat gradX1 = ConvertGradientValueToPixels(aGradient->mEndX,
                        aOneCellArea.width, appUnitsPerPixel);
  gfxFloat gradY1 = ConvertGradientValueToPixels(aGradient->mEndY,
                        aOneCellArea.height, appUnitsPerPixel);

  nsRefPtr<gfxPattern> gradientPattern;
  if (aGradient->mIsRadial) {
    gfxFloat gradRadius0 = double(aGradient->mStartRadius) / appUnitsPerPixel;
    gfxFloat gradRadius1 = double(aGradient->mEndRadius) / appUnitsPerPixel;
    gradientPattern = new gfxPattern(gradX0, gradY0, gradRadius0,
                                     gradX1, gradY1, gradRadius1);
  } else {
    gradientPattern = new gfxPattern(gradX0, gradY0, gradX1, gradY1);
  }

  if (!gradientPattern || gradientPattern->CairoStatus())
    return;

  for (PRUint32 i = 0; i < aGradient->mStops.Length(); i++) {
    gradientPattern->AddColorStop(aGradient->mStops[i].mPosition,
                                  gfxRGBA(aGradient->mStops[i].mColor));
  }

  if (aRepeat)
    gradientPattern->SetExtend(gfxPattern::EXTEND_REPEAT);

  ctx->Save();
  ctx->NewPath();
  // The fill origin is part of the translate call so the pattern starts at
  // the desired point, rather than (0,0).
  ctx->Translate(fillOrigin);
  ctx->SetPattern(gradientPattern);
  ctx->Rectangle(areaToFill - fillOrigin, PR_TRUE);
  ctx->Fill();
  ctx->Restore();
}

void
nsCSSRendering::PaintBackgroundWithSC(nsPresContext* aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      nsIFrame* aForFrame,
                                      const nsRect& aDirtyRect,
                                      const nsRect& aBorderArea,
                                      const nsStyleBackground& aBackground,
                                      const nsStyleBorder& aBorder,
                                      PRUint32 aFlags,
                                      nsRect* aBGClipRect)
{
  NS_PRECONDITION(aForFrame,
                  "Frame is expected to be provided to PaintBackground");

  // Check to see if we have an appearance defined.  If so, we let the theme
  // renderer draw the background and bail out.
  // XXXzw this ignores aBGClipRect.
  const nsStyleDisplay* displayData = aForFrame->GetStyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, aForFrame,
                                            displayData->mAppearance)) {
      nsRect dirty;
      dirty.IntersectRect(aDirtyRect, aBorderArea);
      theme->DrawWidgetBackground(&aRenderingContext, aForFrame, 
                                  displayData->mAppearance, aBorderArea, dirty);
      return;
    }
  }

  // For canvas frames (in the CSS sense) we draw the background color using
  // a solid color item that gets added in nsLayoutUtils::PaintFrame,
  // PresShell::RenderDocument, or nsSubDocumentFrame::BuildDisplayList
  // (bug 488242).
  PRBool isCanvasFrame = IsCanvasFrame(aForFrame);

  // Determine whether we are drawing background images and/or
  // background colors.
  PRBool drawBackgroundImage;
  PRBool drawBackgroundColor;

  nscolor bgColor = DetermineBackgroundColorInternal(aPresContext,
                                                     aBackground,
                                                     aForFrame,
                                                     drawBackgroundImage,
                                                     drawBackgroundColor);

  // At this point, drawBackgroundImage and drawBackgroundColor are
  // true if and only if we are actually supposed to paint an image or
  // color into aDirtyRect, respectively.
  if (!drawBackgroundImage && !drawBackgroundColor)
    return;

  // Compute the outermost boundary of the area that might be painted.
  gfxContext *ctx = aRenderingContext.ThebesContext();
  nscoord appUnitsPerPixel = aPresContext->AppUnitsPerDevPixel();

  // Same coordinate space as aBorderArea & aBGClipRect
  gfxCornerSizes bgRadii;
  PRBool haveRoundedCorners;
  {
    nscoord radii[8];
    haveRoundedCorners =
      GetBorderRadiusTwips(aBorder.mBorderRadius, aForFrame->GetSize().width,
                           radii);
    if (haveRoundedCorners)
      ComputePixelRadii(radii, aBorderArea, aForFrame->GetSkipSides(),
                        appUnitsPerPixel, &bgRadii);
  }
  
  // The 'bgClipArea' (used only by the image tiling logic, far below)
  // is the caller-provided aBGClipRect if any, or else the area
  // determined by the value of 'background-clip' in
  // SetupCurrentBackgroundClip.  (Arguably it should be the
  // intersection, but that breaks the table painter -- in particular,
  // taking the intersection breaks reftests/bugs/403249-1[ab].)
  nsRect bgClipArea, dirtyRect;
  gfxRect dirtyRectGfx;
  PRUint8 currentBackgroundClip;
  PRBool isSolidBorder;
  gfxContextAutoSaveRestore autoSR;
  if (aBGClipRect) {
    bgClipArea = *aBGClipRect;
    SetupDirtyRects(bgClipArea, aDirtyRect, appUnitsPerPixel,
                    &dirtyRect, &dirtyRectGfx);
  } else {
    // The background is rendered over the 'background-clip' area,
    // which is normally equal to the border area but may be reduced
    // to the padding area by CSS.  Also, if the border is solid, we
    // don't need to draw outside the padding area.  In either case,
    // if the borders are rounded, make sure we use the same inner
    // radii as the border code will.
    // The background-color is drawn based on the bottom
    // background-clip.
    currentBackgroundClip = aBackground.BottomLayer().mClip;
    isSolidBorder =
      (aFlags & PAINTBG_WILL_PAINT_BORDER) && IsSolidBorder(aBorder);
    if (isSolidBorder)
      currentBackgroundClip = NS_STYLE_BG_CLIP_PADDING;
    SetupBackgroundClip(ctx, currentBackgroundClip, aForFrame,
                        aBorderArea, aDirtyRect, haveRoundedCorners,
                        bgRadii, appUnitsPerPixel, &autoSR,
                        &bgClipArea, &dirtyRect, &dirtyRectGfx);
  }

  // If we might be using a background color, go ahead and set it now.
  if (drawBackgroundColor && !isCanvasFrame)
    ctx->SetColor(gfxRGBA(bgColor));

  // If there is no background image, draw a color.  (If there is
  // neither a background image nor a color, we wouldn't have gotten
  // this far.)
  if (!drawBackgroundImage) {
    if (!dirtyRectGfx.IsEmpty() && !isCanvasFrame) {
      ctx->NewPath();
      ctx->Rectangle(dirtyRectGfx, PR_TRUE);
      ctx->Fill();
    }
    return;
  }

  // Ensure we get invalidated for loads of the image.  We need to do
  // this here because this might be the only code that knows about the
  // association of the style data with the frame.
  aPresContext->SetupBackgroundImageLoaders(aForFrame, &aBackground);

  // We can skip painting the background color if a background image is opaque.
  if (drawBackgroundColor &&
      aBackground.BottomLayer().mRepeat == NS_STYLE_BG_REPEAT_XY &&
      aBackground.BottomLayer().mImage.IsOpaque())
    drawBackgroundColor = PR_FALSE;

  // The background color is rendered over the entire dirty area,
  // even if the image isn't.
  if (drawBackgroundColor && !isCanvasFrame) {
    if (!dirtyRectGfx.IsEmpty()) {
      ctx->NewPath();
      ctx->Rectangle(dirtyRectGfx, PR_TRUE);
      ctx->Fill();
    }
  }

  if (drawBackgroundImage) {
    NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, &aBackground) {
      const nsStyleBackground::Layer &layer = aBackground.mLayers[i];
      if (!aBGClipRect) {
        PRUint8 newBackgroundClip =
          isSolidBorder ? NS_STYLE_BG_CLIP_PADDING : layer.mClip;
        if (currentBackgroundClip != newBackgroundClip) {
          currentBackgroundClip = newBackgroundClip;
          SetupBackgroundClip(ctx, currentBackgroundClip, aForFrame,
                              aBorderArea, aDirtyRect, haveRoundedCorners,
                              bgRadii, appUnitsPerPixel, &autoSR,
                              &bgClipArea, &dirtyRect, &dirtyRectGfx);
        }
      }
      if (!dirtyRectGfx.IsEmpty()) {
        PaintBackgroundLayer(aPresContext, aRenderingContext, aForFrame, aFlags,
                             dirtyRect, aBorderArea, bgClipArea, aBackground,
                             layer);
      }
    }
  }
}

static inline float
ScaleDimension(nsStyleBackground::Size::Dimension aDimension,
               PRUint8 aType,
               nscoord aLength, nscoord aAvailLength)
{
  switch (aType) {
    case nsStyleBackground::Size::ePercentage:
      return double(aDimension.mFloat) * (double(aAvailLength) / double(aLength));
    case nsStyleBackground::Size::eLength:
      return double(aDimension.mCoord) / double(aLength);
    default:
      NS_ABORT_IF_FALSE(PR_FALSE, "bad aDimension.mType");
      return 1.0f;
    case nsStyleBackground::Size::eAuto:
      NS_ABORT_IF_FALSE(PR_FALSE, "aDimension.mType == eAuto isn't handled");
      return 1.0f;
  }
}

static void
PaintBackgroundLayer(nsPresContext* aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     nsIFrame* aForFrame,
                     PRUint32 aFlags,
                     const nsRect& aDirtyRect, // intersected with aBGClipRect
                     const nsRect& aBorderArea,
                     const nsRect& aBGClipRect,
                     const nsStyleBackground& aBackground,
                     const nsStyleBackground::Layer& aLayer)
{
  /*
   * The background properties we need to keep in mind when drawing background
   * layers are:
   *
   *   background-image
   *   background-repeat
   *   background-attachment
   *   background-position
   *   background-clip (-moz-background-clip)
   *   background-origin (-moz-background-origin)
   *   background-size (-moz-background-size)
   *   background-break (-moz-background-inline-policy)
   *
   * (background-color applies to the entire element and not to individual
   * layers, so it is irrelevant to this method.)
   *
   * These properties have the following dependencies upon each other when
   * determining rendering:
   *
   *   background-image
   *     no dependencies
   *   background-repeat
   *     no dependencies
   *   background-attachment
   *     no dependencies
   *   background-position
   *     depends upon background-size (for the image's scaled size) and
   *     background-break (for the background positioning area)
   *   background-clip
   *     no dependencies
   *   background-origin
   *     depends upon background-attachment (only in the case where that value
   *     is 'fixed')
   *   background-size
   *     depends upon background-break (for the background positioning area for
   *     resolving percentages), background-image (for the image's intrinsic
   *     size), background-repeat (if that value is 'round'), and
   *     background-origin (for the background painting area, when
   *     background-repeat is 'round')
   *   background-break
   *     depends upon background-origin (specifying how the boxes making up the
   *     background positioning area are determined)
   *
   * As a result of only-if dependencies we don't strictly do a topological
   * sort of the above properties when processing, but it's pretty close to one:
   *
   *   background-clip (by caller)
   *   background-image
   *   background-break, background-origin
   *   background-attachment (postfix for background-{origin,break} if 'fixed')
   *   background-size
   *   background-position
   *   background-repeat
   */

  PRUint32 irFlags = 0;
  if (aFlags & nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES)
    irFlags |= ImageRenderer::FLAG_SYNC_DECODE_IMAGES;
  ImageRenderer imageRenderer(aForFrame, aLayer.mImage, irFlags);
  if (!imageRenderer.PrepareImage()) {
    // There's no image or it's not ready to be painted.
    return;
  }

  // Compute background origin area relative to aBorderArea now as we may need
  // it to compute the effective image size for a CSS gradient.
  nsRect bgPositioningArea(0, 0, 0, 0);

  nsIAtom* frameType = aForFrame->GetType();
  nsIFrame* geometryFrame = aForFrame;
  if (frameType == nsGkAtoms::inlineFrame ||
      frameType == nsGkAtoms::positionedInlineFrame) {
    // XXXjwalden Strictly speaking this is not quite faithful to how
    // background-break is supposed to interact with background-origin values,
    // but it's a non-trivial amount of work to make it fully conformant, and
    // until the specification is more finalized (and assuming background-break
    // even makes the cut) it doesn't make sense to hammer out exact behavior.
    switch (aBackground.mBackgroundInlinePolicy) {
    case NS_STYLE_BG_INLINE_POLICY_EACH_BOX:
      bgPositioningArea = nsRect(nsPoint(0,0), aBorderArea.Size());
      break;
    case NS_STYLE_BG_INLINE_POLICY_BOUNDING_BOX:
      bgPositioningArea = gInlineBGData->GetBoundingRect(aForFrame);
      break;
    default:
      NS_ERROR("Unknown background-inline-policy value!  "
               "Please, teach me what to do.");
    case NS_STYLE_BG_INLINE_POLICY_CONTINUOUS:
      bgPositioningArea = gInlineBGData->GetContinuousRect(aForFrame);
      break;
    }
  } else if (frameType == nsGkAtoms::canvasFrame) {
    geometryFrame = aForFrame->GetFirstChild(nsnull);
    // geometryFrame might be null if this canvas is a page created
    // as an overflow container (e.g. the in-flow content has already
    // finished and this page only displays the continuations of
    // absolutely positioned content).
    if (geometryFrame) {
      bgPositioningArea = geometryFrame->GetRect();
    }
  } else {
    bgPositioningArea = nsRect(nsPoint(0,0), aBorderArea.Size());
  }

  // Background images are tiled over the 'background-clip' area
  // but the origin of the tiling is based on the 'background-origin' area
  if (aLayer.mOrigin != NS_STYLE_BG_ORIGIN_BORDER && geometryFrame) {
    nsMargin border = geometryFrame->GetUsedBorder();
    geometryFrame->ApplySkipSides(border);
    bgPositioningArea.Deflate(border);
    if (aLayer.mOrigin != NS_STYLE_BG_ORIGIN_PADDING) {
      nsMargin padding = geometryFrame->GetUsedPadding();
      geometryFrame->ApplySkipSides(padding);
      bgPositioningArea.Deflate(padding);
      NS_ASSERTION(aLayer.mOrigin == NS_STYLE_BG_ORIGIN_CONTENT,
                   "unknown background-origin value");
    }
  }

  // Compute the anchor point.
  //
  // relative to aBorderArea.TopLeft() (which is where the top-left
  // of aForFrame's border-box will be rendered)
  nsPoint imageTopLeft, anchor, offset;
  if (NS_STYLE_BG_ATTACHMENT_FIXED == aLayer.mAttachment) {
    // If it's a fixed background attachment, then the image is placed
    // relative to the viewport, which is the area of the root frame
    // in a screen context or the page content frame in a print context.
    nsIFrame* topFrame =
      aPresContext->PresShell()->FrameManager()->GetRootFrame();
    NS_ASSERTION(topFrame, "no root frame");
    nsIFrame* pageContentFrame = nsnull;
    if (aPresContext->IsPaginated()) {
      pageContentFrame =
        nsLayoutUtils::GetClosestFrameOfType(aForFrame, nsGkAtoms::pageContentFrame);
      if (pageContentFrame) {
        topFrame = pageContentFrame;
      }
      // else this is an embedded shell and its root frame is what we want
    }

    // Set the background positioning area to the viewport's area.
    bgPositioningArea.SetRect(nsPoint(0, 0), topFrame->GetSize());

    if (!pageContentFrame) {
      // Subtract the size of scrollbars.
      nsIScrollableFrame* scrollableFrame =
        aPresContext->PresShell()->GetRootScrollFrameAsScrollable();
      if (scrollableFrame) {
        nsMargin scrollbars = scrollableFrame->GetActualScrollbarSizes();
        bgPositioningArea.Deflate(scrollbars);
      }
    }

    offset = bgPositioningArea.TopLeft() - aForFrame->GetOffsetTo(topFrame);
  } else {
    offset = bgPositioningArea.TopLeft();
  }

  nsSize imageSize = imageRenderer.ComputeSize(bgPositioningArea.Size());
  if (imageSize.width <= 0 || imageSize.height <= 0)
    return;
     
  // Scale the image as specified for background-size and as required for
  // proper background positioning when background-position is defined with
  // percentages.
  float scaleX, scaleY;
  switch (aLayer.mSize.mWidthType) {
    case nsStyleBackground::Size::eContain:
    case nsStyleBackground::Size::eCover: {
      float scaleFitX = double(bgPositioningArea.width) / imageSize.width;
      float scaleFitY = double(bgPositioningArea.height) / imageSize.height;
      if (aLayer.mSize.mWidthType == nsStyleBackground::Size::eCover) {
        scaleX = scaleY = NS_MAX(scaleFitX, scaleFitY);
      } else {
        scaleX = scaleY = NS_MIN(scaleFitX, scaleFitY);
      }
      break;
    }
    default: {
      if (aLayer.mSize.mWidthType == nsStyleBackground::Size::eAuto) {
        if (aLayer.mSize.mHeightType == nsStyleBackground::Size::eAuto) {
          scaleX = scaleY = 1.0f;
        } else {
          scaleX = scaleY =
            ScaleDimension(aLayer.mSize.mHeight, aLayer.mSize.mHeightType,
                           imageSize.height, bgPositioningArea.height);
        }
      } else {
        if (aLayer.mSize.mHeightType == nsStyleBackground::Size::eAuto) {
          scaleX = scaleY =
            ScaleDimension(aLayer.mSize.mWidth, aLayer.mSize.mWidthType,
                           imageSize.width, bgPositioningArea.width);
        } else {
          scaleX = ScaleDimension(aLayer.mSize.mWidth, aLayer.mSize.mWidthType,
                                  imageSize.width, bgPositioningArea.width);
          scaleY = ScaleDimension(aLayer.mSize.mHeight, aLayer.mSize.mHeightType,
                                  imageSize.height, bgPositioningArea.height);
        }
      }
      break;
    }
  }
  imageSize.width = NSCoordSaturatingNonnegativeMultiply(imageSize.width, scaleX);
  imageSize.height = NSCoordSaturatingNonnegativeMultiply(imageSize.height, scaleY);

  // Compute the position of the background now that the background's size is
  // determined.
  ComputeBackgroundAnchorPoint(aLayer, bgPositioningArea.Size(), imageSize,
                               &imageTopLeft, &anchor);
  imageTopLeft += offset;
  anchor += offset;

  nsRect destArea(imageTopLeft + aBorderArea.TopLeft(), imageSize);
  nsRect fillArea = destArea;
  PRIntn repeat = aLayer.mRepeat;
  PR_STATIC_ASSERT(NS_STYLE_BG_REPEAT_XY ==
                   (NS_STYLE_BG_REPEAT_X | NS_STYLE_BG_REPEAT_Y));
  if (repeat & NS_STYLE_BG_REPEAT_X) {
    fillArea.x = aBGClipRect.x;
    fillArea.width = aBGClipRect.width;
  }
  if (repeat & NS_STYLE_BG_REPEAT_Y) {
    fillArea.y = aBGClipRect.y;
    fillArea.height = aBGClipRect.height;
  }
  fillArea.IntersectRect(fillArea, aBGClipRect);

  imageRenderer.Draw(aPresContext, aRenderingContext, destArea, fillArea,
                     anchor + aBorderArea.TopLeft(), aDirtyRect,
                     (repeat != NS_STYLE_BG_REPEAT_OFF));
}

static void
DrawBorderImage(nsPresContext*       aPresContext,
                nsIRenderingContext& aRenderingContext,
                nsIFrame*            aForFrame,
                const nsRect&        aBorderArea,
                const nsStyleBorder& aBorderStyle,
                const nsRect&        aDirtyRect)
{
  if (aDirtyRect.IsEmpty())
    return;

  // Ensure we get invalidated for loads and animations of the image.
  // We need to do this here because this might be the only code that
  // knows about the association of the style data with the frame.
  // XXX We shouldn't really... since if anybody is passing in a
  // different style, they'll potentially have the wrong size for the
  // border too.
  aPresContext->SetupBorderImageLoaders(aForFrame, &aBorderStyle);

  imgIRequest *req = aBorderStyle.GetBorderImage();

#ifdef DEBUG
  {
    PRUint32 status = imgIRequest::STATUS_ERROR;
    if (req)
      req->GetImageStatus(&status);

    NS_ASSERTION(req && (status & imgIRequest::STATUS_LOAD_COMPLETE),
                 "no image to draw");
  }
#endif

  // Get the actual image, and determine where the split points are.
  // Note that mBorderImageSplit is in image pixels, not necessarily
  // CSS pixels.

  nsCOMPtr<imgIContainer> imgContainer;
  req->GetImage(getter_AddRefs(imgContainer));

  nsIntSize imageSize;
  imgContainer->GetWidth(&imageSize.width);
  imgContainer->GetHeight(&imageSize.height);

  // Convert percentages and clamp values to the image size.
  nsIntMargin split;
  NS_FOR_CSS_SIDES(s) {
    nsStyleCoord coord = aBorderStyle.mBorderImageSplit.Get(s);
    PRInt32 imgDimension = ((s == NS_SIDE_TOP || s == NS_SIDE_BOTTOM)
                            ? imageSize.height
                            : imageSize.width);
    double value;
    switch (coord.GetUnit()) {
      case eStyleUnit_Percent:
        value = coord.GetPercentValue() * imgDimension;
        break;
      case eStyleUnit_Factor:
        value = coord.GetFactorValue();
        break;
      default:
        NS_ASSERTION(coord.GetUnit() == eStyleUnit_Null,
                     "unexpected CSS unit for image split");
        value = 0;
        break;
    }
    if (value < 0)
      value = 0;
    if (value > imgDimension)
      value = imgDimension;
    split.side(s) = NS_lround(value);
  }

  nsMargin border(aBorderStyle.GetActualBorder());

  // These helper tables recharacterize the 'split' and 'border' margins
  // in a more convenient form: they are the x/y/width/height coords
  // required for various bands of the border, and they have been transformed
  // to be relative to the image (for 'split') or the page (for 'border').
  enum {
    LEFT, MIDDLE, RIGHT,
    TOP = LEFT, BOTTOM = RIGHT
  };
  const nscoord borderX[3] = {
    aBorderArea.x + 0,
    aBorderArea.x + border.left,
    aBorderArea.x + aBorderArea.width - border.right,
  };
  const nscoord borderY[3] = {
    aBorderArea.y + 0,
    aBorderArea.y + border.top,
    aBorderArea.y + aBorderArea.height - border.bottom,
  };
  const nscoord borderWidth[3] = {
    border.left,
    aBorderArea.width - border.left - border.right,
    border.right,
  };
  const nscoord borderHeight[3] = {
    border.top,
    aBorderArea.height - border.top - border.bottom,
    border.bottom,
  };

  const PRInt32 splitX[3] = {
    0,
    split.left,
    imageSize.width - split.right,
  };
  const PRInt32 splitY[3] = {
    0,
    split.top,
    imageSize.height - split.bottom,
  };
  const PRInt32 splitWidth[3] = {
    split.left,
    imageSize.width - split.left - split.right,
    split.right,
  };
  const PRInt32 splitHeight[3] = {
    split.top,
    imageSize.height - split.top - split.bottom,
    split.bottom,
  };

  // In all the 'factor' calculations below, 'border' measurements are
  // in app units but 'split' measurements are in image/CSS pixels, so
  // the factor corresponding to no additional scaling is
  // CSSPixelsToAppUnits(1), not simply 1.
  for (int i = LEFT; i <= RIGHT; i++) {
    for (int j = TOP; j <= BOTTOM; j++) {
      nsRect destArea(borderX[i], borderY[j], borderWidth[i], borderHeight[j]);
      nsIntRect subArea(splitX[i], splitY[j], splitWidth[i], splitHeight[j]);

      PRUint8 fillStyleH, fillStyleV;
      nsSize unitSize;

      if (i == MIDDLE && j == MIDDLE) {
        // css-background:
        //     The middle image's width is scaled by the same factor as the
        //     top image unless that factor is zero or infinity, in which
        //     case the scaling factor of the bottom is substituted, and
        //     failing that, the width is not scaled. The height of the
        //     middle image is scaled by the same factor as the left image
        //     unless that factor is zero or infinity, in which case the
        //     scaling factor of the right image is substituted, and failing
        //     that, the height is not scaled.
        gfxFloat hFactor, vFactor;

        if (0 < border.left && 0 < split.left)
          vFactor = gfxFloat(border.left)/split.left;
        else if (0 < border.right && 0 < split.right)
          vFactor = gfxFloat(border.right)/split.right;
        else
          vFactor = nsPresContext::CSSPixelsToAppUnits(1);

        if (0 < border.top && 0 < split.top)
          hFactor = gfxFloat(border.top)/split.top;
        else if (0 < border.bottom && 0 < split.bottom)
          hFactor = gfxFloat(border.bottom)/split.bottom;
        else
          hFactor = nsPresContext::CSSPixelsToAppUnits(1);

        unitSize.width = splitWidth[i]*hFactor;
        unitSize.height = splitHeight[j]*vFactor;
        fillStyleH = aBorderStyle.mBorderImageHFill;
        fillStyleV = aBorderStyle.mBorderImageVFill;

      } else if (i == MIDDLE) { // top, bottom
        // Sides are always stretched to the thickness of their border,
        // and stretched proportionately on the other axis.
        gfxFloat factor;
        if (0 < borderHeight[j] && 0 < splitHeight[j])
          factor = gfxFloat(borderHeight[j])/splitHeight[j];
        else
          factor = nsPresContext::CSSPixelsToAppUnits(1);

        unitSize.width = splitWidth[i]*factor;
        unitSize.height = borderHeight[j];
        fillStyleH = aBorderStyle.mBorderImageHFill;
        fillStyleV = NS_STYLE_BORDER_IMAGE_STRETCH;

      } else if (j == MIDDLE) { // left, right
        gfxFloat factor;
        if (0 < borderWidth[i] && 0 < splitWidth[i])
          factor = gfxFloat(borderWidth[i])/splitWidth[i];
        else
          factor = nsPresContext::CSSPixelsToAppUnits(1);

        unitSize.width = borderWidth[i];
        unitSize.height = splitHeight[j]*factor;
        fillStyleH = NS_STYLE_BORDER_IMAGE_STRETCH;
        fillStyleV = aBorderStyle.mBorderImageVFill;

      } else {
        // Corners are always stretched to fit the corner.
        unitSize.width = borderWidth[i];
        unitSize.height = borderHeight[j];
        fillStyleH = NS_STYLE_BORDER_IMAGE_STRETCH;
        fillStyleV = NS_STYLE_BORDER_IMAGE_STRETCH;
      }

      DrawBorderImageComponent(aRenderingContext, aForFrame,
                               imgContainer, aDirtyRect,
                               destArea, subArea,
                               fillStyleH, fillStyleV, unitSize);
    }
  }
}

static void
DrawBorderImageComponent(nsIRenderingContext& aRenderingContext,
                         nsIFrame*            aForFrame,
                         imgIContainer*       aImage,
                         const nsRect&        aDirtyRect,
                         const nsRect&        aFill,
                         const nsIntRect&     aSrc,
                         PRUint8              aHFill,
                         PRUint8              aVFill,
                         const nsSize&        aUnitSize)
{
  if (aFill.IsEmpty() || aSrc.IsEmpty())
    return;

  nsCOMPtr<imgIContainer> subImage;
  if (NS_FAILED(aImage->ExtractFrame(imgIContainer::FRAME_CURRENT, aSrc,
                                     imgIContainer::FLAG_SYNC_DECODE,
                                     getter_AddRefs(subImage))))
    return;

  gfxPattern::GraphicsFilter graphicsFilter =
    nsLayoutUtils::GetGraphicsFilterForFrame(aForFrame);

  // If we have no tiling in either direction, we can skip the intermediate
  // scaling step.
  if ((aHFill == NS_STYLE_BORDER_IMAGE_STRETCH &&
       aVFill == NS_STYLE_BORDER_IMAGE_STRETCH) ||
      (aUnitSize.width == aFill.width &&
       aUnitSize.height == aFill.height)) {
    nsLayoutUtils::DrawSingleImage(&aRenderingContext, subImage,
                                   graphicsFilter,
                                   aFill, aDirtyRect, imgIContainer::FLAG_NONE);
    return;
  }

  // Compute the scale and position of the master copy of the image.
  nsRect tile;
  switch (aHFill) {
  case NS_STYLE_BORDER_IMAGE_STRETCH:
    tile.x = aFill.x;
    tile.width = aFill.width;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT:
    tile.x = aFill.x + aFill.width/2 - aUnitSize.width/2;
    tile.width = aUnitSize.width;
    break;

  case NS_STYLE_BORDER_IMAGE_ROUND:
    tile.x = aFill.x;
    tile.width = aFill.width / ceil(gfxFloat(aFill.width)/aUnitSize.width);
    break;

  default:
    NS_NOTREACHED("unrecognized border-image fill style");
  }

  switch (aVFill) {
  case NS_STYLE_BORDER_IMAGE_STRETCH:
    tile.y = aFill.y;
    tile.height = aFill.height;
    break;
  case NS_STYLE_BORDER_IMAGE_REPEAT:
    tile.y = aFill.y + aFill.height/2 - aUnitSize.height/2;
    tile.height = aUnitSize.height;
    break;

  case NS_STYLE_BORDER_IMAGE_ROUND:
    tile.y = aFill.y;
    tile.height = aFill.height/ceil(gfxFloat(aFill.height)/aUnitSize.height);
    break;

  default:
    NS_NOTREACHED("unrecognized border-image fill style");
  }

  nsLayoutUtils::DrawImage(&aRenderingContext, subImage, graphicsFilter,
                           tile, aFill, tile.TopLeft(), aDirtyRect,
                           imgIContainer::FLAG_NONE);
}

// Begin table border-collapsing section
// These functions were written to not disrupt the normal ones and yet satisfy some additional requirements
// At some point, all functions should be unified to include the additional functionality that these provide

static nscoord
RoundIntToPixel(nscoord aValue, 
                nscoord aTwipsPerPixel,
                PRBool  aRoundDown = PR_FALSE)
{
  if (aTwipsPerPixel <= 0) 
    // We must be rendering to a device that has a resolution greater than Twips! 
    // In that case, aValue is as accurate as it's going to get.
    return aValue; 

  nscoord halfPixel = NSToCoordRound(aTwipsPerPixel / 2.0f);
  nscoord extra = aValue % aTwipsPerPixel;
  nscoord finalValue = (!aRoundDown && (extra >= halfPixel)) ? aValue + (aTwipsPerPixel - extra) : aValue - extra;
  return finalValue;
}

static nscoord
RoundFloatToPixel(float   aValue, 
                  nscoord aTwipsPerPixel,
                  PRBool  aRoundDown = PR_FALSE)
{
  return RoundIntToPixel(NSToCoordRound(aValue), aTwipsPerPixel, aRoundDown);
}

static void
SetPoly(const nsRect& aRect,
        nsPoint*      poly)
{
  poly[0].x = aRect.x;
  poly[0].y = aRect.y;
  poly[1].x = aRect.x + aRect.width;
  poly[1].y = aRect.y;
  poly[2].x = aRect.x + aRect.width;
  poly[2].y = aRect.y + aRect.height;
  poly[3].x = aRect.x;
  poly[3].y = aRect.y + aRect.height;
  poly[4].x = aRect.x;
  poly[4].y = aRect.y;
}
          
static void 
DrawSolidBorderSegment(nsIRenderingContext& aContext,
                       nsRect               aRect,
                       nscoord              aTwipsPerPixel,
                       PRUint8              aStartBevelSide = 0,
                       nscoord              aStartBevelOffset = 0,
                       PRUint8              aEndBevelSide = 0,
                       nscoord              aEndBevelOffset = 0)
{

  if ((aRect.width == aTwipsPerPixel) || (aRect.height == aTwipsPerPixel) ||
      ((0 == aStartBevelOffset) && (0 == aEndBevelOffset))) {
    // simple line or rectangle
    if ((NS_SIDE_TOP == aStartBevelSide) || (NS_SIDE_BOTTOM == aStartBevelSide)) {
      if (1 == aRect.height) 
        aContext.DrawLine(aRect.x, aRect.y, aRect.x, aRect.y + aRect.height); 
      else 
        aContext.FillRect(aRect);
    }
    else {
      if (1 == aRect.width) 
        aContext.DrawLine(aRect.x, aRect.y, aRect.x + aRect.width, aRect.y); 
      else 
        aContext.FillRect(aRect);
    }
  }
  else {
    // polygon with beveling
    nsPoint poly[5];
    SetPoly(aRect, poly);
    switch(aStartBevelSide) {
    case NS_SIDE_TOP:
      poly[0].x += aStartBevelOffset;
      poly[4].x = poly[0].x;
      break;
    case NS_SIDE_BOTTOM:
      poly[3].x += aStartBevelOffset;
      break;
    case NS_SIDE_RIGHT:
      poly[1].y += aStartBevelOffset;
      break;
    case NS_SIDE_LEFT:
      poly[0].y += aStartBevelOffset;
      poly[4].y = poly[0].y;
    }

    switch(aEndBevelSide) {
    case NS_SIDE_TOP:
      poly[1].x -= aEndBevelOffset;
      break;
    case NS_SIDE_BOTTOM:
      poly[2].x -= aEndBevelOffset;
      break;
    case NS_SIDE_RIGHT:
      poly[2].y -= aEndBevelOffset;
      break;
    case NS_SIDE_LEFT:
      poly[3].y -= aEndBevelOffset;
    }

    aContext.FillPolygon(poly, 5);
  }


}

static void
GetDashInfo(nscoord  aBorderLength,
            nscoord  aDashLength,
            nscoord  aTwipsPerPixel,
            PRInt32& aNumDashSpaces,
            nscoord& aStartDashLength,
            nscoord& aEndDashLength)
{
  aNumDashSpaces = 0;
  if (aStartDashLength + aDashLength + aEndDashLength >= aBorderLength) {
    aStartDashLength = aBorderLength;
    aEndDashLength = 0;
  }
  else {
    aNumDashSpaces = (aBorderLength - aDashLength)/ (2 * aDashLength); // round down
    nscoord extra = aBorderLength - aStartDashLength - aEndDashLength - (((2 * aNumDashSpaces) - 1) * aDashLength);
    if (extra > 0) {
      nscoord half = RoundIntToPixel(extra / 2, aTwipsPerPixel);
      aStartDashLength += half;
      aEndDashLength += (extra - half);
    }
  }
}

void 
nsCSSRendering::DrawTableBorderSegment(nsIRenderingContext&     aContext,
                                       PRUint8                  aBorderStyle,  
                                       nscolor                  aBorderColor,
                                       const nsStyleBackground* aBGColor,
                                       const nsRect&            aBorder,
                                       PRInt32                  aAppUnitsPerCSSPixel,
                                       PRUint8                  aStartBevelSide,
                                       nscoord                  aStartBevelOffset,
                                       PRUint8                  aEndBevelSide,
                                       nscoord                  aEndBevelOffset)
{
  aContext.SetColor (aBorderColor); 

  PRBool horizontal = ((NS_SIDE_TOP == aStartBevelSide) || (NS_SIDE_BOTTOM == aStartBevelSide));
  nscoord twipsPerPixel = NSIntPixelsToAppUnits(1, aAppUnitsPerCSSPixel);
  PRUint8 ridgeGroove = NS_STYLE_BORDER_STYLE_RIDGE;

  if ((twipsPerPixel >= aBorder.width) || (twipsPerPixel >= aBorder.height) ||
      (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle) || (NS_STYLE_BORDER_STYLE_DOTTED == aBorderStyle)) {
    // no beveling for 1 pixel border, dash or dot
    aStartBevelOffset = 0;
    aEndBevelOffset = 0;
  }

  gfxContext *ctx = aContext.ThebesContext();
  gfxContext::AntialiasMode oldMode = ctx->CurrentAntialiasMode();
  ctx->SetAntialiasMode(gfxContext::MODE_ALIASED);

  switch (aBorderStyle) {
  case NS_STYLE_BORDER_STYLE_NONE:
  case NS_STYLE_BORDER_STYLE_HIDDEN:
    //NS_ASSERTION(PR_FALSE, "style of none or hidden");
    break;
  case NS_STYLE_BORDER_STYLE_DOTTED:
  case NS_STYLE_BORDER_STYLE_DASHED: 
    {
      nscoord dashLength = (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle) ? DASH_LENGTH : DOT_LENGTH;
      // make the dash length proportional to the border thickness
      dashLength *= (horizontal) ? aBorder.height : aBorder.width;
      // make the min dash length for the ends 1/2 the dash length
      nscoord minDashLength = (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle) 
                              ? RoundFloatToPixel(((float)dashLength) / 2.0f, twipsPerPixel) : dashLength;
      minDashLength = NS_MAX(minDashLength, twipsPerPixel);
      nscoord numDashSpaces = 0;
      nscoord startDashLength = minDashLength;
      nscoord endDashLength   = minDashLength;
      if (horizontal) {
        GetDashInfo(aBorder.width, dashLength, twipsPerPixel, numDashSpaces, startDashLength, endDashLength);
        nsRect rect(aBorder.x, aBorder.y, startDashLength, aBorder.height);
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel);
        for (PRInt32 spaceX = 0; spaceX < numDashSpaces; spaceX++) {
          rect.x += rect.width + dashLength;
          rect.width = (spaceX == (numDashSpaces - 1)) ? endDashLength : dashLength;
          DrawSolidBorderSegment(aContext, rect, twipsPerPixel);
        }
      }
      else {
        GetDashInfo(aBorder.height, dashLength, twipsPerPixel, numDashSpaces, startDashLength, endDashLength);
        nsRect rect(aBorder.x, aBorder.y, aBorder.width, startDashLength);
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel);
        for (PRInt32 spaceY = 0; spaceY < numDashSpaces; spaceY++) {
          rect.y += rect.height + dashLength;
          rect.height = (spaceY == (numDashSpaces - 1)) ? endDashLength : dashLength;
          DrawSolidBorderSegment(aContext, rect, twipsPerPixel);
        }
      }
    }
    break;                                  
  case NS_STYLE_BORDER_STYLE_GROOVE:
    ridgeGroove = NS_STYLE_BORDER_STYLE_GROOVE; // and fall through to ridge
  case NS_STYLE_BORDER_STYLE_RIDGE:
    if ((horizontal && (twipsPerPixel >= aBorder.height)) ||
        (!horizontal && (twipsPerPixel >= aBorder.width))) {
      // a one pixel border
      DrawSolidBorderSegment(aContext, aBorder, twipsPerPixel, aStartBevelSide, aStartBevelOffset,
                             aEndBevelSide, aEndBevelOffset);
    }
    else {
      nscoord startBevel = (aStartBevelOffset > 0) 
                            ? RoundFloatToPixel(0.5f * (float)aStartBevelOffset, twipsPerPixel, PR_TRUE) : 0;
      nscoord endBevel =   (aEndBevelOffset > 0) 
                            ? RoundFloatToPixel(0.5f * (float)aEndBevelOffset, twipsPerPixel, PR_TRUE) : 0;
      PRUint8 ridgeGrooveSide = (horizontal) ? NS_SIDE_TOP : NS_SIDE_LEFT;
      aContext.SetColor ( 
        MakeBevelColor(ridgeGrooveSide, ridgeGroove, aBGColor->mBackgroundColor, aBorderColor));
      nsRect rect(aBorder);
      nscoord half;
      if (horizontal) { // top, bottom
        half = RoundFloatToPixel(0.5f * (float)aBorder.height, twipsPerPixel);
        rect.height = half;
        if (NS_SIDE_TOP == aStartBevelSide) {
          rect.x += startBevel;
          rect.width -= startBevel;
        }
        if (NS_SIDE_TOP == aEndBevelSide) {
          rect.width -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }
      else { // left, right
        half = RoundFloatToPixel(0.5f * (float)aBorder.width, twipsPerPixel);
        rect.width = half;
        if (NS_SIDE_LEFT == aStartBevelSide) {
          rect.y += startBevel;
          rect.height -= startBevel;
        }
        if (NS_SIDE_LEFT == aEndBevelSide) {
          rect.height -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }

      rect = aBorder;
      ridgeGrooveSide = (NS_SIDE_TOP == ridgeGrooveSide) ? NS_SIDE_BOTTOM : NS_SIDE_RIGHT;
      aContext.SetColor ( 
        MakeBevelColor(ridgeGrooveSide, ridgeGroove, aBGColor->mBackgroundColor, aBorderColor));
      if (horizontal) {
        rect.y = rect.y + half;
        rect.height = aBorder.height - half;
        if (NS_SIDE_BOTTOM == aStartBevelSide) {
          rect.x += startBevel;
          rect.width -= startBevel;
        }
        if (NS_SIDE_BOTTOM == aEndBevelSide) {
          rect.width -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }
      else {
        rect.x = rect.x + half;
        rect.width = aBorder.width - half;
        if (NS_SIDE_RIGHT == aStartBevelSide) {
          rect.y += aStartBevelOffset - startBevel;
          rect.height -= startBevel;
        }
        if (NS_SIDE_RIGHT == aEndBevelSide) {
          rect.height -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }
    }
    break;
  case NS_STYLE_BORDER_STYLE_DOUBLE:
    if ((aBorder.width > 2) && (aBorder.height > 2)) {
      nscoord startBevel = (aStartBevelOffset > 0) 
                            ? RoundFloatToPixel(0.333333f * (float)aStartBevelOffset, twipsPerPixel) : 0;
      nscoord endBevel =   (aEndBevelOffset > 0) 
                            ? RoundFloatToPixel(0.333333f * (float)aEndBevelOffset, twipsPerPixel) : 0;
      if (horizontal) { // top, bottom
        nscoord thirdHeight = RoundFloatToPixel(0.333333f * (float)aBorder.height, twipsPerPixel);

        // draw the top line or rect
        nsRect topRect(aBorder.x, aBorder.y, aBorder.width, thirdHeight);
        if (NS_SIDE_TOP == aStartBevelSide) {
          topRect.x += aStartBevelOffset - startBevel;
          topRect.width -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_TOP == aEndBevelSide) {
          topRect.width -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, topRect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);

        // draw the botom line or rect
        nscoord heightOffset = aBorder.height - thirdHeight; 
        nsRect bottomRect(aBorder.x, aBorder.y + heightOffset, aBorder.width, aBorder.height - heightOffset);
        if (NS_SIDE_BOTTOM == aStartBevelSide) {
          bottomRect.x += aStartBevelOffset - startBevel;
          bottomRect.width -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_BOTTOM == aEndBevelSide) {
          bottomRect.width -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, bottomRect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }
      else { // left, right
        nscoord thirdWidth = RoundFloatToPixel(0.333333f * (float)aBorder.width, twipsPerPixel);

        nsRect leftRect(aBorder.x, aBorder.y, thirdWidth, aBorder.height); 
        if (NS_SIDE_LEFT == aStartBevelSide) {
          leftRect.y += aStartBevelOffset - startBevel;
          leftRect.height -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_LEFT == aEndBevelSide) {
          leftRect.height -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, leftRect, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);

        nscoord widthOffset = aBorder.width - thirdWidth; 
        nsRect rightRect(aBorder.x + widthOffset, aBorder.y, aBorder.width - widthOffset, aBorder.height);
        if (NS_SIDE_RIGHT == aStartBevelSide) {
          rightRect.y += aStartBevelOffset - startBevel;
          rightRect.height -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_RIGHT == aEndBevelSide) {
          rightRect.height -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, rightRect, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);
      }
      break;
    }
    // else fall through to solid
  case NS_STYLE_BORDER_STYLE_SOLID:
    DrawSolidBorderSegment(aContext, aBorder, twipsPerPixel, aStartBevelSide, 
                           aStartBevelOffset, aEndBevelSide, aEndBevelOffset);
    break;
  case NS_STYLE_BORDER_STYLE_OUTSET:
  case NS_STYLE_BORDER_STYLE_INSET:
    NS_ASSERTION(PR_FALSE, "inset, outset should have been converted to groove, ridge");
    break;
  case NS_STYLE_BORDER_STYLE_AUTO:
    NS_ASSERTION(PR_FALSE, "Unexpected 'auto' table border");
    break;
  }

  ctx->SetAntialiasMode(oldMode);
}

// End table border-collapsing section

void
nsCSSRendering::PaintDecorationLine(gfxContext* aGfxContext,
                                    const nscolor aColor,
                                    const gfxPoint& aPt,
                                    const gfxSize& aLineSize,
                                    const gfxFloat aAscent,
                                    const gfxFloat aOffset,
                                    const PRUint8 aDecoration,
                                    const PRUint8 aStyle,
                                    const gfxFloat aDescentLimit)
{
  NS_ASSERTION(aStyle != DECORATION_STYLE_NONE, "aStyle is none");

  gfxRect rect =
    GetTextDecorationRectInternal(aPt, aLineSize, aAscent, aOffset,
                                  aDecoration, aStyle, aDescentLimit);
  if (rect.IsEmpty())
    return;

  if (aDecoration != NS_STYLE_TEXT_DECORATION_UNDERLINE &&
      aDecoration != NS_STYLE_TEXT_DECORATION_OVERLINE &&
      aDecoration != NS_STYLE_TEXT_DECORATION_LINE_THROUGH)
  {
    NS_ERROR("Invalid decoration value!");
    return;
  }

  gfxFloat lineHeight = NS_MAX(NS_round(aLineSize.height), 1.0);
  PRBool contextIsSaved = PR_FALSE;

  gfxFloat oldLineWidth;
  nsRefPtr<gfxPattern> oldPattern;

  switch (aStyle) {
    case DECORATION_STYLE_SOLID:
    case DECORATION_STYLE_DOUBLE:
      oldLineWidth = aGfxContext->CurrentLineWidth();
      oldPattern = aGfxContext->GetPattern();
      break;
    case DECORATION_STYLE_DASHED: {
      aGfxContext->Save();
      contextIsSaved = PR_TRUE;
      aGfxContext->Clip(rect);
      gfxFloat dashWidth = lineHeight * DOT_LENGTH * DASH_LENGTH;
      gfxFloat dash[2] = { dashWidth, dashWidth };
      aGfxContext->SetLineCap(gfxContext::LINE_CAP_BUTT);
      aGfxContext->SetDash(dash, 2, 0.0);
      // We should continue to draw the last dash even if it is not in the rect.
      rect.size.width += dashWidth;
      break;
    }
    case DECORATION_STYLE_DOTTED: {
      aGfxContext->Save();
      contextIsSaved = PR_TRUE;
      aGfxContext->Clip(rect);
      gfxFloat dashWidth = lineHeight * DOT_LENGTH;
      gfxFloat dash[2];
      if (lineHeight > 2.0) {
        dash[0] = 0.0;
        dash[1] = dashWidth * 2.0;
        aGfxContext->SetLineCap(gfxContext::LINE_CAP_ROUND);
      } else {
        dash[0] = dashWidth;
        dash[1] = dashWidth;
      }
      aGfxContext->SetDash(dash, 2, 0.0);
      // We should continue to draw the last dot even if it is not in the rect.
      rect.size.width += dashWidth;
      break;
    }
    case DECORATION_STYLE_WAVY:
      aGfxContext->Save();
      contextIsSaved = PR_TRUE;
      aGfxContext->Clip(rect);
      if (lineHeight > 2.0) {
        aGfxContext->SetAntialiasMode(gfxContext::MODE_COVERAGE);
      } else {
        // Don't use anti-aliasing here.  Because looks like lighter color wavy
        // line at this case.  And probably, users don't think the
        // non-anti-aliased wavy line is not pretty.
        aGfxContext->SetAntialiasMode(gfxContext::MODE_ALIASED);
      }
      break;
    default:
      NS_ERROR("Invalid style value!");
      return;
  }

  // The y position should be set to the middle of the line.
  rect.pos.y += lineHeight / 2;

  aGfxContext->SetColor(gfxRGBA(aColor));
  aGfxContext->SetLineWidth(lineHeight);
  switch (aStyle) {
    case DECORATION_STYLE_SOLID:
      aGfxContext->NewPath();
      aGfxContext->MoveTo(rect.TopLeft());
      aGfxContext->LineTo(rect.TopRight());
      aGfxContext->Stroke();
      break;
    case DECORATION_STYLE_DOUBLE:
      /**
       *  We are drawing double line as:
       *
       * +-------------------------------------------+
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| ^
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| | lineHeight
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| v
       * |                                           |
       * |                                           |
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| ^
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| | lineHeight
       * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| v
       * +-------------------------------------------+
       */
      aGfxContext->NewPath();
      aGfxContext->MoveTo(rect.TopLeft());
      aGfxContext->LineTo(rect.TopRight());
      rect.size.height -= lineHeight;
      aGfxContext->MoveTo(rect.BottomLeft());
      aGfxContext->LineTo(rect.BottomRight());
      aGfxContext->Stroke();
      break;
    case DECORATION_STYLE_DOTTED:
    case DECORATION_STYLE_DASHED:
      aGfxContext->NewPath();
      aGfxContext->MoveTo(rect.TopLeft());
      aGfxContext->LineTo(rect.TopRight());
      aGfxContext->Stroke();
      break;
    case DECORATION_STYLE_WAVY: {
      /**
       *  We are drawing wavy line as:
       *
       *  P: Path, X: Painted pixel
       *
       *     +---------------------------------------+
       *   XX|X            XXXXXX            XXXXXX  |
       *   PP|PX          XPPPPPPX          XPPPPPPX |    ^
       *   XX|XPX        XPXXXXXXPX        XPXXXXXXPX|    |
       *     | XPX      XPX      XPX      XPX      XP|X   |adv
       *     |  XPXXXXXXPX        XPXXXXXXPX        X|PX  |
       *     |   XPPPPPPX          XPPPPPPX          |XPX v
       *     |    XXXXXX            XXXXXX           | XX
       *     +---------------------------------------+
       *      <---><--->                                ^
       *      adv  flatLengthAtVertex                   rightMost
       *
       *  1. Always starts from top-left of the drawing area, however, we need
       *     to draw  the line from outside of the rect.  Because the start
       *     point of the line is not good style if we draw from inside it.
       *  2. First, draw horizontal line from outside the rect to top-left of
       *     the rect;
       *  3. Goes down to bottom of the area at 45 degrees.
       *  4. Slides to right horizontaly, see |flatLengthAtVertex|.
       *  5. Goes up to top of the area at 45 degrees.
       *  6. Slides to right horizontaly.
       *  7. Repeat from 2 until reached to right-most edge of the area.
       */

      rect.pos.x += lineHeight / 2.0;
      aGfxContext->NewPath();

      gfxPoint pt(rect.pos);
      gfxFloat rightMost = pt.x + rect.Width() + lineHeight;
      gfxFloat adv = rect.Height() - lineHeight;
      gfxFloat flatLengthAtVertex = NS_MAX((lineHeight - 1.0) * 2.0, 1.0);

      pt.x -= lineHeight;
      aGfxContext->MoveTo(pt); // 1

      pt.x = rect.pos.x;
      aGfxContext->LineTo(pt); // 2

      PRBool goDown = PR_TRUE;
      while (pt.x < rightMost) {
        pt.x += adv;
        pt.y += goDown ? adv : -adv;

        aGfxContext->LineTo(pt); // 3 and 5

        pt.x += flatLengthAtVertex;
        aGfxContext->LineTo(pt); // 4 and 6

        goDown = !goDown;
      }
      aGfxContext->Stroke();
      break;
    }
    default:
      NS_ERROR("Invalid style value!");
      break;
  }

  if (contextIsSaved) {
    aGfxContext->Restore();
  } else {
    aGfxContext->SetPattern(oldPattern);
    aGfxContext->SetLineWidth(oldLineWidth);
  }
}

nsRect
nsCSSRendering::GetTextDecorationRect(nsPresContext* aPresContext,
                                      const gfxSize& aLineSize,
                                      const gfxFloat aAscent,
                                      const gfxFloat aOffset,
                                      const PRUint8 aDecoration,
                                      const PRUint8 aStyle,
                                      const gfxFloat aDescentLimit)
{
  NS_ASSERTION(aPresContext, "aPresContext is null");
  NS_ASSERTION(aStyle != DECORATION_STYLE_NONE, "aStyle is none");

  gfxRect rect =
    GetTextDecorationRectInternal(gfxPoint(0, 0), aLineSize, aAscent, aOffset,
                                  aDecoration, aStyle, aDescentLimit);
  // The rect values are already rounded to nearest device pixels.
  nsRect r;
  r.x = aPresContext->GfxUnitsToAppUnits(rect.X());
  r.y = aPresContext->GfxUnitsToAppUnits(rect.Y());
  r.width = aPresContext->GfxUnitsToAppUnits(rect.Width());
  r.height = aPresContext->GfxUnitsToAppUnits(rect.Height());
  return r;
}

gfxRect
nsCSSRendering::GetTextDecorationRectInternal(const gfxPoint& aPt,
                                              const gfxSize& aLineSize,
                                              const gfxFloat aAscent,
                                              const gfxFloat aOffset,
                                              const PRUint8 aDecoration,
                                              const PRUint8 aStyle,
                                              const gfxFloat aDescentLimit)
{
  NS_ASSERTION(aStyle <= DECORATION_STYLE_WAVY, "Invalid aStyle value");

  if (aStyle == DECORATION_STYLE_NONE)
    return gfxRect(0, 0, 0, 0);

  PRBool canLiftUnderline = aDescentLimit >= 0.0;

  gfxRect r;
  r.pos.x = NS_floor(aPt.x + 0.5);
  r.size.width = NS_round(aLineSize.width);

  gfxFloat lineHeight = NS_round(aLineSize.height);
  lineHeight = NS_MAX(lineHeight, 1.0);

  gfxFloat ascent = NS_round(aAscent);
  gfxFloat descentLimit = NS_floor(aDescentLimit);

  gfxFloat suggestedMaxRectHeight = NS_MAX(NS_MIN(ascent, descentLimit), 1.0);
  r.size.height = lineHeight;
  if (aStyle == DECORATION_STYLE_DOUBLE) {
    /**
     *  We will draw double line as:
     *
     * +-------------------------------------------+
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| ^
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| | lineHeight
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| v
     * |                                           | ^
     * |                                           | | gap
     * |                                           | v
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| ^
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| | lineHeight
     * |XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX| v
     * +-------------------------------------------+
     */
    gfxFloat gap = NS_round(lineHeight / 2.0);
    gap = NS_MAX(gap, 1.0);
    r.size.height = lineHeight * 2.0 + gap;
    if (canLiftUnderline) {
      if (r.Height() > suggestedMaxRectHeight) {
        // Don't shrink the line height, because the thickness has some meaning.
        // We can just shrink the gap at this time.
        r.size.height = NS_MAX(suggestedMaxRectHeight, lineHeight * 2.0 + 1.0);
      }
    }
  } else if (aStyle == DECORATION_STYLE_WAVY) {
    /**
     *  We will draw wavy line as:
     *
     * +-------------------------------------------+
     * |XXXXX            XXXXXX            XXXXXX  | ^
     * |XXXXXX          XXXXXXXX          XXXXXXXX | | lineHeight
     * |XXXXXXX        XXXXXXXXXX        XXXXXXXXXX| v
     * |     XXX      XXX      XXX      XXX      XX|
     * |      XXXXXXXXXX        XXXXXXXXXX        X|
     * |       XXXXXXXX          XXXXXXXX          |
     * |        XXXXXX            XXXXXX           |
     * +-------------------------------------------+
     */
    r.size.height = lineHeight > 2.0 ? lineHeight * 4.0 : lineHeight * 3.0;
    if (canLiftUnderline) {
      if (r.Height() > suggestedMaxRectHeight) {
        // Don't shrink the line height even if there is not enough space,
        // because the thickness has some meaning.  E.g., the 1px wavy line and
        // 2px wavy line can be used for different meaning in IME selections
        // at same time.
        r.size.height = NS_MAX(suggestedMaxRectHeight, lineHeight * 2.0);
      }
    }
  }

  gfxFloat baseline = NS_floor(aPt.y + aAscent + 0.5);
  gfxFloat offset = 0.0;
  switch (aDecoration) {
    case NS_STYLE_TEXT_DECORATION_UNDERLINE:
      offset = aOffset;
      if (canLiftUnderline) {
        if (descentLimit < -offset + r.Height()) {
          // If we can ignore the offset and the decoration line is overflowing,
          // we should align the bottom edge of the decoration line rect if it's
          // possible.  Otherwise, we should lift up the top edge of the rect as
          // far as possible.
          gfxFloat offsetBottomAligned = -descentLimit + r.Height();
          gfxFloat offsetTopAligned = 0.0;
          offset = NS_MIN(offsetBottomAligned, offsetTopAligned);
        }
      }
      break;
    case NS_STYLE_TEXT_DECORATION_OVERLINE:
      offset = aOffset - lineHeight + r.Height();
      break;
    case NS_STYLE_TEXT_DECORATION_LINE_THROUGH: {
      gfxFloat extra = NS_floor(r.Height() / 2.0 + 0.5);
      extra = NS_MAX(extra, lineHeight);
      offset = aOffset - lineHeight + extra;
      break;
    }
    default:
      NS_ERROR("Invalid decoration value!");
  }
  r.pos.y = baseline - NS_floor(offset + 0.5);
  return r;
}

// ------------------
// ImageRenderer
// ------------------
ImageRenderer::ImageRenderer(nsIFrame* aForFrame,
                                       const nsStyleImage& aImage,
                                       PRUint32 aFlags)
  : mForFrame(aForFrame)
  , mImage(aImage)
  , mType(aImage.GetType())
  , mImageContainer(nsnull)
  , mGradientData(nsnull)
  , mIsReady(PR_FALSE)
  , mSize(0, 0)
  , mFlags(aFlags)
{
}

ImageRenderer::~ImageRenderer()
{
}

PRBool
ImageRenderer::PrepareImage()
{
  if (mImage.IsEmpty() || !mImage.IsComplete()) {
    // Make sure the image is actually decoding
    mImage.RequestDecode();

    // We can not prepare the image for rendering if it is not fully loaded.
    // 
    // Special case: If we requested a sync decode and we have an image, push
    // on through
    nsCOMPtr<imgIContainer> img;
    if (!((mFlags & FLAG_SYNC_DECODE_IMAGES) &&
          (mType == eStyleImageType_Image) &&
          (NS_SUCCEEDED(mImage.GetImageData()->GetImage(getter_AddRefs(img))) && img)))
    return PR_FALSE;
  }

  switch (mType) {
    case eStyleImageType_Image:
    {
      nsCOMPtr<imgIContainer> srcImage;
      mImage.GetImageData()->GetImage(getter_AddRefs(srcImage));
      NS_ABORT_IF_FALSE(srcImage, "If srcImage is null, mImage.IsComplete() "
                                  "should have returned false");

      if (!mImage.GetCropRect()) {
        mImageContainer.swap(srcImage);
      } else {
        nsIntRect actualCropRect;
        PRBool isEntireImage;
        PRBool success =
          mImage.ComputeActualCropRect(actualCropRect, &isEntireImage);
        NS_ASSERTION(success, "ComputeActualCropRect() should not fail here");
        if (!success || actualCropRect.IsEmpty()) {
          // The cropped image has zero size
          return PR_FALSE;
        }
        if (isEntireImage) {
          // The cropped image is identical to the source image
          mImageContainer.swap(srcImage);
        } else {
          nsCOMPtr<imgIContainer> subImage;
          PRUint32 aExtractFlags = (mFlags & FLAG_SYNC_DECODE_IMAGES)
                                     ? (PRUint32) imgIContainer::FLAG_SYNC_DECODE
                                     : (PRUint32) imgIContainer::FLAG_NONE;
          nsresult rv = srcImage->ExtractFrame(imgIContainer::FRAME_CURRENT,
                                               actualCropRect, aExtractFlags,
                                               getter_AddRefs(subImage));
          if (NS_FAILED(rv)) {
            NS_WARNING("The cropped image contains no pixels to draw; "
                       "maybe the crop rect is outside the image frame rect");
            return PR_FALSE;
          }
          mImageContainer.swap(subImage);
        }
      }
      mIsReady = PR_TRUE;
      break;
    }
    case eStyleImageType_Gradient:
      mGradientData = mImage.GetGradientData();
      mIsReady = PR_TRUE;
      break;
    case eStyleImageType_Null:
    default:
      break;
  }

  return mIsReady;
}

nsSize
ImageRenderer::ComputeSize(const nsSize& aDefault)
{
  NS_ASSERTION(mIsReady, "Ensure PrepareImage() has returned true "
                         "before calling me");

  switch (mType) {
    case eStyleImageType_Image:
    {
      nsIntSize imageIntSize;
      mImageContainer->GetWidth(&imageIntSize.width);
      mImageContainer->GetHeight(&imageIntSize.height);

      mSize.width = nsPresContext::CSSPixelsToAppUnits(imageIntSize.width);
      mSize.height = nsPresContext::CSSPixelsToAppUnits(imageIntSize.height);
      break;
    }
    case eStyleImageType_Gradient:
      mSize = aDefault;
      break;
    case eStyleImageType_Null:
    default:
      mSize.SizeTo(0, 0);
      break;
  }

  return mSize;
}

void
ImageRenderer::Draw(nsPresContext*       aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDest,
                         const nsRect&        aFill,
                         const nsPoint&       aAnchor,
                         const nsRect&        aDirty,
                         PRBool               aRepeat)
{
  if (!mIsReady) {
    NS_NOTREACHED("Ensure PrepareImage() has returned true before calling me");
    return;
  }

  if (aDest.IsEmpty() || aFill.IsEmpty())
    return;

  switch (mType) {
    case eStyleImageType_Image:
    {
      PRUint32 drawFlags = (mFlags & FLAG_SYNC_DECODE_IMAGES)
                             ? (PRUint32) imgIContainer::FLAG_SYNC_DECODE
                             : (PRUint32) imgIContainer::FLAG_NONE;
      nsLayoutUtils::DrawImage(&aRenderingContext, mImageContainer,
          nsLayoutUtils::GetGraphicsFilterForFrame(mForFrame),
          aDest, aFill, aAnchor, aDirty, drawFlags);
      break;
    }
    case eStyleImageType_Gradient:
      nsCSSRendering::PaintGradient(aPresContext, aRenderingContext,
          mGradientData, aDirty, aDest, aFill, aRepeat);
      break;
    case eStyleImageType_Null:
    default:
      break;
  }
}

// -----
// nsContextBoxBlur
// -----
gfxContext*
nsContextBoxBlur::Init(const gfxRect& aRect, nscoord aBlurRadius,
                       PRInt32 aAppUnitsPerDevPixel,
                       gfxContext* aDestinationCtx,
                       const gfxRect& aDirtyRect)
{
  mDestinationCtx = aDestinationCtx;

  PRInt32 blurRadius = static_cast<PRInt32>(aBlurRadius / aAppUnitsPerDevPixel);

  // if not blurring, draw directly onto the destination device
  if (blurRadius <= 0) {
    mContext = aDestinationCtx;
    return mContext;
  }

  // Convert from app units to device pixels
  gfxRect rect = aRect;
  rect.ScaleInverse(aAppUnitsPerDevPixel);

  if (rect.IsEmpty()) {
    mContext = aDestinationCtx;
    return mContext;
  }

  gfxRect dirtyRect = aDirtyRect;
  dirtyRect.ScaleInverse(aAppUnitsPerDevPixel);

  mDestinationCtx = aDestinationCtx;

  // Create the temporary surface for blurring
  mContext = blur.Init(rect, gfxIntSize(blurRadius, blurRadius), &dirtyRect);
  return mContext;
}

void
nsContextBoxBlur::DoPaint()
{
  if (mContext == mDestinationCtx)
    return;

  blur.Paint(mDestinationCtx);
}

gfxContext*
nsContextBoxBlur::GetContext()
{
  return mContext;
}

