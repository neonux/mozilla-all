/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS display: -moz-flex */

#include "nsFlexContainerFrame.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "prlog.h"

using namespace mozilla::css;

#ifdef PR_LOGGING 
static PRLogModuleInfo *nsFlexContainerFrameLM = PR_NewLogModule("nsFlexContainerFrame");
#endif /* PR_LOGGING */

// Helper enums
// ============

// Represents a physical orientation for an axis.
// The directional suffix indicates the direction in which the axis *grows*.
// So e.g. eAxis_LR means a horizontal left-to-right axis, whereas eAxis_BT
// means a vertical bottom-to-top axis.
// NOTE: The order here is important -- these values are used as indices into
// the static array 'kAxisOrientationToSidesMap', defined below.
enum AxisOrientationType {
  eAxis_LR = 0, // Start at 0 so we can use these values as array indices.
  eAxis_RL,
  eAxis_TB,
  eAxis_BT,
  eNumAxisOrientationTypes // For sizing arrays that use these values as indices
};

// Represents one or the other extreme of an axis (e.g. for the main axis, the
// main-start vs. main-end edge.
// NOTE: The order here is important -- these values are used as indices into
// the sub-arrays in 'kAxisOrientationToSidesMap', defined below.
enum AxisEdgeType {
  eAxisEdge_Start = 0, // Start at 0 so we can use these values as array indices
  eAxisEdge_End,
  eNumAxisEdges
};

// This array maps each axis orientation to a pair of corresponding
// [start, end] physical mozilla::css::Side values.
static const Side
kAxisOrientationToSidesMap[eNumAxisOrientationTypes][eNumAxisEdges] = {
  { eSideLeft,   eSideRight  },  // eAxis_LR
  { eSideRight,  eSideLeft   },  // eAxis_RL
  { eSideTop,    eSideBottom },  // eAxis_TB
  { eSideBottom, eSideTop }      // eAxis_BT
};

// Helper structs / classes / methods
// ==================================

// Indicates whether advancing along the given axis is equivalent to
// increasing our X or Y position (as opposed to decreasing it).
static inline bool
DoesAxisGrowInPositiveDirection(AxisOrientationType aAxis) {
  return eAxis_LR == aAxis || eAxis_TB == aAxis;
}

// Indicates whether the given axis is horizontal.
static inline bool
IsAxisHorizontal(AxisOrientationType aAxis) {
  return eAxis_LR == aAxis || eAxis_RL == aAxis;
}

// Returns aFrame's computed value for 'height' or 'width' -- whichever is in
// the same dimension as aAxis.
static inline const nsStyleCoord&
GetSizePropertyForAxis(const nsIFrame* aFrame, AxisOrientationType aAxis)
{
  const nsStylePosition* stylePos = aFrame->GetStylePosition();

  return IsAxisHorizontal(aAxis) ?
    stylePos->mWidth :
    stylePos->mHeight;
}

static const nscoord&
GetMarginComponentForSideInternal(const nsMargin& aMargin, Side aSide)
{
  switch (aSide) {
    case eSideLeft:
      return aMargin.left;
    case eSideRight:
      return aMargin.right;
    case eSideTop:
      return aMargin.top;
    case eSideBottom:
      return aMargin.bottom;
  }

  NS_NOTREACHED("unexpected Side enum");
  return aMargin.left; // have to return something
                       // (but something's busted if we got here)
}

static nscoord&
GetMarginComponentForSideInternal(nsMargin& aMargin, Side aSide)
{
  switch (aSide) {
    case eSideLeft:
      return aMargin.left;
    case eSideRight:
      return aMargin.right;
    case eSideTop:
      return aMargin.top;
    case eSideBottom:
      return aMargin.bottom;
  }

  NS_NOTREACHED("unexpected Side enum");
  return aMargin.left; // have to return something
                       // (but something's busted if we got here)
}

// Encapsulates a frame for a flex item, with enough information for us to
// sort by flex-order (and by the frame's actual index inside the parent's
// child-frames array, among frames with the same flex-order).
class SortableFrame {
public:
  SortableFrame(nsIFrame* aFrame,
                PRInt32 aOrderValue,
                PRUint32 aIndexInFrameList)
  : mFrame(aFrame),
    mOrderValue(aOrderValue),
    mIndexInFrameList(aIndexInFrameList) {}

  // Implement operator== and operator< so that we can use nsDefaultComparator
  bool operator==(const SortableFrame& rhs) const {
    NS_ASSERTION(mFrame != rhs.mFrame ||
                 (mOrderValue == rhs.mOrderValue &&
                  mIndexInFrameList == rhs.mIndexInFrameList),
                 "if frames are equal, the other member data should be too");
    return mFrame == rhs.mFrame;
  }

  bool operator<(const SortableFrame& rhs) const {
    if (mOrderValue == rhs.mOrderValue) {
      return mIndexInFrameList < rhs.mIndexInFrameList;
    }
    return mOrderValue < rhs.mOrderValue;
  }

  // Accessor for the frame
  inline nsIFrame* GetFrame() const { return mFrame; }

protected:
  nsIFrame* const mFrame;     // The flex item's frame
  PRInt32   const mOrderValue; // mFrame's computed value of 'order' property
  PRUint32  const mIndexInFrameList; // mFrame's idx in nsFlexContainerFrame::mFrames
};

// Represents a flex item whose main size is being resolved.
// Includes the various pieces of input that the Flexbox Layout Algorithm uses
// to resolve a flexible width.
struct UnresolvedFlexItem {
  UnresolvedFlexItem(nsIFrame* aFrame,
                     float aFlexGrow, float aFlexShrink,
                     nscoord aFlexBaseSize,
                     nscoord aMinSize, nscoord aMaxSize)
  : mFrame(aFrame),
    mFlexGrow(aFlexGrow),
    mFlexShrink(aFlexShrink),
    mFlexBaseSize(aFlexBaseSize),
    mMinSize(aMinSize),
    mMaxSize(aMaxSize),
    mMainSize(aFlexBaseSize), // start out main-size at flex base size
    mIsFrozen(false),
    mHadMinViolation(false),
    mHadMaxViolation(false)
  {
    // This is enforced by the nsHTMLReflowState where these values come from:
    NS_ABORT_IF_FALSE(mMinSize <= mMaxSize,
                      "min size is larger than max size");

    // If we're inflexible, clamp our main size to [min,max] range up-front
    // and freeze it there. (Flexible items will get clamped later, after being
    // allowed to flex.)
    if (mFlexGrow == 0.0f && mFlexShrink == 0.0f) {
      mMainSize = NS_CSS_MINMAX(mMainSize, mMinSize, mMaxSize);
      Freeze();
    }
  }

  // Our frame:
  nsIFrame* const mFrame;

  // Inputs to "Resolving Flexible Lengths" algorithm
  const float mFlexGrow;
  const float mFlexShrink;
  const nscoord mFlexBaseSize;
  const nscoord mMinSize;
  const nscoord mMaxSize;

  // The value we're trying to compute: our final main size.
  // This isn't finalized until mIsFrozen gets set to true.
  nscoord mMainSize;

  // Temporary state:
  bool    mIsFrozen;
  bool    mHadMinViolation;
  bool    mHadMaxViolation;


  // Utility methods
  inline bool IsFrozen() { return mIsFrozen; }
  inline void Freeze() { mIsFrozen = true; }

  // Returns the flex ratio that we should use in the "resolving flexible
  // lengths" algorithm.  If we've got a positive amount of free space, we use
  // the flex-grow ratio; otherwise, we use the "scaled flex shrink ratio"
  // (scaled by our flex base size)
  float GetFlexRatioToUse(bool aHavePositiveFreeSpace)
  {
    if (IsFrozen()) {
      return 0.0f;
    }

    return aHavePositiveFreeSpace ?
      mFlexGrow :
      mFlexShrink * mFlexBaseSize;
  }
};

// Represents a flex item after its main size has been resolved.
// XXXdholbert It probably makes more sense to init nscoord member data to 0
// instead of nscoord_MIN, but for now nscoord_MIN is nice for debugging.
class ResolvedFlexItem {
public:
  ResolvedFlexItem(nsIFrame* aFrame, nscoord aMainSize)
  : mFrame(aFrame),
    mMainSize(aMainSize),
    mMainPosn(nscoord_MIN),
    mCrossSize(nscoord_MIN),
    mCrossPosn(nscoord_MIN),
    mCrossMinSize(nscoord_MIN),
    mCrossMaxSize(nscoord_MIN),
    mCrossBorderPaddingSize(nscoord_MIN),
    mAscent(nscoord_MIN),
    mAlignSelf(mFrame->GetStylePosition()->mAlignSelf),
    mIsStretched(false)
  {
    // Do "flex-item-align: auto" fixup:
    if (mAlignSelf == NS_STYLE_ALIGN_SELF_AUTO) {
      mAlignSelf = mFrame->GetParent()->GetStylePosition()->mAlignItems;
    }
  }

