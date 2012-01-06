#ifdef 0
/*
 * This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/.
 */
#endif

/**
 * This singleton provides functionality to update the current grid to a new
 * set of pinned and blocked sites. It adds, moves and removes sites.
 */
let gUpdater = {
  /**
   * Updates the current grid according to its pinned and blocked sites.
   * This removes old, moves existing and creates new sites to fill gaps.
   * @param aCallback The callback to call when finished.
   */
  updateGrid: function Updater_updateGrid(aCallback) {
    let links = gLinks.getLinks().slice(0, gGrid.cells.length);

    // Find all sites that remain in the grid.
    let sites = this._findRemainingSites(links);

    let self = this;

    // Remove sites that are no longer in the grid.
    this._removeLegacySites(sites, function () {
      // Freeze all site positions.
      self._freezeSitePositions(sites);

      // Move all remaining sites to their new DOM positions.
      self._moveSiteNodes(sites);

      // Move remaining sites to their new positions.
      self._rearrangeSites(sites, function () {
        // Try to fill empty cells and finish.
        self._fillEmptyCells(links, aCallback);

        // Update other pages that might be open to keep them synced.
        gAllPages.update(gPage);
      });
    });
  },

  /**
   * Finds all sites that will remain in the grid (and may have changed
   * their positions).
   * @param aLinks The array of links contained in the current grid.
   * @return The remaining sites.
   */
  _findRemainingSites: function Updater_findRemainingSites(aLinks) {
    let map = {};

    // Create a map to easily retrieve the site for a given URL.
    gGrid.sites.forEach(function (aSite) {
      if (aSite)
        map[aSite.url] = aSite;
    });

    // Map each link to its corresponding site, if any.
    return aLinks.map(function (aLink) {
      return aLink && (aLink.url in map) && map[aLink.url];
    });
  },

  /**
   * Freezes the given sites' positions.
   * @param aSites The array of sites to freeze.
   */
  _freezeSitePositions: function Updater_freezeSitePositions(aSites) {
    aSites.forEach(function (aSite) {
      if (aSite)
        gTransformation.freezeSitePosition(aSite);
    });
  },

  /**
   * Moves the given sites' DOM nodes to their new positions.
   * @param aSites The array of sites to move.
   */
  _moveSiteNodes: function Updater_moveSiteNodes(aSites) {
    let cells = gGrid.cells;
    let sites = aSites.slice(0, cells.length);

    sites.forEach(function (aSite, aIndex) {
      let cell = cells[aIndex];
      let cellSite = cell.site;

      // The site's position didn't change.
      if (!aSite || cellSite != aSite) {
        let cellNode = cell.node;

        // Empty the cell if necessary.
        if (cellSite)
          cellNode.removeChild(cellSite.node);

        // Put the new site in place, if any.
        if (aSite)
          cellNode.appendChild(aSite.node);
      }
    }, this);
  },

  /**
   * Rearranges the given sites and slides them to their new positions.
   * @param aSites The array of sites to re-arrange.
   * @param aCallback The callback to call when finished.
   */
  _rearrangeSites: function Updater_rearrangeSites(aSites, aCallback) {
    let options = {callback: aCallback, unfreeze: true};
    gTransformation.rearrangeSites(aSites, options);
  },

  /**
   * Removes all sites from the grid that are not in the given links array or
   * exceed the grid.
   * @param aSites The array of sites remaining in the grid.
   * @param aCallback The callback to call when finished.
   */
  _removeLegacySites: function Updater_removeLegacySites(aSites, aCallback) {
    let batch = new Batch(aCallback);

    // Delete sites that were removed from the grid.
    gGrid.sites.forEach(function (aSite) {
      // The site must be valid and not in the current grid.
      if (!aSite || aSites.indexOf(aSite) != -1)
        return;

      batch.push();

      // Fade out the to-be-removed site.
      gTransformation.hideSite(aSite, function () {
        let node = aSite.node;

        // Remove the site from the DOM.
        node.parentNode.removeChild(node);
        batch.pop();
      });
    });

    batch.close();
  },

  /**
   * Tries to fill empty cells with new links if available.
   * @param aLinks The array of links.
   * @param aCallback The callback to call when finished.
   */
  _fillEmptyCells: function Updater_fillEmptyCells(aLinks, aCallback) {
    let {cells, sites} = gGrid;
    let batch = new Batch(aCallback);

    // Find empty cells and fill them.
    sites.forEach(function (aSite, aIndex) {
      if (aSite || !aLinks[aIndex])
        return;

      batch.push();

      // Create the new site and fade it in.
      let site = gGrid.createSite(aLinks[aIndex], cells[aIndex]);

      // Set the site's initial opacity to zero.
      site.node.style.opacity = 0;

      // Without the setTimeout() the node would just appear instead of fade in.
      setTimeout(function () {
        gTransformation.showSite(site, function () batch.pop());
      }, 0);
    });

    batch.close();
  }
};
