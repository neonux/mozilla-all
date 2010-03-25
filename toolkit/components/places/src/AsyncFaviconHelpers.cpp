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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (original author)
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

#include "AsyncFaviconHelpers.h"
#include "mozilla/storage.h"
#include "nsNetUtil.h"

#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsFaviconService.h"

#define TO_CHARBUFFER(_buffer) \
  reinterpret_cast<char*>(const_cast<PRUint8*>(_buffer))
#define TO_INTBUFFER(_string) \
  reinterpret_cast<PRUint8*>(const_cast<char*>(_string.get()))

#ifdef DEBUG
#define INHERITED_ERROR_HANDLER \
  nsresult rv = AsyncStatementCallback::HandleError(aError); \
  NS_ENSURE_SUCCESS(rv, rv);
#else
#define INHERITED_ERROR_HANDLER /* nothing */
#endif

#define ASYNC_STATEMENT_HANDLEERROR_IMPL(_class) \
NS_IMETHODIMP \
_class::HandleError(mozIStorageError *aError) \
{ \
  INHERITED_ERROR_HANDLER \
  FAVICONSTEP_FAIL_IF_FALSE_RV(false, NS_OK); \
}

#define ASYNC_STATEMENT_EMPTY_HANDLERESULT_IMPL(_class) \
NS_IMETHODIMP \
_class::HandleResult(mozIStorageResultSet* aResultSet) \
{ \
  NS_NOTREACHED("Got an unexpected result?");\
  return NS_OK; \
}

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// AsyncFaviconStep

NS_IMPL_ISUPPORTS0(
  AsyncFaviconStep
)


////////////////////////////////////////////////////////////////////////////////
//// AsyncFaviconStepper

NS_IMPL_ISUPPORTS0(
  AsyncFaviconStepper
)


AsyncFaviconStepper::AsyncFaviconStepper(nsIFaviconDataCallback* aCallback)
  : mStepper(new AsyncFaviconStepperInternal(aCallback))
{
}


nsresult
AsyncFaviconStepper::Start()
{
  FAVICONSTEP_FAIL_IF_FALSE_RV(mStepper->mStatus == STEPPER_INITING,
                               NS_ERROR_FAILURE);
  mStepper->mStatus = STEPPER_RUNNING;
  nsresult rv = mStepper->Step();
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);
  return NS_OK;
}


nsresult
AsyncFaviconStepper::AppendStep(AsyncFaviconStep* aStep)
{
  FAVICONSTEP_FAIL_IF_FALSE_RV(aStep, NS_ERROR_OUT_OF_MEMORY);
  FAVICONSTEP_FAIL_IF_FALSE_RV(mStepper->mStatus == STEPPER_INITING,
                               NS_ERROR_FAILURE);

  aStep->SetStepper(mStepper);
  nsresult rv = mStepper->mSteps.AppendObject(aStep);
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);
  return NS_OK;
}


nsresult
AsyncFaviconStepper::SetIconData(const nsACString& aMimeType,
                                 const PRUint8* _data,
                                 PRUint32 _dataLen)
{
  mStepper->mMimeType = aMimeType;
  mStepper->mData.Adopt(TO_CHARBUFFER(_data), _dataLen);
  mStepper->mIconStatus |= ICON_STATUS_CHANGED;
  return NS_OK;
}


