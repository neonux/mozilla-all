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

// ##########
// Class: Site
// This class represents a site that is contained in a cell and can be pinned,
// moved around or deleted.
function Site(aNode, aLink) {
  this._node = aNode;
  this._node.__newtabSite = this;

  this._link = aLink;

  this._render();
  this._addEventHandlers();
}

Site.prototype = {
  // the site's DOM node
  get node() this._node,

  // the site's link
  get link() this._link,

  // the url of the site's link
  get url() this.link.url,

  // the title of the site's link
  get title() this.link.title,

  // the site's parent cell
  get cell() this.node.parentNode && this.node.parentNode.__newtabCell,

  // ----------
  // Function: pin
  // Pins the site on it's current or a given index.
  //
  // Parameters:
  //   aIndex - the pinned index (optional)
  pin: function Site_pin(aIndex) {
    if (typeof aIndex == "undefined")
      aIndex = this.cell.index;

    this.node.classList.add("site-pinned");
    PinnedLinks.pin(this._link, aIndex);
  },

  // ----------
  // Function: unpin
  // Unpins the site and calls the given callback when done.
  //
  // Parameters:
  //   aCallback - the callback to be called when finished
  unpin: function Site_unpin(aCallback) {
    if (this.isPinned()) {
      this.node.classList.remove("site-pinned");
      PinnedLinks.unpin(this._link);
      Updater.updateGrid(aCallback);
    }
  },

  // ----------
  // Function: isPinned
  // Returns whether this site is pinned.
  isPinned: function Site_isPinned() {
    return PinnedLinks.isPinned(this._link);
  },

  // ----------
  // Function: block
  // Blocks the site (removes it from the grid) and calls the given callback
  // when done.
  //
  // Parameters:
  //   aCallback - the callback to be called when finished
  block: function Site_block(aCallback) {
    BlockedLinks.block(this._link);
    Updater.updateGrid(aCallback);
    Page.checkIfModified();
  },

  // ----------
  // Function: _querySelector
  // Returns the DOM node specified by the given query selector.
  //
  // Parameters:
  //   aSelector - the query selector
  _querySelector: function Site__querySelector(aSelector) {
    return this.node.querySelector(aSelector);
  },

  // ----------
  // Function: _render
  // Renders the site's data (fills the HTML fragment).
  _render: function Site__render() {
    this.node.setAttribute("href", this.url);
    this._querySelector(".site-title").textContent = this.title || this.url;

    if (this.isPinned())
      this.node.classList.add("site-pinned");

    this._renderThumbnail();
  },

  // ----------
  // Function: _renderThumbnail
  // Renders the site's thumbnail.
  _renderThumbnail: function Site__renderThumbnail() {
    let img = this._querySelector(".site-img")
    img.setAttribute("alt", this.title);

    let node = this.node;
    node.classList.add("site-loading");

    // wait until the image has loaded
    img.addEventListener("load", function onLoad() {
      img.removeEventListener("load", onLoad, false);
      node.classList.remove("site-loading");
    }, false);

    // set the thumbnail url
    let uri = PageThumbsUtils.getThumbnailUri(this.url, THUMB_WIDTH, THUMB_HEIGHT);
    img.setAttribute("src", uri.spec);
  },

  // ----------
  // Function: _addEventHandlers
  // Adds event handlers for the site and its buttons.
  _addEventHandlers: function Site__addEventHandlers() {
    // register drag-and-drop event handlers
    ["dragstart", /*"drag",*/ "dragend"].forEach(function (aType) {
      let method = "_" + aType;
      this[method] = this[method].bind(this);
      this._node.addEventListener(aType, this[method], false);
    }, this);

    let self = this;

    function pin(aEvent) {
      if (aEvent)
        aEvent.preventDefault();

      if (self.isPinned())
        self.unpin();
      else
        self.pin();
    }

    function block(aEvent) {
      if (aEvent)
        aEvent.preventDefault();

      self.block();
    }

    this._querySelector(".strip-button-pin").addEventListener("click", pin, false);
    this._querySelector(".strip-button-block").addEventListener("click", block, false);
  },

  // ----------
  // Function: _dragstart
  // Event handler for the 'dragstart' event.
  //
  // Parameters:
  //   aEvent - the drag event
  _dragstart: function Site__dragstart(aEvent) {
    Drag.start(this, aEvent);
  },

  // ----------
  // Function: _drag
  // Event handler for the 'drag' event.
  //
  // Parameters:
  //   aEvent - the drag event
  _drag: function Site__drag(aEvent) {
    Drag.drag(this, aEvent);
  },

  // ----------
  // Function: _dragend
  // Event handler for the 'dragend' event.
  //
  // Parameters:
  //   aEvent - the drag event
  _dragend: function Site__dragend(aEvent) {
    Drag.end(this, aEvent);
  }
};
