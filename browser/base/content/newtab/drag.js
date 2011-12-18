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
 * This singleton implements site dragging functionality.
 */
let gDrag = {
  /**
   * The site offset to the drag start point.
   */
  _offsetX: null,
  _offsetY: null,

  /**
   * The site that is dragged.
   */
  _draggedSite: null,
  get draggedSite() this._draggedSite,

  /**
   * The cell width/height at the point the drag started.
   */
  _cellWidth: null,
  _cellHeight: null,
  get cellWidth() this._cellWidth,
  get cellHeight() this._cellHeight,

  /**
   * Start a new drag operation.
   * @param aSite The site that's being dragged.
   * @param aEvent The 'dragstart' event.
   */
  start: function Drag_start(aSite, aEvent) {
    this._draggedSite = aSite;

    // Prevent moz-transform for left, top.
    aSite.node.setAttribute("dragged", "true");

    // Make sure the dragged site is floating above the grid.
    aSite.node.setAttribute("ontop", "true");

    this._setDragData(aSite, aEvent);

    // Store the cursor offset.
    let node = aSite.node;
    let rect = node.getBoundingClientRect();
    this._offsetX = aEvent.clientX - rect.left;
    this._offsetY = aEvent.clientY - rect.top;

    // Store the cell dimensions.
    let cellNode = aSite.cell.node;
    this._cellWidth = cellNode.offsetWidth;
    this._cellHeight = cellNode.offsetHeight;

    gTransformation.freezeSitePosition(aSite);
  },

  /**
   * Handles the 'drag' event.
   * @param aSite The site that's being dragged.
   * @param aEvent The 'drag' event.
   */
  drag: function Drag_drag(aSite, aEvent) {
    // Get the viewport size.
    let {clientWidth, clientHeight} = document.documentElement;

    // We'll want a padding of 5px.
    let border = 5;

    // Enforce minimum constraints to keep the drag image inside the window.
    let left = Math.max(scrollX + aEvent.clientX - this._offsetX, border);
    let top = Math.max(scrollY + aEvent.clientY - this._offsetY, border);

    // Enforce maximum constraints to keep the drag image inside the window.
    left = Math.min(left, scrollX + clientWidth - this.cellWidth - border);
    top = Math.min(top, scrollY + clientHeight - this.cellHeight - border);

    // Update the drag image's position.
    gTransformation.setSitePosition(aSite, {left: left, top: top});
  },

  /**
   * Ends the current drag operation.
   * @param aSite The site that's being dragged.
   * @param aEvent The 'dragend' event.
   */
  end: function Drag_end(aSite, aEvent) {
    aSite.node.removeAttribute("dragged");

    // Slide the dragged site back into its cell (may be the old or the new cell).
    gTransformation.slideSiteTo(aSite, aSite.cell, {
      unfreeze: true,
      callback: function () aSite.node.removeAttribute("ontop")
    });

    this._draggedSite = null;
  },

  /**
   * Initializes the drag data for the current drag operation.
   * @param aSite The site that's being dragged.
   * @param aEvent The 'dragstart' event.
   */
  _setDragData: function Drag_setDragData(aSite, aEvent) {
    let {url, title} = aSite;

    let dt = aEvent.dataTransfer;
    dt.mozCursor = "default";
    dt.effectAllowed = "move";
    dt.setData("text/plain", url);
    dt.setData("text/uri-list", url);
    dt.setData("text/x-moz-url", url + "\n" + title);
    dt.setData("text/html", "<a href=\"" + url + "\">" + url + "</a>");

    // Create and use an empty drag element. We don't want to use the default
    // drag image with its default opacity.
    let dragElement = document.createElement("div");
    dragElement.classList.add("drag-element");
    let body = document.body;
    body.appendChild(dragElement);
    dt.setDragImage(dragElement, 0, 0);

    // After the 'dragstart' event has been processed we can remove the
    // temporary drag element from the DOM.
    setTimeout(function () body.removeChild(dragElement), 0);
  }
};
