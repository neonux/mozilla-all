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
* Patrick Walton <pcwalton@mozilla.com>
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

#include "AndroidFlexViewWrapper.h"


static AndroidGLController sController;

template<>
const char *AndroidEGLDisplay::sClassName = "com/google/android/gles_jni/EGLDisplayImpl";
template<>
const char *AndroidEGLDisplay::sPointerFieldName = "mEGLDisplay";
template<>
jfieldID AndroidEGLDisplay::jPointerField = 0;
template<>
const char *AndroidEGLConfig::sClassName = "com/google/android/gles_jni/EGLConfigImpl";
template<>
const char *AndroidEGLConfig::sPointerFieldName = "mEGLConfig";
template<>
jfieldID AndroidEGLConfig::jPointerField = 0;
template<>
const char *AndroidEGLContext::sClassName = "com/google/android/gles_jni/EGLContextImpl";
template<>
const char *AndroidEGLContext::sPointerFieldName = "mEGLContext";
template<>
jfieldID AndroidEGLContext::jPointerField = 0;
template<>
const char *AndroidEGLSurface::sClassName = "com/google/android/gles_jni/EGLSurfaceImpl";
template<>
const char *AndroidEGLSurface::sPointerFieldName = "mEGLSurface";
template<>
jfieldID AndroidEGLSurface::jPointerField = 0;

jmethodID AndroidGLController::jSetGLVersionMethod = 0;
jmethodID AndroidGLController::jInitGLContextMethod = 0;
jmethodID AndroidGLController::jDisposeGLContextMethod = 0;
jmethodID AndroidGLController::jGetEGLDisplayMethod = 0;
jmethodID AndroidGLController::jGetEGLConfigMethod = 0;
jmethodID AndroidGLController::jGetEGLContextMethod = 0;
jmethodID AndroidGLController::jGetEGLSurfaceMethod = 0;
jmethodID AndroidGLController::jHasSurfaceMethod = 0;
jmethodID AndroidGLController::jSwapBuffersMethod = 0;
jmethodID AndroidGLController::jCheckForLostContextMethod = 0;
jmethodID AndroidGLController::jWaitForValidSurfaceMethod = 0;
jmethodID AndroidGLController::jGetWidthMethod = 0;
jmethodID AndroidGLController::jGetHeightMethod = 0;

void
AndroidGLController::Init(JNIEnv *aJEnv)
{
    const char *className = "org/mozilla/gecko/gfx/GLController";
    jclass jClass = reinterpret_cast<jclass>(aJEnv->NewGlobalRef(aJEnv->FindClass(className)));

    jSetGLVersionMethod = aJEnv->GetMethodID(jClass, "setGLVersion", "(I)V");
    jInitGLContextMethod = aJEnv->GetMethodID(jClass, "initGLContext", "()V");
    jDisposeGLContextMethod = aJEnv->GetMethodID(jClass, "disposeGLContext", "()V");
    jGetEGLDisplayMethod = aJEnv->GetMethodID(jClass, "getEGLDisplay",
                                              "()Ljavax/microedition/khronos/egl/EGLDisplay;");
    jGetEGLConfigMethod = aJEnv->GetMethodID(jClass, "getEGLConfig",
                                             "()Ljavax/microedition/khronos/egl/EGLConfig;");
    jGetEGLContextMethod = aJEnv->GetMethodID(jClass, "getEGLContext",
                                              "()Ljavax/microedition/khronos/egl/EGLContext;");
    jGetEGLSurfaceMethod = aJEnv->GetMethodID(jClass, "getEGLSurface",
                                              "()Ljavax/microedition/khronos/egl/EGLSurface;");
    jHasSurfaceMethod = aJEnv->GetMethodID(jClass, "hasSurface", "()Z");
    jSwapBuffersMethod = aJEnv->GetMethodID(jClass, "swapBuffers", "()Z");
    jCheckForLostContextMethod = aJEnv->GetMethodID(jClass, "checkForLostContext", "()Z");
    jWaitForValidSurfaceMethod = aJEnv->GetMethodID(jClass, "waitForValidSurface", "()V");
    jGetWidthMethod = aJEnv->GetMethodID(jClass, "getWidth", "()I");
    jGetHeightMethod = aJEnv->GetMethodID(jClass, "getHeight", "()I");
}

void
AndroidGLController::Acquire(JNIEnv* aJEnv, jobject aJObj)
{
    mJEnv = aJEnv;
    mJObj = aJEnv->NewGlobalRef(aJObj);
}

void
AndroidGLController::Acquire(JNIEnv* aJEnv)
{
    mJEnv = aJEnv;
}

void
AndroidGLController::Release()
{
    if (mJObj) {
        mJEnv->DeleteGlobalRef(mJObj);
        mJObj = NULL;
    }

    mJEnv = NULL;
}

void
AndroidGLController::SetGLVersion(int aVersion)
{
    mJEnv->CallVoidMethod(mJObj, jSetGLVersionMethod, aVersion);
}

void
AndroidGLController::InitGLContext()
{
    mJEnv->CallVoidMethod(mJObj, jInitGLContextMethod);
}

void
AndroidGLController::DisposeGLContext()
{
    mJEnv->CallVoidMethod(mJObj, jDisposeGLContextMethod);
}

EGLDisplay
AndroidGLController::GetEGLDisplay()
{
    AndroidEGLDisplay jEGLDisplay(mJEnv, mJEnv->CallObjectMethod(mJObj, jGetEGLDisplayMethod));
    return *jEGLDisplay;
}

EGLConfig
AndroidGLController::GetEGLConfig()
{
    AndroidEGLConfig jEGLConfig(mJEnv, mJEnv->CallObjectMethod(mJObj, jGetEGLConfigMethod));
    return *jEGLConfig;
}

EGLContext
AndroidGLController::GetEGLContext()
{
    AndroidEGLContext jEGLContext(mJEnv, mJEnv->CallObjectMethod(mJObj, jGetEGLContextMethod));
    return *jEGLContext;
}

EGLSurface
AndroidGLController::GetEGLSurface()
{
    AndroidEGLSurface jEGLSurface(mJEnv, mJEnv->CallObjectMethod(mJObj, jGetEGLSurfaceMethod));
    return *jEGLSurface;
}

bool
AndroidGLController::HasSurface()
{
    return mJEnv->CallBooleanMethod(mJObj, jHasSurfaceMethod);
}

bool
AndroidGLController::SwapBuffers()
{
    return mJEnv->CallBooleanMethod(mJObj, jSwapBuffersMethod);
}

bool
AndroidGLController::CheckForLostContext()
{
    return mJEnv->CallBooleanMethod(mJObj, jCheckForLostContextMethod);
}

void
AndroidGLController::WaitForValidSurface()
{
    mJEnv->CallVoidMethod(mJObj, jWaitForValidSurfaceMethod);
}

int
AndroidGLController::GetWidth()
{
    return mJEnv->CallIntMethod(mJObj, jGetWidthMethod);
}

int
AndroidGLController::GetHeight()
{
    return mJEnv->CallIntMethod(mJObj, jGetHeightMethod);
}


