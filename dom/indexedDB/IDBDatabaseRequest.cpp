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

#include "IDBDatabaseRequest.h"

#include "mozilla/Storage.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsDOMLists.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBRequest.h"
#include "LazyIdleThread.h"

USING_INDEXEDDB_NAMESPACE

namespace {

const PRUint32 kDefaultDatabaseTimeoutMS = 5000;
const PRUint32 kDefaultThreadTimeoutMS = 30000;

inline
nsISupports*
isupports_cast(IDBDatabaseRequest* aClassPtr)
{
  return static_cast<nsISupports*>(
    static_cast<IDBRequest::Generator*>(aClassPtr));
}

class CloseConnectionRunnable : public nsRunnable
{
public:
  CloseConnectionRunnable(nsCOMPtr<mozIStorageConnection>& aConnection)
  : mConnection(aConnection)
  { }

  NS_IMETHOD Run()
  {
    if (mConnection) {
      if (NS_FAILED(mConnection->Close())) {
        NS_WARNING("Failed to close connection!");
      }
      mConnection = nsnull;
    }
    return NS_OK;
  }

private:
  nsCOMPtr<mozIStorageConnection>& mConnection;
};

class CreateObjectStoreHelper : public AsyncConnectionHelper
{
public:
  CreateObjectStoreHelper(const nsACString& aASCIIOrigin,
                          nsCOMPtr<mozIStorageConnection>& aConnection,
                          nsIDOMEventTarget* aTarget,
                          const nsAString& aName,
                          const nsAString& aKeyPath,
                          bool aAutoIncrement)
  : AsyncConnectionHelper(aASCIIOrigin, aConnection, aTarget), mName(aName),
    mKeyPath(aKeyPath), mAutoIncrement(aAutoIncrement), mId(LL_MININT)
  { }

  PRUint16 DoDatabaseWork();
  void GetSuccessResult(nsIWritableVariant* aResult);

protected:
  nsString mName;
  nsString mKeyPath;
  bool mAutoIncrement;
  PRInt64 mId;
};

} // anonymous namespace

// static
already_AddRefed<nsIIDBDatabaseRequest>
IDBDatabaseRequest::Create(const nsAString& aName,
                           const nsAString& aDescription,
                           PRBool aReadOnly)
{
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = nsContentUtils::GetSecurityManager()->
    GetSubjectPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCString origin;
  if (nsContentUtils::IsSystemPrincipal(principal)) {
    origin.AssignLiteral("chrome");
  }
  else {
    rv = nsContentUtils::GetASCIIOrigin(principal, origin);
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  nsRefPtr<IDBDatabaseRequest> db(new IDBDatabaseRequest());

  db->mStorageThread = new LazyIdleThread(kDefaultThreadTimeoutMS);

  // Circular reference! Will be broken when the thread times out, when XPCOM is
  // shutting down, or if we explicitly call Shutdown on the thread.
  db->mStorageThread->SetIdleObserver(db);

  db->mASCIIOrigin.Assign(origin);
  db->mName.Assign(aName);
  db->mDescription.Assign(aDescription);
  db->mReadOnly = aReadOnly;

  db->mObjectStores = new nsDOMStringList();
  db->mIndexes = new nsDOMStringList();

  nsIIDBDatabaseRequest* result;
  db.forget(&result);
  return result;
}

IDBDatabaseRequest::IDBDatabaseRequest()
: mReadOnly(PR_FALSE)
{
  
}

IDBDatabaseRequest::~IDBDatabaseRequest()
{
  if (mStorageThread) {
    mStorageThread->Shutdown();
  }
}

NS_IMPL_ADDREF(IDBDatabaseRequest)
NS_IMPL_RELEASE(IDBDatabaseRequest)

NS_INTERFACE_MAP_BEGIN(IDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabase)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBDatabaseRequest)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBDatabaseRequest, IDBDatabaseRequest)

