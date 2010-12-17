/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Mark Steele <mwsteele@gmail.com>
 *   Bas Schouten <bschouten@mozilla.com>
 *   Frederic Plourde <frederic.plourde@collabora.co.uk>
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

#if defined(MOZ_X11)

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdkx.h>
// we're using default display for now
#define GET_NATIVE_WINDOW(aWidget) (EGLNativeWindowType)GDK_WINDOW_XID((GdkWindow *) aWidget->GetNativeData(NS_NATIVE_WINDOW))
#elif defined(MOZ_WIDGET_QT)
#include <QWidget>
#include <QtOpenGL/QGLWidget>
#define GLdouble_defined 1
// we're using default display for now
#define GET_NATIVE_WINDOW(aWidget) (EGLNativeWindowType)static_cast<QWidget*>(aWidget->GetNativeData(NS_NATIVE_SHELLWIDGET))->handle()
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "mozilla/X11Util.h"
#include "gfxXlibSurface.h"
typedef Display *EGLNativeDisplayType;
typedef Pixmap   EGLNativePixmapType;
typedef Window   EGLNativeWindowType;

#define EGL_LIB "/usr/lib/libEGL.so"
#define GLES2_LIB "/usr/lib/libGLESv2.so"

#elif defined(ANDROID)

/* from widget */
#include "AndroidBridge.h"

typedef void *EGLNativeDisplayType;
typedef void *EGLNativePixmapType;
typedef void *EGLNativeWindowType;

#define EGL_LIB "/system/lib/libEGL.so"
#define GLES2_LIB "/system/lib/libGLESv2.so"

#elif defined(XP_WIN)

#include "nsServiceManagerUtils.h"
#include "nsIPrefBranch.h"
#include "nsILocalFile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include <windows.h>

typedef HDC EGLNativeDisplayType;
typedef HBITMAP EGLNativePixmapType;
typedef HWND EGLNativeWindowType;

#define GET_NATIVE_WINDOW(aWidget) ((EGLNativeWindowType)aWidget->GetNativeData(NS_NATIVE_WINDOW))

#define EGL_LIB "libEGL.dll"
#define GLES2_LIB "libGLESv2.dll"

// a little helper
class AutoDestroyHWND {
public:
    AutoDestroyHWND(HWND aWnd = NULL)
        : mWnd(aWnd)
    {
    }

    ~AutoDestroyHWND() {
        if (mWnd) {
            ::DestroyWindow(mWnd);
        }
    }

    operator HWND() {
        return mWnd;
    }

    HWND forget() {
        HWND w = mWnd;
        mWnd = NULL;
        return w;
    }

    HWND operator=(HWND aWnd) {
        if (mWnd && mWnd != aWnd) {
            ::DestroyWindow(mWnd);
        }
        mWnd = aWnd;
        return mWnd;
    }

    HWND mWnd;
};

#else

#error "Platform not recognized"

#endif

#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "GLContextProvider.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"

#include "nsIWidget.h"

static bool gUseBackingSurface = false;