  // Setters (for values that we compute after this object has been created)
  inline void SetMainPosition(nscoord aPosn)     { mMainPosn  = aPosn; }
  inline void SetCrossSize(nscoord aCrossSize)   { mCrossSize = aCrossSize; }
  inline void SetCrossPosition(nscoord aPosn)    { mCrossPosn = aPosn; }
  inline void SetCrossMinSize(nscoord aCrossMinSize)
  { mCrossMinSize = aCrossMinSize; }
  inline void SetCrossMaxSize(nscoord aCrossMaxSize)
  { mCrossMaxSize = aCrossMaxSize; }
  inline void SetCrossBorderPaddingSize(const nscoord aCrossBorderPaddingSize)
  { mCrossBorderPaddingSize = aCrossBorderPaddingSize; }
  inline void SetAscent(nscoord aAscent)         { mAscent = aAscent; }
  inline void SetMargin(const nsMargin& aMargin) { mMargin = aMargin; }
  inline void SetIsStretched()                   { mIsStretched = true; }

  // Getters
  inline nsIFrame* GetFrame() const { return mFrame; }

  inline nscoord GetMainSize() const {
    NS_ABORT_IF_FALSE(mMainSize != nscoord_MIN,
                      "returning uninitialized main size");
    return mMainSize;
  }
  inline nscoord GetMainPosition() const {
    NS_ABORT_IF_FALSE(mMainPosn != nscoord_MIN,
                      "returning uninitialized main axis position");
    return mMainPosn;
  }
  inline nscoord GetCrossSize() const {
    NS_ABORT_IF_FALSE(mCrossSize != nscoord_MIN,
                      "returning uninitialized cross size");
    return mCrossSize;
  }
  // Convenience method to return the border-box cross-size of our frame.
  inline nscoord GetBorderBoxCrossSize() const {
    return GetCrossSize() + GetCrossBorderPaddingSize();
  }
  inline nscoord GetCrossPosition() const {
    NS_ABORT_IF_FALSE(mCrossPosn != nscoord_MIN,
                      "returning uninitialized cross axis position");
    return mCrossPosn;
  }
  inline nscoord GetCrossMinSize() const {
    NS_ABORT_IF_FALSE(mCrossMinSize != nscoord_MIN,
                      "returning uninitialized cross axis min size");
    return mCrossMinSize;
  }
  inline nscoord GetCrossMaxSize() const {
    NS_ABORT_IF_FALSE(mCrossMaxSize != nscoord_MIN,
                      "returning uninitialized cross axis max size");
    return mCrossMaxSize;
  }
  inline nscoord GetCrossBorderPaddingSize() const {
    NS_ABORT_IF_FALSE(mCrossBorderPaddingSize != nscoord_MIN,
                      "returning uninitialized cross border/padding size");
    return mCrossBorderPaddingSize;
  }

  inline nscoord GetAscent() const {
    NS_ABORT_IF_FALSE(mAscent != nscoord_MIN,
                      "returning uninitialized ascent");
    return mAscent;
  }

  // Returns a const reference to our internal margin
  inline const nsMargin& GetMargin() const { return mMargin; }

  // Returns a reference to our margin-component for the given side.
  nscoord& GetMarginComponentForSide(Side aSide)
  { return GetMarginComponentForSideInternal(mMargin, aSide); }

  const nscoord& GetMarginComponentForSide(Side aSide) const
  { return GetMarginComponentForSideInternal(mMargin, aSide); }

  inline nscoord GetMarginSizeInAxis(AxisOrientationType aAxis) const
  {
    Side startSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_Start];
    Side endSide = kAxisOrientationToSidesMap[aAxis][eAxisEdge_End];
    return GetMarginComponentForSide(startSide) +
      GetMarginComponentForSide(endSide);
  }

  inline PRUint8 GetAlignSelf() const { return mAlignSelf; }

  // Indicates whether this item has "flex-item-align: stretch" with an auto
  // cross-size. (If so, we'll need to override the computed cross-size during
  // our final reflow.)
  inline bool IsStretched() const { return mIsStretched; }

  PRUint32 GetNumAutoMarginsInAxis(AxisOrientationType aAxis) const;

private:
  // Member data that's known at constructor-time (& hence is const):
  // ----------
  nsIFrame* const mFrame;
  const nscoord mMainSize;

  // Member data that's determined after we've been constructed:
  // ----------
  nscoord mMainPosn;
  nscoord mCrossSize;
  nscoord mCrossPosn;
  nscoord mCrossMinSize;
  nscoord mCrossMaxSize;
  nscoord mCrossBorderPaddingSize;
  nscoord mAscent;

  nsMargin mMargin;

  PRUint8 mAlignSelf; // My 'align-self' computed value (with "auto" already
                      // swapped out w/ parent's 'align-items' value)

  bool    mIsStretched; // See IsStretched() documentation
};

PRUint32
ResolvedFlexItem::GetNumAutoMarginsInAxis(AxisOrientationType aAxis) const
{
  PRUint32 numAutoMargins = 0;
  for (PRUint32 i = 0; i < eNumAxisEdges; i++) {
    const nsStyleSides& styleMargin = mFrame->GetStyleMargin()->mMargin;
    Side side = kAxisOrientationToSidesMap[aAxis][i];
    if (styleMargin.GetUnit(side) == eStyleUnit_Auto) {
      numAutoMargins++;
    }
  }

  // Mostly for clarity:
  NS_ABORT_IF_FALSE(numAutoMargins <= 2,
                    "We're just looking at one item along one dimension, so we "
                    "should only have examined 2 margins");

  return numAutoMargins;
}

// Encapsulates our flex container's main & cross axes.
NS_STACK_CLASS class FlexboxAxisTracker {
public:
  FlexboxAxisTracker(nsFlexContainerFrame* aFlexContainerFrame);

  // Accessors:
  AxisOrientationType GetMainAxis() const  { return mMainAxis;  }
  AxisOrientationType GetCrossAxis() const { return mCrossAxis; }

  inline void AssertAxesSane() const {
    NS_ABORT_IF_FALSE(IsAxisHorizontal(mMainAxis) !=
                      IsAxisHorizontal(mCrossAxis),
                      "main & cross axes should be in different dimensions");
  }

private:
  AxisOrientationType mMainAxis;
  AxisOrientationType mCrossAxis;
};

// Keeps track of our position along a particular axis (where a '0' position
// corresponds to the 'start' edge of that axis).
// This class shouldn't be instantiated directly -- rather, it should only be
// instantiated via its subclasses defined below.
NS_STACK_CLASS
class PositionTracker {
public:
  // Accessor for the current value of the position that we're tracking.
  inline nscoord GetPosition() const { return mPosition; }

  // Advances our position across the start edge of the given margin, in the
  // axis we're tracking.
  void EnterMargin(const nsMargin& aMargin)
  {
    Side side =
      kAxisOrientationToSidesMap[mAxis][eAxisEdge_Start];
    mPosition += GetMarginComponentForSideInternal(aMargin, side);
  }

  // Advances our position across the end edge of the given margin, in the axis
  // we're tracking.
  void ExitMargin(const nsMargin& aMargin)
  {
    Side side =
      kAxisOrientationToSidesMap[mAxis][eAxisEdge_End];
    mPosition += GetMarginComponentForSideInternal(aMargin, side);
  }

  // Advances our current position from the start side of a child frame's
  // border-box to the frame's upper or left edge (depending on our axis).
  // (Note that this is a no-op if our axis grows in positive direction.)
  void EnterChildFrame(nscoord aChildFrameSize)
  {
    if (!DoesAxisGrowInPositiveDirection(mAxis))
      mPosition += aChildFrameSize;
  }

  // Advances our current position from a frame's upper or left border-box edge
  // (whichever is in the axis we're tracking) to the 'end' side of the frame
  // in the axis that we're tracking. (Note that this is a no-op if our axis
  // grows in the negative direction.)
  void ExitChildFrame(nscoord aChildFrameSize)
  {
    if (DoesAxisGrowInPositiveDirection(mAxis))
      mPosition += aChildFrameSize;
  }

protected:
  // Protected constructor, to be sure we're only instantiated via a subclass.
  PositionTracker(AxisOrientationType aAxis)
    : mPosition(0),
      mAxis(aAxis)
  {}
  
  // Member data:
  nscoord mPosition;               // The position we're tracking
  const AxisOrientationType mAxis; // The axis along which we're moving
};

// Tracks our position in the main axis, when we're laying out flex items.
NS_STACK_CLASS
class MainAxisPositionTracker : public PositionTracker {
public:
  MainAxisPositionTracker(nsFlexContainerFrame* aFlexContainerFrame,
                          const FlexboxAxisTracker& aAxisTracker,
                          const nsHTMLReflowState& aReflowState,
                          const nsTArray<ResolvedFlexItem>& aItems);

  ~MainAxisPositionTracker() {
    NS_ABORT_IF_FALSE(mNumPackingSpacesRemaining == 0,
                      "miscounted the number of packing spaces");
    NS_ABORT_IF_FALSE(mNumAutoMarginsInMainAxis == 0,
                      "miscounted the number of auto margins");
  }

  // Advances past the packing space (if any) between two flex items
  void TraversePackingSpace();

