/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "PluginInstanceChild.h"
#include "PluginModuleChild.h"
#include "BrowserStreamChild.h"
#include "PluginStreamChild.h"
#include "StreamNotifyChild.h"

using namespace mozilla::plugins;

#if defined(OS_LINUX)

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include "gtk2xtbin.h"

#elif defined(OS_WIN)

#include <windows.h>

#endif

namespace {

static const char*
NPNVariableToString(NPNVariable aVar)
{
#define VARSTR(v_)  case v_: return #v_

    switch(aVar) {
        VARSTR(NPNVxDisplay);
        VARSTR(NPNVxtAppContext);
        VARSTR(NPNVnetscapeWindow);
        VARSTR(NPNVjavascriptEnabledBool);
        VARSTR(NPNVasdEnabledBool);
        VARSTR(NPNVisOfflineBool);

        VARSTR(NPNVserviceManager);
        VARSTR(NPNVDOMElement);
        VARSTR(NPNVDOMWindow);
        VARSTR(NPNVToolkit);
        VARSTR(NPNVSupportsXEmbedBool);

        VARSTR(NPNVWindowNPObject);

        VARSTR(NPNVPluginElementNPObject);

        VARSTR(NPNVSupportsWindowless);

        VARSTR(NPNVprivateModeBool);

    default: return "???";
    }
#undef VARSTR
}

} /* anonymous namespace */

PluginInstanceChild::~PluginInstanceChild()
{
#if defined(OS_WIN)
  DestroyPluginWindow();
#endif
}

NPError
PluginInstanceChild::NPN_GetValue(NPNVariable aVar,
                                  void* aValue)
{
    printf ("[PluginInstanceChild] NPN_GetValue(%s)\n",
            NPNVariableToString(aVar));

    switch(aVar) {

    case NPNVSupportsWindowless:
        // FIXME/cjones report true here and use XComposite + child
        // surface to implement windowless plugins
        *((NPBool*)aValue) = false;
        return NPERR_NO_ERROR;

#if defined(OS_LINUX)
    case NPNVSupportsXEmbedBool:
        *((NPBool*)aValue) = true;
        return NPERR_NO_ERROR;

    case NPNVToolkit:
        *((NPNToolkitType*)aValue) = NPNVGtk2;
        return NPERR_NO_ERROR;

#elif defined(OS_WIN)
    case NPNVToolkit:
        return NPERR_GENERIC_ERROR;
#endif
    case NPNVjavascriptEnabledBool: {
        bool v = false;
        NPError result;
        if (!CallNPN_GetValue_NPNVjavascriptEnabledBool(&v, &result)) {
            return NPERR_GENERIC_ERROR;
        }
        *static_cast<NPBool*>(aValue) = v;
        return result;
    }

    case NPNVisOfflineBool: {
        bool v = false;
        NPError result;
        if (!CallNPN_GetValue_NPNVisOfflineBool(&v, &result)) {
            return NPERR_GENERIC_ERROR;
        }
        *static_cast<NPBool*>(aValue) = v;
        return result;
    }

    case NPNVprivateModeBool: {
        bool v = false;
        NPError result;
        if (!CallNPN_GetValue_NPNVprivateModeBool(&v, &result)) {
            return NPERR_GENERIC_ERROR;
        }
        *static_cast<NPBool*>(aValue) = v;
        return result;
    }

    default:
        printf("  unhandled var %s\n", NPNVariableToString(aVar));
        return NPERR_GENERIC_ERROR;   
    }

}

bool
PluginInstanceChild::AnswerNPP_GetValue_NPPVpluginScriptableNPObject(
                                           PPluginScriptableObjectChild** value,
                                           NPError* result)
{

    NPObject* object;
    *result = mPluginIface->getvalue(GetNPP(), NPPVpluginScriptableNPObject,
                                     &object);
    if (*result != NPERR_NO_ERROR) {
        return true;
    }

    PluginScriptableObjectChild* actor = CreateActorForNPObject(object);
    if (!actor) {
        PluginModuleChild::sBrowserFuncs.releaseobject(object);
        *result = NPERR_GENERIC_ERROR;
        return true;
    }

    PluginModuleChild::sBrowserFuncs.releaseobject(object);
    *value = actor;
    return true;
}

