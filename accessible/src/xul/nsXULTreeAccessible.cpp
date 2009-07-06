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
 *   Author: Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsIDOMXULElement.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULTreeElement.h"
#include "nsITreeSelection.h"
#include "nsXULTreeAccessibleWrap.h"
#include "nsIMutableArray.h"
#include "nsComponentManagerUtils.h"

#ifdef MOZ_ACCESSIBILITY_ATK
#include "nsIAccessibleTable.h"
#endif

/* static */
PRBool nsXULTreeAccessible::IsColumnHidden(nsITreeColumn *aColumn)
{
  nsCOMPtr<nsIDOMElement> element;
  aColumn->GetElement(getter_AddRefs(element));
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);
  return content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::hidden,
                              nsAccessibilityAtoms::_true, eCaseMatters);
}

/* static */
already_AddRefed<nsITreeColumn> nsXULTreeAccessible::GetNextVisibleColumn(nsITreeColumn *aColumn)
{
  // Skip hidden columns.
  nsCOMPtr<nsITreeColumn> nextColumn;
  aColumn->GetNext(getter_AddRefs(nextColumn));
  while (nextColumn && IsColumnHidden(nextColumn)) {
    nsCOMPtr<nsITreeColumn> tempColumn;
    nextColumn->GetNext(getter_AddRefs(tempColumn));
    nextColumn.swap(tempColumn);
  }

  nsITreeColumn *retCol = nsnull;
  nextColumn.swap(retCol);
  return retCol;
}

/* static */
already_AddRefed<nsITreeColumn> nsXULTreeAccessible::GetFirstVisibleColumn(nsITreeBoxObject *aTree)
{
  nsCOMPtr<nsITreeColumns> cols;
  nsCOMPtr<nsITreeColumn> column;
  aTree->GetColumns(getter_AddRefs(cols));
  if (cols) {
    cols->GetFirstColumn(getter_AddRefs(column));
  }

  if (column && IsColumnHidden(column)) {
    column = GetNextVisibleColumn(column);
  }
  NS_ENSURE_TRUE(column, nsnull);

  nsITreeColumn *retCol = nsnull;
  column.swap(retCol);
  return retCol;
}

/* static */
already_AddRefed<nsITreeColumn> nsXULTreeAccessible::GetLastVisibleColumn(nsITreeBoxObject *aTree)
{
  nsCOMPtr<nsITreeColumns> cols;
  nsCOMPtr<nsITreeColumn> column;
  aTree->GetColumns(getter_AddRefs(cols));
  if (cols) {
    cols->GetLastColumn(getter_AddRefs(column));
  }

  // Skip hidden columns.
  while (column && IsColumnHidden(column)) {
    nsCOMPtr<nsITreeColumn> tempColumn;
    column->GetPrevious(getter_AddRefs(tempColumn));
    column.swap(tempColumn);
  }
  NS_ENSURE_TRUE(column, nsnull);

  nsITreeColumn *retCol = nsnull;
  column.swap(retCol);
  return retCol;
}

// ---------- nsXULTreeAccessible ----------

nsXULTreeAccessible::nsXULTreeAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell):
nsXULSelectableAccessible(aDOMNode, aShell),
mAccessNodeCache(nsnull)

{
  GetTreeBoxObject(aDOMNode, getter_AddRefs(mTree));
  if (mTree)
    mTree->GetView(getter_AddRefs(mTreeView));
  NS_ASSERTION(mTree && mTreeView, "Can't get mTree or mTreeView!\n");
  mAccessNodeCache = new nsAccessNodeHashtable;
  mAccessNodeCache->Init(kDefaultTreeCacheSize);
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeAccessible,
                             nsXULSelectableAccessible,
                             nsXULTreeAccessible)
                                                                                                       


// Get the nsITreeBoxObject interface from any levels DOMNode under the <tree>
void nsXULTreeAccessible::GetTreeBoxObject(nsIDOMNode *aDOMNode, nsITreeBoxObject **aBoxObject)
{
  nsAutoString name;
  nsCOMPtr<nsIDOMNode> parentNode, currentNode;

  // Find DOMNode's parents recursively until reach the <tree> tag
  currentNode = aDOMNode;
  while (currentNode) {
    currentNode->GetLocalName(name);
    if (name.EqualsLiteral("tree")) {
      // We will get the nsITreeBoxObject from the tree node
      nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(currentNode));
      if (xulElement) {
        nsCOMPtr<nsIBoxObject> box;
        xulElement->GetBoxObject(getter_AddRefs(box));
        nsCOMPtr<nsITreeBoxObject> treeBox(do_QueryInterface(box));
        if (treeBox) {
          *aBoxObject = treeBox;
          NS_ADDREF(*aBoxObject);
          return;
        }
      }
    }
    currentNode->GetParentNode(getter_AddRefs(parentNode));
    currentNode = parentNode;
  }

  *aBoxObject = nsnull;
}

