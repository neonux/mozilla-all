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
// Class: DropPreview
// This singleton provides the ability to re-arrange the current grid to
// indicate the transformation that results from dropping a cell at a certain
// position.
let DropPreview = {
  // ----------
  // Function: rearrange
  // Returns a new arrangement of the sites currently contained in the grid
  // when a site would be dropped onto the given cell.
  //
  // Parameters:
  //   aCell - the drop target cell
  rearrange: function DropPreview_rearrange(aCell) {
    let sites = Grid.sites;

    // insert the dragged site into the current grid
    this._insertDraggedSite(sites, aCell);

    // after the new site has been inserted we need to correct the positions
    // of all pinned tabs that have been moved around
    this._repositionPinnedSites(sites, aCell);

    return sites;
  },

  // ----------
  // Function: _insertDraggedSite
  // Inserts the currently dragged site into the given array of sites.
  //
  // Parameters:
  //   aSites - the array of sites to insert into
  //   aCell - the drop target cell
  _insertDraggedSite: function DropPreview__insertDraggedSite(aSites, aCell) {
    let dropIndex = aCell.index;
    let draggedSite = Drag.draggedSite;

    // we're currently dragging a site
    if (draggedSite) {
      let dragCell = draggedSite.cell;
      let dragIndex = dragCell.index;

      // move the dragged site into its new position
      if (dragIndex != dropIndex) {
        aSites.splice(dragIndex, 1);
        aSites.splice(dropIndex, 0, draggedSite);
      }
    // we're handling an external drag item
    } else {
      aSites.splice(dropIndex, 0, null);
    }
  },

  // ----------
  // Function: _repositionPinnedSites
  // Correct the position of all pinned sites that might have been moved to
  // different positions after the dragged site has been inserted.
  //
  // Parameters:
  //   aSites - the array of sites containing the dragged site
  //   aCell - the drop target cell
  _repositionPinnedSites:
    function DropPreview__repositionPinnedSites(aSites, aCell) {

    // collect all pinned sites
    let pinnedSites = this._filterPinnedSites(aSites, aCell);

    // correct pinned site positions
    pinnedSites.forEach(function (aSite) {
      aSites[aSites.indexOf(aSite)] = aSites[aSite.cell.index];
      aSites[aSite.cell.index] = aSite;
    }, this);

    // there might be a pinned cell that got pushed out of the grid, try to
    // sneak it in by removing a lower-priority cell
    if (this._hasOverflownPinnedSite(aSites, aCell))
      this._repositionOverflownPinnedSite(aSites, aCell);
  },

  // ----------
  // Function: _filterPinnedSites
  // Filter pinned sites out of the grid that are still on their old positions
  // and have not moved.
  //
  // Parameters:
  //   aSites - the array of sites to filter
  //   aCell - the drop target cell
  _filterPinnedSites: function DropPreview__filterPinnedSites(aSites, aCell) {
    let draggedSite = Drag.draggedSite;

    // when dropping on a cell that contains a pinned site make sure that all
    // pinned cells surrounding the drop target are moved as well
    let range = this._getPinnedRange(aCell);

    return aSites.filter(function (aSite, aIndex) {
      // the site must be valid, pinned and not the dragged site
      if (!aSite || aSite == draggedSite || !aSite.isPinned())
        return false;

      let index = aSite.cell.index;

      // if it's not in the 'pinned range' it's a valid pinned site
      return (index > range.end || index < range.start);
    });
  },

  // ----------
  // Function: _getPinnedRange
  // Returns the range of pinned sites surrounding the drop target cell.
  //
  // Parameters:
  //   aCell - the drop target cell
  _getPinnedRange: function DropPreview__getPinnedRange(aCell) {
    let dropIndex = aCell.index;
    let range = {start: dropIndex, end: dropIndex};

    // we need a pinned range only when dropping on a pinned site
    if (aCell.containsPinnedSite()) {
      let links = PinnedLinks.links;

      // find all previous siblings of the drop target that are pinned as well
      while (range.start && links[range.start - 1])
        range.start--;

      let maxEnd = links.length - 1;

      // find all next siblings of the drop target that are pinned as well
      while (range.end < maxEnd && links[range.end + 1])
        range.end++;
    }

    return range;
  },

  // ----------
  // Function: _filterPinnedSites
  // Check if the given arrray of sites contains a pinned sites that has
  // been pushed out of the grid.
  //
  // Parameters:
  //   aSites - the array of sites to check
  //   aCell - the drop target cell
  _hasOverflownPinnedSite:
    function DropPreview__hasOverflownPinnedSite(aSites, aCell) {

    // if the drop target isn't pinned there's no way a pinned site has been
    // pushed out of the grid so we can just exit here
    if (!aCell.containsPinnedSite())
      return false;

    let cells = Grid.cells;

    // no cells have been pushed out of the grid, nothing to do here
    if (aSites.length <= cells.length)
      return false;

    let overflownSite = aSites[cells.length];

    // nothing to do if the site that got pushed out of the grid is not pinned
    return (overflownSite && overflownSite.isPinned());
  },

  // ----------
  // Function: _repositionOverflownPinnedSite
  // We have a overflown pinned site that we need to re-position so that it's
  // visible again. We try to find a lower-prio cell (empty or containing
  // an unpinned site) that we can move it to.
  //
  // Parameters:
  //   aSites - the array of sites
  //   aCell - the drop target cell
  _repositionOverflownPinnedSite:
    function DropPreview__repositionOverflownPinnedSite(aSites, aCell) {

    // try to find a lower-prio cell (empty or containing an unpinned site)
    let index = this._indexOfLowerPrioritySite(aSites, aCell);

    if (index > -1) {
      let cells = Grid.cells;
      let dropIndex = aCell.index;

      // move all pinned cells to their new positions to let the overflown
      // site fit into the grid
      for (let i = index + 1, lastPosition = index; i < aSites.length; i++) {
        if (i != dropIndex) {
          aSites[lastPosition] = aSites[i];
          lastPosition = i;
        }
      }

      // finally, remove the overflown site from its previous position
      aSites.splice(cells.length, 1);
    }
  },

  // ----------
  // Function: _indexOfLowerPrioritySite
  // Returns the index of the last cell that is empty or contains an unpinned
  // site. These are considered to be of a lower priority.
  //
  // Parameters:
  //   aSites - the array of sites
  //   aCell - the drop target cell
  _indexOfLowerPrioritySite:
    function DropPreview__indexOfLowerPrioritySite(aSites, aCell) {

    let cells = Grid.cells;
    let dropIndex = aCell.index;

    // search (beginning with the last site in the grid) for a site that is
    // empty or unpinned (an thus lower-priority) and can be pushed out of the
    // grid instead of the pinned site
    for (let i = cells.length - 1; i >= 0; i--) {
      // the cell that is our drop target is not a good choice
      if (i == dropIndex)
        continue;

      let site = aSites[i];

      // we can use the cell only if it's empty or the site is un-pinned
      if (!site || !site.isPinned())
        return i;
    }

    return -1;
  }
};
