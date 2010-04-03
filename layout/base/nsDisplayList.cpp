/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     robert@ocallahan.org
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
 * ***** END LICENSE BLOCK *****
 */

/*
 * structures that represent things to be painted (ordered in z-order),
 * used during painting and hit testing
 */

#include "nsDisplayList.h"

#include "nsCSSRendering.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsRegion.h"
#include "nsFrameManager.h"
#include "gfxContext.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "gfxMatrix.h"
#ifdef MOZ_SVG
#include "nsSVGIntegrationUtils.h"
#endif
#include "nsLayoutUtils.h"

#include "imgIContainer.h"
#include "nsIInterfaceRequestorUtils.h"
#include "BasicLayers.h"

using namespace mozilla::layers;

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
    PRBool aIsForEvents, PRBool aBuildCaret)
    : mReferenceFrame(aReferenceFrame),
      mMovingFrame(nsnull),
      mSaveVisibleRegionOfMovingContent(nsnull),
      mIgnoreScrollFrame(nsnull),
      mCurrentTableItem(nsnull),
      mBuildCaret(aBuildCaret),
      mEventDelivery(aIsForEvents),
      mIsAtRootOfPseudoStackingContext(PR_FALSE),
      mPaintAllFrames(PR_FALSE),
      mAccurateVisibleRegions(PR_FALSE),
      mInTransform(PR_FALSE),
      mSyncDecodeImages(PR_FALSE) {
  PL_InitArenaPool(&mPool, "displayListArena", 1024, sizeof(void*)-1);

  nsPresContext* pc = aReferenceFrame->PresContext();
  nsIPresShell *shell = pc->PresShell();
  mIsBackgroundOnly = shell->IsPaintingSuppressed();
  if (pc->IsRenderingOnlySelection()) {
    nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
    if (selcon) {
      selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                           getter_AddRefs(mBoundingSelection));
    }
  }

  if (mIsBackgroundOnly) {
    mBuildCaret = PR_FALSE;
  }
}

static void MarkFrameForDisplay(nsIFrame* aFrame, nsIFrame* aStopAtFrame) {
  nsFrameManager* frameManager = aFrame->PresContext()->PresShell()->FrameManager();

  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderFor(frameManager, f)) {
    if (f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)
      return;
    f->AddStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

static void MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame, nsIFrame* aFrame,
                                         const nsRect& aDirtyRect) {
  nsRect dirty = aDirtyRect - aFrame->GetOffsetTo(aDirtyFrame);
  nsRect overflowRect = aFrame->GetOverflowRect();
  if (!dirty.IntersectRect(dirty, overflowRect))
    return;
  aFrame->Properties().Set(nsDisplayListBuilder::OutOfFlowDirtyRectProperty(),
                           new nsRect(dirty));

  MarkFrameForDisplay(aFrame, aDirtyFrame);
}

static void UnmarkFrameForDisplay(nsIFrame* aFrame) {
  nsPresContext* presContext = aFrame->PresContext();
  presContext->PropertyTable()->
    Delete(aFrame, nsDisplayListBuilder::OutOfFlowDirtyRectProperty());

  nsFrameManager* frameManager = presContext->PresShell()->FrameManager();

  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderFor(frameManager, f)) {
    if (!(f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO))
      return;
    f->RemoveStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
  }
}

nsDisplayListBuilder::~nsDisplayListBuilder() {
  NS_ASSERTION(mFramesMarkedForDisplay.Length() == 0,
               "All frames should have been unmarked");
  NS_ASSERTION(mPresShellStates.Length() == 0,
               "All presshells should have been exited");
  NS_ASSERTION(!mCurrentTableItem, "No table item should be active");

  PL_FreeArenaPool(&mPool);
  PL_FinishArenaPool(&mPool);
}

PRUint32
nsDisplayListBuilder::GetBackgroundPaintFlags() {
  PRUint32 flags = 0;
  if (mSyncDecodeImages) {
    flags |= nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES;
  }
  return flags;
}

void
nsDisplayListBuilder::SubtractFromVisibleRegion(nsRegion* aVisibleRegion,
                                                const nsRegion& aRegion)
{
  nsRegion tmp;
  tmp.Sub(*aVisibleRegion, aRegion);
  // Don't let *aVisibleRegion get too complex, but don't let it fluff out
  // to its bounds either, which can be very bad (see bug 516740).
  if (GetAccurateVisibleRegions() || tmp.GetNumRects() <= 15) {
    *aVisibleRegion = tmp;
  }
}

PRBool
nsDisplayListBuilder::IsMovingFrame(nsIFrame* aFrame)
{
  return mMovingFrame &&
     nsLayoutUtils::IsAncestorFrameCrossDoc(mMovingFrame, aFrame, mReferenceFrame);
}

nsCaret *
nsDisplayListBuilder::GetCaret() {
  nsRefPtr<nsCaret> caret = CurrentPresShellState()->mPresShell->GetCaret();
  return caret;
}

void
nsDisplayListBuilder::EnterPresShell(nsIFrame* aReferenceFrame,
                                     const nsRect& aDirtyRect) {
  PresShellState* state = mPresShellStates.AppendElement();
  if (!state)
    return;
  state->mPresShell = aReferenceFrame->PresContext()->PresShell();
  state->mCaretFrame = nsnull;
  state->mFirstFrameMarkedForDisplay = mFramesMarkedForDisplay.Length();

  state->mPresShell->UpdateCanvasBackground();

  if (!mBuildCaret)
    return;

  nsRefPtr<nsCaret> caret = state->mPresShell->GetCaret();
  state->mCaretFrame = caret->GetCaretFrame();

  if (state->mCaretFrame) {
    // Check if the dirty rect intersects with the caret's dirty rect.
    nsRect caretRect =
      caret->GetCaretRect() + state->mCaretFrame->GetOffsetTo(aReferenceFrame);
    if (caretRect.Intersects(aDirtyRect)) {
      // Okay, our rects intersect, let's mark the frame and all of its ancestors.
      mFramesMarkedForDisplay.AppendElement(state->mCaretFrame);
      MarkFrameForDisplay(state->mCaretFrame, nsnull);
    }
  }
}

void
nsDisplayListBuilder::LeavePresShell(nsIFrame* aReferenceFrame,
                                     const nsRect& aDirtyRect) {
  if (CurrentPresShellState()->mPresShell != aReferenceFrame->PresContext()->PresShell()) {
    // Must have not allocated a state for this presshell, presumably due
    // to OOM.
    return;
  }

  // Unmark and pop off the frames marked for display in this pres shell.
  PRUint32 firstFrameForShell = CurrentPresShellState()->mFirstFrameMarkedForDisplay;
  for (PRUint32 i = firstFrameForShell;
       i < mFramesMarkedForDisplay.Length(); ++i) {
    UnmarkFrameForDisplay(mFramesMarkedForDisplay[i]);
  }
  mFramesMarkedForDisplay.SetLength(firstFrameForShell);
  mPresShellStates.SetLength(mPresShellStates.Length() - 1);
}

void
nsDisplayListBuilder::MarkFramesForDisplayList(nsIFrame* aDirtyFrame,
                                               const nsFrameList& aFrames,
                                               const nsRect& aDirtyRect) {
  for (nsFrameList::Enumerator e(aFrames); !e.AtEnd(); e.Next()) {
    mFramesMarkedForDisplay.AppendElement(e.get());
    MarkOutOfFlowFrameForDisplay(aDirtyFrame, e.get(), aDirtyRect);
  }
}

void*
nsDisplayListBuilder::Allocate(size_t aSize) {
  void *tmp;
  PL_ARENA_ALLOCATE(tmp, &mPool, aSize);
  return tmp;
}

void
nsDisplayListBuilder::AccumulateVisibleRegionOfMovingContent(const nsRegion& aMovingContent,
                                                             const nsRegion& aVisibleRegionBeforeMove,
                                                             const nsRegion& aVisibleRegionAfterMove)
{
  if (!mSaveVisibleRegionOfMovingContent)
    return;

  nsRegion beforeRegion = aMovingContent;
  beforeRegion.MoveBy(-mMoveDelta);
  beforeRegion.And(beforeRegion, aVisibleRegionBeforeMove);
  nsRegion afterRegion = aMovingContent;
  afterRegion.And(afterRegion, aVisibleRegionAfterMove);
  
  // Accumulate these regions into our result
  mSaveVisibleRegionOfMovingContent->Or(
      *mSaveVisibleRegionOfMovingContent, beforeRegion);
  mSaveVisibleRegionOfMovingContent->Or(
      *mSaveVisibleRegionOfMovingContent, afterRegion);
  mSaveVisibleRegionOfMovingContent->SimplifyOutward(15);
}

void nsDisplayListSet::MoveTo(const nsDisplayListSet& aDestination) const
{
  aDestination.BorderBackground()->AppendToTop(BorderBackground());
  aDestination.BlockBorderBackgrounds()->AppendToTop(BlockBorderBackgrounds());
  aDestination.Floats()->AppendToTop(Floats());
  aDestination.Content()->AppendToTop(Content());
  aDestination.PositionedDescendants()->AppendToTop(PositionedDescendants());
  aDestination.Outlines()->AppendToTop(Outlines());
}

void
nsDisplayList::FlattenTo(nsTArray<nsDisplayItem*>* aElements) {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nsnull) {
    if (item->GetType() == nsDisplayItem::TYPE_WRAPLIST) {
      item->GetList()->FlattenTo(aElements);
      item->~nsDisplayItem();
    } else {
      aElements->AppendElement(item);
    }
  }
}

nsRect
nsDisplayList::GetBounds(nsDisplayListBuilder* aBuilder) const {
  nsRect bounds;
  for (nsDisplayItem* i = GetBottom(); i != nsnull; i = i->GetAbove()) {
    bounds.UnionRect(bounds, i->GetBounds(aBuilder));
  }
  return bounds;
}

