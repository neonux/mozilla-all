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
 * The Original Code is Firefox Sync.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marina Samuel <msamuel@mozilla.com>
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


const EXPORTED_SYMBOLS = ["SyncScheduler"];

const Cu = Components.utils;

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/status.js");

Cu.import("resource://services-sync/main.js");    // So we can get to Service for callbacks.

let SyncScheduler = {
  _log: Log4Moz.repository.getLogger("Sync.SyncScheduler"),

  // nextSync is in milliseconds, but prefs can't hold that much
  get nextSync() Svc.Prefs.get("nextSync", 0) * 1000,
  set nextSync(value) Svc.Prefs.set("nextSync", Math.floor(value / 1000)),

  get syncInterval() {
    // If we have a partial download, sync sooner if we're not mobile
    if (Status.partial && Clients.clientType != "mobile")
      return PARTIAL_DATA_SYNC;
    return Svc.Prefs.get("syncInterval", MULTI_MOBILE_SYNC);
  },
  set syncInterval(value) Svc.Prefs.set("syncInterval", value),

  get syncThreshold() Svc.Prefs.get("syncThreshold", SINGLE_USER_THRESHOLD),
  set syncThreshold(value) Svc.Prefs.set("syncThreshold", value),

  get globalScore() Svc.Prefs.get("globalScore", 0),
  set globalScore(value) Svc.Prefs.set("globalScore", value),

  init: function init() {
    Svc.Obs.add("weave:engine:score:updated", this);
    Svc.Obs.add("network:offline-status-changed", this);
    Svc.Obs.add("weave:service:sync:start", this);
    Svc.Obs.add("weave:service:sync:finish", this);
    Svc.Obs.add("weave:engine:sync:finish", this);
    Svc.Obs.add("weave:service:login:error", this);
    Svc.Obs.add("weave:service:login:finish", this);
    Svc.Obs.add("weave:service:logout:finish", this);
    Svc.Obs.add("weave:service:sync:error", this);
    Svc.Obs.add("weave:service:backoff:interval", this);
  },

  observe: function observe(subject, topic, data) {
    switch(topic) {
      case "weave:engine:score:updated":
        Utils.namedTimer(this.calculateScore, SCORE_UPDATE_DELAY, this,
                         "_scoreTimer");
        break;
      case "network:offline-status-changed":
        // Whether online or offline, we'll reschedule syncs
        this._log.trace("Network offline status change: " + data);
        this.checkSyncStatus();
        break;
      case "weave:service:sync:start":
        // Clear out any potentially pending syncs now that we're syncing
        this.clearSyncTriggers();
        this.nextSync = 0;

        // reset backoff info, if the server tells us to continue backing off,
        // we'll handle that later
        Status.resetBackoff();

        this.globalScore = 0;
        break;
      case "weave:service:sync:finish":
        let sync_interval;
        this._syncErrors = 0;

        if (Status.sync == NO_SYNC_NODE_FOUND) {
          sync_interval = NO_SYNC_NODE_INTERVAL;
        }
        this.scheduleNextSync(sync_interval);
        break;
      case "weave:engine:sync:finish":
        if (subject == "clients") {
          // Update the client mode because it might change what we sync.
          this.updateClientMode();
        }
        break;
      case "weave:service:login:error":
        this.clearSyncTriggers();
        
        // Try again later, just as if we threw an error... only without the
        // error count.
        if (!this.skipScheduledRetry())
          this.scheduleAtInterval(MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
        break;
      case "weave:service:login:finish":
      case "weave:service:logout:finish":
        // Start or cancel the sync timer depending on if
        // logged in or logged out
        this.checkSyncStatus();
        break; 
      case "weave:service:sync:error":
        this.handleSyncError();
        break;
      case "weave:service:backoff:interval":
        let interval = (data + Math.random() * data * 0.25) * 1000; // required backoff + up to 25%
        Status.backoffInterval = interval;
        Status.minimumNextSync = Date.now() + data;
        break;
      }
  },

  calculateScore: function calculateScore() {
    var engines = Engines.getEnabled();
    for (let i = 0;i < engines.length;i++) {
      this._log.trace(engines[i].name + ": score: " + engines[i].score);
      this.globalScore += engines[i].score;
      engines[i]._tracker.resetScore();
    }

    this._log.trace("Global score updated: " + this.globalScore);
    this.checkSyncStatus();
  },

  /**
   * Process the locally stored clients list to figure out what mode to be in
   */
  updateClientMode: function updateClientMode() {
    // Nothing to do if it's the same amount
    let {numClients, hasMobile} = Clients.stats;
    if (Weave.Service.numClients == numClients)
      return;

    this._log.debug("Client count: " + Weave.Service.numClients + " -> " + numClients);
    Weave.Service.numClients = numClients;

    if (numClients == 1) {
      this.syncInterval = SINGLE_USER_SYNC;
      this.syncThreshold = SINGLE_USER_THRESHOLD;
    }
    else {
      this.syncInterval = hasMobile ? MULTI_MOBILE_SYNC : MULTI_DESKTOP_SYNC;
      this.syncThreshold = MULTI_DEVICE_THRESHOLD;
    }
  },

  /**
   * Check if we should be syncing and schedule the next sync, if it's not scheduled
   */
  checkSyncStatus: function checkSyncStatus() {
    // Should we be syncing now, if not, cancel any sync timers and return
    // if we're in backoff, we'll schedule the next sync
    let ignore = [kSyncBackoffNotMet];

    // We're ready to sync even if we're not logged in... so long as the
    // master password isn't locked.
    if (Utils.mpLocked()) {
      ignore.push(kSyncNotLoggedIn);
      ignore.push(kSyncMasterPasswordLocked);
    }

    let skip = Weave.Service._checkSync(ignore);
    this._log.trace("_checkSync returned \"" + skip + "\".");
    if (skip) {
      this.clearSyncTriggers();
      return;
    }

    // Only set the wait time to 0 if we need to sync right away
    let wait;
    if (this.globalScore > this.syncThreshold) {
      this._log.debug("Global Score threshold hit, triggering sync.");
      wait = 0;
    }
    this.scheduleNextSync(wait);
  },

  /**
   * Call sync() if Master Password is not locked.
   * 
   * Otherwise, reschedule a sync for later.
   */
  syncIfMPUnlocked: function syncIfMPUnlocked() {
    // No point if we got kicked out by the master password dialog.
    if (Status.login == MASTER_PASSWORD_LOCKED &&
        Utils.mpLocked()) {
      this._log.debug("Not initiating sync: Login status is " + Status.login);

      // If we're not syncing now, we need to schedule the next one.
      this.scheduleAtInterval(MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
      return;
    }

    Utils.nextTick(Weave.Service.sync, Weave.Service);
  },

  /**
   * Set a timer for the next sync
   */
  scheduleNextSync: function scheduleNextSync(interval) {
    // Figure out when to sync next if not given a interval to wait
    if (interval == null || interval == undefined) {
      // Check if we had a pending sync from last time
      if (this.nextSync != 0)
        interval = this.nextSync - Date.now();
      // Use the bigger of default sync interval and backoff
      else
        interval = Math.max(this.syncInterval, Status.backoffInterval);
    }

    // Start the sync right away if we're already late
    if (interval <= 0) {
      this.syncIfMPUnlocked();
      return;
    }

    this._log.trace("Next sync in " + Math.ceil(interval / 1000) + " sec.");
    Utils.namedTimer(this._syncIfMPUnlocked, interval, this, "_syncTimer");

    // Save the next sync time in-case sync is disabled (logout/offline/etc.)
    this.nextSync = Date.now() + interval;
  },


  /**
   * Incorporates the backoff/retry logic used in error handling and elective
   * non-syncing.
   */
  scheduleAtInterval: function scheduleAtInterval(minimumInterval) {
    let interval = Utils.calculateBackoff(this._syncErrors, MINIMUM_BACKOFF_INTERVAL);
    if (minimumInterval)
      interval = Math.max(minimumInterval, interval);

    let d = new Date(Date.now() + interval);
    this._log.config("Starting backoff, next sync at:" + d.toString());

    this.scheduleNextSync(interval);
  },

  skipScheduledRetry: function skipScheduledRetry() {
    return [LOGIN_FAILED_INVALID_PASSPHRASE,
            LOGIN_FAILED_LOGIN_REJECTED].indexOf(Status.login) == -1;
  },

  _syncErrors: 0,
  /**
   * Deal with sync errors appropriately
   */
  handleSyncError: function handleSyncError() {
    this._syncErrors++;

    // Do nothing on the first couple of failures, if we're not in
    // backoff due to 5xx errors.
    if (!Status.enforceBackoff) {
      if (this._syncErrors < MAX_ERROR_COUNT_BEFORE_BACKOFF) {
        this.scheduleNextSync();
        return;
      }
      Status.enforceBackoff = true;
    }

    this.scheduleAtInterval();
  },


  /**
   * Remove any timers/observers that might trigger a sync
   */
  clearSyncTriggers: function clearSyncTriggers() {
    this._log.debug("Clearing sync triggers.");

    // Clear out any scheduled syncs
    if (this._syncTimer)
      this._syncTimer.clear();
  }

};
