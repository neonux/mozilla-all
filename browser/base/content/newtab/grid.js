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
      '<img class="img" width="' + THUMB_WIDTH +'" ' +
      ' height="' + THUMB_HEIGHT + '" alt=""/>' +
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
