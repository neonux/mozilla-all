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
 *   Shawn Wilsher <me@shawnwilsher.com>
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


#include "IDBIndex.h"

#include "nsIIDBKeyRange.h"

#include "nsDOMClassInfo.h"
#include "nsThreadUtils.h"
#include "mozilla/storage.h"

#include "AsyncConnectionHelper.h"
#include "IDBCursor.h"
#include "IDBEvents.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "DatabaseInfo.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class GetHelper : public AsyncConnectionHelper
{
public:
  GetHelper(IDBTransaction* aTransaction,
            IDBRequest* aRequest,
            IDBIndex* aIndex,
            const Key& aKey)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex), mKey(aKey)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(nsIWritableVariant* aResult);

  void ReleaseMainThreadObjects()
  {
    mIndex = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

protected:
  // In-params.
  nsRefPtr<IDBIndex> mIndex;
  Key mKey;
};

class GetObjectHelper : public GetHelper
{
public:
  GetObjectHelper(IDBTransaction* aTransaction,
                  IDBRequest* aRequest,
                  IDBIndex* aIndex,
                  const Key& aKey)
  : GetHelper(aTransaction, aRequest, aIndex, aKey)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult OnSuccess(nsIDOMEventTarget* aTarget);

protected:
  nsString mValue;
};

class GetAllHelper : public GetHelper
{
public:
  GetAllHelper(IDBTransaction* aTransaction,
               IDBRequest* aRequest,
               IDBIndex* aIndex,
               const Key& aKey,
               const PRUint32 aLimit)
  : GetHelper(aTransaction, aRequest, aIndex, aKey), mLimit(aLimit)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult OnSuccess(nsIDOMEventTarget* aTarget);

protected:
  const PRUint32 mLimit;
  nsTArray<Key> mKeys;
};

class GetAllObjectsHelper : public GetHelper
{
public:
  GetAllObjectsHelper(IDBTransaction* aTransaction,
                      IDBRequest* aRequest,
                      IDBIndex* aIndex,
                      const Key& aKey,
                      const PRUint32 aLimit)
  : GetHelper(aTransaction, aRequest, aIndex, aKey), mLimit(aLimit)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult OnSuccess(nsIDOMEventTarget* aTarget);

protected:
  const PRUint32 mLimit;
  nsTArray<nsString> mValues;
};

class OpenCursorHelper : public AsyncConnectionHelper
{
public:
  OpenCursorHelper(IDBTransaction* aTransaction,
                   IDBRequest* aRequest,
                   IDBIndex* aIndex,
                   const Key& aLowerKey,
                   const Key& aUpperKey,
                   PRBool aLowerOpen,
                   PRBool aUpperOpen,
                   PRUint16 aDirection)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex),
    mLowerKey(aLowerKey), mUpperKey(aUpperKey), mLowerOpen(aLowerOpen),
    mUpperOpen(aUpperOpen), mDirection(aDirection)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(nsIWritableVariant* aResult);

  void ReleaseMainThreadObjects()
  {
    mIndex = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params.
  nsRefPtr<IDBIndex> mIndex;
  const Key mLowerKey;
  const Key mUpperKey;
  const PRPackedBool mLowerOpen;
  const PRPackedBool mUpperOpen;
  const PRUint16 mDirection;

  // Out-params.
  nsTArray<KeyKeyPair> mData;
};

class OpenObjectCursorHelper : public AsyncConnectionHelper
{
public:
  OpenObjectCursorHelper(IDBTransaction* aTransaction,
                         IDBRequest* aRequest,
                         IDBIndex* aIndex,
                         const Key& aLowerKey,
                         const Key& aUpperKey,
                         PRBool aLowerOpen,
                         PRBool aUpperOpen,
                         PRUint16 aDirection)
  : AsyncConnectionHelper(aTransaction, aRequest), mIndex(aIndex),
    mLowerKey(aLowerKey), mUpperKey(aUpperKey), mLowerOpen(aLowerOpen),
    mUpperOpen(aUpperOpen), mDirection(aDirection)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(nsIWritableVariant* aResult);

