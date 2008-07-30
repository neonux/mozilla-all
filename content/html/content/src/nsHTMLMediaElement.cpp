/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
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
#include "nsIDOMHTMLMediaElement.h"
#include "nsIDOMHTMLSourceElement.h"
#include "nsHTMLMediaElement.h"
#include "nsGenericHTMLElement.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGkAtoms.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMError.h"
#include "nsNodeInfoManager.h"
#include "plbase64.h"
#include "nsNetUtil.h"
#include "prmem.h"
#include "nsNetUtil.h"
#include "nsXPCOMStrings.h"
#include "prlock.h"
#include "nsThreadUtils.h"

#include "nsIScriptSecurityManager.h"
#include "nsIXPConnect.h"
#include "jsapi.h"

#include "nsIRenderingContext.h"
#include "nsITimer.h"

#include "nsEventDispatcher.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMProgressEvent.h"
#include "nsHTMLMediaError.h"

#ifdef MOZ_OGG
#include "nsOggDecoder.h"
#endif

class nsAsyncEventRunner : public nsRunnable
{
private:
  nsString mName;
  nsCOMPtr<nsHTMLMediaElement> mElement;
  PRPackedBool mProgress;
  
public:
  nsAsyncEventRunner(const nsAString& aName, nsHTMLMediaElement* aElement, PRBool aProgress) : 
    mName(aName), mElement(aElement), mProgress(aProgress)
  {
  }
  
  NS_IMETHOD Run() {
    return mProgress ?
      mElement->DispatchProgressEvent(mName) :
      mElement->DispatchSimpleEvent(mName);
  }
};

// nsIDOMHTMLMediaElement
NS_IMPL_URI_ATTR(nsHTMLMediaElement, Src, src)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Controls, controls)
NS_IMPL_BOOL_ATTR(nsHTMLMediaElement, Autoplay, autoplay)
NS_IMPL_FLOAT_ATTR_DEFAULT_VALUE(nsHTMLMediaElement, PlaybackRate, playbackrate, 1.0)
NS_IMPL_FLOAT_ATTR_DEFAULT_VALUE(nsHTMLMediaElement, DefaultPlaybackRate, defaultplaybackrate, 1.0)
NS_IMPL_FLOAT_ATTR(nsHTMLMediaElement, Start, start)
NS_IMPL_FLOAT_ATTR(nsHTMLMediaElement, End, end)
NS_IMPL_FLOAT_ATTR(nsHTMLMediaElement, LoopStart, loopstart)
NS_IMPL_FLOAT_ATTR(nsHTMLMediaElement, LoopEnd, loopend)

/* readonly attribute nsIDOMHTMLMediaError error; */
NS_IMETHODIMP nsHTMLMediaElement::GetError(nsIDOMHTMLMediaError * *aError)
{
  NS_IF_ADDREF(*aError = mError);

  return NS_OK;
}

/* readonly attribute boolean ended; */
NS_IMETHODIMP nsHTMLMediaElement::GetEnded(PRBool *aEnded)
{
  *aEnded = mEnded;

  return NS_OK;
}


/* readonly attribute DOMString currentSrc; */
NS_IMETHODIMP nsHTMLMediaElement::GetCurrentSrc(nsAString & aCurrentSrc)
{
  nsCAutoString src;
  
  if (mDecoder) {
    nsCOMPtr<nsIURI> uri;
    mDecoder->GetCurrentURI(getter_AddRefs(uri));
    if (uri) {
      uri->GetSpec(src);
    }
  }

  aCurrentSrc = NS_ConvertUTF8toUTF16(src);

  return NS_OK;
}

/* readonly attribute unsigned short networkState; */
NS_IMETHODIMP nsHTMLMediaElement::GetNetworkState(PRUint16 *aNetworkState)
{
  *aNetworkState = mNetworkState;

  return NS_OK;
}

