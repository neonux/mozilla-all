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
 *   Margaret Leibovic <margaret.leibovic@gmail.com> (Original Author)
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

var EXPORTED_SYMBOLS = ["NewTabUtils", "NewTabPage"];

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

let gBookmarksService = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                        getService(Ci.nsINavBookmarksService);
let gHistoryService = Cc["@mozilla.org/browser/nav-history-service;1"].
                      getService(Ci.nsINavHistoryService);
let gHistoryACSearch = Cc["@mozilla.org/autocomplete/search;1?name=history"].
                       getService(Ci.nsIAutoCompleteSearch);

XPCOMUtils.defineLazyGetter(this, "FileUtils", function() {
  Cu.import("resource://gre/modules/FileUtils.jsm");
  return FileUtils;
});
XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

function Site(url, title, favicon, itemId, isBookmark) {
  this.url = url;
  this.title = title;
  this.favicon = favicon || "chrome://mozapps/skin/places/defaultFavicon.png";
  this.itemId = itemId;
  this.isBookmark = isBookmark;
}

function NewTabPage(bookmarksContainer, topSitesContainer) {
  this.bookmarksContainer = bookmarksContainer;
  this.topSitesContainer = topSitesContainer;
  this.document = bookmarksContainer.ownerDocument;
}

NewTabPage.prototype = {
  init: function NTP_init() {
    let bookmarksFragment = NewTabUtils.getBookmarksFragment(this.document);
    this.bookmarksContainer.appendChild(bookmarksFragment);

    NewTabUtils.getTopSitesFragment((function(topSitesFragment){
      this.topSitesContainer.appendChild(topSitesFragment);
    }).bind(this), this.document);
  },

  restoreSites: function NTP_restoreSites() {
    NewTabUtils.clearBlockedURLs();
    NewTabUtils.getTopSitesFragment((function(topSitesFragment){
      // Get rid of the old sites
      while (this.topSitesContainer.children.length) {
        this.topSitesContainer.removeChild(this.topSitesContainer.firstChild);
      }
      // Replace with new sites
      this.topSitesContainer.appendChild(topSitesFragment);
    }).bind(this), this.document);
  },

  onBookmarksDragOver: function NTP_onBookmarksDragOver(event) {
    if (event.dataTransfer.types.contains("application/x-moz-node")) {
      event.preventDefault();
    }
  },

  onBookmarksDrop: function NTP_onBookmarksDrop(event) {
    if (!event.dataTransfer.types.contains("application/x-moz-node")) {
      return;
    }
    event.preventDefault();

    let siteItem = event.dataTransfer.mozGetDataAt("application/x-moz-node", 0);
    // Don't do anything if the site is already a bookmark
    if (siteItem.hasAttribute("bookmark")) {
      return;
    }

    this.bookmarksContainer.appendChild(siteItem);
    siteItem.setAttribute("bookmark", true);

    NewTabUtils.moveToBookmarks(siteItem.getAttribute("url"));
    // Add a new top site to replace the one we moved.
    NewTabUtils.addNewTopSite(this.topSitesContainer);
  },

  hideSection: function NTP_hideSection(aSection) {
    aSection.addEventListener("transitionend", function() {
      aSection.setAttribute("hidden", true);
    }, false);
    aSection.setAttribute("hiding", true);
  }
};

