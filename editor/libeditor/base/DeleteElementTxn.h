/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteElementTxn_h__
#define DeleteElementTxn_h__

#include "EditTxn.h"

#include "nsIContent.h"
#include "nsCOMPtr.h"

class nsRangeUpdater;
class nsEditor;

/**
 * A transaction that deletes a single element
 */
class DeleteElementTxn : public EditTxn
{
public:
  /** initialize the transaction.
    * @param aElement the node to delete
    */
  nsresult Init(nsEditor* aEditor, nsINode* aNode,
                nsRangeUpdater* aRangeUpdater);

  DeleteElementTxn();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteElementTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();

protected:

  /** the element to delete */
  nsCOMPtr<nsINode> mNode;

  /** parent of node to delete */
  nsCOMPtr<nsINode> mParent;

  /** next sibling to remember for undo/redo purposes */
  nsCOMPtr<nsIContent> mRefNode;

  /** the editor for this transaction */
  nsEditor* mEditor;

  /** range updater object */
  nsRangeUpdater* mRangeUpdater;
};

#endif
