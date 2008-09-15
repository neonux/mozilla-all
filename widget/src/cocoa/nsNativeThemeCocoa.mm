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
 * Mike Pinkerton (pinkerton@netscape.com).
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Vladimir Vukicevic <vladimir@pobox.com> (HITheme rewrite)
 *    Josh Aas <josh@mozilla.com>
 *    Colin Barrett <cbarrett@mozilla.com>
 *    Matthew Gregan <kinetik@flim.org>
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

#include "nsNativeThemeCocoa.h"
#include "nsObjCExceptions.h"
#include "nsIRenderingContext.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsThemeConstants.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIAtom.h"
#include "nsIEventStateManager.h"
#include "nsINameSpaceManager.h"
#include "nsPresContext.h"
#include "nsILookAndFeel.h"
#include "nsWidgetAtoms.h"
#include "nsToolkit.h"

#include "gfxContext.h"
#include "gfxQuartzSurface.h"
#include "gfxQuartzNativeDrawing.h"

#define DRAW_IN_FRAME_DEBUG 0
#define SCROLLBARS_VISUAL_DEBUG 0

// private Quartz routines needed here
extern "C" {
  CG_EXTERN void CGContextSetCTM(CGContextRef, CGAffineTransform);
}

// Copied from nsLookAndFeel.h
// Apple hasn't defined a constant for scollbars with two arrows on each end, so we'll use this one.
static const int kThemeScrollBarArrowsBoth = 2;

#define HITHEME_ORIENTATION kHIThemeOrientationNormal
#define MAX_FOCUS_RING_WIDTH 4

// These enums are for indexing into the margin array.
enum {
  tigerOS,
  leopardOS
};

enum {
  miniControlSize,
  smallControlSize,
  regularControlSize
};

enum {
  leftMargin,
  topMargin,
  rightMargin,
  bottomMargin
};

static int EnumSizeForCocoaSize(NSControlSize cocoaControlSize) {
  if (cocoaControlSize == NSMiniControlSize)
    return miniControlSize;
  else if (cocoaControlSize == NSSmallControlSize)
    return smallControlSize;
  else
    return regularControlSize;
}

static void InflateControlRect(NSRect* rect, NSControlSize cocoaControlSize, const float marginSet[][3][4])
{
  static int osIndex = nsToolkit::OnLeopardOrLater() ? leopardOS : tigerOS;
  int controlSize = EnumSizeForCocoaSize(cocoaControlSize);
  const float* buttonMargins = marginSet[osIndex][controlSize];
  rect->origin.x -= buttonMargins[leftMargin];
  rect->origin.y -= buttonMargins[bottomMargin];
  rect->size.width += buttonMargins[leftMargin] + buttonMargins[rightMargin];
  rect->size.height += buttonMargins[bottomMargin] + buttonMargins[topMargin];
}


NS_IMPL_ISUPPORTS1(nsNativeThemeCocoa, nsITheme)


nsNativeThemeCocoa::nsNativeThemeCocoa()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mPushButtonCell = [[NSButtonCell alloc] initTextCell:nil];
  [mPushButtonCell setButtonType:NSMomentaryPushInButton];
  [mPushButtonCell setHighlightsBy:NSPushInCellMask];

  mRadioButtonCell = [[NSButtonCell alloc] initTextCell:nil];
  [mRadioButtonCell setButtonType:NSRadioButton];
  [mRadioButtonCell setBezelStyle:NSRoundedBezelStyle];
  [mRadioButtonCell setHighlightsBy:NSPushInCellMask];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsNativeThemeCocoa::~nsNativeThemeCocoa()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mPushButtonCell release];
  [mRadioButtonCell release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsNativeThemeCocoa::DrawCheckbox(CGContextRef cgContext, ThemeButtonKind inKind,
                                 const HIRect& inBoxRect, PRBool inChecked,
                                 PRBool inDisabled, PRInt32 inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = inKind;

  if (inDisabled)
    bdi.state = kThemeStateUnavailable;
  else if ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER))
    bdi.state = kThemeStatePressed;
  else
    bdi.state = kThemeStateActive;

  bdi.value = inChecked ? kThemeButtonOn : kThemeButtonOff;
  bdi.adornment = (inState & NS_EVENT_STATE_FOCUS) ? kThemeAdornmentFocus : kThemeAdornmentNone;

  HIRect drawFrame = inBoxRect;

  // on Tiger, shift the checkbox rendering down 1px to get the frame
  // in the right spot
  if (!nsToolkit::OnLeopardOrLater() && inKind == kThemeSmallCheckBox)
    drawFrame.origin.y += 1;

  HIThemeDrawButton(&drawFrame, &bdi, cgContext, HITHEME_ORIENTATION, NULL);

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Limit on the area of destRect (in pixels^2) in DrawCellWithScaling(),
// above which we don't do any scaling.  This is to avoid crashes in
// [NSGraphicsContext graphicsContextWithGraphicsPort:flipped:] and
// CGContextDrawImage(), and also to avoid very poor drawing performance in
// CGContextDrawImage() (particularly if xscale or yscale is less than but
// near 1 -- e.g. 0.9).  This value was determined by trial and error, on
// OS X 10.4.11 and 10.5.4, and on systems with different amounts of RAM.
#define CELL_SCALING_MAX_AREA 500000

/*
 * Draw the given NSCell into the given cgContext.
 *
 * destRect - the size and position of the resulting control rectangle
 * controlSize - the NSControlSize which will be given to the NSCell before
 *  asking it to render
 * naturalWidth, naturalHeight - The natural dimensions of this control.
 *  If the control rect size is not equal to either of these, a scale
 *  will be applied to the context so that rendering the control at the
 *  natural size will result in it filling the destRect space.
 *  If a control has no natural dimensions in either/both axes, pass 0.0f.
 * minWidth, minHeight - The minimum dimensions of this control.
 *  If the control rect size is less than the minimum for a given axis,
 *  a scale will be applied to the context so that the minimum is used
 *  for drawing.  If a control has no minimum dimensions in either/both
 *  axes, pass 0.0f.
 * marginSet - an array of margins; a multidimensional array of [2][3][4],
 *  with the first two array elements being margins for Tiger or Leopard,
 *  the next three being control size (mini, small, regular), and the 4
 *  being the 4 margin values.
 * doSaveCTM - whether this routine should bother to save the CTM before
 *  manipuating; if the caller has already done this, pass PR_FALSE.
 */
void
nsNativeThemeCocoa::DrawCellWithScaling(NSCell *cell,
                                        CGContextRef cgContext,
                                        const HIRect& destRect,
                                        NSControlSize controlSize,
                                        float naturalWidth, float naturalHeight,
                                        float minWidth, float minHeight,
                                        const float marginSet[][3][4],
                                        PRBool doSaveCTM)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSView *focusView = [NSView focusView];
  // Abort if we have no place to draw (which can happen when switching back
  // to a previously loaded tab).
  if (!focusView)
    return;

  NSRect drawRect = NSMakeRect(destRect.origin.x, destRect.origin.y, destRect.size.width, destRect.size.height);

  CGAffineTransform savedCTM;
  NSGraphicsContext* savedContext = NULL;

  float xscale = 1.0f, yscale = 1.0f;

  if (naturalWidth != 0.0f) {
    xscale = drawRect.size.width / naturalWidth;
    drawRect.size.width = naturalWidth;
  }
  else if (minWidth != 0.0f &&
           drawRect.size.width < minWidth)
  {
    xscale = drawRect.size.width / minWidth;
    drawRect.size.width = minWidth;
  }

  if (naturalHeight != 0.0f) {
    yscale = drawRect.size.height / naturalHeight;
    drawRect.size.height = naturalHeight;
  }
  else if (minHeight != 0.0f &&
           drawRect.size.height < minHeight)
  {
    yscale = drawRect.size.height / minHeight;
    drawRect.size.height = minHeight;
  }

  if (doSaveCTM)
    savedCTM = CGContextGetCTM(cgContext);

  // Fall back to no scaling if the area of our cell (in pixels^2) is too large.
  if (drawRect.size.width * drawRect.size.height > CELL_SCALING_MAX_AREA)
    xscale = yscale = 1.0f;

  if (xscale == 1.0f && yscale == 1.0f) {
    // Inflate the rect Gecko gave us by the margin for the control.
    InflateControlRect(&drawRect, controlSize, marginSet);

    // Set up the graphics context we've been asked to draw to.
    savedContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES]];

    [cell drawWithFrame:drawRect inView:focusView];
  }
  else {
    float w = ceil(drawRect.size.width);
    float h = ceil(drawRect.size.height);

    NSRect tmpRect = NSMakeRect(0.0f, 0.0f, w, h);

    // inflate to figure out the frame we need to tell NSCell to draw in, to get something that's 0,0,w,h
    InflateControlRect(&tmpRect, controlSize, marginSet);

    // and then, expand by MAX_FOCUS_RING_WIDTH size to make sure we can capture any focus ring
    w += MAX_FOCUS_RING_WIDTH * 2.0;
    h += MAX_FOCUS_RING_WIDTH * 2.0;

    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(NULL,
                                             (int) w, (int) h,
                                             8, (int) w * 4,
                                             rgb, kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(rgb);

    CGContextTranslateCTM(ctx, MAX_FOCUS_RING_WIDTH, MAX_FOCUS_RING_WIDTH);

    savedContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:ctx flipped:YES]];

    [cell drawWithFrame:tmpRect inView:focusView];

    [NSGraphicsContext setCurrentContext:savedContext];

    CGImageRef img = CGBitmapContextCreateImage(ctx);

    // Drop the image into the original destination rectangle, scaling to fit
    // XXX in theory we should scale this MAX_FOCUS_RING_WIDTH here by xscale/yscale,
    // but in practice, this looks better.
    CGContextDrawImage(cgContext, CGRectMake(destRect.origin.x - MAX_FOCUS_RING_WIDTH,
                                             destRect.origin.y - MAX_FOCUS_RING_WIDTH,
                                             destRect.size.width + MAX_FOCUS_RING_WIDTH * 2,
                                             destRect.size.height + MAX_FOCUS_RING_WIDTH * 2),
                       img);

    CGImageRelease(img);
    CGContextRelease(ctx);
  }

  if (doSaveCTM)
    CGContextSetCTM(cgContext, savedCTM);

  [NSGraphicsContext setCurrentContext:savedContext];

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, destRect);
#endif

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
                                        
