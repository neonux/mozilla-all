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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Honza Bambas <honzab@firemni.cz>
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

#ifndef nsDOMStorageMemoryDB_h___
#define nsDOMStorageMemoryDB_h___

#include "nscore.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"

class nsDOMStoragePersistentDB;

class nsDOMStorageMemoryDB
{
public:
  nsDOMStorageMemoryDB() : mPreloading(PR_FALSE) {}
  ~nsDOMStorageMemoryDB() {}

  class nsInMemoryItem
  {
  public:
    PRBool mSecure;
    nsString mValue;
    PRInt64 mInsertTime;
  };

  typedef nsClassHashtable<nsStringHashKey, nsInMemoryItem> nsStorageItemsTable;

  class nsInMemoryStorage
  {
  public:
    nsStorageItemsTable mTable;
    PRInt32 mUsageDelta;

    nsInMemoryStorage() : mUsageDelta(0) {}
  };

  /**
   * @param aPreloadDB
   *    If non-null, data for a domain/origin will be preloaded from
   *    the provided database. Used for session-only cookies mode to
   *    provide existing data from the persistent database.
   */
  nsresult
  Init(nsDOMStoragePersistentDB* aPreloadDB = nsnull);

  /**
   *
   */
  nsresult
  GetItemsTable(nsDOMStorage* aStorage,
                nsInMemoryStorage** aMemoryStorage);

  /**
   * Retrieve a list of all the keys associated with a particular domain.
   */
  nsresult
  GetAllKeys(nsDOMStorage* aStorage,
             nsTHashtable<nsSessionStorageEntry>* aKeys);

  /**
   * Retrieve a value and secure flag for a key from storage.
   *
   * @throws NS_ERROR_DOM_NOT_FOUND_ERR if key not found
   */
  nsresult
  GetKeyValue(nsDOMStorage* aStorage,
              const nsAString& aKey,
              nsAString& aValue,
              PRBool* aSecure);

  /**
   * Set the value and secure flag for a key in storage.
   */
  nsresult
  SetKey(nsDOMStorage* aStorage,
         const nsAString& aKey,
         const nsAString& aValue,
         PRBool aSecure,
         PRInt32 aQuota,
         PRBool aExcludeOfflineFromUsage,
         PRInt32* aNewUsage);

  /**
   * Set the secure flag for a key in storage. Does nothing if the key was
   * not found.
   */
  nsresult
  SetSecure(nsDOMStorage* aStorage,
            const nsAString& aKey,
            const PRBool aSecure);

  /**
   * Removes a key from storage.
   */
  nsresult
  RemoveKey(nsDOMStorage* aStorage,
            const nsAString& aKey,
            PRBool aExcludeOfflineFromUsage,
            PRInt32 aKeyUsage);

  /**
    * Remove all keys belonging to this storage.
    */
  nsresult
  ClearStorage(nsDOMStorage* aStorage);

  /**
   * If we have changed the persistent storage, drop any potential session storages
   */
  nsresult
  DropStorage(nsDOMStorage* aStorage);

  /**
   * Removes all keys added by a given domain.
   */
  nsresult
  RemoveOwner(const nsACString& aOwner, PRBool aIncludeSubDomains);

  /**
   * Removes keys owned by domains that either match or don't match the
   * list.
   */
  nsresult
  RemoveOwners(const nsTArray<nsString>& aOwners,
               PRBool aIncludeSubDomains, PRBool aMatch);

  /**
   * Remove all values from all scopes not marked as offline that has been
   * created after the time specified with 'aSince'.  Used by the Clear Private
   * Data dialog.  'aSince' value is compatible with PR_Now() function.
   */
  nsresult
  RemoveTimeRange(PRInt64 aSince);

  /**
   * Removes all keys from storage. Used when clearing storage.
   */
  nsresult
  RemoveAll();

  /**
    * Returns usage for a storage using its GetQuotaDomainDBKey() as a key.
    */
  nsresult
  GetUsage(nsDOMStorage* aStorage, PRBool aExcludeOfflineFromUsage, PRInt32 *aUsage);

  /**
    * Returns usage of the domain and optionaly by any subdomain.
    */
  nsresult
  GetUsage(const nsACString& aDomain, PRBool aIncludeSubDomains, PRInt32 *aUsage);

protected:

  nsClassHashtable<nsCStringHashKey, nsInMemoryStorage> mData;
  nsDOMStoragePersistentDB* mPreloadDB;
  PRBool mPreloading;

  nsresult
  GetUsageInternal(const nsACString& aQuotaDomainDBKey, PRBool aExcludeOfflineFromUsage, PRInt32 *aUsage);
};

#endif
