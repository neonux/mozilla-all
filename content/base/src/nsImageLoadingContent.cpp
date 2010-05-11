/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
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
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christian Biesinger <cbiesinger@web.de>
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

/*
 * A base class which implements nsIImageLoadingContent and can be
 * subclassed by various content nodes that want to provide image
 * loading functionality (eg <img>, <object>, etc).
 */

#include "nsImageLoadingContent.h"
#include "nsAutoPtr.h"
#include "nsContentErrors.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "imgIContainer.h"
#include "imgILoader.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsPLDOMEvent.h"

#include "nsIPresShell.h"
#include "nsIEventStateManager.h"
#include "nsGUIEvent.h"

#include "nsIChannel.h"
#include "nsIStreamListener.h"

#include "nsIFrame.h"
#include "nsIDOMNode.h"

#include "nsContentUtils.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsEventDispatcher.h"
#include "nsDOMClassInfo.h"

#include "mozAutoDocUpdate.h"

#ifdef DEBUG_chb
static void PrintReqURL(imgIRequest* req) {
  if (!req) {
    printf("(null req)\n");
    return;
  }

  nsCOMPtr<nsIURI> uri;
  req->GetURI(getter_AddRefs(uri));
  if (!uri) {
    printf("(null uri)\n");
    return;
  }

  nsCAutoString spec;
  uri->GetSpec(spec);
  printf("spec='%s'\n", spec.get());
}
#endif /* DEBUG_chb */


nsImageLoadingContent::nsImageLoadingContent()
  : mObserverList(nsnull),
    mImageBlockingStatus(nsIContentPolicy::ACCEPT),
    mLoadingEnabled(PR_TRUE),
    mStartingLoad(PR_FALSE),
    mIsImageStateForced(PR_FALSE),
    mLoading(PR_FALSE),
    // mBroken starts out true, since an image without a URI is broken....
    mBroken(PR_TRUE),
    mUserDisabled(PR_FALSE),
    mSuppressed(PR_FALSE),
    mBlockingOnload(PR_FALSE)
{
  if (!nsContentUtils::GetImgLoader()) {
    mLoadingEnabled = PR_FALSE;
  }
}

void
nsImageLoadingContent::DestroyImageLoadingContent()
{
  // If we're blocking onload for any reason, now's a good time to stop
  SetBlockingOnload(PR_FALSE);

  // Cancel our requests so they won't hold stale refs to us
  if (mCurrentRequest) {
    mCurrentRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
    mCurrentRequest = nsnull;
  }
  if (mPendingRequest) {
    mPendingRequest->CancelAndForgetObserver(NS_ERROR_FAILURE);
    mPendingRequest = nsnull;
  }
}

nsImageLoadingContent::~nsImageLoadingContent()
{
  NS_ASSERTION(!mCurrentRequest && !mPendingRequest,
               "DestroyImageLoadingContent not called");
  NS_ASSERTION(!mObserverList.mObserver && !mObserverList.mNext,
               "Observers still registered?");
}

// Macro to call some func on each observer.  This handles observers
// removing themselves.
#define LOOP_OVER_OBSERVERS(func_)                                       \
  PR_BEGIN_MACRO                                                         \
    for (ImageObserver* observer = &mObserverList, *next; observer;      \
         observer = next) {                                              \
      next = observer->mNext;                                            \
      if (observer->mObserver) {                                         \
        observer->mObserver->func_;                                      \
      }                                                                  \
    }                                                                    \
  PR_END_MACRO


/*
 * imgIContainerObserver impl
 */
NS_IMETHODIMP
nsImageLoadingContent::FrameChanged(imgIContainer* aContainer,
                                    nsIntRect* aDirtyRect)
{
  LOOP_OVER_OBSERVERS(FrameChanged(aContainer, aDirtyRect));
  return NS_OK;
}
            
/*
 * imgIDecoderObserver impl
 */
