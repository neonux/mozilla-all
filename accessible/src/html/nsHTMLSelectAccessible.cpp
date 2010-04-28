/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsHTMLSelectAccessible.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsEventShell.h"
#include "nsIAccessibleEvent.h"
#include "nsTextEquivUtils.h"

#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIListControlFrame.h"
#include "nsIServiceManager.h"
#include "nsIMutableArray.h"

/**
  * Selects, Listboxes and Comboboxes, are made up of a number of different
  *  widgets, some of which are shared between the two. This file contains 
  *  all of the widgets for both of the Selects, for HTML only.
  *
  *  Listbox:
  *     - nsHTMLSelectListAccessible
  *       - nsHTMLSelectOptionAccessible
  *
  *  Comboboxes:
  *     - nsHTMLComboboxAccessible
  *        - nsHTMLComboboxListAccessible        [ inserted in accessible tree ]
  *           - nsHTMLSelectOptionAccessible(s)
  */


/** ------------------------------------------------------ */
/**  Impl. of nsHTMLSelectableAccessible                   */
/** ------------------------------------------------------ */

// Helper class
nsHTMLSelectableAccessible::iterator::iterator(nsHTMLSelectableAccessible *aParent, nsIWeakReference *aWeakShell): 
  mWeakShell(aWeakShell), mParentSelect(aParent)
{
  mLength = mIndex = 0;
  mSelCount = 0;

  nsCOMPtr<nsIDOMHTMLSelectElement> htmlSelect(do_QueryInterface(mParentSelect->mDOMNode));
  if (htmlSelect) {
    htmlSelect->GetOptions(getter_AddRefs(mOptions));
    if (mOptions)
      mOptions->GetLength(&mLength);
  }
}

PRBool nsHTMLSelectableAccessible::iterator::Advance() 
{
  if (mIndex < mLength) {
    nsCOMPtr<nsIDOMNode> tempNode;
    if (mOptions) {
      mOptions->Item(mIndex, getter_AddRefs(tempNode));
      mOption = do_QueryInterface(tempNode);
    }
    mIndex++;
    return PR_TRUE;
  }
  return PR_FALSE;
}

void nsHTMLSelectableAccessible::iterator::CalcSelectionCount(PRInt32 *aSelectionCount)
{
  PRBool isSelected = PR_FALSE;

  if (mOption)
    mOption->GetSelected(&isSelected);

  if (isSelected)
    (*aSelectionCount)++;
}

void
nsHTMLSelectableAccessible::iterator::AddAccessibleIfSelected(nsIMutableArray *aSelectedAccessibles, 
                                                              nsPresContext *aContext)
{
  PRBool isSelected = PR_FALSE;
  nsRefPtr<nsAccessible> tempAcc;

  if (mOption) {
    mOption->GetSelected(&isSelected);
    if (isSelected) {
      nsCOMPtr<nsIDOMNode> optionNode(do_QueryInterface(mOption));
      tempAcc = GetAccService()->GetAccessibleInWeakShell(optionNode,
                                                          mWeakShell);
    }
  }

  if (tempAcc)
    aSelectedAccessibles->AppendElement(static_cast<nsIAccessible*>(tempAcc),
                                        PR_FALSE);
}

PRBool
nsHTMLSelectableAccessible::iterator::GetAccessibleIfSelected(PRInt32 aIndex,
                                                              nsPresContext *aContext, 
                                                              nsIAccessible **aAccessible)
{
  PRBool isSelected = PR_FALSE;

  *aAccessible = nsnull;

  if (mOption) {
    mOption->GetSelected(&isSelected);
    if (isSelected) {
      if (mSelCount == aIndex) {
        nsCOMPtr<nsIDOMNode> optionNode(do_QueryInterface(mOption));
        nsRefPtr<nsAccessible> acc =
          GetAccService()->GetAccessibleInWeakShell(optionNode, mWeakShell);
        if (acc)
          CallQueryInterface(acc, aAccessible);

        return PR_TRUE;
      }
      mSelCount++;
    }
  }

  return PR_FALSE;
}

void nsHTMLSelectableAccessible::iterator::Select(PRBool aSelect)
{
  if (mOption)
    mOption->SetSelected(aSelect);
}

nsHTMLSelectableAccessible::nsHTMLSelectableAccessible(nsIDOMNode* aDOMNode, 
                                                       nsIWeakReference* aShell):
