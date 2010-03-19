/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifdef MOZ_IPC
#include "mozilla/dom/ContentProcessChild.h"
#include "mozilla/dom/ContentProcessParent.h"
#endif

#include "History.h"
#include "nsNavHistory.h"

#include "mozilla/storage.h"
#include "mozilla/dom/Link.h"
#include "nsDocShellCID.h"
#include "nsIEventStateManager.h"

using namespace mozilla::dom;

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// Global Defines

#define URI_VISITED "visited"
#define URI_NOT_VISITED "not visited"
#define URI_VISITED_RESOLUTION_TOPIC "visited-status-resolution"

////////////////////////////////////////////////////////////////////////////////
//// Anonymous Helpers

namespace {

class VisitedQuery : public mozIStorageStatementCallback
{
public:
  NS_DECL_ISUPPORTS

  static nsresult Start(nsIURI* aURI)
  {
    NS_ASSERTION(aURI, "Don't pass a null URI!");

    nsNavHistory* navHist = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(navHist, NS_ERROR_FAILURE);
    mozIStorageStatement* stmt = navHist->DBGetIsVisited();
    NS_ENSURE_STATE(stmt);

    // Be sure to reset our statement!
    mozStorageStatementScoper scoper(stmt);
    nsCString spec;
    nsresult rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = BindStatementURLCString(stmt, 0, spec);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<VisitedQuery> callback = new VisitedQuery(aURI);
    NS_ENSURE_TRUE(callback, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<mozIStoragePendingStatement> handle;
    return stmt->ExecuteAsync(callback, getter_AddRefs(handle));
  }

  NS_IMETHOD HandleResult(mozIStorageResultSet* aResults)
  {
    // If this method is called, we've gotten results, which means we have a
    // visit.
    mIsVisited = true;
    return NS_OK;
  }

  NS_IMETHOD HandleError(mozIStorageError* aError)
  {
    // mIsVisited is already set to false, and that's the assumption we will
    // make if an error occurred.
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(PRUint16 aReason)
  {
    History::GetService()->NotifyVisited(mURI, mIsVisited);
    return NS_OK;
  }
private:
  VisitedQuery(nsIURI* aURI)
  : mURI(aURI)
  , mIsVisited(false)
  {
  }

  nsCOMPtr<nsIURI> mURI;
  bool mIsVisited;
};
NS_IMPL_ISUPPORTS1(
  VisitedQuery,
  mozIStorageStatementCallback
)

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//// History

History* History::gService = NULL;

History::History()
{
  NS_ASSERTION(!gService, "Ruh-roh!  This service has already been created!");
  gService = this;
}

History::~History()
{
  gService = NULL;
#ifdef DEBUG
  if (mObservers.IsInitialized()) {
    NS_ASSERTION(mObservers.Count() == 0,
                 "Not all Links were removed before we disappear!");
  }
#endif
}

void
History::NotifyVisited(nsIURI* aURI, bool aIsVisited)
{
  NS_ASSERTION(aURI, "Ruh-roh!  A NULL URI was passed to us!");

#ifdef MOZ_IPC
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    mozilla::dom::ContentProcessParent * cpp = 
        mozilla::dom::ContentProcessParent::GetSingleton();
    NS_ASSERTION(cpp, "Content Protocol is NULL!");

    nsCString aURISpec;
    aURI->GetSpec(aURISpec);
    cpp->SendNotifyVisited(aURISpec, aIsVisited);
  }
#endif

  if (aIsVisited) {
    // If the hash table has not been initialized, then we have nothing to notify
    // about.
    if (!mObservers.IsInitialized()) {
      return;
    }

    // Additionally, if we have no observers for this URI, we have nothing to
    // notify about.
    KeyClass* key = mObservers.GetEntry(aURI);
    if (!key) {
      return;
    }

    // Walk through the array, and update each Link node.
    const ObserverArray& observers = key->array;
    ObserverArray::index_type len = observers.Length();
    for (ObserverArray::index_type i = 0; i < len; i++) {
      Link* link = observers[i];
      link->SetLinkState(eLinkState_Visited);
      NS_ASSERTION(len == observers.Length(),
                   "Calling SetLinkState added or removed an observer!");
    }

  // All the registered nodes can now be removed for this URI.
  mObservers.RemoveEntry(aURI);
  }

  // Notify any observers about that we have resolved the visited state of
  // this URI.
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    nsAutoString status;
    if (aIsVisited) {
      status.AssignLiteral(URI_VISITED);
    }
    else {
      status.AssignLiteral(URI_NOT_VISITED);
    }
    (void)observerService->NotifyObservers(aURI,
      URI_VISITED_RESOLUTION_TOPIC,
      status.get());
  }

}

/* static */
History*
History::GetService()
{
  if (gService) {
    return gService;
  }

  nsCOMPtr<IHistory> service(do_GetService(NS_IHISTORY_CONTRACTID));
  NS_ABORT_IF_FALSE(service, "Cannot obtain IHistory service!");
  NS_ASSERTION(gService, "Our constructor was not run?!");

  return gService;
}

/* static */
History*
History::GetSingleton()
{
  if (!gService) {
    gService = new History();
    NS_ENSURE_TRUE(gService, nsnull);
  }

  NS_ADDREF(gService);
  return gService;
}

////////////////////////////////////////////////////////////////////////////////
//// IHistory

NS_IMETHODIMP
History::RegisterVisitedCallback(nsIURI* aURI,
                                 Link* aLink)
{
  nsresult   rv;

#ifdef MOZ_IPC
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
      rv = VisitedQuery::Start(aURI);
      return rv;
  }
#endif

