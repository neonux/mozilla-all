/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsDocShellLoadInfo.h"
#include "nsReadableUtils.h"

//*****************************************************************************
//***    nsDocShellLoadInfo: Object Management
//*****************************************************************************

nsDocShellLoadInfo::nsDocShellLoadInfo()
  : mInheritOwner(false),
    mOwnerIsExplicit(false),
    mSendReferrer(true),
    mLoadType(nsIDocShellLoadInfo::loadNormal)
{
}

nsDocShellLoadInfo::~nsDocShellLoadInfo()
{
}

//*****************************************************************************
// nsDocShellLoadInfo::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsDocShellLoadInfo)
NS_IMPL_RELEASE(nsDocShellLoadInfo)

NS_INTERFACE_MAP_BEGIN(nsDocShellLoadInfo)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShellLoadInfo)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellLoadInfo)
NS_INTERFACE_MAP_END     

//*****************************************************************************
// nsDocShellLoadInfo::nsIDocShellLoadInfo
//*****************************************************************************   

NS_IMETHODIMP nsDocShellLoadInfo::GetReferrer(nsIURI** aReferrer)
{
   NS_ENSURE_ARG_POINTER(aReferrer);

   *aReferrer = mReferrer;
   NS_IF_ADDREF(*aReferrer);
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetReferrer(nsIURI* aReferrer)
{
   mReferrer = aReferrer;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetOwner(nsISupports** aOwner)
{
   NS_ENSURE_ARG_POINTER(aOwner);

   *aOwner = mOwner;
   NS_IF_ADDREF(*aOwner);
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetOwner(nsISupports* aOwner)
{
   mOwner = aOwner;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetInheritOwner(bool* aInheritOwner)
{
   NS_ENSURE_ARG_POINTER(aInheritOwner);

   *aInheritOwner = mInheritOwner;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetInheritOwner(bool aInheritOwner)
{
   mInheritOwner = aInheritOwner;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetOwnerIsExplicit(bool* aOwnerIsExplicit)
{
   *aOwnerIsExplicit = mOwnerIsExplicit;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetOwnerIsExplicit(bool aOwnerIsExplicit)
{
   mOwnerIsExplicit = aOwnerIsExplicit;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetLoadType(nsDocShellInfoLoadType * aLoadType)
{
   NS_ENSURE_ARG_POINTER(aLoadType);

   *aLoadType = mLoadType;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetLoadType(nsDocShellInfoLoadType aLoadType)
{
   mLoadType = aLoadType;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetSHEntry(nsISHEntry** aSHEntry)
{
   NS_ENSURE_ARG_POINTER(aSHEntry);

   *aSHEntry = mSHEntry;
   NS_IF_ADDREF(*aSHEntry);
   return NS_OK;
}
 
NS_IMETHODIMP nsDocShellLoadInfo::SetSHEntry(nsISHEntry* aSHEntry)
{
   mSHEntry = aSHEntry;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetTarget(PRUnichar** aTarget)
{
   NS_ENSURE_ARG_POINTER(aTarget);

   *aTarget = ToNewUnicode(mTarget);

   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetTarget(const PRUnichar* aTarget)
{
   mTarget.Assign(aTarget);
   return NS_OK;
}


NS_IMETHODIMP
nsDocShellLoadInfo::GetPostDataStream(nsIInputStream **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mPostDataStream;

  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


NS_IMETHODIMP
nsDocShellLoadInfo::SetPostDataStream(nsIInputStream *aStream)
{
  mPostDataStream = aStream;
  return NS_OK;
}

/* attribute nsIInputStream headersStream; */
NS_IMETHODIMP nsDocShellLoadInfo::GetHeadersStream(nsIInputStream * *aHeadersStream)
{
  NS_ENSURE_ARG_POINTER(aHeadersStream);
  *aHeadersStream = mHeadersStream;
  NS_IF_ADDREF(*aHeadersStream);
  return NS_OK;
}
NS_IMETHODIMP nsDocShellLoadInfo::SetHeadersStream(nsIInputStream * aHeadersStream)
{
  mHeadersStream = aHeadersStream;
  return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::GetSendReferrer(bool* aSendReferrer)
{
   NS_ENSURE_ARG_POINTER(aSendReferrer);

   *aSendReferrer = mSendReferrer;
   return NS_OK;
}

NS_IMETHODIMP nsDocShellLoadInfo::SetSendReferrer(bool aSendReferrer)
{
   mSendReferrer = aSendReferrer;
   return NS_OK;
}

//*****************************************************************************
// nsDocShellLoadInfo: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsDocShellLoadInfo: Accessors
//*****************************************************************************   
