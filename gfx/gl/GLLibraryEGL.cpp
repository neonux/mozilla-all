/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLLibraryEGL.h"

#include "gfxCrashReporterUtils.h"
#include "mozilla/Preferences.h"

#define EGL_LIB "libEGL.so"
#define EGL_LIB1 "libEGL.so.1"

#if defined(XP_WIN)

#define EGL_LIB "libEGL.dll"

#endif

namespace mozilla {
namespace gl {

static PRLibrary* LoadApitraceLibrary()
{
    static PRLibrary* sApitraceLibrary = NULL;

    if (sApitraceLibrary)
        return sApitraceLibrary;

#if defined(ANDROID)
    nsCString logFile = Preferences::GetCString("gfx.apitrace.logfile");

    if (logFile.IsEmpty()) {
        logFile = "firefox.trace";
    }

    // The firefox process can't write to /data/local, but it can write
    // to $GRE_HOME/
    nsCAutoString logPath;
    logPath.AppendPrintf("%s/%s", getenv("GRE_HOME"), logFile.get());

    // apitrace uses the TRACE_FILE environment variable to determine where
    // to log trace output to
    printf_stderr("Logging GL tracing output to %s", logPath.get());
    setenv("TRACE_FILE", logPath.get(), false);

    printf_stderr("Attempting load of %s\n", APITRACE_LIB);

    sApitraceLibrary = PR_LoadLibrary(APITRACE_LIB);
#endif

    return sApitraceLibrary;
}

bool
GLLibraryEGL::EnsureInitialized()
{
    if (mInitialized) {
        return true;
    }

    mozilla::ScopedGfxFeatureReporter reporter("EGL");

#ifdef XP_WIN
    // Allow for explicitly specifying the location of libEGL.dll and
    // libGLESv2.dll.
    do {
        nsCOMPtr<nsILocalFile> eglFile, glesv2File;
        nsresult rv = Preferences::GetComplex("gfx.angle.egl.path",
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
        mEGLLibrary = LoadApitraceLibrary();

        if (!mEGLLibrary) {
            printf_stderr("Attempting load of %s\n", EGL_LIB);
            mEGLLibrary = PR_LoadLibrary(EGL_LIB);
#if defined(XP_UNIX)
            if (!mEGLLibrary) {
                mEGLLibrary = PR_LoadLibrary(EGL_LIB1);
            }
#endif
        }
    }

    if (!mEGLLibrary) {
        NS_WARNING("Couldn't load EGL LIB.");
        return false;
    }

#define SYMBOL(name) \
{ (PRFuncPtr*) &mSymbols.f##name, { "egl" #name, NULL } }

    GLLibraryLoader::SymLoadStruct earlySymbols[] = {
        SYMBOL(GetDisplay),
        SYMBOL(GetCurrentSurface),
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

    if (!GLLibraryLoader::LoadSymbols(mEGLLibrary, &earlySymbols[0])) {
        NS_WARNING("Couldn't find required entry points in EGL library (early init)");
        return false;
    }

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    mEGLDisplay = fGetDisplay((EGLNativeDisplayType) gdk_x11_get_default_xdisplay());
#else
    mEGLDisplay = fGetDisplay(EGL_DEFAULT_DISPLAY);
#endif
    if (!fInitialize(mEGLDisplay, NULL, NULL))
        return false;

    const char *vendor = (const char*) fQueryString(mEGLDisplay, LOCAL_EGL_VENDOR);
    if (vendor && (strstr(vendor, "TransGaming") != 0 || strstr(vendor, "Google Inc.") != 0)) {
        mIsANGLE = true;
    }
    
    const char *extensions = (const char*) fQueryString(mEGLDisplay, LOCAL_EGL_EXTENSIONS);
    if (!extensions)
        extensions = "";

    printf_stderr("Extensions: %s 0x%02x\n", extensions, extensions[0]);
    printf_stderr("Extensions length: %d\n", strlen(extensions));

    // note the extra space -- this ugliness tries to match
    // EGL_KHR_image in the middle of the string, or right at the
    // end.  It's a prefix for other extensions, so we have to do
    // this...
    bool hasKHRImage = false;
    if (strstr(extensions, "EGL_KHR_image ") ||
        (strlen(extensions) >= strlen("EGL_KHR_image") &&
         strcmp(extensions+(strlen(extensions)-strlen("EGL_KHR_image")), "EGL_KHR_image")))
    {
        hasKHRImage = true;
    }

    if (strstr(extensions, "EGL_KHR_image_base")) {
        mHave_EGL_KHR_image_base = true;
    }
        
    if (strstr(extensions, "EGL_KHR_image_pixmap")) {
        mHave_EGL_KHR_image_pixmap = true;
        
    }

    if (strstr(extensions, "EGL_KHR_gl_texture_2D_image")) {
        mHave_EGL_KHR_gl_texture_2D_image = true;
    }

    if (strstr(extensions, "EGL_KHR_lock_surface")) {
        mHave_EGL_KHR_lock_surface = true;
    }

    if (hasKHRImage) {
        GLLibraryLoader::SymLoadStruct khrSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCreateImageKHR, { "eglCreateImageKHR", NULL } },
            { (PRFuncPtr*) &mSymbols.fDestroyImageKHR, { "eglDestroyImageKHR", NULL } },
            { (PRFuncPtr*) &mSymbols.fImageTargetTexture2DOES, { "glEGLImageTargetTexture2DOES", NULL } },
            { NULL, { NULL } }
        };

        GLLibraryLoader::LoadSymbols(mEGLLibrary, &khrSymbols[0],
                                         (GLLibraryLoader::PlatformLookupFunction)mSymbols.fGetProcAddress);
    }

    if (mHave_EGL_KHR_lock_surface) {
        GLLibraryLoader::SymLoadStruct lockSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fLockSurfaceKHR, { "eglLockSurfaceKHR", NULL } },
            { (PRFuncPtr*) &mSymbols.fUnlockSurfaceKHR, { "eglUnlockSurfaceKHR", NULL } },
            { NULL, { NULL } }
        };

        GLLibraryLoader::LoadSymbols(mEGLLibrary, &lockSymbols[0],
                                         (GLLibraryLoader::PlatformLookupFunction)mSymbols.fGetProcAddress);
        if (!mSymbols.fLockSurfaceKHR) {
            mHave_EGL_KHR_lock_surface = false;
        }
    }

