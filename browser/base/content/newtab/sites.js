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

    let node = this.node;
    node.setAttribute("href", url);
    node.classList.add("loading");

    this._querySelector(".title").textContent = title;

    let img = this._querySelector(".img")
    img.setAttribute("alt", title);

    // wait until the image has loaded
    img.addEventListener("load", function onLoad() {
      img.removeEventListener("load", onLoad, false);
      node.classList.remove("loading");
    }, false);

    let thumbUri = ThumbnailUtils.getThumbnailUri(url, THUMB_WIDTH, THUMB_HEIGHT);
    img.setAttribute("src", thumbUri.spec);

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