namespace mozilla {
namespace gl {

typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef void *EGLClientBuffer;
typedef void *EGLCastToRelevantPtr;
typedef void *EGLImageKHR;
typedef void *GLeglImageOES;

#define EGL_DEFAULT_DISPLAY  ((EGLNativeDisplayType)0)
#define EGL_NO_CONTEXT       ((EGLContext)0)
#define EGL_NO_DISPLAY       ((EGLDisplay)0)
#define EGL_NO_SURFACE       ((EGLSurface)0)

#define EGL_DISPLAY()        sEGLLibrary.Display()

EGLSurface
CreateSurfaceForWindow(nsIWidget *aWidget, EGLConfig config);
EGLConfig
CreateConfig();

static int
next_power_of_two(int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

static bool
is_power_of_two(int v)
{
    NS_ASSERTION(v >= 0, "bad value");

    if (v == 0)
        return true;

    return (v & (v-1)) == 0;
}

static class EGLLibrary
{
public:
    EGLLibrary() 
        : mInitialized(PR_FALSE),
          mEGLLibrary(nsnull)
    {
        mHave_EGL_KHR_image_base = PR_FALSE;
        mHave_EGL_KHR_image_pixmap = PR_FALSE;
        mHave_EGL_KHR_gl_texture_2D_image = PR_FALSE;
        mHave_EGL_KHR_lock_surface = PR_FALSE;
    }

    typedef EGLDisplay (GLAPIENTRY * pfnGetDisplay)(void *display_id);
    pfnGetDisplay fGetDisplay;
    typedef EGLContext (GLAPIENTRY * pfnGetCurrentContext)(void);
    pfnGetCurrentContext fGetCurrentContext;
    typedef EGLBoolean (GLAPIENTRY * pfnMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
    pfnMakeCurrent fMakeCurrent;
    typedef EGLBoolean (GLAPIENTRY * pfnDestroyContext)(EGLDisplay dpy, EGLContext ctx);
    pfnDestroyContext fDestroyContext;
    typedef EGLContext (GLAPIENTRY * pfnCreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
    pfnCreateContext fCreateContext;
    typedef EGLBoolean (GLAPIENTRY * pfnDestroySurface)(EGLDisplay dpy, EGLSurface surface);
    pfnDestroySurface fDestroySurface;
    typedef EGLSurface (GLAPIENTRY * pfnCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);
    pfnCreateWindowSurface fCreateWindowSurface;
    typedef EGLSurface (GLAPIENTRY * pfnCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
    pfnCreatePbufferSurface fCreatePbufferSurface;
    typedef EGLSurface (GLAPIENTRY * pfnCreatePixmapSurface)(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list);
    pfnCreatePixmapSurface fCreatePixmapSurface;
    typedef EGLBoolean (GLAPIENTRY * pfnBindAPI)(EGLenum api);
    pfnBindAPI fBindAPI;
    typedef EGLBoolean (GLAPIENTRY * pfnInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
    pfnInitialize fInitialize;
    typedef EGLBoolean (GLAPIENTRY * pfnChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
    pfnChooseConfig fChooseConfig;
    typedef EGLint (GLAPIENTRY * pfnGetError)(void);
    pfnGetError fGetError;
    typedef EGLBoolean (GLAPIENTRY * pfnGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);
    pfnGetConfigAttrib fGetConfigAttrib;
    typedef EGLBoolean (GLAPIENTRY * pfnGetConfigs)(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
    pfnGetConfigs fGetConfigs;
    typedef EGLBoolean (GLAPIENTRY * pfnWaitNative)(EGLint engine);
    pfnWaitNative fWaitNative;
    typedef EGLCastToRelevantPtr (GLAPIENTRY * pfnGetProcAddress)(const char *procname);
    pfnGetProcAddress fGetProcAddress;
    typedef EGLBoolean (GLAPIENTRY * pfnSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
    pfnSwapBuffers fSwapBuffers;
    typedef EGLBoolean (GLAPIENTRY * pfnCopyBuffers)(EGLDisplay dpy, EGLSurface surface,
                                                     EGLNativePixmapType target);
    pfnCopyBuffers fCopyBuffers;
    typedef const GLubyte* (GLAPIENTRY * pfnQueryString)(EGLDisplay, EGLint name);
    pfnQueryString fQueryString;
    typedef EGLBoolean (GLAPIENTRY * pfnQueryContext)(EGLDisplay dpy, EGLContext ctx,
                                                      EGLint attribute, EGLint *value);
    pfnQueryContext fQueryContext;
    typedef EGLBoolean (GLAPIENTRY * pfnBindTexImage)(EGLDisplay, EGLSurface surface, EGLint buffer);
    pfnBindTexImage fBindTexImage;
    typedef EGLBoolean (GLAPIENTRY * pfnReleaseTexImage)(EGLDisplay, EGLSurface surface, EGLint buffer);
    pfnReleaseTexImage fReleaseTexImage;
    typedef EGLImageKHR (GLAPIENTRY * pfnCreateImageKHR)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
    pfnCreateImageKHR fCreateImageKHR;
    typedef EGLBoolean (GLAPIENTRY * pfnDestroyImageKHR)(EGLDisplay dpy, EGLImageKHR image);
    pfnDestroyImageKHR fDestroyImageKHR;

    // New extension which allow us to lock texture and get raw image pointer
    typedef EGLBoolean (GLAPIENTRY * pfnLockSurfaceKHR)(EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list);
    pfnLockSurfaceKHR fLockSurfaceKHR;
    typedef EGLBoolean (GLAPIENTRY * pfnUnlockSurfaceKHR)(EGLDisplay dpy, EGLSurface surface);
    pfnUnlockSurfaceKHR fUnlockSurfaceKHR;
    typedef EGLBoolean (GLAPIENTRY * pfnQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);
    pfnQuerySurface fQuerySurface;

    // This is EGL specific GL ext symbol "glEGLImageTargetTexture2DOES"
    // Lets keep it here for now.
    typedef void (GLAPIENTRY * pfnImageTargetTexture2DOES)(GLenum target, GLeglImageOES image);
    pfnImageTargetTexture2DOES fImageTargetTexture2DOES;

    PRBool EnsureInitialized()
    {
        if (mInitialized) {
            return PR_TRUE;
        }

#ifdef XP_WIN
        // Allow for explicitly specifying the location of libEGL.dll and
        // libGLESv2.dll.
        do {
            nsCOMPtr<nsIPrefBranch> prefs = do_GetService("@mozilla.org/preferences-service;1");
            nsCOMPtr<nsILocalFile> eglFile, glesv2File;
            if (!prefs)
                break;

            nsresult rv = prefs->GetComplexValue("gfx.angle.egl.path",
                                                 NS_GET_IID(nsILocalFile),
                                                 getter_AddRefs(eglFile));
            if (NS_FAILED(rv) || !eglFile)
                break;

            nsCAutoString s;

            // note that we have to load the libs in this order, because libEGL.dll
            // depends on libGLESv2.dll, but is not/may not be in our search path.
            nsCOMPtr<nsIFile> f;
            eglFile->Clone(getter_AddRefs(f));
            glesv2File = do_QueryInterface(f);
            if (!glesv2File)
                break;

            glesv2File->Append(NS_LITERAL_STRING("libGLESv2.dll"));

            PRLibrary *glesv2lib = nsnull; // this will be leaked on purpose
            glesv2File->Load(&glesv2lib);
            if (!glesv2lib)
                break;

            eglFile->Append(NS_LITERAL_STRING("libEGL.dll"));
            eglFile->Load(&mEGLLibrary);
        } while (false);
#endif

        if (!mEGLLibrary) {
            mEGLLibrary = PR_LoadLibrary(EGL_LIB);
        }

        if (!mEGLLibrary) {
            NS_WARNING("Couldn't load EGL LIB.");
            return PR_FALSE;
        }

#define SYMBOL(name) \
    { (PRFuncPtr*) &f##name, { "egl" #name, NULL } }

        LibrarySymbolLoader::SymLoadStruct earlySymbols[] = {
            SYMBOL(GetDisplay),
            SYMBOL(GetCurrentContext),
            SYMBOL(MakeCurrent),
            SYMBOL(DestroyContext),
            SYMBOL(CreateContext),
            SYMBOL(DestroySurface),
            SYMBOL(CreateWindowSurface),
            SYMBOL(CreatePbufferSurface),
            SYMBOL(CreatePixmapSurface),
            SYMBOL(BindAPI),
            SYMBOL(Initialize),
            SYMBOL(ChooseConfig),
            SYMBOL(GetError),
            SYMBOL(GetConfigs),
            SYMBOL(GetConfigAttrib),
            SYMBOL(WaitNative),
            SYMBOL(GetProcAddress),
            SYMBOL(SwapBuffers),
            SYMBOL(CopyBuffers),
            SYMBOL(QueryString),
            SYMBOL(QueryContext),
            SYMBOL(BindTexImage),
            SYMBOL(ReleaseTexImage),
            SYMBOL(QuerySurface),
            { NULL, { NULL } }
        };

        if (!LibrarySymbolLoader::LoadSymbols(mEGLLibrary, &earlySymbols[0])) {
            NS_WARNING("Couldn't find required entry points in EGL library (early init)");
            return PR_FALSE;
        }

        mEGLDisplay = fGetDisplay(EGL_DEFAULT_DISPLAY);
        if (!fInitialize(mEGLDisplay, NULL, NULL))
            return PR_FALSE;
        
        const char *extensions = (const char*) fQueryString(mEGLDisplay, LOCAL_EGL_EXTENSIONS);
        if (!extensions)
            extensions = "";

        printf_stderr("Extensions: %s 0x%02x\n", extensions, extensions[0]);
        printf_stderr("Extensions length: %d\n", strlen(extensions));

        // note the extra space -- this ugliness tries to match
        // EGL_KHR_image in the middle of the string, or right at the
        // end.  It's a prefix for other extensions, so we have to do
        // this...
        PRBool hasKHRImage = PR_FALSE;
        if (strstr(extensions, "EGL_KHR_image ") ||
            (strlen(extensions) >= strlen("EGL_KHR_image") &&
             strcmp(extensions+(strlen(extensions)-strlen("EGL_KHR_image")), "EGL_KHR_image")))
        {
            hasKHRImage = PR_TRUE;
        }

        if (strstr(extensions, "EGL_KHR_image_base")) {
            mHave_EGL_KHR_image_base = PR_TRUE;
        }
            
        if (strstr(extensions, "EGL_KHR_image_pixmap")) {
            mHave_EGL_KHR_image_pixmap = PR_TRUE;
            
        }

        if (strstr(extensions, "EGL_KHR_gl_texture_2D_image")) {
            mHave_EGL_KHR_gl_texture_2D_image = PR_TRUE;
        }

        if (strstr(extensions, "EGL_KHR_lock_surface")) {
            mHave_EGL_KHR_lock_surface = PR_TRUE;
        }

        if (hasKHRImage) {
            mHave_EGL_KHR_image_base = PR_TRUE;
            mHave_EGL_KHR_image_pixmap = PR_TRUE;
        }

        LibrarySymbolLoader::SymLoadStruct khrSymbols[] = {
            { (PRFuncPtr*) &fCreateImageKHR, { "eglCreateImageKHR", NULL } },
            { (PRFuncPtr*) &fDestroyImageKHR, { "eglDestroyImageKHR", NULL } },
            { (PRFuncPtr*) &fImageTargetTexture2DOES, { "glEGLImageTargetTexture2DOES", NULL } },
            { NULL, { NULL } }
        };

        LibrarySymbolLoader::LoadSymbols(mEGLLibrary, &khrSymbols[0],
                                         (LibrarySymbolLoader::PlatformLookupFunction)fGetProcAddress);

        if (mHave_EGL_KHR_lock_surface) {
            LibrarySymbolLoader::SymLoadStruct lockSymbols[] = {
                { (PRFuncPtr*) &fLockSurfaceKHR, { "eglLockSurfaceKHR", NULL } },
                { (PRFuncPtr*) &fUnlockSurfaceKHR, { "eglUnlockSurfaceKHR", NULL } },
                { NULL, { NULL } }
            };

            LibrarySymbolLoader::LoadSymbols(mEGLLibrary, &lockSymbols[0],
                                             (LibrarySymbolLoader::PlatformLookupFunction)fGetProcAddress);
            if (!fLockSurfaceKHR) {
                mHave_EGL_KHR_lock_surface = PR_FALSE;
            }
        }

        if (!fCreateImageKHR) {
            mHave_EGL_KHR_image_base = PR_FALSE;
            mHave_EGL_KHR_image_pixmap = PR_FALSE;
            mHave_EGL_KHR_gl_texture_2D_image = PR_FALSE;
        }

        if (!fImageTargetTexture2DOES) {
            mHave_EGL_KHR_gl_texture_2D_image = PR_FALSE;
        }

        mInitialized = PR_TRUE;
        return PR_TRUE;
    }

    EGLDisplay Display() {
        return mEGLDisplay;
    }

    PRBool HasKHRImageBase() {
        return mHave_EGL_KHR_image_base;
    }

    PRBool HasKHRImagePixmap() {
        return mHave_EGL_KHR_image_base;
    }

    PRBool HasKHRImageTexture2D() {
        return mHave_EGL_KHR_gl_texture_2D_image;
    }

    PRBool HasKHRLockSurface() {
        return mHave_EGL_KHR_lock_surface;
    }

    void
    DumpEGLConfig(EGLConfig cfg)
    {
        int attrval;
        int err;

#define ATTR(_x) do {                                                   \
            fGetConfigAttrib(mEGLDisplay, cfg, LOCAL_EGL_##_x, &attrval);  \
            if ((err = fGetError()) != 0x3000) {                        \
                printf_stderr("  %s: ERROR (0x%04x)\n", #_x, err);        \
            } else {                                                    \
                printf_stderr("  %s: %d (0x%04x)\n", #_x, attrval, attrval); \
            }                                                           \
        } while(0)

        printf_stderr("EGL Config: %d [%p]\n", (int)(intptr_t)cfg, cfg);

        ATTR(BUFFER_SIZE);
        ATTR(ALPHA_SIZE);
        ATTR(BLUE_SIZE);
        ATTR(GREEN_SIZE);
        ATTR(RED_SIZE);
        ATTR(DEPTH_SIZE);
        ATTR(STENCIL_SIZE);
        ATTR(CONFIG_CAVEAT);
        ATTR(CONFIG_ID);
        ATTR(LEVEL);
        ATTR(MAX_PBUFFER_HEIGHT);
        ATTR(MAX_PBUFFER_PIXELS);
        ATTR(MAX_PBUFFER_WIDTH);
        ATTR(NATIVE_RENDERABLE);
        ATTR(NATIVE_VISUAL_ID);
        ATTR(NATIVE_VISUAL_TYPE);
        ATTR(PRESERVED_RESOURCES);
        ATTR(SAMPLES);
        ATTR(SAMPLE_BUFFERS);
        ATTR(SURFACE_TYPE);
        ATTR(TRANSPARENT_TYPE);
        ATTR(TRANSPARENT_RED_VALUE);
        ATTR(TRANSPARENT_GREEN_VALUE);
        ATTR(TRANSPARENT_BLUE_VALUE);
        ATTR(BIND_TO_TEXTURE_RGB);
        ATTR(BIND_TO_TEXTURE_RGBA);
        ATTR(MIN_SWAP_INTERVAL);
        ATTR(MAX_SWAP_INTERVAL);
        ATTR(LUMINANCE_SIZE);
        ATTR(ALPHA_MASK_SIZE);
        ATTR(COLOR_BUFFER_TYPE);
        ATTR(RENDERABLE_TYPE);
        ATTR(CONFORMANT);

#undef ATTR
    }

private:
    PRBool mInitialized;
    PRLibrary *mEGLLibrary;
    EGLDisplay mEGLDisplay;

    PRPackedBool mHave_EGL_KHR_image_base;
    PRPackedBool mHave_EGL_KHR_image_pixmap;
    PRPackedBool mHave_EGL_KHR_gl_texture_2D_image;
    PRPackedBool mHave_EGL_KHR_lock_surface;
} sEGLLibrary;

class GLContextEGL : public GLContext
{
    friend class TextureImageEGL;

public:
    GLContextEGL(const ContextFormat& aFormat,
                 GLContext *aShareContext,
                 EGLConfig aConfig,
                 EGLSurface aSurface,
                 EGLContext aContext,
                 PRBool aIsOffscreen = PR_FALSE)
        : GLContext(aFormat, aIsOffscreen, aShareContext)
        , mConfig(aConfig) 
        , mSurface(aSurface), mContext(aContext)
        , mGLWidget(nsnull)
        , mThebesSurface(nsnull)
        , mBound(PR_FALSE)
        , mIsPBuffer(PR_FALSE)
        , mIsDoubleBuffered(PR_FALSE)
#ifdef XP_WIN
        , mWnd(0)
#endif
    {
        // any EGL contexts will always be GLESv2
        SetIsGLES2(PR_TRUE);

#ifdef DEBUG
        printf_stderr("Initializing context %p surface %p on display %p\n", mContext, mSurface, EGL_DISPLAY());
#endif
    }

    ~GLContextEGL()
    {
        MarkDestroyed();

        // If mGLWidget is non-null, then we've been given it by the GL context provider,
        // and it's managed by the widget implementation. In this case, We can't destroy
        // our contexts.
        if (mGLWidget)
            return;

#ifdef DEBUG
        printf_stderr("Destroying context %p surface %p on display %p\n", mContext, mSurface, EGL_DISPLAY());
#endif

        sEGLLibrary.fDestroyContext(EGL_DISPLAY(), mContext);
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
    }

    GLContextType GetContextType() {
        return ContextTypeEGL;
    }

    PRBool Init()
    {
        if (!OpenLibrary(GLES2_LIB)) {
            NS_WARNING("Couldn't load EGL LIB.");
            return PR_FALSE;
        }

        MakeCurrent();
        PRBool ok = InitWithPrefix("gl", PR_TRUE);
#if 0
        if (ok) {
            EGLint v;
            sEGLLibrary.fQueryContext(EGL_DISPLAY(), mContext, LOCAL_EGL_RENDER_BUFFER, &v);
            if (v == LOCAL_EGL_BACK_BUFFER)
                mIsDoubleBuffered = PR_TRUE;
        }
#endif
        return ok;
    }

    PRBool IsDoubleBuffered() {
        return mIsDoubleBuffered;
    }

    void SetIsDoubleBuffered(PRBool aIsDB) {
        mIsDoubleBuffered = aIsDB;
    }

    PRBool BindTexImage()
    {
        if (!mSurface)
            return PR_FALSE;

        if (mBound && !ReleaseTexImage())
            return PR_FALSE;

        EGLBoolean success = sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
            (EGLSurface)mSurface, LOCAL_EGL_BACK_BUFFER);
        if (success == LOCAL_EGL_FALSE)
            return PR_FALSE;

        mBound = PR_TRUE;
        return PR_TRUE;
    }

    PRBool ReleaseTexImage()
    {
        if (!mBound)
            return PR_TRUE;

        if (!mSurface)
            return PR_FALSE;

        EGLBoolean success;
        success = sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                               (EGLSurface)mSurface,
                                               LOCAL_EGL_BACK_BUFFER);
        if (success == LOCAL_EGL_FALSE)
            return PR_FALSE;

        mBound = PR_FALSE;
        return PR_TRUE;
    }

    PRBool MakeCurrentImpl(PRBool aForce = PR_FALSE) {
        PRBool succeeded = PR_TRUE;

        // Assume that EGL has the same problem as WGL does,
        // where MakeCurrent with an already-current context is
        // still expensive.
        if (aForce || sEGLLibrary.fGetCurrentContext() != mContext) {
            if (mGLWidget) {
#ifdef MOZ_WIDGET_QT
                static_cast<QGLWidget*>(mGLWidget)->makeCurrent();
#else
                succeeded = PR_FALSE;
#endif
            } else {
                succeeded = sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                                     mSurface, mSurface,
                                                     mContext);
            }
            NS_ASSERTION(succeeded, "Failed to make GL context current!");
        }

        return succeeded;
    }

#ifdef MOZ_WIDGET_QT
    virtual PRBool
    RenewSurface() {
        /* We don't support renewing on QT because we don't create the surface ourselves */
        return PR_FALSE;
    }
#else
    virtual PRBool
    RenewSurface() {
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);

        EGLConfig config = CreateConfig();
        mSurface = CreateSurfaceForWindow(NULL, config);

        return sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                        mSurface, mSurface,
                                        mContext);
    }
#endif

    PRBool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sEGLLibrary.fGetProcAddress;
        return PR_TRUE;
    }

    void *GetNativeData(NativeDataType aType)
    {
        switch (aType) {
        case NativeGLContext:
            return mContext;

        default:
            return nsnull;
        }
    }

    PRBool SwapBuffers()
    {
        return sEGLLibrary.fSwapBuffers(EGL_DISPLAY(), mSurface);
    }

    virtual already_AddRefed<TextureImage>
    CreateTextureImage(const nsIntSize& aSize,
                       TextureImage::ContentType aContentType,
                       GLenum aWrapMode,
                       PRBool aUseNearestFilter=PR_FALSE);

    // hold a reference to the given surface
    // for the lifetime of this context.
    void HoldSurface(gfxASurface *aSurf) {
        mThebesSurface = aSurf;
    }

    void SetQtGLWidget(void *widget) {
        mGLWidget = widget;
    }

    void SetIsPBuffer() {
        mIsPBuffer = PR_TRUE;
    }

    EGLContext Context() {
        return mContext;
    }

    PRBool BindTex2DOffscreen(GLContext *aOffscreen);
    void UnbindTex2DOffscreen(GLContext *aOffscreen);
    PRBool ResizeOffscreen(const gfxIntSize& aNewSize);
    void BindOffscreenFramebuffer();

    static already_AddRefed<GLContextEGL>
    CreateEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                    const ContextFormat& aFormat);

    static already_AddRefed<GLContextEGL>
    CreateEGLPBufferOffscreenContext(const gfxIntSize& aSize,
                                     const ContextFormat& aFormat);

#ifdef XP_WIN
    static already_AddRefed<GLContextEGL>
    CreateEGLWin32OffscreenContext(const gfxIntSize& aSize,
                                   const ContextFormat& aFormat);

    void HoldWin32Window(HWND aWnd) { mWnd = aWnd; }
    HWND GetWin32Window() { return mWnd; }
#endif

    void SetOffscreenSize(const gfxIntSize &aRequestedSize,
                          const gfxIntSize &aActualSize)
    {
        mOffscreenSize = aRequestedSize;
        mOffscreenActualSize = aActualSize;
    }

protected:
    friend class GLContextProviderEGL;

    EGLConfig  mConfig;
    EGLSurface mSurface;
    EGLContext mContext;
    void *mGLWidget;
    nsRefPtr<gfxASurface> mThebesSurface;
    PRBool mBound;

    PRPackedBool mIsPBuffer;
    PRPackedBool mIsDoubleBuffered;

#ifdef XP_WIN
    AutoDestroyHWND mWnd;
#endif
};

PRBool
GLContextEGL::BindTex2DOffscreen(GLContext *aOffscreen)
{
    if (aOffscreen->GetContextType() != ContextTypeEGL) {
        NS_WARNING("non-EGL context");
        return PR_FALSE;
    }

    GLContextEGL *offs = static_cast<GLContextEGL*>(aOffscreen);

    if (offs->mIsPBuffer) {
        PRBool ok = sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
                                              offs->mSurface,
                                              LOCAL_EGL_BACK_BUFFER);
        return ok;
    }

    if (offs->mOffscreenTexture) {
        if (offs->GetSharedContext() != GLContextProviderEGL::GetGlobalContext())
        {
            NS_WARNING("offscreen FBO context can only be bound with context sharing!");
            return PR_FALSE;
        }

        fBindTexture(LOCAL_GL_TEXTURE_2D, offs->mOffscreenTexture);
        return PR_TRUE;
    }

    NS_WARNING("don't know how to bind this!");

    return PR_FALSE;
}

void
GLContextEGL::UnbindTex2DOffscreen(GLContext *aOffscreen)
{
    NS_ASSERTION(aOffscreen->GetContextType() == ContextTypeEGL, "wrong type");

    GLContextEGL *offs = static_cast<GLContextEGL*>(aOffscreen);

    if (offs->mIsPBuffer) {
        sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                     offs->mSurface,
                                     LOCAL_EGL_BACK_BUFFER);
    }
}

PRBool
GLContextEGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
    if (mIsPBuffer) {
        EGLint pbattrs[] = {
            LOCAL_EGL_WIDTH, 0,
            LOCAL_EGL_HEIGHT, 0,
            LOCAL_EGL_TEXTURE_TARGET, LOCAL_EGL_TEXTURE_2D,

            LOCAL_EGL_TEXTURE_FORMAT,
            mCreationFormat.minAlpha ?
              LOCAL_EGL_TEXTURE_RGBA :
              LOCAL_EGL_TEXTURE_RGB,

            LOCAL_EGL_NONE
        };

        EGLSurface surface = nsnull;
        gfxIntSize pbsize(aNewSize);

TRY_AGAIN_POWER_OF_TWO:
        pbattrs[1] = pbsize.width;
        pbattrs[3] = pbsize.height;

        surface = sEGLLibrary.fCreatePbufferSurface(EGL_DISPLAY(), mConfig, pbattrs);
        if (!surface) {
            if (!is_power_of_two(pbsize.width) ||
                !is_power_of_two(pbsize.height))
            {
                if (!is_power_of_two(pbsize.width))
                    pbsize.width = next_power_of_two(pbsize.width);
                if (!is_power_of_two(pbsize.height))
                    pbsize.height = next_power_of_two(pbsize.height);

                NS_WARNING("Failed to resize pbuffer, trying power of two dims");
                goto TRY_AGAIN_POWER_OF_TWO;
            }

            NS_WARNING("Failed to resize pbuffer");
            return nsnull;
        }

        SetOffscreenSize(aNewSize, pbsize);

        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);

        mSurface = surface;

        MakeCurrent(PR_TRUE);
        ClearSafely();

        return PR_TRUE;
    }

    return ResizeOffscreenFBO(aNewSize);
}


static GLContextEGL *
GetGlobalContextEGL()
{
    return static_cast<GLContextEGL*>(GLContextProviderEGL::GetGlobalContext());
}

static GLenum
GLFormatForImage(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxASurface::ImageFormatARGB32:
        return LOCAL_GL_RGBA;
    case gfxASurface::ImageFormatRGB24:
        // this often isn't correct, because we can't guarantee that
        // the alpha byte will be 0xff coming from the image surface
        NS_WARNING("Using GL_RGBA for ImageFormatRGB24, are you sure you know what you're doing?");
        return LOCAL_GL_RGBA;
    case gfxASurface::ImageFormatRGB16_565:
        return LOCAL_GL_RGB;
    default:
        NS_WARNING("Unknown GL format for Image format");
    }
    return 0;
}

static GLenum
GLTypeForImage(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxASurface::ImageFormatARGB32:
    case gfxASurface::ImageFormatRGB24:
        return LOCAL_GL_UNSIGNED_BYTE;
    case gfxASurface::ImageFormatRGB16_565:
        return LOCAL_GL_UNSIGNED_SHORT_5_6_5;
    default:
        NS_WARNING("Unknown GL format for Image format");
    }
    return 0;
}

