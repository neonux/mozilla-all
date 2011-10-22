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

let Cu = Components.utils;
let Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Geometry.jsm");
Cu.import("resource:///modules/NewTabUtils.jsm");

const PREF_NEWTAB_ENABLED = "browser.newtab.enabled";

#include animations.js
#include transformations.js

// TODO
function debug(aMsg) {
  aMsg = ("NewTabPage: " + aMsg).replace(/\S{80}/g, "$&\n");
  Services.console.logStringMessage(aMsg);
}

// TODO
function trace(aMsg) {
  // cut off the first line of the stack trace, because that's just this function.
  let stack = Error().stack.split("\n").slice(1);

  debug("trace: " + aMsg + "\n" + stack.join("\n"));
}

// TODO
function extend(aParent, aObject) {
  for (let name in aParent) {
    let getter, setter;

    if (getter = aParent.__lookupGetter__(name))
      aObject.__defineGetter__(name, getter);
    else if (setter = aParent.__lookupSetter__(name))
      aObject.__defineSetter__(name, setter);
    else
     aObject[name] = aParent[name];
  }

  return aObject;
}

// TODO
let Page = {
  init: function Page_init(aToolbarSelector, aGridSelector) {
    Toolbar.init(aToolbarSelector);
    this._gridSelector = aGridSelector;

    // check if the new tab feature is enabled
    if (this.isEnabled())
      this._init();
    else
      document.body.classList.add("disabled");

    // register pref observer
    Services.prefs.addObserver(PREF_NEWTAB_ENABLED, this, true);
  },

  lock: function Page_lock() {
    document.body.classList.add("locked");
  },

  unlock: function Page_unlock() {
    document.body.classList.remove("locked");
  },

  isLocked: function Page_isLocked() {
    return document.body.classList.contains("locked");
  },

  isEnabled: function Page_isEnabled() {
    return Services.prefs.getBoolPref(PREF_NEWTAB_ENABLED, true);
  },

  modify: function Page_modify() {
    document.body.classList.add("modified");
  },

  reset: function Page_reset() {
    document.body.classList.remove("modified");
  },

  isModified: function Page_isModified() {
    let blockedLinks = NewTabUtils.Storage.get("blockedLinks", {});
    return Object.keys(blockedLinks).length > 0;
  },

  refresh: function Page_refresh() {
    Links.reset();
    Grid.refresh();
  },

  observe: function Page_observe() {
    let classes = document.body.classList;

    if (this.isEnabled()) {
      classes.remove("disabled");
      this._init();
    } else {
      classes.add("disabled");
    }
  },

  _init: function Page__init() {
    if (this._initialized)
      return;

    this._initialized = true;

    NewTabUtils.Pages.register(this);

    let self = this;

    // listen for 'unload' to unregister this page
    addEventListener("unload", function () {
      NewTabUtils.Pages.unregister(self);
    }, false);

    // check if the grid has been modified
    if (this.isModified())
      this.modify();

    Grid.init(this._gridSelector);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

// TODO
let Toolbar = {
  init: function Toolbar_init(aSelector) {
    this._node = document.querySelector(aSelector);

    let btnShow = this._node.firstElementChild;
    let btnHide = btnShow.nextElementSibling;
    let btnRefresh = btnHide.nextElementSibling;

    btnShow.addEventListener("click", this._show.bind(this), false);
    btnHide.addEventListener("click", this._hide.bind(this), false);
    btnRefresh.addEventListener("click", this._refresh.bind(this), false);
  },

  _show: function Toolbar__show() {
    Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, true);
  },

  _hide: function Toolbar__hide() {
    Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, false);
  },

  _refresh: function Toolbar__refresh() {
    Grid.reset();
  }
};