nsresult
nsXULTreeAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  // Get focus status from base class
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);
  
  // see if we are multiple select if so set ourselves as such
  nsCOMPtr<nsIDOMElement> element (do_QueryInterface(mDOMNode));
  if (element) {
    // the default selection type is multiple
    nsAutoString selType;
    element->GetAttribute(NS_LITERAL_STRING("seltype"), selType);
    if (selType.IsEmpty() || !selType.EqualsLiteral("single"))
      *aState |= nsIAccessibleStates::STATE_MULTISELECTABLE;
  }

  *aState |= nsIAccessibleStates::STATE_READONLY |
             nsIAccessibleStates::STATE_FOCUSABLE;

  return NS_OK;
}

// The value is the first selected child
NS_IMETHODIMP nsXULTreeAccessible::GetValue(nsAString& _retval)
{
  _retval.Truncate(0);

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (! selection)
    return NS_ERROR_FAILURE;

  PRInt32 currentIndex;
  nsCOMPtr<nsIDOMElement> selectItem;
  selection->GetCurrentIndex(&currentIndex);
  if (currentIndex >= 0) {
    nsCOMPtr<nsITreeColumn> keyCol;

    nsCOMPtr<nsITreeColumns> cols;
    mTree->GetColumns(getter_AddRefs(cols));
    if (cols)
      cols->GetKeyColumn(getter_AddRefs(keyCol));

    return mTreeView->GetCellText(currentIndex, keyCol, _retval);
  }

  return NS_OK;
}

PRBool
nsXULTreeAccessible::IsDefunct()
{
  return nsXULSelectableAccessible::IsDefunct() || !mTree || !mTreeView;
}

nsresult
nsXULTreeAccessible::Shutdown()
{
  mTree = nsnull;
  mTreeView = nsnull;

  nsXULSelectableAccessible::Shutdown();

  if (mAccessNodeCache) {
    ClearCache(*mAccessNodeCache);
    delete mAccessNodeCache;
    mAccessNodeCache = nsnull;
  }

  return NS_OK;
}

nsresult
nsXULTreeAccessible::GetRoleInternal(PRUint32 *aRole)
{
  NS_ASSERTION(mTree, "No tree view");
  PRInt32 colCount = 0;
  if (NS_SUCCEEDED(GetColumnCount(mTree, &colCount)) && (colCount > 1))
    *aRole = nsIAccessibleRole::ROLE_TREE_TABLE;
  else
    *aRole = nsIAccessibleRole::ROLE_OUTLINE;
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetFirstChild(nsIAccessible **aFirstChild)
{
  nsAccessible::GetFirstChild(aFirstChild);

  // in normal case, tree's first child should be treecols, if it is not here, 
  //   use the first row as tree's first child
  if (*aFirstChild == nsnull) {
    NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

    PRInt32 rowCount;
    mTreeView->GetRowCount(&rowCount);
    if (rowCount > 0) {
      nsCOMPtr<nsITreeColumn> column = GetFirstVisibleColumn(mTree);
      GetCachedTreeitemAccessible(0, column, aFirstChild);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeAccessible::GetLastChild(nsIAccessible **aLastChild)
{
  NS_ENSURE_ARG_POINTER(aLastChild);
  *aLastChild = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  PRInt32 rowCount = 0;
  mTreeView->GetRowCount(&rowCount);
  if (rowCount > 0) {
    nsCOMPtr<nsITreeColumn> column = GetLastVisibleColumn(mTree);
    GetCachedTreeitemAccessible(rowCount - 1, column, aLastChild);

    if (*aLastChild)
      return NS_OK;
  }

  // If there is not any rows, use treecols as tree's last child.
  return nsAccessible::GetLastChild(aLastChild);
}

// tree's children count is row count + treecols count
NS_IMETHODIMP nsXULTreeAccessible::GetChildCount(PRInt32 *aAccChildCount)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsAccessible::GetChildCount(aAccChildCount);

  if (*aAccChildCount != eChildCountUninitialized) {
    PRInt32 rowCount;
    mTreeView->GetRowCount(&rowCount);
    *aAccChildCount += rowCount;
  }
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetFocusedChild(nsIAccessible **aFocusedChild) 
{
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
    do_QueryInterface(mDOMNode);
  if (multiSelect) {
    PRInt32 row;
    multiSelect->GetCurrentIndex(&row);
    if (row >= 0) {
      GetCachedTreeitemAccessible(row, nsnull, aFocusedChild);
      if (*aFocusedChild)
        return NS_OK;
    }
  }
  NS_ADDREF(*aFocusedChild = this);
  return NS_OK;
}

// nsAccessible::GetChildAtPoint()
nsresult
nsXULTreeAccessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                     PRBool aDeepestChild,
                                     nsIAccessible **aChild)
{
  nsIFrame *frame = GetFrame();
  if (!frame)
    return NS_ERROR_FAILURE;

  nsPresContext *presContext = frame->PresContext();
  nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();

  nsIFrame *rootFrame = presShell->GetRootFrame();
  NS_ENSURE_STATE(rootFrame);

  nsIntRect rootRect = rootFrame->GetScreenRectExternal();

  PRInt32 clientX = presContext->AppUnitsToIntCSSPixels(
    presContext->DevPixelsToAppUnits(aX - rootRect.x));
  PRInt32 clientY = presContext->AppUnitsToIntCSSPixels(
    presContext->DevPixelsToAppUnits(aY - rootRect.y));

  PRInt32 row = -1;
  nsCOMPtr<nsITreeColumn> column;
  nsCAutoString childEltUnused;
  mTree->GetCellAt(clientX, clientY, &row, getter_AddRefs(column),
                   childEltUnused);

  // If we failed to find tree cell for the given point then it might be
  // tree columns.
  if (row == -1 || !column)
    return nsXULSelectableAccessible::
      GetChildAtPoint(aX, aY, aDeepestChild, aChild);

  GetCachedTreeitemAccessible(row, column, aChild);
  return NS_OK;
}

// Ask treeselection to get all selected children
NS_IMETHODIMP nsXULTreeAccessible::GetSelectedChildren(nsIArray **_retval)
{
  *_retval = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (!selection)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsIMutableArray> selectedAccessibles =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_STATE(selectedAccessibles);

  PRInt32 rowIndex, rowCount;
  PRBool isSelected;
  mTreeView->GetRowCount(&rowCount);
  for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
    selection->IsSelected(rowIndex, &isSelected);
    if (isSelected) {
      nsCOMPtr<nsIAccessible> tempAccess;
        GetCachedTreeitemAccessible(rowIndex, nsnull,
                                    getter_AddRefs(tempAccess));
      NS_ENSURE_STATE(tempAccess);

      selectedAccessibles->AppendElement(tempAccess, PR_FALSE);
    }
  }

  PRUint32 length;
  selectedAccessibles->GetLength(&length);
  if (length != 0) {
    *_retval = selectedAccessibles;
    NS_IF_ADDREF(*_retval);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::GetSelectionCount(PRInt32 *aSelectionCount)
{
  *aSelectionCount = 0;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->GetCount(aSelectionCount);

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState)
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    selection->IsSelected(aIndex, aSelState);
    if ((!(*aSelState) && eSelection_Add == aMethod) || 
        ((*aSelState) && eSelection_Remove == aMethod))
      return selection->ToggleSelect(aIndex);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::AddChildToSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Add, &isSelected);
}

NS_IMETHODIMP nsXULTreeAccessible::RemoveChildFromSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Remove, &isSelected);
}