nsresult
AsyncFaviconStepper::GetIconData(nsACString& aMimeType,
                                 const PRUint8** aData,
                                 PRUint32* aDataLen)
{
  PRUint32 dataLen = mStepper->mData.Length();
  NS_ENSURE_TRUE(dataLen > 0, NS_ERROR_NOT_AVAILABLE);
  aMimeType = mStepper->mMimeType;
  *aDataLen = dataLen;
  *aData = TO_INTBUFFER(mStepper->mData);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
//// AsyncFaviconStepperInternal

NS_IMPL_ISUPPORTS0(
  AsyncFaviconStepperInternal
)


AsyncFaviconStepperInternal::AsyncFaviconStepperInternal(
  nsIFaviconDataCallback* aCallback
)
  : mCallback(aCallback)
  , mPageId(0)
  , mIconId(0)
  , mExpiration(0)
  , mIsRevisit(false)
  , mIconStatus(ICON_STATUS_UNKNOWN)
  , mStatus(STEPPER_INITING)
{
}


nsresult
AsyncFaviconStepperInternal::Step()
{
  if (mStatus != STEPPER_RUNNING) {
    Failure();
    return NS_ERROR_FAILURE;
  }

  PRInt32 stepCount = mSteps.Count();
  if (!stepCount) {
    mStatus = STEPPER_COMPLETED;
    // Ran all steps, let's notify.
    if (mCallback) {
      (void)mCallback->OnFaviconDataAvailable(mIconURI,
                                              mData.Length(),
                                              TO_INTBUFFER(mData),
                                              mMimeType);
    }
    return NS_OK;
  }

  // Get the next step.
  nsCOMPtr<AsyncFaviconStep> step = mSteps[0];
  if (!step) {
    Failure();
    return NS_ERROR_UNEXPECTED;
  }

  // Break the cycle.
  nsresult rv = mSteps.RemoveObjectAt(0);
  if (NS_FAILED(rv)) {
    Failure();
    return NS_ERROR_UNEXPECTED;
  }

  // Run the extracted step.
  step->Run();

  return NS_OK;
}


void
AsyncFaviconStepperInternal::Failure()
{
  mStatus = STEPPER_FAILED;

  // Break the cycles so steps are collected.
  mSteps.Clear();
}


void
AsyncFaviconStepperInternal::Cancel(bool aNotify)
{
  mStatus = STEPPER_CANCELED;

  // Break the cycles so steps are collected.
  mSteps.Clear();

  if (aNotify && mCallback) {
    (void)mCallback->OnFaviconDataAvailable(mIconURI,
                                            mData.Length(),
                                            TO_INTBUFFER(mData),
                                            mMimeType);
  }
}


////////////////////////////////////////////////////////////////////////////////
//// GetEffectivePageStep

NS_IMPL_ISUPPORTS_INHERITED0(
  GetEffectivePageStep
, AsyncFaviconStep
)
ASYNC_STATEMENT_HANDLEERROR_IMPL(GetEffectivePageStep)


GetEffectivePageStep::GetEffectivePageStep()
  : mSubStep(0)
  , mIsBookmarked(false)
{
}


void
GetEffectivePageStep::Run()
{
  NS_ASSERTION(mStepper, "Step is not associated to a stepper");
  FAVICONSTEP_FAIL_IF_FALSE(mStepper->mPageURI);
  FAVICONSTEP_FAIL_IF_FALSE(mStepper->mIconURI);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  FAVICONSTEP_FAIL_IF_FALSE(history);
  PRBool canAddToHistory;
  nsresult rv = history->CanAddURI(mStepper->mPageURI, &canAddToHistory);
  FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));

  // If history is disabled or the page isn't addable to history, only load
  // favicons if the page is bookmarked.
  if (!canAddToHistory || history->IsHistoryDisabled()) {
    // Get place id associated with this page.
    mozIStorageStatement* stmt = history->GetStatementById(DB_GET_PAGE_INFO);
    // Statement is null if we are shutting down.
    FAVICONSTEP_CANCEL_IF_TRUE(!stmt, PR_FALSE);
    mozStorageStatementScoper scoper(stmt);

    nsresult rv = BindStatementURI(stmt, 0, mStepper->mPageURI);
    FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));

    nsCOMPtr<mozIStoragePendingStatement> ps;
    rv = stmt->ExecuteAsync(this, getter_AddRefs(ps));
    FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));

    // ExecuteAsync will reset the statement for us.
    scoper.Abandon();
  }
  else {
    CheckPageAndProceed();
  }
}


NS_IMETHODIMP
GetEffectivePageStep::HandleResult(mozIStorageResultSet* aResultSet)
{
  nsCOMPtr<mozIStorageRow> row;
  nsresult rv = aResultSet->GetNextRow(getter_AddRefs(row));
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);

  if (mSubStep == 0) {
    rv = row->GetInt64(0, &mStepper->mPageId);
    FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);
  }
  else {
    NS_ASSERTION(mSubStep == 1, "Wrong sub-step?");
    nsCAutoString spec;
    rv = row->GetUTF8String(0, spec);
    FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);
    // We always want to use the bookmark uri.
    rv = mStepper->mPageURI->SetSpec(spec);
    FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);
    // Since we got a result, this is a bookmark.
    mIsBookmarked = true;
  }

  return NS_OK;
}


