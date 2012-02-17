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
 * The Original Code is Mozilla Communicator client code. This file was split
 * from xpfe/appshell/src/nsAppShellService.cpp
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *   Daniel Brooks <db48x@db48x.net>
 *   Taras Glek <tglek@mozilla.com>
 *   Landry Breuil <landry@openbsd.org>
 *   David Rajchenbach-Teller <dteller@mozilla.com>
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

#include "nsAppStartup.h"

#include "nsIAppShellService.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIProfileChangeStatus.h"
#include "nsIPromptService.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"
#include "nsIXULRuntime.h"
#include "nsIXULWindow.h"
#include "nsNativeCharsetUtils.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include "nsStringGlue.h"
#include "mozilla/Preferences.h"

#include "prprf.h"
#include "nsCRT.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsWidgetsCID.h"
#include "nsAppShellCID.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/Services.h"
#include "mozilla/FunctionTimer.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "prenv.h"

#if defined(XP_WIN)
#include <windows.h>
// windows.h can go to hell 
#undef GetStartupInfo
#elif defined(XP_UNIX)
#include <unistd.h>
#include <sys/syscall.h>
#endif

#ifdef XP_MACOSX
#include <sys/sysctl.h>
#endif

#ifdef __OpenBSD__
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include "mozilla/Telemetry.h"
#include "mozilla/StartupTimeline.h"

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#define kPrefLastSuccess "toolkit.startup.last_success"
#define kPrefMaxResumedCrashes "toolkit.startup.max_resumed_crashes"
#define kPrefRecentCrashes "toolkit.startup.recent_crashes"

#if defined(XP_WIN)
#include "mozilla/perfprobe.h"
/**
 * Events sent to the system for profiling purposes
 */
//Keep them syncronized with the .mof file

//Process-wide GUID, used by the OS to differentiate sources
// {509962E0-406B-46F4-99BA-5A009F8D2225}
//Keep it synchronized with the .mof file
#define NS_APPLICATION_TRACING_CID \
  { 0x509962E0, 0x406B, 0x46F4, \
  { 0x99, 0xBA, 0x5A, 0x00, 0x9F, 0x8D, 0x22, 0x25} }

//Event-specific GUIDs, used by the OS to differentiate events
// {A3DA04E0-57D7-482A-A1C1-61DA5F95BACB}
#define NS_PLACES_INIT_COMPLETE_EVENT_CID \
  { 0xA3DA04E0, 0x57D7, 0x482A, \
  { 0xA1, 0xC1, 0x61, 0xDA, 0x5F, 0x95, 0xBA, 0xCB} }
// {917B96B1-ECAD-4DAB-A760-8D49027748AE}
#define NS_SESSION_STORE_WINDOW_RESTORED_EVENT_CID \
  { 0x917B96B1, 0xECAD, 0x4DAB, \
  { 0xA7, 0x60, 0x8D, 0x49, 0x02, 0x77, 0x48, 0xAE} }

static NS_DEFINE_CID(kApplicationTracingCID,
  NS_APPLICATION_TRACING_CID);
static NS_DEFINE_CID(kPlacesInitCompleteCID,
  NS_PLACES_INIT_COMPLETE_EVENT_CID);
static NS_DEFINE_CID(kSessionStoreWindowRestoredCID,
  NS_SESSION_STORE_WINDOW_RESTORED_EVENT_CID);
#endif //defined(XP_WIN)

using namespace mozilla;

PRUint32 gRestartMode = 0;

class nsAppExitEvent : public nsRunnable {
private:
  nsRefPtr<nsAppStartup> mService;

public:
  nsAppExitEvent(nsAppStartup *service) : mService(service) {}

  NS_IMETHOD Run() {
    // Tell the appshell to exit
    mService->mAppShell->Exit();

    // We're done "shutting down".
    mService->mShuttingDown = false;
    mService->mRunning = false;
    return NS_OK;
  }
};

//
// nsAppStartup
//