/* readonly attribute float bufferingRate; */
NS_IMETHODIMP nsHTMLMediaElement::GetBufferingRate(float *aBufferingRate)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean bufferingThrottled; */
NS_IMETHODIMP nsHTMLMediaElement::GetBufferingThrottled(PRBool *aBufferingRate)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMTimeRanges buffered; */
NS_IMETHODIMP nsHTMLMediaElement::GetBuffered(nsIDOMHTMLTimeRanges * *aBuffered)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMByteRanges bufferedBytes; */
NS_IMETHODIMP nsHTMLMediaElement::GetBufferedBytes(nsIDOMHTMLByteRanges * *aBufferedBytes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute unsigned long totalBytes; */
NS_IMETHODIMP nsHTMLMediaElement::GetTotalBytes(PRUint32 *aTotalBytes)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void load (); */
NS_IMETHODIMP nsHTMLMediaElement::Load()
{
  if (mBegun) {
    mBegun = PR_FALSE;
    
    mError = new nsHTMLMediaError(nsHTMLMediaError::MEDIA_ERR_ABORTED);
    DispatchProgressEvent(NS_LITERAL_STRING("abort"));
    return NS_OK;
  }

  mError = nsnull;
  mLoadedFirstFrame = PR_FALSE;
  mAutoplaying = PR_TRUE;

  float rate = 1.0;
  GetDefaultPlaybackRate(&rate);
  SetPlaybackRate(rate);

  if (mNetworkState != nsIDOMHTMLMediaElement::EMPTY) {
    mNetworkState = nsIDOMHTMLMediaElement::EMPTY;
    ChangeReadyState(nsIDOMHTMLMediaElement::DATA_UNAVAILABLE);
    mPaused = PR_TRUE;
    mSeeking = PR_FALSE;
    // TODO: The current playback position must be set to 0.
    // TODO: The currentLoop DOM attribute must be set to 0.
    DispatchSimpleEvent(NS_LITERAL_STRING("emptied"));
  }

  nsAutoString chosenMediaResource;
  nsresult rv = PickMediaElement(chosenMediaResource);
  NS_ENSURE_SUCCESS(rv, rv);

  mNetworkState = nsIDOMHTMLMediaElement::LOADING;
  
  // This causes the currentSrc attribute to become valid
  rv = InitializeDecoder(chosenMediaResource);
  NS_ENSURE_SUCCESS(rv, rv);

  mBegun = PR_TRUE;
  mEnded = PR_FALSE;

  DispatchAsyncProgressEvent(NS_LITERAL_STRING("loadstart"));

  return NS_OK;
}

/* readonly attribute unsigned short readyState; */
NS_IMETHODIMP nsHTMLMediaElement::GetReadyState(PRUint16 *aReadyState)
{
  *aReadyState = mReadyState;

  return NS_OK;
}

/* readonly attribute boolean seeking; */
NS_IMETHODIMP nsHTMLMediaElement::GetSeeking(PRBool *aSeeking)
{
  *aSeeking = mSeeking;

  return NS_OK;
}

/* attribute float currentTime; */
NS_IMETHODIMP nsHTMLMediaElement::GetCurrentTime(float *aCurrentTime)
{
  *aCurrentTime = mDecoder ? mDecoder->GetCurrentTime() : 0.0;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetCurrentTime(float aCurrentTime)
{
  return mDecoder ? mDecoder->Seek(aCurrentTime) : NS_ERROR_DOM_INVALID_STATE_ERR;
}

/* readonly attribute float duration; */
NS_IMETHODIMP nsHTMLMediaElement::GetDuration(float *aDuration)
{
  *aDuration =  mDecoder ? mDecoder->GetDuration() : 0.0;
  return NS_OK;
}

/* readonly attribute unsigned short paused; */
NS_IMETHODIMP nsHTMLMediaElement::GetPaused(PRUint16 *aPaused)
{
  *aPaused = mPaused;

  return NS_OK;
}

/* readonly attribute nsIDOMHTMLTimeRanges played; */
NS_IMETHODIMP nsHTMLMediaElement::GetPlayed(nsIDOMHTMLTimeRanges * *aPlayed)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMHTMLTimeRanges seekable; */
NS_IMETHODIMP nsHTMLMediaElement::GetSeekable(nsIDOMHTMLTimeRanges * *aSeekable)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void pause (); */
NS_IMETHODIMP nsHTMLMediaElement::Pause()
{
  if (!mDecoder) 
    return NS_OK;

  nsresult rv;

  if (mNetworkState == nsIDOMHTMLMediaElement::EMPTY) {
    rv = Load();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mDecoder->Pause();
  PRBool oldPaused = mPaused;
  mPaused = PR_TRUE;
  mAutoplaying = PR_FALSE;
  
  if (!oldPaused) {
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("timeupdate"));
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("pause"));
  }

  return NS_OK;
}

/* attribute unsigned long playCount; */
NS_IMETHODIMP nsHTMLMediaElement::GetPlayCount(PRUint32 *aPlayCount)
{
  return GetIntAttr(nsGkAtoms::playcount, 1, reinterpret_cast<PRInt32*>(aPlayCount));
}

NS_IMETHODIMP nsHTMLMediaElement::SetPlayCount(PRUint32 aPlayCount)
{
  return SetIntAttr(nsGkAtoms::playcount, static_cast<PRInt32>(aPlayCount));
}

/* attribute unsigned long currentLoop; */
NS_IMETHODIMP nsHTMLMediaElement::GetCurrentLoop(PRUint32 *aCurrentLoop)
{
  return GetIntAttr(nsGkAtoms::currentloop, 0, reinterpret_cast<PRInt32*>(aCurrentLoop));
}

NS_IMETHODIMP nsHTMLMediaElement::SetCurrentLoop(PRUint32 aCurrentLoop)
{
  return SetIntAttr(nsGkAtoms::currentloop, static_cast<PRInt32>(aCurrentLoop));
}

/* void addCueRange (in DOMString className, in float start, in float end, in boolean pauseOnExit, in nsIDOMHTMLVoidCallback enterCallback, in nsIDOMHTMLVoidCallback exitCallback); */
NS_IMETHODIMP nsHTMLMediaElement::AddCueRange(const nsAString & className, float start, float end, PRBool pauseOnExit, nsIDOMHTMLVoidCallback *enterCallback, nsIDOMHTMLVoidCallback *exitCallback)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void removeCueRanges (in DOMString className); */
NS_IMETHODIMP nsHTMLMediaElement::RemoveCueRanges(const nsAString & className)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute float volume; */
NS_IMETHODIMP nsHTMLMediaElement::GetVolume(float *aVolume)
{
  if (mMuted)
    *aVolume = mMutedVolume;
  else
    *aVolume = mDecoder ? mDecoder->GetVolume() : 0.0;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetVolume(float aVolume)
{
  if (mMuted) 
    mMutedVolume = aVolume;
  else {
    if (mDecoder)
      mDecoder->SetVolume(aVolume);

    DispatchSimpleEvent(NS_LITERAL_STRING("volumechange"));
  }
  return NS_OK;
}

/* attribute boolean muted; */
NS_IMETHODIMP nsHTMLMediaElement::GetMuted(PRBool *aMuted)
{
  *aMuted = mMuted;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLMediaElement::SetMuted(PRBool aMuted)
{
  PRBool oldMuted = mMuted;

  if (mDecoder) {
    if (mMuted && !aMuted) {
      mDecoder->SetVolume(mMutedVolume);
    }
    else if (!mMuted && aMuted) {
      mMutedVolume = mDecoder->GetVolume();
      mDecoder->SetVolume(0.0);
    }
  }

  mMuted = aMuted;

  if (oldMuted != mMuted) 
    DispatchSimpleEvent(NS_LITERAL_STRING("volumechange"));
  return NS_OK;
}

nsHTMLMediaElement::nsHTMLMediaElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
  : nsGenericHTMLElement(aNodeInfo),
    mNetworkState(nsIDOMHTMLMediaElement::EMPTY),
    mReadyState(nsIDOMHTMLMediaElement::DATA_UNAVAILABLE),
    mBegun(PR_FALSE),
    mEnded(PR_FALSE),
    mLoadedFirstFrame(PR_FALSE),
    mAutoplaying(PR_TRUE),
    mPaused(PR_TRUE),
    mSeeking(PR_FALSE),
    mMuted(PR_FALSE),
    mMutedVolume(0.0),
    mIsDoneAddingChildren(!aFromParser)
{
}

nsHTMLMediaElement::~nsHTMLMediaElement()
{
}

NS_IMETHODIMP
nsHTMLMediaElement::Play(void)
{
  if (!mDecoder)
    return NS_OK;

  nsresult rv;

  if (mNetworkState == nsIDOMHTMLMediaElement::EMPTY) {
    rv = Load();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // TODO: If the playback has ended, then the user agent must set 
  // currentLoop to zero and seek to the effective start.
  float rate = 1.0;
  GetDefaultPlaybackRate(&rate);
  SetPlaybackRate(rate);
  rv = mDecoder->Play();
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool oldPaused = mPaused;
  mPaused = PR_FALSE;
  mAutoplaying = PR_FALSE;

  if (oldPaused)
    DispatchAsyncSimpleEvent(NS_LITERAL_STRING("play"));


  return NS_OK;
}

PRBool
nsHTMLMediaElement::ParseAttribute(PRInt32 aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::src) {
      static const char* kWhitespace = " \n\r\t\b";
      aResult.SetTo(nsContentUtils::TrimCharsInSet(kWhitespace, aValue));
      return PR_TRUE;
    }
    else if (aAttribute == nsGkAtoms::playbackrate
            || aAttribute == nsGkAtoms::defaultplaybackrate
            || aAttribute == nsGkAtoms::loopstart
            || aAttribute == nsGkAtoms::loopend
            || aAttribute == nsGkAtoms::start
            || aAttribute == nsGkAtoms::end) {
      return aResult.ParseFloatValue(aValue);
    }
    else if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return PR_TRUE;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}
#include "nsString.h"
nsresult
nsHTMLMediaElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            nsIAtom* aPrefix, const nsAString& aValue,
                            PRBool aNotify)
{
  nsresult rv = 
    nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                    aNotify);
  if (aNotify && aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::src) {
      Load();
    }
    else if (aName == nsGkAtoms::playbackrate || aName == nsGkAtoms::defaultplaybackrate) {
      if (mDecoder) 
        mDecoder->PlaybackRateChanged();
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("ratechange"));
    }
  }

  return rv;
}

