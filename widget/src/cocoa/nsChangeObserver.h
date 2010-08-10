/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Pinkerton
 *   Josh Aas <josh@mozilla.com> (decomtaminate)
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

#ifndef nsChangeObserver_h_
#define nsChangeObserver_h_

class nsIContent;
class nsIDocument;
class nsIAtom;

#define NS_DECL_CHANGEOBSERVER \
void ObserveAttributeChanged(nsIDocument *aDocument, nsIContent *aContent, nsIAtom *aAttribute); \
void ObserveContentRemoved(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer); \
void ObserveContentInserted(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer);

// Something that wants to be alerted to changes in attributes or changes in
// its corresponding content object.
//
// This interface is used by our menu code so we only have to have one
// nsIDocumentObserver.
//
// Any class that implements this interface must take care to unregister itself
// on deletion.
class nsChangeObserver
{
public:
  virtual void ObserveAttributeChanged(nsIDocument* aDocument,
                                       nsIContent* aContent,
                                       nsIAtom* aAttribute)=0;

  virtual void ObserveContentRemoved(nsIDocument* aDocument,
                                     nsIContent* aChild, 
                                     PRInt32 aIndexInContainer)=0;

  virtual void ObserveContentInserted(nsIDocument* aDocument,
                                      nsIContent* aChild, 
                                      PRInt32 aIndexInContainer)=0;
};

#endif // nsChangeObserver_h_
