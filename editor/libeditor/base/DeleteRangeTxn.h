/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteRangeTxn_h__
#define DeleteRangeTxn_h__

#include "EditAggregateTxn.h"
#include "nsRange.h"
#include "nsEditor.h"
#include "nsCOMPtr.h"

class nsRangeUpdater;

/**
 * A transaction that deletes an entire range in the content tree
 */
class DeleteRangeTxn : public EditAggregateTxn
{
public:
  /** initialize the transaction.
    * @param aEditor the object providing basic editing operations
    * @param aRange  the range to delete
    */
  nsresult Init(nsEditor* aEditor,
                nsRange* aRange,
                nsRangeUpdater* aRangeUpdater);

  DeleteRangeTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteRangeTxn, EditAggregateTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();

protected:

  nsresult CreateTxnsToDeleteBetween(nsINode* aNode,
                                     PRInt32 aStartOffset,
                                     PRInt32 aEndOffset);

  nsresult CreateTxnsToDeleteNodesBetween();

  nsresult CreateTxnsToDeleteContent(nsINode* aParent,
                                     PRInt32 aOffset,
                                     nsIEditor::EDirection aAction);

protected:

  /** p1 in the range */
  nsRefPtr<nsRange> mRange;

  /** p1 in the range */
  nsCOMPtr<nsINode> mStartParent;

  /** p1 offset */
  PRInt32 mStartOffset;

  /** p2 in the range */
  nsCOMPtr<nsINode> mEndParent;

  /** the closest common parent of p1 and p2 */
  nsCOMPtr<nsINode> mCommonParent;

  /** p2 offset */
  PRInt32 mEndOffset;

  /** the editor for this transaction */
  nsEditor* mEditor;

  /** range updater object */
  nsRangeUpdater* mRangeUpdater;
};

#endif
