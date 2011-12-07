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

var EXPORTED_SYMBOLS = ["NewTabUtils"];

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// define some lazyily loaded services
XPCOMUtils.defineLazyServiceGetter(this, "gHistoryACSearch",
  "@mozilla.org/autocomplete/search;1?name=history", "nsIAutoCompleteSearch");

XPCOMUtils.defineLazyServiceGetter(this, "gPrivateBrowsing",
  "@mozilla.org/privatebrowsing;1", "nsIPrivateBrowsingService");

XPCOMUtils.defineLazyServiceGetter(this, "gScriptSecurityManager",
  "@mozilla.org/scriptsecuritymanager;1", "nsIScriptSecurityManager");

XPCOMUtils.defineLazyServiceGetter(this, "gStorageManager",
  "@mozilla.org/dom/storagemanager;1", "nsIDOMStorageManager");

// ##########
// Class: Storage
// Singleton that provides storage functionality.
let Storage = {
  _storage: null,

  // the local storage
  get storage() {
    if (this._storage)
      return this._storage;

    let uri = Services.io.newURI("about:newtab", null, null);
    let principal = gScriptSecurityManager.getCodebasePrincipal(uri);
    let storage = gStorageManager.getLocalStorageForPrincipal(principal, "");

    return this._storage = storage;
  },

  // ----------
  // Function: init
  // Initializes the storage.
  init: function Storage_init() {
    Services.obs.addObserver(this._createPrivateBrowsingObserver(),
                             "private-browsing-transition-complete", false);
  },

  // ----------
  // Function: get
  // Returns the value for a given key from the storage.
  //
  // Parameters:
  //   aName - the storage key
  //   aDefault - a default value if the key doesn't exist
  get: function Storage_get(aName, aDefault) {
    let value;

    try {
      value = JSON.parse(this.storage.getItem(aName));
    } catch (e) {}

    return value || aDefault;
  },

  // ----------
  // Function: set
  // Sets the storage value for a given key.
  //
  // Parameters:
  //   aName - the storage key
  //   aValue - the value to set
  set: function Storage_set(aName, aValue) {
    this.storage.setItem(aName, JSON.stringify(aValue));
  },

  // ----------
  // Function: clear
  // Clears the storage and removes all values.
  clear: function Storage_clear() {
    this.storage.clear();
  },

  // ----------
  // Function: _createPrivateBrowsingObserver
  // Returns an observer that listens for private browsing mode transitions
  // and switches the underlying storage.
  _createPrivateBrowsingObserver:
    function Storage__createPrivateBrowsingObserver() {

    let self = this;

    return {
      observe: function () {
        if (gPrivateBrowsing.privateBrowsingEnabled) {
          // create a temporary storage for the pb mode
          self._storage = new PrivateBrowsingStorage(self.storage);
        } else {
          // reset to normal DOM storage
          self._storage = null;

          // reset all cached link values
          PinnedLinks.resetCache();
          BlockedLinks.resetCache();

          Pages.update();
        }
      }
    };
  }
};

// ##########
// Class: PrivateBrowsingStorage
// This class implements a temporary storage used while the user is in private
// browsing mode. It is discarded when leaving pb mode.
function PrivateBrowsingStorage(aStorage) {
  this._data = {};

  for (let i = 0; i < aStorage.length; i++) {
    let key = aStorage.key(i);
    this._data[key] = aStorage.getItem(key);
  }
}

PrivateBrowsingStorage.prototype = {
  // the data store
  _data: null,

  // ----------
  // Function: getItem
  // Returns the value for a given key from the storage.
  //
  // Parameters:
  //   aName - the storage key
  //   aDefault - a default value if the key doesn't exist
  getItem: function PrivateBrowsingStorage_getItem(aName) {
    if (aName in this._data)
      return this._data[aName];

    return null;
  },

  // ----------
  // Function: setItem
  // Sets the storage value for a given key.
  //
  // Parameters:
  //   aName - the storage key
  //   aValue - the value to set
  setItem: function PrivateBrowsingStorage_setItem(aName, aValue) {
    this._data[aName] = aValue;
  },

  // ----------
  // Function: clear
  // Clears the storage and removes all values.
  clear: function PrivateBrowsingStorage_clear() {
    this._data = {};
  }
};