  void ReleaseMainThreadObjects()
  {
    mIndex = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  // In-params.
  nsRefPtr<IDBIndex> mIndex;
  const Key mLowerKey;
  const Key mUpperKey;
  const PRPackedBool mLowerOpen;
  const PRPackedBool mUpperOpen;
  const PRUint16 mDirection;

  // Out-params.
  nsTArray<KeyValuePair> mData;
};

inline
already_AddRefed<IDBRequest>
GenerateRequest(IDBIndex* aIndex)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  IDBDatabase* database = aIndex->ObjectStore()->Transaction()->Database();
  return IDBRequest::Create(static_cast<nsPIDOMEventTarget*>(aIndex),
                            database->ScriptContext(), database->Owner());
}

} // anonymous namespace

// static
already_AddRefed<IDBIndex>
IDBIndex::Create(IDBObjectStore* aObjectStore,
                 const IndexInfo* aIndexInfo)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aObjectStore, "Null pointer!");
  NS_ASSERTION(aIndexInfo, "Null pointer!");

  IDBDatabase* database = aObjectStore->Transaction()->Database();

  nsRefPtr<IDBIndex> index = new IDBIndex();

  index->mScriptContext = database->ScriptContext();
  index->mOwner = database->Owner();

  index->mObjectStore = aObjectStore;
  index->mId = aIndexInfo->id;
  index->mName = aIndexInfo->name;
  index->mKeyPath = aIndexInfo->keyPath;
  index->mUnique = aIndexInfo->unique;
  index->mAutoIncrement = aIndexInfo->autoIncrement;

  return index.forget();
}

IDBIndex::IDBIndex()
: mId(LL_MININT),
  mUnique(false),
  mAutoIncrement(false)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");
}

IDBIndex::~IDBIndex()
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBIndex)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBIndex,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mObjectStore,
                                                       nsPIDOMEventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBIndex,
                                                nsDOMEventTargetHelper)
  // Don't unlink mObjectStore!
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBIndex)
  NS_INTERFACE_MAP_ENTRY(nsIIDBIndex)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBIndex)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBIndex, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBIndex, nsDOMEventTargetHelper)

DOMCI_DATA(IDBIndex, IDBIndex)

NS_IMETHODIMP
IDBIndex::GetName(nsAString& aName)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetStoreName(nsAString& aStoreName)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  return mObjectStore->GetName(aStoreName);
}