void
nsDisplayList::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion,
                                 nsRegion* aVisibleRegionBeforeMove) {
  NS_ASSERTION((aVisibleRegionBeforeMove != nsnull) == aBuilder->HasMovingFrames(),
               "Should have aVisibleRegionBeforeMove when there are moving frames");

  nsAutoTArray<nsDisplayItem*, 512> elements;
  FlattenTo(&elements);

  // Accumulate the bounds of all moving content we find in this list.
  // For speed, we store only a bounding box, not a region.
  nsRect movingContentAccumulatedBounds;
  // Store an overapproximation of the visible regions for the moving
  // content in this list
  nsRegion movingContentVisibleRegionBeforeMove;
  nsRegion movingContentVisibleRegionAfterMove;

  for (PRInt32 i = elements.Length() - 1; i >= 0; --i) {
    nsDisplayItem* item = elements[i];
    nsDisplayItem* belowItem = i < 1 ? nsnull : elements[i - 1];

    if (belowItem && item->TryMerge(aBuilder, belowItem)) {
      belowItem->~nsDisplayItem();
      elements.ReplaceElementsAt(i - 1, 1, item);
      continue;
    }

    nsRect bounds = item->GetBounds(aBuilder);

    nsIFrame* f = item->GetUnderlyingFrame();
    PRBool isMoving = f && aBuilder->IsMovingFrame(f);
    // Record bounds of moving visible items in movingContentAccumulatedBounds.
    // We do not need to add items that are uniform across the entire visible
    // area, since they have no visible movement.
    if (isMoving &&
        !(item->IsUniform(aBuilder) &&
          bounds.Contains(aVisibleRegion->GetBounds()) &&
          bounds.Contains(aVisibleRegionBeforeMove->GetBounds()))) {
      if (movingContentAccumulatedBounds.IsEmpty()) {
        // *aVisibleRegion can only shrink during this loop, so storing
        // the first one we see is a sound overapproximation
        movingContentVisibleRegionBeforeMove = *aVisibleRegionBeforeMove;
        movingContentVisibleRegionAfterMove = *aVisibleRegion;
      }
      nscoord appUnitsPerPixel = f->PresContext()->AppUnitsPerDevPixel();
      nsRect roundOutBounds = bounds.
          ToOutsidePixels(appUnitsPerPixel).ToAppUnits(appUnitsPerPixel);
      movingContentAccumulatedBounds.UnionRect(movingContentAccumulatedBounds,
                                               roundOutBounds);
    }

    nsRegion itemVisible;
    if (aVisibleRegionBeforeMove) {
      // Treat the item as visible if it was visible before or after the move.
      itemVisible.Or(*aVisibleRegion, *aVisibleRegionBeforeMove);
      itemVisible.And(itemVisible, bounds);
    } else {
      itemVisible.And(*aVisibleRegion, bounds);
    }
    item->mVisibleRect = itemVisible.GetBounds();

    if (!item->mVisibleRect.IsEmpty() &&
        item->ComputeVisibility(aBuilder, aVisibleRegion, aVisibleRegionBeforeMove)) {
      AppendToBottom(item);

      if (item->IsOpaque(aBuilder) && f) {
        // Subtract opaque item from the visible region
        aBuilder->SubtractFromVisibleRegion(aVisibleRegion, nsRegion(bounds));

        if (aVisibleRegionBeforeMove) {
          nsRect opaqueAreaBeforeMove =
            isMoving ? bounds - aBuilder->GetMoveDelta() : bounds;
          aBuilder->SubtractFromVisibleRegion(aVisibleRegionBeforeMove,
                                              nsRegion(opaqueAreaBeforeMove));
        }
      }
    } else {
      item->~nsDisplayItem();
    }
  }

  aBuilder->AccumulateVisibleRegionOfMovingContent(
    nsRegion(movingContentAccumulatedBounds),
    movingContentVisibleRegionBeforeMove,
    movingContentVisibleRegionAfterMove);

  mIsOpaque = aVisibleRegion->IsEmpty();
#ifdef DEBUG
  mDidComputeVisibility = PR_TRUE;
#endif
}

namespace {
/**
 * This class iterates through a display list tree, descending only into
 * nsDisplayClip items, and returns each display item encountered during
 * such iteration. Along with each item we also return the clip rect
 * accumulated for the item.
 */
class ClippedItemIterator {
public:
  ClippedItemIterator(const nsDisplayList* aList)
  {
    DescendIntoList(aList, nsnull, nsnull);
    AdvanceToItem();
  }
  PRBool IsDone()
  {
    return mStack.IsEmpty();
  }
  void Next()
  {
    State* top = StackTop();
    top->mItem = top->mItem->GetAbove();
    AdvanceToItem();
  }
  // Returns null if there is no clipping affecting the item. The
  // clip rect is in device pixels
  const gfxRect* GetEffectiveClipRect()
  {
    State* top = StackTop();
    return top->mHasClipRect ? &top->mEffectiveClipRect : nsnull;
  }
  nsDisplayItem* Item()
  {
    return StackTop()->mItem;
  }

private:
  // We maintain a stack of state objects. Each State object represents
  // where we're up to in the iteration of a list.
  struct State {
    // The current item we're at in the list
    nsDisplayItem* mItem;
    // The effective clip rect applying to all the items in this list
    gfxRect mEffectiveClipRect;
    PRPackedBool mHasClipRect;
  };

  State* StackTop()
  {
    return &mStack[mStack.Length() - 1];
  }
  void DescendIntoList(const nsDisplayList* aList,
                       nsPresContext* aPresContext,
                       const nsRect* aClipRect)
  {
    State* state = mStack.AppendElement();
    if (!state)
      return;
    if (mStack.Length() >= 2) {
      *state = mStack[mStack.Length() - 2];
    } else {
      state->mHasClipRect = PR_FALSE;
    }
    state->mItem = aList->GetBottom();
    if (aClipRect) {
      gfxRect r(aClipRect->x, aClipRect->y, aClipRect->width, aClipRect->height);
      r.ScaleInverse(aPresContext->AppUnitsPerDevPixel());
      if (state->mHasClipRect) {
        state->mEffectiveClipRect = state->mEffectiveClipRect.Intersect(r);
      } else {
        state->mEffectiveClipRect = r;
        state->mHasClipRect = PR_TRUE;
      }
    }
  }
  // Advances to an item that the iterator should return.
  void AdvanceToItem()
  {
    while (!mStack.IsEmpty()) {
      State* top = StackTop();
      if (!top->mItem) {
        mStack.SetLength(mStack.Length() - 1);
        if (!mStack.IsEmpty()) {
          top = StackTop();
          top->mItem = top->mItem->GetAbove();
        }
        continue;
      }
      if (top->mItem->GetType() != nsDisplayItem::TYPE_CLIP)
        return;
      nsDisplayClip* clipItem = static_cast<nsDisplayClip*>(top->mItem);
      nsRect clip = clipItem->GetClipRect();
      DescendIntoList(clipItem->GetList(),
                      clipItem->GetClippingFrame()->PresContext(),
                      &clip);
    }
  }

  nsAutoTArray<State,10> mStack;
};
}

/**
 * Given a (possibly clipped) display item in aItem, try to append it to
 * the items in aGroup. If aItem is the next item in the sublist in
 * aGroup, and the clipping matches, we can just update aGroup in-place,
 * otherwise we'll allocate a new ItemGroup, add it to the linked list,
 * and put aItem in the new ItemGroup. We return the ItemGroup into which
 * aItem was inserted.
 */
static nsDisplayList::ItemGroup*
AddToItemGroup(nsDisplayListBuilder* aBuilder,
               nsDisplayList::ItemGroup* aGroup, nsDisplayItem* aItem,
               const gfxRect* aClipRect) {
  NS_ASSERTION(!aGroup->mNextItemsForLayer,
               "aGroup must be the last group in the chain");

  if (!aGroup->mStartItem) {
    aGroup->mStartItem = aItem;
    aGroup->mEndItem = aItem->GetAbove();
    aGroup->mHasClipRect = aClipRect != nsnull;
    if (aClipRect) {
      aGroup->mClipRect = *aClipRect;
    }
    return aGroup;
  }

  if (aGroup->mEndItem == aItem &&
      (aGroup->mHasClipRect
       ? (aClipRect && aGroup->mClipRect == *aClipRect)
       : !aClipRect))  {
    aGroup->mEndItem = aItem->GetAbove();
    return aGroup;
  }

  nsDisplayList::ItemGroup* itemGroup =
    new (aBuilder) nsDisplayList::ItemGroup();
  if (!itemGroup)
    return aGroup;
  aGroup->mNextItemsForLayer = itemGroup;
  return AddToItemGroup(aBuilder, itemGroup, aItem, aClipRect);
}

/**
 * Create an empty Thebes layer, with an empty ItemGroup associated with
 * it, and append it to aLayers.
 */
static nsDisplayList::ItemGroup*
CreateEmptyThebesLayer(nsDisplayListBuilder* aBuilder,
                       LayerManager* aManager,
                       nsTArray<nsDisplayList::LayerItems>* aLayers) {
  nsDisplayList::ItemGroup* itemGroup =
    new (aBuilder) nsDisplayList::ItemGroup();
  if (!itemGroup)
    return nsnull;
  nsRefPtr<ThebesLayer> thebesLayer =
    aManager->CreateThebesLayer();
  if (!thebesLayer)
    return nsnull;
  nsDisplayList::LayerItems* layerItems =
    aLayers->AppendElement(nsDisplayList::LayerItems(itemGroup));
  layerItems->mThebesLayer = thebesLayer;
  layerItems->mLayer = thebesLayer.forget();
  return itemGroup;
}

/**
 * This is the heart of layout's integration with layers. We
 * use a ClippedItemIterator to iterate through descendant display
 * items. Each item either has its own layer or is assigned to a
 * ThebesLayer. We create ThebesLayers as necessary, although we try
 * to put items in the bottom-most ThebesLayer because that is most
 * likely to be able to render with an opaque background, which will often
 * be required for subpixel text antialiasing to work.
 */