  // If aItem has any 'auto' margins in the main axis, this method updates the
  // corresponding values in its margin.
  void ResolveAutoMarginsInMainAxis(ResolvedFlexItem& aItem);

private:
  nscoord  mPackingSpaceRemaining;
  PRUint32 mNumAutoMarginsInMainAxis;
  PRUint32 mNumPackingSpacesRemaining;
  PRUint8  mJustifyContent;
};

// Utility class for managing our position along the cross axis along
// the whole flex container (at a higher level than a single line)
class SingleLineCrossAxisPositionTracker;
NS_STACK_CLASS
class CrossAxisPositionTracker : public PositionTracker {
public:
  CrossAxisPositionTracker(nsFlexContainerFrame* aFlexContainerFrame,
                           const FlexboxAxisTracker& aAxisTracker,
                           const nsHTMLReflowState& aReflowState);

  // XXXdholbert This probably needs a ResolveStretchedLines() method,
  // (which takes an array of SingleLineCrossAxisPositionTracker objects
  // and distributes an equal amount of space to each one).
  // For now, we just have Reflow directly call
  // SingleLineCrossAxisPositionTracker::SetLineCrossSize().
};

// Utility class for managing our position along the cross axis, *within* a
// single flex line.
NS_STACK_CLASS
class SingleLineCrossAxisPositionTracker : public PositionTracker {
public:
  SingleLineCrossAxisPositionTracker(nsFlexContainerFrame* aFlexContainerFrame,
                                     const FlexboxAxisTracker& aAxisTracker,
                                     const nsTArray<ResolvedFlexItem>& aItems);

  void ComputeLineCrossSize(const nsTArray<ResolvedFlexItem>& aItems);
  inline nscoord GetLineCrossSize() const { return mLineCrossSize; }

  // Used to override the flex line's size, for cases when the flex container is
  // single-line and has a fixed size, and also in cases where
  // "align-self: stretch" triggers some space-distribution between lines
  // (when we support that property).
  inline void SetLineCrossSize(nscoord aNewLineCrossSize) {
    mLineCrossSize = aNewLineCrossSize;
  }

  void ResolveStretchedCrossSize(ResolvedFlexItem& aItem);
  void ResolveAutoMarginsInCrossAxis(ResolvedFlexItem& aItem);

  void EnterAlignPackingSpace(const ResolvedFlexItem& aItem);

  // Resets our position to the cross-start edge of this line.
  inline void ResetPosition() { mPosition = 0; }

private:
  // Returns the distance from the cross-start side of the given flex item's
  // margin-box to its baseline. (Used in baseline alignment.)
  nscoord GetBaselineOffsetFromCrossStart(const ResolvedFlexItem& aItem) const;
  nscoord GetBaselineOffsetFromCrossEnd(const ResolvedFlexItem& aItem) const;

  nscoord mLineCrossSize;

  // Largest offset from an item's cross-start margin-box edge to its
  // baseline -- computed in ComputeLineCrossSize:
  nscoord mCrossStartToFurthestBaseline;
};

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsFlexContainerFrame)
  NS_QUERYFRAME_ENTRY(nsFlexContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFlexContainerFrameSuper)

NS_IMPL_FRAMEARENA_HELPERS(nsFlexContainerFrame)

nsIFrame*
NS_NewFlexContainerFrame(nsIPresShell* aPresShell,
                         nsStyleContext* aContext)
{
  return new (aPresShell) nsFlexContainerFrame(aContext);
}

//----------------------------------------------------------------------

// nsFlexContainerFrame Method Implementations
// ===========================================

/* virtual */
nsFlexContainerFrame::~nsFlexContainerFrame()
{
}

/* virtual */
void
nsFlexContainerFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  DestroyAbsoluteFrames(aDestructRoot);
  nsFlexContainerFrameSuper::DestroyFrom(aDestructRoot);
}

/* virtual */
nsIAtom*
nsFlexContainerFrame::GetType() const
{
  return nsGkAtoms::flexContainerFrame;
}

#ifdef DEBUG
NS_IMETHODIMP
nsFlexContainerFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("FlexContainer"), aResult);
}
#endif // DEBUG

