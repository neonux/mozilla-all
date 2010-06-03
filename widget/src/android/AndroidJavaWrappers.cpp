/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "AndroidJavaWrappers.h"
#include "AndroidBridge.h"

using namespace mozilla;

jclass AndroidGeckoEvent::jGeckoEventClass = 0;
jfieldID AndroidGeckoEvent::jActionField = 0;
jfieldID AndroidGeckoEvent::jTypeField = 0;
jfieldID AndroidGeckoEvent::jTimeField = 0;
jfieldID AndroidGeckoEvent::jP0Field = 0;
jfieldID AndroidGeckoEvent::jP1Field = 0;
jfieldID AndroidGeckoEvent::jXField = 0;
jfieldID AndroidGeckoEvent::jYField = 0;
jfieldID AndroidGeckoEvent::jZField = 0;
jfieldID AndroidGeckoEvent::jRectField = 0;
jfieldID AndroidGeckoEvent::jNativeWindowField = 0;

jfieldID AndroidGeckoEvent::jCharactersField = 0;
jfieldID AndroidGeckoEvent::jKeyCodeField = 0;
jfieldID AndroidGeckoEvent::jMetaStateField = 0;
jfieldID AndroidGeckoEvent::jFlagsField = 0;
jfieldID AndroidGeckoEvent::jUnicodeCharField = 0;
jfieldID AndroidGeckoEvent::jCountField = 0;
jfieldID AndroidGeckoEvent::jCount2Field = 0;

jclass AndroidPoint::jPointClass = 0;
jfieldID AndroidPoint::jXField = 0;
jfieldID AndroidPoint::jYField = 0;

jclass AndroidRect::jRectClass = 0;
jfieldID AndroidRect::jBottomField = 0;
jfieldID AndroidRect::jLeftField = 0;
jfieldID AndroidRect::jRightField = 0;
jfieldID AndroidRect::jTopField = 0;

jclass AndroidGeckoSurfaceView::jGeckoSurfaceViewClass = 0;
jmethodID AndroidGeckoSurfaceView::jBeginDrawingMethod = 0;
jmethodID AndroidGeckoSurfaceView::jEndDrawingMethod = 0;
jmethodID AndroidGeckoSurfaceView::jGetSoftwareDrawBufferMethod = 0;

#define JNI()  (AndroidBridge::JNI())

#define initInit() jclass jClass

// note that this also sets jClass
#define getClassGlobalRef(cname) \
    (jClass = jclass(jEnv->NewGlobalRef(jEnv->FindClass(cname))))

#define getField(fname, ftype) \
    ((jfieldID) jEnv->GetFieldID(jClass, fname, ftype))

#define getMethod(fname, ftype) \
    ((jmethodID) jEnv->GetMethodID(jClass, fname, ftype))

void
mozilla::InitAndroidJavaWrappers(JNIEnv *jEnv)
{
    AndroidGeckoEvent::InitGeckoEventClass(jEnv);
    AndroidGeckoSurfaceView::InitGeckoSurfaceViewClass(jEnv);
    AndroidPoint::InitPointClass(jEnv);
}

void
AndroidGeckoEvent::InitGeckoEventClass(JNIEnv *jEnv)
{
    initInit();

    jGeckoEventClass = getClassGlobalRef("org/mozilla/gecko/GeckoEvent");

    jActionField = getField("mAction", "I");
    jTypeField = getField("mType", "I");
    jTimeField = getField("mTime", "J");
    jP0Field = getField("mP0", "Landroid/graphics/Point;");
    jP1Field = getField("mP1", "Landroid/graphics/Point;");
    jXField = getField("mX", "F");
    jYField = getField("mY", "F");
    jZField = getField("mZ", "F");
    jRectField = getField("mRect", "Landroid/graphics/Rect;");
    jNativeWindowField = getField("mNativeWindow", "I");

    jCharactersField = getField("mCharacters", "Ljava/lang/String;");
    jKeyCodeField = getField("mKeyCode", "I");
    jMetaStateField = getField("mMetaState", "I");
    jFlagsField = getField("mFlags", "I");
    jUnicodeCharField = getField("mUnicodeChar", "I");
    jCountField = getField("mCount", "I");
    jCount2Field = getField("mCount2", "I");
}

void
AndroidGeckoSurfaceView::InitGeckoSurfaceViewClass(JNIEnv *jEnv)
{
    initInit();

    jGeckoSurfaceViewClass = getClassGlobalRef("org/mozilla/gecko/GeckoSurfaceView");

    jBeginDrawingMethod = getMethod("beginDrawing", "()I");
    jGetSoftwareDrawBufferMethod = getMethod("getSoftwareDrawBuffer", "()Ljava/nio/ByteBuffer;");
    jEndDrawingMethod = getMethod("endDrawing", "()V");
}

void
AndroidPoint::InitPointClass(JNIEnv *jEnv)
{
    initInit();

    jPointClass = getClassGlobalRef("android/graphics/Point");

    jXField = getField("x", "I");
    jYField = getField("y", "I");
}

void
AndroidRect::InitRectClass(JNIEnv *jEnv)
{
    initInit();

    jRectClass = getClassGlobalRef("android/graphics/Rect");

    jBottomField = getField("bottom", "I");
    jLeftField = getField("left", "I");
    jTopField = getField("top", "I");
    jRightField = getField("right", "I");
}

#undef initInit
#undef initClassGlobalRef
#undef getField
#undef getMethod

void
AndroidGeckoEvent::ReadP0Field(JNIEnv *jenv)
{
    AndroidPoint p0(jenv, jenv->GetObjectField(wrappedObject(), jP0Field));
    mP0.x = p0.X();
    mP0.y = p0.Y();
}