bool
PluginInstanceChild::AnswerNPP_HandleEvent(const NPEvent& event,
                                           int16_t* handled)
{
    // plugins might be fooling with these, make a copy
    NPEvent evcopy = event;
    *handled = mPluginIface->event(&mData, reinterpret_cast<void*>(&evcopy));
    return true;
}

bool
PluginInstanceChild::AnswerNPP_SetWindow(const NPWindow& aWindow,
                                         NPError* rv)
{
    printf("[PluginInstanceChild] NPP_SetWindow(%lx, %d, %d)\n",
           reinterpret_cast<unsigned long>(aWindow.window),
           aWindow.width, aWindow.height);

#if defined(OS_LINUX)
    // XXX/cjones: the minimum info is sent over IPC to allow this
    // code to determine the rest.  this code is possibly wrong on
    // some systems, in some conditions

    GdkNativeWindow handle = reinterpret_cast<uintptr_t>(aWindow.window);

    mWindow.window = (void*) handle;
    mWindow.width = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.type = NPWindowTypeWindow;

    mWsInfo.display = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());

    // FIXME/cjones: the code below is correct, but apparently to get
    // a valid GdkWindow*, one needs to create the gtk_plug.  but if we
    // do that, then the plugin can't plug in to plug.  a hacky solution
    // may be to create the plug, then create another socket within that
    // plug.  yuck.
#if 0
    mWsInfo.display = GDK_WINDOW_XDISPLAY(gdkWindow);
    mWsInfo.colormap =
        GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(gdkWindow));
    GdkVisual* gdkVisual = gdk_drawable_get_visual(gdkWindow);
    mWsInfo.visual = GDK_VISUAL_XVISUAL(gdkVisual);
    mWsInfo.depth = gdkVisual->depth;
#endif

    mWindow.ws_info = (void*) &mWsInfo;

    *rv = mPluginIface->setwindow(&mData, &mWindow);

#elif defined(OS_WIN)
    ReparentPluginWindow((HWND)aWindow.window);
    SizePluginWindow(aWindow.width, aWindow.height);

    mWindow.window = (void*)mPluginWindowHWND;
    mWindow.width = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.type = NPWindowTypeWindow;

    *rv = mPluginIface->setwindow(&mData, &mWindow);
    if (*rv == NPERR_NO_ERROR) {
        WNDPROC wndProc = reinterpret_cast<WNDPROC>(
            GetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC));
        if (wndProc != PluginWindowProc) {
            mPluginWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC,
                                 reinterpret_cast<LONG>(PluginWindowProc)));
        }
    }

#else
#  error Implement me for your OS
#endif

    return true;
}

bool
PluginInstanceChild::Initialize()
{
#if defined(OS_WIN)
  if (!CreatePluginWindow())
      return false;
#endif

  return true;
}

#if defined(OS_WIN)

static const TCHAR kWindowClassName[] = TEXT("GeckoPluginWindow");
static const TCHAR kPluginInstanceChildProperty[] = TEXT("PluginInstanceChildProperty");

// static
bool
PluginInstanceChild::RegisterWindowClass()
{
    static bool alreadyRegistered = false;
    if (alreadyRegistered)
        return true;

    alreadyRegistered = true;

    WNDCLASSEX wcex;
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = DummyWindowProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = GetModuleHandle(NULL);
    wcex.hIcon          = 0;
    wcex.hCursor        = 0;
    wcex.hbrBackground  = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWindowClassName;
    wcex.hIconSm        = 0;

    return RegisterClassEx(&wcex) ? true : false;
}