nsresult nsHTMLMediaElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                        nsIContent* aBindingParent,
                                        PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, 
                                                 aParent, 
                                                 aBindingParent, 
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mIsDoneAddingChildren && mNetworkState == nsIDOMHTMLMediaElement::EMPTY) {
    Load();
  }

  return rv;
}

void nsHTMLMediaElement::UnbindFromTree(PRBool aDeep,
                                        PRBool aNullParent)
{
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}


nsresult nsHTMLMediaElement::PickMediaElement(nsAString& aChosenMediaResource)
{
  // Implements:
  // http://www.whatwg.org/specs/web-apps/current-work/#pick-a
  nsAutoString src;
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
    if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
      aChosenMediaResource = src;

#ifdef MOZ_OGG
      // Currently assuming an Ogg file
      // TODO: Instantiate decoder based on type
      if (mDecoder) {
        mDecoder->ElementUnavailable();
        mDecoder->Stop();
        mDecoder = nsnull;
      }

      mDecoder = new nsOggDecoder();
      if (mDecoder && !mDecoder->Init()) {
        mDecoder = nsnull;
      }
#endif
      return NS_OK;
    }
  }

  // Checking of 'source' elements as per:
  // http://www.whatwg.org/specs/web-apps/current-work/#pick-a
  PRUint32 count = GetChildCount();
  for (PRUint32 i = 0; i < count; ++i) {
    nsIContent* child = GetChildAt(i);
    NS_ASSERTION(child, "GetChildCount lied!");
    
    nsCOMPtr<nsIContent> source = do_QueryInterface(child);
    if (source) {
      if (source->HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
        nsAutoString type;

        if (source->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type)) {
#if MOZ_OGG
          if (type.EqualsLiteral("video/ogg") || type.EqualsLiteral("application/ogg")) {
            nsAutoString src;
            if (source->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
              mDecoder = new nsOggDecoder();
              if (mDecoder && !mDecoder->Init()) {
                mDecoder = nsnull;
              }
              aChosenMediaResource = src;
              return NS_OK;
            }
          }
#endif
        }
      }
    }    
  }        

  return NS_ERROR_DOM_INVALID_STATE_ERR;
}

