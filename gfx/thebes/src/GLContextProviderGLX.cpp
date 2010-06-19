/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Woodrow <mwoodrow@mozilla.com>
 *   Bas Schouten <bschouten@mozilla.com>
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

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
// we're using default display for now
#define GET_NATIVE_WINDOW(aWidget) GDK_WINDOW_XID((GdkWindow *) aWidget->GetNativeData(NS_NATIVE_WINDOW))
#define DISPLAY gdk_x11_get_default_xdisplay
#elif defined(MOZ_WIDGET_QT)
#include <QWidget>
#include <QX11Info>
// we're using default display for now
#define GET_NATIVE_WINDOW(aWidget) static_cast<QWidget*>(aWidget->GetNativeData(NS_NATIVE_SHELLWIDGET))->handle()
#define DISPLAY QX11Info().display
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "GLContextProvider.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "GLXLibrary.h"

namespace mozilla {
namespace gl {

GLContextProvider sGLContextProvider;

PRBool
GLXLibrary::EnsureInitialized()
{
    if (mInitialized) {
        return PR_TRUE;
    }

    if (!mOGLLibrary) {
        mOGLLibrary = PR_LoadLibrary("libGL.so.1");
        if (!mOGLLibrary) {
	    NS_WARNING("Couldn't load OpenGL shared library.");
	    return PR_FALSE;
        }
    }

    LibrarySymbolLoader::SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &xCreateContext, { "glXCreateContext", NULL } },
        { (PRFuncPtr*) &xDeleteContext, { "glXDestroyContext", NULL } },
        { (PRFuncPtr*) &xMakeCurrent, { "glXMakeCurrent", NULL } },
        { (PRFuncPtr*) &xGetProcAddress, { "glXGetProcAddress", NULL } },
        { (PRFuncPtr*) &xChooseVisual, { "glXChooseVisual", NULL } },
        { (PRFuncPtr*) &xChooseFBConfig, { "glXChooseFBConfig", NULL } },
        { (PRFuncPtr*) &xCreatePbuffer, { "glXCreatePbuffer", NULL } },
        { (PRFuncPtr*) &xCreateNewContext, { "glXCreateNewContext", NULL } },
        { (PRFuncPtr*) &xDestroyPbuffer, { "glXDestroyPbuffer", NULL } },
        { (PRFuncPtr*) &xGetVisualFromFBConfig, { "glXGetVisualFromFBConfig", NULL } },
        { NULL, { NULL } }
    };

    if (!LibrarySymbolLoader::LoadSymbols(mOGLLibrary, &symbols[0])) {
        NS_WARNING("Couldn't find required entry point in OpenGL shared library");
        return PR_FALSE;
    }

    mInitialized = PR_TRUE;
    return PR_TRUE;
}

GLXLibrary sGLXLibrary;

static bool ctxErrorOccurred = false;
static int
ctxErrorHandler(Display *dpy, XErrorEvent *ev)
{
    ctxErrorOccurred = true;
    return 0;
}

class GLContextGLX : public GLContext
{
public:
    static GLContextGLX *CreateGLContext(Display *display, GLXDrawable drawable, GLXFBConfig cfg, PRBool pbuffer)
    {
        ctxErrorOccurred = false;
        int (*oldHandler)(Display *, XErrorEvent *) = XSetErrorHandler(&ctxErrorHandler);

        GLXContext context = sGLXLibrary.xCreateNewContext(display,
                                                           cfg,
                                                           GLX_RGBA_TYPE,
                                                           NULL,
                                                           True);

        XSync(display, False);
        XSetErrorHandler(oldHandler);

        if (!context || ctxErrorOccurred) {
            NS_WARNING("Failed to create GLXContext!");
            return nsnull;
        }

        GLContextGLX *glContext = new GLContextGLX(display, 
                                                   drawable, 
                                                   context,
                                                   pbuffer);
        if (!glContext->Init()) {
            return nsnull;
        }

        return glContext;
    }

