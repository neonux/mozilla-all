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
 * This singleton provides a custom drop target detection. We need this because
 * the default DnD target detection relies on the cursor's position. We want
 * to pick a drop target based on the dragged site's position.
 */
let gDropTargetShim = {
  /**
   * Cache for the position of all cells, cleaned after drag finished.
   */
  _cellPositions: null,

  /**
   * The last drop target that was hovered.
   */
  _lastDropTarget: null,

  /**
   * Initializes the drop target shim.
   */
  init: function DropTargetShim_init() {
    let node = gGrid.node;

    this._dragover = this._dragover.bind(this);

    // Add drag event handlers.
    node.addEventListener("dragstart", this._start.bind(this), true);
    // XXX bug 505521 - Don't listen for drag, it's useless at the moment.
    //node.addEventListener("drag", this._drag.bind(this), false);
    node.addEventListener("dragend", this._end.bind(this), true);
  },

  /**
   * Handles the 'dragstart' event.
   * @param aEvent The 'dragstart' event.
   */
  _start: function DropTargetShim_start(aEvent) {
    gGrid.lock();

    // XXX bug 505521 - Listen for dragover on the document.
    document.documentElement.addEventListener("dragover", this._dragover, false);
  },

  /**
   * Handles the 'drag' event and determines the current drop target.
   * @param aEvent The 'drag' event.
   */
  _drag: function DropTargetShim_drag(aEvent) {
    // Let's see if we find a drop target.
    let target = this._findDropTarget(aEvent);

    if (target == this._lastDropTarget) {
      // XXX bug 505521 - Don't fire dragover for now (causes recursion).
      /*if (target)
        // The last drop target is valid and didn't change.
        this._dispatchEvent(aEvent, "dragover", target);*/
    } else {
      if (this._lastDropTarget)
        // We left the last drop target.
        this._dispatchEvent(aEvent, "dragexit", this._lastDropTarget);

      if (target)
        // We're now hovering a (new) drop target.
        this._dispatchEvent(aEvent, "dragenter", target);

      if (this._lastDropTarget)
        // We left the last drop target.
        this._dispatchEvent(aEvent, "dragleave", this._lastDropTarget);

      this._lastDropTarget = target;
    }
  },

  /**
   * Handles the 'dragover' event as long as bug 505521 isn't fixed to get
   * current mouse cursor coordinates while dragging.
   * @param aEvent The 'dragover' event.
   */
  _dragover: function DropTargetShim_dragover(aEvent) {
    let sourceNode = aEvent.dataTransfer.mozSourceNode;
    gDrag.drag(sourceNode._newtabSite, aEvent);

    this._drag(aEvent);
  },

  /**
   * Handles the 'dragend' event.
   * @param aEvent The 'dragend' event.
   */
  _end: function DropTargetShim_end(aEvent) {
    // Make sure to determine the current drop target in case the dragenter
    // event hasn't been fired.
    this._drag(aEvent);

    if (this._lastDropTarget) {
      if (aEvent.dataTransfer.mozUserCancelled) {
        // The drag operation was cancelled.
        this._dispatchEvent(aEvent, "dragexit", this._lastDropTarget);
        this._dispatchEvent(aEvent, "dragleave", this._lastDropTarget);
      } else {
        // A site was successfully dropped.
        this._dispatchEvent(aEvent, "drop", this._lastDropTarget);
      }

      // Clean up.
      this._lastDropTarget = null;
      this._cellPositions = null;
    }

    gGrid.unlock();

    // XXX bug 505521 - Remove the document's dragover listener.
    document.documentElement.removeEventListener("dragover", this._dragover, false);
  },

  /**
   * Determines the current drop target by matching the dragged site's position
   * against all cells in the grid.
   * @return The currently hovered drop target or null.
   */
  _findDropTarget: function DropTargetShim_findDropTarget() {
    // These are the minimum intersection values - we want to use the cell if
    // the site is >= 50% hovering its position.
    let minWidth = gDrag.cellWidth / 2;
    let minHeight = gDrag.cellHeight / 2;

    let cellPositions = this._getCellPositions();
    let rect = gTransformation.getNodePosition(gDrag.draggedSite.node);

    // Compare each cell's position to the dragged site's position.
    for (let i = 0; i < cellPositions.length; i++) {
      let inter = rect.intersect(cellPositions[i].rect);

      // If the intersection is big enough we found a drop target.
      if (inter.width >= minWidth && inter.height >= minHeight)
        return cellPositions[i].cell;
    }

    // No drop target found.
    return null;
  },

  /**
   * Gets the positions of all cell nodes.
   * @return The (cached) cell positions.
   */
  _getCellPositions: function DropTargetShim_getCellPositions() {
    if (this._cellPositions)
      return this._cellPositions;

    return this._cellPositions = gGrid.cells.map(function (cell) {
      return {cell: cell, rect: gTransformation.getNodePosition(cell.node)};
    });
  },

  /**
   * Dispatches a custom DragEvent on the given target node.
   * @param aEvent The source event.
   * @param aType The event type.
   * @param aTarget The target node that receives the event.
   */
  _dispatchEvent:
    function DropTargetShim_dispatchEvent(aEvent, aType, aTarget) {

    let node = aTarget.node;
    let event = document.createEvent("DragEvents");

    event.initDragEvent(aType, true, true, window, 0, 0, 0, 0, 0, false, false,
                        false, false, 0, node, aEvent.dataTransfer);

    node.dispatchEvent(event);
  }
};