void nsDisplayList::BuildLayers(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                nsTArray<LayerItems>* aLayers) const {
  NS_ASSERTION(aLayers->IsEmpty(), "aLayers must be initially empty");

  // Create "bottom" Thebes layer. We'll try to put as much content
  // as possible in this layer because if the container is filled with
  // opaque content, this bottommost layer can also be treated as opaque,
  // which means content in this layer can have subpixel AA.
  // firstThebesLayerItems always points to the last ItemGroup for the
  // first Thebes layer.
  ItemGroup* firstThebesLayerItems =
    CreateEmptyThebesLayer(aBuilder, aManager, aLayers);
  if (!firstThebesLayerItems)
    return;
  // lastThebesLayerItems always points to the last ItemGroup for the
  // topmost layer, if it's a ThebesLayer. If the top layer is not a
  // Thebes layer, this is null.
  ItemGroup* lastThebesLayerItems = firstThebesLayerItems;
  // This region contains the bounds of all the content that is above
  // the first Thebes layer.
  nsRegion areaAboveFirstThebesLayer;

  for (ClippedItemIterator iter(this); !iter.IsDone(); iter.Next()) {
    nsDisplayItem* item = iter.Item();
    const gfxRect* clipRect = iter.GetEffectiveClipRect();
    // Ask the item if it manages its own layer
    nsRefPtr<Layer> layer = item->BuildLayer(aBuilder, aManager);
    nsRect bounds = item->GetBounds(aBuilder);
    // We set layerItems to point to the LayerItems object where the
    // item ends up.
    LayerItems* layerItems = nsnull;
    if (layer) {
      // item has a dedicated layer. Add it to the list, with an ItemGroup
      // covering this item only.
      ItemGroup* itemGroup = new (aBuilder) ItemGroup();
      if (itemGroup) {
        AddToItemGroup(aBuilder, itemGroup, item, clipRect);
        layerItems = aLayers->AppendElement(LayerItems(itemGroup));
        if (layerItems) {
          if (itemGroup->mHasClipRect) {
            gfxRect r = itemGroup->mClipRect;
            r.Round();
            nsIntRect intRect(r.X(), r.Y(), r.Width(), r.Height());
            layer->IntersectClipRect(intRect);
          }
          layerItems->mLayer = layer.forget();
        }
      }
      // This item is above the first Thebes layer.
      areaAboveFirstThebesLayer.Or(areaAboveFirstThebesLayer, bounds);
      lastThebesLayerItems = nsnull;
    } else {
      // No dedicated layer. Add it to a Thebes layer. First try to add
      // it to the first Thebes layer, which we can do if there's no
      // content between the first Thebes layer and our display item that
      // overlaps our display item.
      if (!areaAboveFirstThebesLayer.Intersects(bounds)) {
        firstThebesLayerItems =
          AddToItemGroup(aBuilder, firstThebesLayerItems, item, clipRect);
        layerItems = &aLayers->ElementAt(0);
      } else if (lastThebesLayerItems) {
        // Try to add to the last Thebes layer
        lastThebesLayerItems =
          AddToItemGroup(aBuilder, lastThebesLayerItems, item, clipRect);
        // This item is above the first Thebes layer.
        areaAboveFirstThebesLayer.Or(areaAboveFirstThebesLayer, bounds);
        layerItems = &aLayers->ElementAt(aLayers->Length() - 1);
      } else {
        // Create a new Thebes layer
        ItemGroup* itemGroup =
          CreateEmptyThebesLayer(aBuilder, aManager, aLayers);
        if (itemGroup) {
          lastThebesLayerItems =
            AddToItemGroup(aBuilder, itemGroup, item, clipRect);
          NS_ASSERTION(lastThebesLayerItems == itemGroup,
                       "AddToItemGroup shouldn't create a new group if the "
                       "initial group is empty");
          // This item is above the first Thebes layer.
          areaAboveFirstThebesLayer.Or(areaAboveFirstThebesLayer, bounds);
        }
      }
    }

    if (layerItems) {
      // Update the visible region of the layer to account for the new
      // item
      nscoord appUnitsPerDevPixel =
        item->GetUnderlyingFrame()->PresContext()->AppUnitsPerDevPixel();
      layerItems->mVisibleRect.UnionRect(layerItems->mVisibleRect,
        item->mVisibleRect.ToNearestPixels(appUnitsPerDevPixel));
    }
  }

  if (!firstThebesLayerItems->mStartItem) {
    // The first Thebes layer has nothing in it. Delete the layer.
    aLayers->RemoveElementAt(0);
  }

  for (PRUint32 i = 0; i < aLayers->Length(); ++i) {
    LayerItems* layerItems = &aLayers->ElementAt(i);

    gfxMatrix transform;
    nsIntRect visibleRect = layerItems->mVisibleRect;
    if (layerItems->mLayer->GetTransform().Is2D(&transform)) {
      // if 'transform' is not invertible, then nothing will be displayed
      // for the layer, so it doesn't really matter what we do here
      transform.Invert();
      gfxRect layerVisible = transform.TransformBounds(
          gfxRect(visibleRect.x, visibleRect.y, visibleRect.width, visibleRect.height));
      layerVisible.RoundOut();
      if (NS_FAILED(nsLayoutUtils::GfxRectToIntRect(layerVisible, &visibleRect))) {
        NS_ERROR("Visible rect transformed out of bounds");
      }
    } else {
      NS_ERROR("Only 2D transformations currently supported");
    }
    layerItems->mLayer->SetVisibleRegion(nsIntRegion(visibleRect));
  }
}

/**
 * We build a single layer by first building a list of layers needed for
 * all the display items, and then if there's not just one layer in the
 * list, we build a container layer to hold them.
 */
already_AddRefed<Layer>
nsDisplayList::BuildLayer(nsDisplayListBuilder* aBuilder,
                          LayerManager* aManager,
                          nsTArray<LayerItems>* aLayers) const {
  BuildLayers(aBuilder, aManager, aLayers);

  nsRefPtr<Layer> layer;
  if (aLayers->Length() == 1) {
    // We can just return the one layer
    layer = aLayers->ElementAt(0).mLayer;
  } else {
    // We need to group multiple layers together into a container
    nsRefPtr<ContainerLayer> container =
      aManager->CreateContainerLayer();
    if (!container)
      return nsnull;
    
    Layer* lastChild = nsnull;
    nsIntRect visibleRect;
    for (PRUint32 i = 0; i < aLayers->Length(); ++i) {
      LayerItems* layerItems = &aLayers->ElementAt(i);
      visibleRect.UnionRect(visibleRect, layerItems->mVisibleRect);
      Layer* child = layerItems->mLayer;
      container->InsertAfter(child, lastChild);
      lastChild = child;
    }
    container->SetVisibleRegion(nsIntRegion(visibleRect));
    layer = container.forget();
  }
  layer->SetIsOpaqueContent(mIsOpaque);
  return layer.forget();
}

/**
 * We paint by executing a layer manager transaction, constructing a
 * single layer representing the display list, and then making it the
 * root of the layer manager, drawing into the ThebesLayers.
 */
void nsDisplayList::Paint(nsDisplayListBuilder* aBuilder,
                          nsIRenderingContext* aCtx,
                          PRUint32 aFlags) const {
  NS_ASSERTION(mDidComputeVisibility,
               "Must call ComputeVisibility before calling Paint");

  nsRefPtr<LayerManager> layerManager;
  if (aFlags & PAINT_USE_WIDGET_LAYERS) {
    nsIFrame* referenceFrame = aBuilder->ReferenceFrame();
    NS_ASSERTION(referenceFrame == nsLayoutUtils::GetDisplayRootFrame(referenceFrame),
                 "Reference frame must be a display root for us to use the layer manager");
    nsIWidget* window = referenceFrame->GetWindow();
    if (window) {
      layerManager = window->GetLayerManager();
    }
  }
  if (!layerManager) {
    if (!aCtx) {
      NS_WARNING("Nowhere to paint into");
      return;
    }
    layerManager = new BasicLayerManager(aCtx->ThebesContext());
    if (!layerManager)
      return;
  }

  if (aCtx) {
    layerManager->BeginTransactionWithTarget(aCtx->ThebesContext());
  } else {
    layerManager->BeginTransaction();
  }

  nsAutoTArray<LayerItems,10> layers;
  nsRefPtr<Layer> root = BuildLayer(aBuilder, layerManager, &layers);
  if (!root)
    return;

  layerManager->SetRoot(root);
  layerManager->EndConstruction();
  PaintThebesLayers(aBuilder, layers);
  layerManager->EndTransaction();

  nsCSSRendering::DidPaint();
}