    if (!mSymbols.fCreateImageKHR) {
        mHave_EGL_KHR_image_base = false;
        mHave_EGL_KHR_image_pixmap = false;
        mHave_EGL_KHR_gl_texture_2D_image = false;
    }

    if (!mSymbols.fImageTargetTexture2DOES) {
        mHave_EGL_KHR_gl_texture_2D_image = false;
    }

    if (strstr(extensions, "EGL_ANGLE_surface_d3d_texture_2d_share_handle")) {
        GLLibraryLoader::SymLoadStruct d3dSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fQuerySurfacePointerANGLE, { "eglQuerySurfacePointerANGLE", NULL } },
            { NULL, { NULL } }
        };

        GLLibraryLoader::LoadSymbols(mEGLLibrary, &d3dSymbols[0],
                                         (GLLibraryLoader::PlatformLookupFunction)mSymbols.fGetProcAddress);
        if (mSymbols.fQuerySurfacePointerANGLE) {
            mHave_EGL_ANGLE_surface_d3d_texture_2d_share_handle = true;
        }
    }

    if (strstr(extensions, "EGL_EXT_create_context_robustness")) {
        mHasRobustness = true;
    }

    mInitialized = true;
    reporter.SetSuccessful();
    return true;
}

void
GLLibraryEGL::DumpEGLConfig(EGLConfig cfg)
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

void
GLLibraryEGL::DumpEGLConfigs()
{
    int nc = 0;
    fGetConfigs(mEGLDisplay, NULL, 0, &nc);
    EGLConfig *ec = new EGLConfig[nc];
    fGetConfigs(mEGLDisplay, ec, nc, &nc);

    for (int i = 0; i < nc; ++i) {
        printf_stderr ("========= EGL Config %d ========\n", i);
        DumpEGLConfig(ec[i]);
    }

    delete [] ec;
}

} /* namespace gl */
} /* namespace mozilla */