NS_IMETHODIMP
IDBIndex::GetKeyPath(nsAString& aKeyPath)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  aKeyPath.Assign(mKeyPath);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetUnique(PRBool* aUnique)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  *aUnique = mUnique;
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::Get(nsIVariant* aKey,
              nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->TransactionIsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  Key key;
  nsresult rv = IDBObjectStore::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR);

  if (key.IsUnset() || key.IsNull()) {
    return NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetHelper> helper(new GetHelper(transaction, request, this, key));

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetObject(nsIVariant* aKey,
                    nsIIDBRequest** _retval)
{
  NS_PRECONDITION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->TransactionIsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  Key key;
  nsresult rv = IDBObjectStore::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR);

  if (key.IsUnset() || key.IsNull()) {
    return NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetObjectHelper> helper =
    new GetObjectHelper(transaction, request, this, key);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetAll(nsIVariant* aKey,
                 PRUint32 aLimit,
                 PRUint8 aOptionalArgCount,
                 nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->TransactionIsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  Key key;
  nsresult rv = IDBObjectStore::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR);

  if (key.IsNull()) {
    key = Key::UNSETKEY;
  }

  if (aOptionalArgCount < 2) {
    aLimit = PR_UINT32_MAX;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetAllHelper> helper =
    new GetAllHelper(transaction, request, this, key, aLimit);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::GetAllObjects(nsIVariant* aKey,
                        PRUint32 aLimit,
                        PRUint8 aOptionalArgCount,
                        nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->TransactionIsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  Key key;
  nsresult rv = IDBObjectStore::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR);

  if (key.IsNull()) {
    key = Key::UNSETKEY;
  }

  if (aOptionalArgCount < 2) {
    aLimit = PR_UINT32_MAX;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<GetAllObjectsHelper> helper =
    new GetAllObjectsHelper(transaction, request, this, key, aLimit);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::OpenCursor(nsIIDBKeyRange* aKeyRange,
                     PRUint16 aDirection,
                     PRUint8 aOptionalArgCount,
                     nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->TransactionIsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  nsresult rv;
  Key lowerKey, upperKey;
  PRBool lowerOpen = PR_FALSE, upperOpen = PR_FALSE;

  if (aKeyRange) {
    nsCOMPtr<nsIVariant> variant;
    rv = aKeyRange->GetLower(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = IDBObjectStore::GetKeyFromVariant(variant, lowerKey);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = aKeyRange->GetUpper(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = IDBObjectStore::GetKeyFromVariant(variant, upperKey);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = aKeyRange->GetLowerOpen(&lowerOpen);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = aKeyRange->GetUpperOpen(&upperOpen);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  if (aOptionalArgCount >= 2) {
    if (aDirection != nsIIDBCursor::NEXT &&
        aDirection != nsIIDBCursor::NEXT_NO_DUPLICATE &&
        aDirection != nsIIDBCursor::PREV &&
        aDirection != nsIIDBCursor::PREV_NO_DUPLICATE) {
      return NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR;
    }
  }
  else {
    aDirection = nsIIDBCursor::NEXT;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenCursorHelper> helper =
    new OpenCursorHelper(transaction, request, this, lowerKey, upperKey,
                         lowerOpen, upperOpen, aDirection);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBIndex::OpenObjectCursor(nsIIDBKeyRange* aKeyRange,
                           PRUint16 aDirection,
                           PRUint8 aOptionalArgCount,
                           nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = mObjectStore->Transaction();
  if (!transaction->TransactionIsOpen()) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  nsresult rv;
  Key lowerKey, upperKey;
  PRBool lowerOpen = PR_FALSE, upperOpen = PR_FALSE;

  if (aKeyRange) {
    nsCOMPtr<nsIVariant> variant;
    rv = aKeyRange->GetLower(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = IDBObjectStore::GetKeyFromVariant(variant, lowerKey);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = aKeyRange->GetUpper(getter_AddRefs(variant));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = IDBObjectStore::GetKeyFromVariant(variant, upperKey);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = aKeyRange->GetLowerOpen(&lowerOpen);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    rv = aKeyRange->GetUpperOpen(&upperOpen);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  if (aOptionalArgCount >= 2) {
    if (aDirection != nsIIDBCursor::NEXT &&
        aDirection != nsIIDBCursor::NEXT_NO_DUPLICATE &&
        aDirection != nsIIDBCursor::PREV &&
        aDirection != nsIIDBCursor::PREV_NO_DUPLICATE) {
      return NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR;
    }
  }
  else {
    aDirection = nsIIDBCursor::NEXT;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest(this);
  NS_ENSURE_TRUE(request, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<OpenObjectCursorHelper> helper =
    new OpenObjectCursorHelper(transaction, request, this, lowerKey, upperKey,
                               lowerOpen, upperOpen, aDirection);

  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

nsresult
GetHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->IndexGetStatement(mIndex->IsUnique(),
                                    mIndex->IsAutoIncrement());
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_NAMED_LITERAL_CSTRING(value, "value");

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(value, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(value, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Bad key type!");
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mKey = Key::UNSETKEY;

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (hasResult) {
    PRInt32 keyType;
    rv = stmt->GetTypeOfIndex(0, &keyType);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    NS_ASSERTION(keyType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Bad key type!");

    if (keyType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      mKey = stmt->AsInt64(0);
    }
    else if (keyType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      rv = stmt->GetString(0, mKey.ToString());
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }
    else {
      NS_NOTREACHED("Bad SQLite type!");
    }
  }

  return NS_OK;
}

nsresult
GetHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  NS_ASSERTION(!mKey.IsNull(), "Badness!");

  if (mKey.IsUnset()) {
    aResult->SetAsEmpty();
  }
  else if (mKey.IsString()) {
    aResult->SetAsAString(mKey.StringValue());
  }
  else if (mKey.IsInt()) {
    aResult->SetAsInt64(mKey.IntValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  return NS_OK;
}

nsresult
GetObjectHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->IndexGetObjectStatement(mIndex->IsUnique(),
                                          mIndex->IsAutoIncrement());
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("index_id"),
                                      mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_NAMED_LITERAL_CSTRING(value, "value");

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(value, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(value, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Bad key type!");
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mKey = Key::UNSETKEY;

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (hasResult) {
    rv = stmt->GetString(0, mValue);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else {
    mValue.SetIsVoid(PR_TRUE);
  }

  return NS_OK;
}

nsresult
GetObjectHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsRefPtr<GetSuccessEvent> event(new GetSuccessEvent(mValue));
  nsresult rv = event->Init(mRequest, mTransaction);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
  return NS_OK;
}

nsresult
GetAllHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mKeys.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCString keyColumn;
  nsCString tableName;

  if (mIndex->IsAutoIncrement()) {
    keyColumn.AssignLiteral("ai_object_data_id");
    if (mIndex->IsUnique()) {
      tableName.AssignLiteral("ai_unique_index_data");
    }
    else {
      tableName.AssignLiteral("ai_index_data");
    }
  }
  else {
    keyColumn.AssignLiteral("object_data_key");
    if (mIndex->IsUnique()) {
      tableName.AssignLiteral("unique_index_data");
    }
    else {
      tableName.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(value, "value");

  nsCString keyClause;
  if (!mKey.IsUnset()) {
    keyClause = NS_LITERAL_CSTRING(" AND ") + value +
                NS_LITERAL_CSTRING(" = :") + value;
  }

  nsCString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause = NS_LITERAL_CSTRING(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT ") + keyColumn +
                    NS_LITERAL_CSTRING(" FROM ") + tableName +
                    NS_LITERAL_CSTRING(" WHERE ") + indexId  +
                    NS_LITERAL_CSTRING(" = :") + indexId + keyClause +
                    limitClause;
  
  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!mKey.IsUnset()) {
    if (mKey.IsInt()) {
      rv = stmt->BindInt64ByName(value, mKey.IntValue());
    }
    else if (mKey.IsString()) {
      rv = stmt->BindStringByName(value, mKey.StringValue());
    }
    else {
      NS_NOTREACHED("Bad key type!");
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  PRBool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mKeys.Capacity() == mKeys.Length()) {
      if (!mKeys.SetCapacity(mKeys.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    Key* key = mKeys.AppendElement();
    NS_ASSERTION(key, "This shouldn't fail!");

    PRInt32 keyType;
    rv = stmt->GetTypeOfIndex(0, &keyType);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    NS_ASSERTION(keyType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Bad key type!");

    if (keyType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      *key = stmt->AsInt64(0);
    }
    else if (keyType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      rv = stmt->GetString(0, key->ToString());
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }
    else {
      NS_NOTREACHED("Bad SQLite type!");
    }
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
GetAllHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsRefPtr<GetAllKeySuccessEvent> event(new GetAllKeySuccessEvent(mKeys));

  NS_ASSERTION(mKeys.IsEmpty(), "Should have swapped!");

  nsresult rv = event->Init(mRequest, mTransaction);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
  return NS_OK;
}

nsresult
GetAllObjectsHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mValues.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCString dataTableName;
  nsCString objectDataId;
  nsCString indexTableName;

  if (mIndex->IsAutoIncrement()) {
    dataTableName.AssignLiteral("ai_object_data");
    objectDataId.AssignLiteral("ai_object_data_id");
    if (mIndex->IsUnique()) {
      indexTableName.AssignLiteral("ai_unique_index_data");
    }
    else {
      indexTableName.AssignLiteral("ai_index_data");
    }
  }
  else {
    dataTableName.AssignLiteral("object_data");
    objectDataId.AssignLiteral("object_data_id");
    if (mIndex->IsUnique()) {
      indexTableName.AssignLiteral("unique_index_data");
    }
    else {
      indexTableName.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(value, "value");

  nsCString keyClause;
  if (!mKey.IsUnset()) {
    keyClause = NS_LITERAL_CSTRING(" AND ") + value +
                NS_LITERAL_CSTRING(" = :") + value;
  }

  nsCString limitClause;
  if (mLimit != PR_UINT32_MAX) {
    limitClause = NS_LITERAL_CSTRING(" LIMIT ");
    limitClause.AppendInt(mLimit);
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT data FROM ") + dataTableName +
                    NS_LITERAL_CSTRING(" INNER JOIN ") + indexTableName  +
                    NS_LITERAL_CSTRING(" ON ") + dataTableName +
                    NS_LITERAL_CSTRING(".id = ") + indexTableName +
                    NS_LITERAL_CSTRING(".") + objectDataId +
                    NS_LITERAL_CSTRING(" WHERE ") + indexId  +
                    NS_LITERAL_CSTRING(" = :") + indexId + keyClause +
                    limitClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!mKey.IsUnset()) {
    if (mKey.IsInt()) {
      rv = stmt->BindInt64ByName(value, mKey.IntValue());
    }
    else if (mKey.IsString()) {
      rv = stmt->BindStringByName(value, mKey.StringValue());
    }
    else {
      NS_NOTREACHED("Bad key type!");
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  PRBool hasResult;
  while(NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mValues.Capacity() == mValues.Length()) {
      if (!mValues.SetCapacity(mValues.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    nsString* value = mValues.AppendElement();
    NS_ASSERTION(value, "This shouldn't fail!");

#ifdef DEBUG
    {
      PRInt32 keyType;
      NS_ASSERTION(NS_SUCCEEDED(stmt->GetTypeOfIndex(0, &keyType)) &&
                   keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                   "Bad SQLITE type!");
    }
#endif

    rv = stmt->GetString(0, *value);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
GetAllObjectsHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsRefPtr<GetAllSuccessEvent> event(new GetAllSuccessEvent(mValues));

  NS_ASSERTION(mValues.IsEmpty(), "Should have swapped!");

  nsresult rv = event->Init(mRequest, mTransaction);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  PRBool dummy;
  aTarget->DispatchEvent(static_cast<nsDOMEvent*>(event), &dummy);
  return NS_OK;
}

nsresult
OpenCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mData.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCString table;
  nsCString keyColumn;

  if (mIndex->IsAutoIncrement()) {
    keyColumn.AssignLiteral("ai_object_data_id");
    if (mIndex->IsUnique()) {
      table.AssignLiteral("ai_unique_index_data");
    }
    else {
      table.AssignLiteral("ai_index_data");
    }
  }
  else {
    keyColumn.AssignLiteral("object_data_key");
    if (mIndex->IsUnique()) {
      table.AssignLiteral("unique_index_data");
    }
    else {
      table.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(leftKeyName, "left_key");
  NS_NAMED_LITERAL_CSTRING(rightKeyName, "right_key");

  nsCAutoString keyRangeClause;
  if (!mLowerKey.IsUnset()) {
    keyRangeClause = NS_LITERAL_CSTRING(" AND value");
    if (mLowerOpen) {
      keyRangeClause.AppendLiteral(" > :");
    }
    else {
      keyRangeClause.AppendLiteral(" >= :");
    }
    keyRangeClause.Append(leftKeyName);
  }

  if (!mUpperKey.IsUnset()) {
    keyRangeClause.AppendLiteral(" AND value");
    if (mUpperOpen) {
      keyRangeClause.AppendLiteral(" < :");
    }
    else {
      keyRangeClause.AppendLiteral(" <= :");
    }
    keyRangeClause.Append(rightKeyName);
  }

  nsCString groupClause;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::PREV:
      break;

    case nsIIDBCursor::NEXT_NO_DUPLICATE:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      groupClause = NS_LITERAL_CSTRING(" GROUP BY value");
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }

  nsCString directionClause;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::NEXT_NO_DUPLICATE:
      directionClause = NS_LITERAL_CSTRING("DESC");
      break;

    case nsIIDBCursor::PREV:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      directionClause = NS_LITERAL_CSTRING("ASC");
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT value, ") + keyColumn +
                    NS_LITERAL_CSTRING(" FROM ") + table +
                    NS_LITERAL_CSTRING(" WHERE index_id = :") + indexId +
                    keyRangeClause + groupClause +
                    NS_LITERAL_CSTRING(" ORDER BY value ") + directionClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!mLowerKey.IsUnset()) {
    if (mLowerKey.IsString()) {
      rv = stmt->BindStringByName(leftKeyName, mLowerKey.StringValue());
    }
    else if (mLowerKey.IsInt()) {
      rv = stmt->BindInt64ByName(leftKeyName, mLowerKey.IntValue());
    }
    else {
      NS_NOTREACHED("Bad key!");
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  if (!mUpperKey.IsUnset()) {
    if (mUpperKey.IsString()) {
      rv = stmt->BindStringByName(rightKeyName, mUpperKey.StringValue());
    }
    else if (mUpperKey.IsInt()) {
      rv = stmt->BindInt64ByName(rightKeyName, mUpperKey.IntValue());
    }
    else {
      NS_NOTREACHED("Bad key!");
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  PRBool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mData.Capacity() == mData.Length()) {
      if (!mData.SetCapacity(mData.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    KeyKeyPair* pair = mData.AppendElement();
    NS_ASSERTION(pair, "Shouldn't fail if SetCapacity succeeded!");

    PRInt32 keyType;
    rv = stmt->GetTypeOfIndex(0, &keyType);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    NS_ASSERTION(keyType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Bad key type!");

    if (keyType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      pair->key = stmt->AsInt64(0);
    }
    else if (keyType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      rv = stmt->GetString(0, pair->key.ToString());
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }
    else {
      NS_NOTREACHED("Bad SQLite type!");
    }

    rv = stmt->GetTypeOfIndex(1, &keyType);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    NS_ASSERTION(keyType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Bad key type!");

    if (keyType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      pair->value = stmt->AsInt64(1);
    }
    else if (keyType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      rv = stmt->GetString(1, pair->value.ToString());
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }
    else {
      NS_NOTREACHED("Bad SQLite type!");
    }
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
OpenCursorHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  if (mData.IsEmpty()) {
    aResult->SetAsEmpty();
    return NS_OK;
  }

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(mRequest, mTransaction, mIndex, mDirection, mData);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  aResult->SetAsISupports(static_cast<nsPIDOMEventTarget*>(cursor));
  return NS_OK;
}

nsresult
OpenObjectCursorHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(aConnection, "Passed a null connection!");

  if (!mData.SetCapacity(50)) {
    NS_ERROR("Out of memory!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCString indexTable;
  nsCString objectTable;
  nsCString objectDataId;

  if (mIndex->IsAutoIncrement()) {
    objectTable.AssignLiteral("ai_object_data");
    objectDataId.AssignLiteral("ai_object_data_id");
    if (mIndex->IsUnique()) {
      indexTable.AssignLiteral("ai_unique_index_data");
    }
    else {
      indexTable.AssignLiteral("ai_index_data");
    }
  }
  else {
    objectTable.AssignLiteral("object_data");
    objectDataId.AssignLiteral("object_data_id");
    if (mIndex->IsUnique()) {
      indexTable.AssignLiteral("unique_index_data");
    }
    else {
      indexTable.AssignLiteral("index_data");
    }
  }

  NS_NAMED_LITERAL_CSTRING(indexId, "index_id");
  NS_NAMED_LITERAL_CSTRING(leftKeyName, "left_key");
  NS_NAMED_LITERAL_CSTRING(rightKeyName, "right_key");
  NS_NAMED_LITERAL_CSTRING(value, "value");
  NS_NAMED_LITERAL_CSTRING(data, "data");

  nsCAutoString keyRangeClause;
  if (!mLowerKey.IsUnset()) {
    keyRangeClause = NS_LITERAL_CSTRING(" AND value");
    if (mLowerOpen) {
      keyRangeClause.AppendLiteral(" > :");
    }
    else {
      keyRangeClause.AppendLiteral(" >= :");
    }
    keyRangeClause.Append(leftKeyName);
  }

  if (!mUpperKey.IsUnset()) {
    keyRangeClause += NS_LITERAL_CSTRING(" AND value");
    if (mUpperOpen) {
      keyRangeClause.AppendLiteral(" < :");
    }
    else {
      keyRangeClause.AppendLiteral(" <= :");
    }
    keyRangeClause.Append(rightKeyName);
  }

  nsCString groupClause;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::PREV:
      break;

    case nsIIDBCursor::NEXT_NO_DUPLICATE:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      groupClause = NS_LITERAL_CSTRING(" GROUP BY ") + indexTable +
                    NS_LITERAL_CSTRING(".") + value;
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }

  nsCString directionClause;
  switch (mDirection) {
    case nsIIDBCursor::NEXT:
    case nsIIDBCursor::NEXT_NO_DUPLICATE:
      directionClause = NS_LITERAL_CSTRING(" DESC");
      break;

    case nsIIDBCursor::PREV:
    case nsIIDBCursor::PREV_NO_DUPLICATE:
      directionClause = NS_LITERAL_CSTRING(" ASC");
      break;

    default:
      NS_NOTREACHED("Unknown direction!");
  }

  nsCString query = NS_LITERAL_CSTRING("SELECT ") + indexTable +
                    NS_LITERAL_CSTRING(".") + value +
                    NS_LITERAL_CSTRING(", ") + objectTable +
                    NS_LITERAL_CSTRING(".") + data +
                    NS_LITERAL_CSTRING(" FROM ") + objectTable +
                    NS_LITERAL_CSTRING(" INNER JOIN ") + indexTable +
                    NS_LITERAL_CSTRING(" ON ") + indexTable +
                    NS_LITERAL_CSTRING(".") + objectDataId +
                    NS_LITERAL_CSTRING(" = ") + objectTable +
                    NS_LITERAL_CSTRING(".id WHERE ") + indexId +
                    NS_LITERAL_CSTRING(" = :") + indexId + keyRangeClause +
                    groupClause + NS_LITERAL_CSTRING(" ORDER BY ") +
                    indexTable + NS_LITERAL_CSTRING(".") + value +
                    directionClause;

  nsCOMPtr<mozIStorageStatement> stmt = mTransaction->GetCachedStatement(query);
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(indexId, mIndex->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (!mLowerKey.IsUnset()) {
    if (mLowerKey.IsString()) {
      rv = stmt->BindStringByName(leftKeyName, mLowerKey.StringValue());
    }
    else if (mLowerKey.IsInt()) {
      rv = stmt->BindInt64ByName(leftKeyName, mLowerKey.IntValue());
    }
    else {
      NS_NOTREACHED("Bad key!");
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  if (!mUpperKey.IsUnset()) {
    if (mUpperKey.IsString()) {
      rv = stmt->BindStringByName(rightKeyName, mUpperKey.StringValue());
    }
    else if (mUpperKey.IsInt()) {
      rv = stmt->BindInt64ByName(rightKeyName, mUpperKey.IntValue());
    }
    else {
      NS_NOTREACHED("Bad key!");
    }
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  PRBool hasResult;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
    if (mData.Capacity() == mData.Length()) {
      if (!mData.SetCapacity(mData.Capacity() * 2)) {
        NS_ERROR("Out of memory!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
    }

    KeyValuePair* pair = mData.AppendElement();
    NS_ASSERTION(pair, "Shouldn't fail if SetCapacity succeeded!");

    PRInt32 keyType;
    rv = stmt->GetTypeOfIndex(0, &keyType);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    NS_ASSERTION(keyType == mozIStorageStatement::VALUE_TYPE_INTEGER ||
                 keyType == mozIStorageStatement::VALUE_TYPE_TEXT,
                 "Bad key type!");

    if (keyType == mozIStorageStatement::VALUE_TYPE_INTEGER) {
      pair->key = stmt->AsInt64(0);
    }
    else if (keyType == mozIStorageStatement::VALUE_TYPE_TEXT) {
      rv = stmt->GetString(0, pair->key.ToString());
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    }
    else {
      NS_NOTREACHED("Bad SQLite type!");
    }

    rv = stmt->GetString(1, pair->value);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
OpenObjectCursorHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  if (mData.IsEmpty()) {
    aResult->SetAsEmpty();
    return NS_OK;
  }

  nsRefPtr<IDBCursor> cursor =
    IDBCursor::Create(mRequest, mTransaction, mIndex, mDirection, mData);
  NS_ENSURE_TRUE(cursor, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  aResult->SetAsISupports(static_cast<nsPIDOMEventTarget*>(cursor));
  return NS_OK;
}