void
nsDisplayList::PaintThebesLayers(nsDisplayListBuilder* aBuilder,
                                 const nsTArray<LayerItems>& aLayers) const
{
  for (PRUint32 i = 0; i < aLayers.Length(); ++i) {
    ThebesLayer* thebesLayer = aLayers[i].mThebesLayer;
    if (!thebesLayer) {
      // Just ask the display item to paint any Thebes layers that it
      // used to construct its layer
      aLayers[i].mItems->mStartItem->PaintThebesLayers(aBuilder);
      continue;
    }

    // OK, we have a real Thebes layer. Start drawing into it.
    nsIntRegion toDraw;
    gfxContext* ctx = thebesLayer->BeginDrawing(&toDraw);
    if (!ctx)
      continue;
    // For now, we'll ignore toDraw and just draw the entire visible
    // area, because the "visible area" is already confined to just the
    // area that needs to be repainted. Later, when we start reusing layers
    // from paint to paint, we'll need to pay attention to toDraw and
    // actually try to avoid drawing stuff that's not in it.

    // Our list may contain content with different prescontexts at
    // different zoom levels. 'rc' contains the nsIRenderingContext
    // used for the previous display item, and lastPresContext is the
    // prescontext for that item. We also cache the clip state for that
    // item.
    nsRefPtr<nsIRenderingContext> rc;
    nsPresContext* lastPresContext = nsnull;
    gfxRect currentClip;
    PRBool setClipRect = PR_FALSE;
    NS_ASSERTION(aLayers[i].mItems, "No empty layers allowed");
    for (ItemGroup* group = aLayers[i].mItems; group;
         group = group->mNextItemsForLayer) {
      // If the new desired clip state is different from the current state,
      // update the clip.
      if (setClipRect != group->mHasClipRect ||
          (group->mHasClipRect && group->mClipRect != currentClip)) {
        if (setClipRect) {
          ctx->Restore();
        }
        setClipRect = group->mHasClipRect;
        if (setClipRect) {
          ctx->Save();
          ctx->NewPath();
          ctx->Rectangle(group->mClipRect, PR_TRUE);
          ctx->Clip();
          currentClip = group->mClipRect;
        }
      }
      NS_ASSERTION(group->mStartItem, "No empty groups allowed");
      for (nsDisplayItem* item = group->mStartItem; item != group->mEndItem;
           item = item->GetAbove()) {
        nsPresContext* presContext = item->GetUnderlyingFrame()->PresContext();
        if (presContext != lastPresContext) {
          // Create a new rendering context with the right
          // appunits-per-dev-pixel.
          nsresult rv =
            presContext->DeviceContext()->CreateRenderingContextInstance(*getter_AddRefs(rc));
          if (NS_FAILED(rv))
            break;
          rc->Init(presContext->DeviceContext(), ctx);
          lastPresContext = presContext;
        }
        item->Paint(aBuilder, rc);
      }
    }
    if (setClipRect) {
      ctx->Restore();
    }
    thebesLayer->EndDrawing();
  }
}

PRUint32 nsDisplayList::Count() const {
  PRUint32 count = 0;
  for (nsDisplayItem* i = GetBottom(); i; i = i->GetAbove()) {
    ++count;
  }
  return count;
}

nsDisplayItem* nsDisplayList::RemoveBottom() {
  nsDisplayItem* item = mSentinel.mAbove;
  if (!item)
    return nsnull;
  mSentinel.mAbove = item->mAbove;
  if (item == mTop) {
    // must have been the only item
    mTop = &mSentinel;
  }
  item->mAbove = nsnull;
  return item;
}

void nsDisplayList::DeleteBottom() {
  nsDisplayItem* item = RemoveBottom();
  if (item) {
    item->~nsDisplayItem();
  }
}

void nsDisplayList::DeleteAll() {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nsnull) {
    item->~nsDisplayItem();
  }
}

nsIFrame* nsDisplayList::HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt,
                                 nsDisplayItem::HitTestState* aState) const {
  PRInt32 itemBufferStart = aState->mItemBuffer.Length();
  nsDisplayItem* item;
  for (item = GetBottom(); item; item = item->GetAbove()) {
    aState->mItemBuffer.AppendElement(item);
  }
  for (PRInt32 i = aState->mItemBuffer.Length() - 1; i >= itemBufferStart; --i) {
    // Pop element off the end of the buffer. We want to shorten the buffer
    // so that recursive calls to HitTest have more buffer space.
    item = aState->mItemBuffer[i];
    aState->mItemBuffer.SetLength(i);

    if (item->GetBounds(aBuilder).Contains(aPt)) {
      nsIFrame* f = item->HitTest(aBuilder, aPt, aState);
      // Handle the XUL 'mousethrough' feature and 'pointer-events'.
      if (f) {
        if (!f->GetMouseThrough() &&
            f->GetStyleVisibility()->mPointerEvents != NS_STYLE_POINTER_EVENTS_NONE) {
          aState->mItemBuffer.SetLength(itemBufferStart);
          return f;
        }
      }
    }
  }
  NS_ASSERTION(aState->mItemBuffer.Length() == PRUint32(itemBufferStart),
               "How did we forget to pop some elements?");
  return nsnull;
}

static void Sort(nsDisplayList* aList, PRInt32 aCount, nsDisplayList::SortLEQ aCmp,
                 void* aClosure) {
  if (aCount < 2)
    return;

  nsDisplayList list1;
  nsDisplayList list2;
  int i;
  PRInt32 half = aCount/2;
  PRBool sorted = PR_TRUE;
  nsDisplayItem* prev = nsnull;
  for (i = 0; i < aCount; ++i) {
    nsDisplayItem* item = aList->RemoveBottom();
    (i < half ? &list1 : &list2)->AppendToTop(item);
    if (sorted && prev && !aCmp(prev, item, aClosure)) {
      sorted = PR_FALSE;
    }
    prev = item;
  }
  if (sorted) {
    aList->AppendToTop(&list1);
    aList->AppendToTop(&list2);
    return;
  }
  
  Sort(&list1, half, aCmp, aClosure);
  Sort(&list2, aCount - half, aCmp, aClosure);

  for (i = 0; i < aCount; ++i) {
    if (list1.GetBottom() &&
        (!list2.GetBottom() ||
         aCmp(list1.GetBottom(), list2.GetBottom(), aClosure))) {
      aList->AppendToTop(list1.RemoveBottom());
    } else {
      aList->AppendToTop(list2.RemoveBottom());
    }
  }
}

static PRBool IsContentLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                           void* aClosure) {
  // These GetUnderlyingFrame calls return non-null because we're only used
  // in sorting
  return nsLayoutUtils::CompareTreePosition(
      aItem1->GetUnderlyingFrame()->GetContent(),
      aItem2->GetUnderlyingFrame()->GetContent(),
      static_cast<nsIContent*>(aClosure)) <= 0;
}

static PRBool IsZOrderLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                          void* aClosure) {
  // These GetUnderlyingFrame calls return non-null because we're only used
  // in sorting.  Note that we can't just take the difference of the two
  // z-indices here, because that might overflow a 32-bit int.
  PRInt32 index1 = nsLayoutUtils::GetZIndex(aItem1->GetUnderlyingFrame());
  PRInt32 index2 = nsLayoutUtils::GetZIndex(aItem2->GetUnderlyingFrame());
  if (index1 == index2)
    return IsContentLEQ(aItem1, aItem2, aClosure);
  return index1 < index2;
}

void nsDisplayList::ExplodeAnonymousChildLists(nsDisplayListBuilder* aBuilder) {
  // See if there's anything to do
  PRBool anyAnonymousItems = PR_FALSE;
  nsDisplayItem* i;
  for (i = GetBottom(); i != nsnull; i = i->GetAbove()) {
    if (!i->GetUnderlyingFrame()) {
      anyAnonymousItems = PR_TRUE;
      break;
    }
  }
  if (!anyAnonymousItems)
    return;

  nsDisplayList tmp;
  while ((i = RemoveBottom()) != nsnull) {
    if (i->GetUnderlyingFrame()) {
      tmp.AppendToTop(i);
    } else {
      nsDisplayList* list = i->GetList();
      NS_ASSERTION(list, "leaf items can't be anonymous");
      list->ExplodeAnonymousChildLists(aBuilder);
      nsDisplayItem* j;
      while ((j = list->RemoveBottom()) != nsnull) {
        tmp.AppendToTop(static_cast<nsDisplayWrapList*>(i)->
            WrapWithClone(aBuilder, j));
      }
      i->~nsDisplayItem();
    }
  }
  
  AppendToTop(&tmp);
}

void nsDisplayList::SortByZOrder(nsDisplayListBuilder* aBuilder,
                                 nsIContent* aCommonAncestor) {
  Sort(aBuilder, IsZOrderLEQ, aCommonAncestor);
}

void nsDisplayList::SortByContentOrder(nsDisplayListBuilder* aBuilder,
                                       nsIContent* aCommonAncestor) {
  Sort(aBuilder, IsContentLEQ, aCommonAncestor);
}

void nsDisplayList::Sort(nsDisplayListBuilder* aBuilder,
                         SortLEQ aCmp, void* aClosure) {
  ExplodeAnonymousChildLists(aBuilder);
  ::Sort(this, Count(), aCmp, aClosure);
}

void nsDisplaySolidColor::Paint(nsDisplayListBuilder* aBuilder,
                                nsIRenderingContext* aCtx) {
  aCtx->SetColor(mColor);
  aCtx->FillRect(mVisibleRect);
}

// Returns TRUE if aContainedRect is guaranteed to be contained in
// the rounded rect defined by aRoundedRect and aRadii. Complex cases are
// handled conservatively by returning FALSE in some situations where
// a more thorough analysis could return TRUE.
static PRBool RoundedRectContainsRect(const nsRect& aRoundedRect,
                                      const nscoord aRadii[8],
                                      const nsRect& aContainedRect) {
  // rectFullHeight and rectFullWidth together will approximately contain
  // the total area of the frame minus the rounded corners.
  nsRect rectFullHeight = aRoundedRect;
  nscoord xDiff = NS_MAX(aRadii[NS_CORNER_TOP_LEFT_X], aRadii[NS_CORNER_BOTTOM_LEFT_X]);
  rectFullHeight.x += xDiff;
  rectFullHeight.width -= NS_MAX(aRadii[NS_CORNER_TOP_RIGHT_X],
                                 aRadii[NS_CORNER_BOTTOM_RIGHT_X]) + xDiff;
  if (rectFullHeight.Contains(aContainedRect))
    return PR_TRUE;

  nsRect rectFullWidth = aRoundedRect;
  nscoord yDiff = NS_MAX(aRadii[NS_CORNER_TOP_LEFT_Y], aRadii[NS_CORNER_TOP_RIGHT_Y]);
  rectFullWidth.y += yDiff;
  rectFullWidth.height -= NS_MAX(aRadii[NS_CORNER_BOTTOM_LEFT_Y],
                                 aRadii[NS_CORNER_BOTTOM_RIGHT_Y]) + yDiff;
  if (rectFullWidth.Contains(aContainedRect))
    return PR_TRUE;

  return PR_FALSE;
}