NS_IMETHODIMP nsXULTreeAccessible::IsChildSelected(PRInt32 aIndex, PRBool *_retval)
{
  return ChangeSelection(aIndex, eSelection_GetState, _retval);
}

NS_IMETHODIMP nsXULTreeAccessible::ClearSelection()
{
  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->ClearSelection();

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeAccessible::RefSelection(PRInt32 aIndex, nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (!selection)
    return NS_ERROR_FAILURE;

  PRInt32 rowIndex, rowCount;
  PRInt32 selCount = 0;
  PRBool isSelected;
  mTreeView->GetRowCount(&rowCount);
  for (rowIndex = 0; rowIndex < rowCount; rowIndex++) {
    selection->IsSelected(rowIndex, &isSelected);
    if (isSelected) {
      if (selCount == aIndex) {
        GetCachedTreeitemAccessible(rowIndex, nsnull, aAccessible);
        return NS_OK;
      }
      selCount++;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeAccessible::SelectAllSelection(PRBool *_retval)
{
  *_retval = PR_FALSE;

  NS_ENSURE_TRUE(mTree && mTreeView, NS_ERROR_FAILURE);

  // see if we are multiple select if so set ourselves as such
  nsCOMPtr<nsIDOMElement> element (do_QueryInterface(mDOMNode));
  if (element) {
    nsAutoString selType;
    element->GetAttribute(NS_LITERAL_STRING("seltype"), selType);
    if (selType.IsEmpty() || !selType.EqualsLiteral("single")) {
      *_retval = PR_TRUE;
      nsCOMPtr<nsITreeSelection> selection;
      mTreeView->GetSelection(getter_AddRefs(selection));
      if (selection)
        selection->SelectAll();
    }
  }

  return NS_OK;
}

// nsXULTreeAccessible

void
nsXULTreeAccessible::GetCachedTreeitemAccessible(PRInt32 aRow,
                                                 nsITreeColumn* aColumn,
                                                 nsIAccessible** aAccessible)
{
  *aAccessible = nsnull;

  NS_ASSERTION(mAccessNodeCache, "No accessibility cache for tree");
  NS_ASSERTION(mTree && mTreeView, "Can't get mTree or mTreeView!\n");

  nsCOMPtr<nsITreeColumn> col;
#ifdef MOZ_ACCESSIBILITY_ATK
  col = aColumn;
#endif
  PRInt32 columnIndex = -1;

  if (!col && mTree) {
    nsCOMPtr<nsITreeColumns> cols;
    mTree->GetColumns(getter_AddRefs(cols));
    if (cols)
      cols->GetKeyColumn(getter_AddRefs(col));
  }

  // Do not create accessible for treeitem if there is no column in the tree
  // because it doesn't render treeitems properly.
  if (!col)
    return;

  col->GetIndex(&columnIndex);

  nsCOMPtr<nsIAccessNode> accessNode;
  GetCacheEntry(*mAccessNodeCache, (void*)(aRow * kMaxTreeColumns + columnIndex), getter_AddRefs(accessNode));

  if (!accessNode) {
    nsRefPtr<nsAccessNode> treeItemAcc =
      new nsXULTreeitemAccessibleWrap(this, mDOMNode, mWeakShell, aRow, col);
    if (!treeItemAcc)
      return;

    nsresult rv = treeItemAcc->Init();
    if (NS_FAILED(rv))
      return;

    accessNode = treeItemAcc;
    PutCacheEntry(*mAccessNodeCache, (void*)(aRow * kMaxTreeColumns + columnIndex), accessNode);
  }

  CallQueryInterface(accessNode, aAccessible);
}

void
nsXULTreeAccessible::InvalidateCache(PRInt32 aRow, PRInt32 aCount)
{
  if (IsDefunct())
    return;

  // Do not invalidate the cache if rows have been inserted.
  if (aCount > 0)
    return;

  nsCOMPtr<nsITreeColumns> cols;
  mTree->GetColumns(getter_AddRefs(cols));
  if (!cols)
    return;

#ifdef MOZ_ACCESSIBILITY_ATK
  PRInt32 colsCount = 0;
  nsresult rv = cols->GetCount(&colsCount);
  if (NS_FAILED(rv))
    return;
#else
  nsCOMPtr<nsITreeColumn> col;
  cols->GetKeyColumn(getter_AddRefs(col));
  if (!col)
    return;

  PRInt32 colIdx = 0;
  nsresult rv = col->GetIndex(&colIdx);
  if (NS_FAILED(rv))
    return;

#endif

  for (PRInt32 rowIdx = aRow; rowIdx < aRow - aCount; rowIdx++) {
#ifdef MOZ_ACCESSIBILITY_ATK
    for (PRInt32 colIdx = 0; colIdx < colsCount; ++colIdx) {
#else
    {
#endif

      void *key = reinterpret_cast<void*>(rowIdx * kMaxTreeColumns + colIdx);

      nsCOMPtr<nsIAccessNode> accessNode;
      GetCacheEntry(*mAccessNodeCache, key, getter_AddRefs(accessNode));

      if (accessNode) {
        nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(accessNode));
        nsCOMPtr<nsIAccessibleEvent> event =
          new nsAccEvent(nsIAccessibleEvent::EVENT_DOM_DESTROY,
                         accessible, PR_FALSE);
        FireAccessibleEvent(event);

        mAccessNodeCache->Remove(key);
      }
    }
  }

  PRInt32 newRowCount = 0;
  rv = mTreeView->GetRowCount(&newRowCount);
  if (NS_FAILED(rv))
    return;

  PRInt32 oldRowCount = newRowCount - aCount;

  for (PRInt32 rowIdx = newRowCount; rowIdx < oldRowCount; ++rowIdx) {
#ifdef MOZ_ACCESSIBILITY_ATK
    for (PRInt32 colIdx = 0; colIdx < colsCount; ++colIdx) {
#else
    {
#endif
      void *key = reinterpret_cast<void*>(rowIdx * kMaxTreeColumns + colIdx);
      mAccessNodeCache->Remove(key);
    }
  }
}

void
nsXULTreeAccessible::TreeViewInvalidated(PRInt32 aStartRow, PRInt32 aEndRow,
                                         PRInt32 aStartCol, PRInt32 aEndCol)
{
  if (IsDefunct())
    return;

  PRInt32 endRow = aEndRow;

  nsresult rv;
  if (endRow == -1) {
    PRInt32 rowCount = 0;
    rv = mTreeView->GetRowCount(&rowCount);
    if (NS_FAILED(rv))
      return;

    endRow = rowCount - 1;
  }

  nsCOMPtr<nsITreeColumns> treeColumns;
  mTree->GetColumns(getter_AddRefs(treeColumns));
  if (!treeColumns)
    return;

#ifdef MOZ_ACCESSIBILITY_ATK
  PRInt32 endCol = aEndCol;

  if (endCol == -1) {
    PRInt32 colCount = 0;
    rv = treeColumns->GetCount(&colCount);
    if (NS_FAILED(rv))
      return;

    endCol = colCount - 1;
  }
#else
  nsCOMPtr<nsITreeColumn> col;
  rv = treeColumns->GetKeyColumn(getter_AddRefs(col));
  if (!col)
    return;

  PRInt32 colIdx = 0;
  rv = col->GetIndex(&colIdx);
  if (NS_FAILED(rv))
    return;

#endif

  for (PRInt32 rowIdx = aStartRow; rowIdx <= endRow; ++rowIdx) {
#ifdef MOZ_ACCESSIBILITY_ATK
    for (PRInt32 colIdx = aStartCol; colIdx <= endCol; ++colIdx)
#endif
    {
      void *key = reinterpret_cast<void*>(rowIdx * kMaxTreeColumns + colIdx);

      nsCOMPtr<nsIAccessNode> accessNode;
      GetCacheEntry(*mAccessNodeCache, key, getter_AddRefs(accessNode));

      if (accessNode) {
        nsRefPtr<nsXULTreeitemAccessible> treeitemAcc(
          nsAccUtils::QueryAccessibleTreeitem(accessNode));
        NS_ASSERTION(treeitemAcc, "Wrong accessible at the given key!");

        nsAutoString name, cachedName;
        treeitemAcc->GetName(name);

        treeitemAcc->GetCachedName(cachedName);
        if (name != cachedName) {
          nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE,
                                   treeitemAcc);
          treeitemAcc->SetCachedName(name);
        }
      }
    }
  }
}

void
nsXULTreeAccessible::TreeViewChanged()
{
  if (IsDefunct())
    return;

  // Fire only notification destroy/create events on accessible tree to lie to
  // AT because it should be expensive to fire destroy events for each tree item
  // in cache.
  nsCOMPtr<nsIAccessibleEvent> eventDestroy =
    new nsAccEvent(nsIAccessibleEvent::EVENT_DOM_DESTROY,
                   this, PR_FALSE);
  if (!eventDestroy)
    return;

  FirePlatformEvent(eventDestroy);

  ClearCache(*mAccessNodeCache);

  mTree->GetView(getter_AddRefs(mTreeView));

  nsCOMPtr<nsIAccessibleEvent> eventCreate =
    new nsAccEvent(nsIAccessibleEvent::EVENT_DOM_CREATE,
                   this, PR_FALSE);
  if (!eventCreate)
    return;

  FirePlatformEvent(eventCreate);
}

nsresult nsXULTreeAccessible::GetColumnCount(nsITreeBoxObject* aBoxObject, PRInt32* aCount)
{
  NS_ENSURE_TRUE(aBoxObject, NS_ERROR_FAILURE);
  nsCOMPtr<nsITreeColumns> treeColumns;
  aBoxObject->GetColumns(getter_AddRefs(treeColumns));
  NS_ENSURE_TRUE(treeColumns, NS_ERROR_FAILURE);
  return treeColumns->GetCount(aCount);
}

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeitemAccessible

nsXULTreeitemAccessible::nsXULTreeitemAccessible(nsIAccessible *aParent, nsIDOMNode *aDOMNode, nsIWeakReference *aShell, PRInt32 aRow, nsITreeColumn* aColumn)
  : nsLeafAccessible(aDOMNode, aShell)
{
  mParent = aParent;  // xxx todo: do we need this? We already have mParent on nsAccessible

  nsXULTreeAccessible::GetTreeBoxObject(aDOMNode, getter_AddRefs(mTree));
  if (mTree)
    mTree->GetView(getter_AddRefs(mTreeView));
  NS_ASSERTION(mTree && mTreeView, "Can't get mTree or mTreeView!\n");

  // Since the real tree item does not correspond to any DOMNode, use the row index to distinguish each item
  mRow = aRow;
  mColumn = aColumn;

  if (!mColumn && mTree) {
    nsCOMPtr<nsITreeColumns> cols;
    mTree->GetColumns(getter_AddRefs(cols));
    if (cols)
      cols->GetKeyColumn(getter_AddRefs(mColumn));
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXULTreeitemAccessible,
                             nsLeafAccessible,
                             nsXULTreeitemAccessible)

// nsAccessNode
 
nsresult
nsXULTreeitemAccessible::Shutdown()
{
  mTree = nsnull;
  mTreeView = nsnull;
  mColumn = nsnull;
  return nsLeafAccessible::Shutdown();
}

// nsIAccessible

NS_IMETHODIMP
nsXULTreeitemAccessible::GetName(nsAString& aName)
{
  // XXX: we should take into account ARIA usage for content tree. 
  aName.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  mTreeView->GetCellText(mRow, mColumn, aName);
  
  // If there is still no name try the cell value:
  // This is for graphical cells. We need tree/table view implementors to implement
  // FooView::GetCellValue to return a meaningful string for cases where there is
  // something shown in the cell (non-text) such as a star icon; in which case
  // GetCellValue for that cell would return "starred" or "flagged" for example.
  if (aName.IsEmpty()) {
    mTreeView->GetCellValue(mRow, mColumn, aName);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetUniqueID(void **aUniqueID)
{
  // Since mDOMNode is same for all tree item, use |this| pointer as the unique Id
  *aUniqueID = static_cast<void*>(this);
  return NS_OK;
}

// nsAccessNode::Init()
nsresult
nsXULTreeitemAccessible::Init()
{
  nsresult rv = nsLeafAccessible::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return GetName(mCachedName);
}

nsresult
nsXULTreeitemAccessible::GetRoleInternal(PRUint32 *aRole)
{
  PRInt32 colCount = 0;
  if (NS_SUCCEEDED(nsXULTreeAccessible::GetColumnCount(mTree, &colCount)) && colCount > 1)
    *aRole = nsIAccessibleRole::ROLE_CELL;
  else
    *aRole = nsIAccessibleRole::ROLE_OUTLINEITEM;
  return NS_OK;
}

// Possible states: focused, focusable, selected, checkable, checked, 
// expanded/collapsed, invisible
nsresult
nsXULTreeitemAccessible::GetStateInternal(PRUint32 *aState,
                                          PRUint32 *aExtraState)
{
  NS_ENSURE_ARG_POINTER(aState);

  *aState = 0;
  if (aExtraState)
    *aExtraState = 0;

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;
    return NS_OK_DEFUNCT_OBJECT;
  }

  *aState = nsIAccessibleStates::STATE_FOCUSABLE |
            nsIAccessibleStates::STATE_SELECTABLE;

  // get expanded/collapsed state
  if (IsExpandable()) {
    PRBool isContainerOpen;
    mTreeView->IsContainerOpen(mRow, &isContainerOpen);
    *aState |= isContainerOpen? PRUint32(nsIAccessibleStates::STATE_EXPANDED):
                                PRUint32(nsIAccessibleStates::STATE_COLLAPSED);
  }

  // get selected state
  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    PRBool isSelected;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected)
      *aState |= nsIAccessibleStates::STATE_SELECTED;
  }

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
    do_QueryInterface(mDOMNode);
  if (multiSelect) {
    PRInt32 currentIndex;
    multiSelect->GetCurrentIndex(&currentIndex);
    if (currentIndex == mRow) {
      *aState |= nsIAccessibleStates::STATE_FOCUSED;
    }
  }

  PRInt32 firstVisibleRow, lastVisibleRow;
  mTree->GetFirstVisibleRow(&firstVisibleRow);
  mTree->GetLastVisibleRow(&lastVisibleRow);
  if (mRow < firstVisibleRow || mRow > lastVisibleRow)
    *aState |= nsIAccessibleStates::STATE_INVISIBLE;


  PRInt16 type;
  mColumn->GetType(&type);
  if (type == nsITreeColumn::TYPE_CHECKBOX) {
    *aState |= nsIAccessibleStates::STATE_CHECKABLE;
    nsAutoString checked;
    mTreeView->GetCellValue(mRow, mColumn, checked);
    if (checked.EqualsIgnoreCase("true")) {
      *aState |= nsIAccessibleStates::STATE_CHECKED;
    }
  }

  return NS_OK;
}

PRBool
nsXULTreeitemAccessible::IsDefunct()
{
  if (!mTree || !mTreeView || !mColumn || mRow < 0)
    return PR_TRUE;

  PRInt32 rowCount = 0;
  nsresult rv = mTreeView->GetRowCount(&rowCount);
  if (NS_FAILED(rv) || mRow >= rowCount)
    return PR_TRUE;

  // Call GetPresShell() since the accessible may be shut down in it.
  nsCOMPtr<nsIPresShell> presShell(GetPresShell());
  return !presShell;
}

PRBool nsXULTreeitemAccessible::IsExpandable()
{
  if (IsDefunct())
    return PR_FALSE;

  PRBool isContainer;
  mTreeView->IsContainer(mRow, &isContainer);
  if (isContainer) {
    PRBool isEmpty; 
    mTreeView->IsContainerEmpty(mRow, &isEmpty);
    if (!isEmpty) {
      PRBool isPrimary;
      mColumn->GetPrimary(&isPrimary);
      if (isPrimary) {
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

// "activate" (xor "cycle") action is available for all treeitems
// "expand/collapse" action is avaible for treeitem which is container
NS_IMETHODIMP nsXULTreeitemAccessible::GetNumActions(PRUint8 *aNumActions)
{
  NS_ENSURE_ARG_POINTER(aNumActions);
  *aNumActions = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aNumActions = IsExpandable() ? 2 : 1;
  return NS_OK;
}

// Return the name of our actions
NS_IMETHODIMP nsXULTreeitemAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (aIndex == eAction_Click) {
    PRBool isCycler;
    mColumn->GetCycler(&isCycler);
    if (isCycler) {
      aName.AssignLiteral("cycle");
    }
    else {
      aName.AssignLiteral("activate");
    }
    return NS_OK;
  }
  else if (aIndex == eAction_Expand && IsExpandable()) {
    PRBool isContainerOpen;
    mTreeView->IsContainerOpen(mRow, &isContainerOpen);
    if (isContainerOpen)
      aName.AssignLiteral("collapse");
    else
      aName.AssignLiteral("expand");
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

nsresult
nsXULTreeitemAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = nsLeafAccessible::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMXULTreeElement> tree(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(tree, NS_OK);

  nsCOMPtr<nsITreeView> view;
  tree->GetView(getter_AddRefs(view));
  NS_ENSURE_TRUE(view, NS_OK);

  PRInt32 level;
  rv = view->GetLevel(mRow, &level);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 topCount = 1;
  for (PRInt32 index = mRow - 1; index >= 0; index--) {
    PRInt32 lvl = -1;
    if (NS_SUCCEEDED(view->GetLevel(index, &lvl))) {
      if (lvl < level)
        break;

      if (lvl == level)
        topCount++;
    }
  }

  PRInt32 rowCount = 0;
  rv = view->GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 bottomCount = 0;
  for (PRInt32 index = mRow + 1; index < rowCount; index++) {
    PRInt32 lvl = -1;
    if (NS_SUCCEEDED(view->GetLevel(index, &lvl))) {
      if (lvl < level)
        break;

      if (lvl == level)
        bottomCount++;
    }
  }

  PRInt32 setSize = topCount + bottomCount;
  PRInt32 posInSet = topCount;

  // set the group attributes
  nsAccUtils::SetAccGroupAttrs(aAttributes, level + 1, posInSet, setSize);

  // set the "cycles" attribute
  PRBool isCycler;
  mColumn->GetCycler(&isCycler);
  if (isCycler) {
    nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::cycles,
                           NS_LITERAL_STRING("true"));
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::GetParent(nsIAccessible **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (mParent) {
    *aParent = mParent;
    NS_ADDREF(*aParent);
  }

  return NS_OK;
}

// Return the next row of tree if mColumn (if any),
// otherwise return the next cell.
NS_IMETHODIMP nsXULTreeitemAccessible::GetNextSibling(nsIAccessible **aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);
  *aNextSibling = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsRefPtr<nsXULTreeAccessible> treeCache =
    nsAccUtils::QueryAccessibleTree(mParent);
  NS_ENSURE_TRUE(treeCache, NS_ERROR_FAILURE);

  PRInt32 rowCount;
  mTreeView->GetRowCount(&rowCount);

  if (!mColumn) {
    if (mRow < rowCount - 1)
      treeCache->GetCachedTreeitemAccessible(mRow + 1, nsnull, aNextSibling);

    return NS_OK;
  }

  PRInt32 row = mRow;
  nsCOMPtr<nsITreeColumn> column;
#ifdef MOZ_ACCESSIBILITY_ATK
  column = nsXULTreeAccessible::GetNextVisibleColumn(mColumn);

  if (!column) {
    if (mRow < rowCount -1) {
      row++;
      column = nsXULTreeAccessible::GetFirstVisibleColumn(mTree);
    } else {
      // the next sibling of the last treeitem is null
      return NS_OK;
    }
  }
#else
  if (++row >= rowCount) {
    return NS_ERROR_FAILURE;
  }
#endif //MOZ_ACCESSIBILITY_ATK

  treeCache->GetCachedTreeitemAccessible(row, column, aNextSibling);
  return NS_OK;
}

// Return the previous row of tree if mColumn (if any),
// otherwise return the previous cell.
NS_IMETHODIMP nsXULTreeitemAccessible::GetPreviousSibling(nsIAccessible **aPreviousSibling)
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);
  *aPreviousSibling = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsRefPtr<nsXULTreeAccessible> treeCache =
    nsAccUtils::QueryAccessibleTree(mParent);
  NS_ENSURE_TRUE(treeCache, NS_ERROR_FAILURE);

  if (!mColumn && mRow > 0)
    treeCache->GetCachedTreeitemAccessible(mRow - 1, nsnull, aPreviousSibling);

  nsresult rv = NS_OK;


  PRInt32 row = mRow;
  nsCOMPtr<nsITreeColumn> column;
#ifdef MOZ_ACCESSIBILITY_ATK
  rv = mColumn->GetPrevious(getter_AddRefs(column));
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!column && mRow > 0) {
    row--;
    column = nsXULTreeAccessible::GetLastVisibleColumn(mTree);
  }
#else
  if (--row < 0) {
    return NS_ERROR_FAILURE;
  }
#endif

  treeCache->GetCachedTreeitemAccessible(row, column, aPreviousSibling);
  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::DoAction(PRUint8 index)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (index == eAction_Click) {
    nsresult rv = NS_OK;
    PRBool isCycler;
    mColumn->GetCycler(&isCycler);
    if (isCycler) {
      rv = mTreeView->CycleCell(mRow, mColumn);
    } 
    else {
      nsCOMPtr<nsITreeSelection> selection;
      mTreeView->GetSelection(getter_AddRefs(selection));
      if (selection) {
        rv = selection->Select(mRow);
        mTree->EnsureRowIsVisible(mRow);
      }
    }
    return rv;
  }
  else if (index == eAction_Expand && IsExpandable()) {
    return mTreeView->ToggleOpenState(mRow);
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsXULTreeitemAccessible::GetBounds(PRInt32 *aX, PRInt32 *aY,
                                   PRInt32 *aWidth, PRInt32 *aHeight)
{
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Get bounds for tree cell and add x and y of treechildren element to
  // x and y of the cell.

  nsCOMPtr<nsIBoxObject> boxObj = nsCoreUtils::GetTreeBodyBoxObject(mTree);
  NS_ENSURE_STATE(boxObj);

  nsresult rv = mTree->GetCoordsForCellItem(mRow, mColumn, EmptyCString(),
                                            aX, aY, aWidth, aHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 tcX = 0, tcY = 0;
  boxObj->GetScreenX(&tcX);
  boxObj->GetScreenY(&tcY);
  *aX += tcX;
  *aY += tcY;

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::SetSelected(PRBool aSelect)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection) {
    PRBool isSelected;
    selection->IsSelected(mRow, &isSelected);
    if (isSelected != aSelect)
      selection->ToggleSelect(mRow);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULTreeitemAccessible::TakeFocus()
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsITreeSelection> selection;
  mTreeView->GetSelection(getter_AddRefs(selection));
  if (selection)
    selection->SetCurrentIndex(mRow);

  // focus event will be fired here
  return nsAccessible::TakeFocus();
}

NS_IMETHODIMP
nsXULTreeitemAccessible::GetRelationByType(PRUint32 aRelationType,
                                           nsIAccessibleRelation **aRelation)
{
  NS_ENSURE_ARG_POINTER(aRelation);
  *aRelation = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (aRelationType == nsIAccessibleRelation::RELATION_NODE_CHILD_OF) {
    PRInt32 columnIndex;
    if (NS_SUCCEEDED(mColumn->GetIndex(&columnIndex)) && columnIndex == 0) {
      PRInt32 parentIndex;
      if (NS_SUCCEEDED(mTreeView->GetParentIndex(mRow, &parentIndex))) {
        if (parentIndex == -1)
          return nsRelUtils::AddTarget(aRelationType, aRelation, mParent);
  
        nsRefPtr<nsXULTreeAccessible> treeCache =
          nsAccUtils::QueryAccessibleTree(mParent);

        nsCOMPtr<nsIAccessible> accParent;
        treeCache->GetCachedTreeitemAccessible(parentIndex, mColumn,
                                               getter_AddRefs(accParent));

        return nsRelUtils::AddTarget(aRelationType, aRelation, accParent);
      }
    }

    return NS_OK;
  }

  return nsAccessible::GetRelationByType(aRelationType, aRelation);
}

// nsXULTreeitemAccessible
void
nsXULTreeitemAccessible::GetCachedName(nsAString &aName)
{
  aName = mCachedName;
}

void
nsXULTreeitemAccessible::SetCachedName(const nsAString &aName)
{
  mCachedName = aName;
}

////////////////////////////////////////////////////////////////////////////////
//  nsXULTreeColumnsAccessible
nsXULTreeColumnsAccessible::
  nsXULTreeColumnsAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
  nsXULColumnsAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP
nsXULTreeColumnsAccessible::GetNextSibling(nsIAccessible **aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);
  *aNextSibling = nsnull;

  nsCOMPtr<nsITreeBoxObject> tree;
  nsCOMPtr<nsITreeView> treeView;

  nsXULTreeAccessible::GetTreeBoxObject(mDOMNode, getter_AddRefs(tree));
  if (tree) {
    tree->GetView(getter_AddRefs(treeView));
    if (treeView) {
      PRInt32 rowCount;
      treeView->GetRowCount(&rowCount);
      if (rowCount > 0) {
        nsCOMPtr<nsITreeColumn> column =
          nsXULTreeAccessible::GetFirstVisibleColumn(tree);

        nsRefPtr<nsXULTreeAccessible> treeCache =
          nsAccUtils::QueryAccessibleTree(mParent);
        NS_ENSURE_TRUE(treeCache, NS_ERROR_FAILURE);

        treeCache->GetCachedTreeitemAccessible(0, column, aNextSibling);
      }
    }
  }

  return NS_OK;
}

