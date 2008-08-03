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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
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

#include "nsMenuUtilsX.h"
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemX.h"
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"
#include "nsCocoaWindow.h"
#include "nsWidgetAtoms.h"

#import <Carbon/Carbon.h>

nsEventStatus nsMenuUtilsX::DispatchCommandTo(nsIContent* aTargetContent)
{
  NS_PRECONDITION(aTargetContent, "null ptr");

  nsEventStatus status = nsEventStatus_eConsumeNoDefault;
  nsXULCommandEvent event(PR_TRUE, NS_XUL_COMMAND, nsnull);

  // FIXME: Should probably figure out how to init this with the actual
  // pressed keys, but this is a big old edge case anyway. -dwh

  aTargetContent->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  return status;
}


NSString* nsMenuUtilsX::CreateTruncatedCocoaLabel(const nsString& itemLabel)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // ::TruncateThemeText() doesn't take the number of characters to truncate to, it takes a pixel with
  // to fit the string in. Ugh. I talked it over with sfraser and we couldn't come up with an 
  // easy way to compute what this should be given the system font, etc, so we're just going
  // to hard code it to something reasonable and bigger fonts will just have to deal.
  const short kMaxItemPixelWidth = 300;
  NSMutableString *label = [[NSMutableString stringWithCharacters:itemLabel.get() length:itemLabel.Length()] retain];
  ::TruncateThemeText((CFMutableStringRef)label, kThemeMenuItemFont, kThemeStateActive, kMaxItemPixelWidth, truncMiddle, NULL);
  return label; // caller releases

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}


PRUint8 nsMenuUtilsX::GeckoModifiersForNodeAttribute(const nsString& modifiersAttribute)
{
  PRUint8 modifiers = knsMenuItemNoModifier;
  char* str = ToNewCString(modifiersAttribute);
  char* newStr;
  char* token = strtok_r(str, ", \t", &newStr);
  while (token != NULL) {
    if (strcmp(token, "shift") == 0)
      modifiers |= knsMenuItemShiftModifier;
    else if (strcmp(token, "alt") == 0) 
      modifiers |= knsMenuItemAltModifier;
    else if (strcmp(token, "control") == 0) 
      modifiers |= knsMenuItemControlModifier;
    else if ((strcmp(token, "accel") == 0) ||
             (strcmp(token, "meta") == 0)) {
      modifiers |= knsMenuItemCommandModifier;
    }
    token = strtok_r(newStr, ", \t", &newStr);
  }
  free(str);

  return modifiers;
}


unsigned int nsMenuUtilsX::MacModifiersForGeckoModifiers(PRUint8 geckoModifiers)
{
  unsigned int macModifiers = 0;
  
  if (geckoModifiers & knsMenuItemShiftModifier)
    macModifiers |= NSShiftKeyMask;
  if (geckoModifiers & knsMenuItemAltModifier)
    macModifiers |= NSAlternateKeyMask;
  if (geckoModifiers & knsMenuItemControlModifier)
    macModifiers |= NSControlKeyMask;
  if (geckoModifiers & knsMenuItemCommandModifier)
    macModifiers |= NSCommandKeyMask;

  return macModifiers;
}


nsMenuBarX* nsMenuUtilsX::GetHiddenWindowMenuBar()
{
  nsIWidget* hiddenWindowWidgetNoCOMPtr = nsCocoaUtils::GetHiddenWindowWidget();
  if (hiddenWindowWidgetNoCOMPtr)
    return static_cast<nsCocoaWindow*>(hiddenWindowWidgetNoCOMPtr)->GetMenuBar();
  else
    return nsnull;
}


// It would be nice if we could localize these edit menu names.
static NSMenuItem* standardEditMenuItem = nil;
NSMenuItem* nsMenuUtilsX::GetStandardEditMenuItem()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (standardEditMenuItem)
    return standardEditMenuItem;

  standardEditMenuItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
  NSMenu* standardEditMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
  [standardEditMenuItem setSubmenu:standardEditMenu];
  [standardEditMenu release];

  // Add Undo
  NSMenuItem* undoItem = [[NSMenuItem alloc] initWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
  [standardEditMenu addItem:undoItem];
  [undoItem release];

  // Add Redo
  NSMenuItem* redoItem = [[NSMenuItem alloc] initWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
  [standardEditMenu addItem:redoItem];
  [redoItem release];

  // Add separator
  [standardEditMenu addItem:[NSMenuItem separatorItem]];

  // Add Cut
  NSMenuItem* cutItem = [[NSMenuItem alloc] initWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
  [standardEditMenu addItem:cutItem];
  [cutItem release];

  // Add Copy
  NSMenuItem* copyItem = [[NSMenuItem alloc] initWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
  [standardEditMenu addItem:copyItem];
  [copyItem release];

  // Add Paste
  NSMenuItem* pasteItem = [[NSMenuItem alloc] initWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
  [standardEditMenu addItem:pasteItem];
  [pasteItem release];

  // Add Delete
  NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:@"Delete" action:@selector(delete:) keyEquivalent:@""];
  [standardEditMenu addItem:deleteItem];
  [deleteItem release];

  // Add Select All
  NSMenuItem* selectAllItem = [[NSMenuItem alloc] initWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
  [standardEditMenu addItem:selectAllItem];
  [selectAllItem release];

  return standardEditMenuItem;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}


PRBool nsMenuUtilsX::NodeIsHiddenOrCollapsed(nsIContent* inContent)
{
  return (inContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::hidden,
                                 nsWidgetAtoms::_true, eCaseMatters) ||
          inContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::collapsed,
                                 nsWidgetAtoms::_true, eCaseMatters));
}


// Determines how many items are visible among the siblings in a menu that are
// before the given child. Note that this will not count the application menu.
nsresult nsMenuUtilsX::CountVisibleBefore(nsMenuObjectX* aParentMenu, nsMenuObjectX* aChild, PRUint32* outVisibleBefore)
{
  NS_ASSERTION(outVisibleBefore, "bad index param in nsMenuX::CountVisibleBefore");

  nsMenuObjectTypeX parentType = aParentMenu->MenuObjectType();
  if (parentType == eMenuBarObjectType) {
    *outVisibleBefore = 0;
    nsMenuBarX* menubarParent = static_cast<nsMenuBarX*>(aParentMenu);
    PRUint32 numMenus = menubarParent->GetMenuCount();
    for (PRUint32 i = 0; i < numMenus; i++) {
      nsMenuX* currMenu = menubarParent->GetMenuAt(i);
      if (currMenu == aChild)
        return NS_OK; // we found ourselves, break out
      if (currMenu) {
        nsIContent* menuContent = currMenu->Content();
        if (menuContent->GetChildCount() > 0 &&
            !nsMenuUtilsX::NodeIsHiddenOrCollapsed(menuContent)) {
          ++(*outVisibleBefore);
        }
      }
    }
  }
  else if (parentType == eSubmenuObjectType) {
    *outVisibleBefore = 0;
    nsMenuX* menuParent = static_cast<nsMenuX*>(aParentMenu);
    PRUint32 numItems = menuParent->GetItemCount();
    for (PRUint32 i = 0; i < numItems; i++) {
      // Using GetItemAt instead of GetVisibleItemAt to avoid O(N^2)
      nsMenuObjectX* currItem = menuParent->GetItemAt(i);
      if (currItem == aChild)
        return NS_OK; // we found ourselves, break out
      if (!nsMenuUtilsX::NodeIsHiddenOrCollapsed(currItem->Content()))
        ++(*outVisibleBefore);
    }
  }
  return NS_ERROR_FAILURE;
}
