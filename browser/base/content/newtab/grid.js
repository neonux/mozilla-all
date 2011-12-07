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

// ##########
// Class: Grid
// This singleton represents the grid that contains all sites.
let Grid = {
  // the DOM node of the grid
  _node: null,
  get node() this._node,

  // the cached DOM fragment for sites
  _siteFragment: null,

  // all cells contained in the grid
  get cells() {
    let children = this.node.querySelectorAll("li");
    let cells = [new Cell(child) for each (child in children)];

    // replace the getter with our cached value
    this.__defineGetter__("cells", function Grid_getCells() cells);
    return cells;
  },

  // all sites contained in the grid's cells. sites may be empty
  get sites() [cell.site for each (cell in this.cells)],

  // ----------
  // Function: init
  // Initializes the grid.
  //
  // Parameters:
  //   aSelector - the query selector of the grid
  init: function Grid_init(aSelector) {
    this._node = document.querySelector(aSelector);
    this._createSiteFragment();
    this._draw();
  },

  // ----------
  // Function: createSite
  // Returns a new site created in the grid.
  //
  // Parameters:
  //   aLink - the new site's link
  //   aCell - the new site's parent cell
  createSite: function Grid_createSite(aLink, aCell) {
    let node = aCell.node;
    node.appendChild(this._siteFragment.cloneNode(true));
    return new Site(node.firstElementChild, aLink);
  },

  // ----------
  // Function: refresh
  // Refreshes the grid and re-creates all sites.
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

  // ----------
  // Function: lock
  // Locks the grid to block all pointer events.
  lock: function Grid_lock() {
    this.node.classList.add("grid-locked");
  },

  // ----------
  // Function: unlock
  // Unlocks the grid to allow all pointer events.
  unlock: function Grid_unlock() {
    this.node.classList.remove("grid-locked");
  },

  // ----------
  // Function: _createSiteFragment
  // Creates the DOM fragment that is re-used when creating sites.
  _createSiteFragment: function Grid__createSiteFragment() {
    let site = document.createElement("a");
    site.classList.add("site");
    site.setAttribute("draggable", "true");

    // create the site's inner HTML code
    site.innerHTML =
      '<img class="site-img" width="' + THUMB_WIDTH +'" ' +
      ' height="' + THUMB_HEIGHT + '" alt=""/>' +
      '<span class="site-title"/>' +
      '<span class="site-strip">' +
      '  <input class="button strip-button strip-button-pin" type="button"/>' +
      '  <input class="button strip-button strip-button-block" type="button"/>' +
      '</span>';

    let fragment = this._siteFragment = document.createDocumentFragment();
    fragment.appendChild(site);
  },

  // ----------
  // Function: _draw
  // Draws the grid, creates all sites and puts them into their cells.
  _draw: function Grid__draw() {
    let self = this;
    let cells = this.cells;

    // put sites into the cells
    let links = Links.getLinks();
    let length = Math.min(links.length, cells.length);

    for (let i = 0; i < length; i++) {
      if (links[i])
        self.createSite(links[i], cells[i]);
    }
  }
};