nsresult nsHTMLMediaElement::InitializeDecoder(nsAString& aChosenMediaResource)
{
  nsCOMPtr<nsIDocument> doc = GetOwnerDoc();
  if (!doc) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIURI> baseURL = GetBaseURI();
  const nsAFlatCString &charset = doc->GetDocumentCharacterSet();
  rv = NS_NewURI(getter_AddRefs(uri), 
                 aChosenMediaResource, 
                 charset.IsEmpty() ? nsnull : charset.get(), 
                 baseURL, 
                 nsContentUtils::GetIOService());
  NS_ENSURE_SUCCESS(rv, rv);

  if (mDecoder) {
    rv = mDecoder->Load(uri);
    if (NS_FAILED(rv)) {
      mDecoder = nsnull;
    }
  }
  return rv;
}

void nsHTMLMediaElement::MetadataLoaded()
{
  mNetworkState = nsIDOMHTMLMediaElement::LOADED_METADATA;
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("durationchange"));
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("loadedmetadata"));
  float start = 0.0;
  nsresult rv = GetStart(&start);
  if (NS_SUCCEEDED(rv) && start > 0.0 && mDecoder)
    mDecoder->Seek(start);
}

void nsHTMLMediaElement::FirstFrameLoaded()
{
  mNetworkState = nsIDOMHTMLMediaElement::LOADED_FIRST_FRAME;
  ChangeReadyState(nsIDOMHTMLMediaElement::CAN_SHOW_CURRENT_FRAME);
  mLoadedFirstFrame = PR_TRUE;
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("loadedfirstframe"));
  DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canshowcurrentframe"));
}

