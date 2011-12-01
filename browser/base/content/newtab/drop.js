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

// a little delay that prevents the grid from being too sensitive when dragging
// sites around
const DELAY_REARRANGE = 250;

// ##########
// Class: Drag
// This singleton implements site dropping functionality.
let Drop = {
  // the last drop target
  _lastDropTarget: null,

  // ----------
  // Function: enter
  // Handles the 'dragenter' event.
  //
  // Parameters:
  //   aCell - the drop target cell
  enter: function Drop_enter(aCell) {
    this._delayedRearrange(aCell);
  },

  // ----------
  // Function: exit
  // Handles the 'dragexit' event.
  //
  // Parameters:
  //   aCell - the drop target cell
  //   aEvent - the 'dragexit' event
  exit: function Drop_exit(aCell, aEvent) {
    if (aEvent.dataTransfer && !aEvent.dataTransfer.mozUserCancelled) {
      this._delayedRearrange();
    } else {
      // the drag operation has been cancelled
      this._cancelDelayedArrange();
      this._rearrange();
    }
  },

  // ----------
  // Function: drop
  // Handles the 'drop' event.
  //
  // Parameters:
  //   aCell - the drop target cell
  //   aEvent - the 'dragexit' event
  //   aCallback - the callback to call when the drop is finished
  drop: function Drop_drop(aCell, aEvent, aCallback) {
    // the cell that is the drop target could contain a pinned site. we need
    // to find out where that site has gone and re-pin it there.
    if (aCell.containsPinnedSite())
      this._repinSitesAfterDrop(aCell);

    // pin the dragged or insert the new site
    this._pinDraggedSite(aCell, aEvent);

    this._cancelDelayedArrange();

    // update the grid and move all sites to their new places
    Updater.updateGrid(aCallback);
  },

  // ----------
  // Function: _repinSitesAfterDrop
  // Re-pins all pinned sites in their (new) positions.
  //
  // Parameters:
  //   aCell - the drop target cell
  _repinSitesAfterDrop: function Drop__repinSitesAfterDrop(aCell) {
    let sites = DropPreview.rearrange(aCell);

    // filter out pinned sites
    let pinnedSites = sites.filter(function (aSite) {
      return aSite && aSite.isPinned();
    });

    // re-pin all shifted pinned cells
    pinnedSites.forEach(function (aSite) aSite.pin(sites.indexOf(aSite)));
  },

  // ----------
  // Function: _pinDraggedSite
  // Pins the dragged site in its new place.
  //
  // Parameters:
  //   aCell - the drop target cell
  //   aEvent - the 'dragexit' event
  _pinDraggedSite: function Drop__pinDraggedSite(aCell, aEvent) {
    let index = aCell.index;
    let draggedSite = Drag.draggedSite;

    if (draggedSite) {
      // pin the dragged site at its new place
      if (aCell != draggedSite.cell)
        draggedSite.pin(index);
    } else {
      // a new link was dragged onto the grid. create it by pinning its URL.
      let dt = aEvent.dataTransfer;
      let [url, title] = dt.getData("text/x-moz-url").split("\n");
      PinnedLinks.pin({url: url, title: title}, index);
    }
  },

  // ----------
  // Function: _delayedRearrange
  // Time a rearrange with a little delay.
  //
  // Parameters:
  //   aCell - the drop target cell
  _delayedRearrange: function Drop__delayedRearrange(aCell) {
    // the last drop target didn't change so there's no need to re-arrange
    if (this._lastDropTarget == aCell)
      return;

    let self = this;

    function callback() {
      self._rearrangeTimeout = null;
      self._rearrange(aCell);
    }

    this._cancelDelayedArrange();
    this._rearrangeTimeout = setTimeout(callback, DELAY_REARRANGE);

    // store the last drop target
    this._lastDropTarget = aCell;
  },

  // ----------
  // Function: _cancelDelayedArrange
  // Cancels a timed rearrange, if any.
  _cancelDelayedArrange: function Drop__cancelDelayedArrange() {
    if (this._rearrangeTimeout) {
      clearTimeout(this._rearrangeTimeout);
      this._rearrangeTimeout = null;
    }
  },

  // ----------
  // Function: _rearrange
  // Rearrange all sites in the grid depending on the current drop target.
  //
  // Parameters:
  //   aCell - the drop target cell
  _rearrange: function Drop__rearrange(aCell) {
    let sites = Grid.sites;

    // we need to rearrange the grid only if there's a current drop target
    if (aCell)
      sites = DropPreview.rearrange(aCell);

    Transformation.rearrangeSites(sites, {unfreeze: !aCell});
  }
};