class TextureImageEGL
    : public TextureImage
{
public:
    TextureImageEGL(GLuint aTexture,
                    const nsIntSize& aSize,
                    GLenum aWrapMode,
                    ContentType aContentType,
                    GLContext* aContext)
        : TextureImage(aTexture, aSize, aWrapMode, aContentType)
        , mGLContext(aContext)
        , mUpdateFormat(gfxASurface::ImageFormatUnknown)
        , mSurface(nsnull)
        , mConfig(nsnull)
        , mImageKHR(nsnull)
        , mCreated(PR_FALSE)
        , mBound(PR_FALSE)
        , mIsLocked(PR_FALSE)
    {
        mUpdateFormat = gfxASurface::FormatFromContent(GetContentType());

        // Convert RGB24 to either ARGB32 on mobile.  We can't
        // generate GL_RGB data, so we'll always have an alpha byte
        // for RGB24.  No easy way to upload that to GL.
        // 
        // Note that if we start using RGB565 here, we'll need to
        // watch for a) setting mIsRGBFormat to TRUE; and b) getting
        // the stride right.
        if (mUpdateFormat == gfxASurface::ImageFormatRGB24) {
            mUpdateFormat = gfxASurface::ImageFormatARGB32;
        }

        if (gUseBackingSurface) {
            CreateBackingSurface(gfxIntSize(aSize.width, aSize.height));
        }

        // We currently always use BGRA type textures
        mShaderType = BGRALayerProgramType;
    }

    virtual ~TextureImageEGL()
    {
        GLContext *ctx = mGLContext;
        if (ctx->IsDestroyed() || !NS_IsMainThread()) {
            ctx = ctx->GetSharedContext();
        }

        // If we have a context, then we need to delete the texture;
        // if we don't have a context (either real or shared),
        // then they went away when the contex was deleted, because it
        // was the only one that had access to it.
        if (ctx && !ctx->IsDestroyed()) {
            mGLContext->MakeCurrent();
            mGLContext->fDeleteTextures(1, &mTexture);
            ReleaseTexImage();
            DestroyEGLSurface();
        }
    }

    virtual gfxContext* BeginUpdate(nsIntRegion& aRegion)
    {
        NS_ASSERTION(!mUpdateContext, "BeginUpdate() without EndUpdate()?");

        // determine the region the client will need to repaint
        if (!mCreated) {
            // if the texture hasn't been initialized yet, force the
            // client to paint everything
            mUpdateRect = nsIntRect(nsIntPoint(0, 0), mSize);
            //printf_stderr("v Forcing full paint\n");
        } else {
            mUpdateRect = aRegion.GetBounds();
        }

        //printf_stderr("BeginUpdate with updateRect [%d %d %d %d]\n", mUpdateRect.x, mUpdateRect.y, mUpdateRect.width, mUpdateRect.height);

        // We can only draw a rectangle, not subregions due to
        // the way that our texture upload functions work.  If
        // needed, we /could/ do multiple texture uploads if we have
        // non-overlapping rects, but that's a tradeoff.
        aRegion = nsIntRegion(mUpdateRect);

        nsIntSize rgnSize = mUpdateRect.Size();
        if (!nsIntRect(nsIntPoint(0, 0), mSize).Contains(mUpdateRect)) {
            NS_ERROR("update outside of image");
            return NULL;
        }

        if (mBackingSurface) {
            if (sEGLLibrary.HasKHRLockSurface()) {
                nsRefPtr<gfxASurface> surface = GetLockSurface();
                mUpdateContext = new gfxContext(surface);
                mUpdateContext->Rectangle(gfxRect(mUpdateRect.x, mUpdateRect.y, mUpdateRect.width, mUpdateRect.height));
                mUpdateContext->Clip();
            } else {
                mUpdateContext = new gfxContext(mBackingSurface);
            }
            return mUpdateContext;
        }

        // if we get this far, then we're using Cairo's byte order
        mIsRGBFormat = PR_FALSE;

        //printf_stderr("creating image surface %dx%d format %d\n", mUpdateRect.width, mUpdateRect.height, mUpdateFormat);

        nsRefPtr<gfxASurface> updateSurface =
            new gfxImageSurface(gfxIntSize(mUpdateRect.width, mUpdateRect.height),
                                mUpdateFormat);

        updateSurface->SetDeviceOffset(gfxPoint(-mUpdateRect.x, -mUpdateRect.y));
        mUpdateContext = new gfxContext(updateSurface);

        return mUpdateContext;
    }

    virtual PRBool EndUpdate()
    {
        NS_ASSERTION(!!mUpdateContext, "EndUpdate() without BeginUpdate()?");

        if (mIsLocked) {
            UnlockSurface();
            mCreated = PR_TRUE;
            mUpdateContext = nsnull;
            return PR_FALSE;
        }

        if (mBackingSurface && mUpdateContext->OriginalSurface() == mBackingSurface) {
#ifdef MOZ_X11
            if (mBackingSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
                XSync(DefaultXDisplay(), False);
            }
#endif

            mBackingSurface->SetDeviceOffset(gfxPoint(0, 0));
            mCreated = PR_TRUE;
            mUpdateContext = nsnull;
            return PR_FALSE;
        }

        //printf_stderr("EndUpdate: slow path");

        // This is the slower path -- we didn't have any way to set up
        // a fast mapping between our cairo target surface and the GL
        // texture, so we have to upload data.
        nsRefPtr<gfxASurface> originalSurface = mUpdateContext->OriginalSurface();

        // Undo the device offset that BeginUpdate set; doesn't much
        // matter for us here, but important if we ever do anything
        // directly with the surface.
        originalSurface->SetDeviceOffset(gfxPoint(0, 0));

        nsRefPtr<gfxImageSurface> uploadImage = nsnull;
        gfxIntSize updateSize(mUpdateRect.width, mUpdateRect.height);

        NS_ASSERTION(originalSurface->GetType() == gfxASurface::SurfaceTypeImage &&
                     originalSurface->GetSize() == updateSize,
                     "Upload image isn't an image surface when one is expected, or is wrong size!");

        uploadImage = static_cast<gfxImageSurface*>(originalSurface.get());

        if (!uploadImage) {
            return PR_FALSE;
        }

        mGLContext->MakeCurrent();
        mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

        if (!mCreated) {
            NS_ASSERTION(mUpdateRect.x == 0 && mUpdateRect.y == 0 &&
                         mUpdateRect.Size() == mSize,
                         "Bad initial update on non-created texture!");

            mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                                    0,
                                    GLFormatForImage(mUpdateFormat),
                                    mUpdateRect.width,
                                    mUpdateRect.height,
                                    0,
                                    GLFormatForImage(uploadImage->Format()),
                                    GLTypeForImage(uploadImage->Format()),
                                    uploadImage->Data());
            mCreated = PR_TRUE;
        } else {
            mGLContext->fTexSubImage2D(LOCAL_GL_TEXTURE_2D,
                                       0,
                                       mUpdateRect.x,
                                       mUpdateRect.y,
                                       mUpdateRect.width,
                                       mUpdateRect.height,
                                       GLFormatForImage(uploadImage->Format()),
                                       GLTypeForImage(uploadImage->Format()),
                                       uploadImage->Data());
        }

        mUpdateContext = nsnull;
        return PR_TRUE;         // mTexture is bound
    }

    virtual bool DirectUpdate(gfxASurface *aSurf, const nsIntRegion& aRegion)
    {
        nsIntRect bounds = aRegion.GetBounds();
        nsIntPoint dest = bounds.TopLeft();

        // Bounds is the destination rect, it will be at 0,0 on the source
        bounds.x = 0;
        bounds.y = 0;
  
        if (!mCreated) {
            bounds = nsIntRect(0, 0, mSize.width, mSize.height);
        }

        mShaderType =
          mGLContext->UploadSurfaceToTexture(aSurf,
                                             bounds,
                                             mTexture,
                                             !mCreated,
                                             dest,
                                             PR_FALSE);
        mCreated = PR_TRUE;
        return true;
    }

    virtual PRBool InUpdate() const { return !!mUpdateContext; }

    virtual void Resize(const nsIntSize& aSize)
    {
        NS_ASSERTION(!mUpdateContext, "Resize() while in update?");

        if (mSize == aSize && mCreated)
            return;

        if (mBackingSurface) {
            CreateBackingSurface(gfxIntSize(aSize.width, aSize.height));
        } else {
            mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
            mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                                    0,
                                    GLFormatForImage(mUpdateFormat),
                                    aSize.width,
                                    aSize.height,
                                    0,
                                    GLFormatForImage(mUpdateFormat),
                                    GLTypeForImage(mUpdateFormat),
                                    NULL);
        }

        mCreated = PR_TRUE;
        mSize = aSize;
    }

    PRBool BindTexImage()
    {
        if (mBound && !ReleaseTexImage())
            return PR_FALSE;

        EGLBoolean success =
            sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
                                      (EGLSurface)mSurface,
                                      LOCAL_EGL_BACK_BUFFER);

        if (success == LOCAL_EGL_FALSE)
            return PR_FALSE;

        mBound = PR_TRUE;
        return PR_TRUE;
    }

    PRBool ReleaseTexImage()
    {
        if (!mBound)
            return PR_TRUE;

        EGLBoolean success =
            sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                         (EGLSurface)mSurface,
                                         LOCAL_EGL_BACK_BUFFER);

        if (success == LOCAL_EGL_FALSE)
            return PR_FALSE;

        mBound = PR_FALSE;
        return PR_TRUE;
    }

    virtual already_AddRefed<gfxImageSurface> GetLockSurface()
    {
        if (mIsLocked) {
            NS_WARNING("Can't lock surface twice");
            return nsnull;
        }

        if (!sEGLLibrary.HasKHRLockSurface()) {
            NS_WARNING("GetLockSurface called, but no EGL_KHR_lock_surface extension!");
            return nsnull;
        }

        if (!CreateEGLSurface(mBackingSurface)) {
            NS_WARNING("Failed to create EGL surface");
            return nsnull;
        }

        static EGLint lock_attribs[] = {
            LOCAL_EGL_MAP_PRESERVE_PIXELS_KHR, LOCAL_EGL_TRUE,
            LOCAL_EGL_LOCK_USAGE_HINT_KHR, LOCAL_EGL_READ_SURFACE_BIT_KHR | LOCAL_EGL_WRITE_SURFACE_BIT_KHR,
            LOCAL_EGL_NONE
        };

        sEGLLibrary.fLockSurfaceKHR(EGL_DISPLAY(), mSurface, lock_attribs);

        mIsLocked = PR_TRUE;

        unsigned char *data = nsnull;
        int pitch = 0;
        int pixsize = 0;

        sEGLLibrary.fQuerySurface(EGL_DISPLAY(), mSurface, LOCAL_EGL_BITMAP_POINTER_KHR, (EGLint*)&data);
        sEGLLibrary.fQuerySurface(EGL_DISPLAY(), mSurface, LOCAL_EGL_BITMAP_PITCH_KHR, &pitch);
        sEGLLibrary.fQuerySurface(EGL_DISPLAY(), mSurface, LOCAL_EGL_BITMAP_PIXEL_SIZE_KHR, &pixsize);

        nsRefPtr<gfxImageSurface> sharedImage =
            new gfxImageSurface(data,
                                mBackingSurface->GetSize(),
                                pitch,
                                mUpdateFormat);

        return sharedImage.forget();
    }

    virtual void UnlockSurface()
    {
        if (!mIsLocked) {
            NS_WARNING("UnlockSurface called, surface not locked!");
            return;
        }

        sEGLLibrary.fUnlockSurfaceKHR(EGL_DISPLAY(), mSurface);
        DestroyEGLSurface();
        mIsLocked = PR_FALSE;
    }

    virtual already_AddRefed<gfxASurface> GetBackingSurface()
    {
        if (mBackingSurface) {
            NS_ADDREF(mBackingSurface);
        }
        return mBackingSurface.get();
    }

    virtual PRBool CreateEGLSurface(gfxASurface* aSurface)
    {
#if 0

        if (!aSurface) {
            NS_WARNING("no surface");
            return PR_FALSE;
        }

        if (aSurface->GetType() != gfxASurface::SurfaceTypeXlib) {
            NS_WARNING("wrong surface type, must be xlib");
            return PR_FALSE;
        }

        if (mSurface) {
            NS_WARNING("surface already exists");
            return PR_TRUE;
        }

        EGLSurface surface = FindEGLSurfaceforXSurface(aSurface, &mConfig);

        if (!surface) {
            NS_WARNING("couldn't find X config for surface");
            return PR_FALSE;
        }

        mSurface = surface;
        return PR_TRUE;
#else
        return PR_FALSE;
#endif
    }

    virtual void DestroyEGLSurface(void)
    {
        if (!mSurface)
            return;

        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
        mSurface = nsnull;
    }

    virtual PRBool CreateBackingSurface(const gfxIntSize& aSize)
    {
        ReleaseTexImage();
        DestroyEGLSurface();
        mBackingSurface = nsnull;

#ifdef MOZ_X11
        Display* dpy = DefaultXDisplay();
        XRenderPictFormat* renderFMT =
            gfxXlibSurface::FindRenderFormat(dpy, mUpdateFormat);

        nsRefPtr<gfxXlibSurface> xsurface =
            gfxXlibSurface::Create(DefaultScreenOfDisplay(dpy),
                                   renderFMT,
                                   gfxIntSize(aSize.width, aSize.height));

        XSync(dpy, False);
        mConfig = nsnull;

        if (sEGLLibrary.HasKHRImagePixmap() && sEGLLibrary.HasKHRImageTexture2D()) {
            mImageKHR =
                sEGLLibrary.fCreateImageKHR(EGL_DISPLAY(),
                                            EGL_NO_CONTEXT,
                                            LOCAL_EGL_NATIVE_PIXMAP_KHR,
                                            (EGLClientBuffer)xsurface->XDrawable(),
                                            NULL);

            if (!mImageKHR) {
                printf_stderr("couldn't create EGL image: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return PR_FALSE;
            }
            mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
            sEGLLibrary.fImageTargetTexture2DOES(LOCAL_GL_TEXTURE_2D, mImageKHR);
            sEGLLibrary.fDestroyImageKHR(EGL_DISPLAY(), mImageKHR);
            mImageKHR = NULL;
        } else {
            if (!CreateEGLSurface(xsurface)) {
                printf_stderr("ProviderEGL Failed create EGL surface: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return PR_FALSE;
            }

            if (!BindTexImage()) {
                printf_stderr("ProviderEGL Failed to bind teximage: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return PR_FALSE;
            }
        }

        mBackingSurface = xsurface;
        mIsRGBFormat = PR_TRUE;
#endif

        return mBackingSurface != nsnull;
    }

protected:
    typedef gfxASurface::gfxImageFormat ImageFormat;

    GLContext* mGLContext;
    nsRefPtr<gfxContext> mUpdateContext;

    nsIntRect mUpdateRect;
    ImageFormat mUpdateFormat;
    nsRefPtr<gfxASurface> mBackingSurface;
    EGLSurface mSurface;
    EGLConfig mConfig;
    EGLImageKHR mImageKHR;

    PRPackedBool mCreated;
    PRPackedBool mBound;
    PRPackedBool mIsLocked;
};

already_AddRefed<TextureImage>
GLContextEGL::CreateTextureImage(const nsIntSize& aSize,
                                 TextureImage::ContentType aContentType,
                                 GLenum aWrapMode,
                                 PRBool aUseNearestFilter)
{
  MakeCurrent();

  GLuint texture;
  fGenTextures(1, &texture);

  fActiveTexture(LOCAL_GL_TEXTURE0);
  fBindTexture(LOCAL_GL_TEXTURE_2D, texture);

  nsRefPtr<TextureImageEGL> teximage =
      new TextureImageEGL(texture, aSize, aWrapMode, aContentType, this);

  GLint texfilter = aUseNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, aWrapMode);
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, aWrapMode);

  return teximage.forget();
}