PRBool
nsDisplayBackground::IsOpaque(nsDisplayListBuilder* aBuilder) {
  // theme background overrides any other background
  if (mIsThemed)
    return PR_FALSE;

  nsStyleContext *bgSC;
  if (!nsCSSRendering::FindBackground(mFrame->PresContext(), mFrame, &bgSC))
    return PR_FALSE;
  const nsStyleBackground* bg = bgSC->GetStyleBackground();

  const nsStyleBackground::Layer& bottomLayer = bg->BottomLayer();

  // bottom layer's clip is used for the color
  if (bottomLayer.mClip != NS_STYLE_BG_CLIP_BORDER ||
      nsLayoutUtils::HasNonZeroCorner(mFrame->GetStyleBorder()->mBorderRadius))
    return PR_FALSE;

  if (NS_GET_A(bg->mBackgroundColor) == 255 &&
      !nsCSSRendering::IsCanvasFrame(mFrame))
    return PR_TRUE;

  return bottomLayer.mRepeat == NS_STYLE_BG_REPEAT_XY &&
         bottomLayer.mImage.IsOpaque();
}

PRBool
nsDisplayBackground::IsUniform(nsDisplayListBuilder* aBuilder) {
  // theme background overrides any other background
  if (mIsThemed)
    return PR_FALSE;

  nsStyleContext *bgSC;
  PRBool hasBG =
    nsCSSRendering::FindBackground(mFrame->PresContext(), mFrame, &bgSC);
  if (!hasBG)
    return PR_TRUE;
  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  if (bg->BottomLayer().mImage.IsEmpty() &&
      bg->mImageCount == 1 &&
      !nsLayoutUtils::HasNonZeroCorner(mFrame->GetStyleBorder()->mBorderRadius) &&
      bg->BottomLayer().mClip == NS_STYLE_BG_CLIP_BORDER)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool
nsDisplayBackground::IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder)
{
  NS_ASSERTION(aBuilder->IsMovingFrame(mFrame),
              "IsVaryingRelativeToMovingFrame called on non-moving frame!");

  nsPresContext* presContext = mFrame->PresContext();
  nsStyleContext *bgSC;
  PRBool hasBG =
    nsCSSRendering::FindBackground(mFrame->PresContext(), mFrame, &bgSC);
  if (!hasBG)
    return PR_FALSE;
  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  if (!bg->HasFixedBackground())
    return PR_FALSE;

  nsIFrame* movingFrame = aBuilder->GetRootMovingFrame();
  // movingFrame is the frame that is going to be moved. It must be equal
  // to mFrame or some ancestor of mFrame, see assertion above.
  // If mFrame is in the same document as movingFrame, then mFrame
  // will move relative to its viewport, which means this display item will
  // change when it is moved. If they are in different documents, we do not
  // want to return true because mFrame won't move relative to its viewport.
  return movingFrame->PresContext() == presContext;
}

void
nsDisplayBackground::Paint(nsDisplayListBuilder* aBuilder,
                           nsIRenderingContext* aCtx) {
  nsPoint offset = aBuilder->ToReferenceFrame(mFrame);
  PRUint32 flags = aBuilder->GetBackgroundPaintFlags();
  nsDisplayItem* nextItem = GetAbove();
  if (nextItem && nextItem->GetUnderlyingFrame() == mFrame &&
      nextItem->GetType() == TYPE_BORDER) {
    flags |= nsCSSRendering::PAINTBG_WILL_PAINT_BORDER;
  }
  nsCSSRendering::PaintBackground(mFrame->PresContext(), *aCtx, mFrame,
                                  mVisibleRect,
                                  nsRect(offset, mFrame->GetSize()),
                                  flags);
}

nsRect
nsDisplayBackground::GetBounds(nsDisplayListBuilder* aBuilder) {
  if (mIsThemed)
    return mFrame->GetOverflowRect() + aBuilder->ToReferenceFrame(mFrame);

  return nsRect(aBuilder->ToReferenceFrame(mFrame), mFrame->GetSize());
}

nsRect
nsDisplayOutline::GetBounds(nsDisplayListBuilder* aBuilder) {
  return mFrame->GetOverflowRect() + aBuilder->ToReferenceFrame(mFrame);
}

void
nsDisplayOutline::Paint(nsDisplayListBuilder* aBuilder,
                        nsIRenderingContext* aCtx) {
  // TODO join outlines together
  nsPoint offset = aBuilder->ToReferenceFrame(mFrame);
  nsCSSRendering::PaintOutline(mFrame->PresContext(), *aCtx, mFrame,
                               mVisibleRect,
                               nsRect(offset, mFrame->GetSize()),
                               *mFrame->GetStyleBorder(),
                               *mFrame->GetStyleOutline(),
                               mFrame->GetStyleContext());
}

PRBool
nsDisplayOutline::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion,
                                    nsRegion* aVisibleRegionBeforeMove) {
  NS_ASSERTION((aVisibleRegionBeforeMove != nsnull) == aBuilder->HasMovingFrames(),
               "Should have aVisibleRegionBeforeMove when there are moving frames");

  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aVisibleRegionBeforeMove))
    return PR_FALSE;

  const nsStyleOutline* outline = mFrame->GetStyleOutline();
  nsRect borderBox(aBuilder->ToReferenceFrame(mFrame), mFrame->GetSize());
  if (borderBox.Contains(aVisibleRegion->GetBounds()) &&
      (!aVisibleRegionBeforeMove ||
       borderBox.Contains(aVisibleRegionBeforeMove->GetBounds())) &&
      !nsLayoutUtils::HasNonZeroCorner(outline->mOutlineRadius)) {
    if (outline->mOutlineOffset >= 0) {
      // the visible region is entirely inside the border-rect, and the outline
      // isn't rendered inside the border-rect, so the outline is not visible
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

void
nsDisplayCaret::Paint(nsDisplayListBuilder* aBuilder,
                      nsIRenderingContext* aCtx) {
  // Note: Because we exist, we know that the caret is visible, so we don't
  // need to check for the caret's visibility.
  mCaret->PaintCaret(aBuilder, aCtx, mFrame, aBuilder->ToReferenceFrame(mFrame));
}

PRBool
nsDisplayBorder::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   nsRegion* aVisibleRegionBeforeMove) {
  NS_ASSERTION((aVisibleRegionBeforeMove != nsnull) == aBuilder->HasMovingFrames(),
               "Should have aVisibleRegionBeforeMove when there are moving frames");

  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aVisibleRegionBeforeMove))
    return PR_FALSE;

  nsRect paddingRect = mFrame->GetPaddingRect() - mFrame->GetPosition() +
    aBuilder->ToReferenceFrame(mFrame);
  const nsStyleBorder *styleBorder;
  if (paddingRect.Contains(aVisibleRegion->GetBounds()) &&
      (!aVisibleRegionBeforeMove ||
       paddingRect.Contains(aVisibleRegionBeforeMove->GetBounds())) &&
      !(styleBorder = mFrame->GetStyleBorder())->IsBorderImageLoaded() &&
      !nsLayoutUtils::HasNonZeroCorner(styleBorder->mBorderRadius)) {
    // the visible region is entirely inside the content rect, and no part
    // of the border is rendered inside the content rect, so we are not
    // visible
    // Skip this if there's a border-image (which draws a background
    // too) or if there is a border-radius (which makes the border draw
    // further in).
    return PR_FALSE;
  }

  return PR_TRUE;
}

void
nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder,
                       nsIRenderingContext* aCtx) {
  nsPoint offset = aBuilder->ToReferenceFrame(mFrame);
  nsCSSRendering::PaintBorder(mFrame->PresContext(), *aCtx, mFrame,
                              mVisibleRect,
                              nsRect(offset, mFrame->GetSize()),
                              *mFrame->GetStyleBorder(),
                              mFrame->GetStyleContext(),
                              mFrame->GetSkipSides());
}

// Given a region, compute a conservative approximation to it as a list
// of rectangles that aren't vertically adjacent (i.e., vertically
// adjacent or overlapping rectangles are combined).
// Right now this is only approximate, some vertically overlapping rectangles
// aren't guaranteed to be combined.
static void
ComputeDisjointRectangles(const nsRegion& aRegion,
                          nsTArray<nsRect>* aRects) {
  nscoord accumulationMargin = nsPresContext::CSSPixelsToAppUnits(25);
  nsRect accumulated;
  nsRegionRectIterator iter(aRegion);
  while (PR_TRUE) {
    const nsRect* r = iter.Next();
    if (r && !accumulated.IsEmpty() &&
        accumulated.YMost() >= r->y - accumulationMargin) {
      accumulated.UnionRect(accumulated, *r);
      continue;
    }

    if (!accumulated.IsEmpty()) {
      aRects->AppendElement(accumulated);
      accumulated.Empty();
    }

    if (!r)
      break;

    accumulated = *r;
  }
}

void
nsDisplayBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                               nsIRenderingContext* aCtx) {
  nsPoint offset = aBuilder->ToReferenceFrame(mFrame);
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  for (PRUint32 i = 0; i < rects.Length(); ++i) {
    aCtx->PushState();
    aCtx->SetClipRect(rects[i], nsClipCombine_kIntersect);
    nsCSSRendering::PaintBoxShadowOuter(presContext, *aCtx, mFrame,
                                        borderRect, rects[i]);
    aCtx->PopState();
  }
}

nsRect
nsDisplayBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder) {
  return mFrame->GetOverflowRect() + aBuilder->ToReferenceFrame(mFrame);
}

