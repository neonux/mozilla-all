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

var EXPORTED_SYMBOLS = ["NewTabUtils"];

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

let NewTabUtils = {

  MAX_BOOKMARK_SITES: 12,
  MAX_TOP_SITES: 12,
  UPDATE_PERIOD: 86400000,
  BLOCKED_URLS_FILE: "newtab/blockedURLs.json",

  _bookmarkSites: [],
  _topSites: [],
  _lastUpdated: 0,

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
        NewTabUtils.getSitesFromBookmarks(function(aSite){});
      },
      onBeforeItemRemoved: function(aItemId, aItemType, aParentId, aGUID, aParentGUID) {},
      onItemRemoved: function(aItemId, aParentId, aIndex, aItemType, aURI, aGUID, aParentGUID) {
        NewTabUtils._lastUpdated = 0;
        NewTabUtils.getSitesFromBookmarks(function(aSite){});
      },
      onItemChanged: function(aItemId, aProperty, aIsAnnotationProperty, aNewValue,
                              aLastModified, aItemType, aParentId, aGUID, aParentGUID) {
        NewTabUtils.getSitesFromBookmarks(function(aSite){});
      },
      onItemVisited: function(aItemId, aVisitId, aTime, aTransitionType, aURI,
                              aParentId, aGUID, aParentGUID) {},
      onItemMoved: function(aItemId, aOldParentId, aOldIndex, aNewParentId, aNewIndex,
                            aItemType, aGUID, aOldParentGUID, aNewParentGUID) {
        NewTabUtils._lastUpdated = 0;
        NewTabUtils.getSitesFromBookmarks(function(aSite){});
      }
    };
    gBookmarksService.addObserver(bookmarkObserver, false);

    this._bookmarkObserversAdded = true;
  },

  getBookmarkSites: function NTU_getBookmarkSites(aCallback) {
    if (!this._bookmarkObserversAdded) {
      this.addBookmarkObservers();
    }
  
    // Use sites stored in memory if we have them
    if (this._bookmarkSites.length) {
      for (let i = 0; i < this._bookmarkSites.length; i++) {
        aCallback(this._bookmarkSites[i]);
      }
      return;
    }

    // Otherwise get sites from places
    this.getSitesFromBookmarks(aCallback);
  },

  getSitesFromBookmarks: function NTU_getSitesFromBookmarks(aCallback) {
    let bookmarkSites = [];
    // nsINavHistoryContainerResultNode
    let container = PlacesUtils.getFolderContents(PlacesUtils.bookmarksMenuFolderId).root;
    for (let i = 0; i < container.childCount && i < this.MAX_BOOKMARK_SITES; i++) {
      let node = container.getChild(i);
      if (PlacesUtils.nodeIsBookmark(node)) {
        let site = new Site(node.uri, node.title, node.icon, node.itemId, true);
        aCallback(site);
        bookmarkSites.push(site);
      }
    }
    this._bookmarkSites = bookmarkSites;
  },

  removeBookmarkSite: function NTU_removeBookmarkSite(aSiteURL) {
    let i = this._indexOfBookmarkSite(aSiteURL);
    let site = this._bookmarkSites.splice(i, 1)[0];

    // Delete bookmark. TODO: move to unsorted bookmarks?
    gBookmarksService.removeItem(site.itemId);
  },

  // Used when dragging a top site to the bookmarks section
  moveSiteToBookmarks: function NTU_moveSiteToBookmarks(aSiteURL) {
    let i = this._indexOfTopSite(aSiteURL);
    let site = this._topSites.splice(i, 1)[0];
    this._bookmarkSites.push(site);

    site.itemID = gBookmarksService.insertBookmark(gBookmarksService.bookmarksMenuFolder,
                                                   PlacesUtils._uri(site.url),
                                                   gBookmarksService.DEFAULT_INDEX,
                                                   site.title);
  },

  // Returns the index of a bookmark site in _bookmarkSites.
  // Returns -1 if the site is not in the array.
  _indexOfBookmarkSite: function NTU_indexOfBookmarkSite(aSiteURL) {
    for (let i = 0; i < this._bookmarkSites.length; i++) {
      let site = this._bookmarkSites[i];
      if (site.url == aSiteURL) {
        return i;
      }
    }
    return -1;
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

  // aCallback takes a site object as a param
  getTopSites: function NTU_getSites(aCallback) {
    if (!this._historyObserversAdded) {
      this.addHistoryObservers();
    }
  
    // Use sites stored in memory if we have them and they've been updated recently
    if (this._topSites.length && this._lastUpdated + this.UPDATE_PERIOD > Date.now()) {
      for (let i = 0; i < this._topSites.length; i++) {
        aCallback(this._topSites[i]);
      }
      return;
    }

    // Otherwise get sites from places
    this.loadBlockedURLs(function() {
      NewTabUtils.getSitesFromHistory(aCallback);
    });
  },

  // Get top sites from places autocomplete
  // aCallback takes a site object as a param
  getSitesFromHistory: function NTU_getSitesFromHistory(aCallback) {
    let count = 0;
    // Use temporary sites array, in case other methods try to access _sites while results are coming back
    let topSites = [];
    let listener = {
      onSearchResult: function(aSearch, aResult) {        
        for (count; count < aResult.matchCount; count++) {
          let url = aResult.getValueAt(count);
          // Don't add a site that's in the blacklist or a bookmark.
          if (NewTabUtils._blockedURLs[url] ||
              NewTabUtils._indexOfBookmarkSite(url) != -1) {
            continue;
          }

          let site = new Site(url, aResult.getCommentAt(count),
                                   aResult.getImageAt(count));
          aCallback(site);
          topSites.push(site);

          // Stop the search when we have enough sites.  
          if (topSites.length == NewTabUtils.MAX_TOP_SITES) {
            gHistoryACSearch.stopSearch();
            break;
          }
        }

        if (aResult.searchResult == Ci.nsIAutoCompleteResult.RESULT_SUCCESS ||
            topSites.length == NewTabUtils.MAX_TOP_SITES) {
          NewTabUtils._lastUpdated = Date.now();
          NewTabUtils._topSites = topSites;
        }
      },
      onUpdateSearchResult: function(aSearch, aResult) { }
    }
    // The "newtab-maxresults" flag causes the search to return up to 100 results.
    gHistoryACSearch.startSearch("", "newtab-maxresults", null, listener);
  },

  // adds a new site to _topSites
  // aCallback takes the new site as a param
  getNewSiteFromHistory: function NTU_getNewSiteFromHistory(aCallback) {
    let count = 0;
    let listener = {
      onSearchResult: function(aSearch, aResult) {
        for (count; count < aResult.matchCount; count++) {
          let url = aResult.getValueAt(count);
          // Don't add a site that's in the blacklist or already exists.
          if (NewTabUtils._blockedURLs[url] ||
              NewTabUtils._indexOfTopSite(url) != -1 ||
              NewTabUtils._indexOfBookmarkSite(url) != -1) {
            continue;
          }

          let site = new Site(url, aResult.getCommentAt(count),
                                   aResult.getImageAt(count));

          // Stop search after we've found a suitable new site
          gHistoryACSearch.stopSearch();
          aCallback(site);

          // Add the site to the end of the array
          NewTabUtils._topSites.push(site);
          break;
        }
      },
      onUpdateSearchResult: function(aSearch, aResult) { }
    }
    gHistoryACSearch.startSearch("", "newtab-maxresults", null, listener);  
  },

  // aSiteURL is the url of the site to delete from the _topSites array
  removeTopSite: function NTU_removeTopSite(aSiteURL) {
    let i = this._indexOfTopSite(aSiteURL);
    this._topSites.splice(i, 1);

    this._blockedURLs[aSiteURL] = true;
    this.saveBlockedURLs();
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
  
  /* ----- Favicon Color Methods ----- */

  getCachedFaviconColor: function NTU_getCachedFaviconColor(aFaviconImg) {
    return this._faviconColors[aFaviconImg.src];
  },

  getFaviconColor: function NTU_getFaviconColor(aFaviconImg, aDocument) {
    let canvas = aDocument.createElement("canvas");
    canvas.height = canvas.width = 16;

    let context = canvas.getContext("2d");
    context.drawImage(aFaviconImg, 0, 0);

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

    this._faviconColors[aFaviconImg.src] = faviconColor;
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
