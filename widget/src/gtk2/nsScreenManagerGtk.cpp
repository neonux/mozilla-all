/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sylvain Pasche <sylvain.pasche@gmail.com>
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsScreenManagerGtk.h"
#include "nsScreenGtk.h"
#include "nsIComponentManager.h"
#include "nsRect.h"
#include "nsAutoPtr.h"

#include <gdk/gdkx.h>

#define SCREEN_MANAGER_LIBRARY_LOAD_FAILED ((PRLibrary*)1)

// prototypes from Xinerama.h
typedef Bool (*_XnrmIsActive_fn)(Display *dpy);
typedef XineramaScreenInfo* (*_XnrmQueryScreens_fn)(Display *dpy, int *number);

nsScreenManagerGtk :: nsScreenManagerGtk ( )
  : mXineramalib(nsnull)
{
  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenManagerGtk :: ~nsScreenManagerGtk()
{
  if (mXineramalib && mXineramalib != SCREEN_MANAGER_LIBRARY_LOAD_FAILED) {
    PR_UnloadLibrary(mXineramalib);
  }
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerGtk, nsIScreenManager)


// this function will make sure that everything has been initialized.
nsresult
nsScreenManagerGtk :: EnsureInit(void)
{
  if (mCachedScreenArray.Count() == 0) {
    XineramaScreenInfo *screenInfo = NULL;
    int numScreens;

    if (!mXineramalib) {
      mXineramalib = PR_LoadLibrary("libXinerama.so.1");
      if (!mXineramalib) {
        mXineramalib = SCREEN_MANAGER_LIBRARY_LOAD_FAILED;
      }
    }
    if (mXineramalib && mXineramalib != SCREEN_MANAGER_LIBRARY_LOAD_FAILED) {
      _XnrmIsActive_fn _XnrmIsActive = (_XnrmIsActive_fn)
          PR_FindFunctionSymbol(mXineramalib, "XineramaIsActive");

      _XnrmQueryScreens_fn _XnrmQueryScreens = (_XnrmQueryScreens_fn)
          PR_FindFunctionSymbol(mXineramalib, "XineramaQueryScreens");
          
      // get the number of screens via xinerama
      if (_XnrmIsActive && _XnrmQueryScreens &&
          _XnrmIsActive(GDK_DISPLAY())) {
        screenInfo = _XnrmQueryScreens(GDK_DISPLAY(), &numScreens);
      }
    }
    // screenInfo == NULL if either Xinerama couldn't be loaded or
    // isn't running on the current display
    if (!screenInfo) {
      nsRefPtr<nsScreenGtk> screen = new nsScreenGtk();
      if (!screen || !mCachedScreenArray.AppendObject(screen)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      screen->Init();
    }
    // If Xinerama is enabled and there's more than one screen, fill
    // in the info for all of the screens.  If that's not the case
    // then nsScreenGTK() defaults to the screen width + height
    else {
#ifdef DEBUG
      printf("Xinerama superpowers activated for %d screens!\n", numScreens);
#endif
      for (int i = 0; i < numScreens; ++i) {
        nsRefPtr<nsScreenGtk> screen = new nsScreenGtk();
        if (!screen || !mCachedScreenArray.AppendObject(screen)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        // initialize this screen object
        screen->Init(&screenInfo[i]);
      }
    }

    if (screenInfo) {
      XFree(screenInfo);
    }
  }

  return NS_OK;
}


//
// ScreenForRect 
//
// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in pixels (not twips) and in screen coordinates.
//
NS_IMETHODIMP
nsScreenManagerGtk :: ScreenForRect ( PRInt32 aX, PRInt32 aY,
                                      PRInt32 aWidth, PRInt32 aHeight,
                                      nsIScreen **aOutScreen )
{
  nsresult rv;
  rv = EnsureInit();
  if (NS_FAILED(rv)) {
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from ScreenForRect\n");
    return rv;
  }
  // which screen ( index from zero ) should we return?
  PRUint32 which = 0;
  // Optimize for the common case.  If the number of screens is only
  // one then this will fall through with which == 0 and will get the
  // primary screen.
  if (mCachedScreenArray.Count() > 1) {
    // walk the list of screens and find the one that has the most
    // surface area.
    PRUint32 area = 0;
    nsRect   windowRect(aX, aY, aWidth, aHeight);
    for (PRInt32 i = 0, i_end = mCachedScreenArray.Count(); i < i_end; ++i) {
      PRInt32  x, y, width, height;
      x = y = width = height = 0;
      mCachedScreenArray[i]->GetRect(&x, &y, &width, &height);
      // calculate the surface area
      nsRect screenRect(x, y, width, height);
      screenRect.IntersectRect(screenRect, windowRect);
      PRUint32 tempArea = screenRect.width * screenRect.height;
      if (tempArea >= area) {
        which = i;
        area = tempArea;
      }
    }
  }
  *aOutScreen = mCachedScreenArray.SafeObjectAt(which);
  NS_IF_ADDREF(*aOutScreen);
  return NS_OK;
    
} // ScreenForRect


//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP 
nsScreenManagerGtk :: GetPrimaryScreen(nsIScreen * *aPrimaryScreen) 
{
  nsresult rv;
  rv =  EnsureInit();
  if (NS_FAILED(rv)) {
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from GetPrimaryScreen\n");
    return rv;
  }
  *aPrimaryScreen = mCachedScreenArray.SafeObjectAt(0);
  NS_IF_ADDREF(*aPrimaryScreen);
  return NS_OK;
  
} // GetPrimaryScreen


//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP
nsScreenManagerGtk :: GetNumberOfScreens(PRUint32 *aNumberOfScreens)
{
  nsresult rv;
  rv = EnsureInit();
  if (NS_FAILED(rv)) {
    NS_ERROR("nsScreenManagerGtk::EnsureInit() failed from GetNumberOfScreens\n");
    return rv;
  }
  *aNumberOfScreens = mCachedScreenArray.Count();
  return NS_OK;
  
} // GetNumberOfScreens

NS_IMETHODIMP
nsScreenManagerGtk :: ScreenForNativeWidget (void *aWidget, nsIScreen **outScreen)
{
  // I don't know how to go from GtkWindow to nsIScreen, especially
  // given xinerama and stuff, so let's just do this
  gint x, y, width, height, depth;
  x = y = width = height = 0;

  gdk_window_get_geometry(GDK_WINDOW(aWidget), &x, &y, &width, &height,
                          &depth);
  gdk_window_get_origin(GDK_WINDOW(aWidget), &x, &y);
  return ScreenForRect(x, y, width, height, outScreen);
}