static ContextFormat
DepthToGLFormat(int aDepth)
{
    switch (aDepth) {
        case 32:
            return ContextFormat::BasicRGBA32;
        case 24:
            return ContextFormat::BasicRGB24;
        case 16:
            return ContextFormat::BasicRGB16_565;
        default:
            break;
    }
    return ContextFormat::BasicRGBA32;
}


#ifdef MOZ_WIDGET_QT
already_AddRefed<GLContext>
GLContextProviderEGL::CreateForWindow(nsIWidget *aWidget)
{
    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    QWidget *viewport = static_cast<QWidget*>(aWidget->GetNativeData(NS_NATIVE_SHELLWIDGET));
    if (!viewport)
        return nsnull;

    if (viewport->paintEngine()->type() == QPaintEngine::OpenGL2) {
        // Qt widget viewport already have GL context created by Qt
        // Create dummy GLContextEGL class
        nsRefPtr<GLContextEGL> glContext =
            new GLContextEGL(ContextFormat(DepthToGLFormat(viewport->depth())),
                             NULL,
                             NULL, NULL,
                             sEGLLibrary.fGetCurrentContext());
        if (!glContext->Init())
            return nsnull;
        glContext->SetQtGLWidget(viewport);

        return glContext.forget();
    }

    // All Qt nsIWidget's have the same X-Window surface
    // And EGL not allowing to create multiple GL context for the same window
    // we should be able to create GL context for QGV viewport once, and reuse it for all child widgets
    NS_ERROR("Need special GLContext implementation for QT widgets structure");

    // Switch to software rendering here
    return nsnull;
}