// ##########
// Class: Pages
// Singleton that serves as a register for all open 'New Tab Page's.
let Pages = {
  // the array containing all active pages
  _pages: [],

  // ----------
  // Function: register
  // Adds a page to the internal list of pages.
  //
  // Parameters:
  //   aPage - the page to register
  register: function Pages_register(aPage) {
    this._pages.push(aPage);
  },

  // ----------
  // Function: unregister
  // Removes a page from the internal list of pages.
  //
  // Parameters:
  //   aPage - the page to unregister
  unregister: function Pages_unregister(aPage) {
    let index = this._pages.indexOf(aPage);
    this._pages.splice(index, 1);
  },

  // ----------
  // Function: update
  // Updates all currently active pages but the given one.
  //
  // Parameters:
  //   aExceptPage - the page to exclude from updating
  update: function Pages_update(aExceptPage) {
    this._pages.forEach(function (aPage) {
      if (aExceptPage != aPage)
        aPage.update();
    });
  }
};

// ##########
// Class: PinnedLinks
// Singleton that keeps track of all pinned links and their positions in the
// grid.
let PinnedLinks = {
  // the cached list of pinned links
  _links: null,

  // the array of pinned links
  get links() {
    if (!this._links)
      this._links = Storage.get("pinnedLinks", []);

    return this._links;
  },

  // ----------
  // Function: pin
  // Pins a link at the given position.
  //
  // Parameters:
  //   aLink - the link to pin
  //   aIndex - the grid index to pin the cell at
  pin: function PinnedLinks_pin(aLink, aIndex) {
    // clear the link's old position, if any
    this.unpin(aLink);

    this.links[aIndex] = aLink;
    Storage.set("pinnedLinks", this.links);
  },

  // ----------
  // Function: unpin
  // Unpins a given link.
  //
  // Parameters:
  //   aLink - the link to unpin
  unpin: function PinnedLinks_unpin(aLink) {
    let index = this._indexOfLink(aLink);

    if (-1 < index) {
      this.links[index] = null;
      Storage.set("pinnedLinks", this.links);
    }
  },

  // ----------
  // Function: isPinned
  // Returns whether a given link is pinned.
  //
  // Parameters:
  //   aLink - the link to check
  isPinned: function PinnedLinks_isPinned(aLink) {
    return (-1 < this._indexOfLink(aLink));
  },

  // ----------
  // Function: resetCache
  // Resets the links cache.
  resetCache: function PinnedLinks_resetCache() {
    this._links = null;
  },

  // ----------
  // Function: _indexOfLink
  // Returns the index of a given link in the list of pinned links.
  //
  // Parameters:
  //   aLink - the link to find an index for
  _indexOfLink: function PinnedLinks__indexOfLink(aLink) {
    for (let i = 0; i < this.links.length; i++) {
      let link = this.links[i];

      if (link && link.url == aLink.url)
        return i;
    }

    // the given link is unpinned
    return -1;
  }
};

// ##########
// Class: BlockedLinks
// Singleton that keeps track of all blocked links in the grid.
let BlockedLinks = {
  // the cached list of blocked links
  _links: null,

  // the list of blocked links
  get links() {
    if (!this._links)
      this._links = Storage.get("blockedLinks", {});

    return this._links;
  },

  // ----------
  // Function: block
  // Blocks a given link.
  //
  // Parameters:
  //   aLink - the link to block
  block: function BlockedLinks_block(aLink) {
    this.links[aLink.url] = 1;

    // make sure we unpin blocked links
    PinnedLinks.unpin(aLink);

    Storage.set("blockedLinks", this.links);
  },

  // ----------
  // Function: isBlocked
  // Returns whether a given link is blocked.
  //
  // Parameters:
  //   aLink - the link to check
  isBlocked: function BlockedLinks_isBlocked(aLink) {
    return (aLink.url in this.links);
  },

  // ----------
  // Function: isEmpty
  // Returns whether the list of blocked links is empty.
  isEmpty: function BlockedLinks_isEmpty() {
    return !Object.keys(this.links).length;
  },

  // ----------
  // Function: resetCache
  // Resets the links cache.
  resetCache: function BlockedLinks_resetCache() {
    this._links = null;
  }
};

