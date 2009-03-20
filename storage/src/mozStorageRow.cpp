/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#include "nsString.h"

#include "sqlite3.h"
#include "mozStorageVariant.h"
#include "mozStorageRow.h"

////////////////////////////////////////////////////////////////////////////////
//// mozStorageRow

/**
 * Note:  This object is only ever accessed on one thread at a time.  It it not
 *        threadsafe, but it does need threadsafe AddRef and Release.
 */
NS_IMPL_THREADSAFE_ISUPPORTS2(
  mozStorageRow,
  mozIStorageRow,
  mozIStorageValueArray
)

nsresult
mozStorageRow::initialize(sqlite3_stmt *aStatement)
{
  // Initialize the hash table
  NS_ENSURE_TRUE(mNameHashtable.Init(), NS_ERROR_OUT_OF_MEMORY);

  // Get the number of results
  mNumCols = sqlite3_column_count(aStatement);

  // Start copying over values
  for (PRUint32 i = 0; i < mNumCols; i++) {
    // Store the value
    nsIVariant *variant = nsnull;
    int type = sqlite3_column_type(aStatement, i);
    switch (type) {
      case SQLITE_INTEGER:
        variant = new mozStorageInteger(sqlite3_column_int64(aStatement, i));
        break;
      case SQLITE_FLOAT:
        variant = new mozStorageFloat(sqlite3_column_double(aStatement, i));
        break;
      case SQLITE_TEXT:
      {
        nsDependentString str(
          static_cast<const PRUnichar *>(sqlite3_column_text16(aStatement, i))
        );
        variant = new mozStorageText(str);
        break;
      }
      case SQLITE_NULL:
        variant = new mozStorageNull();
        break;
      case SQLITE_BLOB:
      {
        int size = sqlite3_column_bytes(aStatement, i);
        const void *data = sqlite3_column_blob(aStatement, i);
        variant = new mozStorageBlob(std::pair<const void *, int>(data, size));
        break;
      }
      default:
        return NS_ERROR_UNEXPECTED;
    }
    NS_ENSURE_TRUE(variant, NS_ERROR_OUT_OF_MEMORY);

    // Insert into our storage array
    NS_ENSURE_TRUE(mData.InsertObjectAt(variant, i), NS_ERROR_OUT_OF_MEMORY);

    // Associate the name (if any) with the index
    const char *name = sqlite3_column_name(aStatement, i);
    if (!name) break;
    nsCAutoString colName(name);
    mNameHashtable.Put(colName, i);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageRow

NS_IMETHODIMP
mozStorageRow::GetResultByIndex(PRUint32 aIndex, nsIVariant **_result)
{
  if (aIndex >= mNumCols)
    return NS_ERROR_ILLEGAL_VALUE;

  NS_ADDREF(*_result = mData.ObjectAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP
mozStorageRow::GetResultByName(const nsACString &aName, nsIVariant **_result)
{
  PRUint32 index;
  NS_ENSURE_TRUE(mNameHashtable.Get(aName, &index), NS_ERROR_NOT_AVAILABLE);
  return GetResultByIndex(index, _result);
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageValueArray

NS_IMETHODIMP
mozStorageRow::GetNumEntries(PRUint32 *_entries)
{
  *_entries = mNumCols;
  return NS_OK;
}

NS_IMETHODIMP
mozStorageRow::GetTypeOfIndex(PRUint32 aIndex, PRInt32 *_type)
{
  if (aIndex >= mNumCols)
    return NS_ERROR_ILLEGAL_VALUE;

  PRUint16 type;
  (void)mData.ObjectAt(aIndex)->GetDataType(&type);
  switch (type) {
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_INT64:
      *_type = mozIStorageValueArray::VALUE_TYPE_INTEGER;
      break;
    case nsIDataType::VTYPE_DOUBLE:
      *_type = mozIStorageValueArray::VALUE_TYPE_FLOAT;
      break;
    case nsIDataType::VTYPE_ASTRING:
      *_type = mozIStorageValueArray::VALUE_TYPE_TEXT;
      break;
    case nsIDataType::VTYPE_ARRAY:
      *_type = mozIStorageValueArray::VALUE_TYPE_BLOB;
      break;
    default:
      *_type = mozIStorageValueArray::VALUE_TYPE_NULL;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
mozStorageRow::GetInt32(PRUint32 aIndex, PRInt32 *_value)
{
  if (aIndex >= mNumCols)
    return NS_ERROR_ILLEGAL_VALUE;

  return mData.ObjectAt(aIndex)->GetAsInt32(_value);
}

NS_IMETHODIMP
mozStorageRow::GetInt64(PRUint32 aIndex, PRInt64 *_value)
{
  if (aIndex >= mNumCols)
    return NS_ERROR_ILLEGAL_VALUE;

  return mData.ObjectAt(aIndex)->GetAsInt64(_value);
}

NS_IMETHODIMP
mozStorageRow::GetDouble(PRUint32 aIndex, double *_value)
{
  if (aIndex >= mNumCols)
    return NS_ERROR_ILLEGAL_VALUE;

  return mData.ObjectAt(aIndex)->GetAsDouble(_value);
}

NS_IMETHODIMP
mozStorageRow::GetUTF8String(PRUint32 aIndex, nsACString &_value)
{
  if (aIndex >= mNumCols)
    return NS_ERROR_ILLEGAL_VALUE;

  return mData.ObjectAt(aIndex)->GetAsAUTF8String(_value);
}

NS_IMETHODIMP
mozStorageRow::GetString(PRUint32 aIndex, nsAString &_value)
{
  if (aIndex >= mNumCols)
    return NS_ERROR_ILLEGAL_VALUE;

  return mData.ObjectAt(aIndex)->GetAsAString(_value);
}

NS_IMETHODIMP
mozStorageRow::GetBlob(PRUint32 aIndex, PRUint32 *_size, PRUint8 **_blob)
{
  if (aIndex >= mNumCols)
    return NS_ERROR_ILLEGAL_VALUE;

  PRUint16 type;
  nsIID interfaceIID;
  return mData.ObjectAt(aIndex)->GetAsArray(&type, &interfaceIID, _size,
                                            reinterpret_cast<void **>(_blob));
}

NS_IMETHODIMP
mozStorageRow::GetIsNull(PRUint32 aIndex, PRBool *_isNull)
{
  if (aIndex >= mNumCols)
    return NS_ERROR_ILLEGAL_VALUE;

  PRUint16 type;
  (void)mData.ObjectAt(aIndex)->GetDataType(&type);
  *_isNull = type == nsIDataType::VTYPE_VOID;
  return NS_OK;
}

NS_IMETHODIMP
mozStorageRow::GetSharedUTF8String(PRUint32, PRUint32 *, char const **)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozStorageRow::GetSharedString(PRUint32, PRUint32 *, const PRUnichar **)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozStorageRow::GetSharedBlob(PRUint32, PRUint32 *, const PRUint8 **)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