// These are the sizes that Gecko needs to request to draw if it wants
// to get a standard-sized Aqua radio button drawn. Note that the rects
// that draw these are actually a little bigger.
#define NATURAL_MINI_RADIO_BUTTON_WIDTH 11
#define NATURAL_MINI_RADIO_BUTTON_HEIGHT 11
#define NATURAL_SMALL_RADIO_BUTTON_WIDTH 14
#define NATURAL_SMALL_RADIO_BUTTON_HEIGHT 14
#define NATURAL_REGULAR_RADIO_BUTTON_WIDTH 16
#define NATURAL_REGULAR_RADIO_BUTTON_HEIGHT 16

// These were calculated by testing all three sizes on the respective operating system.
static const float radioButtonMargins[2][3][4] =
{
  { // Tiger
    {0, 0, 0, 0}, // mini     - if we ever use this we'll have to calculate it
    {0, 0, 0, 0}, // small    - if we ever use this we'll have to calculate it
    {0, 3, 0, -3}  // regular
  },
  { // Leopard
    {0, 4, 0, -4}, // mini
    {0, 3, 0, -3}, // small
    {0, 3, 0, -3}  // regular
  }
};

void
nsNativeThemeCocoa::DrawRadioButton(CGContextRef cgContext, const HIRect& inBoxRect, PRBool inSelected,
                                    PRBool inDisabled, PRInt32 inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSRect drawRect = NSMakeRect(inBoxRect.origin.x, inBoxRect.origin.y, inBoxRect.size.width, inBoxRect.size.height);

  [mRadioButtonCell setEnabled:!inDisabled];
  [mRadioButtonCell setShowsFirstResponder:(inState & NS_EVENT_STATE_FOCUS)];
  [mRadioButtonCell setState:(inSelected ? NSOnState : NSOffState)];
  [mRadioButtonCell setHighlighted:((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER))];

  // Always use a regular size control because for some reason NSCell doesn't respect other
  // size choices here. Maybe because of a rendering context/ctm setup it doesn't like?
  NSControlSize controlSize = NSRegularControlSize;
  [mRadioButtonCell setControlSize:controlSize];

  DrawCellWithScaling(mRadioButtonCell, cgContext, inBoxRect, controlSize,
                      NATURAL_REGULAR_RADIO_BUTTON_WIDTH, NATURAL_REGULAR_RADIO_BUTTON_HEIGHT,
                      0.0f, 0.0f,
                      radioButtonMargins, PR_TRUE);


  NS_OBJC_END_TRY_ABORT_BLOCK;
}


// These are the sizes that Gecko needs to request to draw if it wants
// to get a standard-sized Aqua rounded bevel button drawn. Note that
// the rects that draw these are actually a little bigger.
#define NATURAL_MINI_ROUNDED_BUTTON_MIN_WIDTH 18
#define NATURAL_MINI_ROUNDED_BUTTON_HEIGHT 16
#define NATURAL_SMALL_ROUNDED_BUTTON_MIN_WIDTH 26
#define NATURAL_SMALL_ROUNDED_BUTTON_HEIGHT 19
#define NATURAL_REGULAR_ROUNDED_BUTTON_MIN_WIDTH 30
#define NATURAL_REGULAR_ROUNDED_BUTTON_HEIGHT 22

// These were calculated by testing all three sizes on the respective operating system.
static const float pushButtonMargins[2][3][4] =
{
  { // Tiger
    {1, 1, 1, 1}, // mini
    {5, 1, 5, 1}, // small
    {6, 0, 6, 2}  // regular
  },
  { // Leopard
    {0, 0, 0, 0}, // mini
    {4, 0, 4, 1}, // small
    {5, 0, 5, 2}  // regular
  }
};

// The height at which we start doing square buttons instead of rounded buttons
// Rounded buttons look bad if drawn at a height greater than 26, so at that point
// we switch over to doing square buttons which looks fine at any size.
#define DO_SQUARE_BUTTON_HEIGHT 26

