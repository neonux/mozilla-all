/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
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

/* rendering object for the HTML <video> element */

#include "nsHTMLParts.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"

#include "nsVideoFrame.h"
#include "nsHTMLVideoElement.h"
#include "nsIDOMHTMLVideoElement.h"
#include "nsDisplayList.h"
#include "nsIRenderingContext.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsPresContext.h"
#include "nsTransform2D.h"
#include "nsContentCreatorFunctions.h"
#include "nsBoxLayoutState.h"
#include "nsBoxFrame.h"

nsIFrame*
NS_NewHTMLVideoFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsVideoFrame(aContext);
}

nsVideoFrame::nsVideoFrame(nsStyleContext* aContext) :
  nsContainerFrame(aContext)
{
}

nsVideoFrame::~nsVideoFrame()
{
}

NS_QUERYFRAME_HEAD(nsVideoFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
#ifdef DEBUG
  NS_QUERYFRAME_ENTRY(nsIFrameDebug)
#endif
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

nsresult
nsVideoFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  // Set up "videocontrols" XUL element which will be XBL-bound to the
  // actual controls.
  nsPresContext* presContext = PresContext();
  nsNodeInfoManager *nodeInfoManager =
    presContext->Document()->NodeInfoManager();
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = nodeInfoManager->GetNodeInfo(nsGkAtoms::videocontrols, nsnull,
                                          kNameSpaceID_XUL);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = NS_NewElement(getter_AddRefs(mVideoControls), kNameSpaceID_XUL, nodeInfo, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!aElements.AppendElement(mVideoControls))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

void
nsVideoFrame::Destroy()
{
  nsContentUtils::DestroyAnonymousContent(&mVideoControls);
  nsContainerFrame::Destroy();
}

PRBool
nsVideoFrame::IsLeaf() const
{
  return PR_TRUE;
}

// Return the largest rectangle that fits in aRect and has the
// same aspect ratio as aRatio, centered at the center of aRect
static gfxRect
CorrectForAspectRatio(const gfxRect& aRect, const nsIntSize& aRatio)
{
  NS_ASSERTION(aRatio.width > 0 && aRatio.height > 0 && !aRect.IsEmpty(),
               "Nothing to draw");
  // Choose scale factor that scales aRatio to just fit into aRect
  gfxFloat scale =
    PR_MIN(aRect.Width()/aRatio.width, aRect.Height()/aRatio.height);
  gfxSize scaledRatio(scale*aRatio.width, scale*aRatio.height);
  gfxPoint topLeft((aRect.Width() - scaledRatio.width)/2,
                   (aRect.Height() - scaledRatio.height)/2);
  return gfxRect(aRect.TopLeft() + topLeft, scaledRatio);
}

void
nsVideoFrame::PaintVideo(nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect, nsPoint aPt) 
{
  nsRect area = GetContentRect() - GetPosition() + aPt;
  nsHTMLVideoElement* element = static_cast<nsHTMLVideoElement*>(GetContent());
  nsIntSize videoSize = element->GetVideoSize(nsIntSize(0, 0));
  if (videoSize.width <= 0 || videoSize.height <= 0 || area.IsEmpty())
    return;

  gfxContext* ctx = static_cast<gfxContext*>(aRenderingContext.GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT));
  nsPresContext* presContext = PresContext();
  gfxRect r = gfxRect(presContext->AppUnitsToGfxUnits(area.x), 
                      presContext->AppUnitsToGfxUnits(area.y), 
                      presContext->AppUnitsToGfxUnits(area.width), 
                      presContext->AppUnitsToGfxUnits(area.height));

  r = CorrectForAspectRatio(r, videoSize);
  element->Paint(ctx, r);
}

NS_IMETHODIMP
nsVideoFrame::Reflow(nsPresContext*           aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsVideoFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsVideoFrame::Reflow: availSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus = NS_FRAME_COMPLETE;

  aMetrics.width = aReflowState.ComputedWidth();
  aMetrics.height = aReflowState.ComputedHeight();

  // stash this away so we can compute our inner area later
  mBorderPadding   = aReflowState.mComputedBorderPadding;

  aMetrics.width += mBorderPadding.left + mBorderPadding.right;
  aMetrics.height += mBorderPadding.top + mBorderPadding.bottom;

  nsIFrame* child = mFrames.FirstChild();
  if (child) {
    NS_ASSERTION(child->GetContent() == mVideoControls,
                 "What is this child doing here?");
    nsBoxLayoutState boxState(PresContext(), aReflowState.rendContext);
    nsBoxFrame::LayoutChildAt(boxState, child,
                              nsRect(mBorderPadding.left, mBorderPadding.top,
                                     aReflowState.ComputedWidth(), aReflowState.ComputedHeight()));
  }

  aMetrics.mOverflowArea.SetRect(0, 0, aMetrics.width, aMetrics.height);

  FinishAndStoreOverflow(&aMetrics);

  if (mRect.width != aMetrics.width || mRect.height != aMetrics.height) {
    Invalidate(nsRect(0, 0, mRect.width, mRect.height));
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsVideoFrame::Reflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);

  return NS_OK;
}

