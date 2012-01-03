#ifdef 0
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
#endif

/**
 * This class represents a site that is contained in a cell and can be pinned,
 * moved around or deleted.
 */
function Site(aNode, aLink) {
  this._node = aNode;
  this._node.__newtabSite = this;

  this._link = aLink;

  this._render();
  this._addEventHandlers();
}

Site.prototype = {
  /**
   * The site's DOM node.
   */
  get node() this._node,

  /**
   * The site's link.
   */
  get link() this._link,

  /**
   * The url of the site's link.
   */
  get url() this.link.url,

  /**
   * The title of the site's link.
   */
  get title() this.link.title,

  /**
   * The site's parent cell.
   */
  get cell() {
    let parentNode = this.node.parentNode;
    return parentNode && parentNode.__newtabCell;
  },

  /**
   * Pins the site on its current or a given index.
   * @param aIndex The pinned index (optional).
   */
  pin: function Site_pin(aIndex) {
    if (typeof aIndex == "undefined")
      aIndex = this.cell.index;

    this._updateAttributes(true);
    gPinnedLinks.pin(this._link, aIndex);
  },

  /**
   * Unpins the site and calls the given callback when done.
   * @param aCallback The callback to be called when finished.
   */
  unpin: function Site_unpin(aCallback) {
    if (this.isPinned()) {
      this._updateAttributes(false);
      gPinnedLinks.unpin(this._link);
      gUpdater.updateGrid(aCallback);
    }
  },

  /**
   * Checks whether this site is pinned.
   * @return Whether this site is pinned.
   */
  isPinned: function Site_isPinned() {
    return gPinnedLinks.isPinned(this._link);
  },

  /**
   * Blocks the site (removes it from the grid) and calls the given callback
   * when done.
   * @param aCallback The callback to be called when finished.
   */
  block: function Site_block(aCallback) {
    gBlockedLinks.block(this._link);
    gUpdater.updateGrid(aCallback);
    gPage.checkIfModified();
  },

  /**
   * Gets the DOM node specified by the given query selector.
   * @param aSelector The query selector.
   * @return The DOM node we found.
   */
  _querySelector: function Site_querySelector(aSelector) {
    return this.node.querySelector(aSelector);
  },

  // TODO
  _updateAttributes: function (aPinned) {
    let buttonPin = this._querySelector(".strip-button-pin");

    if (aPinned) {
      this.node.setAttribute("pinned", true);
      buttonPin.setAttribute("title", newTabString("unpin"));
    } else {
      this.node.removeAttribute("pinned");
      buttonPin.setAttribute("title", newTabString("pin"));
    }
  },

  /**
   * Renders the site's data (fills the HTML fragment).
   */
  _render: function Site_render() {
    this.node.setAttribute("href", this.url);
    this._querySelector(".site-title").textContent = this.title || this.url;

    if (this.isPinned())
      this._updateAttributes(true);

    this._renderThumbnail();
  },

  /**
   * Renders the site's thumbnail.
   */
  _renderThumbnail: function Site_renderThumbnail() {
    let img = this._querySelector(".site-img")
    img.setAttribute("alt", this.title);
    img.setAttribute("loading", "true");

    // Wait until the image has loaded.
    img.addEventListener("load", function onLoad() {
      img.removeEventListener("load", onLoad, false);
      img.removeAttribute("loading");
    }, false);

    // Set the thumbnail url.
    let uri = PageThumbs.getThumbnailForUrl(this.url, THUMB_WIDTH, THUMB_HEIGHT);
    img.setAttribute("src", uri);
  },

  /**
   * Adds event handlers for the site and its buttons.
   */
  _addEventHandlers: function Site_addEventHandlers() {
    // Register drag-and-drop event handlers.
    ["DragStart", /*"Drag",*/ "DragEnd"].forEach(function (aType) {
      let method = "_on" + aType;
      this[method] = this[method].bind(this);
      this._node.addEventListener(aType.toLowerCase(), this[method], false);
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

  /**
   * Event handler for the 'dragstart' event.
   * @param aEvent The drag event.
   */
  _onDragStart: function Site_onDragStart(aEvent) {
    gDrag.start(this, aEvent);
  },

  /**
   * Event handler for the 'drag' event.
   * @param aEvent The drag event.
  */
  _onDrag: function Site_onDrag(aEvent) {
    gDrag.drag(this, aEvent);
  },

  /**
   * Event handler for the 'dragend' event.
   * @param aEvent The drag event.
   */
  _onDragEnd: function Site_onDragEnd(aEvent) {
    gDrag.end(this, aEvent);
  }
};
