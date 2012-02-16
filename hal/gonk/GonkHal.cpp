/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "hardware_legacy/uevent.h"
#include "Hal.h"
#include "HalImpl.h"
#include "mozilla/dom/battery/Constants.h"
#include "mozilla/FileUtils.h"
#include "nsAlgorithm.h"
#include "nsThreadUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Services.h"
#include "mozilla/FileUtils.h"
#include "nsThreadUtils.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "hardware/lights.h"
#include "hardware/hardware.h"
#include "hardware_legacy/vibrator.h"
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include "mozilla/dom/network/Constants.h"

using mozilla::hal::WindowIdentifier;

namespace mozilla {
namespace hal_impl {

namespace {

/**
 * This runnable runs for the lifetime of the program, once started.  It's
 * responsible for "playing" vibration patterns.
 */
class VibratorRunnable
  : public nsIRunnable
  , public nsIObserver
{
public:
  VibratorRunnable()
    : mMonitor("VibratorRunnable")
    , mIndex(0)
    , mShuttingDown(false)
  {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (!os) {
      NS_WARNING("Could not get observer service!");
      return;
    }

    os->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, /* weak ref */ true);
  } 
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  // Run on the main thread, not the vibrator thread.
  void Vibrate(const nsTArray<uint32> &pattern);
  void CancelVibrate();

private:
  Monitor mMonitor;

  // The currently-playing pattern.
  nsTArray<uint32> mPattern;

  // The index we're at in the currently-playing pattern.  If mIndex >=
  // mPattern.Length(), then we're not currently playing anything.
  uint32 mIndex;

