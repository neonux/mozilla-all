/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPermission.h"

// nsPermission Implementation

NS_IMPL_ISUPPORTS1(nsPermission, nsIPermission)

nsPermission::nsPermission(const nsACString &aHost,
                           const nsACString &aType,
                           uint32_t         aCapability,
                           uint32_t         aExpireType,
                           int64_t          aExpireTime)
 : mHost(aHost)
 , mType(aType)
 , mCapability(aCapability)
 , mExpireType(aExpireType)
 , mExpireTime(aExpireTime)
{
}

nsPermission::~nsPermission()
{
}

NS_IMETHODIMP
nsPermission::GetHost(nsACString &aHost)
{
  aHost = mHost;
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::GetType(nsACString &aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::GetCapability(uint32_t *aCapability)
{
  *aCapability = mCapability;
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::GetExpireType(uint32_t *aExpireType)
{
  *aExpireType = mExpireType;
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::GetExpireTime(int64_t *aExpireTime)
{
  *aExpireTime = mExpireTime;
  return NS_OK;
}
