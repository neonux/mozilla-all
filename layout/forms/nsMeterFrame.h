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
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vincent Lamotte <Vincent.Lamotte@ensimag.imag.fr>
 *   Laurent Dulary <Laurent.Dulary@ensimag.imag.fr>
 *   Yoan Teboul <Yoan.Teboul@ensimag.imag.fr>
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

#ifndef nsMeterFrame_h___
#define nsMeterFrame_h___

#include "nsContainerFrame.h"
#include "nsIAnonymousContentCreator.h"
#include "nsCOMPtr.h"

class nsMeterFrame : public nsContainerFrame,
                     public nsIAnonymousContentCreator

{
public:
  NS_DECL_QUERYFRAME_TARGET(nsMeterFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  nsMeterFrame(nsStyleContext* aContext);
  virtual ~nsMeterFrame();

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_IMETHOD Reflow(nsPresContext*           aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("Meter"), aResult);
  }
#endif

  virtual bool IsLeaf() const { return true; }

  // nsIAnonymousContentCreator
  virtual nsresult CreateAnonymousContent(nsTArray<ContentInfo>& aElements);
  virtual void AppendAnonymousContentTo(nsBaseContentList& aElements,
                                        PRUint32 aFilter);

  NS_IMETHOD AttributeChanged(PRInt32  aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32  aModType);

  virtual nsSize ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                 nsSize aCBSize, nscoord aAvailableWidth,
                                 nsSize aMargin, nsSize aBorder,
                                 nsSize aPadding, bool aShrinkWrap);

  virtual bool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsContainerFrame::IsFrameOfType(aFlags &
      ~(nsIFrame::eReplaced | nsIFrame::eReplacedContainsBlock));
  }

  /**
   * Returns whether the frame and its child should use the native style.
   */
  bool ShouldUseNativeStyle() const;

protected:
  // Helper function which reflow the anonymous div frame.
  void ReflowBarFrame(nsIFrame*                aBarFrame,
                      nsPresContext*           aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus);
  /**
   * The div used to show the meter bar.
   * @see nsMeterFrame::CreateAnonymousContent
   */
  nsCOMPtr<nsIContent> mBarDiv;
};

#endif