  // Set to true in our shutdown observer.  When this is true, we kill the
  // vibrator thread.
  bool mShuttingDown;
};

NS_IMPL_ISUPPORTS2(VibratorRunnable, nsIRunnable, nsIObserver);

NS_IMETHODIMP
VibratorRunnable::Run()
{
  MonitorAutoLock lock(mMonitor);

  // We currently assume that mMonitor.Wait(X) waits for X milliseconds.  But in
  // reality, the kernel might not switch to this thread for some time after the
  // wait expires.  So there's potential for some inaccuracy here.
  //
  // This doesn't worry me too much.  Note that we don't even start vibrating
  // immediately when VibratorRunnable::Vibrate is called -- we go through a
  // condvar onto another thread.  Better just to be chill about small errors in
  // the timing here.

  while (!mShuttingDown) {
    if (mIndex < mPattern.Length()) {
      uint32 duration = mPattern[mIndex];
      if (mIndex % 2 == 0) {
        vibrator_on(duration);
      }
      mIndex++;
      mMonitor.Wait(PR_MillisecondsToInterval(duration));
    }
    else {
      mMonitor.Wait();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
VibratorRunnable::Observe(nsISupports *subject, const char *topic,
                          const PRUnichar *data)
{
  MOZ_ASSERT(strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);
  MonitorAutoLock lock(mMonitor);
  mShuttingDown = true;
  mMonitor.Notify();
  return NS_OK;
}

void
VibratorRunnable::Vibrate(const nsTArray<uint32> &pattern)
{
  MonitorAutoLock lock(mMonitor);
  mPattern = pattern;
  mIndex = 0;
  mMonitor.Notify();
}

void
VibratorRunnable::CancelVibrate()
{
  MonitorAutoLock lock(mMonitor);
  mPattern.Clear();
  mPattern.AppendElement(0);
  mIndex = 0;
  mMonitor.Notify();
}

VibratorRunnable *sVibratorRunnable = NULL;

void
EnsureVibratorThreadInitialized()
{
  if (sVibratorRunnable) {
    return;
  }

  nsRefPtr<VibratorRunnable> runnable = new VibratorRunnable();
  sVibratorRunnable = runnable;
  nsCOMPtr<nsIThread> thread;
  NS_NewThread(getter_AddRefs(thread), sVibratorRunnable);
}

} // anonymous namespace

void
Vibrate(const nsTArray<uint32> &pattern, const hal::WindowIdentifier &)
{
  EnsureVibratorThreadInitialized();
  sVibratorRunnable->Vibrate(pattern);
}

void
CancelVibrate(const hal::WindowIdentifier &)
{
  EnsureVibratorThreadInitialized();
  sVibratorRunnable->CancelVibrate();
}

namespace {

class BatteryUpdater : public nsRunnable {
public:
  NS_IMETHOD Run()
  {
    hal::BatteryInformation info;
    hal_impl::GetCurrentBatteryInformation(&info);
    hal::NotifyBatteryChange(info);
    return NS_OK;
  }
};

class UEventWatcher : public nsRunnable {
public:
  UEventWatcher()
    : mUpdater(new BatteryUpdater())
    , mRunning(false)
  {
  }

  NS_IMETHOD Run()
  {
    while (mRunning) {
      char buf[1024];
      int count = uevent_next_event(buf, sizeof(buf) - 1);
      if (!count) {
        NS_WARNING("uevent_next_event() returned 0!");
        continue;
      }

      buf[sizeof(buf) - 1] = 0;
      if (strstr(buf, "battery"))
        NS_DispatchToMainThread(mUpdater);
    }
    return NS_OK;
  }

  bool mRunning;

private:
  nsRefPtr<BatteryUpdater> mUpdater;
};

} // anonymous namespace

static bool sUEventInitialized = false;
static UEventWatcher *sWatcher = NULL;
static nsIThread *sWatcherThread = NULL;

void
EnableBatteryNotifications()
{
  if (!sUEventInitialized)
    sUEventInitialized = uevent_init();
  if (!sUEventInitialized) {
    NS_WARNING("uevent_init() failed!");
    return;
  }

  if (!sWatcher)
    sWatcher = new UEventWatcher();
  NS_ADDREF(sWatcher);

  sWatcher->mRunning = true;
  nsresult rv = NS_NewThread(&sWatcherThread, sWatcher);
  if (NS_FAILED(rv))
    NS_WARNING("Failed to get new thread for uevent watching");
}

void
DisableBatteryNotifications()
{
  sWatcher->mRunning = false;
  sWatcherThread->Shutdown();
  NS_IF_RELEASE(sWatcherThread);
  delete sWatcher;
}

void
GetCurrentBatteryInformation(hal::BatteryInformation *aBatteryInfo)
{
  static const int BATTERY_NOT_CHARGING = 0;
  static const int BATTERY_CHARGING_USB = 1;
  static const int BATTERY_CHARGING_AC  = 2;

  FILE *capacityFile = fopen("/sys/class/power_supply/battery/capacity", "r");
  double capacity = dom::battery::kDefaultLevel * 100;
  if (capacityFile) {
    fscanf(capacityFile, "%lf", &capacity);
    fclose(capacityFile);
  }

  FILE *chargingFile = fopen("/sys/class/power_supply/battery/charging_source", "r");
  int chargingSrc = 1;
  if (chargingFile) {
    fscanf(chargingFile, "%d", &chargingSrc);
    fclose(chargingFile);
  }

  #ifdef DEBUG
  if (chargingSrc != BATTERY_NOT_CHARGING &&
      chargingSrc != BATTERY_CHARGING_USB &&
      chargingSrc != BATTERY_CHARGING_AC) {
    HAL_LOG(("charging_source contained unknown value: %d", chargingSrc));
  }
  #endif

  aBatteryInfo->level() = capacity / 100;
  aBatteryInfo->charging() = (chargingSrc == BATTERY_CHARGING_USB ||
                              chargingSrc == BATTERY_CHARGING_AC);
  aBatteryInfo->remainingTime() = dom::battery::kUnknownRemainingTime;
}

namespace {

/**
 * RAII class to help us remember to close file descriptors.
 */
const char *screenEnabledFilename = "/sys/power/state";

template<ssize_t n>
bool ReadFromFile(const char *filename, char (&buf)[n])
{
  int fd = open(filename, O_RDONLY);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG(("Unable to open file %s.", filename));
    return false;
  }

  ssize_t numRead = read(fd, buf, n);
  if (numRead < 0) {
    HAL_LOG(("Error reading from file %s.", filename));
    return false;
  }

  buf[PR_MIN(numRead, n - 1)] = '\0';
  return true;
}

void WriteToFile(const char *filename, const char *toWrite)
{
  int fd = open(filename, O_WRONLY);
  ScopedClose autoClose(fd);
  if (fd < 0) {
    HAL_LOG(("Unable to open file %s.", filename));
    return;
  }

  if (write(fd, toWrite, strlen(toWrite)) < 0) {
    HAL_LOG(("Unable to write to file %s.", filename));
    return;
  }
}

// We can write to screenEnabledFilename to enable/disable the screen, but when
// we read, we always get "mem"!  So we have to keep track ourselves whether
// the screen is on or not.
bool sScreenEnabled = true;

} // anonymous namespace

bool
GetScreenEnabled()
{
  return sScreenEnabled;
}

void
SetScreenEnabled(bool enabled)
{
  WriteToFile(screenEnabledFilename, enabled ? "on" : "mem");
  sScreenEnabled = enabled;
}

double
GetScreenBrightness()
{
  hal::LightConfiguration aConfig;
  hal::LightType light = hal::eHalLightID_Backlight;

  hal::GetLight(light, &aConfig);
  // backlight is brightness only, so using one of the RGB elements as value.
  int brightness = aConfig.color() & 0xFF;
  return brightness / 255.0;
}

void
SetScreenBrightness(double brightness)
{
  // Don't use De Morgan's law to push the ! into this expression; we want to
  // catch NaN too.
  if (!(0 <= brightness && brightness <= 1)) {
    HAL_LOG(("SetScreenBrightness: Dropping illegal brightness %f.",
             brightness));
    return;
  }

  // Convert the value in [0, 1] to an int between 0 and 255 and convert to a color
  // note that the high byte is FF, corresponding to the alpha channel.
  int val = static_cast<int>(round(brightness * 255));
  uint32_t color = (0xff<<24) + (val<<16) + (val<<8) + val;

  hal::LightConfiguration aConfig;
  aConfig.mode() = hal::eHalLightMode_User;
  aConfig.flash() = hal::eHalLightFlash_None;
  aConfig.flashOnMS() = aConfig.flashOffMS() = 0;
  aConfig.color() = color;
  hal::SetLight(hal::eHalLightID_Backlight, aConfig);
}

static light_device_t* sLights[hal::eHalLightID_Count];	// will be initialized to NULL

light_device_t* GetDevice(hw_module_t* module, char const* name)
{
  int err;
  hw_device_t* device;
  err = module->methods->open(module, name, &device);
  if (err == 0) {
    return (light_device_t*)device;
  } else {
    return NULL;
  }
}

void
InitLights()
{
  // assume that if backlight is NULL, nothing has been set yet
  // if this is not true, the initialization will occur everytime a light is read or set!
  if (!sLights[hal::eHalLightID_Backlight]) {
    int err;
    hw_module_t* module;

    err = hw_get_module(LIGHTS_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
    if (err == 0) {
      sLights[hal::eHalLightID_Backlight]
             = GetDevice(module, LIGHT_ID_BACKLIGHT);
      sLights[hal::eHalLightID_Keyboard]
             = GetDevice(module, LIGHT_ID_KEYBOARD);
      sLights[hal::eHalLightID_Buttons]
             = GetDevice(module, LIGHT_ID_BUTTONS);
      sLights[hal::eHalLightID_Battery]
             = GetDevice(module, LIGHT_ID_BATTERY);
      sLights[hal::eHalLightID_Notifications]
             = GetDevice(module, LIGHT_ID_NOTIFICATIONS);
      sLights[hal::eHalLightID_Attention]
             = GetDevice(module, LIGHT_ID_ATTENTION);
      sLights[hal::eHalLightID_Bluetooth]
             = GetDevice(module, LIGHT_ID_BLUETOOTH);
      sLights[hal::eHalLightID_Wifi]
             = GetDevice(module, LIGHT_ID_WIFI);
        }
    }
}

/**
 * The state last set for the lights until liblights supports
 * getting the light state.
 */
static light_state_t sStoredLightState[hal::eHalLightID_Count];

bool
SetLight(hal::LightType light, const hal::LightConfiguration& aConfig)
{
  light_state_t state;

  InitLights();

  if (light < 0 || light >= hal::eHalLightID_Count || sLights[light] == NULL) {
    return false;
  }

  memset(&state, 0, sizeof(light_state_t));
  state.color = aConfig.color();
  state.flashMode = aConfig.flash();
  state.flashOnMS = aConfig.flashOnMS();
  state.flashOffMS = aConfig.flashOffMS();
  state.brightnessMode = aConfig.mode();

  sLights[light]->set_light(sLights[light], &state);
  sStoredLightState[light] = state;
  return true;
}

bool
GetLight(hal::LightType light, hal::LightConfiguration* aConfig)
{
  light_state_t state;

#ifdef HAVEGETLIGHT
  InitLights();
#endif

  if (light < 0 || light >= hal::eHalLightID_Count || sLights[light] == NULL) {
    return false;
  }

  memset(&state, 0, sizeof(light_state_t));

#ifdef HAVEGETLIGHT
  sLights[light]->get_light(sLights[light], &state);
#else
  state = sStoredLightState[light];
#endif

  aConfig->light() = light;
  aConfig->color() = state.color;
  aConfig->flash() = hal::FlashMode(state.flashMode);
  aConfig->flashOnMS() = state.flashOnMS;
  aConfig->flashOffMS() = state.flashOffMS;
  aConfig->mode() = hal::LightMode(state.brightnessMode);

  return true;
}

void
EnableNetworkNotifications()
{}

void
DisableNetworkNotifications()
{}

void
GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo)
{
  aNetworkInfo->bandwidth() = dom::network::kDefaultBandwidth;
  aNetworkInfo->canBeMetered() = dom::network::kDefaultCanBeMetered;
}

} // hal_impl
} // mozilla
