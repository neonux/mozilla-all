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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef nsAccUtils_h_
#define nsAccUtils_h_

#include "nsIAccessible.h"
#include "nsIAccessNode.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessibleRole.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTable.h"
#include "nsARIAMap.h"

#include "nsIDOMNode.h"
#include "nsIPersistentProperties2.h"
#include "nsIContent.h"
#include "nsPoint.h"

class nsAccessNode;
class nsAccessible;
class nsHTMLTableAccessible;
class nsDocAccessible;
#ifdef MOZ_XUL
class nsXULTreeAccessible;
#endif

class nsAccUtils
{
public:
  /**
   * Returns value of attribute from the given attributes container.
   *
   * @param aAttributes - attributes container
   * @param aAttrName - the name of requested attribute
   * @param aAttrValue - value of attribute
   */
  static void GetAccAttr(nsIPersistentProperties *aAttributes,
                         nsIAtom *aAttrName,
                         nsAString& aAttrValue);

  /**
   * Set value of attribute for the given attributes container.
   *
   * @param aAttributes - attributes container
   * @param aAttrName - the name of requested attribute
   * @param aAttrValue - new value of attribute
   */
  static void SetAccAttr(nsIPersistentProperties *aAttributes,
                         nsIAtom *aAttrName,
                         const nsAString& aAttrValue);

  /**
   * Set group attributes ('level', 'setsize', 'posinset').
   */
  static void SetAccGroupAttrs(nsIPersistentProperties *aAttributes,
                               PRInt32 aLevel, PRInt32 aSetSize,
                               PRInt32 aPosInSet);

  /**
   * Get default value of the level for the given accessible.
   */
  static PRInt32 GetDefaultLevel(nsAccessible *aAcc);

  /**
   * Return ARIA level value or the default one if ARIA is missed for the
   * given accessible.
   */
  static PRInt32 GetARIAOrDefaultLevel(nsIAccessible *aAcc);

  /**
   * Compute position in group (posinset) and group size (setsize) for
   * nsIDOMXULSelectControlItemElement node.
   */
  static void GetPositionAndSizeForXULSelectControlItem(nsIDOMNode *aNode,
                                                        PRInt32 *aPosInSet,
                                                        PRInt32 *aSetSize);

  /**
   * Compute group position and group size (posinset and setsize) for
   * nsIDOMXULContainerItemElement node.
   */
  static void GetPositionAndSizeForXULContainerItem(nsIDOMNode *aNode,
                                                    PRInt32 *aPosInSet,
                                                    PRInt32 *aSetSize);

  /**
   * Compute group level for nsIDOMXULContainerItemElement node.
   */
  static PRInt32 GetLevelForXULContainerItem(nsIDOMNode *aNode);

  /**
   * Set container-foo live region attributes for the given node.
   *
   * @param aAttributes    where to store the attributes
   * @param aStartContent  node to start from
   * @param aTopContent    node to end at
   */
  static void SetLiveContainerAttributes(nsIPersistentProperties *aAttributes,
                                         nsIContent *aStartContent,
                                         nsIContent *aTopContent);

  /**
   * Any ARIA property of type boolean or NMTOKEN is undefined if the ARIA
   * property is not present, or is "" or "undefined". Do not call 
   * this method for properties of type string, decimal, IDREF or IDREFS.
   * 
   * Return PR_TRUE if the ARIA property is defined, otherwise PR_FALSE
   */
  static PRBool HasDefinedARIAToken(nsIContent *aContent, nsIAtom *aAtom);

  /**
   * Return true if the given DOM node contains accessible children.
   */
  static PRBool HasAccessibleChildren(nsIDOMNode *aNode);

  /**
    * If an ancestor in this document exists with the given role, return it
    * @param aDescendant Descendant to start search with
    * @param aRole Role to find matching ancestor for
    * @return The ancestor accessible with the given role, or nsnull if no match is found
    */
   static already_AddRefed<nsIAccessible>
     GetAncestorWithRole(nsIAccessible *aDescendant, PRUint32 aRole);