nsAccessibleWrap(aDOMNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLSelectableAccessible, nsAccessible, nsIAccessibleSelectable)

// Helper methods
NS_IMETHODIMP nsHTMLSelectableAccessible::ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState)
{
  *aSelState = PR_FALSE;

  nsCOMPtr<nsIDOMHTMLSelectElement> htmlSelect(do_QueryInterface(mDOMNode));
  if (!htmlSelect)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
  htmlSelect->GetOptions(getter_AddRefs(options));
  if (!options)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> tempNode;
  options->Item(aIndex, getter_AddRefs(tempNode));
  nsCOMPtr<nsIDOMHTMLOptionElement> tempOption(do_QueryInterface(tempNode));
  if (!tempOption)
    return NS_ERROR_FAILURE;

  tempOption->GetSelected(aSelState);
  nsresult rv = NS_OK;
  if (eSelection_Add == aMethod && !(*aSelState))
    rv = tempOption->SetSelected(PR_TRUE);
  else if (eSelection_Remove == aMethod && (*aSelState))
    rv = tempOption->SetSelected(PR_FALSE);
  return rv;
}

// Interface methods
NS_IMETHODIMP nsHTMLSelectableAccessible::GetSelectedChildren(nsIArray **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIMutableArray> selectedAccessibles =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_STATE(selectedAccessibles);
  
  nsPresContext *context = GetPresContext();
  if (!context)
    return NS_ERROR_FAILURE;

  nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
  while (iter.Advance())
    iter.AddAccessibleIfSelected(selectedAccessibles, context);

  PRUint32 uLength = 0;
  selectedAccessibles->GetLength(&uLength); 
  if (uLength != 0) { // length of nsIArray containing selected options
    *_retval = selectedAccessibles;
    NS_ADDREF(*_retval);
  }
  return NS_OK;
}

// return the nth selected child's nsIAccessible object
NS_IMETHODIMP nsHTMLSelectableAccessible::RefSelection(PRInt32 aIndex, nsIAccessible **_retval)
{
  *_retval = nsnull;

  nsPresContext *context = GetPresContext();
  if (!context)
    return NS_ERROR_FAILURE;

  nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
  while (iter.Advance())
    if (iter.GetAccessibleIfSelected(aIndex, context, _retval))
      return NS_OK;
  
  // No matched item found
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLSelectableAccessible::GetSelectionCount(PRInt32 *aSelectionCount)
{
  *aSelectionCount = 0;

  nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
  while (iter.Advance())
    iter.CalcSelectionCount(aSelectionCount);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectableAccessible::AddChildToSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Add, &isSelected);
}

NS_IMETHODIMP nsHTMLSelectableAccessible::RemoveChildFromSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Remove, &isSelected);
}

NS_IMETHODIMP nsHTMLSelectableAccessible::IsChildSelected(PRInt32 aIndex, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return ChangeSelection(aIndex, eSelection_GetState, _retval);
}