static void PaintVideo(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                        const nsRect& aDirtyRect, nsPoint aPt)
{
#if 0
  double start = double(PR_IntervalToMilliseconds(PR_IntervalNow()))/1000.0;
#endif

  static_cast<nsVideoFrame*>(aFrame)->PaintVideo(*aCtx, aDirtyRect, aPt);
#if 0
  double end = double(PR_IntervalToMilliseconds(PR_IntervalNow()))/1000.0;
  printf("PaintVideo: %f\n", (float)end - (float)start);

#endif
}

NS_IMETHODIMP
nsVideoFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsVideoFrame");

  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (HasVideoData()) {
    rv = aLists.Content()->AppendNewToTop(new (aBuilder) nsDisplayGeneric(this, ::PaintVideo, "Video"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsIFrame *kid = mFrames.FirstChild();
  if (kid) {
    rv = kid->BuildDisplayListForStackingContext(aBuilder, aDirtyRect - kid->GetOffsetTo(this), aLists.Content());
  }
  return rv;
}

nsIAtom*
nsVideoFrame::GetType() const
{
  return nsGkAtoms::HTMLVideoFrame;
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP
nsVideoFrame::GetAccessible(nsIAccessible** aAccessible)
{
  return NS_ERROR_FAILURE;
}
#endif

#ifdef DEBUG
NS_IMETHODIMP
nsVideoFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("HTMLVideo"), aResult);
}
#endif

nsSize nsVideoFrame::ComputeSize(nsIRenderingContext *aRenderingContext,
                                     nsSize aCBSize, 
                                     nscoord aAvailableWidth,
                                     nsSize aMargin, 
                                     nsSize aBorder, 
                                     nsSize aPadding,
                                     PRBool aShrinkWrap)
{
  nsSize size = GetIntrinsicSize(aRenderingContext);

  IntrinsicSize intrinsicSize;
  intrinsicSize.width.SetCoordValue(size.width);
  intrinsicSize.height.SetCoordValue(size.height);

  nsSize& intrinsicRatio = size; // won't actually be used

  return nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(aRenderingContext, 
                                                           this,
                                                           intrinsicSize,
                                                           intrinsicRatio,
                                                           aCBSize, 
                                                           aMargin, 
                                                           aBorder, 
                                                           aPadding);
}

nscoord nsVideoFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = GetIntrinsicSize(aRenderingContext).width;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

nscoord nsVideoFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = GetIntrinsicSize(aRenderingContext).width;
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

nsSize nsVideoFrame::GetIntrinsicRatio()
{
  return GetIntrinsicSize(nsnull);
}

nsSize nsVideoFrame::GetIntrinsicSize(nsIRenderingContext *aRenderingContext)
{
  // Defaulting size to 300x150 if no size given.
  nsIntSize size(300,150);

  if (!HasVideoData()) {
    if (!aRenderingContext || !mFrames.FirstChild()) {
      // We just want our intrinsic ratio, but audio elements need no
      // intrinsic ratio, so just return "no ratio". Also, if there's
      // no controls frame, we prefer to be zero-sized.
      return nsSize(0, 0);
    }

    // Ask the controls frame what its preferred height is
    nsBoxLayoutState boxState(PresContext(), aRenderingContext, 0);
    nscoord prefHeight = mFrames.FirstChild()->GetPrefSize(boxState).height;
    return nsSize(nsPresContext::CSSPixelsToAppUnits(size.width), prefHeight);
  }

  nsHTMLVideoElement* element = static_cast<nsHTMLVideoElement*>(GetContent());
  if (element) {
    nsresult rv;

    size = element->GetVideoSize(size);
  }

  return nsSize(nsPresContext::CSSPixelsToAppUnits(size.width), 
                nsPresContext::CSSPixelsToAppUnits(size.height));
}

PRBool nsVideoFrame::HasVideoData()
{
  nsCOMPtr<nsIDOMHTMLVideoElement> videoElem = do_QueryInterface(mContent);
  return videoElem != nsnull;
}