void
nsNativeThemeCocoa::DrawPushButton(CGContextRef cgContext, const HIRect& inBoxRect, PRBool inIsDefault,
                                   PRBool inDisabled, PRInt32 inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSRect drawRect = NSMakeRect(inBoxRect.origin.x, inBoxRect.origin.y, inBoxRect.size.width, inBoxRect.size.height);

  [mPushButtonCell setEnabled:!inDisabled];
  [mPushButtonCell setHighlighted:((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER) || (inIsDefault && !inDisabled))];
  [mPushButtonCell setShowsFirstResponder:(inState & NS_EVENT_STATE_FOCUS)];

  CGAffineTransform savedCTM = CGContextGetCTM(cgContext);

  // This flips the image in place and is necessary to work around a bug in the way
  // NSButtonCell draws buttons.
  CGContextScaleCTM(cgContext, 1.0f, -1.0f);
  CGContextTranslateCTM(cgContext, 0.0f, -(2.0 * drawRect.origin.y + drawRect.size.height));

  // Set up the graphics context we've been asked to draw to.

  // If the button is tall enough, draw the square button style so that buttons with
  // non-standard content look good. Otherwise draw normal rounded aqua buttons.
  if (drawRect.size.height > DO_SQUARE_BUTTON_HEIGHT) {
    NSGraphicsContext* savedContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES]];

    [mPushButtonCell setBezelStyle:NSShadowlessSquareBezelStyle];
    [mPushButtonCell drawWithFrame:drawRect inView:[NSView focusView]];

    [NSGraphicsContext setCurrentContext:savedContext];
  } else {
    [mPushButtonCell setBezelStyle:NSRoundedBezelStyle];

    // Figure out what size cell control we're going to draw and grab its
    // natural height and min width.
    NSControlSize controlSize = NSRegularControlSize;
    float naturalHeight = NATURAL_REGULAR_ROUNDED_BUTTON_HEIGHT;
    float minWidth = NATURAL_REGULAR_ROUNDED_BUTTON_MIN_WIDTH;
    if (drawRect.size.height <= NATURAL_MINI_ROUNDED_BUTTON_HEIGHT &&
        drawRect.size.width >= NATURAL_MINI_ROUNDED_BUTTON_MIN_WIDTH) {
      controlSize = NSMiniControlSize;
      naturalHeight = NATURAL_MINI_ROUNDED_BUTTON_HEIGHT;
      minWidth = NATURAL_MINI_ROUNDED_BUTTON_MIN_WIDTH;
    }
    else if (drawRect.size.height <= NATURAL_SMALL_ROUNDED_BUTTON_HEIGHT &&
             drawRect.size.width >= NATURAL_SMALL_ROUNDED_BUTTON_MIN_WIDTH) {
      controlSize = NSSmallControlSize;
      naturalHeight = NATURAL_SMALL_ROUNDED_BUTTON_HEIGHT;
      minWidth = NATURAL_SMALL_ROUNDED_BUTTON_MIN_WIDTH;
    }
    [mPushButtonCell setControlSize:controlSize];

    DrawCellWithScaling(mPushButtonCell, cgContext, inBoxRect, controlSize,
                        0.0f, naturalHeight,
                        minWidth, 0.0f,
                        pushButtonMargins, PR_FALSE);
  }

  CGContextSetCTM (cgContext, savedCTM);

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void
nsNativeThemeCocoa::DrawButton(CGContextRef cgContext, ThemeButtonKind inKind,
                               const HIRect& inBoxRect, PRBool inIsDefault, PRBool inDisabled,
                               ThemeButtonValue inValue, ThemeButtonAdornment inAdornment,
                               PRInt32 inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = inKind;
  bdi.value = inValue;
  bdi.adornment = inAdornment;

  if (inDisabled)
    bdi.state = kThemeStateUnavailable;
  else if ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER))
    bdi.state = kThemeStatePressed;
  else
    bdi.state = (inKind == kThemeArrowButton) ? kThemeStateUnavailable : kThemeStateActive;

  if (inState & NS_EVENT_STATE_FOCUS)
    bdi.adornment |= kThemeAdornmentFocus;

  if (inIsDefault && !inDisabled)
    bdi.adornment |= kThemeAdornmentDefault;

  HIRect drawFrame = inBoxRect;
  PRBool needsScaling = PR_FALSE;
  int drawWidth = 0, drawHeight = 0;

  if (inKind == kThemePopupButton) {
    /* popup buttons draw outside their frame by 1 pixel on each side and
     * two on the bottom but of the bottom two pixels one is a 'shadow'
     * and not the frame itself.  That extra pixel should be handled
     * by GetWidgetOverflow, but we already extend each widget's overflow
     * by 4px to handle a potential focus ring.
     */

    if (nsToolkit::OnLeopardOrLater()) {
      /* Leopard will happily scale up for buttons that are sized 20px or higher,
       * drawing 1px below the actual requested area.  (So 20px == 21px.)
       * but anything below that will be clamped:
       *  requested: 20 actual: 21 (handled above)
       *  requested: 19 actual: 18 <- note that there is no way to draw a dropdown that's exactly 20 px in size
       *  requested: 18 actual: 18
       *  requested: 17 actual: 18
       *  requested: 16 actual: 15 (min size)
       * For those, draw to a buffer and scale
       */
      if (drawFrame.size.height != 18 && drawFrame.size.height != 15) {
        if (drawFrame.size.height > 20) {
          drawFrame.size.width -= 2;
          drawFrame.origin.x += 1;
          drawFrame.size.height -= 1;
        }
        else {
          // pick which native height to use for the small scale
          float nativeHeight = 15.0f;
          if (drawFrame.size.height > 16)
            nativeHeight = 18.0f;

          drawWidth = (int) drawFrame.size.width;
          drawHeight = (int) nativeHeight;

          needsScaling = PR_TRUE;
        }
      }
    }
    else {
      // leave things alone on Tiger
      drawFrame.size.height -= 1;
    }
  }

  if (!needsScaling) {
    HIThemeDrawButton(&drawFrame, &bdi, cgContext, kHIThemeOrientationNormal, NULL);
  } else {
    int w = drawWidth + MAX_FOCUS_RING_WIDTH*2;
    int h = drawHeight + MAX_FOCUS_RING_WIDTH*2;

    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(NULL, w, h, 8, w * 4,
                                             rgb, kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(rgb);

    // Flip the context
    CGContextTranslateCTM(ctx, 0.0f, h);
    CGContextScaleCTM(ctx, 1.0f, -1.0f);

    // then draw the button (offset by the focus ring size
    CGRect tmpFrame = CGRectMake(MAX_FOCUS_RING_WIDTH, MAX_FOCUS_RING_WIDTH, drawWidth, drawHeight);
    HIThemeDrawButton(&tmpFrame, &bdi, ctx, kHIThemeOrientationNormal, NULL);

    CGImageRef img = CGBitmapContextCreateImage(ctx);
    CGRect imgRect = CGRectMake(drawFrame.origin.x - MAX_FOCUS_RING_WIDTH,
                                drawFrame.origin.y - MAX_FOCUS_RING_WIDTH,
                                drawFrame.size.width + MAX_FOCUS_RING_WIDTH * 2.0,
                                drawFrame.size.height + MAX_FOCUS_RING_WIDTH * 2.0);

    // And then flip the main context here so that the image gets drawn right-side up
    CGAffineTransform ctm = CGContextGetCTM (cgContext);

    CGContextTranslateCTM (cgContext, imgRect.origin.x, imgRect.origin.y + imgRect.size.height);
    CGContextScaleCTM (cgContext, 1.0, -1.0);

    imgRect.origin.x = imgRect.origin.y = 0.0f;

    // See comment about why we don't scale MAX_FOCUS_RING in DrawCellWithScaling
    CGContextDrawImage(cgContext, imgRect, img);

    CGContextSetCTM (cgContext, ctm);

    CGImageRelease(img);
    CGContextRelease(ctx);
  }

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void
nsNativeThemeCocoa::DrawSpinButtons(CGContextRef cgContext, ThemeButtonKind inKind,
                                    const HIRect& inBoxRect, PRBool inDisabled,
                                    ThemeDrawState inDrawState,
                                    ThemeButtonAdornment inAdornment,
                                    PRInt32 inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeButtonDrawInfo bdi;
  bdi.version = 0;
  bdi.kind = inKind;
  bdi.state = inDrawState;
  bdi.value = kThemeButtonOff;
  bdi.adornment = inAdornment;

  if (inDisabled)
    bdi.state = kThemeStateUnavailable;

  HIThemeDrawButton(&inBoxRect, &bdi, cgContext, HITHEME_ORIENTATION, NULL);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void
nsNativeThemeCocoa::DrawFrame(CGContextRef cgContext, HIThemeFrameKind inKind,
                              const HIRect& inBoxRect, PRBool inIsDisabled, PRInt32 inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeFrameDrawInfo fdi;
  fdi.version = 0;
  fdi.kind = inKind;
  fdi.state = inIsDisabled ? kThemeStateUnavailable : kThemeStateActive;
  // for some reason focus rings on listboxes draw incorrectly
  if (inKind == kHIThemeFrameListBox)
    fdi.isFocused = 0;
  else
    fdi.isFocused = (inState & NS_EVENT_STATE_FOCUS) != 0;

  // HIThemeDrawFrame takes the rect for the content area of the frame, not
  // the bounding rect for the frame. Here we reduce the size of the rect we
  // will pass to make it the size of the content.
  HIRect drawRect = inBoxRect;
  if (inKind == kHIThemeFrameTextFieldSquare) {
    SInt32 frameOutset = 0;
    ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);
    drawRect.origin.x += frameOutset;
    drawRect.origin.y += frameOutset;
    drawRect.size.width -= frameOutset * 2;
    drawRect.size.height -= frameOutset * 2;
  }
  else if (inKind == kHIThemeFrameListBox) {
    SInt32 frameOutset = 0;
    ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
    drawRect.origin.x += frameOutset;
    drawRect.origin.y += frameOutset;
    drawRect.size.width -= frameOutset * 2;
    drawRect.size.height -= frameOutset * 2;
  }

#if DRAW_IN_FRAME_DEBUG
  CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 0.5, 0.25);
  CGContextFillRect(cgContext, inBoxRect);
#endif

  HIThemeDrawFrame(&drawRect, &fdi, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void
nsNativeThemeCocoa::DrawProgress(CGContextRef cgContext,
                                 const HIRect& inBoxRect, PRBool inIsIndeterminate, 
                                 PRBool inIsHorizontal, PRInt32 inValue)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeTrackDrawInfo tdi;

  PRInt32 stepsPerSecond = inIsIndeterminate ? 60 : 30;
  PRInt32 milliSecondsPerStep = 1000 / stepsPerSecond;

  tdi.version = 0;
  tdi.kind = inIsIndeterminate ? kThemeMediumIndeterminateBar: kThemeMediumProgressBar;
  tdi.bounds = inBoxRect;
  tdi.min = 0;
  tdi.max = 100;
  tdi.value = inValue;
  tdi.attributes = inIsHorizontal ? kThemeTrackHorizontal : 0;
  tdi.enableState = kThemeTrackActive;
  tdi.trackInfo.progress.phase = PR_IntervalToMilliseconds(PR_IntervalNow()) /
                                 milliSecondsPerStep % 16;

  HIThemeDrawTrack(&tdi, NULL, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void
nsNativeThemeCocoa::DrawTabPanel(CGContextRef cgContext, const HIRect& inBoxRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeTabPaneDrawInfo tpdi;

  tpdi.version = 0;
  tpdi.state = kThemeStateActive;
  tpdi.direction = kThemeTabNorth;
  tpdi.size = kHIThemeTabSizeNormal;

  HIThemeDrawTabPane(&inBoxRect, &tpdi, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void
nsNativeThemeCocoa::DrawScale(CGContextRef cgContext, const HIRect& inBoxRect,
                              PRBool inIsDisabled, PRInt32 inState,
                              PRBool inIsVertical, PRBool inIsReverse,
                              PRInt32 inCurrentValue,
                              PRInt32 inMinValue, PRInt32 inMaxValue)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeTrackDrawInfo tdi;

  tdi.version = 0;
  tdi.kind = kThemeMediumSlider;
  tdi.bounds = inBoxRect;
  tdi.min = inMinValue;
  tdi.max = inMaxValue;
  tdi.value = inCurrentValue;
  tdi.attributes = kThemeTrackShowThumb;
  if (!inIsVertical)
    tdi.attributes |= kThemeTrackHorizontal;
  if (inIsReverse)
    tdi.attributes |= kThemeTrackRightToLeft;
  if (inState & NS_EVENT_STATE_FOCUS)
    tdi.attributes |= kThemeTrackHasFocus;
  tdi.enableState = inIsDisabled ? kThemeTrackDisabled : kThemeTrackActive;
  tdi.trackInfo.slider.thumbDir = kThemeThumbPlain;
  tdi.trackInfo.slider.pressState = 0;

  HIThemeDrawTrack(&tdi, NULL, cgContext, HITHEME_ORIENTATION);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void
nsNativeThemeCocoa::DrawTab(CGContextRef cgContext, const HIRect& inBoxRect,
                            PRBool inIsDisabled, PRBool inIsFrontmost,
                            PRBool inIsHorizontal, PRBool inTabBottom,
                            PRInt32 inState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  HIThemeTabDrawInfo tdi;

  tdi.version = 0;

  if (inIsFrontmost) {
    if (inIsDisabled) 
      tdi.style = kThemeTabFrontUnavailable;
    else
      tdi.style = kThemeTabFront;
  } else {
    if (inIsDisabled)
      tdi.style = kThemeTabNonFrontUnavailable;
    else if ((inState & NS_EVENT_STATE_ACTIVE) && (inState & NS_EVENT_STATE_HOVER))
      tdi.style = kThemeTabNonFrontPressed;
    else
      tdi.style = kThemeTabNonFront;  
  }

  // don't yet support vertical tabs
  tdi.direction = inTabBottom ? kThemeTabSouth : kThemeTabNorth;
  tdi.size = kHIThemeTabSizeNormal;
  tdi.adornment = kThemeAdornmentNone;

  HIThemeDrawTab(&inBoxRect, &tdi, cgContext, HITHEME_ORIENTATION, NULL);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


static inline UInt8
ConvertToPressState(PRInt32 aButtonState, UInt8 aPressState)
{
  // If the button is pressed, return the press state passed in. Otherwise, return 0.
  return ((aButtonState & NS_EVENT_STATE_ACTIVE) && (aButtonState & NS_EVENT_STATE_HOVER)) ? aPressState : 0;
}


void 
nsNativeThemeCocoa::GetScrollbarPressStates(nsIFrame *aFrame, PRInt32 aButtonStates[])
{
  static nsIContent::AttrValuesArray attributeValues[] = {
    &nsWidgetAtoms::scrollbarUpTop,
    &nsWidgetAtoms::scrollbarDownTop,
    &nsWidgetAtoms::scrollbarUpBottom,
    &nsWidgetAtoms::scrollbarDownBottom,
    nsnull
  };

  // Get the state of any scrollbar buttons in our child frames
  for (nsIFrame *childFrame = aFrame->GetFirstChild(nsnull); 
       childFrame;
       childFrame = childFrame->GetNextSibling()) {

    nsIContent *childContent = childFrame->GetContent();
    if (!childContent) continue;
    PRInt32 attrIndex = childContent->FindAttrValueIn(kNameSpaceID_None, nsWidgetAtoms::sbattr, 
                                                      attributeValues, eCaseMatters);
    if (attrIndex < 0) continue;

    PRInt32 currentState = GetContentState(childFrame, NS_THEME_BUTTON);
    aButtonStates[attrIndex] = currentState;
  }
}


// Both of the following sets of numbers were derived by loading the testcase in
// bmo bug 380185 in Safari and observing its behavior for various heights of scrollbar.
// These magic numbers are the minimum sizes we can draw a scrollbar and still 
// have room for everything to display, including the thumb
#define MIN_SCROLLBAR_SIZE_WITH_THUMB 61
#define MIN_SMALL_SCROLLBAR_SIZE_WITH_THUMB 49
// And these are the minimum sizes if we don't draw the thumb
#define MIN_SCROLLBAR_SIZE 56
#define MIN_SMALL_SCROLLBAR_SIZE 46

void
nsNativeThemeCocoa::GetScrollbarDrawInfo(HIThemeTrackDrawInfo& aTdi, nsIFrame *aFrame, 
                                         const HIRect& aRect, PRBool aShouldGetButtonStates)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PRInt32 curpos = CheckIntAttr(aFrame, nsWidgetAtoms::curpos, 0);
  PRInt32 minpos = CheckIntAttr(aFrame, nsWidgetAtoms::minpos, 0);
  PRInt32 maxpos = CheckIntAttr(aFrame, nsWidgetAtoms::maxpos, 100);
  PRInt32 thumbSize = CheckIntAttr(aFrame, nsWidgetAtoms::pageincrement, 10);

  PRBool isHorizontal = aFrame->GetContent()->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::orient, 
                                                          nsWidgetAtoms::horizontal, eCaseMatters);
  PRBool isSmall = aFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL;

  aTdi.version = 0;
  aTdi.kind = isSmall ? kThemeSmallScrollBar : kThemeMediumScrollBar;
  aTdi.bounds = aRect;
  aTdi.min = minpos;
  aTdi.max = maxpos;
  aTdi.value = curpos;
  aTdi.attributes = 0;
  aTdi.enableState = kThemeTrackActive;
  if (isHorizontal)
    aTdi.attributes |= kThemeTrackHorizontal;

  aTdi.trackInfo.scrollbar.viewsize = (SInt32)thumbSize;

  /* Only display features if we have enough room for them.
   * Gecko still maintains the scrollbar info; this is just a visual issue (bug 380185).
   */
  PRInt32 longSideLength = (PRInt32)(isHorizontal ? (aRect.size.width) : (aRect.size.height));
  if (longSideLength >= (isSmall ? MIN_SMALL_SCROLLBAR_SIZE_WITH_THUMB : MIN_SCROLLBAR_SIZE_WITH_THUMB)) {
    aTdi.attributes |= kThemeTrackShowThumb;
  }
  else if (longSideLength < (isSmall ? MIN_SMALL_SCROLLBAR_SIZE : MIN_SCROLLBAR_SIZE)) {
    aTdi.enableState = kThemeTrackNothingToScroll;
    return;
  }

  // Only go get these scrollbar button states if we need it. For example, there's no reaon to look up scrollbar button 
  // states when we're only creating a TrackDrawInfo to determine the size of the thumb.
  if (aShouldGetButtonStates) {
    PRInt32 buttonStates[] = {0, 0, 0, 0};
    GetScrollbarPressStates(aFrame, buttonStates);
    ThemeScrollBarArrowStyle arrowStyle;
    ::GetThemeScrollBarArrowStyle(&arrowStyle);
    // If all four buttons are visible
    if (arrowStyle == kThemeScrollBarArrowsBoth) {
      aTdi.trackInfo.scrollbar.pressState = ConvertToPressState(buttonStates[0], kThemeTopOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[1], kThemeTopInsideArrowPressed) |
                                            ConvertToPressState(buttonStates[2], kThemeBottomInsideArrowPressed) |
                                            ConvertToPressState(buttonStates[3], kThemeBottomOutsideArrowPressed);
    } else {
      // It seems that unless all four buttons are showing, kThemeTopOutsideArrowPressed is the correct constant for
      // the up scrollbar button.
      aTdi.trackInfo.scrollbar.pressState = ConvertToPressState(buttonStates[0], kThemeTopOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[1], kThemeBottomOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[2], kThemeTopOutsideArrowPressed) |
                                            ConvertToPressState(buttonStates[3], kThemeBottomOutsideArrowPressed);
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void
nsNativeThemeCocoa::DrawScrollbar(CGContextRef aCGContext, const HIRect& aBoxRect, nsIFrame *aFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // HIThemeDrawTrack is buggy with rotations and scaling
  CGAffineTransform savedCTM = CGContextGetCTM(aCGContext);
  PRBool drawDirect;
  HIRect drawRect = aBoxRect;

  if (savedCTM.a == 1.0f && savedCTM.b == 0.0f &&
      savedCTM.c == 0.0f && (savedCTM.d == 1.0f || savedCTM.d == -1.0f))
  {
    drawDirect = TRUE;
  } else {
    drawRect.origin.x = drawRect.origin.y = 0.0f;
    drawDirect = FALSE;
  }

  HIThemeTrackDrawInfo tdi;
  GetScrollbarDrawInfo(tdi, aFrame, drawRect, PR_TRUE); //True means we want the press states

  if (drawDirect) {
    ::HIThemeDrawTrack(&tdi, NULL, aCGContext, HITHEME_ORIENTATION);
  } else {
    // Note that NSScroller can draw transformed just fine, but HITheme can't.
    // However, we can't make NSScroller's parts light up easily (depressed buttons, etc.)
    // This is very frustrating.

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmapctx = CGBitmapContextCreate(NULL,
                                                   (size_t) ceil(drawRect.size.width),
                                                   (size_t) ceil(drawRect.size.height),
                                                   8,
                                                   (size_t) ceil(drawRect.size.width) * 4,
                                                   colorSpace,
                                                   kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(colorSpace);

    // HITheme always wants to draw into a flipped context, or things
    // get confused.
    CGContextTranslateCTM(bitmapctx, 0.0f, aBoxRect.size.height);
    CGContextScaleCTM(bitmapctx, 1.0f, -1.0f);

    HIThemeDrawTrack(&tdi, NULL, bitmapctx, HITHEME_ORIENTATION);

    CGImageRef bitmap = CGBitmapContextCreateImage(bitmapctx);

    CGAffineTransform ctm = CGContextGetCTM(aCGContext);

    // We need to unflip, so that we can do a DrawImage without getting a flipped image.
    CGContextTranslateCTM(aCGContext, 0.0f, aBoxRect.size.height);
    CGContextScaleCTM(aCGContext, 1.0f, -1.0f);

    CGContextDrawImage(aCGContext, aBoxRect, bitmap);

    CGContextSetCTM(aCGContext, ctm);

    CGImageRelease(bitmap);
    CGContextRelease(bitmapctx);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


nsIFrame*
nsNativeThemeCocoa::GetParentScrollbarFrame(nsIFrame *aFrame)
{
  // Walk our parents to find a scrollbar frame
  nsIFrame *scrollbarFrame = aFrame;
  do {
    if (scrollbarFrame->GetType() == nsWidgetAtoms::scrollbarFrame) break;
  } while ((scrollbarFrame = scrollbarFrame->GetParent()));
  
  // We return null if we can't find a parent scrollbar frame
  return scrollbarFrame;
}


NS_IMETHODIMP
nsNativeThemeCocoa::DrawWidgetBackground(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                         PRUint8 aWidgetType, const nsRect& aRect,
                                         const nsRect& aDirtyRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // setup to draw into the correct port
  nsCOMPtr<nsIDeviceContext> dctx;
  aContext->GetDeviceContext(*getter_AddRefs(dctx));
  PRInt32 p2a = dctx->AppUnitsPerDevPixel();

  gfxRect nativeDirtyRect(aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  gfxRect nativeWidgetRect(aRect.x, aRect.y, aRect.width, aRect.height);
  nativeWidgetRect.ScaleInverse(gfxFloat(p2a));
  nativeDirtyRect.ScaleInverse(gfxFloat(p2a));
  nativeWidgetRect.Round();

  nsRefPtr<gfxContext> thebesCtx = aContext->ThebesContext();
  if (!thebesCtx)
    return NS_ERROR_FAILURE;

  gfxQuartzNativeDrawing nativeDrawing(thebesCtx, nativeDirtyRect);

  CGContextRef cgContext = nativeDrawing.BeginNativeDrawing();
  if (cgContext == nsnull) {
    // The Quartz surface handles 0x0 surfaces by internally
    // making all operations no-ops; there's no cgcontext created for them.
    // Unfortunately, this means that callers that want to render
    // directly to the CGContext need to be aware of this quirk.
    return NS_OK;
  }

#if 0
  if (1 /*aWidgetType == NS_THEME_TEXTFIELD*/) {
    fprintf(stderr, "Native theme drawing widget %d [%p] dis:%d in rect [%d %d %d %d]\n",
            aWidgetType, aFrame, IsDisabled(aFrame), aRect.x, aRect.y, aRect.width, aRect.height);
    fprintf(stderr, "Cairo matrix: [%f %f %f %f %f %f]\n",
            mat.xx, mat.yx, mat.xy, mat.yy, mat.x0, mat.y0);
    fprintf(stderr, "Native theme xform[0]: [%f %f %f %f %f %f]\n",
            mm0.a, mm0.b, mm0.c, mm0.d, mm0.tx, mm0.ty);
    CGAffineTransform mm = CGContextGetCTM(cgContext);
    fprintf(stderr, "Native theme xform[1]: [%f %f %f %f %f %f]\n",
            mm.a, mm.b, mm.c, mm.d, mm.tx, mm.ty);
  }
#endif

  CGRect macRect = CGRectMake(nativeWidgetRect.X(), nativeWidgetRect.Y(),
                              nativeWidgetRect.Width(), nativeWidgetRect.Height());

#if 0
  fprintf(stderr, "    --> macRect %f %f %f %f\n",
          macRect.origin.x, macRect.origin.y, macRect.size.width, macRect.size.height);
  CGRect bounds = CGContextGetClipBoundingBox(cgContext);
  fprintf(stderr, "    --> clip bounds: %f %f %f %f\n",
          bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height);

  //CGContextSetRGBFillColor(cgContext, 0.0, 0.0, 1.0, 0.1);
  //CGContextFillRect(cgContext, bounds);
#endif

  PRInt32 eventState = GetContentState(aFrame, aWidgetType);

  switch (aWidgetType) {
    case NS_THEME_DIALOG: {
      HIThemeSetFill(kThemeBrushDialogBackgroundActive, NULL, cgContext, HITHEME_ORIENTATION);
      CGContextFillRect(cgContext, macRect);
    }
      break;

    case NS_THEME_MENUPOPUP: {
      HIThemeMenuDrawInfo mdi = {
        version: 0,
        menuType: IsDisabled(aFrame) ? kThemeMenuTypeInactive : kThemeMenuTypePopUp
      };

      HIThemeDrawMenuBackground(&macRect, &mdi, cgContext, HITHEME_ORIENTATION);
    }
      break;

    case NS_THEME_MENUITEM: {
      // maybe use kThemeMenuItemHierBackground or PopUpBackground instead of just Plain?
      HIThemeMenuItemDrawInfo drawInfo = {
        version: 0,
        itemType: kThemeMenuItemPlain,
        state: (IsDisabled(aFrame) ? kThemeMenuDisabled :
                CheckBooleanAttr(aFrame, nsWidgetAtoms::mozmenuactive) ? kThemeMenuSelected :
                kThemeMenuActive)
      };

      // XXX pass in the menu rect instead of always using the item rect
      HIRect ignored;
      HIThemeDrawMenuItem(&macRect, &macRect, &drawInfo, cgContext, HITHEME_ORIENTATION, &ignored);
    }
      break;

    case NS_THEME_MENUSEPARATOR: {
      ThemeMenuState menuState;
      if (IsDisabled(aFrame)) {
        menuState = kThemeMenuDisabled;
      }
      else {
        menuState = CheckBooleanAttr(aFrame, nsWidgetAtoms::mozmenuactive) ?
                    kThemeMenuSelected : kThemeMenuActive;
      }

      HIThemeMenuItemDrawInfo midi = { 0, kThemeMenuItemPlain, menuState };
      HIThemeDrawMenuSeparator(&macRect, &macRect, &midi, cgContext, HITHEME_ORIENTATION);
    }
      break;

    case NS_THEME_TOOLTIP:
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 0.78, 1.0);
      CGContextFillRect(cgContext, macRect);
      break;

    case NS_THEME_CHECKBOX:
      DrawCheckbox(cgContext, kThemeCheckBox, macRect, IsChecked(aFrame), IsDisabled(aFrame), eventState);
      break;

    case NS_THEME_CHECKBOX_SMALL:
      DrawCheckbox(cgContext, kThemeSmallCheckBox, macRect, IsChecked(aFrame), IsDisabled(aFrame), eventState);
      break;

    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
      DrawRadioButton(cgContext, macRect, IsSelected(aFrame), IsDisabled(aFrame), eventState);
      break;

    case NS_THEME_BUTTON:
      DrawPushButton(cgContext, macRect, IsDefaultButton(aFrame), IsDisabled(aFrame), eventState);
      break;

    case NS_THEME_BUTTON_BEVEL:
      DrawButton(cgContext, kThemeMediumBevelButton, macRect,
                 IsDefaultButton(aFrame), IsDisabled(aFrame), 
                 kThemeButtonOff, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_SPINNER: {
      ThemeDrawState state = kThemeStateActive;
      nsIContent* content = aFrame->GetContent();
      if (content->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::state,
                               NS_LITERAL_STRING("up"), eCaseMatters)) {
        state = kThemeStatePressedUp;
      }
      else if (content->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::state,
                                    NS_LITERAL_STRING("down"), eCaseMatters)) {
        state = kThemeStatePressedDown;
      }

      DrawSpinButtons(cgContext, kThemeIncDecButton, macRect, IsDisabled(aFrame),
                      state, kThemeAdornmentNone, eventState);
    }
      break;

    case NS_THEME_TOOLBAR_BUTTON:
      DrawButton(cgContext, kThemePushButton, macRect,
                 IsDefaultButton(aFrame), IsDisabled(aFrame),
                 kThemeButtonOn, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_TOOLBAR_SEPARATOR: {
      HIThemeSeparatorDrawInfo sdi = { 0, kThemeStateActive };
      HIThemeDrawSeparator(&macRect, &sdi, cgContext, HITHEME_ORIENTATION);
    }
      break;

    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBOX:
    case NS_THEME_STATUSBAR: {
      HIThemeHeaderDrawInfo hdi = { 0, kThemeStateActive, kHIThemeHeaderKindWindow };
      HIThemeDrawHeader(&macRect, &hdi, cgContext, HITHEME_ORIENTATION);
    }
      break;
      
    case NS_THEME_DROPDOWN:
      DrawButton(cgContext, kThemePopupButton, macRect,
                 IsDefaultButton(aFrame), IsDisabled(aFrame), 
                 kThemeButtonOn, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_DROPDOWN_BUTTON:
      DrawButton (cgContext, kThemeArrowButton, macRect, PR_FALSE,
                  IsDisabled(aFrame), kThemeButtonOn,
                  kThemeAdornmentArrowDownArrow, eventState);
      break;

    case NS_THEME_GROUPBOX: {
      HIThemeGroupBoxDrawInfo gdi = { 0, kThemeStateActive, kHIThemeGroupBoxKindPrimary };
      HIThemeDrawGroupBox(&macRect, &gdi, cgContext, HITHEME_ORIENTATION);
      break;
    }

    case NS_THEME_TEXTFIELD:
      // HIThemeSetFill is not available on 10.3
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);

      // XUL textboxes set the native appearance on the containing box, while
      // concrete focus is set on the html:input element within it. We can
      // though, check the focused attribute of xul textboxes in this case.
      if (aFrame->GetContent()->IsNodeOfType(nsINode::eXUL) &&
          IsFocused(aFrame)) {
        eventState |= NS_EVENT_STATE_FOCUS;
      }

      DrawFrame(cgContext, kHIThemeFrameTextFieldSquare,
                macRect, (IsDisabled(aFrame) || IsReadOnly(aFrame)), eventState);
      break;
      
    case NS_THEME_PROGRESSBAR:
      DrawProgress(cgContext, macRect, IsIndeterminateProgress(aFrame),
                   PR_TRUE, GetProgressValue(aFrame));
      break;

    case NS_THEME_PROGRESSBAR_VERTICAL:
      DrawProgress(cgContext, macRect, IsIndeterminateProgress(aFrame),
                   PR_FALSE, GetProgressValue(aFrame));
      break;

    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
      // do nothing, covered by the progress bar cases above
      break;

    case NS_THEME_TREEVIEW_TWISTY:
      DrawButton(cgContext, kThemeDisclosureButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                 kThemeDisclosureRight, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_TREEVIEW_TWISTY_OPEN:
      DrawButton(cgContext, kThemeDisclosureButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                 kThemeDisclosureDown, kThemeAdornmentNone, eventState);
      break;

    case NS_THEME_TREEVIEW_HEADER_CELL: {
      TreeSortDirection sortDirection = GetTreeSortDirection(aFrame);
      DrawButton(cgContext, kThemeListHeaderButton, macRect, PR_FALSE, IsDisabled(aFrame), 
                 sortDirection == eTreeSortDirection_Natural ? kThemeButtonOff : kThemeButtonOn,
                 sortDirection == eTreeSortDirection_Descending ?
                 kThemeAdornmentHeaderButtonSortUp : kThemeAdornmentNone, eventState);      
    }
      break;

    case NS_THEME_TREEVIEW_TREEITEM:
    case NS_THEME_TREEVIEW:
      // HIThemeSetFill is not available on 10.3
      // HIThemeSetFill(kThemeBrushWhite, NULL, cgContext, HITHEME_ORIENTATION);
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);
      break;

    case NS_THEME_TREEVIEW_HEADER:
      // do nothing, taken care of by individual header cells
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
      // do nothing, taken care of by treeview header
    case NS_THEME_TREEVIEW_LINE:
      // do nothing, these lines don't exist on macos
      break;

    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL: {
      PRInt32 curpos = CheckIntAttr(aFrame, nsWidgetAtoms::curpos, 0);
      PRInt32 minpos = CheckIntAttr(aFrame, nsWidgetAtoms::minpos, 0);
      PRInt32 maxpos = CheckIntAttr(aFrame, nsWidgetAtoms::maxpos, 100);
      if (!maxpos)
        maxpos = 100;

      PRBool reverse = aFrame->GetContent()->
        AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::dir,
                    NS_LITERAL_STRING("reverse"), eCaseMatters);
      DrawScale(cgContext, macRect, IsDisabled(aFrame), eventState,
                (aWidgetType == NS_THEME_SCALE_VERTICAL), reverse,
                curpos, minpos, maxpos);
    }
      break;

    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
      // do nothing, drawn by scale
      break;

    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR: {
      DrawScrollbar(cgContext, macRect, aFrame);
    }
      break;
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
    break;
#endif
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 1.0, 0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
    break;
#endif
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
#if SCROLLBARS_VISUAL_DEBUG
      CGContextSetRGBFillColor(cgContext, 0, 1.0, 0, 0.6);
      CGContextFillRect(cgContext, macRect);
    break;      
#endif
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
      // do nothing, drawn by scrollbar
      break;

    case NS_THEME_TEXTFIELD_MULTILINE: {
      // we have to draw this by hand because there is no HITheme value for it
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      
      CGContextFillRect(cgContext, macRect);

      CGContextSetLineWidth(cgContext, 1.0);
      CGContextSetShouldAntialias(cgContext, false);

      // stroke everything but the top line of the text area
      CGContextSetRGBStrokeColor(cgContext, 0.6, 0.6, 0.6, 1.0);
      CGContextBeginPath(cgContext);
      CGContextMoveToPoint(cgContext, macRect.origin.x, macRect.origin.y + 1);
      CGContextAddLineToPoint(cgContext, macRect.origin.x, macRect.origin.y + macRect.size.height);
      CGContextAddLineToPoint(cgContext, macRect.origin.x + macRect.size.width - 1, macRect.origin.y + macRect.size.height);
      CGContextAddLineToPoint(cgContext, macRect.origin.x + macRect.size.width - 1, macRect.origin.y + 1);
      CGContextStrokePath(cgContext);

      // stroke the line across the top of the text area
      CGContextSetRGBStrokeColor(cgContext, 0.4510, 0.4510, 0.4510, 1.0);
      CGContextBeginPath(cgContext);
      CGContextMoveToPoint(cgContext, macRect.origin.x, macRect.origin.y + 1);
      CGContextAddLineToPoint(cgContext, macRect.origin.x + macRect.size.width - 1, macRect.origin.y + 1);
      CGContextStrokePath(cgContext);

      // draw a focus ring
      if (eventState & NS_EVENT_STATE_FOCUS) {
        // We need to bring the rectangle in by 1 pixel on each side.
        CGRect cgr = CGRectMake(macRect.origin.x + 1,
                                macRect.origin.y + 1,
                                macRect.size.width - 2,
                                macRect.size.height - 2);
        HIThemeDrawFocusRect(&cgr, true, cgContext, kHIThemeOrientationNormal);
      }
    }
      break;

    case NS_THEME_LISTBOX:
      // HIThemeSetFill is not available on 10.3
      CGContextSetRGBFillColor(cgContext, 1.0, 1.0, 1.0, 1.0);
      CGContextFillRect(cgContext, macRect);
      DrawFrame(cgContext, kHIThemeFrameListBox,
                macRect, (IsDisabled(aFrame) || IsReadOnly(aFrame)), eventState);
      break;
    
    case NS_THEME_TAB: {
      DrawTab(cgContext, macRect,
              IsDisabled(aFrame), IsSelectedTab(aFrame),
              PR_TRUE, IsBottomTab(aFrame),
              eventState);
    }
      break;

    case NS_THEME_TAB_PANELS:
      DrawTabPanel(cgContext, macRect);
      break;
  }

  nativeDrawing.EndNativeDrawing();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


static const int kAquaDropdownLeftBorder = 5;
static const int kAquaDropdownRightBorder = 22;

NS_IMETHODIMP
nsNativeThemeCocoa::GetWidgetBorder(nsIDeviceContext* aContext, 
                                    nsIFrame* aFrame,
                                    PRUint8 aWidgetType,
                                    nsMargin* aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  aResult->SizeTo(0, 0, 0, 0);

  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    {
      aResult->SizeTo(7, 1, 7, 3);
      break;
    }

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
      aResult->SizeTo(kAquaDropdownLeftBorder, 2, kAquaDropdownRightBorder, 2);
      break;

    case NS_THEME_TEXTFIELD:
    {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricEditTextFrameOutset, &frameOutset);

      SInt32 textPadding = 0;
      ::GetThemeMetric(kThemeMetricEditTextWhitespace, &textPadding);

      frameOutset += textPadding;

      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
      break;
    }

    case NS_THEME_TEXTFIELD_MULTILINE:
      aResult->SizeTo(1, 1, 1, 1);
      break;

    case NS_THEME_LISTBOX:
    {
      SInt32 frameOutset = 0;
      ::GetThemeMetric(kThemeMetricListBoxFrameOutset, &frameOutset);
      aResult->SizeTo(frameOutset, frameOutset, frameOutset, frameOutset);
      break;
    }

    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    {
      // There's only an endcap to worry about when both arrows are on the bottom
      ThemeScrollBarArrowStyle arrowStyle;
      ::GetThemeScrollBarArrowStyle(&arrowStyle);
      if (arrowStyle == kThemeScrollBarArrowsLowerRight) {
        PRBool isHorizontal = (aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL);

        nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
        if (!scrollbarFrame) return NS_ERROR_FAILURE;
        PRBool isSmall = (scrollbarFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL);

        // There isn't a metric for this, so just hardcode a best guess at the value.
        // This value is even less exact due to the fact that the endcap is partially concave.
        PRInt32 endcapSize = isSmall ? 5 : 6;

        if (isHorizontal)
          aResult->SizeTo(endcapSize, 0, 0, 0);
        else
          aResult->SizeTo(0, endcapSize, 0, 0);
      }
      break;
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


// Return PR_FALSE here to indicate that CSS padding values should be used. There is
// no reason to make a distinction between padding and border values, just specify
// whatever values you want in GetWidgetBorder and only use this to return PR_TRUE
// if you want to override CSS padding values.
PRBool
nsNativeThemeCocoa::GetWidgetPadding(nsIDeviceContext* aContext, 
                                     nsIFrame* aFrame,
                                     PRUint8 aWidgetType,
                                     nsMargin* aResult)
{
  // We don't want CSS padding being used for certain widgets.
  // See bug 381639 for an example of why.
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
      aResult->SizeTo(0, 0, 0, 0);
      return PR_TRUE;
  }
  return PR_FALSE;
}


PRBool
nsNativeThemeCocoa::GetWidgetOverflow(nsIDeviceContext* aContext, nsIFrame* aFrame,
                                      PRUint8 aWidgetType, nsRect* aOverflowRect)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_LISTBOX:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
    {
      // We assume that the above widgets can draw a focus ring that will be less than
      // or equal to 4 pixels thick.
      nsIntMargin extraSize = nsIntMargin(MAX_FOCUS_RING_WIDTH, MAX_FOCUS_RING_WIDTH, MAX_FOCUS_RING_WIDTH, MAX_FOCUS_RING_WIDTH);
      PRInt32 p2a = aContext->AppUnitsPerDevPixel();
      nsMargin m(NSIntPixelsToAppUnits(extraSize.left, p2a),
                 NSIntPixelsToAppUnits(extraSize.top, p2a),
                 NSIntPixelsToAppUnits(extraSize.right, p2a),
                 NSIntPixelsToAppUnits(extraSize.bottom, p2a));
      aOverflowRect->Inflate(m);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}


NS_IMETHODIMP
nsNativeThemeCocoa::GetMinimumWidgetSize(nsIRenderingContext* aContext,
                                         nsIFrame* aFrame,
                                         PRUint8 aWidgetType,
                                         nsSize* aResult,
                                         PRBool* aIsOverridable)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  aResult->SizeTo(0,0);
  *aIsOverridable = PR_TRUE;

  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    {
      aResult->SizeTo(NATURAL_MINI_ROUNDED_BUTTON_MIN_WIDTH, NATURAL_MINI_ROUNDED_BUTTON_HEIGHT);
      break;
    }

    case NS_THEME_SPINNER:
    {
      SInt32 buttonHeight = 0, buttonWidth = 0;
      ::GetThemeMetric(kThemeMetricLittleArrowsWidth, &buttonWidth);
      ::GetThemeMetric(kThemeMetricLittleArrowsHeight, &buttonHeight);
      aResult->SizeTo(buttonWidth, buttonHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_CHECKBOX:
    {
      SInt32 boxHeight = 0, boxWidth = 0;
      ::GetThemeMetric(kThemeMetricCheckBoxWidth, &boxWidth);
      ::GetThemeMetric(kThemeMetricCheckBoxHeight, &boxHeight);
      aResult->SizeTo(boxWidth, boxHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_CHECKBOX_SMALL:
    {
      SInt32 boxHeight = 0, boxWidth = 0;
      ::GetThemeMetric(kThemeMetricSmallCheckBoxWidth, &boxWidth);
      ::GetThemeMetric(kThemeMetricSmallCheckBoxHeight, &boxHeight);
      aResult->SizeTo(boxWidth, boxHeight - 1);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_RADIO:
    {
      aResult->SizeTo(NATURAL_REGULAR_RADIO_BUTTON_WIDTH, NATURAL_REGULAR_RADIO_BUTTON_HEIGHT);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_RADIO_SMALL:
    {
      aResult->SizeTo(NATURAL_SMALL_RADIO_BUTTON_WIDTH, NATURAL_SMALL_RADIO_BUTTON_HEIGHT);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    {
      SInt32 popupHeight = 0;
      ::GetThemeMetric(kThemeMetricPopupButtonHeight, &popupHeight);
      aResult->SizeTo(0, popupHeight);
      break;
    }
 
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    {
      // at minimum, we should be tall enough for 9pt text.
      // I'm using hardcoded values here because the appearance manager
      // values for the frame size are incorrect.
      aResult->SizeTo(0, (2 + 2) /* top */ + 9 + (1 + 1) /* bottom */);
      break;
    }
      
    case NS_THEME_PROGRESSBAR:
    {
      SInt32 barHeight = 0;
      ::GetThemeMetric(kThemeMetricNormalProgressBarThickness, &barHeight);
      aResult->SizeTo(0, barHeight);
      break;
    }

    case NS_THEME_TREEVIEW_TWISTY:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:   
    {
      SInt32 twistyHeight = 0, twistyWidth = 0;
      ::GetThemeMetric(kThemeMetricDisclosureButtonWidth, &twistyWidth);
      ::GetThemeMetric(kThemeMetricDisclosureButtonHeight, &twistyHeight);
      aResult->SizeTo(twistyWidth, twistyHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }
    
    case NS_THEME_TREEVIEW_HEADER:
    case NS_THEME_TREEVIEW_HEADER_CELL:
    {
      SInt32 headerHeight = 0;
      ::GetThemeMetric(kThemeMetricListHeaderHeight, &headerHeight);
      aResult->SizeTo(0, headerHeight);
      break;
    }

    case NS_THEME_SCALE_HORIZONTAL:
    {
      SInt32 scaleHeight = 0;
      ::GetThemeMetric(kThemeMetricHSliderHeight, &scaleHeight);
      aResult->SizeTo(scaleHeight, scaleHeight);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_SCALE_VERTICAL:
    {
      SInt32 scaleWidth = 0;
      ::GetThemeMetric(kThemeMetricVSliderWidth, &scaleWidth);
      aResult->SizeTo(scaleWidth, scaleWidth);
      *aIsOverridable = PR_FALSE;
      break;
    }
      
    case NS_THEME_SCROLLBAR_SMALL:
    {
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(kThemeMetricSmallScrollBarWidth, &scrollbarWidth);
      aResult->SizeTo(scrollbarWidth, scrollbarWidth);
      *aIsOverridable = PR_FALSE;
      break;
    }

    // Get the rect of the thumb from HITheme, so we can return it to Gecko, which has different ideas about
    // how big the thumb should be. This is kind of a hack.
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    {
      // Find our parent scrollbar frame. If we can't, abort.
      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame) return NS_ERROR_FAILURE;

      nsRect scrollbarRect = scrollbarFrame->GetRect();      
      *aIsOverridable = PR_FALSE;

      if (scrollbarRect.IsEmpty()) {
        // just return (0,0)
        return NS_OK;
      }

      // We need to get the device context to convert from app units :(
      nsCOMPtr<nsIDeviceContext> dctx;
      aContext->GetDeviceContext(*getter_AddRefs(dctx));
      PRInt32 p2a = dctx->AppUnitsPerDevPixel();
      CGRect macRect = CGRectMake(NSAppUnitsToIntPixels(scrollbarRect.x, p2a),
                                  NSAppUnitsToIntPixels(scrollbarRect.y, p2a),
                                  NSAppUnitsToIntPixels(scrollbarRect.width, p2a),
                                  NSAppUnitsToIntPixels(scrollbarRect.height, p2a));

      // False here means not to get scrollbar button state information.
      HIThemeTrackDrawInfo tdi;
      GetScrollbarDrawInfo(tdi, scrollbarFrame, macRect, PR_FALSE);

      HIRect thumbRect;
      ::HIThemeGetTrackPartBounds(&tdi, kControlIndicatorPart, &thumbRect);

      // HITheme is just lying to us, I guess...
      PRInt32 thumbAdjust = ((scrollbarFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL) ?
                             2 : 4);

      if (aWidgetType == NS_THEME_SCROLLBAR_THUMB_VERTICAL)
        aResult->SizeTo(nscoord(thumbRect.size.width), nscoord(thumbRect.size.height - thumbAdjust));
      else
        aResult->SizeTo(nscoord(thumbRect.size.width - thumbAdjust), nscoord(thumbRect.size.height));
      break;
    }

    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    {
      // yeah, i know i'm cheating a little here, but i figure that it
      // really doesn't matter if the scrollbar is vertical or horizontal
      // and the width metric is a really good metric for every piece
      // of the scrollbar.

      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame) return NS_ERROR_FAILURE;

      PRInt32 themeMetric = (scrollbarFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL) ?
                            kThemeMetricSmallScrollBarWidth :
                            kThemeMetricScrollBarWidth;
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(themeMetric, &scrollbarWidth);
      aResult->SizeTo(scrollbarWidth, scrollbarWidth);
      *aIsOverridable = PR_FALSE;
      break;
    }

    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    {
      nsIFrame *scrollbarFrame = GetParentScrollbarFrame(aFrame);
      if (!scrollbarFrame) return NS_ERROR_FAILURE;

      // Since there is no NS_THEME_SCROLLBAR_BUTTON_UP_SMALL we need to ask the parent what appearance style it has.
      PRInt32 themeMetric = (scrollbarFrame->GetStyleDisplay()->mAppearance == NS_THEME_SCROLLBAR_SMALL) ?
                            kThemeMetricSmallScrollBarWidth :
                            kThemeMetricScrollBarWidth;
      SInt32 scrollbarWidth = 0;
      ::GetThemeMetric(themeMetric, &scrollbarWidth);

      // It seems that for both sizes of scrollbar, the buttons are one pixel "longer".
      if (aWidgetType == NS_THEME_SCROLLBAR_BUTTON_LEFT || aWidgetType == NS_THEME_SCROLLBAR_BUTTON_RIGHT)
        aResult->SizeTo(scrollbarWidth+1, scrollbarWidth);
      else
        aResult->SizeTo(scrollbarWidth, scrollbarWidth+1);
 
      *aIsOverridable = PR_FALSE;
      break;
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


NS_IMETHODIMP
nsNativeThemeCocoa::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                     nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
  // Some widget types just never change state.
  switch (aWidgetType) {
    case NS_THEME_TOOLBOX:
    case NS_THEME_TOOLBAR:
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL: 
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_STATUSBAR:
    case NS_THEME_STATUSBAR_PANEL:
    case NS_THEME_STATUSBAR_RESIZER_PANEL:
    case NS_THEME_TOOLTIP:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB_PANEL:
    case NS_THEME_DIALOG:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_GROUPBOX:
      *aShouldRepaint = PR_FALSE;
      return NS_OK;
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
      *aShouldRepaint = (aAttribute == nsWidgetAtoms::step);
      return NS_OK;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = PR_TRUE;
  } else {
    // Check the attribute to see if it's relevant.  
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = PR_FALSE;
    if (aAttribute == nsWidgetAtoms::disabled ||
        aAttribute == nsWidgetAtoms::checked ||
        aAttribute == nsWidgetAtoms::selected ||
        aAttribute == nsWidgetAtoms::mozmenuactive ||
        aAttribute == nsWidgetAtoms::sortdirection ||
        aAttribute == nsWidgetAtoms::focused ||
        aAttribute == nsWidgetAtoms::_default)
      *aShouldRepaint = PR_TRUE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeCocoa::ThemeChanged()
{
  // This is unimplemented because we don't care if gecko changes its theme
  // and Mac OS X doesn't have themes.
  return NS_OK;
}


PRBool 
nsNativeThemeCocoa::ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                                      PRUint8 aWidgetType)
{
  if (aPresContext && !aPresContext->PresShell()->IsThemeSupportEnabled())
    return PR_FALSE;

  // if this is a dropdown button in a combobox the answer is always no
  if (aWidgetType == NS_THEME_DROPDOWN_BUTTON) {
    nsIFrame* parentFrame = aFrame->GetParent();
    if (parentFrame && (parentFrame->GetType() == nsWidgetAtoms::comboboxControlFrame))
      return PR_FALSE;
  }

  switch (aWidgetType) {
    case NS_THEME_LISTBOX:

    case NS_THEME_DIALOG:
    case NS_THEME_WINDOW:
    case NS_THEME_MENUPOPUP:
    case NS_THEME_MENUITEM:
    case NS_THEME_MENUSEPARATOR:
    case NS_THEME_TOOLTIP:
    
    case NS_THEME_CHECKBOX:
    case NS_THEME_CHECKBOX_SMALL:
    case NS_THEME_CHECKBOX_CONTAINER:
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL:
    case NS_THEME_RADIO_CONTAINER:
    case NS_THEME_GROUPBOX:
    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_BEVEL:
    case NS_THEME_SPINNER:
    case NS_THEME_TOOLBAR:
    case NS_THEME_STATUSBAR:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    //case NS_THEME_TOOLBOX:
    //case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TOOLBAR_SEPARATOR:
    
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    
    case NS_THEME_TREEVIEW_TWISTY:
    case NS_THEME_TREEVIEW_TWISTY_OPEN:
    case NS_THEME_TREEVIEW:
    case NS_THEME_TREEVIEW_HEADER:
    case NS_THEME_TREEVIEW_HEADER_CELL:
    case NS_THEME_TREEVIEW_HEADER_SORTARROW:
    case NS_THEME_TREEVIEW_TREEITEM:
    case NS_THEME_TREEVIEW_LINE:

    case NS_THEME_SCALE_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_VERTICAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:

    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_SMALL:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:

    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_DROPDOWN_TEXT:
      return !IsWidgetStyled(aPresContext, aFrame, aWidgetType);
      break;
  }

  return PR_FALSE;
}


PRBool
nsNativeThemeCocoa::WidgetIsContainer(PRUint8 aWidgetType)
{
  // flesh this out at some point
  switch (aWidgetType) {
   case NS_THEME_DROPDOWN_BUTTON:
   case NS_THEME_RADIO:
   case NS_THEME_RADIO_SMALL:
   case NS_THEME_CHECKBOX:
   case NS_THEME_CHECKBOX_SMALL:
   case NS_THEME_PROGRESSBAR:
    return PR_FALSE;
    break;
  }
  return PR_TRUE;
}


PRBool
nsNativeThemeCocoa::ThemeDrawsFocusForWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType)
{
  if (aWidgetType == NS_THEME_DROPDOWN ||
      aWidgetType == NS_THEME_BUTTON ||
      aWidgetType == NS_THEME_RADIO ||
      aWidgetType == NS_THEME_RADIO_SMALL ||
      aWidgetType == NS_THEME_CHECKBOX ||
      aWidgetType == NS_THEME_CHECKBOX_SMALL)
    return PR_TRUE;

  return PR_FALSE;
}

PRBool
nsNativeThemeCocoa::ThemeNeedsComboboxDropmarker()
{
  return PR_FALSE;
}
