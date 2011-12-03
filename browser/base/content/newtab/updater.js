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
// Class: Updater
// This singleton provides functionality to update the current grid to a new
// set of pinned and blocked sites. It adds, moves and removes sites.
let Updater = {
  // ----------
  // Function: updateGrid
  // Updates the current grid according to its pinned and blocked sites.
  // This removes old, moves existing and creates new sites to fill gaps.
  //
  // Parameters:
  //   aCallback - to callback to call when finished
  updateGrid: function Updater_updateGrid(aCallback) {
    let links = Links.getLinks().slice(0, Grid.cells.length);

    // find all sites that remain in the grid ...
    let sites = this._findRemainingSites(links);

    // ... freeze these sites ...
    this._freezeSitePositions(sites);

    // ... and move them to their new DOM positions
    this._moveSiteNodes(sites);

    let self = this;

    // remove sites that are no longer in the grid
    this._removeLegacySites(sites, function () {
      // move remaining sites to their new positions
      self._rearrangeSites(sites, function () {
        // try to fill empty cells and finish
        self._fillEmptyCells(links, aCallback);

        // update other pages that might be open to keep them synced
        Pages.update(Page);
      });
    });
  },

  // ----------
  // Function: _findRemainingSites
  // Returns all sites that will remain in the grid (and may have changed
  // their positions).
  //
  // Parameters:
  //   aLinks - the array of links contained in the current grid
  _findRemainingSites: function Updater__findRemainingSites(aLinks) {
    let map = {};

    // create a map to easily retrieve the site for a given URL
    Grid.sites.forEach(function (aSite) {
      if (aSite)
        map[aSite.url] = aSite;
    });

    // map each link to its corresponding site, if any
    return aLinks.map(function (aLink) {
      return aLink && (aLink.url in map) && map[aLink.url];
    });
  },

  // ----------
  // Function: _freezeSitePositions
  // Freezes the given sites' positions.
  //
  // Parameters:
  //   aSites - the array of sites to freeze
  _freezeSitePositions: function Updater__freezeSitePositions(aSites) {
    aSites.forEach(function (aSite) {
      if (aSite)
        Transformation.freezeSitePosition(aSite);
    });
  },

  // ----------
  // Function: _moveSiteNodes
  // Moves the given sites' DOM nodes to their new positions.
  //
  // Parameters:
  //   aSites - the array of sites to move
  _moveSiteNodes: function Updater__moveSiteNodes(aSites) {
    let cells = Grid.cells;
    let sites = aSites.slice(0, cells.length);

    sites.forEach(function (aSite, aIndex) {
      let cell = cells[aIndex];
      let cellSite = cell.site;

      // the site's position didn't change
      if (!aSite || cellSite != aSite) {
        let cellNode = cell.node;

        // empty the cell if necessary
        if (cellSite)
          cellNode.removeChild(cellSite.node);

        // put the new site in place, if any
        if (aSite)
          cellNode.appendChild(aSite.node);
      }
    }, this);
  },

  // ----------
  // Function: _rearrangeSites
  // Rearranges the given sites and slides them to their new positions.
  //
  // Parameters:
  //   aSites - the array of sites to re-arrange
  //   aCallback - the callback to call when finished
  _rearrangeSites: function Updater__rearrangeSites(aSites, aCallback) {
    let options = {callback: aCallback, unfreeze: true};
    Transformation.rearrangeSites(aSites, options);
  },

  // ----------
  // Function: _removeLegacySites
  // Removes all sites from the grid that are not in the given links array or
  // exceed the grid.
  //
  // Parameters:
  //   aSites - the array of sites remaining in the grid
  //   aCallback - the callback to call when finished
  _removeLegacySites: function Updater__removeLegacySites(aSites, aCallback) {
    let batch = new Batch(aCallback);

    // delete sites that were removed from the grid
    Grid.sites.forEach(function (aSite) {
      // the site must valid and not in the current grid
      if (!aSite || -1 < aSites.indexOf(aSite))
        return;

      let node = aSite.node;

      batch.push();

      // fade out the to-be-removed site
      Animations.fadeOut(node, function () {
        // remove the site from the DOM
        node.parentNode.removeChild(node);
        batch.pop();
      });
    });

    batch.close();
  },

  // ----------
  // Function: _fillEmptyCells
  // Tries to fill empty cells with new links if available.
  //
  // Parameters:
  //   aLinks - the array of links
  //   aCallback - the callback to call when finished
  _fillEmptyCells: function Updater__fillEmptyCells(aLinks, aCallback) {
    let {cells, sites} = Grid;
    let batch = new Batch(aCallback);

    // find empty cells and fill them
    sites.forEach(function (aSite, aIndex) {
      if (aSite || !aLinks[aIndex])
        return;

      batch.push();

      // create the new site and fade it in
      let site = Grid.createSite(aLinks[aIndex], cells[aIndex]);
      Animations.fadeIn(site.node, function () batch.pop());
    });

    batch.close();
  }
};