#else

EGLConfig
CreateConfig()
{
    EGLConfig  config;
    EGLint attribs[] = {
        LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
        LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
        LOCAL_EGL_RED_SIZE,        5,
        LOCAL_EGL_GREEN_SIZE,      6,
        LOCAL_EGL_BLUE_SIZE,       5,
        LOCAL_EGL_ALPHA_SIZE,      0,
#else
        LOCAL_EGL_RED_SIZE,        8,
        LOCAL_EGL_GREEN_SIZE,      8,
        LOCAL_EGL_BLUE_SIZE,       8,
        LOCAL_EGL_ALPHA_SIZE,      8,
#endif

        LOCAL_EGL_NONE
    };

    EGLConfig configs[64];
    EGLint ncfg = 64;
    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(), attribs, configs, ncfg, &ncfg) ||
        ncfg < 1)
    {
        return nsnull;
    }

    config = 0;

    for (int i = 0; i < ncfg; ++i) {
        EGLint r, g, b, a;

        sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), configs[i], LOCAL_EGL_RED_SIZE, &r);
        sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), configs[i], LOCAL_EGL_GREEN_SIZE, &g);
        sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), configs[i], LOCAL_EGL_BLUE_SIZE, &b);
        sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), configs[i], LOCAL_EGL_ALPHA_SIZE, &a);

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
        if (r == 5 && g == 6 && b == 5) {
            config = configs[i];
            break;
        }