PRBool
nsDisplayBoxShadowOuter::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           nsRegion* aVisibleRegionBeforeMove) {
  NS_ASSERTION((aVisibleRegionBeforeMove != nsnull) == aBuilder->HasMovingFrames(),
               "Should have aVisibleRegionBeforeMove when there are moving frames");

  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aVisibleRegionBeforeMove))
    return PR_FALSE;

  // Store the actual visible region
  mVisibleRegion.And(*aVisibleRegion, mVisibleRect);

  nsPoint origin = aBuilder->ToReferenceFrame(mFrame);
  nsRect visibleBounds = aVisibleRegion->GetBounds();
  if (aVisibleRegionBeforeMove) {
    visibleBounds.UnionRect(visibleBounds, aVisibleRegionBeforeMove->GetBounds());
  }
  nsRect frameRect(origin, mFrame->GetSize());
  if (!frameRect.Contains(visibleBounds))
    return PR_TRUE;

  // the visible region is entirely inside the border-rect, and box shadows
  // never render within the border-rect (unless there's a border radius).
  nscoord twipsRadii[8];
  PRBool hasBorderRadii =
     nsCSSRendering::GetBorderRadiusTwips(mFrame->GetStyleBorder()->mBorderRadius,
                                          frameRect.width,
                                          twipsRadii);
  if (!hasBorderRadii)
    return PR_FALSE;

  return !RoundedRectContainsRect(frameRect, twipsRadii, visibleBounds);
}

void
nsDisplayBoxShadowInner::Paint(nsDisplayListBuilder* aBuilder,
                               nsIRenderingContext* aCtx) {
  nsPoint offset = aBuilder->ToReferenceFrame(mFrame);
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  for (PRUint32 i = 0; i < rects.Length(); ++i) {
    aCtx->PushState();
    aCtx->SetClipRect(rects[i], nsClipCombine_kIntersect);
    nsCSSRendering::PaintBoxShadowInner(presContext, *aCtx, mFrame,
                                        borderRect, rects[i]);
    aCtx->PopState();
  }
}

PRBool
nsDisplayBoxShadowInner::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           nsRegion* aVisibleRegionBeforeMove) {
  NS_ASSERTION((aVisibleRegionBeforeMove != nsnull) == aBuilder->HasMovingFrames(),
               "Should have aVisibleRegionBeforeMove when there are moving frames");

  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aVisibleRegionBeforeMove))
    return PR_FALSE;

  // Store the actual visible region
  mVisibleRegion.And(*aVisibleRegion, mVisibleRect);
  return PR_TRUE;
}

nsDisplayWrapList::nsDisplayWrapList(nsIFrame* aFrame, nsDisplayList* aList)
  : nsDisplayItem(aFrame) {
  mList.AppendToTop(aList);
}

nsDisplayWrapList::nsDisplayWrapList(nsIFrame* aFrame, nsDisplayItem* aItem)
  : nsDisplayItem(aFrame) {
  mList.AppendToTop(aItem);
}

nsDisplayWrapList::~nsDisplayWrapList() {
  mList.DeleteAll();
}

nsIFrame*
nsDisplayWrapList::HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt,
                           HitTestState* aState) {
  return mList.HitTest(aBuilder, aPt, aState);
}

nsRect
nsDisplayWrapList::GetBounds(nsDisplayListBuilder* aBuilder) {
  return mList.GetBounds(aBuilder);
}

PRBool
nsDisplayWrapList::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                     nsRegion* aVisibleRegion,
                                     nsRegion* aVisibleRegionBeforeMove) {
  mList.ComputeVisibility(aBuilder, aVisibleRegion, aVisibleRegionBeforeMove);
  // If none of the items are visible, they will all have been deleted
  return mList.GetTop() != nsnull;
}

PRBool
nsDisplayWrapList::IsOpaque(nsDisplayListBuilder* aBuilder) {
  // We could try to do something but let's conservatively just return PR_FALSE.
  // We reimplement ComputeVisibility and that's what really matters
  return PR_FALSE;
}

PRBool nsDisplayWrapList::IsUniform(nsDisplayListBuilder* aBuilder) {
  // We could try to do something but let's conservatively just return PR_FALSE.
  return PR_FALSE;
}

PRBool nsDisplayWrapList::IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder) {
  // The only existing consumer of IsVaryingRelativeToMovingFrame is
  // nsLayoutUtils::ComputeRepaintRegionForCopy, which refrains from calling
  // this on wrapped lists.
  NS_WARNING("nsDisplayWrapList::IsVaryingRelativeToMovingFrame called unexpectedly");
  // We could try to do something but let's conservatively just return PR_TRUE.
  return PR_TRUE;
}

void nsDisplayWrapList::Paint(nsDisplayListBuilder* aBuilder,
                              nsIRenderingContext* aCtx) {
  NS_ERROR("nsDisplayWrapList should have been flattened away for painting");
}

static nsresult
WrapDisplayList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  if (!aList->GetTop() && !aBuilder->HasMovingFrames())
    return NS_OK;
  nsDisplayItem* item = aWrapper->WrapList(aBuilder, aFrame, aList);
  if (!item)
    return NS_ERROR_OUT_OF_MEMORY;
  // aList was emptied
  aList->AppendToTop(item);
  return NS_OK;
}

static nsresult
WrapEachDisplayItem(nsDisplayListBuilder* aBuilder,
                    nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  nsDisplayList newList;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom())) {
    item = aWrapper->WrapItem(aBuilder, item);
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;
    newList.AppendToTop(item);
  }
  // aList was emptied
  aList->AppendToTop(&newList);
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapLists(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aIn, const nsDisplayListSet& aOut)
{
  nsresult rv = WrapListsInPlace(aBuilder, aFrame, aIn);
  NS_ENSURE_SUCCESS(rv, rv);

  if (&aOut == &aIn)
    return NS_OK;
  aOut.BorderBackground()->AppendToTop(aIn.BorderBackground());
  aOut.BlockBorderBackgrounds()->AppendToTop(aIn.BlockBorderBackgrounds());
  aOut.Floats()->AppendToTop(aIn.Floats());
  aOut.Content()->AppendToTop(aIn.Content());
  aOut.PositionedDescendants()->AppendToTop(aIn.PositionedDescendants());
  aOut.Outlines()->AppendToTop(aIn.Outlines());
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapListsInPlace(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aLists)
{
  nsresult rv;
  if (WrapBorderBackground()) {
    // Our border-backgrounds are in-flow
    rv = WrapDisplayList(aBuilder, aFrame, aLists.BorderBackground(), this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // Our block border-backgrounds are in-flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.BlockBorderBackgrounds(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The floats are not in flow
  rv = WrapEachDisplayItem(aBuilder, aLists.Floats(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // Our child content is in flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.Content(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The positioned descendants may not be in-flow
  rv = WrapEachDisplayItem(aBuilder, aLists.PositionedDescendants(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The outlines may not be in-flow
  return WrapEachDisplayItem(aBuilder, aLists.Outlines(), this);
}

nsDisplayOpacity::nsDisplayOpacity(nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplayOpacity);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayOpacity::~nsDisplayOpacity() {
  MOZ_COUNT_DTOR(nsDisplayOpacity);
}
#endif

PRBool nsDisplayOpacity::IsOpaque(nsDisplayListBuilder* aBuilder) {
  // We are never opaque, if our opacity was < 1 then we wouldn't have
  // been created.
  return PR_FALSE;
}

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer>
nsDisplayOpacity::BuildLayer(nsDisplayListBuilder* aBuilder,
                             LayerManager* aManager) {
  nsRefPtr<Layer> layer =
    mList.BuildLayer(aBuilder, aManager, &mChildLayers);
  if (!layer)
    return nsnull;

  layer->SetOpacity(mFrame->GetStyleDisplay()->mOpacity*layer->GetOpacity());
  return layer.forget();
}

void
nsDisplayOpacity::PaintThebesLayers(nsDisplayListBuilder* aBuilder) {
  mList.PaintThebesLayers(aBuilder, mChildLayers);
}

PRBool nsDisplayOpacity::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           nsRegion* aVisibleRegionBeforeMove) {
  NS_ASSERTION((aVisibleRegionBeforeMove != nsnull) == aBuilder->HasMovingFrames(),
               "Should have aVisibleRegionBeforeMove when there are moving frames");

  // Our children are translucent so we should not allow them to subtract
  // area from aVisibleRegion. We do need to find out what is visible under
  // our children in the temporary compositing buffer, because if our children
  // paint our entire bounds opaquely then we don't need an alpha channel in
  // the temporary compositing buffer.
  nsRect bounds = GetBounds(aBuilder);
  nsRegion visibleUnderChildren;
  visibleUnderChildren.And(*aVisibleRegion, bounds);
  nsRegion visibleUnderChildrenBeforeMove;
  if (aVisibleRegionBeforeMove) {
    visibleUnderChildrenBeforeMove.And(*aVisibleRegionBeforeMove, bounds);
  }
  return
    nsDisplayWrapList::ComputeVisibility(aBuilder, &visibleUnderChildren,
      aVisibleRegionBeforeMove ? &visibleUnderChildrenBeforeMove : nsnull);
}

PRBool nsDisplayOpacity::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_OPACITY)
    return PR_FALSE;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
    return PR_FALSE;
  mList.AppendToBottom(&static_cast<nsDisplayOpacity*>(aItem)->mList);
  return PR_TRUE;
}

nsDisplayClip::nsDisplayClip(nsIFrame* aFrame, nsIFrame* aClippingFrame,
        nsDisplayItem* aItem, const nsRect& aRect)
   : nsDisplayWrapList(aFrame, aItem),
     mClippingFrame(aClippingFrame), mClip(aRect) {
  MOZ_COUNT_CTOR(nsDisplayClip);
}

nsDisplayClip::nsDisplayClip(nsIFrame* aFrame, nsIFrame* aClippingFrame,
        nsDisplayList* aList, const nsRect& aRect)
   : nsDisplayWrapList(aFrame, aList),
     mClippingFrame(aClippingFrame), mClip(aRect) {
  MOZ_COUNT_CTOR(nsDisplayClip);
}

nsRect nsDisplayClip::GetBounds(nsDisplayListBuilder* aBuilder) {
  nsRect r = nsDisplayWrapList::GetBounds(aBuilder);
  r.IntersectRect(mClip, r);
  return r;
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayClip::~nsDisplayClip() {
  MOZ_COUNT_DTOR(nsDisplayClip);
}
#endif

void nsDisplayClip::Paint(nsDisplayListBuilder* aBuilder,
                          nsIRenderingContext* aCtx) {
  NS_ERROR("nsDisplayClip should have been flattened away for painting");
}

PRBool nsDisplayClip::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion,
                                        nsRegion* aVisibleRegionBeforeMove) {
  NS_ASSERTION((aVisibleRegionBeforeMove != nsnull) == aBuilder->HasMovingFrames(),
               "Should have aVisibleRegionBeforeMove when there are moving frames");

  PRBool isMoving = aBuilder->IsMovingFrame(mClippingFrame);

  if (aBuilder->HasMovingFrames() && !isMoving) {
    // There may be some clipped moving children that were visible before
    // but are clipped out now. Conservatively assume they were there
    // and add their possible area to the visible region of moving
    // content.
    // Compute the after-move region of moving content that could have been
    // totally clipped out.
    nsRegion r;
    r.Sub(mClip + aBuilder->GetMoveDelta(), mClip);
    // These hypothetical items are not visible after the move, so we pass
    // an empty region for the after-move visible region to make sure they
    // don't get added in the after-move position, only the before-move position.
    aBuilder->AccumulateVisibleRegionOfMovingContent(r, *aVisibleRegionBeforeMove,
                                                     nsRegion());
  }

  nsRegion clipped;
  clipped.And(*aVisibleRegion, mClip);
  nsRegion clippedBeforeMove;
  if (aVisibleRegionBeforeMove) {
    nsRect beforeMoveClip = isMoving ? mClip - aBuilder->GetMoveDelta() : mClip;
    clippedBeforeMove.And(*aVisibleRegionBeforeMove, beforeMoveClip);
  }

  nsRegion finalClipped(clipped);
  nsRegion finalClippedBeforeMove(clippedBeforeMove);
  PRBool anyVisible =
    nsDisplayWrapList::ComputeVisibility(aBuilder, &finalClipped,
      aVisibleRegionBeforeMove ? &finalClippedBeforeMove : nsnull);

  nsRegion removed;
  removed.Sub(clipped, finalClipped);
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);
  if (aVisibleRegionBeforeMove) {
    removed.Sub(clippedBeforeMove, finalClippedBeforeMove);
    aBuilder->SubtractFromVisibleRegion(aVisibleRegionBeforeMove, removed);
  }

  return anyVisible;
}

PRBool nsDisplayClip::TryMerge(nsDisplayListBuilder* aBuilder,
                               nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_CLIP)
    return PR_FALSE;
  nsDisplayClip* other = static_cast<nsDisplayClip*>(aItem);
  if (other->mClip != mClip || other->mClippingFrame != mClippingFrame)
    return PR_FALSE;
  mList.AppendToBottom(&other->mList);
  return PR_TRUE;
}

nsDisplayWrapList* nsDisplayClip::WrapWithClone(nsDisplayListBuilder* aBuilder,
                                                nsDisplayItem* aItem) {
  return new (aBuilder)
    nsDisplayClip(aItem->GetUnderlyingFrame(), mClippingFrame, aItem, mClip);
}



///////////////////////////////////////////////////
// nsDisplayTransform Implementation
//

// Write #define UNIFIED_CONTINUATIONS here to have the transform property try
// to transform content with continuations as one unified block instead of
// several smaller ones.  This is currently disabled because it doesn't work
// correctly, since when the frames are initially being reflowed, their
// continuations all compute their bounding rects independently of each other
// and consequently get the wrong value.  Write #define DEBUG_HIT here to have
// the nsDisplayTransform class dump out a bunch of information about hit
// detection.
#undef  UNIFIED_CONTINUATIONS
#undef  DEBUG_HIT

/* Returns the bounds of a frame as defined for transforms.  If
 * UNIFIED_CONTINUATIONS is not defined, this is simply the frame's bounding
 * rectangle, translated to the origin. Otherwise, returns the smallest
 * rectangle containing a frame and all of its continuations.  For example, if
 * there is a <span> element with several continuations split over several
 * lines, this function will return the rectangle containing all of those
 * continuations.  This rectangle is relative to the origin of the frame's local
 * coordinate space.
 */
#ifndef UNIFIED_CONTINUATIONS

nsRect
nsDisplayTransform::GetFrameBoundsForTransform(const nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "Can't get the bounds of a nonexistent frame!");
  return nsRect(nsPoint(0, 0), aFrame->GetSize());
}

#else

nsRect
nsDisplayTransform::GetFrameBoundsForTransform(const nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "Can't get the bounds of a nonexistent frame!");

  nsRect result;
  
  /* Iterate through the continuation list, unioning together all the
   * bounding rects.
   */
  for (const nsIFrame *currFrame = aFrame->GetFirstContinuation();
       currFrame != nsnull;
       currFrame = currFrame->GetNextContinuation())
    {
      /* Get the frame rect in local coordinates, then translate back to the
       * original coordinates.
       */
      result.UnionRect(result, nsRect(currFrame->GetOffsetTo(aFrame),
                                      currFrame->GetSize()));
    }

  return result;
}

