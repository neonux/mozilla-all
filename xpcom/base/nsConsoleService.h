/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsConsoleService class declaration.
 */

#ifndef __nsconsoleservice_h__
#define __nsconsoleservice_h__

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

#include "nsCOMPtr.h"
#include "nsInterfaceHashtable.h"
#include "nsHashKeys.h"

#include "nsIConsoleService.h"

class nsConsoleService MOZ_FINAL : public nsIConsoleService
{
public:
    nsConsoleService();
    nsresult Init();

    NS_DECL_ISUPPORTS
    NS_DECL_NSICONSOLESERVICE

    void SetIsDelivering() {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(!mDeliveringMessage);
        mDeliveringMessage = true;
    }

    void SetDoneDelivering() {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(mDeliveringMessage);
        mDeliveringMessage = false;
    }

private:
    ~nsConsoleService();

    // Circular buffer of saved messages
    nsIConsoleMessage **mMessages;

    // How big?
    uint32_t mBufferSize;

    // Index of slot in mMessages that'll be filled by *next* log message
    uint32_t mCurrent;

    // Is the buffer full? (Has mCurrent wrapped around at least once?)
    bool mFull;

    // Are we currently delivering a console message on the main thread? If
    // so, we suppress incoming messages on the main thread only, to avoid
    // infinite repitition.
    bool mDeliveringMessage;

    // Listeners to notify whenever a new message is logged.
    nsInterfaceHashtable<nsISupportsHashKey, nsIConsoleListener> mListeners;

    // To serialize interesting methods.
    mozilla::Mutex mLock;
};

#endif /* __nsconsoleservice_h__ */
