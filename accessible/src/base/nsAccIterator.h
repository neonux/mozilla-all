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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef nsAccIterator_h_
#define nsAccIterator_h_

#include "nsAccessible.h"
#include "nsAccUtils.h"

/**
 * Return true if the traversed accessible complies with filter.
 */
typedef PRBool (*AccIteratorFilterFuncPtr) (nsAccessible *);

/**
 * Allows to iterate through accessible children or subtree complying with
 * filter function.
 */
class nsAccIterator
{
public:
  /**
   * Used to define iteration type.
   */
  enum IterationType {
    /**
     * Navigation happens through direct children.
     */
    eFlatNav,

    /**
     * Navigation through subtree excluding iterator root; if the accessible
     * complies with filter, iterator ignores its children.
     */
    eTreeNav
  };

  nsAccIterator(nsAccessible *aRoot, AccIteratorFilterFuncPtr aFilterFunc,
                IterationType aIterationType = eFlatNav);
  ~nsAccIterator();

  /**
   * Return next accessible complying with filter function. Return the first
   * accessible for the first time.
   */
  nsAccessible *GetNext();

  /**
   * Predefined filters.
   */
  static PRBool GetSelected(nsAccessible *aAccessible)
  {
    return nsAccUtils::State(aAccessible) & nsIAccessibleStates::STATE_SELECTED;
  }
  static PRBool GetSelectable(nsAccessible *aAccessible)
  {
    return nsAccUtils::State(aAccessible) & nsIAccessibleStates::STATE_SELECTABLE;
  }
  static PRBool GetRow(nsAccessible *aAccessible)
  {
    return nsAccUtils::Role(aAccessible) == nsIAccessibleRole::ROLE_ROW;
  }
  static PRBool GetCell(nsAccessible *aAccessible)
  {
    PRUint32 role = nsAccUtils::Role(aAccessible);
    return role == nsIAccessibleRole::ROLE_GRID_CELL ||
           role == nsIAccessibleRole::ROLE_ROWHEADER ||
           role == nsIAccessibleRole::ROLE_COLUMNHEADER;
  }

private:
  nsAccIterator();
  nsAccIterator(const nsAccIterator&);
  nsAccIterator& operator =(const nsAccIterator&);

  struct IteratorState
  {
    IteratorState(nsAccessible *aParent, IteratorState *mParentState = nsnull);

    nsAccessible *mParent;
    PRInt32 mIndex;
    IteratorState *mParentState;
  };

  AccIteratorFilterFuncPtr mFilterFunc;
  PRBool mIsDeep;
  IteratorState *mState;
};

#endif