let NewTabUtils = {
  MAX_BOOKMARK_SITES: 12,
  MAX_TOP_SITES: 12,
  UPDATE_PERIOD: 86400000,
  BLOCKED_URLS_FILE: "newtab/blockedURLs.json",

  _bookmarksFragment: null,
  _topSitesFragment: null,

  _topSites: [],
  _topSitesCallbacks: [],
  _lastUpdated: 0,
  _searchingHistory: 0,

  // mapping of url -> boolean
  _blockedURLs: {},
  _blockedURLsLoaded: false,

  _faviconColors: {},

  /* ----- Bookmarks ----- */

  addBookmarkObservers: function NTU_addBookmarkObservers() {
    // Fetch bookmarks again when things change
    // We also need to update top sites, since those sites may be recently added/deleted bookmarks
    // TODO: be smarter
    let bookmarkObserver = {
      onBeginUpdateBatch: function(){},
      onEndUpdateBatch: function(){},
      onItemAdded: function(aItemId, aParentId, aIndex, aItemType, aURI,
                            aTitle, aDateAdded, aGUID, aParentGUID) {
        NewTabUtils._lastUpdated = 0;
      },
      onBeforeItemRemoved: function(aItemId, aItemType, aParentId, aGUID, aParentGUID) {},
      onItemRemoved: function(aItemId, aParentId, aIndex, aItemType, aURI, aGUID, aParentGUID) {
        NewTabUtils._lastUpdated = 0;
      },
      onItemChanged: function(aItemId, aProperty, aIsAnnotationProperty, aNewValue,
                              aLastModified, aItemType, aParentId, aGUID, aParentGUID) {
        NewTabUtils._lastUpdated = 0;
      },
      onItemVisited: function(aItemId, aVisitId, aTime, aTransitionType, aURI,
                              aParentId, aGUID, aParentGUID) {},
      onItemMoved: function(aItemId, aOldParentId, aOldIndex, aNewParentId, aNewIndex,
                            aItemType, aGUID, aOldParentGUID, aNewParentGUID) {
        NewTabUtils._lastUpdated = 0;
      }
    };
    gBookmarksService.addObserver(bookmarkObserver, false);

    this._bookmarkObserversAdded = true;
  },

  getBookmarksFragment: function NTU_getBookmarksFragment(aDoc) {
    if (!this._bookmarkObserversAdded) {
      this.addBookmarkObservers();
    }

    if (this._bookmarksFragment && this._lastUpdated + this.UPDATE_PERIOD > Date.now()) {
      return aDoc.importNode(this._bookmarksFragment, true);
    }

    let bookmarkSites = this.getBookmarkSites();
    let bookmarksFragment = this.createSitesFragment(bookmarkSites, aDoc);
    // Cache a copy of the fragment
    this._bookmarksFragment = bookmarksFragment.cloneNode(true);
    return bookmarksFragment;
  },

  getBookmarkSites: function NTU_getSitesFromBookmarks() {
    let bookmarkSites = [];
    // nsINavHistoryContainerResultNode
    let folder = PlacesUtils.getFolderContents(PlacesUtils.bookmarksMenuFolderId).root;
    for (let i = 0; i < folder.childCount && i < this.MAX_BOOKMARK_SITES; i++) {
      let node = folder.getChild(i);
      if (PlacesUtils.nodeIsBookmark(node)) {
        let site = new Site(node.uri, node.title, node.icon, node.itemId, true);
        bookmarkSites.push(site);
      }
    }
    return bookmarkSites;
  },

  removeBookmarkSite: function NTU_removeBookmarkSite(aSiteURL) {
    let siteItem = this._getBookmarkSiteItem(aSiteURL);
    // Delete bookmark. TODO: move to unsorted bookmarks?
    gBookmarksService.removeItem(siteItem.getAttribute("itemId"));

    // Remove the item from the cached DOM fragment
    this._bookmarksFragment.removeChild(siteItem);
  },

  _getBookmarkSiteItem: function NTU_getBookmarkSiteItem(aSiteURL) {
    return this._bookmarksFragment.querySelector(".site-item[url='" + aSiteURL + "']");
  },

  /* ----- Top Sites ----- */

  addHistoryObservers: function NTU_addHistoryObservers() {
    // Invalidate the cache if the user explicitly deletes sites.
    // TODO: be smarter
    let historyObserver = {
      onBeginUpdateBatch: function() {},
      onEndUpdateBatch: function() {},
      onClearHistory: function() {
        NewTabUtils._lastUpdated = 0;
      },
      onVisit: function() {},
      onTitleChanged: function() {},
      onBeforeDeleteURI: function() {},
      onDeleteURI: function() {
        NewTabUtils._lastUpdated = 0;
      },
      onPageChanged: function() {},
      onDeleteVisits: function() {
        NewTabUtils._lastUpdated = 0;
      }
    };
    gHistoryService.addObserver(historyObserver, false);

    Services.obs.addObserver(function(aSubject, aTopic, aData) {
      // Invalidate cache so we get new sites the next time the page is loaded.
      NewTabUtils._lastUpdated = 0;

      // Delete all blocked URLs from this domain.
      for (let url in NewTabUtils._blockedURLs) {
        if (url.hasRootDomain(aData)) {
          delete url;
        }
      }
      NewTabUtils.saveBlockedURLs();
    }, "browser:purge-domain-data", false);

    this._historyObserversAdded = true;
  },

  // aCallback takes a DOM fragment of top sites
  getTopSitesFragment: function NTU_getTopSitesFragment(aCallback, aDoc) {
    if (!this._historyObserversAdded) {
      this.addHistoryObservers();
    }

    if (this._topSitesFragment && this._lastUpdated + this.UPDATE_PERIOD > Date.now()) {
      aCallback(aDoc.importNode(this._topSitesFragment, true));
      return;
    }

    this.loadBlockedURLs((function() {
      this.getTopSites((function(topSites) {
        // Make site items with the first chunk of sites in the topSites array
        let topSitesFragment = this.createSitesFragment(topSites.slice(0, this.MAX_TOP_SITES), aDoc);
        this._topSitesFragment = topSitesFragment.cloneNode(true);
        aCallback(topSitesFragment);
      }).bind(this));
    }).bind(this));
  },

  // Get up to 100 top sites from places autocomplete
  // aCallback takes an array of Site objects
  getTopSites: function NTU_getTopSites(aCallback) {
    let listener = {
      onSearchResult: function(aSearch, aResult) {
        // TODO: add error handling
        if (aResult.searchResult != Ci.nsIAutoCompleteResult.RESULT_SUCCESS) {
          return;
        }

        let topSites = [];
        for (let i = 0; i < aResult.matchCount; i++) {
          let url = aResult.getValueAt(i);
          // Don't add a site that's in the blacklist or a bookmark.
          if (NewTabUtils._blockedURLs[url] ||
              NewTabUtils._getBookmarkSiteItem(url)) {
            continue;
          }

          let site = new Site(url, aResult.getCommentAt(i), aResult.getImageAt(i));
          topSites.push(site);
        }

        while (NewTabUtils._topSitesCallbacks.length) {
          let callback = NewTabUtils._topSitesCallbacks.shift();
          callback(topSites);
        }

        NewTabUtils._topSites = topSites;
        NewTabUtils._lastUpdated = Date.now();
        NewTabUtils._searchingHistory = 0;
      },
      onUpdateSearchResult: function(aSearch, aResult) { }
    }
    // Starting a search will cancel previous search queries, so we'll make a queue of callbacks.
    this._topSitesCallbacks.push(aCallback);

    // Only kick off the search if there's not already a search going on
    // Use a timer hack because places autocomplete code may kill this search, so the result handler will never be called
    if (Date.now() - this._searchingHistory > 1000) {    
      // The "newtab-maxresults" flag causes the search to return up to 100 results.
      gHistoryACSearch.startSearch("", "newtab-maxresults", null, listener);
      this._searchingHistory = Date.now();
    }
  },

  // aSiteURL is the url of the site to delete from the _topSites array
  removeTopSite: function NTU_removeTopSite(aSiteURL) {
    let siteItem = this._getTopSiteItem(aSiteURL);
    // Remove the item from the cached DOM fragment
    this._topSitesFragment.removeChild(siteItem);

    let i = this._indexOfTopSite(aSiteURL);
    this._topSites.splice(i, 1);

    this._blockedURLs[aSiteURL] = true;
    this.saveBlockedURLs();
  },

  addNewTopSite: function NTU_addTopSite(aTopSitesContainer) {
    let newSite = this._topSites[this.MAX_TOP_SITES];
    if (!newSite) {
      Cu.reportError("NewTabUtils [addNewTopSite]: No more top sites!");
      return;
    }

    let siteItem = this.createSiteItem(newSite, aTopSitesContainer.ownerDocument);
    aTopSitesContainer.appendChild(siteItem);
    this._topSitesFragment.appendChild(siteItem.cloneNode(true));
  },

  // Moves a site from top sites to bookmarks
  moveToBookmarks: function NTU_moveToBookmarks(aSiteURL) {
    // Update _topSites
    let site = this._topSites.splice(this._indexOfTopSite(aSiteURL), 1)[0];
    let itemId = gBookmarksService.insertBookmark(gBookmarksService.bookmarksMenuFolder,
                                                   PlacesUtils._uri(site.url),
                                                   gBookmarksService.DEFAULT_INDEX,
                                                   site.title);
    // Update _topSitesFragment
    let siteItem = this._getTopSiteItem(aSiteURL);
    siteItem.setAttribute("bookmark", true);
    siteItem.setAttribute("itemId", itemId);
    // This also removes the siteItem from _topSitesFramgment
    this._bookmarksFragment.appendChild(siteItem);
  },

  _getTopSiteItem: function NTU_getTopSiteItem(aSiteURL) {
    return this._topSitesFragment.querySelector(".site-item[url='" + aSiteURL + "']");
  },

  // Returns the index of a site in _topSites.
  // Returns -1 if the site is not in the array.
  _indexOfTopSite: function NTU_indexOfTopSite(aSiteURL) {
    for (let i = 0; i < this._topSites.length; i++) {
      let site = this._topSites[i];
      if (site.url == aSiteURL) {
        return i;
      }
    }
    return -1;
  },

  /* ----- File I/O Helper Methods ----- */

  // aCallback is executed after the blocked urls are loaded into memory
  loadBlockedURLs: function NTU_loadBlockedURLs(aCallback) {
    // If the URLs are already loaded, just call the callback function
    if (this._blockedURLsLoaded) {
      aCallback();
      return;
    }

    this._loadFromFile(this.BLOCKED_URLS_FILE, function(blockedURLs) {
      if (blockedURLs) {
        NewTabUtils._blockedURLs = blockedURLs;
      }
      NewTabUtils._blockedURLsLoaded = true;
      aCallback();
    });  
  },

  saveBlockedURLs: function NTU_saveBlockedURLs() {
    this._saveToFile(this.BLOCKED_URLS_FILE, this._blockedURLs);
  },

  clearBlockedURLs: function NTU_clearBlockedURLs() {
    this._blockedURLs = {};
    this._lastUpdated = 0;
    this.saveBlockedURLs();
  },

  // aCallback takes JSON object made from file contents as a parameter
  // object will be null if the file does not exist
  _loadFromFile: function NTU_loadFromFile(aFile, aCallback) {
    let file = FileUtils.getFile("ProfD", aFile.split("/"), true);
    if (!file.exists()) {
      aCallback(null);
      return;
    }
    try {
      let channel = NetUtil.newChannel(file);
      channel.contentType = "application/json";
      NetUtil.asyncFetch(channel, function(aInputStream, aResultCode) {
        if (Components.isSuccessCode(aResultCode)) {
          fileJSON = Cc["@mozilla.org/dom/json;1"].
                     createInstance(Ci.nsIJSON).
                     decodeFromStream(aInputStream, aInputStream.available());
          aCallback(fileJSON);
        } else {
          Cu.reportError("NewTabUtils [loadFromFile]: " + aResultCode);
        }
      });
    } catch (e) {
      Cu.reportError("NewTabUtils [loadFromFile]: " + e);
    }
  },

  _saveToFile: function NTU_saveToFile(aFile, aObj) {
    let file = FileUtils.getFile("ProfD", aFile.split("/"), true);    
    let fos = FileUtils.openSafeFileOutputStream(file);
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    try {
      let ins = converter.convertToInputStream(JSON.stringify(aObj));
      NetUtil.asyncCopy(ins, fos, function(aResultCode) {
        if (!Components.isSuccessCode(aResultCode)) {
          Cu.reportError("NewTabUtils [saveToFile]: " + aResultCode);
        }
      });
    } catch (e) {
      Cu.reportError("NewTabUtils [saveToFile]: " + e);
    }
  },

  /* ----- DOM Methods ----- */

  createSitesFragment: function NTU_createSitesFragment(aSites, aDoc) {
    let fragment = aDoc.createDocumentFragment();
    for (let i = 0; i < aSites.length; i++) {
      let siteItem = this.createSiteItem(aSites[i], aDoc);
      fragment.appendChild(siteItem);
    }
    return fragment;
  },

  createSiteItem: function NTU_createSiteItem(aSite, aDoc) {
    let imageItem = aDoc.createElement("img");
    imageItem.className = "image-item";
    imageItem.setAttribute("src", aSite.favicon);
    imageItem.setAttribute("onload", "NewTabUtils.setIconColor(this);");

    let imageContainer = aDoc.createElement("div");
    imageContainer.className = "image-container";
    imageContainer.setAttribute("onclick", "document.location = '" + aSite.url + "'");
    imageContainer.appendChild(imageItem);

    let urlItem = aDoc.createElement("a");
    urlItem.className = "url-item";    
    urlItem.href = aSite.url;
    let title = aSite.title || aSite.url;
    if (title.length > 40) {
      let ellipsis = Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data;
      title = title.substr(0,40).trim() + ellipsis;   
    }
    urlItem.textContent = title;

    let removeButton = aDoc.createElement("button");
    removeButton.className = "remove-button";
    removeButton.setAttribute("onclick", "NewTabUtils.removeSiteItem(event);");

    let siteItem = aDoc.createElement("div");
    siteItem.className = "site-item";
    siteItem.setAttribute("url", aSite.url);
    siteItem.appendChild(removeButton);
    siteItem.appendChild(imageContainer);
    siteItem.appendChild(urlItem);

    if (aSite.isBookmark) {
      siteItem.setAttribute("bookmark", true);
      siteItem.setAttribute("itemId", aSite.itemId);
    } else {
      // Set up drag and drop for top sites items
      siteItem.setAttribute("draggable", true);
      siteItem.setAttribute("ondragstart", "NewTabUtils.dragSiteItem(event);");
    }
    return siteItem;
  },
  
  dragSiteItem: function NTU_dragSiteItem(event) {
    let siteItem = event.target;
    while (siteItem.className != "site-item") {
      siteItem = siteItem.parentNode;
    }
    event.dataTransfer.mozSetDataAt("application/x-moz-node", siteItem, 0);
    event.dataTransfer.setDragImage(siteItem.querySelector(".image-container"), 16, 16);
  },

  removeSiteItem: function NTU_removeSiteItem(event) {
    let siteItem = event.target.parentNode;
    let bookmark = siteItem.hasAttribute("bookmark");
    if (bookmark) {
      this.removeBookmarkSite(siteItem.getAttribute("url"));
    } else {
      this.removeTopSite(siteItem.getAttribute("url"));
    }

    siteItem.addEventListener("transitionend", (function() {
      let container = siteItem.parentNode;
      container.removeChild(siteItem);
      if (!bookmark) {
        this.addNewTopSite(container);
      }
    }).bind(this), false);
    siteItem.setAttribute("removing", true);
  },

  /* ----- Favicon Color Methods ----- */

  setIconColor: function NTU_setIconColor(aImgElmt) {
    let color = this._faviconColors[aImgElmt.src] || this.getFaviconColor(aImgElmt);
    let parent = aImgElmt.parentNode;
    parent.style.backgroundImage =
      "-moz-linear-gradient(top, rgba(" + color + ",0.1), rgba(" + color + ",0.3))";
    parent.style.borderColor = "rgba(" + color + ",0.9)";
  },

  getFaviconColor: function NTU_getFaviconColor(aImgElmt) {
    let canvas = aImgElmt.ownerDocument.createElement("canvas");
    canvas.height = canvas.width = 16;

    let context = canvas.getContext("2d");
    context.drawImage(aImgElmt, 0, 0);

    // keep track of how many times a color appears in the image
    let colorCount = {};
    let maxCount = 0;
    let faviconColor = "";

    // data is an array of a series of 4 one-byte values representing the rgba values of each pixel
    let data = context.getImageData(0, 0, 16, 16).data;
    for (let i = 0; i < data.length; i += 4) {
      // ignore transparent pixels
      if (data[i+3] == 0) {
        continue;
      }

      let color = data[i] + "," + data[i+1] + "," + data[i+2];
      // ignore white
      if (color == "255,255,255") {
        continue;
      }

      colorCount[color] = colorCount[color] ?  colorCount[color] + 1 : 1;

      // keep track of the color that appears the most times
      if (colorCount[color] > maxCount) {
        maxCount = colorCount[color];
        faviconColor = color;
      }
    }

    this._faviconColors[aImgElmt.src] = faviconColor;
    return faviconColor;
  }
};

// See nsPrivateBrowsingService.js
String.prototype.hasRootDomain = function hasRootDomain(aDomain) {
  let index = this.indexOf(aDomain);
  if (index == -1)
    return false;

  if (this == aDomain)
    return true;

  let prevChar = this[index - 1];
  return (index == (this.length - aDomain.length)) &&
         (prevChar == "." || prevChar == "/");
}