nsAppStartup::nsAppStartup() :
  mConsiderQuitStopper(0),
  mRunning(false),
  mShuttingDown(false),
  mAttemptingQuit(false),
  mRestart(false),
  mInterrupted(false),
  mIsSafeModeNecessary(false),
  mStartupCrashTrackingEnded(false)
{ }


nsresult
nsAppStartup::Init()
{
  NS_TIME_FUNCTION;
  nsresult rv;

  // Create widget application shell
  mAppShell = do_GetService(kAppShellCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_TIME_FUNCTION_MARK("Got AppShell service");

  nsCOMPtr<nsIObserverService> os =
    mozilla::services::GetObserverService();
  if (!os)
    return NS_ERROR_FAILURE;

  NS_TIME_FUNCTION_MARK("Got Observer service");

  os->AddObserver(this, "quit-application-forced", true);
  os->AddObserver(this, "sessionstore-windows-restored", true);
  os->AddObserver(this, "profile-change-teardown", true);
  os->AddObserver(this, "xul-window-registered", true);
  os->AddObserver(this, "xul-window-destroyed", true);

#if defined(XP_WIN)
  os->AddObserver(this, "places-init-complete", true);
  // This last event is only interesting to us for xperf-based measures

  // Initialize interaction with profiler
  mProbesManager =
    new ProbeManager(
                     kApplicationTracingCID,
                     NS_LITERAL_CSTRING("Application startup probe"));
  // Note: The operation is meant mostly for in-house profiling.
  // Therefore, we do not warn if probes manager cannot be initialized

  if (mProbesManager) {
    mPlacesInitCompleteProbe =
      mProbesManager->
      GetProbe(kPlacesInitCompleteCID,
               NS_LITERAL_CSTRING("places-init-complete"));
    NS_WARN_IF_FALSE(mPlacesInitCompleteProbe,
                     "Cannot initialize probe 'places-init-complete'");

    mSessionWindowRestoredProbe =
      mProbesManager->
      GetProbe(kSessionStoreWindowRestoredCID,
               NS_LITERAL_CSTRING("sessionstore-windows-restored"));
    NS_WARN_IF_FALSE(mSessionWindowRestoredProbe,
                     "Cannot initialize probe 'sessionstore-windows-restored'");

    rv = mProbesManager->StartSession();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                     "Cannot initialize system probe manager");
  }
#endif //defined(XP_WIN)

  return NS_OK;
}


//
// nsAppStartup->nsISupports
//

NS_IMPL_THREADSAFE_ISUPPORTS5(nsAppStartup,
                              nsIAppStartup,
                              nsIWindowCreator,
                              nsIWindowCreator2,
                              nsIObserver,
                              nsISupportsWeakReference)


//
// nsAppStartup->nsIAppStartup
//

NS_IMETHODIMP
nsAppStartup::CreateHiddenWindow()
{
  nsCOMPtr<nsIAppShellService> appShellService
    (do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShellService, NS_ERROR_FAILURE);

  return appShellService->CreateHiddenWindow();
}


NS_IMETHODIMP
nsAppStartup::DestroyHiddenWindow()
{
  nsCOMPtr<nsIAppShellService> appShellService
    (do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShellService, NS_ERROR_FAILURE);

  return appShellService->DestroyHiddenWindow();
}

NS_IMETHODIMP
nsAppStartup::Run(void)
{
  NS_ASSERTION(!mRunning, "Reentrant appstartup->Run()");

  // If we have no windows open and no explicit calls to
  // enterLastWindowClosingSurvivalArea, or somebody has explicitly called
  // quit, don't bother running the event loop which would probably leave us
  // with a zombie process.

  if (!mShuttingDown && mConsiderQuitStopper != 0) {
#ifdef XP_MACOSX
    EnterLastWindowClosingSurvivalArea();
#endif

    mRunning = true;

    nsresult rv = mAppShell->Run();
    if (NS_FAILED(rv))
      return rv;
  }

  return mRestart ? NS_SUCCESS_RESTART_APP : NS_OK;
}