void nsHTMLMediaElement::ResourceLoaded()
{
  mBegun = PR_FALSE;
  mEnded = PR_FALSE;
  mNetworkState = nsIDOMHTMLMediaElement::LOADED;
  ChangeReadyState(nsIDOMHTMLMediaElement::CAN_PLAY_THROUGH);

  DispatchProgressEvent(NS_LITERAL_STRING("load"));
}

void nsHTMLMediaElement::NetworkError()
{
  mError = new nsHTMLMediaError(nsHTMLMediaError::MEDIA_ERR_NETWORK);
  mBegun = PR_FALSE;
  DispatchProgressEvent(NS_LITERAL_STRING("error"));
  mNetworkState = nsIDOMHTMLMediaElement::EMPTY;
  DispatchSimpleEvent(NS_LITERAL_STRING("empty"));
}

void nsHTMLMediaElement::PlaybackCompleted()
{
  mBegun = PR_FALSE;
  mEnded = PR_TRUE;
  Pause();
  SetCurrentTime(0);
  DispatchSimpleEvent(NS_LITERAL_STRING("ended"));
}

void nsHTMLMediaElement::CanPlayThrough()
{
  ChangeReadyState(nsIDOMHTMLMediaElement::CAN_PLAY_THROUGH);
}

void nsHTMLMediaElement::ChangeReadyState(nsMediaReadyState aState)
{
  mReadyState = aState;
  if (mNetworkState != nsIDOMHTMLMediaElement::EMPTY) {
    switch(mReadyState) {
    case nsIDOMHTMLMediaElement::DATA_UNAVAILABLE:
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("dataunavailable"));
      LOG(PR_LOG_DEBUG, ("Ready state changed to DATA_UNAVAILABLE"));
      break;
      
    case nsIDOMHTMLMediaElement::CAN_SHOW_CURRENT_FRAME:
      if (mLoadedFirstFrame) {
        DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canshowcurrentframe"));
        LOG(PR_LOG_DEBUG, ("Ready state changed to CAN_SHOW_CURRENT_FRAME"));
      }
      break;

    case nsIDOMHTMLMediaElement::CAN_PLAY:
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canplay"));
      LOG(PR_LOG_DEBUG, ("Ready state changed to CAN_PLAY"));
      break;

    case nsIDOMHTMLMediaElement::CAN_PLAY_THROUGH:
      DispatchAsyncSimpleEvent(NS_LITERAL_STRING("canplaythrough"));
      if (mAutoplaying && 
         mPaused && 
         HasAttr(kNameSpaceID_None, nsGkAtoms::autoplay)) {
        mPaused = PR_FALSE;
        if (mDecoder) {
          mDecoder->Play();
        }
        LOG(PR_LOG_DEBUG, ("Ready state changed to CAN_PLAY_THROUGH"));
        DispatchAsyncSimpleEvent(NS_LITERAL_STRING("play"));
      }
      break;
    }
  }  
}