#endif

/* Returns the delta specified by the -moz-tranform-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the transform origin.
 */
static
gfxPoint GetDeltaToMozTransformOrigin(const nsIFrame* aFrame,
                                      float aFactor,
                                      const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Can't get a delta for an untransformed frame!");

  /* For both of the coordinates, if the value of -moz-transform is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */
  const nsStyleDisplay* display = aFrame->GetStyleDisplay();
  nsRect boundingRect = (aBoundsOverride ? *aBoundsOverride :
                         nsDisplayTransform::GetFrameBoundsForTransform(aFrame));

  /* Allows us to access named variables by index. */
  gfxPoint result;
  gfxFloat* coords[2] = {&result.x, &result.y};
  const nscoord* dimensions[2] =
    {&boundingRect.width, &boundingRect.height};

  for (PRUint8 index = 0; index < 2; ++index) {
    /* If the -moz-transform-origin specifies a percentage, take the percentage
     * of the size of the box.
     */
    if (display->mTransformOrigin[index].GetUnit() == eStyleUnit_Percent)
      *coords[index] = NSAppUnitsToFloatPixels(*dimensions[index], aFactor) *
        display->mTransformOrigin[index].GetPercentValue();
    
    /* Otherwise, it's a length. */
    else
      *coords[index] =
        NSAppUnitsToFloatPixels(display->
                                mTransformOrigin[index].GetCoordValue(),
                                aFactor);
  }
  
  /* Adjust based on the origin of the rectangle. */
  result.x += NSAppUnitsToFloatPixels(boundingRect.x, aFactor);
  result.y += NSAppUnitsToFloatPixels(boundingRect.y, aFactor);

  return result;
}

/* Wraps up the -moz-transform matrix in a change-of-basis matrix pair that
 * translates from local coordinate space to transform coordinate space, then
 * hands it back.
 */
gfxMatrix
nsDisplayTransform::GetResultingTransformMatrix(const nsIFrame* aFrame,
                                                const nsPoint &aOrigin,
                                                float aFactor,
                                                const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Cannot get transform matrix for a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Cannot get transform matrix if frame isn't transformed!");

  /* Account for the -moz-transform-origin property by translating the
   * coordinate space to the new origin.
   */
  gfxPoint toMozOrigin = GetDeltaToMozTransformOrigin(aFrame, aFactor, aBoundsOverride);
  gfxPoint newOrigin = gfxPoint(NSAppUnitsToFloatPixels(aOrigin.x, aFactor),
                                NSAppUnitsToFloatPixels(aOrigin.y, aFactor));

  /* Get the underlying transform matrix.  This requires us to get the
   * bounds of the frame.
   */
  const nsStyleDisplay* disp = aFrame->GetStyleDisplay();
  nsRect bounds = (aBoundsOverride ? *aBoundsOverride :
                   nsDisplayTransform::GetFrameBoundsForTransform(aFrame));

  /* Get the matrix, then change its basis to factor in the origin. */
  return nsLayoutUtils::ChangeMatrixBasis
    (newOrigin + toMozOrigin, disp->mTransform.GetThebesMatrix(bounds, aFactor));
}

/* Painting applies the transform, paints the sublist, then unapplies
 * the transform.
 */
void nsDisplayTransform::Paint(nsDisplayListBuilder *aBuilder,
                               nsIRenderingContext *aCtx)
{
  /* Get the local transform matrix with which we'll transform all wrapped
   * elements.  If this matrix is singular, we shouldn't display anything
   * and can abort.
   */
  gfxMatrix newTransformMatrix =
    GetResultingTransformMatrix(mFrame, aBuilder->ToReferenceFrame(mFrame),
                                 mFrame->PresContext()->AppUnitsPerDevPixel(),
                                nsnull);
  if (newTransformMatrix.IsSingular())
    return;

  /* Get the context and automatically save and restore it. */
  gfxContext* gfx = aCtx->ThebesContext();
  gfxContextAutoSaveRestore autoRestorer(gfx);

  /* Get the new CTM by applying this transform after all of the
   * transforms preceding it.
   */
  newTransformMatrix.Multiply(gfx->CurrentMatrix());

  /* Set the matrix for the transform based on the old matrix and the new
   * transform data.
   */
  gfx->SetMatrix(newTransformMatrix);

  /* Now, send the paint call down.
   */    
  mStoredList.GetList()->Paint(aBuilder, aCtx, nsDisplayList::PAINT_DEFAULT);

  /* The AutoSaveRestore object will clean things up. */
}

