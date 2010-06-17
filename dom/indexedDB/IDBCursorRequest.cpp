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

// XXX remove once we can get jsvals out of XPIDL
#include "jscntxt.h"
#include "jsapi.h"

#include "IDBCursorRequest.h"

#include "nsIIDBDatabaseException.h"
#include "nsIVariant.h"

#include "mozilla/storage.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsJSON.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBEvents.h"
#include "IDBIndexRequest.h"
#include "IDBObjectStoreRequest.h"
#include "IDBTransactionRequest.h"
#include "Savepoint.h"
#include "TransactionThreadPool.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class UpdateHelper : public AsyncConnectionHelper
{
public:
  UpdateHelper(IDBTransactionRequest* aTransaction,
               IDBRequest* aRequest,
               PRInt64 aObjectStoreID,
               const nsAString& aValue,
               const Key& aKey,
               bool aAutoIncrement,
               nsTArray<IndexUpdateInfo>& aIndexUpdateInfo)
  : AsyncConnectionHelper(aTransaction, aRequest), mOSID(aObjectStoreID),
    mValue(aValue), mKey(aKey), mAutoIncrement(aAutoIncrement)
  {
    mIndexUpdateInfo.SwapElements(aIndexUpdateInfo);
  }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  const PRInt64 mOSID;
  const nsString mValue;
  const Key mKey;
  const bool mAutoIncrement;
  nsTArray<IndexUpdateInfo> mIndexUpdateInfo;
};

class RemoveHelper : public AsyncConnectionHelper
{
public:
  RemoveHelper(IDBTransactionRequest* aTransaction,
               IDBRequest* aRequest,
               PRInt64 aObjectStoreID,
               const Key& aKey,
               bool aAutoIncrement)
  : AsyncConnectionHelper(aTransaction, aRequest), mOSID(aObjectStoreID),
    mKey(aKey), mAutoIncrement(aAutoIncrement)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  const PRInt64 mOSID;
  const nsString mValue;
  const Key mKey;
  const bool mAutoIncrement;
};

} // anonymous namespace

BEGIN_INDEXEDDB_NAMESPACE

class ContinueRunnable : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  ContinueRunnable(IDBCursorRequest* aCursor,
                   const Key& aKey)
  : mCursor(aCursor), mKey(aKey)
  { }

private:
  nsRefPtr<IDBCursorRequest> mCursor;
  const Key mKey;
};

END_INDEXEDDB_NAMESPACE

// static
already_AddRefed<IDBCursorRequest>
IDBCursorRequest::Create(IDBRequest* aRequest,
                         IDBTransactionRequest* aTransaction,
                         IDBObjectStoreRequest* aObjectStore,
                         PRUint16 aDirection,
                         nsTArray<KeyValuePair>& aData)
{
  NS_ASSERTION(aObjectStore, "Null pointer!");

  nsRefPtr<IDBCursorRequest> cursor =
    IDBCursorRequest::CreateCommon(aRequest, aTransaction, aDirection);

  cursor->mObjectStore = aObjectStore;

  if (!cursor->mData.SwapElements(aData)) {
    NS_ERROR("Out of memory?!");
    return nsnull;
  }
  NS_ASSERTION(!cursor->mData.IsEmpty(), "Should never have an empty set!");

  cursor->mDataIndex = cursor->mData.Length() - 1;
  cursor->mType = OBJECTSTORE;

  return cursor.forget();
}

// static
already_AddRefed<IDBCursorRequest>
IDBCursorRequest::Create(IDBRequest* aRequest,
                         IDBTransactionRequest* aTransaction,
                         IDBIndexRequest* aIndex,
                         PRUint16 aDirection,
                         nsTArray<KeyKeyPair>& aData)
{
  NS_ASSERTION(aIndex, "Null pointer!");

  nsRefPtr<IDBCursorRequest> cursor =
    IDBCursorRequest::CreateCommon(aRequest, aTransaction, aDirection);

  cursor->mObjectStore = aIndex->ObjectStore();
  cursor->mIndex = aIndex;

  if (!cursor->mKeyData.SwapElements(aData)) {
    NS_ERROR("Out of memory?!");
    return nsnull;
  }
  NS_ASSERTION(!cursor->mKeyData.IsEmpty(), "Should never have an empty set!");

  cursor->mDataIndex = cursor->mKeyData.Length() - 1;
  cursor->mType = INDEX;

  return cursor.forget();
}