void nsHTMLMediaElement::Paint(gfxContext* aContext, const gfxRect& aRect) 
{
  if (mDecoder)
    mDecoder->Paint(aContext, aRect);
}

nsresult nsHTMLMediaElement::DispatchSimpleEvent(const nsAString& aName)
{
  return nsContentUtils::DispatchTrustedEvent(GetOwnerDoc(), 
                                              static_cast<nsIContent*>(this), 
                                              aName, 
                                              PR_TRUE, 
                                              PR_TRUE);
}

nsresult nsHTMLMediaElement::DispatchAsyncSimpleEvent(const nsAString& aName)
{
  nsCOMPtr<nsIRunnable> event = new nsAsyncEventRunner(aName, this, PR_FALSE);
  if (event)
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL); 
  return NS_OK;                           
}

nsresult nsHTMLMediaElement::DispatchAsyncProgressEvent(const nsAString& aName)
{
  nsCOMPtr<nsIRunnable> event = new nsAsyncEventRunner(aName, this, PR_TRUE);
  if (event)
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL); 
  return NS_OK;                           
}

nsresult nsHTMLMediaElement::DispatchProgressEvent(const nsAString& aName)
{
  if (!mDecoder)
    return NS_OK;

  nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(GetOwnerDoc()));
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(static_cast<nsIContent*>(this)));
  NS_ENSURE_TRUE(docEvent && target, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = docEvent->CreateEvent(NS_LITERAL_STRING("ProgressEvent"), getter_AddRefs(event));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMProgressEvent> progressEvent(do_QueryInterface(event));
  NS_ENSURE_TRUE(progressEvent, NS_ERROR_FAILURE);
  
  rv = progressEvent->InitProgressEvent(aName, PR_TRUE, PR_TRUE, PR_FALSE, mDecoder->GetBytesLoaded(), mDecoder->GetTotalBytes());
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool dummy;
  return target->DispatchEvent(event, &dummy);  
}

nsresult nsHTMLMediaElement::DoneAddingChildren(PRBool aHaveNotified)
{
  if (!mIsDoneAddingChildren) {
    mIsDoneAddingChildren = PR_TRUE;
  
    if (mNetworkState == nsIDOMHTMLMediaElement::EMPTY) {
      Load();
    }
  }

  return NS_OK;
}

PRBool nsHTMLMediaElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}
