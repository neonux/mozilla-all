/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsToolkit_h__
#define nsToolkit_h__

#include "nsdefs.h"

#include "nsITimer.h"
#include "nsCOMPtr.h"
#include <windows.h>

// Avoid including windowsx.h to prevent macro pollution
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(pt) (short(LOWORD(pt)))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(pt) (short(HIWORD(pt)))
#endif

/**
 * Makes sure exit/enter mouse messages are always dispatched.
 * In the case where the mouse has exited the outer most window the
 * only way to tell if it has exited is to set a timer and look at the
 * mouse pointer to see if it is within the outer most window.
 */ 

class MouseTrailer 
{
public:
    HWND                  GetMouseTrailerWindow() { return mMouseTrailerWindow; }
    HWND                  GetCaptureWindow() { return mCaptureWindow; }

    void                  SetMouseTrailerWindow(HWND aWnd);
    void                  SetCaptureWindow(HWND aWnd);
    void                  Disable() { mEnabled = false; DestroyTimer(); }
    void                  Enable() { mEnabled = true; CreateTimer(); }
    void                  DestroyTimer();

                          MouseTrailer();
                          ~MouseTrailer();
private:

    nsresult              CreateTimer();

    static void           TimerProc(nsITimer* aTimer, void* aClosure);

    // Information for mouse enter/exit events
    HWND                  mMouseTrailerWindow;
    HWND                  mCaptureWindow;
    bool                  mIsInCaptureMode;
    bool                  mEnabled;
    nsCOMPtr<nsITimer>    mTimer;
};

/**
 * Wrapper around the thread running the message pump.
 * The toolkit abstraction is necessary because the message pump must
 * execute within the same thread that created the widget under Win32.
 */ 

class nsToolkit
{
public:
    nsToolkit();

private:
    ~nsToolkit();

public:
    static nsToolkit* GetToolkit();

    static HINSTANCE mDllInstance;
    static MouseTrailer *gMouseTrailer;

    static void Startup(HMODULE hModule);
    static void Shutdown();
    static void StartAllowingD3D9();

protected:
    static nsToolkit* gToolkit;

    nsCOMPtr<nsITimer> mD3D9Timer;
    MouseTrailer mMouseTrailer;
};

#endif  // TOOLKIT_H