bool
PluginInstanceChild::CreatePluginWindow()
{
    if (!RegisterWindowClass())
        return false;

    if (!mPluginWindowHWND) {
        mPluginWindowHWND =
            CreateWindowEx(WS_EX_LEFT | WS_EX_LTRREADING |
                           WS_EX_NOPARENTNOTIFY | // XXXbent Get rid of this!
                           WS_EX_RIGHTSCROLLBAR,
                           kWindowClassName, 0,
                           WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0,
                           0, 0, NULL, 0, GetModuleHandle(NULL), 0);
        if (!mPluginWindowHWND)
            return false;
        if (!SetProp(mPluginWindowHWND, kPluginInstanceChildProperty, this))
            return false;

        // Apparently some plugins require an ASCII WndProc.
        SetWindowLongPtrA(mPluginWindowHWND, GWLP_WNDPROC,
                          reinterpret_cast<LONG>(DefWindowProcA));
    }

    return true;
}

void
PluginInstanceChild::DestroyPluginWindow()
{
    if (mPluginWindowHWND) {
        // Unsubclass the window.
        WNDPROC wndProc = reinterpret_cast<WNDPROC>(
            GetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC));
        if (wndProc == PluginWindowProc) {
            NS_ASSERTION(mPluginWndProc, "Should have old proc here!");
            SetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC,
                             reinterpret_cast<LONG>(mPluginWndProc));
            mPluginWndProc = 0;
        }

        RemoveProp(mPluginWindowHWND, kPluginInstanceChildProperty);
        DestroyWindow(mPluginWindowHWND);
        mPluginWindowHWND = 0;
    }
}

void
PluginInstanceChild::ReparentPluginWindow(HWND hWndParent)
{
    if (hWndParent != mPluginParentHWND && IsWindow(hWndParent)) {
        LONG style = GetWindowLongPtr(mPluginWindowHWND, GWL_STYLE);
        style |= WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        SetWindowLongPtr(mPluginWindowHWND, GWL_STYLE, style);
        SetParent(mPluginWindowHWND, hWndParent);
        ShowWindow(mPluginWindowHWND, SW_SHOWNA);
    }
    mPluginParentHWND = hWndParent;
}

void
PluginInstanceChild::SizePluginWindow(int width,
                                      int height)
{
    if (mPluginWindowHWND) {
        SetWindowPos(mPluginWindowHWND, NULL, 0, 0, width, height,
                     SWP_NOZORDER | SWP_NOREPOSITION);
    }
}

// See chromium's webplugin_delegate_impl.cc for explanation of this function.
// static
LRESULT CALLBACK
PluginInstanceChild::DummyWindowProc(HWND hWnd,
                                     UINT message,
                                     WPARAM wParam,
                                     LPARAM lParam)
{
    return CallWindowProc(DefWindowProc, hWnd, message, wParam, lParam);
}

// static
LRESULT CALLBACK
PluginInstanceChild::PluginWindowProc(HWND hWnd,
                                      UINT message,
                                      WPARAM wParam,
                                      LPARAM lParam)
{
    PluginInstanceChild* self = reinterpret_cast<PluginInstanceChild*>(
        GetProp(hWnd, kPluginInstanceChildProperty));
    if (!self) {
        NS_NOTREACHED("Badness!");
        return 0;
    }

    NS_ASSERTION(self->mPluginWindowHWND == hWnd, "Wrong window!");

    LRESULT res = CallWindowProc(self->mPluginWndProc, hWnd, message, wParam,
                                 lParam);

    if (message == WM_CLOSE)
        self->DestroyPluginWindow();

    if (message == WM_NCDESTROY)
        RemoveProp(hWnd, kPluginInstanceChildProperty);

    return res;
}

#endif // OS_WIN

PPluginScriptableObjectChild*
PluginInstanceChild::AllocPPluginScriptableObject()
{
    nsAutoPtr<PluginScriptableObjectChild>* object =
        mScriptableObjects.AppendElement();
    NS_ENSURE_TRUE(object, nsnull);

    *object = new PluginScriptableObjectChild();
    NS_ENSURE_TRUE(*object, nsnull);

    return object->get();
}

