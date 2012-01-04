/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 sts=4 tw=80 et: */
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
 * The Original Code is Gonk.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
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

#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "nscore.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Services.h"
#include "nsAppShell.h"
#include "nsGkAtoms.h"
#include "nsGUIEvent.h"
#include "nsIObserverService.h"
#include "nsWindow.h"

#include "android/log.h"

#ifndef ABS_MT_TOUCH_MAJOR
// Taken from include/linux/input.h
// XXX update the bionic input.h so we don't have to do this!
#define ABS_X			0x00
#define ABS_Y			0x01
// ...
#define ABS_MT_TOUCH_MAJOR      0x30    /* Major axis of touching ellipse */
#define ABS_MT_TOUCH_MINOR      0x31    /* Minor axis (omit if circular) */
#define ABS_MT_WIDTH_MAJOR      0x32    /* Major axis of approaching ellipse */
#define ABS_MT_WIDTH_MINOR      0x33    /* Minor axis (omit if circular) */
#define ABS_MT_ORIENTATION      0x34    /* Ellipse orientation */
#define ABS_MT_POSITION_X       0x35    /* Center X ellipse position */
#define ABS_MT_POSITION_Y       0x36    /* Center Y ellipse position */
#define ABS_MT_TOOL_TYPE        0x37    /* Type of touching device */
#define ABS_MT_BLOB_ID          0x38    /* Group a set of packets as a blob */
#define ABS_MT_TRACKING_ID      0x39    /* Unique ID of initiated contact */
#define ABS_MT_PRESSURE         0x3a    /* Pressure on contact area */
#define SYN_MT_REPORT           2
#endif

#define LOG(args...)                                            \
    __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#ifdef VERBOSE_LOG_ENABLED
# define VERBOSE_LOG(args...)                           \
    __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#else
# define VERBOSE_LOG(args...)                   \
    (void)0
#endif

using namespace mozilla;

bool gDrawRequest = false;
static nsAppShell *gAppShell = NULL;
static int epollfd = 0;
static int signalfds[2] = {0};

namespace mozilla {

bool ProcessNextEvent()
{
    return gAppShell->ProcessNextNativeEvent(true);
}

void NotifyEvent()
{
    gAppShell->NotifyNativeEvent();
}

}

static void
pipeHandler(int fd, FdHandler *data)
{
    ssize_t len;
    do {
        char tmp[32];
        len = read(fd, tmp, sizeof(tmp));
    } while (len > 0);
}

static
PRUint64 timevalToMS(const struct timeval &time)
{
    return time.tv_sec * 1000 + time.tv_usec / 1000;
}

static void
sendMouseEvent(PRUint32 msg, struct timeval& time, int x, int y)
{
    nsMouseEvent event(true, msg, NULL,
                       nsMouseEvent::eReal, nsMouseEvent::eNormal);

    event.refPoint.x = x;
    event.refPoint.y = y;
    event.time = timevalToMS(time);
    event.isShift = false;
    event.isControl = false;
    event.isMeta = false;
    event.isAlt = false;
    event.button = nsMouseEvent::eLeftButton;
    if (msg != NS_MOUSE_MOVE)
        event.clickCount = 1;

    nsWindow::DispatchInputEvent(event);
}

static nsEventStatus
sendKeyEventWithMsg(PRUint32 keyCode,
                    PRUint32 msg,
                    const timeval &time,
                    PRUint32 flags)
{
    nsKeyEvent event(true, msg, NULL);
    event.keyCode = keyCode;
    event.time = timevalToMS(time);
    event.flags |= flags;
    return nsWindow::DispatchInputEvent(event);
}

static void
sendKeyEvent(PRUint32 keyCode, bool down, const timeval &time)
{
    nsEventStatus status =
        sendKeyEventWithMsg(keyCode, down ? NS_KEY_DOWN : NS_KEY_UP, time, 0);
    if (down) {
        sendKeyEventWithMsg(keyCode, NS_KEY_PRESS, time,
                            status == nsEventStatus_eConsumeNoDefault ?
                            NS_EVENT_FLAG_NO_DEFAULT : 0);
    }
}