    ~GLContextGLX()
    {
        if (mPBuffer) {
            sGLXLibrary.xDestroyPbuffer(mDisplay, mWindow);
        }

        sGLXLibrary.xDeleteContext(mDisplay, mContext);
    }

    PRBool Init()
    {
        MakeCurrent();
        SetupLookupFunction();
        if (!InitWithPrefix("gl", PR_TRUE)) {
            return PR_FALSE;
        }

        return IsExtensionSupported("GL_EXT_framebuffer_object");
    }

    PRBool MakeCurrent()
    {
        Bool succeeded = sGLXLibrary.xMakeCurrent(mDisplay, mWindow, mContext);
        NS_ASSERTION(succeeded, "Failed to make GL context current!");
        return succeeded;
    }

    PRBool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sGLXLibrary.xGetProcAddress;
        return PR_TRUE;
    }

    void *GetNativeData(NativeDataType aType)
    {
        switch(aType) {
        case NativeGLContext:
            return mContext;
 
        case NativePBuffer:
            if (mPBuffer) {
                return (void *)mWindow;
            }

        default:
            return nsnull;
        }
    }

private:
    GLContextGLX(Display *aDisplay, GLXDrawable aWindow, GLXContext aContext, PRBool aPBuffer = PR_FALSE)
        : mContext(aContext), 
          mDisplay(aDisplay), 
          mWindow(aWindow), 
          mPBuffer(aPBuffer) {}

    GLXContext mContext;
    Display *mDisplay;
    GLXDrawable mWindow;
    PRBool mPBuffer;
};

already_AddRefed<GLContext>
GLContextProvider::CreateForWindow(nsIWidget *aWidget)
{
    return nsnull;
}

already_AddRefed<GLContext>
GLContextProvider::CreatePBuffer(const gfxIntSize &aSize, const ContextFormat& aFormat)
{
    if (!sGLXLibrary.EnsureInitialized()) {
        return nsnull;
    }

    nsTArray<int> attribs;

#define A1_(_x)  do { attribs.AppendElement(_x); } while(0)
#define A2_(_x,_y)  do {                                                \
        attribs.AppendElement(_x);                                      \
        attribs.AppendElement(_y);                                      \
    } while(0)

    int numFormats;
    Display *display = DISPLAY();
    int xscreen = DefaultScreen(display);

    A2_(GLX_DOUBLEBUFFER, False);
    A2_(GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT);

    A2_(GLX_RED_SIZE, aFormat.red);
    A2_(GLX_GREEN_SIZE, aFormat.green);
    A2_(GLX_BLUE_SIZE, aFormat.blue);
    A2_(GLX_ALPHA_SIZE, aFormat.alpha);
    A2_(GLX_DEPTH_SIZE, aFormat.depth);
    A1_(0);

    GLXFBConfig *cfg = sGLXLibrary.xChooseFBConfig(display,
                                                   xscreen,
                                                   attribs.Elements(),
                                                   &numFormats);

    if (!cfg) {
        return nsnull;
    }
    NS_ASSERTION(numFormats > 0, "");
   
    nsTArray<int> pbattribs;
    pbattribs.AppendElement(GLX_PBUFFER_WIDTH);
    pbattribs.AppendElement(aSize.width);
    pbattribs.AppendElement(GLX_PBUFFER_HEIGHT);
    pbattribs.AppendElement(aSize.height);
    pbattribs.AppendElement(GLX_PRESERVED_CONTENTS);
    pbattribs.AppendElement(True);

    GLXPbuffer pbuffer = sGLXLibrary.xCreatePbuffer(display,
                                                    cfg[0],
                                                    pbattribs.Elements());

    if (pbuffer == 0) {
        XFree(cfg);
        return nsnull;
    }

    nsRefPtr<GLContextGLX> glContext = GLContextGLX::CreateGLContext(display,
                                                                     pbuffer,
                                                                     cfg[0],
                                                                     PR_TRUE);
    XFree(cfg);

    if (!glContext) {
        return nsnull;
    }

    return glContext.forget().get();
}

} /* namespace gl */
} /* namespace mozilla */