NS_IMETHODIMP nsHTMLSelectableAccessible::ClearSelection()
{
  nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
  while (iter.Advance())
    iter.Select(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectableAccessible::SelectAllSelection(PRBool *_retval)
{
  *_retval = PR_FALSE;
  
  nsCOMPtr<nsIDOMHTMLSelectElement> htmlSelect(do_QueryInterface(mDOMNode));
  if (!htmlSelect)
    return NS_ERROR_FAILURE;

  htmlSelect->GetMultiple(_retval);
  if (*_retval) {
    nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
    while (iter.Advance())
      iter.Select(PR_TRUE);
  }
  return NS_OK;
}

/** ------------------------------------------------------ */
/**  First, the common widgets                             */
/** ------------------------------------------------------ */

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible
////////////////////////////////////////////////////////////////////////////////

/** Default Constructor */
nsHTMLSelectListAccessible::nsHTMLSelectListAccessible(nsIDOMNode* aDOMNode, 
                                                       nsIWeakReference* aShell)
:nsHTMLSelectableAccessible(aDOMNode, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible: nsAccessible public

nsresult
nsHTMLSelectListAccessible::GetStateInternal(PRUint32 *aState,
                                             PRUint32 *aExtraState)
{
  nsresult rv = nsHTMLSelectableAccessible::GetStateInternal(aState,
                                                             aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  // As a nsHTMLSelectListAccessible we can have the following states:
  //   nsIAccessibleStates::STATE_MULTISELECTABLE
  //   nsIAccessibleStates::STATE_EXTSELECTABLE

  nsCOMPtr<nsIDOMHTMLSelectElement> select (do_QueryInterface(mDOMNode));
  if (select) {
    if (*aState & nsIAccessibleStates::STATE_FOCUSED) {
      // Treat first focusable option node as actual focus, in order
      // to avoid confusing JAWS, which needs focus on the option
      nsCOMPtr<nsIDOMNode> focusedOption;
      nsHTMLSelectOptionAccessible::GetFocusedOptionNode(mDOMNode, 
                                                         getter_AddRefs(focusedOption));
      if (focusedOption) { // Clear focused state since it is on option
        *aState &= ~nsIAccessibleStates::STATE_FOCUSED;
      }
    }
    PRBool multiple;
    select->GetMultiple(&multiple);
    if ( multiple )
      *aState |= nsIAccessibleStates::STATE_MULTISELECTABLE |
                 nsIAccessibleStates::STATE_EXTSELECTABLE;
  }

  return NS_OK;
}

nsresult
nsHTMLSelectListAccessible::GetRoleInternal(PRUint32 *aRole)
{
  if (nsAccUtils::Role(mParent) == nsIAccessibleRole::ROLE_COMBOBOX)
    *aRole = nsIAccessibleRole::ROLE_COMBOBOX_LIST;
  else
    *aRole = nsIAccessibleRole::ROLE_LISTBOX;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible: nsAccessible protected

void
nsHTMLSelectListAccessible::CacheChildren()
{
  // Cache accessibles for <optgroup> and <option> DOM decendents as children,
  // as well as the accessibles for them. Avoid whitespace text nodes. We want
  // to count all the <optgroup>s and <option>s as children because we want
  // a flat tree under the Select List.
  
  nsCOMPtr<nsIContent> selectContent(do_QueryInterface(mDOMNode));
  CacheOptSiblings(selectContent);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible protected

void
nsHTMLSelectListAccessible::CacheOptSiblings(nsIContent *aParentContent)
{
  PRUint32 numChildren = aParentContent->GetChildCount();
  for (PRUint32 count = 0; count < numChildren; count ++) {
    nsIContent *childContent = aParentContent->GetChildAt(count);
    if (!childContent->IsHTML()) {
      continue;
    }

    nsCOMPtr<nsIAtom> tag = childContent->Tag();
    if (tag == nsAccessibilityAtoms::option ||
        tag == nsAccessibilityAtoms::optgroup) {

      // Get an accessible for option or optgroup and cache it.
      nsCOMPtr<nsIDOMNode> childNode(do_QueryInterface(childContent));

      nsRefPtr<nsAccessible> acc =
        GetAccService()->GetAccessibleInWeakShell(childNode, mWeakShell);
      if (acc) {
        mChildren.AppendElement(acc);
        acc->SetParent(this);
      }

      // Deep down into optgroup element.
      if (tag == nsAccessibilityAtoms::optgroup)
        CacheOptSiblings(childContent);
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLSelectOptionAccessible::
  nsHTMLSelectOptionAccessible(nsIDOMNode *aDOMNode, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aDOMNode, aShell)
{
  nsCOMPtr<nsIDOMNode> parentNode;
  aDOMNode->GetParentNode(getter_AddRefs(parentNode));

  if (!parentNode)
    return;

  // If the parent node is a Combobox, then the option's accessible parent
  // is nsHTMLComboboxListAccessible, not the nsHTMLComboboxAccessible that
  // GetParent would normally return. This is because the 
  // nsHTMLComboboxListAccessible is inserted into the accessible hierarchy
  // where there is no DOM node for it.
  nsRefPtr<nsAccessible> parentAcc =
    GetAccService()->GetAccessibleInWeakShell(parentNode, mWeakShell);
  if (!parentAcc)
    return;

  if (nsAccUtils::RoleInternal(parentAcc) == nsIAccessibleRole::ROLE_COMBOBOX) {
    PRInt32 childCount = parentAcc->GetChildCount();
    parentAcc = parentAcc->GetChildAt(childCount - 1);
  }

  SetParent(parentAcc);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: nsAccessible public

nsresult
nsHTMLSelectOptionAccessible::GetRoleInternal(PRUint32 *aRole)
{
  if (nsAccUtils::Role(mParent) == nsIAccessibleRole::ROLE_COMBOBOX_LIST)
    *aRole = nsIAccessibleRole::ROLE_COMBOBOX_OPTION;
  else
    *aRole = nsIAccessibleRole::ROLE_OPTION;

  return NS_OK;
}

nsresult
nsHTMLSelectOptionAccessible::GetNameInternal(nsAString& aName)
{
  // CASE #1 -- great majority of the cases
  // find the label attribute - this is what the W3C says we should use
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::label, aName);
  if (!aName.IsEmpty())
    return NS_OK;
  
  // CASE #2 -- no label parameter, get the first child, 
  // use it if it is a text node
  nsCOMPtr<nsIContent> text = content->GetChildAt(0);
  if (!text)
    return NS_OK;

  if (text->IsNodeOfType(nsINode::eTEXT)) {
    nsAutoString txtValue;
    nsresult rv = nsTextEquivUtils::
      AppendTextEquivFromTextContent(text, &txtValue);
    NS_ENSURE_SUCCESS(rv, rv);

    // Temp var (txtValue) needed until CompressWhitespace built for nsAString
    txtValue.CompressWhitespace();
    aName.Assign(txtValue);
    return NS_OK;
  }

  return NS_OK;
}

nsIFrame* nsHTMLSelectOptionAccessible::GetBoundsFrame()
{
  PRUint32 state;
  nsCOMPtr<nsIContent> content = GetSelectState(&state);
  if (state & nsIAccessibleStates::STATE_COLLAPSED) {
    if (content) {
      return content->GetPrimaryFrame();
    }

    return nsnull;
  }

  return nsAccessible::GetBoundsFrame();
}

/**
  * As a nsHTMLSelectOptionAccessible we can have the following states:
  *     STATE_SELECTABLE
  *     STATE_SELECTED
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_OFFSCREEN
  */
nsresult
nsHTMLSelectOptionAccessible::GetStateInternal(PRUint32 *aState,
                                               PRUint32 *aExtraState)
{
  // Upcall to nsAccessible, but skip nsHyperTextAccessible impl
  // because we don't want EXT_STATE_EDITABLE or EXT_STATE_SELECTABLE_TEXT
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  PRUint32 selectState, selectExtState;
  nsCOMPtr<nsIContent> selectContent = GetSelectState(&selectState,
                                                      &selectExtState);
  if (selectState & nsIAccessibleStates::STATE_INVISIBLE) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> selectNode = do_QueryInterface(selectContent); 
  NS_ENSURE_TRUE(selectNode, NS_ERROR_FAILURE);

  // Is disabled?
  if (0 == (*aState & nsIAccessibleStates::STATE_UNAVAILABLE)) {
    *aState |= (nsIAccessibleStates::STATE_FOCUSABLE |
                nsIAccessibleStates::STATE_SELECTABLE);
    // When the list is focused but no option is actually focused,
    // Firefox draws a focus ring around the first non-disabled option.
    // We need to indicated STATE_FOCUSED in that case, because it
    // prevents JAWS from ignoring the list
    // GetFocusedOptionNode() ensures that an option node is 
    // returned in this case, as long as some focusable option exists
    // in the listbox
    nsCOMPtr<nsIDOMNode> focusedOptionNode;
    GetFocusedOptionNode(selectNode, getter_AddRefs(focusedOptionNode));
    if (focusedOptionNode == mDOMNode) {
      *aState |= nsIAccessibleStates::STATE_FOCUSED;
    }
  }

  // Are we selected?
  PRBool isSelected = PR_FALSE;
  nsCOMPtr<nsIDOMHTMLOptionElement> option (do_QueryInterface(mDOMNode));
  if (option) {
    option->GetSelected(&isSelected);
    if ( isSelected ) 
      *aState |= nsIAccessibleStates::STATE_SELECTED;
  }

  if (selectState & nsIAccessibleStates::STATE_OFFSCREEN) {
    *aState |= nsIAccessibleStates::STATE_OFFSCREEN;
  }
  else if (selectState & nsIAccessibleStates::STATE_COLLAPSED) {
    // <select> is COLLAPSED: add STATE_OFFSCREEN, if not the currently
    // visible option
    if (!isSelected) {
      *aState |= nsIAccessibleStates::STATE_OFFSCREEN;
    }
    else {
      // Clear offscreen and invisible for currently showing option
      *aState &= ~nsIAccessibleStates::STATE_OFFSCREEN;
      *aState &= ~nsIAccessibleStates::STATE_INVISIBLE;
       if (aExtraState) {
         *aExtraState |= selectExtState & nsIAccessibleStates::EXT_STATE_OPAQUE;
       }
    }
  }
  else {
    // XXX list frames are weird, don't rely on nsAccessible's general
    // visibility implementation unless they get reimplemented in layout
    *aState &= ~nsIAccessibleStates::STATE_OFFSCREEN;
    // <select> is not collapsed: compare bounds to calculate STATE_OFFSCREEN
    nsAccessible* listAcc = GetParent();
    if (listAcc) {
      PRInt32 optionX, optionY, optionWidth, optionHeight;
      PRInt32 listX, listY, listWidth, listHeight;
      GetBounds(&optionX, &optionY, &optionWidth, &optionHeight);
      listAcc->GetBounds(&listX, &listY, &listWidth, &listHeight);
      if (optionY < listY || optionY + optionHeight > listY + listHeight) {
        *aState |= nsIAccessibleStates::STATE_OFFSCREEN;
      }
    }
  }
 
  return NS_OK;
}

PRInt32
nsHTMLSelectOptionAccessible::GetLevelInternal()
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsIContent *parentContent = content->GetParent();

  PRInt32 level =
    parentContent->NodeInfo()->Equals(nsAccessibilityAtoms::optgroup) ? 2 : 1;

  if (level == 1 &&
      nsAccUtils::Role(this) != nsIAccessibleRole::ROLE_HEADING) {
    level = 0; // In a single level list, the level is irrelevant
  }

  return level;
}

void
nsHTMLSelectOptionAccessible::GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                                         PRInt32 *aSetSize)
{  
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsIContent *parentContent = content->GetParent();

  PRInt32 posInSet = 0, setSize = 0;
  PRBool isContentFound = PR_FALSE;

  PRUint32 childCount = parentContent->GetChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsIContent *childContent = parentContent->GetChildAt(childIdx);
    if (childContent->NodeInfo()->Equals(content->NodeInfo())) {
      if (!isContentFound) {
        if (childContent == content)
          isContentFound = PR_TRUE;

        posInSet++;
      }
      setSize++;
    }
  }

  *aSetSize = setSize;
  *aPosInSet = posInSet;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: nsIAccessible

/** select us! close combo box if necessary*/
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Select) {
    aName.AssignLiteral("select"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectOptionAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Select) {   // default action
    nsCOMPtr<nsIDOMHTMLOptionElement> newHTMLOption(do_QueryInterface(mDOMNode));
    if (!newHTMLOption) 
      return NS_ERROR_FAILURE;
    // Clear old selection
    nsCOMPtr<nsIDOMNode> oldHTMLOptionNode, selectNode;
    nsAccessible* parent = GetParent();
    NS_ASSERTION(parent, "No parent!");

    parent->GetDOMNode(getter_AddRefs(selectNode));
    GetFocusedOptionNode(selectNode, getter_AddRefs(oldHTMLOptionNode));
    nsCOMPtr<nsIDOMHTMLOptionElement> oldHTMLOption(do_QueryInterface(oldHTMLOptionNode));
    if (oldHTMLOption)
      oldHTMLOption->SetSelected(PR_FALSE);
    // Set new selection
    newHTMLOption->SetSelected(PR_TRUE);

    // If combo box, and open, close it
    // First, get the <select> widgets list control frame
    nsCOMPtr<nsIDOMNode> testSelectNode;
    nsCOMPtr<nsIDOMNode> thisNode(do_QueryInterface(mDOMNode));
    do {
      thisNode->GetParentNode(getter_AddRefs(testSelectNode));
      nsCOMPtr<nsIDOMHTMLSelectElement> selectControl(do_QueryInterface(testSelectNode));
      if (selectControl)
        break;
      thisNode = testSelectNode;
    } while (testSelectNode);

    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
    nsCOMPtr<nsIContent> selectContent(do_QueryInterface(testSelectNode));
    nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(mDOMNode));

    if (!testSelectNode || !selectContent || !presShell || !option) 
      return NS_ERROR_FAILURE;

    nsIFrame *selectFrame = selectContent->GetPrimaryFrame();
    nsIComboboxControlFrame *comboBoxFrame = do_QueryFrame(selectFrame);
    if (comboBoxFrame) {
      nsIFrame *listFrame = comboBoxFrame->GetDropDown();
      if (comboBoxFrame->IsDroppedDown() && listFrame) {
        // use this list control frame to roll up the list
        nsIListControlFrame *listControlFrame = do_QueryFrame(listFrame);
        if (listControlFrame) {
          PRInt32 newIndex = 0;
          option->GetIndex(&newIndex);
          listControlFrame->ComboboxFinish(newIndex);
        }
      }
    }
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: static methods

/**
  * Helper method for getting the focused DOM Node from our parent(list) node. We
  *  need to use the frame to get the focused option because for some reason we
  *  weren't getting the proper notification when the focus changed using the DOM
  */
nsresult nsHTMLSelectOptionAccessible::GetFocusedOptionNode(nsIDOMNode *aListNode, 
                                                            nsIDOMNode **aFocusedOptionNode)
{
  *aFocusedOptionNode = nsnull;
  NS_ASSERTION(aListNode, "Called GetFocusedOptionNode without a valid list node");

  nsCOMPtr<nsIContent> content(do_QueryInterface(aListNode));
  nsIFrame *frame = content->GetPrimaryFrame();
  if (!frame)
    return NS_ERROR_FAILURE;

  PRInt32 focusedOptionIndex = 0;

  // Get options
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(aListNode));
  NS_ASSERTION(selectElement, "No select element where it should be");

  nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
  nsresult rv = selectElement->GetOptions(getter_AddRefs(options));
  
  if (NS_SUCCEEDED(rv)) {
    nsIListControlFrame *listFrame = do_QueryFrame(frame);
    if (listFrame) {
      // Get what's focused in listbox by asking frame for "selected item". 
      // Can't use dom interface for this, because it will always return the first selected item
      // when there is more than 1 item selected. We need the focused item, not
      // the first selected item.
      focusedOptionIndex = listFrame->GetSelectedIndex();
      if (focusedOptionIndex == -1) {
        nsCOMPtr<nsIDOMNode> nextOption;
        while (PR_TRUE) {
          ++ focusedOptionIndex;
          options->Item(focusedOptionIndex, getter_AddRefs(nextOption));
          nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = do_QueryInterface(nextOption);
          if (!optionElement) {
            break;
          }
          PRBool disabled;
          optionElement->GetDisabled(&disabled);
          if (!disabled) {
            break;
          }
        }
      }
    }
    else  // Combo boxes can only have 1 selected option, so they can use the dom interface for this
      rv = selectElement->GetSelectedIndex(&focusedOptionIndex);
  }

  // Either use options and focused index, or default return null
  if (NS_SUCCEEDED(rv) && options && focusedOptionIndex >= 0) {  // Something is focused
    rv = options->Item(focusedOptionIndex, aFocusedOptionNode);
  }

  return rv;
}

void nsHTMLSelectOptionAccessible::SelectionChangedIfOption(nsIContent *aPossibleOption)
{
  if (!aPossibleOption || aPossibleOption->Tag() != nsAccessibilityAtoms::option ||
      !aPossibleOption->IsHTML()) {
    return;
  }

  nsCOMPtr<nsIDOMNode> optionNode(do_QueryInterface(aPossibleOption));
  NS_ASSERTION(optionNode, "No option node for nsIContent with option tag!");

  nsCOMPtr<nsIAccessible> multiSelect =
    nsAccUtils::GetMultiSelectableContainer(optionNode);
  if (!multiSelect)
    return;

  nsCOMPtr<nsIAccessible> optionAccessible;
  GetAccService()->GetAccessibleFor(optionNode,
                                    getter_AddRefs(optionAccessible));
  if (!optionAccessible)
    return;

  nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_SELECTION_WITHIN,
                          multiSelect);

  PRUint32 state = nsAccUtils::State(optionAccessible);
  PRUint32 eventType;
  if (state & nsIAccessibleStates::STATE_SELECTED) {
    eventType = nsIAccessibleEvent::EVENT_SELECTION_ADD;
  }
  else {
    eventType = nsIAccessibleEvent::EVENT_SELECTION_REMOVE;
  }

  nsEventShell::FireEvent(eventType, optionAccessible);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: private methods

nsIContent* nsHTMLSelectOptionAccessible::GetSelectState(PRUint32* aState,
                                                         PRUint32* aExtraState)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  while (content && content->Tag() != nsAccessibilityAtoms::select) {
    content = content->GetParent();
  }

  nsCOMPtr<nsIDOMNode> selectNode(do_QueryInterface(content));
  if (selectNode) {
    nsCOMPtr<nsIAccessible> selAcc;
    GetAccService()->GetAccessibleFor(selectNode, getter_AddRefs(selAcc));
    if (selAcc) {
      selAcc->GetState(aState, aExtraState);
      return content;
    }
  }
  return nsnull; 
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptGroupAccessible
////////////////////////////////////////////////////////////////////////////////

/** Default Constructor */
nsHTMLSelectOptGroupAccessible::nsHTMLSelectOptGroupAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsHTMLSelectOptionAccessible(aDOMNode, aShell)
{
}

nsresult
nsHTMLSelectOptGroupAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_HEADING;
  return NS_OK;
}

nsresult
nsHTMLSelectOptGroupAccessible::GetStateInternal(PRUint32 *aState,
                                                 PRUint32 *aExtraState)
{
  nsresult rv = nsHTMLSelectOptionAccessible::GetStateInternal(aState,
                                                               aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState &= ~(nsIAccessibleStates::STATE_FOCUSABLE |
               nsIAccessibleStates::STATE_SELECTABLE);

  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::DoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetNumActions(PRUint8 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptGroupAccessible: nsAccessible protected

void
nsHTMLSelectOptGroupAccessible::CacheChildren()
{
  // XXX To do (bug 378612) - create text child for the anonymous attribute
  // content, so that nsIAccessibleText is supported for the <optgroup> as it is
  // for an <option>. Attribute content is what layout creates for
  // the label="foo" on the <optgroup>. See eStyleContentType_Attr and
  // CreateAttributeContent() in nsCSSFrameConstructor
}

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

////////////////////////////////////////////////////////////////////////////////
// nsHTMLComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLComboboxAccessible::nsHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsAccessibleWrap(aDOMNode, aShell)
{
}

/** We are a combobox */
nsresult
nsHTMLComboboxAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_COMBOBOX;
  return NS_OK;
}

void
nsHTMLComboboxAccessible::CacheChildren()
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return;

  nsIComboboxControlFrame *comboFrame = do_QueryFrame(frame);
  if (!comboFrame)
    return;

  nsIFrame *listFrame = comboFrame->GetDropDown();
  if (!listFrame)
    return;

  if (!mListAccessible) {
    mListAccessible = 
      new nsHTMLComboboxListAccessible(mParent, mDOMNode, mWeakShell);
    if (!mListAccessible)
      return;

    mListAccessible->Init();
  }

  mChildren.AppendElement(mListAccessible);
  mListAccessible->SetParent(this);
}

nsresult
nsHTMLComboboxAccessible::Shutdown()
{
  nsAccessibleWrap::Shutdown();

  if (mListAccessible) {
    mListAccessible->Shutdown();
    mListAccessible = nsnull;
  }
  return NS_OK;
}

/**
  * As a nsHTMLComboboxAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_HASPOPUP
  *     STATE_EXPANDED
  *     STATE_COLLAPSED
  */
nsresult
nsHTMLComboboxAccessible::GetStateInternal(PRUint32 *aState,
                                           PRUint32 *aExtraState)
{
  // Get focus status from base class
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsIFrame *frame = GetBoundsFrame();
  nsIComboboxControlFrame *comboFrame = do_QueryFrame(frame);
  if (comboFrame && comboFrame->IsDroppedDown()) {
    *aState |= nsIAccessibleStates::STATE_EXPANDED;
  }
  else {
    *aState &= ~nsIAccessibleStates::STATE_FOCUSED; // Focus is on an option
    *aState |= nsIAccessibleStates::STATE_COLLAPSED;
  }

  *aState |= nsIAccessibleStates::STATE_HASPOPUP |
             nsIAccessibleStates::STATE_FOCUSABLE;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLComboboxAccessible::GetDescription(nsAString& aDescription)
{
  aDescription.Truncate();
  // First check to see if combo box itself has a description, perhaps through
  // tooltip (title attribute) or via aria-describedby
  nsAccessible::GetDescription(aDescription);
  if (!aDescription.IsEmpty()) {
    return NS_OK;
  }
  // Use description of currently focused option
  nsRefPtr<nsAccessible> optionAcc = GetFocusedOptionAccessible();
  return optionAcc ? optionAcc->GetDescription(aDescription) : NS_OK;
}

already_AddRefed<nsAccessible>
nsHTMLComboboxAccessible::GetFocusedOptionAccessible()
{
  if (IsDefunct())
    return nsnull;

  nsCOMPtr<nsIDOMNode> focusedOptionNode;
  nsHTMLSelectOptionAccessible::
    GetFocusedOptionNode(mDOMNode, getter_AddRefs(focusedOptionNode));
  if (!focusedOptionNode) {
    return nsnull;
  }

  return GetAccService()->GetAccessibleInWeakShell(focusedOptionNode,
                                                   mWeakShell);
}

/**
  * MSAA/ATK accessible value != HTML value, especially not in combo boxes.
  * Our accessible value is the text label for of our ( first ) selected child.
  * The easiest way to get this is from the first child which is the readonly textfield.
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetValue(nsAString& aValue)
{
  // Use accessible name of currently focused option.
  nsRefPtr<nsAccessible> optionAcc = GetFocusedOptionAccessible();
  return optionAcc ? optionAcc->GetName(aValue) : NS_OK;
}

/** Just one action ( click ). */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetNumActions(PRUint8 *aNumActions)
{
  *aNumActions = 1;
  return NS_OK;
}

/**
  * Programmaticaly toggle the combo box
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != nsHTMLComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }
  nsIFrame *frame = GetFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }
  nsIComboboxControlFrame *comboFrame = do_QueryFrame(frame);
  if (!comboFrame) {
    return NS_ERROR_FAILURE;
  }
  // Reverse whether it's dropped down or not
  comboFrame->ShowDropDown(!comboFrame->IsDroppedDown());

  return NS_OK;
}

/**
  * Our action name is the reverse of our state: 
  *     if we are closed -> open is our name.
  *     if we are open -> closed is our name.
  * Uses the frame to get the state, updated on every click
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != nsHTMLComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }
  nsIFrame *frame = GetFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }
  nsIComboboxControlFrame *comboFrame = do_QueryFrame(frame);
  if (!comboFrame) {
    return NS_ERROR_FAILURE;
  }
  if (comboFrame->IsDroppedDown())
    aName.AssignLiteral("close"); 
  else
    aName.AssignLiteral("open"); 

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLComboboxListAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLComboboxListAccessible::nsHTMLComboboxListAccessible(nsIAccessible *aParent,
                                                           nsIDOMNode* aDOMNode,
                                                           nsIWeakReference* aShell):
nsHTMLSelectListAccessible(aDOMNode, aShell)
{
}

nsIFrame*
nsHTMLComboboxListAccessible::GetFrame()
{
  nsIFrame* frame = nsHTMLSelectListAccessible::GetFrame();

  if (frame) {
    nsIComboboxControlFrame* comboBox = do_QueryFrame(frame);
    if (comboBox) {
      return comboBox->GetDropDown();
    }
  }

  return nsnull;
}

/**
  * As a nsHTMLComboboxListAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_INVISIBLE
  *     STATE_FLOATING
  */
nsresult
nsHTMLComboboxListAccessible::GetStateInternal(PRUint32 *aState,
                                               PRUint32 *aExtraState)
{
  // Get focus status from base class
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame = do_QueryFrame(boundsFrame);
  if (comboFrame && comboFrame->IsDroppedDown())
    *aState |= nsIAccessibleStates::STATE_FLOATING;
  else
    *aState |= nsIAccessibleStates::STATE_INVISIBLE;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLComboboxListAccessible::GetUniqueID(void **aUniqueID)
{
  // Since mDOMNode is same for all tree item, use |this| pointer as the unique Id
  *aUniqueID = static_cast<void*>(this);
  return NS_OK;
}

/**
  * Gets the bounds for the areaFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void nsHTMLComboboxListAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
  *aBoundingFrame = nsnull;

  nsAccessible* comboAcc = GetParent();
  if (!comboAcc)
    return;

  if (0 == (nsAccUtils::State(comboAcc) & nsIAccessibleStates::STATE_COLLAPSED)) {
    nsHTMLSelectListAccessible::GetBoundsRect(aBounds, aBoundingFrame);
    return;
  }
   // get our first option
  nsCOMPtr<nsIDOMNode> child;
  mDOMNode->GetFirstChild(getter_AddRefs(child));

  // now get its frame
  nsCOMPtr<nsIContent> content(do_QueryInterface(child));
  if (!content) {
    return;
  }
  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame) {
    *aBoundingFrame = nsnull;
    return;
  }

  *aBoundingFrame = frame->GetParent();
  aBounds = (*aBoundingFrame)->GetRect();
}

// nsHTMLComboboxListAccessible. nsAccessible public mehtod
nsAccessible*
nsHTMLComboboxListAccessible::GetParent()
{
  return mParent;
}