static void
sendSpecialKeyEvent(nsIAtom *command, const timeval &time)
{
    nsCommandEvent event(true, nsGkAtoms::onAppCommand, command, NULL);
    event.time = timevalToMS(time);
    nsWindow::DispatchInputEvent(event);
}

static void
maybeSendKeyEvent(int keyCode, bool pressed, const timeval& time)
{
    switch (keyCode) {
    case KEY_BACK:
        sendKeyEvent(NS_VK_ESCAPE, pressed, time);
        break;
    case KEY_MENU:
        if (!pressed)
            sendSpecialKeyEvent(nsGkAtoms::Menu, time);
        break;
    case KEY_SEARCH:
        if (pressed)
            sendSpecialKeyEvent(nsGkAtoms::Search, time);
        break;
    case KEY_HOME:
        sendKeyEvent(NS_VK_HOME, pressed, time);
        break;
    case KEY_POWER:
        sendKeyEvent(NS_VK_SLEEP, pressed, time);
        break;
    case KEY_VOLUMEUP:
        if (pressed)
            sendSpecialKeyEvent(nsGkAtoms::VolumeUp, time);
        break;
    case KEY_VOLUMEDOWN:
        if (pressed)
            sendSpecialKeyEvent(nsGkAtoms::VolumeDown, time);
        break;
    default:
        VERBOSE_LOG("Got unknown key event code. type 0x%04x code 0x%04x value %d",
                    keyCode, pressed);
    }
}

static void
maybeSendKeyEvent(const input_event& e)
{
    if (e.type != EV_KEY) {
        VERBOSE_LOG("Got unknown key event type. type 0x%04x code 0x%04x value %d",
            e.type, e.code, e.value);
        return;
    }

    if (e.value != 0 && e.value != 1) {
        VERBOSE_LOG("Got unknown key event value. type 0x%04x code 0x%04x value %d",
            e.type, e.code, e.value);
        return;
    }

    bool pressed = e.value == 1;
    maybeSendKeyEvent(e.code, pressed, e.time);
}

static void
configureVButtons(FdHandler& data)
{
    char vbuttonsPath[PATH_MAX];
    snprintf(vbuttonsPath, sizeof(vbuttonsPath),
             "/sys/board_properties/virtualkeys.%s",
             data.name);
    ScopedClose fd(open(vbuttonsPath, O_RDONLY));
    if (0 > fd.mFd) {
        LOG("No vbuttons for mt device %s", data.name);
        return;
    }

    // This device has vbuttons.  Process the configuration.
    char config[1024];
    ssize_t nread;
    do {
        nread = read(fd.mFd, config, sizeof(config));
    } while (-1 == nread && EINTR == errno);

    if (0 > nread) {
        LOG("Error reading virtualkey configuration");
        return;
    }

    config[nread] = '\0';

    LOG("Device %s has vbutton config '%s'", data.name, config);

    char* startStr = config;
    for (size_t i = 0; i < FdHandler::kMaxVButtons; ++i) {
        FdHandler::VButton& vbutton = data.vbuttons[i];
        char* token;
        char* state;
                
        // XXX not clear what "0x01" is ... maybe a version
        // number?  See InputManager.java.
        if (!(token = strtok_r(startStr, ":", &state)) ||
            strcmp(token, "0x01")) {
            LOG("  magic 0x01 tag missing");
            break;
        }
        startStr = NULL;

        if (!(token = strtok_r(NULL, ":", &state))) {
            LOG("  failed to read keycode");
            break;
        }
        vbutton.keyCode = atoi(token);

        const char *centerX, *centerY, *width, *height;
        if (!((centerX = strtok_r(NULL, ":", &state)) &&
              (centerY = strtok_r(NULL, ":", &state)) &&
              (width = strtok_r(NULL, ":", &state)) &&
              (height = strtok_r(NULL, ":", &state)))) {
            LOG("  failed to read bound params");
            break;
        }

        // NB: these coordinates are in *screen* space, not input
        // space.  That means the values in /sys/board_config make
        // assumptions about how the raw input events are mapped
        // ... le sigh.
        nsIntRect rect;
        rect.width = atoi(width);
        rect.height = atoi(height);
        rect.x = atoi(centerX) - rect.width / 2;
        rect.y = atoi(centerY) - rect.height / 2;
        vbutton.buttonRect = rect;

        LOG("  configured vbutton code=%d at <x=%d,y=%d,w=%d,h=%d>",
            vbutton.keyCode, rect.x, rect.y, rect.width, rect.height);
    }
}