// static
already_AddRefed<IDBCursorRequest>
IDBCursorRequest::Create(IDBRequest* aRequest,
                         IDBTransactionRequest* aTransaction,
                         IDBIndexRequest* aIndex,
                         PRUint16 aDirection,
                         nsTArray<KeyValuePair>& aData)
{
  NS_ASSERTION(aIndex, "Null pointer!");

  nsRefPtr<IDBCursorRequest> cursor =
    IDBCursorRequest::CreateCommon(aRequest, aTransaction, aDirection);

  cursor->mObjectStore = aIndex->ObjectStore();
  cursor->mIndex = aIndex;

  if (!cursor->mData.SwapElements(aData)) {
    NS_ERROR("Out of memory?!");
    return nsnull;
  }
  NS_ASSERTION(!cursor->mData.IsEmpty(), "Should never have an empty set!");

  cursor->mDataIndex = cursor->mData.Length() - 1;
  cursor->mType = INDEXOBJECT;

  return cursor.forget();
}

// static
already_AddRefed<IDBCursorRequest>
IDBCursorRequest::CreateCommon(IDBRequest* aRequest,
                               IDBTransactionRequest* aTransaction,
                               PRUint16 aDirection)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRequest, "Null pointer!");
  NS_ASSERTION(aTransaction, "Null pointer!");

  nsRefPtr<IDBCursorRequest> cursor(new IDBCursorRequest());
  cursor->mRequest = aRequest;
  cursor->mTransaction = aTransaction;
  cursor->mDirection = aDirection;

  return cursor.forget();
}

IDBCursorRequest::IDBCursorRequest()
: mDirection(nsIIDBCursor::NEXT),
  mCachedValue(JSVAL_VOID),
  mHaveCachedValue(false),
  mJSRuntime(nsnull),
  mContinueCalled(false),
  mDataIndex(0),
  mType(OBJECTSTORE)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBCursorRequest::~IDBCursorRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mJSRuntime) {
    JS_RemoveRootRT(mJSRuntime, &mCachedValue);
  }
}

NS_IMPL_ADDREF(IDBCursorRequest)
NS_IMPL_RELEASE(IDBCursorRequest)

NS_INTERFACE_MAP_BEGIN(IDBCursorRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, IDBRequest::Generator)
  NS_INTERFACE_MAP_ENTRY(nsIIDBCursorRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBCursor)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBCursorRequest)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBCursorRequest, IDBCursorRequest)

NS_IMETHODIMP
IDBCursorRequest::GetDirection(PRUint16* aDirection)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  *aDirection = mDirection;
  return NS_OK;
}