void
AndroidGeckoEvent::ReadP1Field(JNIEnv *jenv)
{
    AndroidPoint p1(jenv, jenv->GetObjectField(wrappedObject(), jP1Field));
    mP1.x = p1.X();
    mP1.y = p1.Y();
}

void
AndroidGeckoEvent::ReadRectField(JNIEnv *jenv)
{
    AndroidRect r(jenv, jenv->GetObjectField(wrappedObject(), jRectField));
    if (!r.isNull()) {
        mRect.SetRect(r.Left(),
                      r.Top(),
                      r.Right() - r.Left(),
                      r.Bottom() - r.Top());
    } else {
        mRect.Empty();
    }
}

void
AndroidGeckoEvent::ReadCharactersField(JNIEnv *jenv)
{
    jstring s = (jstring) jenv->GetObjectField(wrapped_obj, jCharactersField);
    if (!s) {
        mCharacters.SetIsVoid(PR_TRUE);
        return;
    }

    int len = jenv->GetStringLength(s);
    mCharacters.SetLength(len);
    jenv->GetStringRegion(s, 0, len, mCharacters.BeginWriting());
}

void
AndroidGeckoEvent::Init(JNIEnv *jenv, jobject jobj)
{
    NS_ASSERTION(!wrapped_obj, "Init called on non-null wrapped_obj!");

    wrapped_obj = jobj;

    if (!jobj)
        return;

    mAction = jenv->GetIntField(jobj, jActionField);
    mType = jenv->GetIntField(jobj, jTypeField);
    mNativeWindow = (void*) jenv->GetIntField(jobj, jNativeWindowField);

    switch (mType) {
        case SIZE_CHANGED:
            ReadP0Field(jenv);
            ReadP1Field(jenv);
            break;

        case KEY_EVENT:
            mTime = jenv->GetLongField(jobj, jTimeField);
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            mFlags = jenv->GetIntField(jobj, jFlagsField);
            mKeyCode = jenv->GetIntField(jobj, jKeyCodeField);
            mUnicodeChar = jenv->GetIntField(jobj, jUnicodeCharField);
            ReadCharactersField(jenv);
            break;

        case MOTION_EVENT:
            mTime = jenv->GetLongField(jobj, jTimeField);
            ReadP0Field(jenv);
            break;

        case IME_EVENT:
            mCount = jenv->GetIntField(jobj, jCountField);
            mCount2 = jenv->GetIntField(jobj, jCount2Field);
            ReadCharactersField(jenv);
            break;

        case DRAW:
            ReadRectField(jenv);
            break;

        case SENSOR_EVENT:
            mX = jenv->GetFloatField(jobj, jXField);
            mY = jenv->GetFloatField(jobj, jYField);
            mZ = jenv->GetFloatField(jobj, jZField);
            break;

        default:
            break;
    }

#ifndef ANDROID_DEBUG_EVENTS
    ALOG("AndroidGeckoEvent: %p : %d %p", (void*)jobj, mType, (void*)mNativeWindow);
#endif
}

void
AndroidGeckoEvent::Init(int aType)
{
    mType = aType;
}

void
AndroidGeckoEvent::Init(void *window, int x1, int y1, int x2, int y2)
{
    mType = DRAW;
    mNativeWindow = window;
    mRect.Empty();
}

void
AndroidGeckoSurfaceView::Init(jobject jobj)
{
    NS_ASSERTION(wrapped_obj == nsnull, "Init called on non-null wrapped_obj!");

    wrapped_obj = jobj;
}

int
AndroidGeckoSurfaceView::BeginDrawing()
{
    NS_ASSERTION(!isNull(), "BeginDrawing called on null surfaceview!");

    return JNI()->CallIntMethod(wrapped_obj, jBeginDrawingMethod);
}

void
AndroidGeckoSurfaceView::EndDrawing()
{
    JNI()->CallVoidMethod(wrapped_obj, jEndDrawingMethod);
}

unsigned char *
AndroidGeckoSurfaceView::GetSoftwareDrawBuffer(int *cap)
{
    jobject buf = JNI()->CallObjectMethod(wrapped_obj, jGetSoftwareDrawBufferMethod);
    if (!buf)
        return nsnull;

    void *bp = JNI()->GetDirectBufferAddress(buf);
    jlong blen = JNI()->GetDirectBufferCapacity(buf);

    if (!bp || blen == -1)
        return nsnull;

    if (cap)
        *cap = blen;

    return (unsigned char*) bp;
}

void
AndroidPoint::Init(JNIEnv *jenv, jobject jobj)
{
    NS_ASSERTION(wrapped_obj == nsnull, "Init called on non-null wrapped_obj!");

    wrapped_obj = jobj;

    if (jobj) {
        mX = jenv->GetIntField(jobj, jXField);
        mY = jenv->GetIntField(jobj, jYField);
    } else {
        mX = 0;
        mY = 0;
    }
}

void
AndroidRect::Init(JNIEnv *jenv, jobject jobj)
{
    NS_ASSERTION(wrapped_obj == nsnull, "Init called on non-null wrapped_obj!");

    wrapped_obj = jobj;

    if (jobj) {
        mTop = jenv->GetIntField(jobj, jTopField);
        mLeft = jenv->GetIntField(jobj, jLeftField);
        mRight = jenv->GetIntField(jobj, jRightField);
        mBottom = jenv->GetIntField(jobj, jBottomField);
    } else {
        mTop = 0;
        mLeft = 0;
        mRight = 0;
        mBottom = 0;
    }
}