static bool
calibrateMultitouchDevice(FdHandler& data)
{
    if (data.calibrated)
        return true;
    if (gScreenBounds.IsEmpty()) {
        // The framebuffer hasn't been initialized yet.  We *could*
        // force it to be initialized here, but that's another patch.
        LOG("Deferring multitouch calibrate, fb not ready");
        return false;
    }

    struct input_absinfo xInfo, yInfo;
    if (0 > ioctl(data.fd, EVIOCGABS(ABS_MT_POSITION_X), &xInfo) ||
        0 > ioctl(data.fd, EVIOCGABS(ABS_MT_POSITION_Y), &yInfo)) {
        LOG("Couldn't get absinfo for multitouch axes");
        return false;
    }
    LOG("Input coordinate bounds: xmin=%d, xmax=%d, ymin=%d, ymax=%d",
        xInfo.minimum, xInfo.maximum, yInfo.minimum, yInfo.maximum);

    data.inputMinX = xInfo.minimum;
    data.inputMinY = yInfo.minimum;
    data.inputToScreenScaleX =
        float(gScreenBounds.width) / float(xInfo.maximum - xInfo.minimum);
    data.inputToScreenScaleY =
        float(gScreenBounds.height) / float(yInfo.maximum - yInfo.minimum);

    configureVButtons(data);

    data.calibrated = true;
    return true;
}

static void
multitouchHandler(int fd, FdHandler *data)
{
    if (!calibrateMultitouchDevice(*data))
        return;

    // The Linux's input documentation (Documentation/input/input.txt)
    // says that we'll always read a multiple of sizeof(input_event) bytes here.
    input_event events[16];
    int event_count = read(fd, events, sizeof(events));
    if (event_count < 0) {
        LOG("Error reading in multitouchHandler");
        return;
    }
    MOZ_ASSERT(event_count % sizeof(input_event) == 0);

    event_count /= sizeof(struct input_event);

    for (int i = 0; i < event_count; i++) {
        input_event *event = &events[i];

        if (event->type == EV_ABS) {
            if (data->mtState == FdHandler::MT_IGNORE)
                continue;
            if (data->mtState == FdHandler::MT_START)
                data->mtState = FdHandler::MT_COLLECT;

            switch (event->code) {
            case ABS_MT_TOUCH_MAJOR:
                data->mtMajor = event->value;
                break;
            case ABS_MT_TOUCH_MINOR:
            case ABS_MT_WIDTH_MAJOR:
            case ABS_MT_WIDTH_MINOR:
            case ABS_MT_ORIENTATION:
            case ABS_MT_TOOL_TYPE:
            case ABS_MT_BLOB_ID:
            case ABS_MT_TRACKING_ID:
            case ABS_MT_PRESSURE:
                break;
            case ABS_MT_POSITION_X:
                data->mtX = data->inputXToScreenX(event->value);
                break;
            case ABS_MT_POSITION_Y:
                data->mtY = data->inputYToScreenY(event->value);
                break;
            default:
                VERBOSE_LOG("Got unknown mt event type 0x%04x with code 0x%04x and value %d",
                            event->type, event->code, event->value);
                break;
            }
        } else if (event->type == EV_SYN) {
            switch (event->code) {
            case SYN_MT_REPORT:
                if (data->mtState == FdHandler::MT_COLLECT)
                    data->mtState = FdHandler::MT_IGNORE;
                break;
            case SYN_REPORT:
                if (!data->mtMajor || data->mtState == FdHandler::MT_START) {
                    data->mtDown = false;
                    if (data->keyCode) {
                        maybeSendKeyEvent(data->keyCode, data->mtDown,
                                          event->time);
                        data->keyCode = 0;
                    } else {
                        sendMouseEvent(NS_MOUSE_BUTTON_UP, event->time,
                                       data->mtX, data->mtY);
                    }
                } else if (!data->mtDown) {
                    int x = data->mtX, y = data->mtY;

                    bool isKeyEvent = false;
                    if (!gScreenBounds.Contains(x, y)) {
                        // Off-screen mt down.  Should be a vbutton.
                        for (size_t i = 0; i < FdHandler::kMaxVButtons; ++i) {
                            const FdHandler::VButton& vbutton = data->vbuttons[i];
                            if (vbutton.buttonRect.IsEmpty())
                                break;

                            if (vbutton.buttonRect.Contains(x, y)) {
                                isKeyEvent = true;
                                data->keyCode = vbutton.keyCode;
                                break;
                            }
                        }
                    }
                    data->mtDown = true;

                    if (isKeyEvent) {
                        maybeSendKeyEvent(data->keyCode, data->mtDown,
                                          event->time);
                    } else {
                        sendMouseEvent(NS_MOUSE_BUTTON_DOWN, event->time,
                                       data->mtX, data->mtY);
                    }
                } else if (!data->keyCode) {
                    sendMouseEvent(NS_MOUSE_MOVE, event->time,
                                   data->mtX, data->mtY);
                    data->mtDown = true;
                }

                data->mtState = FdHandler::MT_START;

                break;
            default:
                VERBOSE_LOG("Got unknown mt event type 0x%04x with code 0x%04x and value %d",
                            event->type, event->code, event->value);

            }
        } else
            VERBOSE_LOG("Got unknown mt event type 0x%04x with code 0x%04x and value %d",
                        event->type, event->code, event->value);
    }
}