NS_IMETHODIMP
IDBDatabaseRequest::GetName(nsAString& aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetDescription(nsAString& aDescription)
{
  aDescription.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetVersion(nsAString& aVersion)
{
  aVersion.Assign(mVersion);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetObjectStores(nsIDOMDOMStringList** aObjectStores)
{
  nsCOMPtr<nsIDOMDOMStringList> objectStores(mObjectStores);
  objectStores.forget(aObjectStores);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetIndexes(nsIDOMDOMStringList** aIndexes)
{
  nsCOMPtr<nsIDOMDOMStringList> indexes(mIndexes);
  indexes.forget(aIndexes);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetCurrentTransaction(nsIIDBTransaction** aTransaction)
{
  nsCOMPtr<nsIIDBTransaction> transaction(mCurrentTransaction);
  transaction.forget(aTransaction);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::CreateObjectStore(const nsAString& aName,
                                      const nsAString& aKeyPath,
                                      PRBool aAutoIncrement,
                                      nsIIDBRequest** _retval)
{
  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<CreateObjectStoreHelper> helper =
    new CreateObjectStoreHelper(mASCIIOrigin, mStorage, request, aName,
                                aKeyPath, !!aAutoIncrement);
  nsresult rv = helper->Dispatch(mStorageThread);
  NS_ENSURE_SUCCESS(rv, rv);

  IDBRequest* retval;
  request.forget(&retval);
  *_retval = retval;
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenObjectStore(const nsAString& aName,
                                    PRUint16 aMode,
                                    nsIIDBRequest** _retval)
{
  nsRefPtr<IDBRequest> request = GenerateRequest();
  /*
  nsresult rv = mDatabase->OpenObjectStore(aName, aMode, request);
  NS_ENSURE_SUCCESS(rv, rv);
  */
  IDBRequest* retval;
  request.forget(&retval);
  *_retval = retval;
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::CreateIndex(const nsAString& aName,
                                const nsAString& aStoreName,
                                const nsAString& aKeyPath,
                                PRBool aUnique,
                                nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenIndex(const nsAString& aName,
                              nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::RemoveObjectStore(const nsAString& aStoreName,
                                      nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::RemoveIndex(const nsAString& aIndexName,
                                nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::SetVersion(const nsAString& aVersion,
                               nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::OpenTransaction(nsIDOMDOMStringList* aStoreNames,
                                    PRUint32 aTimeout,
                                    PRUint8 aArgCount,
                                    nsIIDBRequest** _retval)
{
  NS_NOTYETIMPLEMENTED("Implement me!");

  if (aArgCount < 2) {
    aTimeout = kDefaultDatabaseTimeoutMS;
  }

  nsCOMPtr<nsIIDBRequest> request(GenerateRequest());
  request.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const PRUnichar* aData)
{
  NS_ENSURE_FALSE(strcmp(aTopic, IDLE_THREAD_TOPIC), NS_ERROR_UNEXPECTED);

  // This should be safe to clear mStorage before we're deleted since we own
  // the thread and must join with it before we can be deleted.
  nsCOMPtr<nsIRunnable> runnable = new CloseConnectionRunnable(mStorage);

  nsCOMPtr<nsIThread> thread(do_QueryInterface(aSubject));
  NS_ENSURE_TRUE(thread, NS_NOINTERFACE);

  nsresult rv = thread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

PRUint16
CreateObjectStoreHelper::DoDatabaseWork()
{
  if (mName.IsEmpty() || (!mKeyPath.IsVoid() && mKeyPath.IsEmpty())) {
    return nsIIDBDatabaseError::CONSTRAINT_ERR;
  }

  nsresult rv = EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  // Rollback on any errors.
  mozStorageTransaction transaction(mConnection, PR_FALSE);

  // Insert the data into the database.
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO object_store (name, key_path, auto_increment) "
    "VALUES (:name, :key_path, :auto_increment)"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  if (mKeyPath.IsVoid()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("key_path"));
  } else {
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"), mKeyPath);
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("auto_increment"),
                             mAutoIncrement ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::UNKNOWN_ERR);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseError::CONSTRAINT_ERR);

  // Get the id of this object store, and store it for future use.
  (void)mConnection->GetLastInsertRowID(&mId);

  return NS_SUCCEEDED(transaction.Commit()) ? OK :
                                              nsIIDBDatabaseError::UNKNOWN_ERR;
}

void
CreateObjectStoreHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  aResult->SetAsBool(PR_TRUE);
}
