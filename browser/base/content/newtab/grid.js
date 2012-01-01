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
 * This singleton represents the grid that contains all sites.
 */
let gGrid = {
  /**
   * The DOM node of the grid.
   */
  _node: null,
  get node() this._node,

  /**
   * The cached DOM fragment for sites.
   */
  _siteFragment: null,

  /**
   * All cells contained in the grid.
   */
  get cells() {
    let children = this.node.querySelectorAll("li");
    let cells = [new Cell(child) for each (child in children)];

    // Replace the getter with our cached value.
    Object.defineProperty(this, "cells", {value: cells, enumerable: true});

    return cells;
  },

  /**
   * All sites contained in the grid's cells. Sites may be empty.
   */
  get sites() [cell.site for each (cell in this.cells)],

  /**
   * Initializes the grid.
   * @param aSelector The query selector of the grid.
   */
  init: function Grid_init(aSelector) {
    this._node = document.querySelector(aSelector);
    this._createSiteFragment();
    this._draw();
  },

  /**
   * Creates a new site in the grid.
   * @param aLink The new site's link.
   * @param aCell The new site's parent cell.
   * @return The newly created site.
   */
  createSite: function Grid_createSite(aLink, aCell) {
    let node = aCell.node;
    node.appendChild(this._siteFragment.cloneNode(true));
    return new Site(node.firstElementChild, aLink);
  },

  /**
   * Refreshes the grid and re-creates all sites.
   */
  refresh: function Grid_refresh() {
    // Remove all sites.
    this.cells.forEach(function (cell) {
      let node = cell.node;
      let child = node.firstElementChild;

      if (child)
        node.removeChild(child);
    }, this);

    // Draw the grid again.
    this._draw();
  },

  /**
   * Locks the grid to block all pointer events.
   */
  lock: function Grid_lock() {
    this.node.setAttribute("locked", "true");
  },

  /**
   * Unlocks the grid to allow all pointer events.
   */
  unlock: function Grid_unlock() {
    this.node.removeAttribute("locked");
  },

  /**
   * Creates the DOM fragment that is re-used when creating sites.
   */
  _createSiteFragment: function Grid_createSiteFragment() {
    let site = document.createElementNS(HTML_NAMESPACE, "a");
    site.classList.add("site");
    site.setAttribute("draggable", "true");

    // Create the site's inner HTML code.
    site.innerHTML =
      '<img class="site-img" width="' + THUMB_WIDTH +'" ' +
      ' height="' + THUMB_HEIGHT + '" alt=""/>' +
      '<span class="site-title"/>' +
      '<span class="site-strip">' +
      '  <input class="button strip-button strip-button-pin" type="button"/>' +
      '  <input class="button strip-button strip-button-block" type="button"/>' +
      '</span>';

    this._siteFragment = document.createDocumentFragment();
    this._siteFragment.appendChild(site);
  },

  /**
   * Draws the grid, creates all sites and puts them into their cells.
   */
  _draw: function Grid_draw() {
    let cells = this.cells;

    // Put sites into the cells.
    let links = gLinks.getLinks();
    let length = Math.min(links.length, cells.length);

    for (let i = 0; i < length; i++) {
      if (links[i])
        this.createSite(links[i], cells[i]);
    }
  }
};