#else
        if (r == 8 && g == 8 && b == 8 && a == 8) {
            config = configs[i];
            break;
        }
#endif
    }

    return config;
}

EGLSurface
CreateSurfaceForWindow(nsIWidget *aWidget, EGLConfig config)
{
    EGLSurface surface;


#ifdef DEBUG
    sEGLLibrary.DumpEGLConfig(config);
#endif

#ifdef ANDROID
    // On Android, we have to ask Java to make the eglCreateWindowSurface
    // call for us.  See GLHelpers.java for a description of why.
    //
    // We also only have one true "window", so we just use it directly and ignore
    // what was passed in.
    printf_stderr("... requesting window surface from bridge\n");
    surface = mozilla::AndroidBridge::Bridge()->
        CallEglCreateWindowSurface(EGL_DISPLAY(), config,
                                   mozilla::AndroidBridge::Bridge()->SurfaceView());
    printf_stderr("got surface %p\n", surface);
#else
    surface = sEGLLibrary.fCreateWindowSurface(EGL_DISPLAY(), config, GET_NATIVE_WINDOW(aWidget), 0);
#endif

    return surface;
}

already_AddRefed<GLContext>
GLContextProviderEGL::CreateForWindow(nsIWidget *aWidget)
{
    EGLContext context;
    EGLConfig config;

    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    config = CreateConfig();

    if (!config) {
        printf_stderr("Failed to create EGL config!\n");
        return nsnull;
    }

    EGLSurface surface = CreateSurfaceForWindow(aWidget, config);

    if (!surface) {
        return nsnull;
    }

    if (!sEGLLibrary.fBindAPI(LOCAL_EGL_OPENGL_ES_API)) {
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    EGLint cxattribs[] = {
        LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2,
        LOCAL_EGL_NONE
    };

    GLContextEGL *shareContext = GetGlobalContextEGL();

TRY_AGAIN_NO_SHARING:
    context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                         config,
                                         shareContext ? shareContext->mContext : EGL_NO_CONTEXT,
                                         cxattribs);
    if (!context) {
        if (shareContext) {
            NS_WARNING("CreateForWindow -- couldn't share, trying again");
            shareContext = nsnull;
            goto TRY_AGAIN_NO_SHARING;
        }

        NS_WARNING("CreateForWindow -- no context, giving up");
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    nsRefPtr<GLContextEGL> glContext = new GLContextEGL(ContextFormat(ContextFormat::BasicRGB24),
                                                        shareContext,
                                                        config, surface, context);

    if (!glContext->Init())
        return nsnull;

#if defined(XP_WIN) || defined(ANDROID)
    glContext->SetIsDoubleBuffered(PR_TRUE);
#endif

    return glContext.forget();
}
#endif

already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLPBufferOffscreenContext(const gfxIntSize& aSize,
                                               const ContextFormat& aFormat)
{
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLint attribs[] = {
        LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
        LOCAL_EGL_SURFACE_TYPE, LOCAL_EGL_PBUFFER_BIT,

        LOCAL_EGL_RED_SIZE, aFormat.red,
        LOCAL_EGL_GREEN_SIZE, aFormat.green,
        LOCAL_EGL_BLUE_SIZE, aFormat.blue,
        LOCAL_EGL_ALPHA_SIZE, aFormat.alpha,
        LOCAL_EGL_DEPTH_SIZE, aFormat.minDepth,
        LOCAL_EGL_STENCIL_SIZE, aFormat.minStencil,

        aFormat.minAlpha ?
          LOCAL_EGL_BIND_TO_TEXTURE_RGBA :
          LOCAL_EGL_BIND_TO_TEXTURE_RGB,
        LOCAL_EGL_TRUE,

        LOCAL_EGL_NONE
    };

    EGLConfig configs[64];
    int numConfigs = 64;

    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(),
                                   attribs,
                                   configs, numConfigs,
                                   &numConfigs)
        || numConfigs == 0)
    {
        NS_WARNING("No configs");
        // no configs? no pbuffers!
        return nsnull;
    }

    // XXX do some smarter matching here
    config = configs[0];
