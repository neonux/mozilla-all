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

/*
 * interface representing a collection of style data attached to a
 * document, which may be or be combined into a style rule processor
 */

#ifndef nsIStyleSheet_h___
#define nsIStyleSheet_h___

#include <stdio.h>
#include "nsISupports.h"

class nsString;
class nsIURI;
class nsIDocument;
template<class T> struct already_AddRefed;

// IID for the nsIStyleSheet interface
// 0304a68a-3ab2-413e-9979-a256b3416caa
#define NS_ISTYLE_SHEET_IID     \
{ 0x0304a68a, 0x3ab2, 0x413e,   \
 { 0x99, 0x79, 0xa2, 0x56, 0xb3, 0x41, 0x6c, 0xaa } }

/**
 * A style sheet is a thing associated with a document that has style
 * rules.  Those style rules can be reached in one of two ways, depending
 * on which level of the nsStyleSet it is in:
 *   1) It can be |QueryInterface|d to nsIStyleRuleProcessor
 *   2) It can be |QueryInterface|d to nsCSSStyleSheet, with which the
 *      |nsStyleSet| uses an |nsCSSRuleProcessor| to access the rules.
 */
class nsIStyleSheet : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISTYLE_SHEET_IID)

  // basic style sheet data
  virtual already_AddRefed<nsIURI> GetSheetURI() const = 0;
  virtual already_AddRefed<nsIURI> GetBaseURI() const = 0;
  virtual void GetTitle(nsString& aTitle) const = 0;
  virtual void GetType(nsString& aType) const = 0;
  virtual PRBool HasRules() const = 0;

  /**
   * Whether the sheet is applicable.  A sheet that is not applicable
   * should never be inserted into a style set.  A sheet may not be
   * applicable for a variety of reasons including being disabled and
   * being incomplete.
   *
   */
  virtual PRBool GetApplicable() const = 0;

  /**
   * Set the stylesheet to be enabled.  This may or may not make it
   * applicable.  Note that this WILL inform the sheet's document of
   * its new applicable state if the state changes but WILL NOT call
   * BeginUpdate() or EndUpdate() on the document -- calling those is
   * the caller's responsibility.  This allows use of SetEnabled when
   * batched updates are desired.  If you want updates handled for
   * you, see nsIDOMStyleSheet::SetDisabled().
   */
  virtual void SetEnabled(PRBool aEnabled) = 0;

  /**
   * Whether the sheet is complete.
   */
  virtual PRBool GetComplete() const = 0;
  virtual void SetComplete() = 0;

  // style sheet owner info
  virtual already_AddRefed<nsIStyleSheet> GetParentSheet() const = 0;  // may be null
  virtual already_AddRefed<nsIDocument> GetOwningDocument() const = 0; // may be null
  virtual void SetOwningDocument(nsIDocument* aDocument) = 0;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
#endif
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStyleSheet, NS_ISTYLE_SHEET_IID)

#endif /* nsIStyleSheet_h___ */
