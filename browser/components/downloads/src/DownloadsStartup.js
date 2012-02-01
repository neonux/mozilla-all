/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Downloads Panel Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

/**
 * This component listens to notifications for startup, shutdown and session
 * restore, controlling which downloads should be loaded from the database.
 *
 * To avoid affecting startup performance, this component monitors the current
 * session restore state, but defers the actual downloads data manipulation
 * until the Download Manager service is loaded.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gSessionStore",
                                   "@mozilla.org/browser/sessionstore;1",
                                   "nsISessionStore");

XPCOMUtils.defineLazyServiceGetter(this, "gPrivateBrowsingService",
                                   "@mozilla.org/privatebrowsing;1",
                                   "nsIPrivateBrowsingService");

const kObservedTopics = [
  "sessionstore-state-read",
  "sessionstore-windows-restored",
  "sessionstore-browser-state-restored",
  "download-manager-initialized",
  "private-browsing-transition-complete",
  "quit-application",
];

////////////////////////////////////////////////////////////////////////////////
//// DownloadsStartup

function DownloadsStartup() { }

DownloadsStartup.prototype = {
  classID: Components.ID("{49507fe5-2cee-4824-b6a3-e999150ce9b8}"),

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DS_observe(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case "app-startup":
        for each (let topic in kObservedTopics) {
          Services.obs.addObserver(this, topic, false);
        }

        // Override Toolkit's nsIDownloadManagerUI implementation with our own.
        // This must be done at application startup and not in the manifest to
        // ensure that our implementation overrides the original one.
        Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
                          .registerFactory(this._downloadsUICid, "",
                                           this._downloadsUIContractId, null);
        break;

      case "sessionstore-state-read":
        this._sessionStateWasAvailable = true;
        break;

      case "sessionstore-windows-restored":
      case "sessionstore-browser-state-restored":
        // Delay so that canRestoreLastSession has the correct value.
        this._executeSoon(function() {
          // The earliest point at which we can detect whether the session has
          // been actually restored is just after the browser state restore
          // process completed.  If we are not waiting for a deferred session
          // restore, and session state was available at startup, then the
          // session has already been restored.  This condition is not relevant
          // if we are operating in Private Browsing Mode.
          if (this._sessionStateWasAvailable &&
              !gPrivateBrowsingService.privateBrowsingEnabled &&
              !gSessionStore.canRestoreLastSession) {
            this._sessionWasRestored = true;
          }
          this._ensureDataLoaded();
        });
        break;

      case "download-manager-initialized":
        // Don't initialize the JavaScript data and user interface layer if we
        // are initializing the Download Manager service during shutdown.
        if (this._shuttingDown) {
          break;
        }

        // Start receiving events for active and new downloads before we return
        // from this observer function.  We can't defer the execution of this
        // step, to ensure that we don't lose events raised in the meantime.
        DownloadsCommon.data.initializeDataLink(
                             aSubject.QueryInterface(Ci.nsIDownloadManager));

        this._downloadsServiceInitialized = true;

        // Since this notification is generated during the getService call and
        // we need to get the Download Manager service ourselves, we must post
        // the handler on the event queue to be executed later.
        this._executeSoon(this._ensureDataLoaded);
        break;

      case "private-browsing-transition-complete":
        // Ensure that persistent data is reloaded only when the database
        // connection is available again.
        this._ensureDataLoaded();
        break;

      case "quit-application":
        this._shuttingDown = true;

        for each (let topic in kObservedTopics) {
          Services.obs.removeObserver(this, topic);
        }

        if (this._downloadsServiceInitialized) {
          DownloadsCommon.data.terminateDataLink();
        }

        // Stop if we need the behavior associated with the window interface.
        if (DownloadsCommon.useWindowUI) {
          break;
        }

        // If we restored the previous session, the list of downloads in the
        // database match the list of downloads in memory, and we should keep
        // all of them in the database, to allow restoring them in the next
        // session.
        if (this._sessionWasRestored) {
          break;
        }

        // If we never initialized the Download Manager service and didn't
        // restore the previous session, we should delete all the inactive
        // downloads from the database.
        if (!this._downloadsServiceInitialized) {
          Services.downloads.cleanUp();
          break;
        }

        // We need to delete from the database all the downloads from the
        // previous session, i.e. downloads that we didn't load in memory.
        // Downloads that started during this session should be kept in the
        // database, to allow restoring them in the next session.  Note that,
        // when "quit-application" is invoked, we've already exited Private
        // Browsing Mode, thus we are always working on the disk database.
        DownloadsCommon.data.deleteOldPersistentData();
        break;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Private

  /**
   * CID of our implementation of nsIDownloadManagerUI.
   */
  get _downloadsUICid() Components.ID("{4d99321e-d156-455b-81f7-e7aa2308134f}"),

  /**
   * Contract ID of the service implementing nsIDownloadManagerUI.
   */
  get _downloadsUIContractId() "@mozilla.org/download-manager-ui;1",

  /**
   * Indicates whether a session state file was available during startup.
   */
  _sessionStateWasAvailable: false,

  /**
   * Indicates whether the previous browsing session has been restored.
   */
  _sessionWasRestored: false,

  /**
   * Indicates whether the Download Manager service has been initialized.  This
   * flag is required because we want to avoid accessing the service immediately
   * at browser startup.  The service will start when the user first requests a
   * download, or some time after browser startup.
   */
  _downloadsServiceInitialized: false,

  /**
   * True while we are processing the application shutdown event, and later.
   */
  _shuttingDown: false,

  /**
   * Ensures that persistent download data is reloaded at the appropriate time.
   */
  _ensureDataLoaded: function DS_ensureDataLoaded()
  {
    if (!this._downloadsServiceInitialized ||
        gPrivateBrowsingService.privateBrowsingEnabled) {
      return;
    }

    // If the previous session has been already restored, then we ensure that
    // all the downloads are loaded.  Otherwise, we only ensure that the active
    // downloads from the previous session are loaded.
    DownloadsCommon.data.ensurePersistentDataLoaded(!this._sessionWasRestored);
  },

  /**
   * Enqueues the given function to be executed on the main thread.
   */
  _executeSoon: function DS_executeSoon(aCallbackFn)
  {
    let self = this;
    Services.tm.mainThread.dispatch({ run: function() aCallbackFn.apply(self) },
                                    Ci.nsIThread.DISPATCH_NORMAL);
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Module

const NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadsStartup]);