// ##########
// Class: AutoCompleteProvider
// Singleton that serves as the default link provider for the grid. It uses
// the autocomplete history to retrieve to most used sites.
let AutoCompleteProvider = {
  // ----------
  // Function: getLinks
  // Returns the current set of links delivered by this provider.
  //
  // Parameters:
  //   aCallback - the callback to call when the links have been retrieved
  getLinks: function AutoCompleteProvider_getLinks(aCallback) {
    let listener = {
      onSearchResult: function (aSearch, aResult) {
        let result = aResult.searchResult;

        // no links found or an error occured
        if (result != Ci.nsIAutoCompleteResult.RESULT_SUCCESS) {
          if (result == Ci.nsIAutoCompleteResult.RESULT_NOMATCH)
            aCallback([]);

          return;
        }

        let links = [];

        // collect the links from the search result
        for (let i = 0; i < aResult.matchCount; i++) {
          let url = aResult.getValueAt(i);
          let title = aResult.getCommentAt(i);
          links.push({url: url, title: title});
        }

        aCallback(links);
      },

      onUpdateSearchResult: function () {}
    };

    gHistoryACSearch.startSearch("", "newtab-maxresults", null, listener);
  }
};

// ##########
// Class: Links
// Singleton that provides access to all links contained in the grid (including
// the ones that don't fit on the grid)
let Links = {
  // the links cache
  _links: [],

  // the default provider for links
  _provider: AutoCompleteProvider,

  // ----------
  // Function: populateCache
  // Populates the cache with fresh links from the current provider.
  //
  // Parameters:
  //   aCallback - the callback to call when finished (optional)
  populateCache: function Links_populateCache(aCallback) {
    let self = this;

    this._provider.getLinks(function (aLinks) {
      self._links = aLinks;
      aCallback && aCallback();
    });
  },

  // ----------
  // Function: getLinks
  // Returns the current set of links contained in the grid.
  getLinks: function Links_getLinks() {
    let pinnedLinks = PinnedLinks.links.concat();

    // filter blocked and pinned links
    let links = this._links.filter(function (link) {
      return !BlockedLinks.isBlocked(link) && !PinnedLinks.isPinned(link);
    });

    // try to fill the gaps between pinned links
    for (let i = 0; i < pinnedLinks.length && links.length; i++)
      if (!pinnedLinks[i])
        pinnedLinks[i] = links.shift();

    // append the remaining links if any
    if (links.length)
      pinnedLinks = pinnedLinks.concat(links);

    return pinnedLinks;
  }
};

// ##########
// Class: Testing
// Singleton that provides testing functionality and wrapped access to core
// objects to mock certain values.
let Testing = {
  // ----------
  // Function: mockLinks
  // Set a mock value for the list of links.
  //
  // Parameters:
  //   aLinks - the list of links
  mockLinks: function Testing_mockLinks(aLinks) {
    Links._queryCallbacks = [];
    Links._provider = {getLinks: function (aCallback) aCallback(aLinks)};
    Links.populateCache();
  },

  // ----------
  // Function: mockPinnedLinks
  // Set a mock value for the list of pinned links.
  //
  // Parameters:
  //   aPinnedLinks - the list of pinned links
  mockPinnedLinks: function Testing_mockPinnedLinks(aPinnedLinks) {
    PinnedLinks._links = aPinnedLinks;
  }
};

// ##########
// Class: NewTabUtils
// Singleton that provides the public API of this JSM.
let NewTabUtils = {
  // ----------
  // Function: init
  // Initalizes and prepares the NewTabUtils module.
  init: function NewTabUtils_init() {
    // prefetch the links
    Links.populateCache();

    // initialize storage
    Storage.init();
  },

  // ----------
  // Function: reset
  // Resets the NewTabUtils module, its links and its storage.
  //
  // Parameters:
  //   aCallback - the callback to call when finished (optional)
  reset: function NewTabUtils_reset(aCallback) {
    Storage.clear();

    PinnedLinks.resetCache();
    BlockedLinks.resetCache();

    Links.populateCache(aCallback);
  },

  Pages: Pages,
  Links: Links,
  Testing: Testing,
  PinnedLinks: PinnedLinks,
  BlockedLinks: BlockedLinks
};
