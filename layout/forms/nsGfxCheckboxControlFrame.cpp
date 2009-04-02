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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "nsGfxCheckboxControlFrame.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsCSSRendering.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsDisplayList.h"
#include "nsCSSAnonBoxes.h"
#include "nsIDOMNSHTMLInputElement.h"

static void
PaintCheckMark(nsIRenderingContext& aRenderingContext,
               const nsRect& aRect)
{
  // Points come from the coordinates on a 7X7 unit box centered at 0,0
  const PRInt32 checkPolygonX[] = { -3, -1,  3,  3, -1, -3 };
  const PRInt32 checkPolygonY[] = { -1,  1, -3, -1,  3,  1 };
  const PRInt32 checkNumPoints = sizeof(checkPolygonX) / sizeof(PRInt32);
  const PRInt32 checkSize      = 9; // This is value is determined by adding 2
                                    // units to pad the 7x7 unit checkmark

  // Scale the checkmark based on the smallest dimension
  nscoord paintScale = PR_MIN(aRect.width, aRect.height) / checkSize;
  nsPoint paintCenter(aRect.x + aRect.width  / 2,
                      aRect.y + aRect.height / 2);

  nsPoint paintPolygon[checkNumPoints];
  // Convert checkmark for screen rendering
  for (PRInt32 polyIndex = 0; polyIndex < checkNumPoints; polyIndex++) {
    paintPolygon[polyIndex] = paintCenter +
                              nsPoint(checkPolygonX[polyIndex] * paintScale,
                                      checkPolygonY[polyIndex] * paintScale);
  }

  aRenderingContext.FillPolygon(paintPolygon, checkNumPoints);
}

static void
PaintIndeterminateMark(nsIRenderingContext& aRenderingContext,
                       const nsRect& aRect)
{
  // Drawing a thin horizontal line in the middle of the rect.
  nsRect fillRect = aRect;
  fillRect.height /= 4;
  fillRect.y += (aRect.height - fillRect.height) / 2;

  aRenderingContext.FillRect(fillRect);
}

//------------------------------------------------------------
nsIFrame*
NS_NewGfxCheckboxControlFrame(nsIPresShell* aPresShell,
                              nsStyleContext* aContext)
{
  return new (aPresShell) nsGfxCheckboxControlFrame(aContext);
}


//------------------------------------------------------------
// Initialize GFX-rendered state
nsGfxCheckboxControlFrame::nsGfxCheckboxControlFrame(nsStyleContext* aContext)
: nsFormControlFrame(aContext)
{
}

nsGfxCheckboxControlFrame::~nsGfxCheckboxControlFrame()
{
}


NS_QUERYFRAME_HEAD(nsGfxCheckboxControlFrame)
  NS_QUERYFRAME_ENTRY(nsICheckboxControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFormControlFrame)

#ifdef ACCESSIBILITY
NS_IMETHODIMP
nsGfxCheckboxControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService
    = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLCheckboxAccessible(
      static_cast<nsIFrame*>(this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

//------------------------------------------------------------
NS_IMETHODIMP
nsGfxCheckboxControlFrame::OnChecked(nsPresContext* aPresContext,
                                     PRBool aChecked)
{
  InvalidateOverflowRect();
  return NS_OK;
}

class nsDisplayCheckMark : public nsDisplayItem {
public:
  nsDisplayCheckMark(nsGfxCheckboxControlFrame* aFrame)
    : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayCheckMark);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayCheckMark() {
    MOZ_COUNT_DTOR(nsDisplayCheckMark);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("CheckMark")
};

void
nsDisplayCheckMark::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  static_cast<nsGfxCheckboxControlFrame*>(mFrame)->
    PaintCheckBox(*aCtx, aBuilder->ToReferenceFrame(mFrame), aDirtyRect);
}


//------------------------------------------------------------
void
nsGfxCheckboxControlFrame::PaintCheckBox(nsIRenderingContext& aRenderingContext,
                                         nsPoint aPt,
                                         const nsRect& aDirtyRect)
{
  // REVIEW: moved the mAppearance test out so we avoid constructing
  // a display item if it's not needed
  nsRect checkRect(aPt, mRect.Size());
  checkRect.Deflate(GetUsedBorderAndPadding());

  const nsStyleColor* color = GetStyleColor();
  aRenderingContext.SetColor(color->mColor);

  if (IsIndeterminate())
    PaintIndeterminateMark(aRenderingContext, checkRect);
  else
    PaintCheckMark(aRenderingContext, checkRect);
}

//------------------------------------------------------------
NS_IMETHODIMP
nsGfxCheckboxControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  nsresult rv = nsFormControlFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Get current checked state through content model.
  if ((!IsChecked() && !IsIndeterminate()) || !IsVisibleForPainting(aBuilder))
    return NS_OK;   // we're not checked or not visible, nothing to paint.
    
  if (IsThemed())
    return NS_OK; // No need to paint the checkmark. The theme will do it.

  return aLists.Content()->AppendNewToTop(new (aBuilder)
                                          nsDisplayCheckMark(this));
}

//------------------------------------------------------------
PRBool
nsGfxCheckboxControlFrame::IsChecked()
{
  nsCOMPtr<nsIDOMHTMLInputElement> elem(do_QueryInterface(mContent));
  PRBool retval = PR_FALSE;
  elem->GetChecked(&retval);
  return retval;
}

PRBool
nsGfxCheckboxControlFrame::IsIndeterminate()
{
  nsCOMPtr<nsIDOMNSHTMLInputElement> elem(do_QueryInterface(mContent));
  PRBool retval = PR_FALSE;
  elem->GetIndeterminate(&retval);
  return retval;
}