#ifdef DEBUG
    sEGLLibrary.DumpEGLConfig(config);
#endif

    gfxIntSize pbsize(aSize);

    EGLint pbattrs[] = {
        LOCAL_EGL_WIDTH, 0,
        LOCAL_EGL_HEIGHT, 0,

        LOCAL_EGL_TEXTURE_TARGET, LOCAL_EGL_TEXTURE_2D,

        LOCAL_EGL_TEXTURE_FORMAT,
        aFormat.minAlpha ?
          LOCAL_EGL_TEXTURE_RGBA :
          LOCAL_EGL_TEXTURE_RGB,

        LOCAL_EGL_NONE
    };

TRY_AGAIN_POWER_OF_TWO:
    pbattrs[1] = pbsize.width;
    pbattrs[3] = pbsize.height;

    surface = sEGLLibrary.fCreatePbufferSurface(EGL_DISPLAY(), config, pbattrs);
    if (!surface) {
        if (!is_power_of_two(pbsize.width) ||
            !is_power_of_two(pbsize.height))
        {
            if (!is_power_of_two(pbsize.width))
                pbsize.width = next_power_of_two(pbsize.width);
            if (!is_power_of_two(pbsize.height))
                pbsize.height = next_power_of_two(pbsize.height);

            NS_WARNING("Failed to create pbuffer, trying power of two dims");
            goto TRY_AGAIN_POWER_OF_TWO;
        }

        NS_WARNING("Failed to create pbuffer");
        return nsnull;
    }

    sEGLLibrary.fBindAPI(LOCAL_EGL_OPENGL_ES_API);

    EGLint cxattrs[] = {
        LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2,
        LOCAL_EGL_NONE
    };

    context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                         config,
                                         EGL_NO_CONTEXT,
                                         cxattrs);
    if (!context) {
        NS_WARNING("Failed to create context");
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    nsRefPtr<GLContextEGL> glContext = new GLContextEGL(aFormat, nsnull,
                                                        config, surface, context,
                                                        PR_TRUE);

    if (!glContext->Init()) {
        return nsnull;
    }

    glContext->SetOffscreenSize(aSize, pbsize);

    glContext->SetIsPBuffer();

    return glContext.forget();
}

#ifdef MOZ_X11
static EGLConfig
FindConfigForThebesXSurface(gfxASurface *aSurface, EGLSurface *aRetSurface)
{
    gfxXlibSurface *xsurface = static_cast<gfxXlibSurface*>(aSurface);

    EGLConfig configs[32];
    int numConfigs = 32;
    EGLSurface surface = nsnull;

    EGLint pixmap_config[] = {
        LOCAL_EGL_SURFACE_TYPE,         LOCAL_EGL_PIXMAP_BIT,
        LOCAL_EGL_RENDERABLE_TYPE,      LOCAL_EGL_OPENGL_ES2_BIT,
        LOCAL_EGL_DEPTH_SIZE,           0,
        LOCAL_EGL_BIND_TO_TEXTURE_RGB,  LOCAL_EGL_TRUE,
        LOCAL_EGL_NONE
    };

    EGLint pixmap_config_rgb[] = {
        LOCAL_EGL_TEXTURE_TARGET,       LOCAL_EGL_TEXTURE_2D,
        LOCAL_EGL_TEXTURE_FORMAT,       LOCAL_EGL_TEXTURE_RGB,
        LOCAL_EGL_NONE
    };

    EGLint pixmap_config_rgba[] = {
        LOCAL_EGL_TEXTURE_TARGET,       LOCAL_EGL_TEXTURE_2D,
        LOCAL_EGL_TEXTURE_FORMAT,       LOCAL_EGL_TEXTURE_RGBA,
        LOCAL_EGL_NONE
    };

    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(), pixmap_config,
                                   configs, numConfigs, &numConfigs))
        return nsnull;

    if (numConfigs == 0)
        return nsnull;

    PRBool opaque =
        aSurface->GetContentType() == gfxASurface::CONTENT_COLOR;
    int i = 0;
    for (i = 0; i < numConfigs; ++i) {
        if (opaque)
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), configs[i],
                                                       xsurface->XDrawable(),
                                                       pixmap_config_rgb);
        else
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), configs[i],
                                                       xsurface->XDrawable(),
                                                       pixmap_config_rgba);

        if (surface != EGL_NO_SURFACE)
            break;
    }

    if (!surface) {
        return nsnull;
    }

    if (aRetSurface) {
        *aRetSurface = surface;
    }

    return configs[i];
}
#endif

