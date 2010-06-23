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
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Myk Melez <myk@mozilla.org>
 *  Jono DiCarlo <jdicarlo@mozilla.com>
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

const EXPORTED_SYMBOLS = ['TabEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/stores.js");
Cu.import("resource://services-sync/trackers.js");
Cu.import("resource://services-sync/type_records/tabs.js");
Cu.import("resource://services-sync/util.js");

function TabEngine() {
  SyncEngine.call(this, "Tabs");

  // Reset the client on every startup so that we fetch recent tabs
  this._resetClient();
}
TabEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: TabStore,
  _trackerObj: TabTracker,
  _recordObj: TabSetRecord,

  // API for use by Weave UI code to give user choices of tabs to open:
  getAllClients: function TabEngine_getAllClients() {
    return this._store._remoteClients;
  },

  getClientById: function TabEngine_getClientById(id) {
    return this._store._remoteClients[id];
  },

  _resetClient: function TabEngine__resetClient() {
    SyncEngine.prototype._resetClient.call(this);
    this._store.wipe();
  },

  /* The intent is not to show tabs in the menu if they're already
   * open locally.  There are a couple ways to interpret this: for
   * instance, we could do it by removing a tab from the list when
   * you open it -- but then if you close it, you can't get back to
   * it.  So the way I'm doing it here is to not show a tab in the menu
   * if you have a tab open to the same URL, even though this means
   * that as soon as you navigate anywhere, the original tab will
   * reappear in the menu.
   */
  locallyOpenTabMatchesURL: function TabEngine_localTabMatches(url) {
    return this._store.getAllTabs().some(function(tab) {
      return tab.urlHistory[0] == url;
    });
  }
};


function TabStore(name) {
  Store.call(this, name);
}
TabStore.prototype = {
  __proto__: Store.prototype,

  itemExists: function TabStore_itemExists(id) {
    return id == Clients.localID;
  },

  getAllTabs: function getAllTabs(filter) {
    let filteredUrls = new RegExp(Svc.Prefs.get("engine.tabs.filteredUrls"), "i");

    let allTabs = [];

    let currentState = JSON.parse(Svc.Session.getBrowserState());
    currentState.windows.forEach(function(window) {
      window.tabs.forEach(function(tab) {
        // Make sure there are history entries to look at.
        if (!tab.entries.length)
          return;
        // Until we store full or partial history, just grab the current entry.
        // index is 1 based, so make sure we adjust.
        let entry = tab.entries[tab.index - 1];

        // Filter out some urls if necessary. SessionStore can return empty
        // tabs in some cases - easiest thing is to just ignore them for now.
        if (!entry.url || filter && filteredUrls.test(entry.url))
          return;

        // weaveLastUsed will only be set if the tab was ever selected (or
        // opened after Weave was running). So it might not ever be set.
        // I think it's also possible that attributes[.image] might not be set
        // so handle that as well.
        allTabs.push({
          title: entry.title || "",
          urlHistory: [entry.url],
          icon: tab.attributes && tab.attributes.image || "",
          lastUsed: tab.extData && tab.extData.weaveLastUsed || 0
        });
      });
    });

    return allTabs;
  },

  createRecord: function createRecord(guid) {
    let record = new TabSetRecord();
    record.clientName = Clients.localName;

    // Sort tabs in descending-used order to grab the most recently used
    let tabs = this.getAllTabs(true).sort(function(a, b) {
      return b.lastUsed - a.lastUsed;
    });

    // Figure out how many tabs we can pack into a payload. Starting with a 28KB
    // payload, we can estimate various overheads from encryption/JSON/WBO.
    let size = JSON.stringify(tabs).length;
    let origLength = tabs.length;
    const MAX_TAB_SIZE = 20000;
    if (size > MAX_TAB_SIZE) {
      // Estimate a little more than the direct fraction to maximize packing
      let cutoff = Math.ceil(tabs.length * MAX_TAB_SIZE / size);
      tabs = tabs.slice(0, cutoff + 1);

      // Keep dropping off the last entry until the data fits
      while (JSON.stringify(tabs).length > MAX_TAB_SIZE)
        tabs.pop();
    }

    this._log.trace("Created tabs " + tabs.length + " of " + origLength);
    tabs.forEach(function(tab) {
      this._log.trace("Wrapping tab: " + JSON.stringify(tab));
    }, this);

    record.tabs = tabs;
    return record;
  },

  getAllIDs: function TabStore_getAllIds() {
    let ids = {};
    ids[Clients.localID] = true;
    return ids;
  },

  wipe: function TabStore_wipe() {
    this._remoteClients = {};
  },

  create: function TabStore_create(record) {
    this._log.debug("Adding remote tabs from " + record.clientName);
    this._remoteClients[record.id] = record.cleartext;

    // Lose some precision, but that's good enough (seconds)
    let roundModify = Math.floor(record.modified / 1000);
    let notifyState = Svc.Prefs.get("notifyTabState");
    // If there's no existing pref, save this first modified time
    if (notifyState == null)
      Svc.Prefs.set("notifyTabState", roundModify);
    // Don't change notifyState if it's already 0 (don't notify)
    else if (notifyState == 0)
      return;
    // We must have gotten a new tab that isn't the same as last time
    else if (notifyState != roundModify)
      Svc.Prefs.set("notifyTabState", 0);
  }
};


function TabTracker(name) {
  Tracker.call(this, name);

  // Make sure "this" pointer is always set correctly for event listeners
  this.onTab = Utils.bind2(this, this.onTab);

  // Register as an observer so we can catch windows opening and closing:
  Svc.WinWatcher.registerNotification(this);

  // Also register listeners on already open windows
  let wins = Svc.WinMediator.getEnumerator("navigator:browser");
  while (wins.hasMoreElements())
    this._registerListenersForWindow(wins.getNext());
}
TabTracker.prototype = {
  __proto__: Tracker.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  _registerListenersForWindow: function TabTracker__registerListen(window) {
    this._log.trace("Registering tab listeners in new window");

    // For each topic, add or remove onTab as the listener
    let topics = ["pageshow", "TabOpen", "TabClose", "TabSelect"];
    let onTab = this.onTab;
    let addRem = function(add) topics.forEach(function(topic) {
      window[(add ? "add" : "remove") + "EventListener"](topic, onTab, false);
    });

    // Add the listeners now and remove them on unload
    addRem(true);
    window.addEventListener("unload", function() addRem(false), false);
  },

  observe: function TabTracker_observe(aSubject, aTopic, aData) {
    // Add tab listeners now that a window has opened
    if (aTopic == "domwindowopened") {
      let self = this;
      aSubject.addEventListener("load", function onLoad(event) {
        aSubject.removeEventListener("load", onLoad, false);
        // Only register after the window is done loading to avoid unloads
        self._registerListenersForWindow(aSubject);
      }, false);
    }
  },

  onTab: function onTab(event) {
    this._log.trace("onTab event: " + event.type);
    this.addChangedID(Clients.localID);

    // For pageshow events, only give a partial score bump (~.1)
    let chance = .1;

    // For regular Tab events, do a full score bump and remember when it changed
    if (event.type != "pageshow") {
      chance = 1;

      // Store a timestamp in the tab to track when it was last used
      Svc.Session.setTabValue(event.originalTarget, "weaveLastUsed",
                              Math.floor(Date.now() / 1000));
    }

    // Only increase the score by whole numbers, so use random for partial score
    if (Math.random() < chance)
      this.score++;
  },
}