   /**
     * For an ARIA tree item , get the accessible that represents its conceptual parent.
     * This method will use the correct method for the given way the tree is constructed.
     * The conceptual parent is what the user sees as the parent, not the DOM or accessible parent.
     * @param aStartTreeItem  The tree item to get the parent for
     * @param aStartTreeItemContent  The content node for the tree item
     * @param The tree item's parent, or null if none
     */
   static void
     GetARIATreeItemParent(nsIAccessible *aStartTreeItem,
                           nsIContent *aStartTreeItemContent,
                           nsIAccessible **aTreeItemParent);

  /**
   * Return single or multi selectable container for the given item.
   *
   * @param  aAccessible  [in] the item accessible
   * @param  aState       [in] the state of the item accessible
   */
  static already_AddRefed<nsIAccessible>
    GetSelectableContainer(nsIAccessible *aAccessible, PRUint32 aState);

  /**
   * Return multi selectable container for the given item.
   */
  static already_AddRefed<nsIAccessible>
    GetMultiSelectableContainer(nsIDOMNode *aNode);

  /**
   * Return true if the DOM node of given accessible has aria-selected="true"
   * attribute.
   */
  static PRBool IsARIASelected(nsIAccessible *aAccessible);

  /**
   * Return text accessible containing focus point of the given selection.
   * Used for normal and misspelling selection changes processing.
   *
   * @param aSelection  [in] the given selection
   * @param aNode       [out, optional] the DOM node of text accessible
   * @return            text accessible
   */
  static already_AddRefed<nsIAccessibleText>
    GetTextAccessibleFromSelection(nsISelection *aSelection,
                                   nsIDOMNode **aNode = nsnull);

  /**
   * Converts the given coordinates to coordinates relative screen.
   *
   * @param aX               [in] the given x coord
   * @param aY               [in] the given y coord
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessNode      [in] the accessible if coordinates are given
   *                         relative it.
   * @param aCoords          [out] converted coordinates
   */
  static nsresult ConvertToScreenCoords(PRInt32 aX, PRInt32 aY,
                                        PRUint32 aCoordinateType,
                                        nsIAccessNode *aAccessNode,
                                        nsIntPoint *aCoords);

  /**
   * Converts the given coordinates relative screen to another coordinate
   * system.
   *
   * @param aX               [in, out] the given x coord
   * @param aY               [in, out] the given y coord
   * @param aCoordinateType  [in] specifies coordinates origin (refer to
   *                         nsIAccessibleCoordinateType)
   * @param aAccessNode      [in] the accessible if coordinates are given
   *                         relative it
   */
  static nsresult ConvertScreenCoordsTo(PRInt32 *aX, PRInt32 *aY,
                                        PRUint32 aCoordinateType,
                                        nsIAccessNode *aAccessNode);

  /**
   * Returns coordinates relative screen for the top level window.
   *
   * @param aAccessNode  the accessible hosted in the window
   */
  static nsIntPoint GetScreenCoordsForWindow(nsIAccessNode *aAccessNode);

  /**
   * Returns coordinates relative screen for the parent of the given accessible.
   *
   * @param aAccessNode  the accessible
   */
  static nsIntPoint GetScreenCoordsForParent(nsIAccessNode *aAccessNode);

  /**
   * Get the role map entry for a given DOM node. This will use the first
   * ARIA role if the role attribute provides a space delimited list of roles.
   * @param aNode  The DOM node to get the role map entry for
   * @return       A pointer to the role map entry for the ARIA role, or nsnull if none
   */
  static nsRoleMapEntry* GetRoleMapEntry(nsIDOMNode *aNode);

  /**
   * Return the role of the given accessible.
   */
  static PRUint32 Role(nsIAccessible *aAcc)
  {
    PRUint32 role = nsIAccessibleRole::ROLE_NOTHING;
    if (aAcc)
      aAcc->GetRole(&role);

    return role;
  }

  /**
   * Return the role from native markup of the given accessible.
   */
  static PRUint32 RoleInternal(nsIAccessible *aAcc);

  /**
   * Return the state for the given accessible.
   */
  static PRUint32 State(nsIAccessible *aAcc)
  {
    PRUint32 state = 0;
    if (aAcc)
      aAcc->GetState(&state, nsnull);

    return state;
  }

  /**
   * Return the extended state for the given accessible.
   */
  static PRUint32 ExtendedState(nsIAccessible *aAcc)
  {
    PRUint32 state = 0;
    PRUint32 extstate = 0;
    if (aAcc)
      aAcc->GetState(&state, &extstate);

    return extstate;
  }

