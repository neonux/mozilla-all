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

XPCOMUtils.defineLazyServiceGetter(this, "gHistoryACSearch",
  "@mozilla.org/autocomplete/search;1?name=history", "nsIAutoCompleteSearch");

// TODO
let NewTabUtils = {};

// TODO
NewTabUtils.Storage = {
  get _storage() {
    let uri = Services.io.newURI("about:newtab", null, null);
    let principal = Cc["@mozilla.org/scriptsecuritymanager;1"]
                    .getService(Ci.nsIScriptSecurityManager)
                    .getCodebasePrincipal(uri);

    let dsm = Cc["@mozilla.org/dom/storagemanager;1"]
              .getService(Ci.nsIDOMStorageManager);

    delete this._storage;
    this._storage = dsm.getLocalStorageForPrincipal(principal, "");
  },

  get: function Storage_get(aName, aDefault) {
    let value;

    try {
      value = JSON.parse(this._storage.getItem(aName));
    } catch (e) {}

    return value || aDefault;
  },

  set: function Storage_set(aName, aValue) {
    this._storage.setItem(aName, JSON.stringify(aValue));
  },

  clear: function Storage_clear() {
    this._storage.clear();
  }
};

// TODO
NewTabUtils.Pages = {
  _pages: [],

  register: function NTU_Pages_register(aPage) {
    this._pages.push(aPage);
  },

  unregister: function NTU_Pages_unregister(aPage) {
    let index = this._pages.indexOf(aPage);
    this._pages.splice(index, 1);
  },

  lock: function NTU_Pages_lock() {
    this._pages.forEach(function (page) page.lock());
  },

  unlock: function NTU_Pages_lock() {
    this._pages.forEach(function (page) page.unlock());
  },

  modify: function NTU_Pages_modify() {
    this._pages.forEach(function (page) page.modify());
  },

  reset: function NTU_Pages_reset() {
    this._pages.forEach(function (page) page.reset());
  }
};

// TODO
NewTabUtils.Links = {
  _fetchCallbacks: [],

  fetchLinks: function NTU_Links_fetchLinks(aCallback) {
    // we fetched them before, return the cached value
    if (this._links) {
      aCallback(this._links);
      return;
    }

    // TODO
    this._fetchCallbacks.push(aCallback);

    // TODO
    if (this._fetchCallbacks.length > 1)
      return;

    let self = this;

    let listener = {
      onSearchResult: function (aSearch, aResult) {
        // TODO: add error handling
        if (aResult.searchResult != Ci.nsIAutoCompleteResult.RESULT_SUCCESS)
          return;

        let links = self._links = [];

        for (let i = 0; i < aResult.matchCount; i++) {
          let url = aResult.getValueAt(i);
          let title = aResult.getCommentAt(i);
          links.push({url: url, title: title});
        }

        self._callFetchCallbacks(links);
      },

      onUpdateSearchResult: function (aSearch, aResult) {}
    };

    gHistoryACSearch.startSearch("", "newtab-maxresults", null, listener);
  },

  _callFetchCallbacks: function Sites__callFetchCallbacks(aLinks) {
    this._fetchCallbacks.forEach(function (callback) {
      callback(aLinks);
    });

    this._fetchCallbacks = [];
  }
};

// TODO
NewTabUtils.Testing = {
  mockLinks: function NTU_Testing_mockLinks(aLinks) {
    NewTabUtils.Links._links = aLinks;
  },

  blockSite: function NTU_Testing_blockSite(aSite, aCallback) {
    aSite.block();

    let node = aSite.node;
    let window = node.ownerDocument.defaultView;
    window.Transformation.blockSite(aSite);

    this._waitForUnlock(node, aCallback);
  },

  unpinSite: function NTU_Testing_blockSite(aSite, aCallback) {
    aSite.unpin();

    let node = aSite.node;
    let window = node.ownerDocument.defaultView;
    window.Transformation.unpinSite(aSite);

    this._waitForUnlock(node, aCallback);
  },

  reset: function NTU_Testing_reset() {
    NewTabUtils.Storage.clear();

    NewTabUtils.Links._links = null;
    NewTabUtils.Links._fetchCallbacks = [];
  },

  _waitForUnlock: function NTU_Testing__waitForUnlock(aNode, aCallback) {
    let doc = aNode.ownerDocument;
    let ntp = doc.defaultView.Page;
    let body = doc.body;

    body.addEventListener("DOMAttrModified", function onDOMAttrModified(event) {
      if (ntp.isLocked())
        return;

      body.removeEventListener("DOMAttrModified", onDOMAttrModified, false);
      aCallback();
    }, false);
  }
};