static void
singleTouchHandler(int fd, FdHandler *data)
{
    // The Linux's input documentation (Documentation/input/input.txt)
    // says that we'll always read a multiple of sizeof(input_event) bytes here.
    input_event events[16];
    int event_count = read(fd, events, sizeof(events));
    if (event_count < 0) {
        LOG("Error reading in singleTouchHandler");
        return;
    }
    MOZ_ASSERT(event_count % sizeof(input_event) == 0);

    event_count /= sizeof(struct input_event);

    for (int i = 0; i < event_count; i++) {
        input_event *event = &events[i];

        if (event->type == EV_KEY) {
            switch (event->code) {
            case BTN_TOUCH:
                data->mtDown = event->value;
                break;
            default:
                maybeSendKeyEvent(*event);
            }
        } else if (event->type == EV_ABS) {
            switch (event->code) {
            case ABS_X:
                data->mtX = event->value;
                break;
            case ABS_Y:
                data->mtY = event->value;
                break;
            default:
                LOG("Got unknown st abs event type 0x%04x with code 0x%04x and value %d",
                    event->type, event->code, event->value);
            }
        } else if (event->type == EV_SYN) {
            if (data->mtState == FdHandler::MT_START) {
                MOZ_ASSERT(data->mtDown);
                sendMouseEvent(NS_MOUSE_BUTTON_DOWN, event->time,
                               data->mtX, data->mtY);
                data->mtState = FdHandler::MT_COLLECT;
            } else if (data->mtDown) {
                MOZ_ASSERT(data->mtDown);
                sendMouseEvent(NS_MOUSE_MOVE, event->time,
                               data->mtX, data->mtY);
            } else {
                MOZ_ASSERT(!data->mtDown);
                sendMouseEvent(NS_MOUSE_BUTTON_UP, event->time,
                                   data->mtX, data->mtY);
                data->mtDown = false;
                data->mtState = FdHandler::MT_START;
            }
        }
    }
}

static void
keyHandler(int fd, FdHandler *data)
{
    input_event events[16];
    ssize_t bytesRead = read(fd, events, sizeof(events));
    if (bytesRead < 0) {
        LOG("Error reading in keyHandler");
        return;
    }
    MOZ_ASSERT(bytesRead % sizeof(input_event) == 0);

    for (unsigned int i = 0; i < bytesRead / sizeof(struct input_event); i++) {
        const input_event &e = events[i];

        if (e.type == EV_SYN) {
            // Ignore this event; it just signifies that a key was pressed.
            continue;
        }

        maybeSendKeyEvent(e);
    }
}

