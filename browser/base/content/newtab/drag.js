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
// Class: Drag
// This singleton implements site dragging functionality.
let Drag = {
  // the site offset to the drag start point
  _offsetX: null,
  _offsetY: null,

  // the site that is dragged
  _draggedSite: null,
  get draggedSite() this._draggedSite,

  // the cell width/height at the point the drag started
  _cellWidth: null,
  _cellHeight: null,
  get cellWidth() this._cellWidth,
  get cellHeight() this._cellHeight,

  // ----------
  // Function: start
  // Start a new drag operation.
  //
  // Parameters:
  //   aSite - the site that's being dragged
  //   aEvent - the 'dragstart' event
  start: function (aSite, aEvent) {
    this._draggedSite = aSite;

    // prevent moz-transform for left, top
    aSite.node.classList.add("site-dragged");

    // make sure the dragged site is floating above the grid
    aSite.node.classList.add("site-ontop");

    this._setDragData(aSite, aEvent);

    // store cursor offset
    let node = aSite.node;
    let rect = node.getBoundingClientRect();
    this._offsetX = aEvent.clientX - rect.left;
    this._offsetY = aEvent.clientY - rect.top;

    // store cell dimensions
    let cellNode = aSite.cell.node;
    this._cellWidth = cellNode.offsetWidth;
    this._cellHeight = cellNode.offsetHeight;

    Transformation.freezeSitePosition(aSite);
  },

  // ----------
  // Function: drag
  // Handles the 'drag' event.
  //
  // Parameters:
  //   aSite - the site that's being dragged
  //   aEvent - the 'drag' event
  drag: function (aSite, aEvent) {
    // get the viewport size
    let {clientWidth, clientHeight} = document.documentElement;

    // we'll want a padding of 5px
    let border = 5;

    // enforce minimum constraints to keep the drag image inside the window
    let left = Math.max(scrollX + aEvent.clientX - this._offsetX, border);
    let top = Math.max(scrollY + aEvent.clientY - this._offsetY, border);

    // enforce maximum constraints to keep the drag image inside the window
    left = Math.min(left, scrollX + clientWidth - this.cellWidth - border);
    top = Math.min(top, scrollY + clientHeight - this.cellHeight - border);

    // update the drag image's position
    Transformation.setSitePosition(aSite, {left: left, top: top});
  },

  // ----------
  // Function: end
  // Ends the current drag operation.
  //
  // Parameters:
  //   aSite - the site that's being dragged
  //   aEvent - the 'dragend' event
  end: function (aSite, aEvent) {
    let self = this;

    let classes = aSite.node.classList;
    classes.remove("site-dragged");

    // slide the dragged site back into its cell (may be the old or the new cell)
    Transformation.slideSiteTo(aSite, aSite.cell, {
      unfreeze: true,
      callback: function () {
        classes.remove("site-ontop");
      }
    });

    this._draggedSite = null;
  },

  // ----------
  // Function: _setDragData
  // Initializes the drag data for the current drag operation.
  //
  // Parameters:
  //   aSite - the site that's being dragged
  //   aEvent - the 'dragstart' event
  _setDragData: function (aSite, aEvent) {
    let {url, title} = aSite;

    let dt = aEvent.dataTransfer;
    dt.effectAllowed = "move";
    dt.setData("text/plain", url);
    dt.setData("text/uri-list", url);
    dt.setData("text/x-moz-url", url + "\n" + title);
    dt.setData("text/html", "<a href=\"" + url + "\">" + url + "</a>");

    // create and use an empty drag image. we don't want to use the default
    // drag image with its default opacity.
    let img = document.createElement("div");
    img.classList.add("drag-img");
    let body = document.body;
    body.appendChild(img);
    dt.setDragImage(img, 0, 0);

    // after the 'dragstart' event has been processed we can remove the
    // temporary drag image from the DOM
    setTimeout(function () body.removeChild(img), 0);
  }
};