NS_IMETHODIMP
GetEffectivePageStep::HandleCompletion(PRUint16 aReason)
{
  FAVICONSTEP_FAIL_IF_FALSE_RV(aReason == mozIStorageStatementCallback::REASON_FINISHED, NS_OK);

  if (mSubStep == 0) {
    // Prepare for next sub step.
    mSubStep++;
    // If we have never seen this page, we don't want to go on.
    FAVICONSTEP_CANCEL_IF_TRUE_RV(mStepper->mPageId == 0, PR_FALSE, NS_OK);

    // Check if the page is bookmarked.
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    FAVICONSTEP_FAIL_IF_FALSE_RV(bookmarks, NS_ERROR_OUT_OF_MEMORY);
    mozIStorageStatement* stmt = bookmarks->GetStatementById(DB_FIND_REDIRECTED_BOOKMARK);
    // Statement is null if we are shutting down.
    FAVICONSTEP_CANCEL_IF_TRUE_RV(!stmt, PR_FALSE, NS_OK);
    mozStorageStatementScoper scoper(stmt);

    nsresult rv = stmt->BindInt64Parameter(0, mStepper->mPageId);
    FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);

    nsCOMPtr<mozIStoragePendingStatement> ps;
    rv = stmt->ExecuteAsync(this, getter_AddRefs(ps));
    FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);

    // ExecuteAsync will reset the statement for us.
    scoper.Abandon();
  }
  else {
    NS_ASSERTION(mSubStep == 1, "Wrong sub-step?");
    // If the page is not bookmarked, we don't want to go on.
    FAVICONSTEP_CANCEL_IF_TRUE_RV(!mIsBookmarked, PR_FALSE, NS_OK);

    CheckPageAndProceed();
  }

  return NS_OK;
}


void
GetEffectivePageStep::CheckPageAndProceed()
{
  // If pageURI is an image, the favicon URI will be the same as the page URI.
  // TODO: In future we could store a resample of the image, but
  // for now we just avoid that, for database size concerns.
  PRBool pageEqualsFavicon;
  nsresult rv = mStepper->mPageURI->Equals(mStepper->mIconURI,
                                           &pageEqualsFavicon);
  FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));
  FAVICONSTEP_CANCEL_IF_TRUE(pageEqualsFavicon, PR_FALSE);

  // Don't store favicons to error pages.
  nsCOMPtr<nsIURI> errorPageFaviconURI;
  rv = NS_NewURI(getter_AddRefs(errorPageFaviconURI),
                 NS_LITERAL_CSTRING(FAVICON_ERRORPAGE_URL));
  FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));
  PRBool isErrorPage;
  rv = mStepper->mIconURI->Equals(errorPageFaviconURI, &isErrorPage);
  FAVICONSTEP_CANCEL_IF_TRUE(isErrorPage, PR_FALSE);

  // Proceed to next step.
  rv = mStepper->Step();
  FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));
}


////////////////////////////////////////////////////////////////////////////////
//// FetchDatabaseIconStep

NS_IMPL_ISUPPORTS_INHERITED0(
  FetchDatabaseIconStep
, AsyncFaviconStep
)
ASYNC_STATEMENT_HANDLEERROR_IMPL(FetchDatabaseIconStep)


