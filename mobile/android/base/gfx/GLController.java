/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011-2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
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

package org.mozilla.gecko.gfx;

import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGL11;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL;
import javax.microedition.khronos.opengles.GL10;

public class GLController {
    private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
    private static final String LOGTAG = "GeckoGLController";

    private LayerView mView;
    private int mGLVersion;
    private boolean mSurfaceValid;
    private int mWidth, mHeight;

    private EGL10 mEGL;
    private EGLDisplay mEGLDisplay;
    private EGLConfig mEGLConfig;
    private EGLContext mEGLContext;
    private EGLSurface mEGLSurface;

    private GL mGL;

    private static final int LOCAL_EGL_OPENGL_ES2_BIT = 4;

    private static final int[] CONFIG_SPEC = {
        EGL10.EGL_RED_SIZE, 5,
        EGL10.EGL_GREEN_SIZE, 6,
        EGL10.EGL_BLUE_SIZE, 5,
        EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT,
        EGL10.EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
        EGL10.EGL_NONE
    };

    public GLController(LayerView view) {
        mView = view;
        mGLVersion = 2;
        mSurfaceValid = false;
    }

    public void setGLVersion(int version) {
        mGLVersion = version;
    }

    public GL getGL()                       { return mEGLContext.getGL(); }
    public EGLDisplay getEGLDisplay()       { return mEGLDisplay;         }
    public EGLConfig getEGLConfig()         { return mEGLConfig;          }
    public EGLContext getEGLContext()       { return mEGLContext;         }
    public EGLSurface getEGLSurface()       { return mEGLSurface;         }
    public LayerView getView()              { return mView;               }

    public boolean hasSurface() {
        return mEGLSurface != null;
    }

    public boolean swapBuffers() {
        return mEGL.eglSwapBuffers(mEGLDisplay, mEGLSurface);
    }

    public boolean checkForLostContext() {
        if (mEGL.eglGetError() != EGL11.EGL_CONTEXT_LOST) {
            return false;
        }

        mEGLDisplay = null;
        mEGLConfig = null;
        mEGLContext = null;
        mEGLSurface = null;
        mGL = null;
        return true;
    }

    // Wait until we are allowed to use EGL functions on the Surface backing
    // this window.
    public synchronized void waitForValidSurface() {
        while (!mSurfaceValid) {
            try {
                wait();
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    public synchronized int getWidth() {
        return mWidth;
    }

    public synchronized int getHeight() {
        return mHeight;
    }

    synchronized void surfaceCreated() {
        mSurfaceValid = true;
        notifyAll();
    }

    synchronized void surfaceDestroyed() {
        mSurfaceValid = false;
        notifyAll();
    }

    synchronized void sizeChanged(int newWidth, int newHeight) {
        mWidth = newWidth;
        mHeight = newHeight;
        if (mGL != null) {
          mView.getRenderer().onSurfaceChanged((GL10)mGL, mWidth, mHeight);
        }
    }

    private void initEGL() {
        mEGL = (EGL10)EGLContext.getEGL();

        mEGLDisplay = mEGL.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
        if (mEGLDisplay == EGL10.EGL_NO_DISPLAY) {
            throw new GLControllerException("eglGetDisplay() failed");
        }

        mEGLConfig = chooseConfig();

        int[] attribList = { EGL_CONTEXT_CLIENT_VERSION, mGLVersion, EGL10.EGL_NONE };
        mEGLContext = mEGL.eglCreateContext(mEGLDisplay, mEGLConfig, EGL10.EGL_NO_CONTEXT,
                                            attribList);
        if (mEGLContext == null || mEGLContext == EGL10.EGL_NO_CONTEXT) {
            throw new GLControllerException("createContext() failed " +
                                            getEGLError());
        }

        mGL = mEGLContext.getGL();

        if (mView.getRenderer() != null) {
            mView.getRenderer().onSurfaceCreated((GL10)mGL, mEGLConfig);
            mView.getRenderer().onSurfaceChanged((GL10)mGL, mView.getWidth(), mView.getHeight());
        }
    }

    private EGLConfig chooseConfig() {
        int[] numConfigs = new int[1];
        if (!mEGL.eglChooseConfig(mEGLDisplay, CONFIG_SPEC, null, 0, numConfigs) ||
                numConfigs[0] <= 0) {
            throw new GLControllerException("No available EGL configurations " +
                                            getEGLError());
        }

        EGLConfig[] configs = new EGLConfig[numConfigs[0]];
        if (!mEGL.eglChooseConfig(mEGLDisplay, CONFIG_SPEC, configs, numConfigs[0], numConfigs)) {
            throw new GLControllerException("No EGL configuration for that specification " +
                                            getEGLError());
        }

        // Select the first 565 RGB configuration.
        int[] red = new int[1], green = new int[1], blue = new int[1];
        for (EGLConfig config : configs) {
            mEGL.eglGetConfigAttrib(mEGLDisplay, config, EGL10.EGL_RED_SIZE, red);
            mEGL.eglGetConfigAttrib(mEGLDisplay, config, EGL10.EGL_GREEN_SIZE, green);
            mEGL.eglGetConfigAttrib(mEGLDisplay, config, EGL10.EGL_BLUE_SIZE, blue);
            if (red[0] == 5 && green[0] == 6 && blue[0] == 5) {
                return config;
            }
        }

        throw new GLControllerException("No suitable EGL configuration found");
    }

    /**
     * Provides an EGLSurface without assuming ownership of this surface.
     * This class does not keep a reference to the provided EGL surface; the
     * caller assumes ownership of the surface once it is returned.
     */
    private EGLSurface provideEGLSurface() {
        if (mEGL == null) {
            initEGL();
        }

        SurfaceHolder surfaceHolder = mView.getHolder();
        EGLSurface surface = mEGL.eglCreateWindowSurface(mEGLDisplay, mEGLConfig, surfaceHolder, null);
        if (surface == null || surface == EGL10.EGL_NO_SURFACE) {
            throw new GLControllerException("EGL window surface could not be created! " +
                                            getEGLError());
        }

        return surface;
    }

    private String getEGLError() {
        return "Error " + mEGL.eglGetError();
    }

    public static class GLControllerException extends RuntimeException {
        public static final long serialVersionUID = 1L;

        GLControllerException(String e) {
            super(e);
        }
    }
}