NS_IMETHODIMP
nsImageLoadingContent::OnStartRequest(imgIRequest* aRequest)
{
  LOOP_OVER_OBSERVERS(OnStartRequest(aRequest));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStartDecode(imgIRequest* aRequest)
{
  // Block onload if it's the current request
  if (aRequest == mCurrentRequest) {
    NS_ABORT_IF_FALSE(!mBlockingOnload, "Shouldn't already be blocking");
    SetBlockingOnload(PR_TRUE);
  }

  LOOP_OVER_OBSERVERS(OnStartDecode(aRequest));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStartContainer(imgIRequest* aRequest,
                                        imgIContainer* aContainer)
{
  LOOP_OVER_OBSERVERS(OnStartContainer(aRequest, aContainer));

  // Have to check for state changes here, since we might have been in
  // the LOADING state before.
  UpdateImageState(PR_TRUE);
  return NS_OK;    
}

NS_IMETHODIMP
nsImageLoadingContent::OnStartFrame(imgIRequest* aRequest,
                                    PRUint32 aFrame)
{
  LOOP_OVER_OBSERVERS(OnStartFrame(aRequest, aFrame));
  return NS_OK;    
}

NS_IMETHODIMP
nsImageLoadingContent::OnDataAvailable(imgIRequest* aRequest,
                                       PRBool aCurrentFrame,
                                       const nsIntRect* aRect)
{
  LOOP_OVER_OBSERVERS(OnDataAvailable(aRequest, aCurrentFrame, aRect));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopFrame(imgIRequest* aRequest,
                                   PRUint32 aFrame)
{
  // If we're blocking a load, one frame is enough
  if (aRequest == mCurrentRequest)
    SetBlockingOnload(PR_FALSE);

  LOOP_OVER_OBSERVERS(OnStopFrame(aRequest, aFrame));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopContainer(imgIRequest* aRequest,
                                       imgIContainer* aContainer)
{
  // This is really hacky. We need to handle the case where we start decoding,
  // block onload, but then hit an error before we get to our first frame. In
  // theory we would just hook in at OnStopDecode, but OnStopDecode is broken
  // until we fix bug 505385. OnStopContainer is actually going away at that
  // point. So for now we take advantage of the fact that OnStopContainer is
  // always fired in the decoders at the same time as OnStopDecode.
  if (aRequest == mCurrentRequest)
    SetBlockingOnload(PR_FALSE);

  LOOP_OVER_OBSERVERS(OnStopContainer(aRequest, aContainer));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopDecode(imgIRequest* aRequest,
                                    nsresult aStatus,
                                    const PRUnichar* aStatusArg)
{
  // We should definitely have a request here
  NS_ABORT_IF_FALSE(aRequest, "no request?");

  NS_PRECONDITION(aRequest == mCurrentRequest || aRequest == mPendingRequest,
                  "Unknown request");
  LOOP_OVER_OBSERVERS(OnStopDecode(aRequest, aStatus, aStatusArg));

  if (aRequest == mPendingRequest) {

    // If we were blocking for the soon-to-be-obsolete request, stop doing so
    SetBlockingOnload(PR_FALSE);

    // The new image is decoded - switch to it
    // XXXbholley - This is technically not true pre bug 505385, but I don't
    // think it's a big enough issue to worry about handling in the mean time
    mCurrentRequest->Cancel(NS_ERROR_IMAGE_SRC_CHANGED);
    mPendingRequest.swap(mCurrentRequest);
    mPendingRequest = nsnull;
  }

  // XXXbholley - When we fix bug 505385,  this should go in OnStopRequest.
  //
  // We just loaded all the data we're going to get. If we haven't done an
  // initial paint, we want to make sure the image starts decoding for 2
  // reasons:
  //
  // 1) This image is sitting idle but might need to be decoded as soon as we
  // start painting, in which case we've wasted time.
  //
  // 2) We want to block onload until all visible images are decoded. We do this
  // by blocking onload until all in progress decodes get at least one frame
  // decoded. However, if all the data comes in while painting is suppressed
  // (ie, before the initial paint delay is finished), we fire onload without
  // doing a paint first. This means that decode-on-draw images don't start
  // decoding, so we can't wait for them to finish. See bug 512435.

  // We can only do this if we have a presshell
  nsIDocument* doc = GetOurDocument();
  nsIPresShell* shell = doc ? doc->GetPrimaryShell() : nsnull;
  if (shell) {

    // We need to figure out whether to kick off decoding
    PRBool doRequestDecode = PR_FALSE;

    // If we haven't got the initial reflow yet, IsPaintingSuppressed actually
    // returns false
    if (!shell->DidInitialReflow())
      doRequestDecode = PR_TRUE;

    // Figure out if painting is suppressed. Note that it's possible for painting
    // to be suppressed for reasons other than the initial paint delay (for
    // example - being in the bfcache), but we probably aren't loading images in
    // those situations.
    if (shell->IsPaintingSuppressed())
      doRequestDecode = PR_TRUE;

    // If we're requesting a decode, do it
    if (doRequestDecode)
      aRequest->RequestDecode();
  }

  // XXXldb What's the difference between when OnStopDecode and OnStopRequest
  // fire?  Should we do this work there instead?  Should they just be the
  // same?

  if (NS_SUCCEEDED(aStatus)) {
    FireEvent(NS_LITERAL_STRING("load"));
  } else {
    FireEvent(NS_LITERAL_STRING("error"));
  }

  // Have to check for state changes here (for example, the new load could
  // have resulted in a broken image).  Note that we don't want to do this
  // async, unlike the event, because while this is waiting to happen our
  // state could change yet again, and then we'll get confused about our
  // state.
  UpdateImageState(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopRequest(imgIRequest* aRequest, PRBool aLastPart)
{
  LOOP_OVER_OBSERVERS(OnStopRequest(aRequest, aLastPart));

  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnDiscard(imgIRequest *aRequest)
{
  LOOP_OVER_OBSERVERS(OnDiscard(aRequest));

  return NS_OK;
}

/*
 * nsIImageLoadingContent impl
 */

NS_IMETHODIMP
nsImageLoadingContent::GetLoadingEnabled(PRBool *aLoadingEnabled)
{
  *aLoadingEnabled = mLoadingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::SetLoadingEnabled(PRBool aLoadingEnabled)
{
  if (nsContentUtils::GetImgLoader()) {
    mLoadingEnabled = aLoadingEnabled;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::GetImageBlockingStatus(PRInt16* aStatus)
{
  NS_PRECONDITION(aStatus, "Null out param");
  *aStatus = mImageBlockingStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::AddObserver(imgIDecoderObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (!mObserverList.mObserver) {
    mObserverList.mObserver = aObserver;
    // Don't touch the linking of the list!
    return NS_OK;
  }

  // otherwise we have to create a new entry

  ImageObserver* observer = &mObserverList;
  while (observer->mNext) {
    observer = observer->mNext;
  }

  observer->mNext = new ImageObserver(aObserver);
  if (! observer->mNext) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::RemoveObserver(imgIDecoderObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (mObserverList.mObserver == aObserver) {
    mObserverList.mObserver = nsnull;
    // Don't touch the linking of the list!
    return NS_OK;
  }

  // otherwise have to find it and splice it out
  ImageObserver* observer = &mObserverList;
  while (observer->mNext && observer->mNext->mObserver != aObserver) {
    observer = observer->mNext;
  }

  // At this point, we are pointing to the list element whose mNext is
  // the right observer (assuming of course that mNext is not null)
  if (observer->mNext) {
    // splice it out
    ImageObserver* oldObserver = observer->mNext;
    observer->mNext = oldObserver->mNext;
    oldObserver->mNext = nsnull;  // so we don't destroy them all
    delete oldObserver;
  }
#ifdef DEBUG
  else {
    NS_WARNING("Asked to remove non-existent observer");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::GetRequest(PRInt32 aRequestType,
                                  imgIRequest** aRequest)
{
  switch(aRequestType) {
  case CURRENT_REQUEST:
    *aRequest = mCurrentRequest;
    break;
  case PENDING_REQUEST:
    *aRequest = mPendingRequest;
    break;
  default:
    NS_ERROR("Unknown request type");
    *aRequest = nsnull;
    return NS_ERROR_UNEXPECTED;
  }
  
  NS_IF_ADDREF(*aRequest);
  return NS_OK;
}


NS_IMETHODIMP
nsImageLoadingContent::GetRequestType(imgIRequest* aRequest,
                                      PRInt32* aRequestType)
{
  NS_PRECONDITION(aRequestType, "Null out param");
  
  if (aRequest == mCurrentRequest) {
    *aRequestType = CURRENT_REQUEST;
    return NS_OK;
  }

  if (aRequest == mPendingRequest) {
    *aRequestType = PENDING_REQUEST;
    return NS_OK;
  }

  *aRequestType = UNKNOWN_REQUEST;
  NS_ERROR("Unknown request");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsImageLoadingContent::GetCurrentURI(nsIURI** aURI)
{
  if (mCurrentRequest) {
    return mCurrentRequest->GetURI(aURI);
  }

  if (!mCurrentURI) {
    *aURI = nsnull;
    return NS_OK;
  }
  
  return NS_EnsureSafeToReturn(mCurrentURI, aURI);
}

NS_IMETHODIMP
nsImageLoadingContent::LoadImageWithChannel(nsIChannel* aChannel,
                                            nsIStreamListener** aListener)
{
  NS_PRECONDITION(aListener, "null out param");
  
  NS_ENSURE_ARG_POINTER(aChannel);

  if (!nsContentUtils::GetImgLoader()) {
    return NS_ERROR_NULL_POINTER;
  }

  // XXX what should we do with content policies here, if anything?
  // Shouldn't that be done before the start of the load?
  // XXX what about shouldProcess?
  
  nsCOMPtr<nsIDocument> doc = GetOurDocument();
  if (!doc) {
    // Don't bother
    return NS_OK;
  }

  // Null out our mCurrentURI, in case we have no image requests right now.
  mCurrentURI = nsnull;
  
  CancelImageRequests(NS_ERROR_IMAGE_SRC_CHANGED, PR_FALSE,
                      nsIContentPolicy::ACCEPT);

  nsCOMPtr<imgIRequest> & req = mCurrentRequest ? mPendingRequest : mCurrentRequest;

  nsresult rv = nsContentUtils::GetImgLoader()->
    LoadImageWithChannel(aChannel, this, doc, aListener, getter_AddRefs(req));

  // Make sure our state is up to date
  UpdateImageState(PR_TRUE);

  return rv;
}

NS_IMETHODIMP nsImageLoadingContent::ForceReload()
{
  nsCOMPtr<nsIURI> currentURI;
  GetCurrentURI(getter_AddRefs(currentURI));
  if (!currentURI) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return LoadImage(currentURI, PR_TRUE, PR_TRUE, nsnull, nsIRequest::VALIDATE_ALWAYS);
}

/*
 * Non-interface methods
 */

nsresult
nsImageLoadingContent::LoadImage(const nsAString& aNewURI,
                                 PRBool aForce,
                                 PRBool aNotify)
{
  // First, get a document (needed for security checks and the like)
  nsIDocument* doc = GetOurDocument();
  if (!doc) {
    // No reason to bother, I think...
    return NS_OK;
  }

  nsCOMPtr<nsIURI> imageURI;
  nsresult rv = StringToURI(aNewURI, doc, getter_AddRefs(imageURI));
  NS_ENSURE_SUCCESS(rv, rv);
  // XXXbiesi fire onerror if that failed?

  PRBool equal;

  if (aNewURI.IsEmpty() &&
      doc->GetDocumentURI() &&
      NS_SUCCEEDED(doc->GetDocumentURI()->Equals(imageURI, &equal)) && 
      equal)  {

    // Loading an embedded img from the same URI as the document URI will not work
    // as a resource cannot recursively embed itself. Attempting to do so generally
    // results in having to pre-emptively close down an in-flight HTTP transaction 
    // and then incurring the significant cost of establishing a new TCP channel.
    // This is generally triggered from <img src=""> 
    // In light of that, just skip loading it..
    // Do make sure to drop our existing image, if any
    CancelImageRequests(aNotify);
    return NS_OK;
  }

  NS_TryToSetImmutable(imageURI);

  return LoadImage(imageURI, aForce, aNotify, doc);
}

nsresult
nsImageLoadingContent::LoadImage(nsIURI* aNewURI,
                                 PRBool aForce,
                                 PRBool aNotify,
                                 nsIDocument* aDocument,
                                 nsLoadFlags aLoadFlags)
{
  if (!mLoadingEnabled) {
    // XXX Why fire an error here? seems like the callers to SetLoadingEnabled
    // don't want/need it.
    FireEvent(NS_LITERAL_STRING("error"));
    return NS_OK;
  }

  NS_ASSERTION(!aDocument || aDocument == GetOurDocument(),
               "Bogus document passed in");
  // First, get a document (needed for security checks and the like)
  if (!aDocument) {
    aDocument = GetOurDocument();
    if (!aDocument) {
      // No reason to bother, I think...
      return NS_OK;
    }
  }


  nsresult rv;   // XXXbz Should failures in this method fire onerror?

  // Skip the URI equality check if our current image was blocked.  If
  // that happened, we really do want to try loading again.
  if (!aForce && NS_CP_ACCEPTED(mImageBlockingStatus)) {
    nsCOMPtr<nsIURI> currentURI;
    GetCurrentURI(getter_AddRefs(currentURI));
    PRBool equal;
    if (currentURI &&
        NS_SUCCEEDED(currentURI->Equals(aNewURI, &equal)) &&
        equal) {
      // Nothing to do here.
      return NS_OK;
    }
  }

  // From this point on, our state could change before return, so make
  // sure to notify if it does.
  AutoStateChanger changer(this, aNotify);

  // Use the principal of aDocument to avoid having to QI |this| an extra time.
  // It should be the same as the principal of this node in any case.
#ifdef DEBUG
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this);
  NS_ASSERTION(thisContent &&
               thisContent->NodePrincipal() == aDocument->NodePrincipal(),
               "Principal mismatch?");
#endif
  
  // If we'll be loading a new image, we want to cancel our existing
  // requests; the question is what reason to pass in.  If everything
  // is going smoothly, that reason should be
  // NS_ERROR_IMAGE_SRC_CHANGED so that our frame (if any) will know
  // not to show the broken image icon.  If the load is blocked by the
  // content policy or security manager, we will want to cancel with
  // the error code from those.

  PRInt16 newImageStatus;
  PRBool loadImage = nsContentUtils::CanLoadImage(aNewURI, this, aDocument,
                                                  aDocument->NodePrincipal(),
                                                  &newImageStatus);
  NS_ASSERTION(loadImage || !NS_CP_ACCEPTED(newImageStatus),
               "CanLoadImage lied");

  nsresult cancelResult = loadImage ? NS_ERROR_IMAGE_SRC_CHANGED
                                    : NS_ERROR_IMAGE_BLOCKED;

  CancelImageRequests(cancelResult, PR_FALSE, newImageStatus);

  // Remember the URL of this request, in case someone asks us for it later.
  // But this only matters if we are affecting the current request.  Need to do
  // this after CancelImageRequests, since that affects the value of
  // mCurrentRequest.
  if (!mCurrentRequest) {
    mCurrentURI = aNewURI;
  }
  
  if (!loadImage) {
    // Don't actually load anything!  This was blocked by CanLoadImage.
    FireEvent(NS_LITERAL_STRING("error"));
    return NS_OK;
  }

  nsCOMPtr<imgIRequest> & req = mCurrentRequest ? mPendingRequest : mCurrentRequest;

  rv = nsContentUtils::LoadImage(aNewURI, aDocument,
                                 aDocument->NodePrincipal(),
                                 aDocument->GetDocumentURI(),
                                 this, aLoadFlags,
                                 getter_AddRefs(req));
  if (NS_FAILED(rv)) {
    FireEvent(NS_LITERAL_STRING("error"));
    return NS_OK;
  }

  // If we now have a current request, we don't need to store the URI, since
  // we can get it off the request. Release it.
  if (mCurrentRequest) {
    mCurrentURI = nsnull;
  }

  return NS_OK;
}

nsresult
nsImageLoadingContent::ForceImageState(PRBool aForce, PRInt32 aState)
{
  mIsImageStateForced = aForce;
  mForcedImageState = aState;
  return NS_OK;
}

PRInt32
nsImageLoadingContent::ImageState() const
{
  return mIsImageStateForced ? mForcedImageState :
    (mBroken * NS_EVENT_STATE_BROKEN) |
    (mUserDisabled * NS_EVENT_STATE_USERDISABLED) |
    (mSuppressed * NS_EVENT_STATE_SUPPRESSED) |
    (mLoading * NS_EVENT_STATE_LOADING);
}

void
nsImageLoadingContent::UpdateImageState(PRBool aNotify)
{
  if (mStartingLoad) {
    // Ignore this call; we'll update our state when the state changer is
    // destroyed.  Need this to work around the fact that some libpr0n stuff is
    // actually sync and hence we can get OnStopDecode called while we're still
    // under LoadImage, and OnStopDecode doesn't know anything about
    // aNotify
    return;
  }
  
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this);
  if (!thisContent) {
    return;
  }

  PRInt32 oldState = ImageState();

  mLoading = mBroken = mUserDisabled = mSuppressed = PR_FALSE;
  
  // If we were blocked by server-based content policy, we claim to be
  // suppressed.  If we were blocked by type-based content policy, we claim to
  // be user-disabled.  Otherwise, claim to be broken.
  if (mImageBlockingStatus == nsIContentPolicy::REJECT_SERVER) {
    mSuppressed = PR_TRUE;
  } else if (mImageBlockingStatus == nsIContentPolicy::REJECT_TYPE) {
    mUserDisabled = PR_TRUE;
  } else if (!mCurrentRequest) {
    // No current request means error, since we weren't disabled or suppressed
    mBroken = PR_TRUE;
  } else {
    PRUint32 currentLoadStatus;
    nsresult rv = mCurrentRequest->GetImageStatus(&currentLoadStatus);
    if (NS_FAILED(rv) || (currentLoadStatus & imgIRequest::STATUS_ERROR)) {
      mBroken = PR_TRUE;
    } else if (!(currentLoadStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
      mLoading = PR_TRUE;
    }
  }

  if (aNotify) {
    nsIDocument* doc = thisContent->GetCurrentDoc();
    if (doc) {
      NS_ASSERTION(thisContent->IsInDoc(), "Something is confused");
      PRInt32 changedBits = oldState ^ ImageState();
      if (changedBits) {
        mozAutoDocUpdate upd(doc, UPDATE_CONTENT_STATE, PR_TRUE);
        doc->ContentStatesChanged(thisContent, nsnull, changedBits);
      }
    }
  }
}

void
nsImageLoadingContent::CancelImageRequests(PRBool aNotify)
{
  // Make sure to null out mCurrentURI here, so we no longer look like an image
  AutoStateChanger changer(this, aNotify);
  mCurrentURI = nsnull;
  CancelImageRequests(NS_BINDING_ABORTED, PR_TRUE, nsIContentPolicy::ACCEPT);
}

void
nsImageLoadingContent::CancelImageRequests(nsresult aReason,
                                           PRBool   aEvenIfSizeAvailable,
                                           PRInt16  aNewImageStatus)
{
  // Cancel the pending request, if any
  if (mPendingRequest) {
    mPendingRequest->Cancel(aReason);
    mPendingRequest = nsnull;
  }

  // Cancel the current request if it has not progressed enough to
  // have a size yet
  if (mCurrentRequest) {
    PRUint32 loadStatus = imgIRequest::STATUS_ERROR;
    mCurrentRequest->GetImageStatus(&loadStatus);

    NS_ASSERTION(NS_CP_ACCEPTED(mImageBlockingStatus),
                 "Have current request but blocked image?");
    
    if (aEvenIfSizeAvailable ||
        !(loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
      // The new image is going to become the current request.  Make sure to
      // set mImageBlockingStatus _before_ we cancel the request... if we set
      // it after, things that are watching the mCurrentRequest will get wrong
      // data.

      // If we were blocking onload for this image, stop doing so
      SetBlockingOnload(PR_FALSE);

      // Get rid of it
      mImageBlockingStatus = aNewImageStatus;
      mCurrentRequest->Cancel(aReason);
      mCurrentRequest = nsnull;
    }
  } else {
    // No current request so the new image status will become the
    // status of the current request
    mImageBlockingStatus = aNewImageStatus;
  }

  // Note that the only way we could have avoided setting the image blocking
  // status above is if we have a current request and have kept it as the
  // current request.  In that case, we want to leave our old status, since the
  // status corresponds to the current request.  Even if we plan to do a
  // pending request load, having an mCurrentRequest means that our current
  // status is not a REJECT_* status, and doing the load shouldn't change that.
  // XXXbz there is an issue here if different ACCEPT statuses are used, but...
}

nsresult
nsImageLoadingContent::UseAsPrimaryRequest(imgIRequest* aRequest,
                                           PRBool aNotify)
{
  // Use an AutoStateChanger so that the clone call won't
  // automatically notify from inside OnStopDecode.
  // Also, make sure to use the CancelImageRequests which doesn't
  // notify, so that the changer is handling the notifications.
  NS_PRECONDITION(aRequest, "Must have a request here!");
  AutoStateChanger changer(this, aNotify);
  mCurrentURI = nsnull;
  CancelImageRequests(NS_BINDING_ABORTED, PR_TRUE, nsIContentPolicy::ACCEPT);

  NS_ASSERTION(!mCurrentRequest, "We should not have a current request now");

  return aRequest->Clone(this, getter_AddRefs(mCurrentRequest));
}

nsIDocument*
nsImageLoadingContent::GetOurDocument()
{
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this);
  NS_ENSURE_TRUE(thisContent, nsnull);

  return thisContent->GetOwnerDoc();
}

nsresult
nsImageLoadingContent::StringToURI(const nsAString& aSpec,
                                   nsIDocument* aDocument,
                                   nsIURI** aURI)
{
  NS_PRECONDITION(aDocument, "Must have a document");
  NS_PRECONDITION(aURI, "Null out param");

  // (1) Get the base URI
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(this);
  NS_ASSERTION(thisContent, "An image loading content must be an nsIContent");
  nsCOMPtr<nsIURI> baseURL = thisContent->GetBaseURI();

  // (2) Get the charset
  const nsAFlatCString &charset = aDocument->GetDocumentCharacterSet();

  // (3) Construct the silly thing
  return NS_NewURI(aURI,
                   aSpec,
                   charset.IsEmpty() ? nsnull : charset.get(),
                   baseURL,
                   nsContentUtils::GetIOService());
}

nsresult
nsImageLoadingContent::FireEvent(const nsAString& aEventType)
{
  // We have to fire the event asynchronously so that we won't go into infinite
  // loops in cases when onLoad handlers reset the src and the new src is in
  // cache.

  nsCOMPtr<nsINode> thisNode = do_QueryInterface(this);

  nsRefPtr<nsPLDOMEvent> event =
    new nsLoadBlockingPLDOMEvent(thisNode, aEventType, PR_FALSE, PR_FALSE);
  event->PostDOMEvent();
  
  return NS_OK;
}

void
nsImageLoadingContent::SetBlockingOnload(PRBool aBlocking)
{
  // If we're already in the desired state, we have nothing to do
  if (mBlockingOnload == aBlocking)
    return;

  // Get the document
  nsIDocument* doc = GetOurDocument();

  if (doc) {
    // Take the appropriate action
    if (aBlocking)
      doc->BlockOnload();
    else
      doc->UnblockOnload(PR_FALSE);

    // Update our state
    mBlockingOnload = aBlocking;
  }
}

void
nsImageLoadingContent::CreateStaticImageClone(nsImageLoadingContent* aDest) const
{
  aDest->mCurrentRequest = nsContentUtils::GetStaticRequest(mCurrentRequest);
  aDest->mForcedImageState = mForcedImageState;
  aDest->mImageBlockingStatus = mImageBlockingStatus;
  aDest->mLoadingEnabled = mLoadingEnabled;
  aDest->mStartingLoad = mStartingLoad;
  aDest->mIsImageStateForced = mIsImageStateForced;
  aDest->mLoading = mLoading;
  aDest->mBroken = mBroken;
  aDest->mUserDisabled = mUserDisabled;
  aDest->mSuppressed = mSuppressed;
}