// TODO
let Grid = {
  get cells() {
    let cells = [];
    let children = this._node.querySelectorAll("li");
    let numChildren = children.length;

    // create the cells
    for (let i = 0; i < numChildren; i++)
      cells.push(new Cell(children[i]));

    // replace the getter with our cached value
    this.__defineGetter__("cells", function Grid_getCells() cells);
    return cells;
  },

  get _siteFragment() {
    let site = document.createElement("a");
    site.classList.add("site");
    site.setAttribute("draggable", "true");

    site.innerHTML =
      '<img class="img" width="201" height="127" ' +
      ' src="chrome://browser/content/newtab/thumb.png" alt=""/>' +
      '<span class="title"/>' +
      '<span class="strip">' +
      '  <input class="btn-pin" type="button"/>' +
      '  <input class="btn-block" type="button"/>' +
      '</span>';

    let fragment = document.createDocumentFragment();
    fragment.appendChild(site);

    // replace the getter with our cached value
    let getter = function Grid__getSiteFragment() fragment;
    this.__defineGetter__("_siteFragment", getter);
    return fragment;
  },

  init: function Grid_init(aSelector) {
    this._node = document.querySelector(aSelector);
    this._draw();
  },

  createSite: function Grid_createSite(aLink, aCell) {
    let node = aCell.node;
    node.appendChild(this._siteFragment.cloneNode(true));
    return new Site(node.firstElementChild, aLink);
  },

  reset: function Grid_reset() {
    let node = this._node;

    Animations.fadeOut(node, function () {
      NewTabUtils.Storage.clear();
      NewTabUtils.Pages.reset();
      NewTabUtils.Pages.refresh(Page);
      Page.refresh();

      Animations.fadeIn(node);
    });
  },

  refresh: function Grid_refresh() {
    // remove all sites
    this.cells.forEach(function (cell) {
      let node = cell.node;
      let child = node.firstElementChild;

      if (child)
        node.removeChild(child);
    });

    // draw the grid again
    this._draw();
  },

  _draw: function Grid__draw() {
    let self = this;
    let cells = this.cells;

    // put sites into the cells
    Links.getLinks(function (links) {
      let maxIndex = Math.min(links.length, cells.length);

      for (let i = 0; i < maxIndex; i++)
        if (links[i])
          self.createSite(links[i], cells[i]);
    });
  }
};

// TODO
function Cell(aNode) {
  this._node = aNode;
  this._node.__newtabCell = this;
}

Cell.prototype = {
  get node() this._node,
  get index() Grid.cells.indexOf(this),

  get nextSibling() {
    let next = this.node.nextElementSibling;
    return next && next.__newtabCell;
  },

  get site() {
    let firstChild = this.node.firstElementChild;
    return firstChild && firstChild.__newtabSite;
  },

  containsPinnedSite: function Cell_containsPinnedSite() {
    let site = this.site;
    return site && site.isPinned();
  },

  isEmpty: function Cell_isEmpty() {
    return !this.site;
  }
};

// TODO
function Site(aNode, aLink) {
  this._node = aNode;
  this._node.__newtabSite = this;

  this._link = aLink;

  this._fillNode();
  this._connectButtons();

  this._dragHandler = new DragHandler(this);
}

Site.prototype = {
  get node() this._node,
  get index() this.cell.index,
  get url() this._link.url,
  get title() this._link.title,
  get cellNode() this.cell.node,
  get cell() this.node.parentNode.__newtabCell,

  pin: function Site_pin() {
    if (this.isPinned())
      return;

    this.node.classList.add("pinned");
    this._link.pin(this.index);
  },

  unpin: function Site_unpin() {
    if (!this.isPinned())
      return;

    this.node.classList.remove("pinned");
    this._link.unpin();
  },

  isPinned: function Site_isPinned() {
    return this._link.isPinned();
  },

  block: function Site_block(aEvent) {
    this._link.block();
  },

  _querySelector: function (aSelector) {
    return this.node.querySelector(aSelector);
  },

  _fillNode: function Site__fillNode() {
    let {url, title} = this;

    this.node.setAttribute("href", url);
    this._querySelector(".img").setAttribute("alt", title);
    this._querySelector(".title").textContent = title;

    if (this.isPinned())
      this.node.classList.add("pinned");
  },

  _connectButtons: function Site__connectButtons() {
    let self = this;

    this._querySelector(".btn-pin").addEventListener("click", function (aEvent) {
      if (aEvent)
        aEvent.preventDefault();

      if (self.isPinned()) {
        self.unpin();
        Transformation.unpinSite(self);
      } else {
        self.pin();
      }
    }, false);

    this._querySelector(".btn-block").addEventListener("click", function (aEvent) {
      if (aEvent)
        aEvent.preventDefault();

      if (!Page.isModified())
        NewTabUtils.Pages.modify();

      self.block();
      Transformation.blockSite(self);
    }, false);
  }
};

// TODO
function Link(aUrl, aTitle) {
  this._url = aUrl;
  this._title = aTitle;
}

Link.unserialize = function (aData) {
  return new Link(aData.url, aData.title);
};