nsAppShell::nsAppShell()
    : mNativeCallbackRequest(false)
    , mHandlers()
{
    gAppShell = this;
}

nsAppShell::~nsAppShell()
{
    gAppShell = NULL;
}

nsresult
nsAppShell::Init()
{
    nsresult rv = nsBaseAppShell::Init();
    NS_ENSURE_SUCCESS(rv, rv);

    epollfd = epoll_create(16);
    NS_ENSURE_TRUE(epollfd >= 0, NS_ERROR_UNEXPECTED);

    int ret = pipe2(signalfds, O_NONBLOCK);
    NS_ENSURE_FALSE(ret, NS_ERROR_UNEXPECTED);

    rv = AddFdHandler(signalfds[0], pipeHandler, "");
    NS_ENSURE_SUCCESS(rv, rv);

    DIR *dir = opendir("/dev/input");
    NS_ENSURE_TRUE(dir, NS_ERROR_UNEXPECTED);

#define IS_BIT_SET(bit, flags) (flags[bit >> 3] & (1 << (bit & 0x7)))

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        char entryName[64];
        char entryPath[MAXPATHLEN];
        if (snprintf(entryPath, sizeof(entryPath),
                     "/dev/input/%s", entry->d_name) < 0) {
            LOG("Couldn't generate path while enumerating input devices!");
            continue;
        }
        int fd = open(entryPath, O_RDONLY);
        if (ioctl(fd, EVIOCGNAME(sizeof(entryName)), entryName) >= 0)
            LOG("Found device %s - %s", entry->d_name, entryName);
        else
            continue;

        FdHandlerCallback handlerFunc = NULL;

        char flags[(NS_MAX(ABS_MAX, KEY_MAX) + 1) / 8];
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(flags)), flags) >= 0 &&
            IS_BIT_SET(ABS_MT_POSITION_X, flags)) {

            LOG("Found multitouch input device");
            handlerFunc = multitouchHandler;
        } else if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(flags)), flags) >= 0 &&
                   IS_BIT_SET(ABS_X, flags)) {
            LOG("Found single touch input device");
            handlerFunc = singleTouchHandler;
        } else if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(flags)), flags) >= 0) {
            LOG("Found key input device");
            handlerFunc = keyHandler;
        }

        // Register the handler, if we have one.
        if (!handlerFunc)
            continue;

        rv = AddFdHandler(fd, handlerFunc, entryName);
        if (NS_FAILED(rv))
            LOG("Failed to add fd to epoll fd");
    }

    return rv;
}

nsresult
nsAppShell::AddFdHandler(int fd, FdHandlerCallback handlerFunc,
                         const char* deviceName)
{
    epoll_event event = {
        EPOLLIN,
        { 0 }
    };

    FdHandler *handler = mHandlers.AppendElement();
    handler->fd = fd;
    strncpy(handler->name, deviceName, sizeof(handler->name) - 1);
    handler->func = handlerFunc;
    event.data.u32 = mHandlers.Length() - 1;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) ?
           NS_ERROR_UNEXPECTED : NS_OK;
}

void
nsAppShell::ScheduleNativeEventCallback()
{
    mNativeCallbackRequest = true;
    NotifyEvent();
}

bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
    epoll_event events[16] = {{ 0 }};

    int event_count;
    if ((event_count = epoll_wait(epollfd, events, 16,  mayWait ? -1 : 0)) <= 0)
        return true;

    for (int i = 0; i < event_count; i++)
        mHandlers[events[i].data.u32].run();

    // NativeEventCallback always schedules more if it needs it
    // so we can coalesce these.
    // See the implementation in nsBaseAppShell.cpp for more info
    if (mNativeCallbackRequest) {
        mNativeCallbackRequest = false;
        NativeEventCallback();
    }

    if (gDrawRequest) {
        gDrawRequest = false;
        nsWindow::DoDraw();
    }

    return true;
}

void
nsAppShell::NotifyNativeEvent()
{
    write(signalfds[1], "w", 1);
}