void
FetchDatabaseIconStep::Run()
{
  NS_ASSERTION(mStepper, "Step is not associated to a stepper");
  FAVICONSTEP_FAIL_IF_FALSE(mStepper->mIconURI);

  // Check if this favicon exists in the database and get associated
  // information.
  nsFaviconService* fs = nsFaviconService::GetFaviconService();
  FAVICONSTEP_FAIL_IF_FALSE(fs);
  mozIStorageStatement* stmt =
    fs->GetStatementById(mozilla::places::DB_GET_ICON_INFO_WITH_PAGE);
  FAVICONSTEP_CANCEL_IF_TRUE(!stmt, PR_FALSE);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = BindStatementURI(stmt, 0, mStepper->mIconURI);
  FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));
  if (mStepper->mPageURI) {
    rv = BindStatementURI(stmt, 1, mStepper->mPageURI);
  }
  else {
    rv = stmt->BindNullParameter(1);
  }
  FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));

  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = stmt->ExecuteAsync(this, getter_AddRefs(ps));
  FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));

  // ExecuteAsync will reset the statement for us.
  scoper.Abandon();
}


NS_IMETHODIMP
FetchDatabaseIconStep::HandleResult(mozIStorageResultSet* aResultSet)
{
  nsCOMPtr<mozIStorageRow> row;
  nsresult rv = aResultSet->GetNextRow(getter_AddRefs(row));
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);

  rv = row->GetInt64(0, &mStepper->mIconId);
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);

  // Don't need to fetch dataLen (index 1) since it will be read with data, it
  // is in the query to mimic mDBGetIconInfo.
  // Indeed in future we could want to retain only one statement.

  rv = row->GetInt64(2, &mStepper->mExpiration);
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);

  PRUint8* data;
  PRUint32 dataLen = 0;
  rv = row->GetBlob(3, &dataLen, &data);
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);
  mStepper->mData.Adopt(TO_CHARBUFFER(data), dataLen);
  rv = row->GetUTF8String(4, mStepper->mMimeType);
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);

  PRInt32 isRevisit;
  rv = row->GetInt32(5, &isRevisit);
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);
  mStepper->mIsRevisit = !!isRevisit;

  return NS_OK;
}


NS_IMETHODIMP
FetchDatabaseIconStep::HandleCompletion(PRUint16 aReason)
{
  FAVICONSTEP_FAIL_IF_FALSE_RV(aReason == mozIStorageStatementCallback::REASON_FINISHED, NS_OK);

  // Proceed to next step.
  nsresult rv = mStepper->Step();
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
//// EnsureDatabaseEntryStep

NS_IMPL_ISUPPORTS_INHERITED0(
  EnsureDatabaseEntryStep
, AsyncFaviconStep
)
ASYNC_STATEMENT_HANDLEERROR_IMPL(EnsureDatabaseEntryStep)
ASYNC_STATEMENT_EMPTY_HANDLERESULT_IMPL(EnsureDatabaseEntryStep)


void
EnsureDatabaseEntryStep::Run()
{
  NS_ASSERTION(mStepper, "Step is not associated to a stepper");
  FAVICONSTEP_FAIL_IF_FALSE(mStepper->mIconURI);
  nsresult rv;

  // If the icon entry already exists, just proceed to next step.
  if (mStepper->mIconId > 0 || mStepper->mIsRevisit) {
    rv = mStepper->Step();
    FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));
    return;
  }

  // Insert a new icon entry in the database.
  nsFaviconService* fs = nsFaviconService::GetFaviconService();
  FAVICONSTEP_FAIL_IF_FALSE(fs);
  mozIStorageStatement* stmt =
    fs->GetStatementById(mozilla::places::DB_INSERT_ICON);
  // Statement is null if we are shutting down.
  FAVICONSTEP_CANCEL_IF_TRUE(!stmt, PR_FALSE);
  mozStorageStatementScoper scoper(stmt);
  rv = BindStatementURI(stmt, 0, mStepper->mIconURI);
  FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));

  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = stmt->ExecuteAsync(this, getter_AddRefs(ps));
  FAVICONSTEP_FAIL_IF_FALSE(NS_SUCCEEDED(rv));

  // ExecuteAsync will reset the statement for us.
  scoper.Abandon();
}


NS_IMETHODIMP
EnsureDatabaseEntryStep::HandleCompletion(PRUint16 aReason)
{
  FAVICONSTEP_FAIL_IF_FALSE_RV(aReason == mozIStorageStatementCallback::REASON_FINISHED, NS_OK);

  // Proceed to next step.
  nsresult rv = mStepper->Step();
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////

} // namespace places
} // namespace mozilla
