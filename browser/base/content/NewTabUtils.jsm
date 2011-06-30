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

XPCOMUtils.defineLazyServiceGetter(this, "gHistoryACSearch",
                                   "@mozilla.org/autocomplete/search;1?name=history",
                                   "nsIAutoCompleteSearch");

XPCOMUtils.defineLazyGetter(this, "FileUtils", function() {
  Cu.import("resource://gre/modules/FileUtils.jsm");
  return FileUtils;
});

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

function Site(url, title, favicon) {
  this.url = url;
  this.host = Services.io.newURI(url, null, null).host;
  this.title = title;
  this.favicon = favicon;
}

let NewTabUtils = {

  SITES_FILE: "newtab/sitesArray.json",
  BLOCKED_SITES_FILE: "newtab/blockedSites.json",

  // ordered list of site objects
  _sites: [],
  _lastUpdated: 0,

  // mapping of url -> boolean
  _blockedSites: {},
  _blockedSitesLoaded: false,

  _obsInitialized: false,

  _faviconColors: {},

  // pay attention to changes in browser history, specifically deleting/forgetting sites
  // TODO: clean up blockedSites when a site is forgotten 
  initObservers: function NTU_initObservers() {
    // Invalidate the cache if the user explicitly deletes sites.
    let observer = {
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

    let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
             getService(Ci.nsINavHistoryService);
    hs.addObserver(observer, false);

    this._obsInitialized = true;
  },

  // returns a site and that site's index in _sites
  getSite: function NTU_getSite(aSiteURL) {
    for (let i = 0; i < this._sites.length; i++) {
      let site = this._sites[i];
      if (site.url == aSiteURL) {
        return { "site": site, "index": i };
      }
    }
    // If the site isn't found, just return null
    return null;
  },

  // aCallback takes a site object as a param
  getSites: function NTU_getSites(aCallback) {
    if (!this._obsInitialized) {
      this.initObservers();
    }

    // Use sites stored in memory if we have them
    if (this._sites.length) {
      // Refresh sites if it's been more than a day since we updated them
      /* Disabled until we decide how to preserve re-ordered sites
      if (this._lastUpdated + 86400000 < Date.now()) {
        this.refreshSites(aCallback);
        return;
      } */   

      // Otherwise, just use the sites we got
      for (let i = 0; i < this._sites.length; i++) {
        aCallback(this._sites[i]);
      }
      return;
    }

    // Otherwise load sites from disk
    this.loadSites(aCallback);
  },

  // aCallback takes a site object as a param
  loadSites: function NTU_loadSites(aCallback) {
    this.loadFromFile(this.SITES_FILE, function(sites) {
      if (sites && sites.length) {
        for (let i = 0; i < sites.length; i++) {
          aCallback(sites[i]);
        }
        NewTabUtils._sites = sites;

        // If there were saved sites, there may be blocked sites as well
        NewTabUtils.loadBlockedSites();
        return;
      }

      // If we don't have any sites stored on disk, just get them all from history AC    
      NewTabUtils.getSitesFromHistory(aCallback);
    });
  },

  loadBlockedSites: function NTU_loadBlockedSites() {
    this.loadFromFile(this.BLOCKED_SITES_FILE, function(blockedSites) {
      if (blockedSites) {
        NewTabUtils._blockedSites = blockedSites;
      }
      NewTabUtils._blockedSitesLoaded = true;
    });  
  },

  // Get top sites from places autocomplete
  // aCallback takes a site object as a param
  getSitesFromHistory: function NTU_getSitesFromHistory(aCallback) {
    let resultCount = 0;
    // Use temporary sites array, in case other methods try to access _sites while results are coming back
    let sites = [];
    let listener = {
      onSearchResult: function(aSearch, aResult) {        
        for (resultCount; resultCount < aResult.matchCount; resultCount++) {
          let url = aResult.getValueAt(resultCount);
          let site = new Site(aResult.getValueAt(resultCount),
                              aResult.getCommentAt(resultCount),
                              aResult.getImageAt(resultCount));
          aCallback(site);
          sites.push(site);
        }

        if (aResult.searchResult == Ci.nsIAutoCompleteResult.RESULT_SUCCESS) {
          NewTabUtils._lastUpdated = Date.now();
          NewTabUtils._sites = sites;
          NewTabUtils.saveSites();
        }
      },
      onUpdateSearchResult: function(aSearch, aResult) { }
    }
    gHistoryACSearch.startSearch("", "", null, listener);
  },

  // aCallback takes a site object as a param
  refreshSites: function NTU_refreshSites(aCallback) {
    // Use temporary sites array, in case other methods try to access _sites while results are coming back
    let sites = [];
    let siteUrls = {};
    // Start by adding pinned sites
    for (let i = 0; i < this._sites.length; i++) {
      let site = this._sites[i];
      if (site.pinned) {
        sites.push(site);
        // Keep track of which sites we're using up
        siteUrls[site.url] = true;
      } else {
        // Add a placeholder to the array that will be filled in later
        sites.push(null);
      }
    }
    
    // Go fetch new sites to fill up the extra spots if there are some
    let siteCount = 0;
    let resultCount = 0;
    let listener = {
      onSearchResult: function(aSearch, aResult) {
        while (resultCount < aResult.matchCount && siteCount < 12) {
          // Check to see if there's already a site in this slot
          let site = sites[siteCount];
          if (!site) {
            // If there isn't a site, use the next autocomplete result
            let url = aResult.getValueAt(resultCount);
            // If the next result site is already in the sites array or it's in
            // the blacklist, move onto the next result and try again
            if (siteUrls[url] || NewTabUtils._blockedSites[url]) {
              resultCount++;
              continue;
            }
            // Fill in the slot with this site
            site = new Site(url, aResult.getCommentAt(resultCount),
                                 aResult.getImageAt(resultCount));
            sites[siteCount] = site;
            resultCount++;
          }
          aCallback(site);
          // Move onto the next slot in the sites array
          siteCount++;
        }
  
        // Stop when we have 12 sites or when we've run out of sites
        if (siteCount > 11 || aResult.searchResult == Ci.nsIAutoCompleteResult.RESULT_SUCCESS) {
          gHistoryACSearch.stopSearch();
          NewTabUtils._lastUpdated = Date.now();
          NewTabUtils._sites = sites;
          NewTabUtils.saveSites();
        }
      },
      onUpdateSearchResult: function(aSearch, aResult) { }
    }
    gHistoryACSearch.startSearch("", "newtab-maxresults", null, listener);
  },

  // adds a new site to _sites
  // aCallback takes the added site as a param
  findNewSite: function NTU_findNewSite(aCallback) {
    let resultCount = 0;
    let listener = {
      onSearchResult: function(aSearch, aResult) {
        for (resultCount; resultCount < aResult.matchCount; resultCount++) {
          let url = aResult.getValueAt(resultCount);
          // don't add site that already exists or is in blacklist
          if (NewTabUtils.getSite(url) || NewTabUtils._blockedSites[url]) {
            continue;
          }

          let site = new Site(url, aResult.getCommentAt(resultCount),
                                   aResult.getImageAt(resultCount));

          // Stop search after we've found a suitable new site
          gHistoryACSearch.stopSearch();
          aCallback(site);

          // Add the site to the end of the array
          NewTabUtils._sites.push(site);
          NewTabUtils.saveSites();
          return;
        }
      },
      onUpdateSearchResult: function(aSearch, aResult) { }
    }
    gHistoryACSearch.startSearch("", "newtab-maxresults", null, listener);  
  },

  // aSiteURL is the url of the site to delete from the site array
  removeSite: function NTU_removeSite(aSiteURL) {
    let siteIndex = this.getSite(aSiteURL).index;
    this._sites.splice(siteIndex, 1);
    this.saveSites();

    this._blockedSites[aSiteURL] = true;
    this.saveBlockedSites();
  },

  // aSiteURL is the url of the site to save from the site array
  updatePinnedState: function NTU_updatePinnedState(aSiteURL, pinned) {
    let site = this.getSite(aSiteURL).site;
    if (pinned) {
      site.pinned = true;
    } else {
      delete site.pinned;
    }
    this.saveSites();
  },

  moveDroppedSite: function NTU_moveSite(aDropSiteURL, aTargetSiteURL) {
    let droppedSite = this.getSite(aDropSiteURL);

    // Remove dropped site from its original spot
    this._sites.splice(droppedSite.index, 1);

    // Insert the dropped site before the target item
    let targetIndex = this.getSite(aTargetSiteURL).index;
    this._sites.splice(targetIndex, 0, droppedSite.site);

    // Once you move a site, don't edit any sites beyond that one

    this.saveSites();
  },

  /* ----- File I/O Helper Methods ----- */

  // aCallback takes JSON object made from file contents as a parameter
  // object will be null if the file does not exist
  loadFromFile: function NTU_loadFromFile(aFile, aCallback) {
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

  saveSites: function NTU_saveSites() {
    this.saveToFile(this.SITES_FILE, this._sites);
  },

  saveBlockedSites: function NTU_saveBlockedSites() {
    this.saveToFile(this.BLOCKED_SITES_FILE, this._blockedSites);
  },

  saveToFile: function NTU_saveToFile(aFile, aObj) {
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
