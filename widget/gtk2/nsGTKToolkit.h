/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GTKTOOLKIT_H      
#define GTKTOOLKIT_H

#include "nsString.h"
#include <gtk/gtk.h>

/**
 * Wrapper around the thread running the message pump.
 * The toolkit abstraction is necessary because the message pump must
 * execute within the same thread that created the widget under Win32.
 */ 

class nsGTKToolkit
{
public:
    nsGTKToolkit();
    virtual ~nsGTKToolkit();

    static nsGTKToolkit* GetToolkit();

    static void Shutdown() {
      delete gToolkit;
      gToolkit = nsnull;
    }

    void          CreateSharedGC(void);
    GdkGC         *GetSharedGC(void);
    
    /**
     * Get/set our value of DESKTOP_STARTUP_ID. When non-empty, this is applied
     * to the next toplevel window to be shown or focused (and then immediately
     * cleared).
     */ 
    void SetDesktopStartupID(const nsACString& aID) { mDesktopStartupID = aID; }
    void GetDesktopStartupID(nsACString* aID) { *aID = mDesktopStartupID; }

    /**
     * Get/set the timestamp value to be used, if non-zero, to focus the
     * next top-level window to be shown or focused (upon which it is cleared).
     */
    void SetFocusTimestamp(PRUint32 aTimestamp) { mFocusTimestamp = aTimestamp; }
    PRUint32 GetFocusTimestamp() { return mFocusTimestamp; }

private:
    static nsGTKToolkit* gToolkit;

    GdkGC         *mSharedGC;
    nsCString      mDesktopStartupID;
    PRUint32       mFocusTimestamp;
};

#endif  // GTKTOOLKIT_H