Link.prototype = {
  get url() this._url,
  get title() this._title || this._url,

  block: function Link_block() {
    Links.block(this);
  },

  pin: function Link_pin(aIndex) {
    Links.pin(this, aIndex);
  },

  unpin: function Link_unpin() {
    Links.unpin(this);
  },

  isPinned: function Link_isPinned() {
    return Links.isPinned(this);
  },

  serialize: function Link_serialize() {
    return {url: this.url, title: this.title};
  }
};

// TODO
let Links = {
  getLinks: function Links_getLinks(aCallback) {
    let pinnedLinks = this._pinnedLinks.concat();
    let blockedLinks = this._blockedLinks;

    NewTabUtils.Links.fetchLinks(function (links) {
      // create Link objects
      links = links.map(function (link) {
        return new Link(link.url, link.title);
      });

      // filter blocked and pinned links
      links = links.filter(function (link) {
        return !(link.url in blockedLinks) && !link.isPinned();
      });

      // unserialize all pinned links
      for (let i = 0; i < pinnedLinks.length; i++)
        if (pinnedLinks[i])
          pinnedLinks[i] = Link.unserialize(pinnedLinks[i]);

      // try to fill the gaps between pinned links
      for (let i = 0; i < pinnedLinks.length && links.length; i++)
        if (!pinnedLinks[i])
          pinnedLinks[i] = links.shift();

      // append the remaining links if any
      if (links.length)
        pinnedLinks = pinnedLinks.concat(links);

      aCallback(pinnedLinks);
    });
  },

  block: function Links_block(aLink) {
    this._blockedLinks[aLink.url] = 1;
    NewTabUtils.Storage.set("blockedLinks", this._blockedLinks);

    // make sure we unpin blocked links
    if (!this.unpin(aLink))
      NewTabUtils.Pages.refresh(Page);
  },

  pin: function Links_pin(aLink, aIndex) {
    this._pinnedLinks[aIndex] = aLink.serialize();
    NewTabUtils.Storage.set("pinnedLinks", this._pinnedLinks);
    NewTabUtils.Pages.refresh(Page);
  },

  unpin: function Links_unpin(aLink) {
    let index = this._pinnedIndexOf(aLink);

    if (index > -1) {
      this._pinnedLinks[index] = null;
      NewTabUtils.Storage.set("pinnedLinks", this._pinnedLinks);
      NewTabUtils.Pages.refresh(Page);

      return true;
    }

    return false;
  },

  isPinned: function Links_isPinned(aLink) {
    return this._pinnedIndexOf(aLink) > -1;
  },

  reset: function Links_reset() {
    this._initStorageValues();
  },

  _init: function Links__init() {
    this._initStorageValues();
  },

  _initStorageValues: function Links__initStorageValues() {
    this._pinnedLinks = NewTabUtils.Storage.get("pinnedLinks", []);
    this._blockedLinks = NewTabUtils.Storage.get("blockedLinks", {});
  },

  _pinnedIndexOf: function Links__pinnedIndexOf(aLink) {
    let pinned = this._pinnedLinks;
    let url = aLink.url;

    for (let i = 0; i < pinned.length; i++)
      if (pinned[i] && pinned[i].url == url)
        return i;

    return -1;
  }
};

Links._init();

// TODO
function DragHandler(aSite) {
  this._site = aSite;
  this._node = this._site.node;

  // listen for 'dragstart'
  this._dragstart = this._dragstart.bind(this);
  this._node.addEventListener("dragstart", this._dragstart.bind(this), false);
}

DragHandler.prototype = {
  _dragstart: function DragHandler__dragstart(aEvent) {
    let node = this._node;

    // TODO
    node.classList.add("ontop");
    document.body.classList.add("dragging");

    let self = this;

    Transformation.dragSite(this._site, aEvent, function () {
      node.classList.remove("ontop");
      document.body.classList.remove("dragging");
      document.body.removeChild(self._dragImage);
    });

    this._setDragData(aEvent);

    return true;
  },

  _setDragData: function DragHandler__setDragData(aEvent) {
    let {url, title} = this._site;

    let dt = aEvent.dataTransfer;
    dt.setData("text/plain", url);
    dt.setData("text/uri-list", url);
    dt.setData("text/x-moz-url", url + "\n" + title);
    dt.setData("text/html", "<a href=\"" + url + "\">" + url + "</a>");

    // create and use an empty drag image
    let img = this._dragImage = document.createElement("div");
    img.classList.add("drag-img");
    document.body.appendChild(img);
    dt.setDragImage(img, 0, 0);

    dt.effectAllowed = "move";
  }
};