NS_IMETHODIMP
nsAppStartup::Quit(PRUint32 aMode)
{
  PRUint32 ferocity = (aMode & 0xF);

  // Quit the application. We will asynchronously call the appshell's
  // Exit() method via nsAppExitEvent to allow one last pass
  // through any events in the queue. This guarantees a tidy cleanup.
  nsresult rv = NS_OK;
  bool postedExitEvent = false;

  if (mShuttingDown)
    return NS_OK;

  // If we're considering quitting, we will only do so if:
  if (ferocity == eConsiderQuit) {
    if (mConsiderQuitStopper == 0) {
      // there are no windows...
      ferocity = eAttemptQuit;
    }
#ifdef XP_MACOSX
    else if (mConsiderQuitStopper == 1) {
      // ... or there is only a hiddenWindow left, and it's useless:
      nsCOMPtr<nsIAppShellService> appShell
        (do_GetService(NS_APPSHELLSERVICE_CONTRACTID));

      // Failure shouldn't be fatal, but will abort quit attempt:
      if (!appShell)
        return NS_OK;

      bool usefulHiddenWindow;
      appShell->GetApplicationProvidedHiddenWindow(&usefulHiddenWindow);
      nsCOMPtr<nsIXULWindow> hiddenWindow;
      appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
      // If the one window is useful, we won't quit:
      if (!hiddenWindow || usefulHiddenWindow)
        return NS_OK;

      ferocity = eAttemptQuit;
    }
#endif
  }

  nsCOMPtr<nsIObserverService> obsService;
  if (ferocity == eAttemptQuit || ferocity == eForceQuit) {

    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
    nsCOMPtr<nsIWindowMediator> mediator (do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
    if (mediator) {
      mediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
      if (windowEnumerator) {
        bool more;
        while (windowEnumerator->HasMoreElements(&more), more) {
          nsCOMPtr<nsISupports> window;
          windowEnumerator->GetNext(getter_AddRefs(window));
          nsCOMPtr<nsPIDOMWindow> domWindow(do_QueryInterface(window));
          if (domWindow) {
            if (!domWindow->CanClose())
              return NS_OK;
          }
        }
      }
    }

    mShuttingDown = true;
    if (!mRestart) {
      mRestart = (aMode & eRestart) != 0;
      gRestartMode = (aMode & 0xF0);
    }

    if (mRestart) {
      // Firefox-restarts reuse the process. Process start-time isn't a useful indicator of startup time
      PR_SetEnv(PR_smprintf("MOZ_APP_RESTART=%lld", (PRInt64) PR_Now() / PR_USEC_PER_MSEC));
    }

    obsService = mozilla::services::GetObserverService();

    if (!mAttemptingQuit) {
      mAttemptingQuit = true;
#ifdef XP_MACOSX
      // now even the Mac wants to quit when the last window is closed
      ExitLastWindowClosingSurvivalArea();
#endif
      if (obsService)
        obsService->NotifyObservers(nsnull, "quit-application-granted", nsnull);
    }

    /* Enumerate through each open window and close it. It's important to do
       this before we forcequit because this can control whether we really quit
       at all. e.g. if one of these windows has an unload handler that
       opens a new window. Ugh. I know. */
    CloseAllWindows();

    if (mediator) {
      if (ferocity == eAttemptQuit) {
        ferocity = eForceQuit; // assume success

        /* Were we able to immediately close all windows? if not, eAttemptQuit
           failed. This could happen for a variety of reasons; in fact it's
           very likely. Perhaps we're being called from JS and the window->Close
           method hasn't had a chance to wrap itself up yet. So give up.
           We'll return (with eConsiderQuit) as the remaining windows are
           closed. */
        mediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
        if (windowEnumerator) {
          bool more;
          while (windowEnumerator->HasMoreElements(&more), more) {
            /* we can't quit immediately. we'll try again as the last window
               finally closes. */
            ferocity = eAttemptQuit;
            nsCOMPtr<nsISupports> window;
            windowEnumerator->GetNext(getter_AddRefs(window));
            nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(window);
            if (domWindow) {
              bool closed = false;
              domWindow->GetClosed(&closed);
              if (!closed) {
                rv = NS_ERROR_FAILURE;
                break;
              }
            }
          }
        }
      }
    }
  }

  if (ferocity == eForceQuit) {
    // do it!

    // No chance of the shutdown being cancelled from here on; tell people
    // we're shutting down for sure while all services are still available.
    if (obsService) {
      NS_NAMED_LITERAL_STRING(shutdownStr, "shutdown");
      NS_NAMED_LITERAL_STRING(restartStr, "restart");
      obsService->NotifyObservers(nsnull, "quit-application",
        mRestart ? restartStr.get() : shutdownStr.get());
    }

    if (!mRunning) {
      postedExitEvent = true;
    }
    else {
      // no matter what, make sure we send the exit event.  If
      // worst comes to worst, we'll do a leaky shutdown but we WILL
      // shut down. Well, assuming that all *this* stuff works ;-).
      nsCOMPtr<nsIRunnable> event = new nsAppExitEvent(this);
      rv = NS_DispatchToCurrentThread(event);
      if (NS_SUCCEEDED(rv)) {
        postedExitEvent = true;
      }
      else {
        NS_WARNING("failed to dispatch nsAppExitEvent");
      }
    }
  }

  // turn off the reentrancy check flag, but not if we have
  // more asynchronous work to do still.
  if (!postedExitEvent)
    mShuttingDown = false;
  return rv;
}


void
nsAppStartup::CloseAllWindows()
{
  nsCOMPtr<nsIWindowMediator> mediator
    (do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

  mediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));

  if (!windowEnumerator)
    return;

  bool more;
  while (NS_SUCCEEDED(windowEnumerator->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> isupports;
    if (NS_FAILED(windowEnumerator->GetNext(getter_AddRefs(isupports))))
      break;

    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(isupports);
    NS_ASSERTION(window, "not an nsPIDOMWindow");
    if (window)
      window->ForceClose();
  }
}

NS_IMETHODIMP
nsAppStartup::EnterLastWindowClosingSurvivalArea(void)
{
  ++mConsiderQuitStopper;
  return NS_OK;
}


NS_IMETHODIMP
nsAppStartup::ExitLastWindowClosingSurvivalArea(void)
{
  NS_ASSERTION(mConsiderQuitStopper > 0, "consider quit stopper out of bounds");
  --mConsiderQuitStopper;

  if (mRunning)
    Quit(eConsiderQuit);

  return NS_OK;
}

//
// nsAppStartup->nsIAppStartup2
//

NS_IMETHODIMP
nsAppStartup::GetShuttingDown(bool *aResult)
{
  *aResult = mShuttingDown;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::SetInterrupted(bool aInterrupted)
{
  mInterrupted = aInterrupted;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::GetInterrupted(bool *aInterrupted)
{
  *aInterrupted = mInterrupted;
  return NS_OK;
}

//
// nsAppStartup->nsIWindowCreator
//

NS_IMETHODIMP
nsAppStartup::CreateChromeWindow(nsIWebBrowserChrome *aParent,
                                 PRUint32 aChromeFlags,
                                 nsIWebBrowserChrome **_retval)
{
  bool cancel;
  return CreateChromeWindow2(aParent, aChromeFlags, 0, 0, &cancel, _retval);
}


//
// nsAppStartup->nsIWindowCreator2
//

NS_IMETHODIMP
nsAppStartup::CreateChromeWindow2(nsIWebBrowserChrome *aParent,
                                  PRUint32 aChromeFlags,
                                  PRUint32 aContextFlags,
                                  nsIURI *aURI,
                                  bool *aCancel,
                                  nsIWebBrowserChrome **_retval)
{
  NS_ENSURE_ARG_POINTER(aCancel);
  NS_ENSURE_ARG_POINTER(_retval);
  *aCancel = false;
  *_retval = 0;

  // Non-modal windows cannot be opened if we are attempting to quit
  if (mAttemptingQuit && (aChromeFlags & nsIWebBrowserChrome::CHROME_MODAL) == 0)
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;

  nsCOMPtr<nsIXULWindow> newWindow;

  if (aParent) {
    nsCOMPtr<nsIXULWindow> xulParent(do_GetInterface(aParent));
    NS_ASSERTION(xulParent, "window created using non-XUL parent. that's unexpected, but may work.");

    if (xulParent)
      xulParent->CreateNewWindow(aChromeFlags, getter_AddRefs(newWindow));
    // And if it fails, don't try again without a parent. It could fail
    // intentionally (bug 115969).
  } else { // try using basic methods:
    /* You really shouldn't be making dependent windows without a parent.
      But unparented modal (and therefore dependent) windows happen
      in our codebase, so we allow it after some bellyaching: */
    if (aChromeFlags & nsIWebBrowserChrome::CHROME_DEPENDENT)
      NS_WARNING("dependent window created without a parent");

    nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
    if (!appShell)
      return NS_ERROR_FAILURE;
    
    appShell->CreateTopLevelWindow(0, 0, aChromeFlags,
                                   nsIAppShellService::SIZE_TO_CONTENT,
                                   nsIAppShellService::SIZE_TO_CONTENT,
                                   getter_AddRefs(newWindow));
  }

  // if anybody gave us anything to work with, use it
  if (newWindow) {
    newWindow->SetContextFlags(aContextFlags);
    nsCOMPtr<nsIInterfaceRequestor> thing(do_QueryInterface(newWindow));
    if (thing)
      CallGetInterface(thing.get(), _retval);
  }

  return *_retval ? NS_OK : NS_ERROR_FAILURE;
}


//
// nsAppStartup->nsIObserver
//

NS_IMETHODIMP
nsAppStartup::Observe(nsISupports *aSubject,
                      const char *aTopic, const PRUnichar *aData)
{
  NS_ASSERTION(mAppShell, "appshell service notified before appshell built");
  if (!strcmp(aTopic, "quit-application-forced")) {
    mShuttingDown = true;
  }
  else if (!strcmp(aTopic, "profile-change-teardown")) {
    if (!mShuttingDown) {
      EnterLastWindowClosingSurvivalArea();
      CloseAllWindows();
      ExitLastWindowClosingSurvivalArea();
    }
  } else if (!strcmp(aTopic, "xul-window-registered")) {
    EnterLastWindowClosingSurvivalArea();
  } else if (!strcmp(aTopic, "xul-window-destroyed")) {
    ExitLastWindowClosingSurvivalArea();
  } else if (!strcmp(aTopic, "sessionstore-windows-restored")) {
    StartupTimeline::Record(StartupTimeline::SESSION_RESTORED);
#if defined(XP_WIN)
    if (mSessionWindowRestoredProbe) {
      mSessionWindowRestoredProbe->Trigger();
    }
  } else if (!strcmp(aTopic, "places-init-complete")) {
    if (mPlacesInitCompleteProbe) {
      mPlacesInitCompleteProbe->Trigger();
    }
#endif //defined(XP_WIN)
  } else {
    NS_ERROR("Unexpected observer topic.");
  }

  return NS_OK;
}

#if defined(LINUX) || defined(ANDROID)
static PRUint64 
JiffiesSinceBoot(const char *file)
{
  char stat[512];
  FILE *f = fopen(file, "r");
  if (!f)
    return 0;
  int n = fread(&stat, 1, sizeof(stat) - 1, f);
  fclose(f);
  if (n <= 0)
    return 0;
  stat[n] = 0;
  
  long long unsigned starttime = 0; // instead of PRUint64 to keep GCC quiet
  
  char *s = strrchr(stat, ')');
  if (!s)
    return 0;
  int ret = sscanf(s + 2,
                   "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u "
                   "%*u %*u %*u %*u %*u %*d %*d %*d %*d %llu",
                   &starttime);
  if (ret != 1 || !starttime)
    return 0;
  return starttime;
}

static void
ThreadedCalculateProcessCreationTimestamp(void *aClosure)
{
  PRTime now = PR_Now();
  long hz = sysconf(_SC_CLK_TCK);
  if (!hz)
    return;

  char thread_stat[40];
  sprintf(thread_stat, "/proc/self/task/%d/stat", (pid_t) syscall(__NR_gettid));
  
  PRUint64 thread_jiffies = JiffiesSinceBoot(thread_stat);
  PRUint64 self_jiffies = JiffiesSinceBoot("/proc/self/stat");
  
  if (!thread_jiffies || !self_jiffies)
    return;

  PRTime interval = (thread_jiffies - self_jiffies) * PR_USEC_PER_SEC / hz;
  StartupTimeline::Record(StartupTimeline::PROCESS_CREATION, now - interval);
}

static PRTime
CalculateProcessCreationTimestamp()
{
 PRThread *thread = PR_CreateThread(PR_USER_THREAD,
                                    ThreadedCalculateProcessCreationTimestamp,
                                    NULL,
                                    PR_PRIORITY_NORMAL,
                                    PR_LOCAL_THREAD,
                                    PR_JOINABLE_THREAD,
                                    0);

  PR_JoinThread(thread);
  return StartupTimeline::Get(StartupTimeline::PROCESS_CREATION);
}
#elif defined(XP_WIN)
static PRTime
CalculateProcessCreationTimestamp()
{
  FILETIME start, foo, bar, baz;
  bool success = GetProcessTimes(GetCurrentProcess(), &start, &foo, &bar, &baz);
  if (!success)
    return 0;
  // copied from NSPR _PR_FileTimeToPRTime
  PRUint64 timestamp = 0;
  CopyMemory(&timestamp, &start, sizeof(PRTime));
#ifdef __GNUC__
  timestamp = (timestamp - 116444736000000000LL) / 10LL;
#else
  timestamp = (timestamp - 116444736000000000i64) / 10i64;
#endif
  return timestamp;
}
#elif defined(XP_MACOSX)
static PRTime
CalculateProcessCreationTimestamp()
{
  int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
  size_t buffer_size;
  if (sysctl(mib, 4, NULL, &buffer_size, NULL, 0))
    return 0;

  struct kinfo_proc *proc = (kinfo_proc*) malloc(buffer_size);  
  if (sysctl(mib, 4, proc, &buffer_size, NULL, 0)) {
    free(proc);
    return 0;
  }
  PRTime starttime = static_cast<PRTime>(proc->kp_proc.p_un.__p_starttime.tv_sec) * PR_USEC_PER_SEC;
  starttime += proc->kp_proc.p_un.__p_starttime.tv_usec;
  free(proc);
  return starttime;
}
#elif defined(__OpenBSD__)
static PRTime
CalculateProcessCreationTimestamp()
{
  int mib[6] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid(), sizeof(struct kinfo_proc), 1 };
  size_t buffer_size;
  if (sysctl(mib, 6, NULL, &buffer_size, NULL, 0))
    return 0;

  struct kinfo_proc *proc = (struct kinfo_proc*) malloc(buffer_size);
  if (sysctl(mib, 6, proc, &buffer_size, NULL, 0)) {
    free(proc);
    return 0;
  }
  PRTime starttime = static_cast<PRTime>(proc->p_ustart_sec) * PR_USEC_PER_SEC;
  starttime += proc->p_ustart_usec;
  free(proc);
  return starttime;
}
#else
static PRTime
CalculateProcessCreationTimestamp()
{
  return 0;
}
#endif
 
NS_IMETHODIMP
nsAppStartup::GetStartupInfo(JSContext* aCx, JS::Value* aRetval)
{
  JSObject *obj = JS_NewObject(aCx, NULL, NULL, NULL);
  *aRetval = OBJECT_TO_JSVAL(obj);

  PRTime ProcessCreationTimestamp = StartupTimeline::Get(StartupTimeline::PROCESS_CREATION);

  if (!ProcessCreationTimestamp) {
    char *moz_app_restart = PR_GetEnv("MOZ_APP_RESTART");
    if (moz_app_restart) {
      ProcessCreationTimestamp = nsCRT::atoll(moz_app_restart) * PR_USEC_PER_MSEC;
    } else {
      ProcessCreationTimestamp = CalculateProcessCreationTimestamp();
    }
    // Bug 670008: Avoid obviously invalid process creation times
    if (PR_Now() <= ProcessCreationTimestamp) {
      ProcessCreationTimestamp = -1;
      Telemetry::Accumulate(Telemetry::STARTUP_MEASUREMENT_ERRORS, StartupTimeline::PROCESS_CREATION);
    }
    StartupTimeline::Record(StartupTimeline::PROCESS_CREATION, ProcessCreationTimestamp);
  }

  for (int i = StartupTimeline::PROCESS_CREATION; i < StartupTimeline::MAX_EVENT_ID; ++i) {
    StartupTimeline::Event ev = static_cast<StartupTimeline::Event>(i);
    if (StartupTimeline::Get(ev) > 0) {
      // always define main to aid with bug 689256
      if ((ev != StartupTimeline::MAIN) &&
          (StartupTimeline::Get(ev) < StartupTimeline::Get(StartupTimeline::PROCESS_CREATION))) {
        Telemetry::Accumulate(Telemetry::STARTUP_MEASUREMENT_ERRORS, i);
        StartupTimeline::Record(ev, -1);
      } else {
        JSObject *date = JS_NewDateObjectMsec(aCx, StartupTimeline::Get(ev) / PR_USEC_PER_MSEC);
        JS_DefineProperty(aCx, obj, StartupTimeline::Describe(ev), OBJECT_TO_JSVAL(date), NULL, NULL, JSPROP_ENUMERATE);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::GetAutomaticSafeModeNecessary(bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = mIsSafeModeNecessary;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::TrackStartupCrashBegin(bool *aIsSafeModeNecessary)
{
  const PRInt32 MAX_TIME_SINCE_STARTUP = 6 * 60 * 60 * 1000;
  const PRInt32 MAX_STARTUP_BUFFER = 10;
  nsresult rv;

  mStartupCrashTrackingEnded = false;

  bool hasLastSuccess = Preferences::HasUserValue(kPrefLastSuccess);
  if (!hasLastSuccess) {
    // Clear so we don't get stuck with SafeModeNecessary returning true if we
    // have had too many recent crashes and the last success pref is missing.
    Preferences::ClearUser(kPrefRecentCrashes);
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool inSafeMode = false;
  nsCOMPtr<nsIXULRuntime> xr = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(xr, NS_ERROR_FAILURE);

  xr->GetInSafeMode(&inSafeMode);

  PRInt64 replacedLockTime;
  rv = xr->GetReplacedLockTime(&replacedLockTime);

  if (NS_FAILED(rv) || !replacedLockTime) {
    if (!inSafeMode)
      Preferences::ClearUser(kPrefRecentCrashes);
    GetAutomaticSafeModeNecessary(aIsSafeModeNecessary);
    return NS_OK;
  }

  // check whether safe mode is necessary
  PRInt32 maxResumedCrashes = -1;
  rv = Preferences::GetInt(kPrefMaxResumedCrashes, &maxResumedCrashes);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  PRInt32 recentCrashes = 0;
  Preferences::GetInt(kPrefRecentCrashes, &recentCrashes);
  mIsSafeModeNecessary = (recentCrashes > maxResumedCrashes && maxResumedCrashes != -1);

  // time of last successful startup
  PRInt32 lastSuccessfulStartup;
  rv = Preferences::GetInt(kPrefLastSuccess, &lastSuccessfulStartup);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 lockSeconds = (PRInt32)(replacedLockTime / PR_MSEC_PER_SEC);

  // started close enough to good startup so call it good
  if (lockSeconds <= lastSuccessfulStartup + MAX_STARTUP_BUFFER
      && lockSeconds >= lastSuccessfulStartup - MAX_STARTUP_BUFFER) {
    GetAutomaticSafeModeNecessary(aIsSafeModeNecessary);
    return NS_OK;
  }

  // sanity check that the pref set at last success is not greater than the current time
  if (PR_Now() / PR_USEC_PER_SEC <= lastSuccessfulStartup)
    return NS_ERROR_FAILURE;

  if (inSafeMode) {
    GetAutomaticSafeModeNecessary(aIsSafeModeNecessary);
    return NS_OK;
  }

  PRTime now = (PR_Now() / PR_USEC_PER_MSEC);
  // if the last startup attempt which crashed was in the last 6 hours
  if (replacedLockTime >= now - MAX_TIME_SINCE_STARTUP) {
    NS_WARNING("Last startup was detected as a crash.");
    recentCrashes++;
    rv = Preferences::SetInt(kPrefRecentCrashes, recentCrashes);
  } else {
    // Otherwise ignore that crash and all previous since it may not be applicable anymore
    // and we don't want someone to get stuck in safe mode if their prefs are read-only.
    rv = Preferences::ClearUser(kPrefRecentCrashes);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // recalculate since recent crashes count may have changed above
  mIsSafeModeNecessary = (recentCrashes > maxResumedCrashes && maxResumedCrashes != -1);

  nsCOMPtr<nsIPrefService> prefs = Preferences::GetService();
  rv = prefs->SavePrefFile(nsnull); // flush prefs to disk since we are tracking crashes
  NS_ENSURE_SUCCESS(rv, rv);

  GetAutomaticSafeModeNecessary(aIsSafeModeNecessary);
  return rv;
}

NS_IMETHODIMP
nsAppStartup::TrackStartupCrashEnd()
{
  bool inSafeMode = false;
  nsCOMPtr<nsIXULRuntime> xr = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  if (xr)
    xr->GetInSafeMode(&inSafeMode);

  // return if we already ended or we're restarting into safe mode
  if (mStartupCrashTrackingEnded || (mIsSafeModeNecessary && !inSafeMode))
    return NS_OK;
  mStartupCrashTrackingEnded = true;

  // Use the timestamp of XRE_main as an approximation for the lock file timestamp.
  // See MAX_STARTUP_BUFFER for the buffer time period.
  nsresult rv;
  PRTime mainTime = StartupTimeline::Get(StartupTimeline::MAIN);
  if (mainTime <= 0) {
    NS_WARNING("Could not get StartupTimeline::MAIN time.");
  } else {
    PRInt32 lockFileTime = (PRInt32)(mainTime / PR_USEC_PER_SEC);
    rv = Preferences::SetInt(kPrefLastSuccess, lockFileTime);
    if (NS_FAILED(rv)) NS_WARNING("Could not set startup crash detection pref.");
  }

  if (inSafeMode && mIsSafeModeNecessary) {
    // On a successful startup in automatic safe mode, allow the user one more crash
    // in regular mode before returning to safe mode.
    PRInt32 maxResumedCrashes = 0;
    PRInt32 prefType;
    rv = Preferences::GetDefaultRootBranch()->GetPrefType(kPrefMaxResumedCrashes, &prefType);
    NS_ENSURE_SUCCESS(rv, rv);
    if (prefType == nsIPrefBranch::PREF_INT) {
      rv = Preferences::GetInt(kPrefMaxResumedCrashes, &maxResumedCrashes);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = Preferences::SetInt(kPrefRecentCrashes, maxResumedCrashes);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (!inSafeMode) {
    // clear the count of recent crashes after a succesful startup when not in safe mode
    rv = Preferences::ClearUser(kPrefRecentCrashes);
    if (NS_FAILED(rv)) NS_WARNING("Could not clear startup crash count.");
  }
  nsCOMPtr<nsIPrefService> prefs = Preferences::GetService();
  rv = prefs->SavePrefFile(nsnull); // flush prefs to disk since we are tracking crashes

  return rv;
}

NS_IMETHODIMP
nsAppStartup::RestartInSafeMode(PRUint32 aQuitMode)
{
  PR_SetEnv("MOZ_SAFE_MODE_RESTART=1");
  this->Quit(aQuitMode | nsIAppStartup::eRestart);

  return NS_OK;
}
