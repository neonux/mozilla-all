/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * The Initial Developer of the Original Code is IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal <aleventh@us.ibm.com>
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

#ifndef _nsARIAMap_H_
#define _nsARIAMap_H_

#include "ARIAStateMap.h"
#include "mozilla/a11y/Role.h"

class nsIAtom;
class nsIContent;

////////////////////////////////////////////////////////////////////////////////
// Value constants

/**
 * Used to define if role requires to expose nsIAccessibleValue.
 */
enum EValueRule
{
  /**
   * nsIAccessibleValue isn't exposed.
   */
  eNoValue,

  /**
   * nsIAccessibleValue is implemented, supports value, min and max from
   * aria-valuenow, aria-valuemin and aria-valuemax.
   */
  eHasValueMinMax
};


////////////////////////////////////////////////////////////////////////////////
// Action constants

/**
 * Used to define if the role requires to expose action.
 */
enum EActionRule
{
  eNoAction,
  eActivateAction,
  eClickAction,
  ePressAction,
  eCheckUncheckAction,
  eExpandAction,
  eJumpAction,
  eOpenCloseAction,
  eSelectAction,
  eSortAction,
  eSwitchAction
};


////////////////////////////////////////////////////////////////////////////////
// Live region constants

/**
 * Used to define if role exposes default value of aria-live attribute.
 */
enum ELiveAttrRule
{
  eNoLiveAttr,
  eOffLiveAttr,
  ePoliteLiveAttr
};


////////////////////////////////////////////////////////////////////////////////
// Role constants

/**
 * ARIA role overrides role from native markup.
 */
const bool kUseMapRole = true;

/**
 * ARIA role doesn't override the role from native markup.
 */
const bool kUseNativeRole = false;


////////////////////////////////////////////////////////////////////////////////
// ARIA attribute characteristic masks

/**
 * This mask indicates the attribute should not be exposed as an object
 * attribute via the catch-all logic in nsAccessible::GetAttributes.
 * This means it either isn't mean't to be exposed as an object attribute, or
 * that it should, but is already handled in other code.
 */
const PRUint8 ATTR_BYPASSOBJ  = 0x0001;

/**
 * This mask indicates the attribute is expected to have an NMTOKEN or bool value.
 * (See for example usage in nsAccessible::GetAttributes)
 */
const PRUint8 ATTR_VALTOKEN   = 0x0010;

/**
 * Small footprint storage of persistent aria attribute characteristics.
 */
struct nsAttributeCharacteristics
{
  nsIAtom** attributeName;
  const PRUint8 characteristics;
};


////////////////////////////////////////////////////////////////////////////////
// State map entry

/**
 * Used in nsRoleMapEntry.state if no nsIAccessibleStates are automatic for
 * a given role.
 */
#define kNoReqStates 0

enum EDefaultStateRule
{
  //eNoDefaultState,
  eUseFirstState
};

////////////////////////////////////////////////////////////////////////////////
// Role map entry

/**
 * For each ARIA role, this maps the nsIAccessible information.
 */
struct nsRoleMapEntry
{
  // ARIA role: string representation such as "button"
  const char *roleString;
  
  // Role mapping rule: maps to this nsIAccessibleRole
  mozilla::a11y::role role;
  
  // Role rule: whether to use mapped role or native semantics
  bool roleRule;
  
  // Value mapping rule: how to compute nsIAccessible value
  EValueRule valueRule;

  // Action mapping rule, how to expose nsIAccessible action
  EActionRule actionRule;

  // 'live' and 'container-live' object attributes mapping rule: how to expose
  // these object attributes if ARIA 'live' attribute is missed.
  ELiveAttrRule liveAttRule;

  // Automatic state mapping rule: always include in nsIAccessibleStates
  PRUint64 state;   // or kNoReqStates if no nsIAccessibleStates are automatic for this role.

  // ARIA properties supported for this role
  // (in other words, the aria-foo attribute to nsIAccessibleStates mapping rules)
  // Currently you cannot have unlimited mappings, because
  // a variable sized array would not allow the use of
  // C++'s struct initialization feature.
  mozilla::a11y::aria::EStateRule attributeMap1;
  mozilla::a11y::aria::EStateRule attributeMap2;
  mozilla::a11y::aria::EStateRule attributeMap3;
};


////////////////////////////////////////////////////////////////////////////////
// ARIA map

/**
 *  These are currently initialized (hardcoded) in nsARIAMap.cpp, 
 *  and provide the mappings for WAI-ARIA roles and properties using the 
 *  structs defined in this file.
 */
struct nsARIAMap
{
  /**
   * Array of supported ARIA role map entries and its length.
   */
  static nsRoleMapEntry gWAIRoleMap[];
  static PRUint32 gWAIRoleMapLength;

  /**
   * Landmark role map entry. Used when specified ARIA role isn't mapped to
   * accessibility API.
   */
  static nsRoleMapEntry gLandmarkRoleMap;

  /**
   * Empty role map entry. Used by accessibility service to create an accessible
   * if the accessible can't use role of used accessible class. For example,
   * it is used for table cells that aren't contained by table.
   */
  static nsRoleMapEntry gEmptyRoleMap;

  /**
   * State map of ARIA states applied to any accessible not depending on
   * the role.
   */
  static mozilla::a11y::aria::EStateRule gWAIUnivStateMap[];

  /**
   * Map of attribute to attribute characteristics.
   */
  static nsAttributeCharacteristics gWAIUnivAttrMap[];
  static PRUint32 gWAIUnivAttrMapLength;

  /**
   * Return accessible state from ARIA universal states applied to the given
   * element.
   */
  static PRUint64 UniversalStatesFor(mozilla::dom::Element* aElement)
  {
    PRUint64 state = 0;
    PRUint32 index = 0;
    while (mozilla::a11y::aria::MapToState(gWAIUnivStateMap[index],
                                           aElement, &state))
      index++;

    return state;
  }
};

#endif