  /**
   * Get the ARIA attribute characteristics for a given ARIA attribute.
   * 
   * @param aAtom  ARIA attribute
   * @return       A bitflag representing the attribute characteristics
   *               (see nsARIAMap.h for possible bit masks, prefixed "ARIA_")
   */
  static PRUint8 GetAttributeCharacteristics(nsIAtom* aAtom);

  /**
   * Get the 'live' or 'container-live' object attribute value from the given
   * ELiveAttrRule constant.
   *
   * @param  aRule   [in] rule constant (see ELiveAttrRule in nsAccMap.h)
   * @param  aValue  [out] object attribute value
   *
   * @return         true if object attribute should be exposed
   */
  static PRBool GetLiveAttrValue(PRUint32 aRule, nsAString& aValue);

  /**
   * Query DestinationType from the given SourceType.
   */
  template<class DestinationType, class SourceType> static inline
    already_AddRefed<DestinationType> QueryObject(SourceType *aObject)
  {
    DestinationType* object = nsnull;
    if (aObject)
      CallQueryInterface(aObject, &object);

    return object;
  }
  template<class DestinationType, class SourceType> static inline
    already_AddRefed<DestinationType> QueryObject(nsCOMPtr<SourceType>& aObject)
  {
    DestinationType* object = nsnull;
    if (aObject)
      CallQueryInterface(aObject, &object);

    return object;
  }
  template<class DestinationType, class SourceType> static inline
  already_AddRefed<DestinationType> QueryObject(nsRefPtr<SourceType>& aObject)
  {
    DestinationType* object = nsnull;
    if (aObject)
      CallQueryInterface(aObject.get(), &object);
    
    return object;
  }

#ifdef DEBUG_A11Y
  /**
   * Detect whether the given accessible object implements nsIAccessibleText,
   * when it is text or has text child node.
   */
  static PRBool IsTextInterfaceSupportCorrect(nsIAccessible *aAccessible);
#endif

  /**
   * Return true if the given accessible has text role.
   */
  static PRBool IsText(nsIAccessible *aAcc)
  {
    PRUint32 role = Role(aAcc);
    return role == nsIAccessibleRole::ROLE_TEXT_LEAF ||
           role == nsIAccessibleRole::ROLE_STATICTEXT;
  }

  /**
   * Return text length of the given accessible, return -1 on failure.
   */
  static PRInt32 TextLength(nsIAccessible *aAccessible);

  /**
   * Return true if the given accessible is embedded object.
   */
  static PRBool IsEmbeddedObject(nsIAccessible *aAcc)
  {
    PRUint32 role = Role(aAcc);
    return role != nsIAccessibleRole::ROLE_TEXT_LEAF &&
           role != nsIAccessibleRole::ROLE_WHITESPACE &&
           role != nsIAccessibleRole::ROLE_STATICTEXT;
  }

  /**
   * Return true if the given accessible hasn't children.
   */
  static inline PRBool IsLeaf(nsIAccessible *aAcc)
  {
    PRInt32 numChildren = 0;
    aAcc->GetChildCount(&numChildren);
    return numChildren == 0;
  }

  /**
   * Return true if the given accessible can't have children. Used when exposing
   * to platform accessibility APIs, should the children be pruned off?
   */
  static PRBool MustPrune(nsIAccessible *aAccessible);

  /**
   * Return true if the given node can be accessible and attached to
   * the document's accessible tree.
   */
  static PRBool IsNodeRelevant(nsIDOMNode *aNode);

  /**
   * Search hint enum constants. Used by GetHeaderCellsFor() method.
   */
  enum {
    // search for row header cells, left direction
    eRowHeaderCells,
    // search for column header cells, top direction
    eColumnHeaderCells
  };

  /**
   * Return an array of row or column header cells for the given cell.
   *
   * @param aTable                [in] table accessible
   * @param aCell                 [in] cell accessible within the given table to
   *                               get header cells
   * @param aRowOrColHeaderCells  [in] specifies whether column or row header
   *                               cells are returned (see enum constants
   *                               above)
   * @param aCells                [out] array of header cell accessibles
   */
  static nsresult GetHeaderCellsFor(nsIAccessibleTable *aTable,
                                    nsIAccessibleTableCell *aCell,
                                    PRInt32 aRowOrColHeaderCells,
                                    nsIArray **aCells);
};

#endif
