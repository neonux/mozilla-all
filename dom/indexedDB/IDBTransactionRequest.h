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

#ifndef mozilla_dom_indexeddb_idbtransactionrequest_h__
#define mozilla_dom_indexeddb_idbtransactionrequest_h__

#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "mozilla/dom/indexedDB/IDBDatabaseRequest.h"

#include "nsIIDBTransactionRequest.h"
#include "nsIRunnable.h"

#include "nsDOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"

#include "nsAutoPtr.h"
#include "mozilla/Storage.h"

class nsIThread;

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class CommitHelper;
class ObjectStoreInfo;
class TransactionThreadPool;

class IDBTransactionRequest : public nsDOMEventTargetHelper,
                              public IDBRequest::Generator,
                              public nsIIDBTransactionRequest
{
  friend class AsyncConnectionHelper;
  friend class CommitHelper;
  friend class TransactionThreadPool;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIIDBTRANSACTION
  NS_DECL_NSIIDBTRANSACTIONREQUEST

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBTransactionRequest,
                                           nsDOMEventTargetHelper)

  static already_AddRefed<IDBTransactionRequest>
  Create(IDBDatabaseRequest* aDatabase,
         nsTArray<nsString>& aObjectStoreNames,
         PRUint16 aMode,
         PRUint32 aTimeout);

  void OnNewRequest();
  void OnRequestFinished();

  bool StartSavepoint();
  nsresult ReleaseSavepoint();
  void RollbackSavepoint();

  already_AddRefed<mozIStorageStatement> AddStatement(bool aCreate,
                                                      bool aOverwrite,
                                                      bool aAutoIncrement);

  already_AddRefed<mozIStorageStatement> RemoveStatement(bool aAutoIncrement);

  already_AddRefed<mozIStorageStatement> GetStatement(bool aAutoIncrement);

#ifdef DEBUG
  bool TransactionIsOpen() const;
  bool IsWriteAllowed() const;
#else
  bool TransactionIsOpen() const
  {
    return mReadyState == nsIIDBTransaction::INITIAL ||
           mReadyState == nsIIDBTransaction::LOADING;
  }

  bool IsWriteAllowed() const
  {
    return mMode == nsIIDBTransaction::READ_WRITE;
  }
#endif

private:
  IDBTransactionRequest();
  ~IDBTransactionRequest();

  // Only meant to be called on mStorageThread!
  nsresult GetOrCreateConnection(mozIStorageConnection** aConnection);

  void CloseConnection();

  nsresult CommitOrRollback();

  nsRefPtr<IDBDatabaseRequest> mDatabase;
  nsTArray<nsString> mObjectStoreNames;
  PRUint16 mReadyState;
  PRUint16 mMode;
  PRUint32 mTimeout;
  PRUint32 mPendingRequests;

  // Only touched on the main thread.
  nsRefPtr<nsDOMEventListenerWrapper> mOnCompleteListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnAbortListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnTimeoutListener;

  // Only touched on the database thread.
  nsCOMPtr<mozIStorageConnection> mConnection;
  nsCOMPtr<mozIStorageStatement> mAddStmt;
  nsCOMPtr<mozIStorageStatement> mAddAutoIncrementStmt;
  nsCOMPtr<mozIStorageStatement> mModifyStmt;
  nsCOMPtr<mozIStorageStatement> mModifyAutoIncrementStmt;
  nsCOMPtr<mozIStorageStatement> mAddOrModifyStmt;
  nsCOMPtr<mozIStorageStatement> mAddOrModifyAutoIncrementStmt;
  nsCOMPtr<mozIStorageStatement> mRemoveStmt;
  nsCOMPtr<mozIStorageStatement> mRemoveAutoIncrementStmt;
  nsCOMPtr<mozIStorageStatement> mGetStmt;
  nsCOMPtr<mozIStorageStatement> mGetAutoIncrementStmt;

  // Only touched on the database thread.
  PRUint32 mSavepointCount;

  bool mHasInitialSavepoint;
  bool mAborted;
};

NS_STACK_CLASS
class AutoTransactionRequestNotifier
{
public:
  AutoTransactionRequestNotifier(IDBTransactionRequest* aTransaction)
  : mTransaction(aTransaction)
  {
    NS_ASSERTION(mTransaction, "Null pointer!");
    mTransaction->OnNewRequest();
  }

  ~AutoTransactionRequestNotifier()
  {
    mTransaction->OnRequestFinished();
  }

private:
  nsRefPtr<IDBTransactionRequest> mTransaction;
};

class CloseConnectionRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  template<class T>
  bool AddDoomedObject(nsCOMPtr<T>& aCOMPtr)
  {
    if (aCOMPtr) {
      if (!mDoomedObjects.AppendElement(do_QueryInterface(aCOMPtr))) {
        NS_ERROR("Out of memory!");
        return false;
      }
      aCOMPtr = nsnull;
    }
    return true;
  }

private:
  nsAutoTArray<nsCOMPtr<nsISupports>, 10> mDoomedObjects;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbtransactionrequest_h__
