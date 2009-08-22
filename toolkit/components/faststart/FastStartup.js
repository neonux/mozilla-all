 /* -*- Mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil; -*- */
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
 * The Original Code is FastStartup.js
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Brian Crowder <crowder@fiverocks.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const Timer = Components.Constructor("@mozilla.org/timer;1", "nsITimer", "initWithCallback");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function nsFastStartupObserver() {
  this.fastStartObserver = null;
}

function getPreferredBrowserURI() {
  let chromeURI;
  try {
    chromeURI = prefs.getCharPref("toolkit.defaultChromeURI");
  } catch (e) { }
  return chromeURI;
}

nsFastStartupObserver.prototype = {
  _browserWindowCount: 0,
  _memCleanupTimer: null,

  stopMemoryCleanup: function() {
    if (this._memCleanupTimer) {
      this._memCleanupTimer.cancel();
      this._memCleanupTimer = null;
    }
  },

  scheduleMemoryCleanup: function() {
    this.stopMemoryCleanup();

    function memoryCleanup() {
      var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);

      // Do this 3 times, because of a comment in TestGtkEmbed that says this
      // gives the cycle collector the best chance at purging things.
      os.notifyObservers(null, "memory-pressure", "heap-minimize");
      os.notifyObservers(null, "memory-pressure", "heap-minimize");
      os.notifyObservers(null, "memory-pressure", "heap-minimize");

      // XXX start our long (15 min?) stub restart timer here
      this.fastStartObserver._memCleanupTimer = null;
    };

    // wait 30s until firing off the memory cleanup, in case the user
    // opens another window right away
    this._memCleanupTimer = new Timer(memoryCleanup, 30000, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  //
  // nsIObserver
  //
  observe: function fs_observe(subject, topic, data) {
    var win = subject;

    // XXX the window at this point won't actually have its document loaded --
    // win.document.documentURI will pretty much always be about:blank.  We need
    // to attach a load handler to actually figure out which document gets loaded.
    if (topic == "domwindowopened") {
      var loadListener = function(e) {
        if (win.document.documentURI == getPreferredBrowserURI()) {
          this.fastStartObserver.stopMemoryCleanup();
          this.fastStartObserver._browserWindowCount++;
        }
        win.removeEventListener("load", loadListener, false);
        return false;
      }

      win.addEventListener("load", loadListener, false);
    } else if (topic == "domwindowclosed") {
      if (win.document.documentURI == getPreferredBrowserURI()) {
        this.fastStartObserver._browserWindowCount--;
        if (this.fastStartObserver._browserWindowCount == 0)
          this.fastStartObserver.scheduleMemoryCleanup();
      }
    }
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
};

function nsFastStartupCLH() {
  inited = false;
}

nsFastStartupCLH.prototype = {
  //
  // nsICommandLineHandler
  //
  handle: function fs_handle(cmdLine) {
    // the rest of this only handles -faststart here
    if (!cmdLine.handleFlag("faststart", false))
      return;

    cmdLine.preventDefault = true;

    try {
      // did we already initialize faststart?  if so,
      // nothing to do here.
      if (this.inited)
        return;

      this.inited = true;

      let wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                   getService(Ci.nsIWindowWatcher);
      wwatch.registerNotification(new nsFastStartupObserver());

      let appstartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                       getService(Ci.nsIAppStartup);
      appstartup.enterLastWindowClosingSurvivalArea();
    } catch (e) {
      Cu.reportError(e);
    }
  },

  helpInfo: "    -faststart\n",

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),

  // XPCOMUtils factory
  classDescription: "Fast Startup Component",
  contractID: "@mozilla.org/browser/faststart;1",
  classID: Components.ID("{580c6c51-f690-4ce1-9ecc-b678e0c031c7}"),
  _xpcom_categories: [{ category: "command-line-handler", entry: "00-faststart" }],
};

var components = [ nsFastStartupCLH ];

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}
