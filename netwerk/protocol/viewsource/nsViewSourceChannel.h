/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
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

#ifndef nsViewSourceChannel_h___
#define nsViewSourceChannel_h___

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIViewSourceChannel.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsViewSourceHandler.h"
#include "nsNetCID.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsICachingChannel.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIUploadChannel.h"

class nsViewSourceChannel : public nsIViewSourceChannel,
                            public nsIStreamListener,
                            public nsIHttpChannel,
                            public nsIHttpChannelInternal,
                            public nsICachingChannel,
                            public nsIApplicationCacheChannel,
                            public nsIUploadChannel
{

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIVIEWSOURCECHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIHTTPCHANNEL
    NS_FORWARD_SAFE_NSICACHEINFOCHANNEL(mCachingChannel)
    NS_FORWARD_SAFE_NSICACHINGCHANNEL(mCachingChannel)
    NS_FORWARD_SAFE_NSIAPPLICATIONCACHECHANNEL(mApplicationCacheChannel)
    NS_FORWARD_SAFE_NSIAPPLICATIONCACHECONTAINER(mApplicationCacheChannel)
    NS_FORWARD_SAFE_NSIUPLOADCHANNEL(mUploadChannel)
    NS_FORWARD_SAFE_NSIHTTPCHANNELINTERNAL(mHttpChannelInternal)

    // nsViewSourceChannel methods:
    nsViewSourceChannel()
        : mIsDocument(false)
        , mOpened(false) {}

    NS_HIDDEN_(nsresult) Init(nsIURI* uri);

protected:
    nsCOMPtr<nsIChannel>        mChannel;
    nsCOMPtr<nsIHttpChannel>    mHttpChannel;
    nsCOMPtr<nsIHttpChannelInternal>    mHttpChannelInternal;
    nsCOMPtr<nsICachingChannel> mCachingChannel;
    nsCOMPtr<nsIApplicationCacheChannel> mApplicationCacheChannel;
    nsCOMPtr<nsIUploadChannel>  mUploadChannel;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsIURI>            mOriginalURI;
    nsCString                   mContentType;
    bool                        mIsDocument; // keeps track of the LOAD_DOCUMENT_URI flag
    bool                        mOpened;
};

#endif /* nsViewSourceChannel_h___ */