NS_IMETHODIMP
nsFlexContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                 const nsRect&           aDirtyRect,
                                 const nsDisplayListSet& aLists)
{
  // XXXdholbert Cribbed from nsColumnSetFrame::BuildDisplayList()
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsresult rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

#ifdef DEBUG
// helper for the debugging method below
bool
FrameWantsToBeInAnonymousFlexItem(nsIFrame* aFrame)
{
  // Note: This needs to match the logic in
  // nsCSSFrameConstructor::FrameConstructionItem::NeedsAnonFlexItem()
  return (aFrame->IsFrameOfType(nsIFrame::eLineParticipant) ||
          nsGkAtoms::placeholderFrame == aFrame->GetType());
}

// Debugging method, to let us assert that our anonymous flex items are
// set up correctly -- in particular, we assert:
//  (1) we don't have any inline non-replaced children
//  (2) we don't have any consecutive anonymous flex items
//  (3) we don't have any empty anonymous flex items
void
nsFlexContainerFrame::SanityCheckAnonymousFlexItems() const
{
  bool prevChildWasAnonFlexItem = false;
  for (nsIFrame* child = mFrames.FirstChild(); child;
       child = child->GetNextSibling()) {
    NS_ABORT_IF_FALSE(!FrameWantsToBeInAnonymousFlexItem(child),
                      "frame wants to be inside an anonymous flex item, "
                      "but it isn't");
    if (child->GetStyleContext()->GetPseudo() ==
        nsCSSAnonBoxes::anonymousFlexItem) {
      NS_ABORT_IF_FALSE(!prevChildWasAnonFlexItem,
                        "two anon flex items in a row (shouldn't happen)");

      nsIFrame* firstWrappedChild = child->GetFirstPrincipalChild();
      NS_ABORT_IF_FALSE(firstWrappedChild,
                        "anonymous flex item is empty (shouldn't happen)");
      prevChildWasAnonFlexItem = true;
    } else {
      prevChildWasAnonFlexItem = false;
    }
  }
}
#endif // DEBUG

// Returns the amount of margin/border/padding that we need reserved for this
// child (so we can subtract that from the total space before we run the
// distribution algorithm).
UnresolvedFlexItem
nsFlexContainerFrame::CreateUnresolvedFlexItemForChild(
  nsPresContext* aPresContext,
  nsIFrame*      aChildFrame,
  const nsHTMLReflowState& aParentReflowState,
  nscoord& aReservedMBP)
{
  // Create temporary reflow state just for sizing -- to get hypothetical
  // main-size and the computed values of min / max main-size property.
  nsHTMLReflowState pretendRS(aPresContext, aParentReflowState, aChildFrame,
                              nsSize(aParentReflowState.ComputedWidth(),
                                     aParentReflowState.ComputedHeight()));

  const nsStylePosition* stylePos =
    aChildFrame->GetStyleContext()->GetStylePosition();

  NS_ABORT_IF_FALSE(stylePos->mFlexGrow >= 0.0f,
                    "flex values should be nonnegative");
  NS_ABORT_IF_FALSE(stylePos->mFlexShrink >= 0.0f,
                    "flex values should be nonnegative");

  // XXXdholbert Assuming horizontal/width axis
  // Return sum of MBP in main axis
  aReservedMBP =
    pretendRS.mComputedBorderPadding.left +
    pretendRS.mComputedBorderPadding.right +
    pretendRS.mComputedMargin.left +
    pretendRS.mComputedMargin.right;

  
  return UnresolvedFlexItem(aChildFrame,
                            stylePos->mFlexGrow,
                            stylePos->mFlexShrink,
                            pretendRS.ComputedWidth(),
                            pretendRS.mComputedMinWidth,
                            pretendRS.mComputedMaxWidth);
}

// Based on the sign of aTotalViolation, this function freezes a subset of our
// flexible sizes, and restores the remaining ones to their initial pref sizes.
/* static */
void
nsFlexContainerFrame::FreezeOrRestoreEachFlexibleSize(
  const nscoord aTotalViolation,
  nsTArray<UnresolvedFlexItem>& aItems)
{
  enum FreezeType {
    eFreezeEverything,
    eFreezeMinViolations,
    eFreezeMaxViolations
  };

  FreezeType freezeType;
  if (aTotalViolation == 0) {
    freezeType = eFreezeEverything;
  } else if (aTotalViolation > 0) {
    freezeType = eFreezeMinViolations;
  } else { // aTotalViolation < 0
    freezeType = eFreezeMaxViolations;
  }

  for (PRUint32 i = 0; i < aItems.Length(); i++) {
    UnresolvedFlexItem& item = aItems[i];
    if (!item.IsFrozen()) {
      if (eFreezeEverything == freezeType ||
          (eFreezeMinViolations == freezeType && item.mHadMinViolation) ||
          (eFreezeMaxViolations == freezeType && item.mHadMaxViolation)) {

        NS_ASSERTION(item.mMainSize >= item.mMinSize,
                     "Freezing item at a size below its minimum");
        NS_ASSERTION(item.mMainSize <= item.mMaxSize,
                     "Freezing item at a size above its maximum");

        item.Freeze();
      } else {
        // For items that we're not freezing, we reset their main sizes to
        // their (fixed) hypothetical sizes.
        item.mMainSize = item.mFlexBaseSize;
      }
    }
  }
}

// Implementation of flexbox spec's "Determine sign of flexibility" step.
// NOTE: aTotalFreeSpace should already have the flex items' margin, border,
// & padding values subtracted out.
static bool
ShouldUseFlexGrow(nscoord aTotalFreeSpace,
                  nsTArray<UnresolvedFlexItem>& aTuples)
{
  for (PRUint32 i = 0; i < aTuples.Length(); i++) {
    nscoord adjustedSize = NS_CSS_MINMAX(aTuples[i].mMainSize,
                                         aTuples[i].mMinSize,
                                         aTuples[i].mMaxSize);
    aTotalFreeSpace -= adjustedSize;

    if (aTotalFreeSpace <= 0) {
      return false;
    }
  }
  NS_ABORT_IF_FALSE(aTotalFreeSpace > 0,
                    "if we used up all the space, should've already returned");
  return true;
}

// Implementation of flexbox spec's "resolve the flexible lengths" algorithm.
// NOTE: aTotalFreeSpace should already have the flex items' margin, border,
// & padding values subtracted out, so that all we need to do is distribute the
// remaining free space among content-box sizes.  (The spec deals with
// margin-box sizes, but we can have fewer values in play & a simpler algorithm
// if we subtract margin/border/padding up front.)
void
nsFlexContainerFrame::ResolveFlexibleLengths(
  nscoord aTotalFreeSpace,
  nsTArray<UnresolvedFlexItem>& aItems)
{
  PR_LOG(nsFlexContainerFrameLM, PR_LOG_DEBUG, ("ResolveFlexibleLengths\n"));
  if (aItems.IsEmpty()) {
    return;
  }

  // First: Determine the sign of flexibility.
  bool havePositiveFreeSpace = ShouldUseFlexGrow(aTotalFreeSpace, aItems);

  while (true) {
    // Second: Calculate Available Free Space
    nscoord availableFreeSpace = aTotalFreeSpace;
    for (PRUint32 i = 0; i < aItems.Length(); i++) {
      NS_ABORT_IF_FALSE(aItems[i].IsFrozen() ||
                        aItems[i].mMainSize == aItems[i].mFlexBaseSize,
                        "Any unfrozen items should've been initialized/reset "
                        "to their hypothetical main size at this point");
      availableFreeSpace -= aItems[i].mMainSize;
    }

    PR_LOG(nsFlexContainerFrameLM, PR_LOG_DEBUG,
           (" available free space = %d\n", availableFreeSpace));

    // If sign of free space matches flexType, give each flexible
    // item a portion of availableFreeSpace.
    if ((availableFreeSpace > 0 && havePositiveFreeSpace) ||
        (availableFreeSpace < 0 && !havePositiveFreeSpace)) {

      float sumOfFlexRatios = 0.0f;

      // To work around floating-point error, we keep track of the last item
      // with nonzero flexibility, and when we hit that entry in our main
      // distribution loop, we'll just give it all the remaining flexiblity.
      PRUint32 idxOfLastNonzeroFlex = 0;
      for (PRUint32 i = 0; i < aItems.Length(); i++) {
        float curFlexRatio = aItems[i].GetFlexRatioToUse(havePositiveFreeSpace);
        if (curFlexRatio > 0.0f) {
          idxOfLastNonzeroFlex = i;
          sumOfFlexRatios += curFlexRatio;
        }
      }

      if (sumOfFlexRatios != 0.0f) { // (no distribution if nothing is flexible)
        // Distribute!
        // (chipping away at sumOfFlexRatios & availableSpace as we go)

        PR_LOG(nsFlexContainerFrameLM, PR_LOG_DEBUG,
               (" Distributing available space:"));
        for (PRUint32 i = 0; i < aItems.Length(); i++) {
          UnresolvedFlexItem& item = aItems[i];
          float curFlex = item.GetFlexRatioToUse(havePositiveFreeSpace);

          // If this is the last flexible element, then assign it all the
          // remaining flexibility, so that no flexibility is left over.
          // (to account for floating-point rounding error.)
          if (i == idxOfLastNonzeroFlex) {
            NS_WARN_IF_FALSE(fabs(sumOfFlexRatios - curFlex) < 0.0001,
                             "Especially large amount of floating-point error "
                             "in computation of total flexibility");
            curFlex = sumOfFlexRatios;
          }

          NS_ASSERTION(sumOfFlexRatios >= 0.0f, "miscalculated total flex");
          NS_ASSERTION(sumOfFlexRatios != 0.0f || curFlex == 0.0f,
                       "miscalculated total flex");

          if (curFlex > 0.0f) {
            nscoord sizeDelta = NSToCoordRound(availableFreeSpace * curFlex /
                                               sumOfFlexRatios);

            // To avoid rounding issues, subtract our flex & our sizeDelta from
            // the totals (so that at the end, the remaining free space gets
            // entirely assigned to the final entry, with no rounding issues)
            availableFreeSpace  -= sizeDelta;
            sumOfFlexRatios -= curFlex;

            item.mMainSize += sizeDelta;
            PR_LOG(nsFlexContainerFrameLM, PR_LOG_DEBUG,
                   ("  child %d receives %d, for a total of %d\n",
                    i, sizeDelta, item.mMainSize));

          }
        }
      }
      NS_ABORT_IF_FALSE(sumOfFlexRatios == 0.0f,
                        "Shouldn't have any flexibility left over");
    }

    // Fix min/max violations:
    nscoord totalViolation = 0; // keeps track of adjustments for min/max
    PR_LOG(nsFlexContainerFrameLM, PR_LOG_DEBUG,
           (" Checking for violations:"));

    for (PRUint32 i = 0; i < aItems.Length(); i++) {
      UnresolvedFlexItem& item = aItems[i];
      item.mHadMinViolation = false; // XXXdholbert do "ClearViolations" at the end instead of at the beginning
      item.mHadMaxViolation = false;

      if (item.mMainSize < item.mMinSize) {
        // min violation
        totalViolation += item.mMinSize - item.mMainSize;
        item.mMainSize = item.mMinSize;
        item.mHadMinViolation = true;
      } else if (item.mMainSize > item.mMaxSize) {
        // max violation
        totalViolation += item.mMaxSize - item.mMainSize;
        item.mMainSize = item.mMaxSize;
        item.mHadMaxViolation = true;
      }
    }

    FreezeOrRestoreEachFlexibleSize(totalViolation, aItems);

    PR_LOG(nsFlexContainerFrameLM, PR_LOG_DEBUG,
           (" Total violation: %d\n", totalViolation));

    if (totalViolation == 0) {
      break;
    }
  }

  // Post-condition: all lengths should've been frozen.
#ifdef DEBUG
  for (PRUint32 i = 0; i < aItems.Length(); ++i) {
    NS_ABORT_IF_FALSE(aItems[i].IsFrozen(),
                      "All flexible lengths should've been resolved");
  }
#endif // DEBUG
}

const nsTArray<SortableFrame>
BuildSortedChildArray(const nsFrameList& aChildren)
{
  // NOTE: To benefit from Return Value Optimization, we must only return
  // this value:
  nsTArray<SortableFrame> sortedChildArray(aChildren.GetLength());

  // Throw all our children in the array...
  PRUint32 indexInFrameList = 0;
  for (nsIFrame* child = aChildren.FirstChild(); child;
       child = child->GetNextSibling()) {
    PRInt32 orderValue = child->GetStylePosition()->mOrder;
    sortedChildArray.AppendElement(SortableFrame(child, orderValue,
                                                 indexInFrameList));
    indexInFrameList++;
  }

  // ... and sort by flex-order.
  sortedChildArray.Sort();

  return sortedChildArray;
}

MainAxisPositionTracker::
  MainAxisPositionTracker(nsFlexContainerFrame* aFlexContainerFrame,
                          const FlexboxAxisTracker& aAxisTracker,
                          const nsHTMLReflowState& aReflowState,
                          const nsTArray<ResolvedFlexItem>& aItems)
  : PositionTracker(aAxisTracker.GetMainAxis()),
    mNumAutoMarginsInMainAxis(0),
    mNumPackingSpacesRemaining(0)
{
  // Step over flex container's own main-start border/padding.
  EnterMargin(aReflowState.mComputedBorderPadding);

  // Set up our state for managing packing space & auto margins
  mPackingSpaceRemaining = aReflowState.ComputedWidth();
  for (PRUint32 i = 0; i < aItems.Length(); i++) {
    mPackingSpaceRemaining -= aItems[i].GetMainSize();
  }

  if (mPackingSpaceRemaining > 0) {
    for (PRUint32 i = 0; i < aItems.Length(); i++) {
      mNumAutoMarginsInMainAxis += aItems[i].GetNumAutoMarginsInAxis(mAxis);
    }
  }

  mJustifyContent = aFlexContainerFrame->GetStylePosition()->mJustifyContent;
  // If packing space is negative, 'justify' behaves like 'start', and
  // 'distribute' behaves like 'center'.  In those cases, it's simplest to
  // just pretend we have a different 'justify-content' value and share code.
  if (mPackingSpaceRemaining < 0) {
    if (mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_SPACE_BETWEEN) {
      mJustifyContent = NS_STYLE_JUSTIFY_CONTENT_FLEX_START;
    } else if (mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_SPACE_AROUND) {
      mJustifyContent = NS_STYLE_JUSTIFY_CONTENT_CENTER;
    }
  }

  // Figure out how much space we'll set aside for auto margins or
  // packing spaces, and advance past any leading packing-space.
  if (mNumAutoMarginsInMainAxis == 0 && mPackingSpaceRemaining) {
    switch (mJustifyContent) {
      case NS_STYLE_JUSTIFY_CONTENT_FLEX_START:
        // All packing space should go at the end --> nothing to do here.
        break;
      case NS_STYLE_JUSTIFY_CONTENT_FLEX_END:
        // All packing space goes at the beginning
        mPosition += mPackingSpaceRemaining;
        break;
      case NS_STYLE_JUSTIFY_CONTENT_CENTER:
        // Half the packing space goes at the beginning
        mPosition += mPackingSpaceRemaining / 2;
        break;
      case NS_STYLE_JUSTIFY_CONTENT_SPACE_BETWEEN:
        NS_ABORT_IF_FALSE(mPackingSpaceRemaining >= 0,
                          "negative packing space should make us use 'start' "
                          "instead of 'justify'");
        // 1 packing space between each flex item, no packing space at ends.
        mNumPackingSpacesRemaining = aItems.Length() - 1;
        break;
      case NS_STYLE_JUSTIFY_CONTENT_SPACE_AROUND:
        NS_ABORT_IF_FALSE(mPackingSpaceRemaining >= 0,
                          "negative packing space should make us use 'center' "
                          "instead of 'distribute'");
        // 1 packing space between each flex item, plus half a packing space
        // at beginning & end.  So our number of full packing-spaces is equal
        // to the number of flex items.
        mNumPackingSpacesRemaining = aItems.Length();
        if (mNumPackingSpacesRemaining > 0) {
          // The edges (start/end) share one full packing space
          nscoord totalEdgePackingSpace =
            NSToCoordRound(float(mPackingSpaceRemaining) /
                           mNumPackingSpacesRemaining);

          // ...and we'll use half of that right now, at the start
          mPosition += totalEdgePackingSpace / 2;
          // ...but we need to subtract all of it right away, so that we won't
          // hand out any of it to intermediate packing spaces.
          mPackingSpaceRemaining -= totalEdgePackingSpace;
          mNumPackingSpacesRemaining--;
        }
        break;
      default:
        NS_ABORT_IF_FALSE(false, "Unexpected justify-content value");
    }
  }

  NS_ABORT_IF_FALSE(mNumPackingSpacesRemaining == 0 ||
                    mNumAutoMarginsInMainAxis == 0,
                    "extra space should either go to packing space or to "
                    "auto margins, but not to both");
}

void
MainAxisPositionTracker::ResolveAutoMarginsInMainAxis(ResolvedFlexItem& aItem)
{
  if (mNumAutoMarginsInMainAxis) {
    for (PRUint32 i = 0; i < eNumAxisEdges; i++) {
      const nsStyleSides& styleMargin =
        aItem.GetFrame()->GetStyleMargin()->mMargin;
      Side side = kAxisOrientationToSidesMap[mAxis][i];
      if (styleMargin.GetUnit(side) == eStyleUnit_Auto) {
        // If this is the last auto margin, then there's no need to do any
        // float math -- just use all the remaining packing space.
        nscoord curAutoMarginSize = (mNumAutoMarginsInMainAxis == 1) ?
          mPackingSpaceRemaining :
          NSToCoordRound(float(mPackingSpaceRemaining) /
                         mNumAutoMarginsInMainAxis);

        nscoord& curAutoMarginComponent = aItem.GetMarginComponentForSide(side);
        NS_ABORT_IF_FALSE(curAutoMarginComponent == 0,
                          "Expecting auto margins to have value '0' before we "
                          "update them");
        curAutoMarginComponent = curAutoMarginSize;

        mNumAutoMarginsInMainAxis--;
        mPackingSpaceRemaining -= curAutoMarginSize;
      }
    }
  }
}

void
MainAxisPositionTracker::TraversePackingSpace()
{
  if (mNumPackingSpacesRemaining) {
    NS_ABORT_IF_FALSE(
     mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_SPACE_BETWEEN ||
     mJustifyContent == NS_STYLE_JUSTIFY_CONTENT_SPACE_AROUND,
     "mNumPackingSpacesRemaining only applies for space-between/space-around");

    // (This is a warning, not an assertion, because it can fire in the valid
    // case where we only have 1 app unit to divide between 2 packing spaces.)
    NS_ABORT_IF_FALSE(mPackingSpaceRemaining >= 0,
                     "ran out of packing space earlier than we expected");

    // If this is the last packing space, then there's no need to do any
    // float math -- just use all the remaining packing space.
    nscoord curPackingSpace = (mNumPackingSpacesRemaining == 1) ?
      mPackingSpaceRemaining :
      NSToCoordRound(float(mPackingSpaceRemaining) /
                     mNumPackingSpacesRemaining);

    mPosition += curPackingSpace;
    mNumPackingSpacesRemaining--;
    mPackingSpaceRemaining -= curPackingSpace;
  }
}

CrossAxisPositionTracker::
  CrossAxisPositionTracker(nsFlexContainerFrame* aFlexContainerFrame,
                           const FlexboxAxisTracker& aAxisTracker,
                           const nsHTMLReflowState& aReflowState)
    : PositionTracker(aAxisTracker.GetCrossAxis())
{
  // Step over flex container's cross-start border/padding.
  EnterMargin(aReflowState.mComputedBorderPadding);
}

SingleLineCrossAxisPositionTracker::
  SingleLineCrossAxisPositionTracker(nsFlexContainerFrame* aFlexContainerFrame,
                                     const FlexboxAxisTracker& aAxisTracker,
                                     const nsTArray<ResolvedFlexItem>& aItems)
  : PositionTracker(aAxisTracker.GetCrossAxis()),
    mLineCrossSize(nscoord_MIN),
    mCrossStartToFurthestBaseline(nscoord_MIN)
{
}

void
SingleLineCrossAxisPositionTracker::
  ComputeLineCrossSize(const nsTArray<ResolvedFlexItem>& aItems)
{
  // NOTE: mCrossStartToFurthestBaseline is a member var rather than a local
  // var, because we'll need it when we're baseline-aligning our children.
  NS_ABORT_IF_FALSE(mCrossStartToFurthestBaseline == nscoord_MIN,
                    "Computing largest baseline offset more than once");

  nscoord crossEndToFurthestBaseline = nscoord_MIN;
  nscoord largestOuterCrossSize = 0;
  for (PRUint32 i = 0; i < aItems.Length(); ++i) {
    const ResolvedFlexItem& curItem = aItems[i];
    if (curItem.GetAlignSelf() == NS_STYLE_ALIGN_ITEMS_BASELINE &&
        curItem.GetNumAutoMarginsInAxis(mAxis) == 0) {
      mCrossStartToFurthestBaseline =
        NS_MAX(mCrossStartToFurthestBaseline,
               GetBaselineOffsetFromCrossStart(curItem));

      crossEndToFurthestBaseline =
        NS_MAX(crossEndToFurthestBaseline,
               GetBaselineOffsetFromCrossEnd(curItem));

    } else {
      nscoord curOuterCrossSize = curItem.GetBorderBoxCrossSize() +
        curItem.GetMarginSizeInAxis(mAxis);

      largestOuterCrossSize = NS_MAX(largestOuterCrossSize, curOuterCrossSize);
    }
  }

  // The line's cross-size is the larger of:
  //  (a) [largest cross-start-to-baseline + largest baseline-to-cross-end] of
  //      all baseline-aligned items with no cross-axis auto margins...
  // and
  //  (b) largest cross-size of all other children.
  mLineCrossSize = NS_MAX(mCrossStartToFurthestBaseline +
                          crossEndToFurthestBaseline,
                          largestOuterCrossSize);
}

nscoord
SingleLineCrossAxisPositionTracker::
  GetBaselineOffsetFromCrossStart(const ResolvedFlexItem& aItem) const
{
  Side crossStartSide = kAxisOrientationToSidesMap[mAxis][eAxisEdge_Start];
  
  // XXXdholbert This assumes cross axis is Top-To-Bottom.
  // For bottom-to-top support, probably want to make this depend on
  //   DoesAxisGrowInPositiveDirection(mAxis)
  return aItem.GetAscent() + aItem.GetMarginComponentForSide(crossStartSide);
}

nscoord
SingleLineCrossAxisPositionTracker::
  GetBaselineOffsetFromCrossEnd(const ResolvedFlexItem& aItem) const
{
  Side crossEndSide = kAxisOrientationToSidesMap[mAxis][eAxisEdge_End];

  // XXXdholbert This assumes cross axis is Top-To-Bottom.
  // For bottom-to-top support, probably want to make this depend on
  //   DoesAxisGrowInPositiveDirection(mAxis)

  // (Distiance from line cross-end to top of frame) -
  //  (Distance from top of frame to baseline)
  return (aItem.GetMarginComponentForSide(crossEndSide) +
          aItem.GetBorderBoxCrossSize()) -
    aItem.GetAscent();
    
}

void
SingleLineCrossAxisPositionTracker::
  ResolveStretchedCrossSize(ResolvedFlexItem& aItem)
{
  // We stretch IFF we are flex-item-align:stretch, have no auto margins in
  // cross axis, and have cross-axis size property == "auto". If any of those
  // conditions don't hold up, we can just return.
  if (aItem.GetAlignSelf() != NS_STYLE_ALIGN_ITEMS_STRETCH ||
      aItem.GetNumAutoMarginsInAxis(mAxis) != 0 ||
      GetSizePropertyForAxis(aItem.GetFrame(), mAxis).GetUnit() !=
        eStyleUnit_Auto) {
    return;
  }

  // Reserve space for margins & border & padding, and then use whatever
  // remains as our item's cross-size (clamped to its min/max range).
  nscoord stretchedSize = mLineCrossSize -
    aItem.GetMarginSizeInAxis(mAxis) -
    aItem.GetCrossBorderPaddingSize();
  
  stretchedSize = NS_CSS_MINMAX(stretchedSize,
                                aItem.GetCrossMinSize(),
                                aItem.GetCrossMaxSize());

  // Update the cross-size & make a note that it's stretched, so we know to
  // override the reflow state's computed cross-size in our final reflow.
  aItem.SetCrossSize(stretchedSize);
  aItem.SetIsStretched();
}

void
SingleLineCrossAxisPositionTracker::
  ResolveAutoMarginsInCrossAxis(ResolvedFlexItem& aItem)
{
  PRUint32 numAutoMargins = 0;
  nscoord spaceForAutoMargins = mLineCrossSize -
    aItem.GetBorderBoxCrossSize();

  if (spaceForAutoMargins <= 0) {
    return; // no extra space  --> nothing to do
  }

  // First: Count the number of auto margins (and subtract non-auto margins
  // from available space)
  for (PRUint32 i = 0; i < eNumAxisEdges; i++) {
    const nsStyleSides& styleMargin =
      aItem.GetFrame()->GetStyleMargin()->mMargin;
    Side side = kAxisOrientationToSidesMap[mAxis][i];
    if (styleMargin.GetUnit(side) == eStyleUnit_Auto) {
      numAutoMargins++;
    } else {
      spaceForAutoMargins -= aItem.GetMarginComponentForSide(side);
    }
  }

  NS_ABORT_IF_FALSE(numAutoMargins <= 2, "only 2 margin-sides along an axis");
  
  if (numAutoMargins == 0 || spaceForAutoMargins <= 0) {
    return; // No auto margins, or no space is left now that we've reserved
            // some for a non-auto margin --> nothing to do.
  }

  // OK, we have at least one auto margin and we have some available space.
  // Give each auto margin a share of the space.
  for (PRUint32 i = 0; i < eNumAxisEdges; i++) {
    const nsStyleSides& styleMargin =
      aItem.GetFrame()->GetStyleMargin()->mMargin;
    Side side = kAxisOrientationToSidesMap[mAxis][i];
    if (styleMargin.GetUnit(side) == eStyleUnit_Auto) {
      nscoord& curAutoMarginComponent = aItem.GetMarginComponentForSide(side);

      NS_ABORT_IF_FALSE(curAutoMarginComponent == 0,
                        "Expecting auto margins to have value '0' before we "
                        "update them");

      // NOTE: integer divison is fine here; numAutoMargins is either 1 or 2.
      // If it's 2 & spaceForAutoMargins is odd, 1st margin gets smaller half.
      curAutoMarginComponent = spaceForAutoMargins / numAutoMargins;
      numAutoMargins--;
      spaceForAutoMargins -= curAutoMarginComponent;
    }
  }
}

void
SingleLineCrossAxisPositionTracker::
  EnterAlignPackingSpace(const ResolvedFlexItem& aItem)
{
  // We don't do flex-item-align alignment on items that have auto margins
  // in the cross axis.
  if (aItem.GetNumAutoMarginsInAxis(mAxis)) {
    return;
  }

  switch (aItem.GetAlignSelf()) {
    case NS_STYLE_ALIGN_ITEMS_FLEX_START:
    case NS_STYLE_ALIGN_ITEMS_STRETCH:
      // No space to skip over -- we're done.
      // NOTE: 'stretch' behaves like 'start' once we've stretched any
      // auto-sized items (which we've already done).
      break;
    case NS_STYLE_ALIGN_ITEMS_FLEX_END:
      mPosition += (mLineCrossSize - (aItem.GetBorderBoxCrossSize() +
                                      aItem.GetMarginSizeInAxis(mAxis)));
      break;
    case NS_STYLE_ALIGN_ITEMS_CENTER:
      // Note: If cross-size is odd, the "after" space will get the extra unit.
      mPosition += (mLineCrossSize - (aItem.GetBorderBoxCrossSize() +
                                      aItem.GetMarginSizeInAxis(mAxis))) / 2;
      break;
    case NS_STYLE_ALIGN_ITEMS_BASELINE:
      NS_ABORT_IF_FALSE(mCrossStartToFurthestBaseline != nscoord_MIN,
                        "using uninitialized baseline offset");
      NS_ABORT_IF_FALSE(mCrossStartToFurthestBaseline >=
                        GetBaselineOffsetFromCrossStart(aItem),
                        "failed at finding largest ascent");

      // Advance so that aItem's baseline is aligned with
      // largest baseline offset.      
      mPosition += (mCrossStartToFurthestBaseline -
                    GetBaselineOffsetFromCrossStart(aItem));
      break;
    default:
      NS_NOTREACHED("Unexpected flex-item-align value");
      break;
  }
}

FlexboxAxisTracker::FlexboxAxisTracker(nsFlexContainerFrame* aFlexContainerFrame)
{
  PRUint32 flexDirection = aFlexContainerFrame->GetStylePosition()->mFlexDirection;
  PRUint32 cssDirection = aFlexContainerFrame->GetStyleVisibility()->mDirection;

  NS_ABORT_IF_FALSE(cssDirection == NS_STYLE_DIRECTION_LTR ||
                    cssDirection == NS_STYLE_DIRECTION_RTL,
                    "Unexpected computed value for 'direction' property");
  // (Not asserting for flexDirection here; it's checked by the switch below.)

  // XXXdholbert Once we support 'writing-mode', use its value here to further
  // customize what "row" and "column" translate to.
  switch (flexDirection) {
    case NS_STYLE_FLEX_DIRECTION_ROW:
      mMainAxis = cssDirection == NS_STYLE_DIRECTION_RTL ? eAxis_RL : eAxis_LR;
      mCrossAxis = eAxis_TB;
      break;
    case NS_STYLE_FLEX_DIRECTION_ROW_REVERSE:
      mMainAxis = cssDirection == NS_STYLE_DIRECTION_RTL ? eAxis_LR : eAxis_RL;
      mCrossAxis = eAxis_TB;
      break;
    case NS_STYLE_FLEX_DIRECTION_COLUMN:
      mMainAxis = eAxis_TB;
      mCrossAxis = eAxis_LR;
      break;
    case NS_STYLE_FLEX_DIRECTION_COLUMN_REVERSE:
      mMainAxis = eAxis_BT;
      mCrossAxis = eAxis_LR;
      break;
    default:
      NS_ABORT_IF_FALSE(false,
                        "Unexpected computed value for 'flex-flow' property");
      // Default to LTR & top-to-bottom
      mMainAxis = eAxis_LR;
      mCrossAxis = eAxis_TB;
      break;
  }

  AssertAxesSane();
}

nsTArray<ResolvedFlexItem>
nsFlexContainerFrame::GenerateResolvedFlexibleItems(
  nsPresContext* aPresContext,
  const nsHTMLReflowState& aReflowState,
  const FlexboxAxisTracker& aAxisTracker)

{
  // NOTE: To benefit from Return Value Optimization, we must only return
  // a single variable: resolvedFlexItems, declared at the end of this method.

  // Sort by flex-order:
  const nsTArray<SortableFrame> sortedChildren = BuildSortedChildArray(mFrames);

  // Build list of unresolved flex items:

  // XXXdholbert When we support multi-line, we  might want this to be a linked
  // list, so we can easily split into multiple lines.
  nsTArray<UnresolvedFlexItem> items(sortedChildren.Length());
  nscoord reservedMBPForChildren = 0;
  for (PRUint32 i = 0; i < sortedChildren.Length(); ++i) {
    nscoord currentReservedMBP;
    UnresolvedFlexItem currentItem =
      CreateUnresolvedFlexItemForChild(aPresContext,
                                       sortedChildren[i].GetFrame(),
                                       aReflowState, currentReservedMBP);
    items.AppendElement(currentItem);

    reservedMBPForChildren += currentReservedMBP;
  }

  // XXXdholbert FOR MULTI-LINE FLEX CONTAINERS: Do line-breaking here.
  // Then, this function would return an array of arrays, or a list of arrays,
  // or something like that. (one list/array per line)
  nscoord availableSpace =
    NS_MAX(0, aReflowState.ComputedWidth() - reservedMBPForChildren);

  // Distribute the space among our |items|...
  ResolveFlexibleLengths(availableSpace, items);

  // ...and convert them to ResolvedFlexItem objects.
  nsTArray<ResolvedFlexItem> resolvedFlexItems(items.Length());
  for (PRUint32 i = 0; i < items.Length(); ++i) {
    NS_ABORT_IF_FALSE(items[i].IsFrozen(),
                      "Flex item's main size should've been frozen by now");
    resolvedFlexItems.AppendElement(
      ResolvedFlexItem(items[i].mFrame,
                       items[i].mMainSize));
  }
  return resolvedFlexItems;
}

void
nsFlexContainerFrame::PositionItemInMainAxis(
  MainAxisPositionTracker&  aMainAxisPosnTracker,
  const nsHTMLReflowState&  aChildReflowState,
  ResolvedFlexItem&         aItem)
{
  // Resolve any main-axis 'auto' margins on aChild to an actual value.
  aMainAxisPosnTracker.ResolveAutoMarginsInMainAxis(aItem);

  // Advance our position tracker to child's upper-left content-box corner,
  // and use that as its position in the main axis.
  aMainAxisPosnTracker.EnterMargin(aItem.GetMargin());

  // XXXdholbert Assuming horizontal (left/right)
  nscoord borderBoxSize = aItem.GetMainSize() +
    aChildReflowState.mComputedBorderPadding.left +
    aChildReflowState.mComputedBorderPadding.right;

  aMainAxisPosnTracker.EnterChildFrame(borderBoxSize);

  aItem.SetMainPosition(aMainAxisPosnTracker.GetPosition());

  aMainAxisPosnTracker.ExitChildFrame(borderBoxSize);
  aMainAxisPosnTracker.ExitMargin(aItem.GetMargin());
  aMainAxisPosnTracker.TraversePackingSpace();
}

nsresult
nsFlexContainerFrame::SizeItemInCrossAxis(
  nsPresContext* aPresContext,
  const FlexboxAxisTracker& aAxisTracker,
  const nsHTMLReflowState& aChildReflowState,
  ResolvedFlexItem& aItem)
{
  // Invalidate child's old overflow rect (greedy; once the
  // display-list-based invalidation patches have landed, this can go away.)
  aItem.GetFrame()->InvalidateOverflowRect();


  // Cache resolved versions of min/max size & border/padding in cross axis,
  // for use in further resolving cross size position
  aItem.SetCrossMinSize(IsAxisHorizontal(aAxisTracker.GetCrossAxis()) ?
                        aChildReflowState.mComputedMinWidth :
                        aChildReflowState.mComputedMinHeight);
  aItem.SetCrossMaxSize(IsAxisHorizontal(aAxisTracker.GetCrossAxis()) ?
                        aChildReflowState.mComputedMaxWidth :
                        aChildReflowState.mComputedMaxHeight);

  nscoord crossBorderPaddingSize =
    GetMarginComponentForSideInternal(
      aChildReflowState.mComputedBorderPadding,
      kAxisOrientationToSidesMap[aAxisTracker.GetCrossAxis()][eAxisEdge_Start]) +
    GetMarginComponentForSideInternal(
      aChildReflowState.mComputedBorderPadding,
      kAxisOrientationToSidesMap[aAxisTracker.GetCrossAxis()][eAxisEdge_End]);

  aItem.SetCrossBorderPaddingSize(crossBorderPaddingSize);


  nsHTMLReflowMetrics childDesiredSize;
  nsReflowStatus childReflowStatus;
  nsresult rv = ReflowChild(aItem.GetFrame(), aPresContext,
                            childDesiredSize, aChildReflowState,
                            0, 0, 0, childReflowStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXXdholbert Once we do pagination / splitting, we'll need to actually
  // handle incomplete childReflowStatuses. But for now, we give our kids
  // unconstrained available height, which means they should always complete.
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(childReflowStatus),
               "We gave flex item unconstrained available height, so it "
               "should be complete");

  NS_ASSERTION(childDesiredSize.width ==
               aItem.GetMainSize() +
               aChildReflowState.mComputedBorderPadding.left +
               aChildReflowState.mComputedBorderPadding.right,
               "child didn't take the size that its flex container assigned");

  // Tell the child we're done with its initial reflow.
  // (Necessary for e.g. GetBaseline() to work below w/out asserting)
  rv = FinishReflowChild(aItem.GetFrame(), aPresContext,
                         &aChildReflowState, childDesiredSize, 0, 0, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  // Save the sizing info that we learned from this reflow
  // -----------------------------------------------------

  // Tentatively accept the child's desired size, minus border/padding, as its
  // cross-size:
  NS_ASSERTION(childDesiredSize.height >= aItem.GetCrossBorderPaddingSize(),
               "Child should ask for at least enough space for border/padding");
  aItem.SetCrossSize(childDesiredSize.height -
                     aItem.GetCrossBorderPaddingSize());

  // If we need to do baseline-alignment, store the child's ascent.
  if (aItem.GetAlignSelf() == NS_STYLE_ALIGN_ITEMS_BASELINE) {
    // Cribbed from nsLineLayout::PlaceFrame
    // This is a little hacky. Basically: we'll trust our child on its
    // GetBaseline(), but not for display:block.  Blocks are supposed to use
    // the baseline of the first line, but nsBlockFrame::GetBaseline()
    // returns the baseline of the last line (which is appropriate for
    // inline-block), so we can't use GetBaseline() for display:block.
    if (childDesiredSize.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
      if (NS_STYLE_DISPLAY_BLOCK ==
          aItem.GetFrame()->GetStyleDisplay()->mDisplay) {
        if (!nsLayoutUtils::GetFirstLineBaseline(aItem.GetFrame(),
                                                 &childDesiredSize.ascent)) {
          childDesiredSize.ascent = childDesiredSize.height;
        }
      } else {
        childDesiredSize.ascent = aItem.GetFrame()->GetBaseline();
      }
    }
    aItem.SetAscent(childDesiredSize.ascent);
  }

  return NS_OK;
}

void
nsFlexContainerFrame::PositionItemInCrossAxis(
  nscoord aLineStartPosition,
  SingleLineCrossAxisPositionTracker& aLineCrossAxisPosnTracker,
  ResolvedFlexItem& aItem)
{
  NS_ASSERTION(aLineCrossAxisPosnTracker.GetPosition() == 0,
               "per-line cross-axis posiiton tracker wasn't correctly reset");

  // Resolve any to-be-stretched cross-sizes & auto margins in cross axis.
  aLineCrossAxisPosnTracker.ResolveStretchedCrossSize(aItem);
  aLineCrossAxisPosnTracker.ResolveAutoMarginsInCrossAxis(aItem);

  // Compute the cross-axis position of this item
  aLineCrossAxisPosnTracker.EnterAlignPackingSpace(aItem);
  aLineCrossAxisPosnTracker.EnterMargin(aItem.GetMargin());
  aLineCrossAxisPosnTracker.EnterChildFrame(aItem.GetBorderBoxCrossSize());

  aItem.SetCrossPosition(aLineStartPosition +
                         aLineCrossAxisPosnTracker.GetPosition());

  // Back out to cross-axis edge of the line.
  aLineCrossAxisPosnTracker.ResetPosition();
}

NS_IMETHODIMP
nsFlexContainerFrame::Reflow(nsPresContext*           aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFlexContainerFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  PR_LOG(nsFlexContainerFrameLM, PR_LOG_DEBUG,
         ("Reflow() for nsFlexContainerFrame %p\n", this));
#ifdef DEBUG
  SanityCheckAnonymousFlexItems();
#endif // DEBUG


  // XXXdholbert For simplicity, we'll treat NS_SUBTREE_DIRTY(this) as a signal
  // that I and all my children need to be reflowed.  (Technically, we might
  // only need to reflow some children; but the ones that don't need a reflow
  // can figure that out themselves, since their NS_FRAME_IS_DIRTY bit
  // shouldn't be set.
  bool shouldReflowAllChildren =
    NS_SUBTREE_DIRTY(this) || aReflowState.ShouldReflowAllKids();

  const FlexboxAxisTracker axisTracker(this);

  // Generate a list of our flex items (already sorted, with flexible sizes
  // already resolved)
  nsTArray<ResolvedFlexItem> items =
    GenerateResolvedFlexibleItems(aPresContext, aReflowState, axisTracker);

  nscoord frameMainSize = aReflowState.ComputedWidth() +
    aReflowState.mComputedBorderPadding.left +
    aReflowState.mComputedBorderPadding.right;

  nscoord frameCrossSize;

  if (!shouldReflowAllChildren) {
    // Children don't need reflow --> assume our content-box size is the same
    // since our last reflow.
    frameCrossSize = mCachedContentBoxCrossSize +
      aReflowState.mComputedBorderPadding.top +
      aReflowState.mComputedBorderPadding.bottom;
  } else {
    MainAxisPositionTracker mainAxisPosnTracker(this, axisTracker,
                                                aReflowState, items);

    // First loop: Compute main axis position & cross-axis size of each item
    for (PRUint32 i = 0; i < items.Length(); ++i) {
      ResolvedFlexItem& curItem = items[i];

      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         curItem.GetFrame(),
                                         nsSize(curItem.GetMainSize(),
                                                NS_UNCONSTRAINEDSIZE),
                                         -1, -1, false);
      childReflowState.mFlags.mFlexContainerHasDistributedSpace = true;
      // XXXdholbert assuming horizontal
      childReflowState.mFlags.mFlexContainerIsHorizontal = true;
      childReflowState.Init(aPresContext);

      // Initialize item's margin to our reflow state computed-margin values.
      // (The PositionItemIn*Axis method-calls below will update any
      // margin-components that are really "auto".)
      curItem.SetMargin(childReflowState.mComputedMargin);

      PositionItemInMainAxis(mainAxisPosnTracker, childReflowState, curItem);

      nsresult rv =
        SizeItemInCrossAxis(aPresContext, axisTracker,
                            childReflowState, curItem);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // SIZE & POSITION THE FLEX LINE (IN CROSS AXIS)
    // Set up state for cross-axis alignment, at a high level (outside the
    // scope of a particular flex line)
    CrossAxisPositionTracker
      crossAxisPosnTracker(this, axisTracker, aReflowState);

    // Set up state for cross-axis-positioning of children _within_ a single
    // flex line.
    SingleLineCrossAxisPositionTracker
      lineCrossAxisPosnTracker(this, axisTracker, items);

    lineCrossAxisPosnTracker.ComputeLineCrossSize(items);
    // XXXdholbert Once we've got multiline flexbox support: here, after we've
    // computed the cross size of all lines, we need to check if if
    // 'align-content' is 'stretch' -- if it is, we need to give each line an
    // additional share of our flex container's desired cross-size. (if it's
    // not NS_AUTOHEIGHT and there's any cross-size left over to distribute)

    // Figure out our flex container's cross size
    mCachedContentBoxCrossSize = aReflowState.ComputedHeight();
    if (mCachedContentBoxCrossSize == NS_AUTOHEIGHT) {
      // height = 'auto': shrink-wrap our line(s)
      mCachedContentBoxCrossSize =
        lineCrossAxisPosnTracker.GetLineCrossSize();
    } else {
      // XXXdholbert When we support multi-line flex containers, we should
      // distribute any extra space among or between our lines here according
      // to 'align-content'. For now, we do the single-line special behavior:
      // "If the flex container has only a single line (even if it's a
      // multi-line flex container), the cross size of the flex line is the
      // flex container's inner cross size."
      lineCrossAxisPosnTracker.SetLineCrossSize(mCachedContentBoxCrossSize);
    }
    frameCrossSize = mCachedContentBoxCrossSize +
      aReflowState.mComputedBorderPadding.top +
      aReflowState.mComputedBorderPadding.bottom;


    // XXXdholbert FOLLOW ACTUAL RULES FOR FLEX CONTAINER BASELINE
    // If we have any baseline-aligned items on first line, use their baseline.
    // ...ELSE if we have at least one flex item and our first flex item's
    //         baseline is parallel to main axis, then use that baseline.
    // ...ELSE use "after" edge of content box.
    // Default baseline: the "after" edge of content box. (Note: if we have any
    // flex items, they'll override this.)
    mCachedAscent = mCachedContentBoxCrossSize +
      aReflowState.mComputedBorderPadding.top;

    // Position the items in cross axis, within their line
    for (PRUint32 i = 0; i < items.Length(); ++i) {
      PositionItemInCrossAxis(crossAxisPosnTracker.GetPosition(),
                              lineCrossAxisPosnTracker, items[i]);
    }

    // FINAL REFLOW: Give each child frame another chance to reflow, now that
    // we know its final size and position.
    for (PRUint32 i = 0; i < items.Length(); ++i) {
      ResolvedFlexItem& curItem = items[i];
      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         curItem.GetFrame(),
                                         nsSize(curItem.GetMainSize(),
                                                NS_UNCONSTRAINEDSIZE),
                                         -1, -1, false);
      childReflowState.mFlags.mFlexContainerHasDistributedSpace = true;
      // XXXdholbert assuming horizontal
      childReflowState.mFlags.mFlexContainerIsHorizontal = true;
      childReflowState.Init(aPresContext);

      // Override reflow state's computed cross-size, for stretched items.
      if (curItem.IsStretched()) {
        NS_ABORT_IF_FALSE(curItem.GetAlignSelf() ==
                          NS_STYLE_ALIGN_ITEMS_STRETCH,
                          "stretched item w/o 'align-self: stretch'?");
        if (IsAxisHorizontal(axisTracker.GetCrossAxis())) {
          childReflowState.SetComputedWidth(curItem.GetCrossSize());
        } else {
          childReflowState.SetComputedHeight(curItem.GetCrossSize());
        }
      }

      // XXXdholbert Assuming horizontal
      nscoord x = curItem.GetMainPosition();
      nscoord y = curItem.GetCrossPosition();
      if (!DoesAxisGrowInPositiveDirection(axisTracker.GetMainAxis())) {
        x = frameMainSize - x;
      }
      if (!DoesAxisGrowInPositiveDirection(axisTracker.GetCrossAxis())) {
        y = frameCrossSize - y;
      }

      nsHTMLReflowMetrics childDesiredSize;
      nsReflowStatus childReflowStatus;
      nsresult rv = ReflowChild(curItem.GetFrame(), aPresContext,
                                childDesiredSize, childReflowState,
                                x, y, 0, childReflowStatus);
      NS_ENSURE_SUCCESS(rv, rv);

      // XXXdholbert Once we do pagination / splitting, we'll need to actually
      // handle incomplete childReflowStatuses. But for now, we give our kids
      // unconstrained available height, which means they should always
      // complete.
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(childReflowStatus),
                   "We gave flex item unconstrained available height, so it "
                   "should be complete");

      // Apply CSS relative positioning
      const nsStyleDisplay* styleDisp = curItem.GetFrame()->GetStyleDisplay();
      if (NS_STYLE_POSITION_RELATIVE == styleDisp->mPosition) {
        x += childReflowState.mComputedOffsets.left;
        y += childReflowState.mComputedOffsets.top;
      }

      rv = FinishReflowChild(curItem.GetFrame(), aPresContext,
                             &childReflowState, childDesiredSize, x, y, 0);
      NS_ENSURE_SUCCESS(rv, rv);

      // Invalidate child's new overflow rect (greedy; once the
      // display-list-based invalidation patches have landed, this can go away.)
      curItem.GetFrame()->InvalidateOverflowRect();
    }
  }

  aDesiredSize.width = frameMainSize;
  aDesiredSize.height = frameCrossSize;
  aDesiredSize.ascent = mCachedAscent;

  // Overflow area = union(my overflow area, kids' overflow areas)
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  for (nsIFrame* child = mFrames.FirstChild(); child;
       child = child->GetNextSibling()) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, child);
  }

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize)

  aStatus = NS_FRAME_COMPLETE;

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize,
                                 aReflowState, aStatus);

  return NS_OK;
}