NS_IMETHODIMP
IDBCursorRequest::GetKey(nsIVariant** aKey)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mCachedKey) {
    nsresult rv;
    nsCOMPtr<nsIWritableVariant> variant =
      do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    const Key& key = mType == INDEX ?
                     mKeyData[mDataIndex].key :
                     mData[mDataIndex].key;
    NS_ASSERTION(!key.IsUnset() && !key.IsNull(), "Bad key!");

    if (key.IsString()) {
      rv = variant->SetAsAString(key.StringValue());
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (key.IsInt()) {
      rv = variant->SetAsInt64(key.IntValue());
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      NS_NOTREACHED("Huh?!");
    }

    rv = variant->SetWritable(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIWritableVariant* result;
    variant.forget(&result);

    mCachedKey = dont_AddRef(static_cast<nsIVariant*>(result));
  }

  nsCOMPtr<nsIVariant> result(mCachedKey);
  result.forget(aKey);
  return NS_OK;
}

NS_IMETHODIMP
IDBCursorRequest::GetValue(nsIVariant** aValue)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv;

  if (mType == INDEX) {
    nsCOMPtr<nsIWritableVariant> variant =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
    if (!variant) {
      NS_ERROR("Couldn't create variant!");
      return NS_ERROR_FAILURE;
    }

    const Key& value = mKeyData[mDataIndex].value;
    NS_ASSERTION(!value.IsUnset() && !value.IsNull(), "Bad key!");
    if (value.IsInt()) {
      rv = variant->SetAsInt64(value.IntValue());
    }
    else if (value.IsString()) {
      rv = variant->SetAsAString(value.StringValue());
    }
    else {
      NS_NOTREACHED("Bad key type!");
    }
    NS_ENSURE_SUCCESS(rv, rv);

    rv = variant->SetWritable(PR_FALSE);;
    NS_ENSURE_SUCCESS(rv, rv);

    nsIWritableVariant* result;
    variant.forget(&result);
    *aValue = result;
    return NS_OK;
  }

  NS_WARNING("Using a slow path for GetValue! Fix this now!");

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);

  nsAXPCNativeCallContext* cc;
  rv = xpc->GetCurrentNativeCallContext(&cc);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(cc, NS_ERROR_UNEXPECTED);

  jsval* retval;
  rv = cc->GetRetValPtr(&retval);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mHaveCachedValue) {
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    NS_ENSURE_SUCCESS(rv, rv);

    JSAutoRequest ar(cx);

    if (!mJSRuntime) {
      JSRuntime* rt = JS_GetRuntime(cx);

      JSBool ok = JS_AddNamedRootRT(rt, &mCachedValue,
                                   "IDBCursorRequest::mCachedValue");
      NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

      mJSRuntime = rt;
    }

    nsCOMPtr<nsIJSON> json(new nsJSON());
    rv = json->DecodeToJSVal(mData[mDataIndex].value, cx, &mCachedValue);
    NS_ENSURE_SUCCESS(rv, rv);

    mHaveCachedValue = true;
  }

  *retval = mCachedValue;
  cc->SetReturnValueWasSet(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
IDBCursorRequest::Continue(nsIVariant* aKey,
                           PRUint8 aOptionalArgCount,
                           PRBool* _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mObjectStore->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mContinueCalled) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Key key;
  nsresult rv = IDBObjectStoreRequest::GetKeyFromVariant(aKey, key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (key.IsNull()) {
    if (aOptionalArgCount) {
      return NS_ERROR_INVALID_ARG;
    }
    else {
      key = Key::UNSETKEY;
    }
  }

  if (mType != OBJECTSTORE && !key.IsUnset()) {
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
  NS_ENSURE_TRUE(pool, NS_ERROR_FAILURE);

  nsRefPtr<ContinueRunnable> runnable(new ContinueRunnable(this, key));

  rv = pool->Dispatch(mTransaction, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  mTransaction->OnNewRequest();

  mContinueCalled = true;

  *_retval = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
IDBCursorRequest::Update(nsIVariant* aValue,
                         nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mType != OBJECTSTORE) {
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!mObjectStore->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mObjectStore->IsWriteAllowed()) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  const Key& key = mData[mDataIndex].key;
  NS_ASSERTION(!key.IsUnset() && !key.IsNull(), "Bad key!");

  // This is the slow path, need to do this better once XPIDL can have raw
  // jsvals as arguments.
  NS_WARNING("Using a slow path for Update! Fix this now!");

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);

  nsAXPCNativeCallContext* cc;
  nsresult rv = xpc->GetCurrentNativeCallContext(&cc);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(cc, NS_ERROR_UNEXPECTED);

  PRUint32 argc;
  rv = cc->GetArgc(&argc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (argc < 1) {
    return NS_ERROR_XPC_NOT_ENOUGH_ARGS;
  }

  jsval* argv;
  rv = cc->GetArgvPtr(&argv);
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext* cx;
  rv = cc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  js::AutoValueRooter clone(cx);
  rv = nsContentUtils::CreateStructuredClone(cx, argv[0], clone.addr());
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mObjectStore->KeyPath().IsEmpty()) {
    // Make sure the object given has the correct keyPath value set on it or
    // we will add it.
    const nsString& keyPath = mObjectStore->KeyPath();
    const jschar* keyPathChars = reinterpret_cast<const jschar*>(keyPath.get());
    const size_t keyPathLen = keyPath.Length();

    js::AutoValueRooter prop(cx);
    JSBool ok = JS_GetUCProperty(cx, JSVAL_TO_OBJECT(clone.value()),
                                 keyPathChars, keyPathLen, prop.addr());
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

    if (JSVAL_IS_VOID(prop.value())) {
      if (key.IsInt()) {
        ok = JS_NewNumberValue(cx, key.IntValue(), prop.addr());
        NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
      }
      else if (key.IsString()) {
        const nsString& keyString = key.StringValue();
        JSString* str =
          JS_NewUCStringCopyN(cx,
                              reinterpret_cast<const jschar*>(keyString.get()),
                              keyString.Length());
        NS_ENSURE_TRUE(str, NS_ERROR_FAILURE);

        prop.set(STRING_TO_JSVAL(str));
      }
      else {
        NS_NOTREACHED("Bad key!");
      }

      ok = JS_DefineUCProperty(cx, JSVAL_TO_OBJECT(clone.value()), keyPathChars,
                               keyPathLen, prop.value(), nsnull, nsnull,
                               JSPROP_ENUMERATE);
      NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
    }

    if (JSVAL_IS_NULL(prop.value())) {
      return NS_ERROR_INVALID_ARG;
    }

    if (JSVAL_IS_INT(prop.value())) {
      if (!key.IsInt() || JSVAL_TO_INT(prop.value()) != key.IntValue()) {
        return NS_ERROR_INVALID_ARG;
      }
    }

    if (JSVAL_IS_DOUBLE(prop.value())) {
      if (!key.IsInt() || *JSVAL_TO_DOUBLE(prop.value()) != key.IntValue()) {
        return NS_ERROR_INVALID_ARG;
      }
    }

    if (JSVAL_IS_STRING(prop.value())) {
      if (!key.IsString()) {
        return NS_ERROR_INVALID_ARG;
      }

      if (key.StringValue() != nsDependentJSString(prop.value())) {
        return NS_ERROR_INVALID_ARG;
      }
    }
  }

  nsTArray<IndexUpdateInfo> indexUpdateInfo;
  rv = IDBObjectStoreRequest::GetIndexUpdateInfo(mObjectStore->GetObjectStoreInfo(),
                                                 cx, clone.value(),
                                                 indexUpdateInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIJSON> json(new nsJSON());

  nsString jsonValue;
  rv = json->EncodeFromJSVal(clone.addr(), cx, jsonValue);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<IDBRequest> request = GenerateWriteRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<UpdateHelper> helper =
    new UpdateHelper(mTransaction, request, mObjectStore->Id(), jsonValue, key,
                     mObjectStore->IsAutoIncrement(), indexUpdateInfo);
  rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBCursorRequest::Remove(nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mType != OBJECTSTORE) {
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!mObjectStore->TransactionIsOpen()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mObjectStore->IsWriteAllowed()) {
    return NS_ERROR_OBJECT_IS_IMMUTABLE;
  }

  const Key& key = mData[mDataIndex].key;
  NS_ASSERTION(!key.IsUnset() && !key.IsNull(), "Bad key!");

  nsRefPtr<IDBRequest> request = GenerateWriteRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<RemoveHelper> helper =
    new RemoveHelper(mTransaction, request, mObjectStore->Id(), key,
                     mObjectStore->IsAutoIncrement());
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

PRUint16
UpdateHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsresult rv;
  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Badness!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->AddStatement(false, true, mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  Savepoint savepoint(mTransaction);

  NS_NAMED_LITERAL_CSTRING(keyValue, "key_value");

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(keyValue, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(keyValue, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("data"), mValue);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::CONSTRAINT_ERR;
  }

  // Update our indexes if needed.
  if (!mIndexUpdateInfo.IsEmpty()) {
    PRInt64 objectDataId = mAutoIncrement ? mKey.IntValue() : LL_MININT;
    rv = IDBObjectStoreRequest::UpdateIndexes(mTransaction, mOSID, mKey,
                                              mAutoIncrement, true,
                                              objectDataId, mIndexUpdateInfo);
    if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
      return nsIIDBDatabaseException::CONSTRAINT_ERR;
    }
    NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  }

  rv = savepoint.Release();
  return NS_SUCCEEDED(rv) ? OK : nsIIDBDatabaseException::UNKNOWN_ERR;
}

PRUint16
UpdateHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Badness!");

  if (mKey.IsString()) {
    aResult->SetAsAString(mKey.StringValue());
  }
  else if (mKey.IsInt()) {
    aResult->SetAsInt64(mKey.IntValue());
  }
  else {
    NS_NOTREACHED("Bad key!");
  }
  return OK;
}

PRUint16
RemoveHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passed a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->RemoveStatement(mAutoIncrement);
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("osid"), mOSID);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Must have a key here!");

  NS_NAMED_LITERAL_CSTRING(key_value, "key_value");

  if (mKey.IsInt()) {
    rv = stmt->BindInt64ByName(key_value, mKey.IntValue());
  }
  else if (mKey.IsString()) {
    rv = stmt->BindStringByName(key_value, mKey.StringValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  // Search for it!
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
RemoveHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  NS_ASSERTION(!mKey.IsUnset() && !mKey.IsNull(), "Badness!");

  if (mKey.IsString()) {
    aResult->SetAsAString(mKey.StringValue());
  }
  else if (mKey.IsInt()) {
    aResult->SetAsInt64(mKey.IntValue());
  }
  else {
    NS_NOTREACHED("Unknown key type!");
  }
  return OK;
}

NS_IMETHODIMP
ContinueRunnable::Run()
{
  nsresult rv;

  if (!NS_IsMainThread()) {
    rv = NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // Remove cached stuff from last time.
  mCursor->mCachedKey = nsnull;
  mCursor->mCachedValue = JSVAL_VOID;
  mCursor->mHaveCachedValue = false;
  mCursor->mContinueCalled = false;

  if (mCursor->mType == IDBCursorRequest::INDEX) {
    mCursor->mKeyData.RemoveElementAt(mCursor->mDataIndex);
  }
  else {
    mCursor->mData.RemoveElementAt(mCursor->mDataIndex);
  }
  if (mCursor->mDataIndex) {
    mCursor->mDataIndex--;
  }

  nsCOMPtr<nsIWritableVariant> variant =
    do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!variant) {
    NS_ERROR("Couldn't create variant!");
    return NS_ERROR_FAILURE;
  }

  PRBool empty = mCursor->mType == IDBCursorRequest::INDEX ?
                 mCursor->mKeyData.IsEmpty() :
                 mCursor->mData.IsEmpty();

  if (empty) {
    rv = variant->SetAsEmpty();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    if (!mKey.IsUnset()) {
      NS_ASSERTION(!mKey.IsNull(), "Huh?!");

      if (mCursor->mType != IDBCursorRequest::OBJECTSTORE) {
        NS_NOTYETIMPLEMENTED("Implement me!");
        return NS_ERROR_NOT_IMPLEMENTED;
      }

      NS_WARNING("Using a slow O(n) search for continue(key), do something "
                 "smarter!");

      // Skip ahead to our next key match.
      PRInt32 index = PRInt32(mCursor->mDataIndex);
      while (index >= 0) {
        const Key& key = mCursor->mData[index].key;
        if (mKey == key) {
          break;
        }
        if (key < mKey) {
          index--;
          continue;
        }
        index = -1;
        break;
      }

      if (index >= 0) {
        mCursor->mDataIndex = PRUint32(index);
        mCursor->mData.RemoveElementsAt(index + 1,
                                        mCursor->mData.Length() - index - 1);
      }
    }

    rv = variant->SetAsISupports(static_cast<IDBRequest::Generator*>(mCursor));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = variant->SetWritable(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMEvent> event =
    IDBSuccessEvent::Create(mCursor->mRequest, variant, mCursor->mTransaction);
  if (!event) {
    NS_ERROR("Failed to create event!");
    return NS_ERROR_FAILURE;
  }

  PRBool dummy;
  mCursor->mRequest->DispatchEvent(event, &dummy);

  mCursor->mTransaction->OnRequestFinished();

  return NS_OK;
}
