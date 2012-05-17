/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

#ifndef mozilla_dom_indexeddb_checkquotahelper_h__
#define mozilla_dom_indexeddb_checkquotahelper_h__

// Only meant to be included in IndexedDB source files, not exported.
#include "IndexedDatabase.h"
#include "IDBDatabase.h"

#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"

#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"

class nsPIDOMWindow;

BEGIN_INDEXEDDB_NAMESPACE

class CheckQuotaHelper MOZ_FINAL : public nsIRunnable,
                                   public nsIInterfaceRequestor,
                                   public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIOBSERVER

  CheckQuotaHelper(nsPIDOMWindow* aWindow,
                   mozilla::Mutex& aMutex);

  bool PromptAndReturnQuotaIsDisabled();

  void Cancel();

private:
  nsPIDOMWindow* mWindow;

  nsCString mASCIIOrigin;
  mozilla::Mutex& mMutex;
  mozilla::CondVar mCondVar;
  PRUint32 mPromptResult;
  bool mWaiting;
  bool mHasPrompted;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_checkquotahelper_h__