/* virtual */ nscoord
nsFlexContainerFrame::GetMinWidth(nsRenderingContext* aRenderingContext)
{
  // XXXdholbert Assuming horizontal & single-line for now.
  // If we're horizontal+multi-line OR if we're vertical, our min-width
  // is just max(child min widths).  (I'm hoping that's true
  // even if we're vertical+multi-line.)

  // For a horizontal single-line flex container, min width is:
  //  sum(child min widths)
  nscoord minWidth = 0;
  for (nsIFrame* child = mFrames.FirstChild(); child;
       child = child->GetNextSibling()) {
    nscoord childMinWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, child,
                                           nsLayoutUtils::MIN_WIDTH);
    minWidth += childMinWidth;
  }
  return minWidth;
}

/* virtual */ nscoord
nsFlexContainerFrame::GetPrefWidth(nsRenderingContext* aRenderingContext)
{
  // XXXdholbert Optimization: We should cache our intrinsic width as like
  // nsBlockFrame does (and return it early from this function if it's set).
  // Whenever anything happens that might change it, set it to
  // NS_INTRINSIC_WIDTH_UNKNOWN (like nsBlockFrame::MarkIntrinsicWidthsDirty
  // does)

  // XXXdholbert Assuming horizontal for now.
  // If we're vertical, this should be max(child pref widths)
  // That's assuming no wrapping is needed. Once we've got multi-line, if
  // wrapping is needed, it we might need to be sum(child pref widths),
  // if we interpret pref-width to mean "width we'd be if we take all the
  // linebreaks and thereby end up all on one _horizontal_ line"...

  // For a horizontal flex container, pref width is:
  //  sum(kids' pref widths)
  nscoord prefWidth = 0;
  for (nsIFrame* child = mFrames.FirstChild(); child;
       child = child->GetNextSibling()) {
    nscoord childPrefWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, child,
                                           nsLayoutUtils::PREF_WIDTH);
    prefWidth += childPrefWidth;
  }
  return prefWidth;
}