bool
PluginInstanceChild::DeallocPPluginScriptableObject(PPluginScriptableObjectChild* aObject)
{
    PluginScriptableObjectChild* object =
        reinterpret_cast<PluginScriptableObjectChild*>(aObject);

    PRUint32 count = mScriptableObjects.Length();
    for (PRUint32 index = 0; index < count; index++) {
        if (mScriptableObjects[index] == object) {
            mScriptableObjects.RemoveElementAt(index);
            break;
        }
    }
    return true;
}

PBrowserStreamChild*
PluginInstanceChild::AllocPBrowserStream(const nsCString& url,
                                         const uint32_t& length,
                                         const uint32_t& lastmodified,
                                         const PStreamNotifyChild* notifyData,
                                         const nsCString& headers,
                                         const nsCString& mimeType,
                                         const bool& seekable,
                                         NPError* rv,
                                         uint16_t *stype)
{
    return new BrowserStreamChild(this, url, length, lastmodified, headers,
                                  mimeType, seekable, rv, stype);
}

bool
PluginInstanceChild::AnswerPBrowserStreamDestructor(PBrowserStreamChild* stream,
                                                    const NPError& reason,
                                                    const bool& artificial)
{
    if (!artificial)
        static_cast<BrowserStreamChild*>(stream)->NPP_DestroyStream(reason);
    return true;
}

bool
PluginInstanceChild::DeallocPBrowserStream(PBrowserStreamChild* stream,
                                           const NPError& reason,
                                           const bool& artificial)
{
    delete stream;
    return true;
}

PPluginStreamChild*
PluginInstanceChild::AllocPPluginStream(const nsCString& mimeType,
                                        const nsCString& target,
                                        NPError* result)
{
    NS_RUNTIMEABORT("not callable");
    return NULL;
}

bool
PluginInstanceChild::AnswerPPluginStreamDestructor(PPluginStreamChild* stream,
                                                   const NPReason& reason,
                                                   const bool& artificial)
{
    if (!artificial) {
        static_cast<PluginStreamChild*>(stream)->NPP_DestroyStream(reason);
    }
    return true;
}

bool
PluginInstanceChild::DeallocPPluginStream(PPluginStreamChild* stream,
                                          const NPError& reason,
                                          const bool& artificial)
{
    delete stream;
    return true;
}

PStreamNotifyChild*
PluginInstanceChild::AllocPStreamNotify(const nsCString& url,
                                        const nsCString& target,
                                        const bool& post,
                                        const nsCString& buffer,
                                        const bool& file,
                                        NPError* result)
{
    NS_RUNTIMEABORT("not reached");
    return NULL;
}

bool
PluginInstanceChild::DeallocPStreamNotify(PStreamNotifyChild* notifyData,
                                          const NPReason& reason)
{
    StreamNotifyChild* sn = static_cast<StreamNotifyChild*>(notifyData);
    mPluginIface->urlnotify(&mData, sn->mURL.get(), reason, sn->mClosure);
    delete sn;

    return true;
}

PluginScriptableObjectChild*
PluginInstanceChild::CreateActorForNPObject(NPObject* aObject)
{
  NS_ASSERTION(aObject, "Null pointer!");

  PluginScriptableObjectChild* actor =
      reinterpret_cast<PluginScriptableObjectChild*>(
          CallPPluginScriptableObjectConstructor());
  NS_ENSURE_TRUE(actor, nsnull);

  actor->Initialize(this, aObject);

#ifdef DEBUG
  bool ok =
#endif
  PluginModuleChild::current()->RegisterNPObject(aObject, actor);
  NS_ASSERTION(ok, "Out of memory?");

  return actor;
}

NPError
PluginInstanceChild::NPN_NewStream(NPMIMEType aMIMEType, const char* aWindow,
                                   NPStream** aStream)
{
    PluginStreamChild* ps = new PluginStreamChild(this);

    NPError result;
    CallPPluginStreamConstructor(ps, nsDependentCString(aMIMEType),
                                 NullableString(aWindow), &result);
    if (NPERR_NO_ERROR != result) {
        *aStream = NULL;
        CallPPluginStreamDestructor(ps, NPERR_GENERIC_ERROR, true);
        return result;
    }

    *aStream = &ps->mStream;
    return NPERR_NO_ERROR;
}
