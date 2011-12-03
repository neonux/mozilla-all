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
// Class: DropTargetShim
// This singleton provides a custom drop target detection. We need this because
// the default DnD target detection relies on the cursor's position. We want
// to pick a drop target based on the dragged site's position.
let DropTargetShim = {
  // cache for the position of all cells, cleaned after drag finished
  _cellPositions: null,

  // the last drop target that was hovered
  _lastDropTarget: null,

  // ----------
  // Function: init
  // Initializes the drop target shim.
  init: function DropTargetShim_init() {
    let node = Grid.node;

    // add drag event handlers
    node.addEventListener("dragstart", this._start.bind(this), true);
    node.addEventListener("drag", this._drag.bind(this), false);
    node.addEventListener("dragend", this._end.bind(this), true);
  },

  // ----------
  // Function: _start
  // Handles the 'dragstart' event.
  //
  // Parameters:
  //   aEvent - the 'dragstart' event
  _start: function DropTargetShim__start(aEvent) {
    Grid.lock();
  },

  // ----------
  // Function: _start
  // Handles the 'drag' event and determines the current drop target.
  //
  // Parameters:
  //   aEvent - the 'drag' event
  _drag: function DropTargetShim__drag(aEvent) {
    // let's see if we find a drop target
    let target = this._findDropTarget(aEvent);

    if (target == this._lastDropTarget) {
      if (target)
        // the last drop target is valid and didn't change
        this._dispatchEvent(aEvent, "dragover", target);
    } else {
      if (this._lastDropTarget)
        // we left the last drop target
        this._dispatchEvent(aEvent, "dragexit", this._lastDropTarget);

      if (target)
        // we're now hovering a (new) drop target
        this._dispatchEvent(aEvent, "dragenter", target);

      if (this._lastDropTarget)
        // we left the last drop target
        this._dispatchEvent(aEvent, "dragleave", this._lastDropTarget);

      this._lastDropTarget = target;
    }
  },

  // ----------
  // Function: _end
  // Handles the 'dragend' event.
  //
  // Parameters:
  //   aEvent - the 'dragend' event
  _end: function DropTargetShim__end(aEvent) {
    // make sure to determine the current drop target in case the dragenter
    // event hasn't been fired.
    this._drag(aEvent);

    if (this._lastDropTarget) {
      if (aEvent.dataTransfer.mozUserCancelled) {
        // the drag operation was cancelled
        this._dispatchEvent(aEvent, "dragexit", this._lastDropTarget);
        this._dispatchEvent(aEvent, "dragleave", this._lastDropTarget);
      } else {
        // a site was successfully dropped
        this._dispatchEvent(aEvent, "drop", this._lastDropTarget);
      }

      // clean up
      this._lastDropTarget = null;
      this._cellPositions = null;
    }

    Grid.unlock();
  },

  // ----------
  // Function: _findDropTarget
  // Determines the current drop target by matching the dragged site's position
  // against all cells in the grid.
  _findDropTarget: function DropTargetShim__findDropTarget() {
    // these are the minimum intersection values - we want to use the cell if
    // the site is >= 50% hovering its position
    let minWidth = Drag.cellWidth / 2;
    let minHeight = Drag.cellHeight / 2;

    let cellPositions = this._getCellPositions();
    let rect = Transformation.getNodePosition(Drag.draggedSite.node);

    // compare each cell's position to the dragged site's position
    for (let i = 0; i < cellPositions.length; i++) {
      let inter = rect.intersect(cellPositions[i].rect);

      // if the intersection is big enough we found a drop target
      if (inter.width >= minWidth && inter.height >= minHeight)
        return cellPositions[i].cell;
    }

    // no drop target found
    return null;
  },

  // ----------
  // Function: _getCellPositions
  // Returns the (cached) positions of all cell nodes.
  _getCellPositions: function DropTargetShim__getCellPositions() {
    if (this._cellPositions)
      return this._cellPositions;

    return this._cellPositions = Grid.cells.map(function (cell) {
      return {cell: cell, rect: Transformation.getNodePosition(cell.node)};
    });
  },

  // ----------
  // Function: _dispatchEvent
  // Dispatches a custom DragEvent on the given target node.
  //
  // Parameters:
  //   aEvent - the source event
  //   aType - the event type
  //   aTarget - the target node that receives the event
  _dispatchEvent:
    function DropTargetShim__dispatchEvent(aEvent, aType, aTarget) {

    let node = aTarget.node;
    let event = document.createEvent("DragEvents");

    event.initDragEvent(aType, true, true, window, 0, 0, 0, 0, 0, false, false,
                        false, false, 0, node, aEvent.dataTransfer);

    node.dispatchEvent(event);
  }
};