  NS_ASSERTION(aURI, "Must pass a non-null URI!");
  NS_ASSERTION(aLink, "Must pass a non-null Link object!");

  // First, ensure that our hash table is setup.
  if (!mObservers.IsInitialized()) {
    NS_ENSURE_TRUE(mObservers.Init(), NS_ERROR_OUT_OF_MEMORY);
  }

  // Obtain our array of observers for this URI.
#ifdef DEBUG
  bool keyAlreadyExists = !!mObservers.GetEntry(aURI);
#endif
  KeyClass* key = mObservers.PutEntry(aURI);
  NS_ENSURE_TRUE(key, NS_ERROR_OUT_OF_MEMORY);
  ObserverArray& observers = key->array;

  if (observers.IsEmpty()) {
    NS_ASSERTION(!keyAlreadyExists,
                 "An empty key was kept around in our hashtable!");

    // We are the first Link node to ask about this URI, or there are no pending
    // Links wanting to know about this URI.  Therefore, we should query the
    // database now.
#ifdef MOZ_IPC
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    mozilla::dom::ContentProcessChild * cpc = 
        mozilla::dom::ContentProcessChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");

    nsCString aURISpec;
    aURI->GetSpec(aURISpec);
    cpc->SendStartVisitedQuery(aURISpec, &rv);
  }
#else
    rv = VisitedQuery::Start(aURI);
#endif
    if (NS_FAILED(rv)) {
      // Remove our array from the hashtable so we don't keep it around.
      mObservers.RemoveEntry(aURI);
      return rv;
    }
  }

  // Sanity check that Links are not registered more than once for a given URI.
  // This will not catch a case where it is registered for two different URIs.
  NS_ASSERTION(!observers.Contains(aLink),
               "Already tracking this Link object!");

  // Start tracking our Link.
  if (!observers.AppendElement(aLink)) {
    // Curses - unregister and return failure.
    (void)UnregisterVisitedCallback(aURI, aLink);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
History::UnregisterVisitedCallback(nsIURI* aURI,
                                   Link* aLink)
{
  NS_ASSERTION(aURI, "Must pass a non-null URI!");
  NS_ASSERTION(aLink, "Must pass a non-null Link object!");

  // Get the array, and remove the item from it.
  KeyClass* key = mObservers.GetEntry(aURI);
  if (!key) {
    NS_ERROR("Trying to unregister for a URI that wasn't registered!");
    return NS_ERROR_UNEXPECTED;
  }
  ObserverArray& observers = key->array;
  if (!observers.RemoveElement(aLink)) {
    NS_ERROR("Trying to unregister a node that wasn't registered!");
    return NS_ERROR_UNEXPECTED;
  }

  // If the array is now empty, we should remove it from the hashtable.
  if (observers.IsEmpty()) {
    mObservers.RemoveEntry(aURI);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsISupports

NS_IMPL_ISUPPORTS1(
  History,
  IHistory
)

} // namespace places
} // namespace mozilla