already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                              const ContextFormat& aFormat)
{
    // XXX -- write me.
    // This needs to find a FBConfig/Visual that matches aFormat, and allocate
    // a gfxXlibSurface of the appropriat format, and then create a context
    // for it.
    //
    // The code below is almost correct, except it doesn't do the format-FBConfig
    // matching, instead just creating a random gfxXlibSurface.  The code below just
    // uses context sharing and a FBO target, when instead it should avoid context
    // sharing if some form of texture-from-pixmap functionality is available.

    gfxASurface *thebesSurface = nsnull;
    EGLNativePixmapType pixmap = 0;

#ifdef MOZ_X11
    nsRefPtr<gfxXlibSurface> xsurface =
        gfxXlibSurface::Create(DefaultScreenOfDisplay(DefaultXDisplay()),
                               gfxXlibSurface::FindRenderFormat(DefaultXDisplay(),
                                                                gfxASurface::ImageFormatRGB24),
                               gfxIntSize(16, 16));
    if (xsurface->CairoStatus() != 0)
        return nsnull;

    thebesSurface = xsurface;
    pixmap = xsurface->XDrawable();
#endif

    if (!pixmap) {
        return nsnull;
    }

    EGLSurface surface;
    EGLConfig config = 0;

#ifdef MOZ_X11
    config = FindConfigForThebesXSurface(thebesSurface,
                                         &surface);
#endif
    if (!config) {
        return nsnull;
    }

    EGLint cxattribs[] = {
        LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2,
        LOCAL_EGL_NONE
    };

    GLContextEGL *shareContext = GetGlobalContextEGL();
    if (!shareContext) {
        // we depend on context sharing currently
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    EGLContext context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                                    config,
                                                    shareContext->Context(),
                                                    cxattribs);
    if (!context) {
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    nsRefPtr<GLContextEGL> glContext = new GLContextEGL(aFormat, shareContext,
                                                        config, surface, context,
                                                        PR_TRUE);

    if (!glContext->Init() ||
        !glContext->ResizeOffscreenFBO(aSize))
        return nsnull;

    glContext->HoldSurface(thebesSurface);

    return glContext.forget();
}

#ifdef XP_WIN
already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLWin32OffscreenContext(const gfxIntSize& aSize,
                                             const ContextFormat& aFormat)
{
    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    WNDCLASSW wc;
    if (!GetClassInfoW(GetModuleHandle(NULL), L"ANGLEContextClass", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASSW));
        wc.style = CS_OWNDC;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = L"ANGLEContextClass";
        if (!RegisterClassW(&wc)) {
            NS_WARNING("Failed to register ANGLEContextClass?!");
            return NULL;
        }
    }

    AutoDestroyHWND wnd = CreateWindowW(L"ANGLEContextClass", L"ANGLEContext", 0,
                                        0, 0, 16, 16,
                                        NULL, NULL, GetModuleHandle(NULL), NULL);
    NS_ENSURE_TRUE(HWND(wnd), NULL);

    EGLConfig  config;
    EGLSurface surface;
    EGLContext context;

    // We don't really care, we're going to use a FBO anyway
    EGLint attribs[] = {
        LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
        LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
        LOCAL_EGL_NONE
    };

    EGLint ncfg = 1;
    if (!sEGLLibrary.fChooseConfig(sEGLLibrary.Display(), attribs, &config, ncfg, &ncfg) ||
        ncfg < 1)
    {
        return nsnull;
    }

    surface = sEGLLibrary.fCreateWindowSurface(sEGLLibrary.Display(),
                                               config,
                                               HWND(wnd),
                                               0);
    if (!surface) {
        return nsnull;
    }

    if (!sEGLLibrary.fBindAPI(LOCAL_EGL_OPENGL_ES_API)) {
        sEGLLibrary.fDestroySurface(sEGLLibrary.Display(), surface);
        return nsnull;
    }

    EGLint cxattribs[] = {
        LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2,
        LOCAL_EGL_NONE
    };
    context = sEGLLibrary.fCreateContext(sEGLLibrary.Display(),
                                         config,
                                         EGL_NO_CONTEXT,
                                         cxattribs);
    if (!context) {
        sEGLLibrary.fDestroySurface(sEGLLibrary.Display(), surface);
        return nsnull;
    }

    nsRefPtr<GLContextEGL> glContext = new GLContextEGL(aFormat, nsnull,
                                                        config, surface, context,
                                                        PR_TRUE);

    // hold this even before we initialize, because we need to make
    // sure it gets destroyed after the surface etc. in case of error.
    glContext->HoldWin32Window(wnd.forget());

    if (!glContext->Init() ||
        !glContext->ResizeOffscreenFBO(aSize))
    {
        return nsnull;
    }

    return glContext.forget();
}
#endif

// Under EGL, if we're under X11, then we have to create a Pixmap
// because Maemo's EGL implementation doesn't support pbuffers at all
// for some reason.  On Android, pbuffers are supported fine, though
// often without the ability to texture from them directly.
already_AddRefed<GLContext>
GLContextProviderEGL::CreateOffscreen(const gfxIntSize& aSize,
                                      const ContextFormat& aFormat)
{
    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

#if defined(ANDROID)
    return GLContextEGL::CreateEGLPBufferOffscreenContext(aSize, aFormat);
#elif defined(MOZ_X11)
    return GLContextEGL::CreateEGLPixmapOffscreenContext(aSize, aFormat);
#elif defined(XP_WIN)
    return GLContextEGL::CreateEGLWin32OffscreenContext(aSize, aFormat);
#else
    return nsnull;
#endif
}

static ContextFormat
ContentTypeToGLFormat(gfxASurface::gfxContentType aCType)
{
    switch (aCType) {
        case gfxASurface::CONTENT_COLOR_ALPHA:
            return ContextFormat::BasicRGBA32;
        case gfxASurface::CONTENT_COLOR:
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
            return ContextFormat::BasicRGB16_565;
#else
            return ContextFormat::BasicRGB24;
#endif
        default:
            break;
    }
    return ContextFormat::BasicRGBA32;
}

already_AddRefed<GLContext>
GLContextProviderEGL::CreateForNativePixmapSurface(gfxASurface* aSurface)
{
    EGLSurface surface = nsnull;
    EGLContext context = nsnull;

    if (!sEGLLibrary.EnsureInitialized())
        return nsnull;

#ifdef MOZ_X11
    if (aSurface->GetType() != gfxASurface::SurfaceTypeXlib) {
        // Not implemented
        return nsnull;
    }

    EGLConfig config = FindConfigForThebesXSurface(aSurface, &surface);
    if (!config) {
        return nsnull;
    }

    EGLint cxattribs[] = {
        LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2,
        LOCAL_EGL_NONE
    };

    context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                         config,
                                         EGL_NO_SURFACE,
                                         cxattribs);
    if (!context) {
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    nsRefPtr<GLContextEGL> glContext =
        new GLContextEGL(ContextFormat(ContentTypeToGLFormat(aSurface->GetContentType())),
                         nsnull, config, surface, context, PR_FALSE);
    glContext->HoldSurface(aSurface);

    return glContext.forget().get();
#else
    (void)surface;
    (void)context;

    // Not implemented
    return nsnull;
#endif
}

static nsRefPtr<GLContext> gGlobalContext;

GLContext *
GLContextProviderEGL::GetGlobalContext()
{
    static bool triedToCreateContext = false;
    if (!triedToCreateContext && !gGlobalContext) {
        triedToCreateContext = true;
        gGlobalContext = CreateOffscreen(gfxIntSize(16, 16));
        if (gGlobalContext)
            gGlobalContext->SetIsGlobalSharedContext(PR_TRUE);
    }

    return gGlobalContext;
}

void
GLContextProviderEGL::Shutdown()
{
    gGlobalContext = nsnull;
}

} /* namespace gl */
} /* namespace mozilla */