PRBool nsDisplayTransform::ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                             nsRegion *aVisibleRegion,
                                             nsRegion *aVisibleRegionBeforeMove)
{
  NS_ASSERTION((aVisibleRegionBeforeMove != nsnull) == aBuilder->HasMovingFrames(),
               "Should have aVisibleRegionBeforeMove when there are moving frames");

  /* As we do this, we need to be sure to
   * untransform the visible rect, since we want everything that's painting to
   * think that it's painting in its original rectangular coordinate space. */
  nsRegion untransformedVisible =
    UntransformRect(mVisibleRect, mFrame, aBuilder->ToReferenceFrame(mFrame));

  nsRegion untransformedVisibleBeforeMove;
  if (aVisibleRegionBeforeMove) {
    // mVisibleRect contains areas visible before and after the move, so it's
    // OK (although conservative) to just use the same regions here.
    untransformedVisibleBeforeMove = untransformedVisible;
  }
  mStoredList.ComputeVisibility(aBuilder, &untransformedVisible,
                                aVisibleRegionBeforeMove
                                  ? &untransformedVisibleBeforeMove
                                  : nsnull);
  return PR_TRUE;
}

#ifdef DEBUG_HIT
#include <time.h>
#endif

/* HitTest does some fun stuff with matrix transforms to obtain the answer. */
nsIFrame *nsDisplayTransform::HitTest(nsDisplayListBuilder *aBuilder,
                                      nsPoint aPt,
                                      HitTestState *aState)
{
  /* Here's how this works:
   * 1. Get the matrix.  If it's singular, abort (clearly we didn't hit
   *    anything).
   * 2. Invert the matrix.
   * 3. Use it to transform the point into the correct space.
   * 4. Pass that point down through to the list's version of HitTest.
   */
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  gfxMatrix matrix =
    GetResultingTransformMatrix(mFrame, aBuilder->ToReferenceFrame(mFrame),
                                factor, nsnull);
  if (matrix.IsSingular())
    return nsnull;

  /* We want to go from transformed-space to regular space.
   * Thus we have to invert the matrix, which normally does
   * the reverse operation (e.g. regular->transformed)
   */
  matrix.Invert();

  /* Now, apply the transform and pass it down the channel. */
  gfxPoint result = matrix.Transform(gfxPoint(NSAppUnitsToFloatPixels(aPt.x, factor),
                                              NSAppUnitsToFloatPixels(aPt.y, factor)));

#ifdef DEBUG_HIT
  printf("Frame: %p\n", dynamic_cast<void *>(mFrame));
  printf("  Untransformed point: (%f, %f)\n", result.x, result.y);
#endif

  nsIFrame* resultFrame =
    mStoredList.HitTest(aBuilder,
                        nsPoint(NSFloatPixelsToAppUnits(float(result.x), factor),
                                NSFloatPixelsToAppUnits(float(result.y), factor)), aState);
  
#ifdef DEBUG_HIT
  if (resultFrame)
    printf("  Hit!  Time: %f, frame: %p\n", static_cast<double>(clock()),
           dynamic_cast<void *>(resultFrame));
  printf("=== end of hit test ===\n");
#endif

  return resultFrame;
}

/* The bounding rectangle for the object is the overflow rectangle translated
 * by the reference point.
 */
nsRect nsDisplayTransform::GetBounds(nsDisplayListBuilder *aBuilder)
{
  return mFrame->GetOverflowRect() + aBuilder->ToReferenceFrame(mFrame);
}

/* The transform is opaque iff the transform consists solely of scales and
 * transforms and if the underlying content is opaque.  Thus if the transform
 * is of the form
 *
 * |a c e|
 * |b d f|
 * |0 0 1|
 *
 * We need b and c to be zero.
 */
PRBool nsDisplayTransform::IsOpaque(nsDisplayListBuilder *aBuilder)
{
  const nsStyleDisplay* disp = mFrame->GetStyleDisplay();
  return disp->mTransform.GetMainMatrixEntry(1) == 0.0f &&
    disp->mTransform.GetMainMatrixEntry(2) == 0.0f &&
    mStoredList.IsOpaque(aBuilder);
}

/* The transform is uniform if it fills the entire bounding rect and the
 * wrapped list is uniform.  See IsOpaque for discussion of why this
 * works.
 */
PRBool nsDisplayTransform::IsUniform(nsDisplayListBuilder *aBuilder)
{
  const nsStyleDisplay* disp = mFrame->GetStyleDisplay();
  return disp->mTransform.GetMainMatrixEntry(1) == 0.0f &&
    disp->mTransform.GetMainMatrixEntry(2) == 0.0f &&
    mStoredList.IsUniform(aBuilder);
}

/* If UNIFIED_CONTINUATIONS is defined, we can merge two display lists that
 * share the same underlying content.  Otherwise, doing so results in graphical
 * glitches.
 */
#ifndef UNIFIED_CONTINUATIONS

PRBool
nsDisplayTransform::TryMerge(nsDisplayListBuilder *aBuilder,
                             nsDisplayItem *aItem)
{
  return PR_FALSE;
}

#else

PRBool
nsDisplayTransform::TryMerge(nsDisplayListBuilder *aBuilder,
                             nsDisplayItem *aItem)
{
  NS_PRECONDITION(aItem, "Why did you try merging with a null item?");
  NS_PRECONDITION(aBuilder, "Why did you try merging with a null builder?");

  /* Make sure that we're dealing with two transforms. */
  if (aItem->GetType() != TYPE_TRANSFORM)
    return PR_FALSE;

  /* Check to see that both frames are part of the same content. */
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
    return PR_FALSE;

  /* Now, move everything over to this frame and signal that
   * we merged things!
   */
  mStoredList.GetList()->
    AppendToBottom(&static_cast<nsDisplayTransform *>(aItem)->mStoredList);
  return PR_TRUE;
}

#endif

/* TransformRect takes in as parameters a rectangle (in app space) and returns
 * the smallest rectangle (in app space) containing the transformed image of
 * that rectangle.  That is, it takes the four corners of the rectangle,
 * transforms them according to the matrix associated with the specified frame,
 * then returns the smallest rectangle containing the four transformed points.
 *
 * @param aUntransformedBounds The rectangle (in app units) to transform.
 * @param aFrame The frame whose transformation should be applied.
 * @param aOrigin The delta from the frame origin to the coordinate space origin
 * @param aBoundsOverride (optional) Force the frame bounds to be the
 *        specified bounds.
 * @return The smallest rectangle containing the image of the transformed
 *         rectangle.
 */
nsRect nsDisplayTransform::TransformRect(const nsRect &aUntransformedBounds,
                                         const nsIFrame* aFrame,
                                         const nsPoint &aOrigin,
                                         const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Cannot transform a rectangle if there's no transformation!");

  float factor = nsPresContext::AppUnitsPerCSSPixel();
  return nsLayoutUtils::MatrixTransformRect
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, aOrigin, factor, aBoundsOverride),
     factor);
}

nsRect nsDisplayTransform::UntransformRect(const nsRect &aUntransformedBounds,
                                           const nsIFrame* aFrame,
                                           const nsPoint &aOrigin)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Cannot transform a rectangle if there's no transformation!");


  /* Grab the matrix.  If the transform is degenerate, just hand back the
   * empty rect.
   */
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  gfxMatrix matrix = GetResultingTransformMatrix(aFrame, aOrigin, factor, nsnull);
  if (matrix.IsSingular())
    return nsRect();

  /* We want to untransform the matrix, so invert the transformation first! */
  matrix.Invert();

  return nsLayoutUtils::MatrixTransformRect(aUntransformedBounds, matrix,
                                            factor);
}

#ifdef MOZ_SVG
nsDisplaySVGEffects::nsDisplaySVGEffects(nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aFrame, aList), mEffectsFrame(aFrame),
      mBounds(aFrame->GetOverflowRectRelativeToSelf())
{
  MOZ_COUNT_CTOR(nsDisplaySVGEffects);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplaySVGEffects::~nsDisplaySVGEffects()
{
  MOZ_COUNT_DTOR(nsDisplaySVGEffects);
}
#endif

PRBool nsDisplaySVGEffects::IsOpaque(nsDisplayListBuilder* aBuilder)
{
  return PR_FALSE;
}

nsIFrame*
nsDisplaySVGEffects::HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt,
                             HitTestState* aState)
{
  if (!nsSVGIntegrationUtils::HitTestFrameForEffects(mEffectsFrame,
          aPt - aBuilder->ToReferenceFrame(mEffectsFrame)))
    return nsnull;
  return mList.HitTest(aBuilder, aPt, aState);
}

void nsDisplaySVGEffects::Paint(nsDisplayListBuilder* aBuilder,
                                nsIRenderingContext* aCtx)
{
  nsSVGIntegrationUtils::PaintFramesWithEffects(aCtx,
          mEffectsFrame, mVisibleRect, aBuilder, &mList);
}

PRBool nsDisplaySVGEffects::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                              nsRegion* aVisibleRegion,
                                              nsRegion* aVisibleRegionBeforeMove) {
  NS_ASSERTION((aVisibleRegionBeforeMove != nsnull) == aBuilder->HasMovingFrames(),
               "Should have aVisibleRegionBeforeMove when there are moving frames");

  nsPoint offset = aBuilder->ToReferenceFrame(mEffectsFrame);
  nsRect dirtyRect =
    nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(mEffectsFrame,
                                                           mVisibleRect - offset) +
    offset;

  // Our children may be made translucent or arbitrarily deformed so we should
  // not allow them to subtract area from aVisibleRegion.
  nsRegion childrenVisible(dirtyRect);
  // mVisibleRect contains areas visible before and after the move, so it's
  // OK (although conservative) to just use the same regions here.
  nsRegion childrenVisibleBeforeMove(dirtyRect);
  nsDisplayWrapList::ComputeVisibility(aBuilder, &childrenVisible,
    aVisibleRegionBeforeMove ? &childrenVisibleBeforeMove : nsnull);
  return PR_TRUE;
}

PRBool nsDisplaySVGEffects::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem)
{
  if (aItem->GetType() != TYPE_SVG_EFFECTS)
    return PR_FALSE;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplaySVGEffects
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
    return PR_FALSE;
  nsDisplaySVGEffects* other = static_cast<nsDisplaySVGEffects*>(aItem);
  mList.AppendToBottom(&other->mList);
  mBounds.UnionRect(mBounds,
    other->mBounds + other->mEffectsFrame->GetOffsetTo(mEffectsFrame));
  return PR_TRUE;
}
#endif
