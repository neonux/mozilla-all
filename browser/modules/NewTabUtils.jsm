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
 * The Original Code is New Tab Page code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Taubert <ttaubert@mozilla.com> (Original Author)
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

"use strict";

let EXPORTED_SYMBOLS = ["NewTabUtils"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

// The preference that tells whether this feature is enabled.
const PREF_NEWTAB_ENABLED = "browser.newtabpage.enabled";

// The maximum number of results we want to retrieve from history.
const HISTORY_RESULTS_LIMIT = 100;

// Define some lazyily loaded services.
XPCOMUtils.defineLazyServiceGetter(this, "gPrivateBrowsing",
  "@mozilla.org/privatebrowsing;1", "nsIPrivateBrowsingService");

XPCOMUtils.defineLazyServiceGetter(this, "gScriptSecurityManager",
  "@mozilla.org/scriptsecuritymanager;1", "nsIScriptSecurityManager");

XPCOMUtils.defineLazyServiceGetter(this, "gStorageManager",
  "@mozilla.org/dom/storagemanager;1", "nsIDOMStorageManager");

/**
 * Singleton that provides storage functionality.
 */
let Storage = {
  /**
   * Cached storage object.
   */
  _storage: null,

  /**
   * Tells whether we already added a private browsing mode observer.
   */
  _observing: false,

  /**
   * The local storage.
   */
  get storage() {
    if (!this._storage) {
      let uri = Services.io.newURI("about:newtab", null, null);
      let principal = gScriptSecurityManager.getCodebasePrincipal(uri);
      this._storage = gStorageManager.getLocalStorageForPrincipal(principal, "");

      // Check if we're starting in private browsing mode.
      if (gPrivateBrowsing.privateBrowsingEnabled)
        this._storage = new PrivateBrowsingStorage(this._storage);

      // Register an observer to listen for private browsing mode changes.
      if (!this._observing) {
        this._observing = true;
        Services.obs.addObserver(this, "private-browsing", true);
      }
    }

    return this._storage;
  },

  /**
   * Gets the value for a given key from the storage.
   * @param aName The storage key.
   * @param aDefault A default value if the key doesn't exist.
   * @return The value for the given key.
   */
  get: function Storage_get(aName, aDefault) {
    let value;

    try {
      value = JSON.parse(this.storage.getItem(aName));
    } catch (e) {}

    return value || aDefault;
  },

  /**
   * Sets the storage value for a given key.
   * @param aName The storage key.
   * @param aValue The value to set.
   */
  set: function Storage_set(aName, aValue) {
    this.storage.setItem(aName, JSON.stringify(aValue));
  },

  /**
   * Clears the storage and removes all values.
   */
  clear: function Storage_clear() {
    this.storage.clear();
  },

  /**
   * Implements the nsIObserver interface to get notified about private
   * browsing mode changes.
   */
  observe: function Storage_observe(aSubject, aTopic, aData) {
    if (aData == "enter") {
      // When switching to private browsing mode we keep the current state
      // of the grid and provide a volatile storage for it that is
      // discarded upon leaving private browsing.
      this._storage = new PrivateBrowsingStorage(this.storage);
    } else {
      // Reset to normal DOM storage.
      this._storage = null;

      // When switching back from private browsing we need to reset the
      // grid and re-read its values from the underlying storage. We don't
      // want any data from private browsing to show up.
      PinnedLinks.resetCache();
      BlockedLinks.resetCache();

      Pages.update();
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

/**
 * This class implements a temporary storage used while the user is in private
 * browsing mode. It is discarded when leaving pb mode.
 */
function PrivateBrowsingStorage(aStorage) {
  this._data = {};

  for (let i = 0; i < aStorage.length; i++) {
    let key = aStorage.key(i);
    this._data[key] = aStorage.getItem(key);
  }
}

PrivateBrowsingStorage.prototype = {
  /**
   * The data store.
   */
  _data: null,

  /**
   * Gets the value for a given key from the storage.
   * @param aName The storage key.
   * @param aDefault A default value if the key doesn't exist.
   * @return The value for the given key.
   */
  getItem: function PrivateBrowsingStorage_getItem(aName) {
    return this._data[aName] || null;
  },

  /**
   * Sets the storage value for a given key.
   * @param aName The storage key.
   * @param aValue The value to set.
   */
  setItem: function PrivateBrowsingStorage_setItem(aName, aValue) {
    this._data[aName] = aValue;
  },

  /**
   * Clears the storage and removes all values.
   */
  clear: function PrivateBrowsingStorage_clear() {
    this._data = {};
  }
};

/**
 * Singleton that serves as a registry for all open 'New Tab Page's.
 */
let AllPages = {
  /**
   * The array containing all active pages.
   */
  _pages: [],

  /**
   * Tells whether we already added a preference observer.
   */
  _observing: false,

  /**
   * Adds a page to the internal list of pages.
   * @param aPage The page to register.
   */
  register: function AllPages_register(aPage) {
    this._pages.push(aPage);

    // Add the preference observer if we haven't already.
    if (!this._observing) {
      this._observing = true;
      Services.prefs.addObserver(PREF_NEWTAB_ENABLED, this, true);
    }
  },

  /**
   * Removes a page from the internal list of pages.
   * @param aPage The page to unregister.
   */
  unregister: function AllPages_unregister(aPage) {
    let index = this._pages.indexOf(aPage);
    this._pages.splice(index, 1);
  },

  /**
   * Updates all currently active pages but the given one.
   * @param aExceptPage The page to exclude from updating.
   */
  update: function AllPages_update(aExceptPage) {
    this._pages.forEach(function (aPage) {
      if (aExceptPage != aPage)
        aPage.update();
    });
  },

  /**
   * Implements the nsIObserver interface to get notified when the preference
   * value changes.
   */
  observe: function AllPages_observe() {
    let enabled = Services.prefs.getBoolPref(PREF_NEWTAB_ENABLED);
    this._pages.forEach(function (aPage) aPage.enabled = enabled);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

/**
 * Singleton that keeps track of all pinned links and their positions in the
 * grid.
 */
let PinnedLinks = {
  /**
   * The cached list of pinned links.
   */
  _links: null,

  /**
   * The array of pinned links.
   */
  get links() {
    if (!this._links)
      this._links = Storage.get("pinnedLinks", []);

    return this._links;
  },

  /**
   * Pins a link at the given position.
   * @param aLink The link to pin.
   * @param aIndex The grid index to pin the cell at.
   */
  pin: function PinnedLinks_pin(aLink, aIndex) {
    // Clear the link's old position, if any.
    this.unpin(aLink);

    this.links[aIndex] = aLink;
    Storage.set("pinnedLinks", this.links);
  },

  /**
   * Unpins a given link.
   * @param aLink The link to unpin.
   */
  unpin: function PinnedLinks_unpin(aLink) {
    let index = this._indexOfLink(aLink);
    if (index != -1) {
      this.links[index] = null;
      Storage.set("pinnedLinks", this.links);
    }
  },

  /**
   * Checks whether a given link is pinned.
   * @params aLink The link to check.
   * @return whether The link is pinned.
   */
  isPinned: function PinnedLinks_isPinned(aLink) {
    return this._indexOfLink(aLink) != -1;
  },

  /**
   * Resets the links cache.
   */
  resetCache: function PinnedLinks_resetCache() {
    this._links = null;
  },

  /**
   * Finds the index of a given link in the list of pinned links.
   * @param aLink The link to find an index for.
   * @return The link's index.
   */
  _indexOfLink: function PinnedLinks_indexOfLink(aLink) {
    for (let i = 0; i < this.links.length; i++) {
      let link = this.links[i];
      if (link && link.url == aLink.url)
        return i;
    }

    // The given link is unpinned.
    return -1;
  }
};

/**
 * Singleton that keeps track of all blocked links in the grid.
 */
let BlockedLinks = {
  /**
   * The cached list of blocked links.
   */
  _links: null,

  /**
   * The list of blocked links.
   */
  get links() {
    if (!this._links)
      this._links = Storage.get("blockedLinks", {});

    return this._links;
  },

  /**
   * Blocks a given link.
   * @param aLink The link to block.
   */
  block: function BlockedLinks_block(aLink) {
    this.links[aLink.url] = 1;

    // Make sure we unpin blocked links.
    PinnedLinks.unpin(aLink);

    Storage.set("blockedLinks", this.links);
  },

  /**
   * Returns whether a given link is blocked.
   * @param aLink The link to check.
   */
  isBlocked: function BlockedLinks_isBlocked(aLink) {
    return (aLink.url in this.links);
  },

  /**
   * Checks whether the list of blocked links is empty.
   * @return Whether the list is empty.
   */
  isEmpty: function BlockedLinks_isEmpty() {
    return Object.keys(this.links).length == 0;
  },

  /**
   * Resets the links cache.
   */
  resetCache: function BlockedLinks_resetCache() {
    this._links = null;
  }
};

/**
 * Singleton that serves as the default link provider for the grid. It queries
 * the history to retrieve the most frequently visited sites.
 */
let PlacesProvider = {
  /**
   * Gets the current set of links delivered by this provider.
   * @param aCallback The callback to call when the links have been retrieved.
   */
  getLinks: function PlacesProvider_getLinks(aCallback) {
    let options = PlacesUtils.history.getNewQueryOptions();
    options.maxResults = HISTORY_RESULTS_LIMIT;

    // Sort by frecency, descending.
    options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_FRECENCY_DESCENDING

    // We don't want source redirects for this query.
    options.redirectsMode = Ci.nsINavHistoryQueryOptions.REDIRECTS_MODE_TARGET;

    let links = [];

    let callback = {
      handleResult: function (aResultSet) {
        let row;

        while (row = aResultSet.getNextRow()) {
          let url = row.getResultByIndex(1);
          let title = row.getResultByIndex(2);
          links.push({url: url, title: title});
        }
      },

      handleError: function (aError) {
        // Should we somehow handle this error?
        aCallback([]);
      },

      handleCompletion: function (aReason) {
        aCallback(links);
      }
    };

    // Execute the query.
    let query = PlacesUtils.history.getNewQuery();
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase);
    db.asyncExecuteLegacyQueries([query], 1, options, callback);
  }
};

/**
 * Singleton that provides access to all links contained in the grid (including
 * the ones that don't fit on the grid).
 */
let Links = {
  /**
   * The links cache.
   */
  _links: [],

  /**
   * The default provider for links.
   */
  _provider: PlacesProvider,

  /**
   * Populates the cache with fresh links from the current provider.
   * @param aCallback The callback to call when finished (optional).
   */
  populateCache: function Links_populateCache(aCallback) {
    let self = this;

    this._provider.getLinks(function (aLinks) {
      self._links = aLinks;
      aCallback && aCallback();
    });
  },

  /**
   * Gets the current set of links contained in the grid.
   * @return The links in the grid.
   */
  getLinks: function Links_getLinks() {
    let pinnedLinks = Array.slice(PinnedLinks.links);

    // Filter blocked and pinned links.
    let links = this._links.filter(function (link) {
      return !BlockedLinks.isBlocked(link) && !PinnedLinks.isPinned(link);
    });

    // Try to fill the gaps between pinned links.
    for (let i = 0; i < pinnedLinks.length && links.length; i++)
      if (!pinnedLinks[i])
        pinnedLinks[i] = links.shift();

    // Append the remaining links if any.
    if (links.length)
      pinnedLinks = pinnedLinks.concat(links);

    return pinnedLinks;
  }
};

/**
 * Singleton that provides the public API of this JSM.
 */
let NewTabUtils = {
  /**
   * Initializes and prepares the NewTabUtils module.
   */
  init: function NewTabUtils_init() {
    // Prefetch the links.
    Links.populateCache();
  },

  /**
   * Resets the NewTabUtils module, its links and its storage.
   * @param aCallback The callback to call when finished (optional).
   */
  reset: function NewTabUtils_reset(aCallback) {
    Storage.clear();

    PinnedLinks.resetCache();
    BlockedLinks.resetCache();

    Links.populateCache(aCallback);
  },

  allPages: AllPages,
  links: Links,
  pinnedLinks: PinnedLinks,
  blockedLinks: BlockedLinks
};
