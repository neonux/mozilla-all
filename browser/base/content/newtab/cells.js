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
// Class: Cell
// This class represents a cell contained in the grid.
function Cell(aNode) {
  this._node = aNode;
  this._node.__newtabCell = this;

  // register drag-and-drop event handlers
  ["dragenter", "dragover", "dragexit", "drop"].forEach(function (aType) {
    let method = "_" + aType;
    this[method] = this[method].bind(this);
    this._node.addEventListener(aType, this[method], false);
  }, this);
}

Cell.prototype = {
  // the cell's DOM node
  get node() this._node,

  // the cell's offset in the grid
  get index() {
    let index = Grid.cells.indexOf(this);

    // cache this value, overwrite the getter
    this.__defineGetter__("index", function () index);

    return index;
  },

  // the previous cell in the grid
  get previousSibling() {
    let prev = this.node.previousElementSibling;
    prev = prev && prev.__newtabCell;

    // cache this value, overwrite the getter
    this.__defineGetter__("previousSibling", function () prev);

    return prev;
  },

  // the next cell in the grid
  get nextSibling() {
    let next = this.node.nextElementSibling;
    next = next && next.__newtabCell;

    // cache this value, overwrite the getter
    this.__defineGetter__("nextSibling", function () next);

    return next;
  },

  // the site contained in the cell, if any
  get site() {
    let firstChild = this.node.firstElementChild;
    return firstChild && firstChild.__newtabSite;
  },

  // ----------
  // Function: containsPinnedSite
  // Returns whether the cell contains a pinned site.
  containsPinnedSite: function Cell_containsPinnedSite() {
    let site = this.site;
    return site && site.isPinned();
  },

  // ----------
  // Function: isEmpty
  // Returns whether the cell contains a site (is empty).
  isEmpty: function Cell_isEmpty() {
    return !this.site;
  },

  // ----------
  // Function: _dragenter
  // Event handler for the 'dragstart' event.
  //
  // Parameters:
  //   aEvent - the drag event
  _dragenter: function Cell__dragenter(aEvent) {
    aEvent.preventDefault();
    Drop.enter(this, aEvent);
  },

  // ----------
  // Function: _dragover
  // Event handler for the 'dragover' event.
  //
  // Parameters:
  //   aEvent - the drag event
  _dragover: function Cell__dragover(aEvent) {
    aEvent.preventDefault();
  },

  // ----------
  // Function: _dragexit
  // Event handler for the 'dragexit' event.
  //
  // Parameters:
  //   aEvent - the drag event
  _dragexit: function Cell__dragexit(aEvent) {
    Drop.exit(this, aEvent);
  },

  // ----------
  // Function: _drop
  // Event handler for the 'drop' event.
  //
  // Parameters:
  //   aEvent - the drag event
  _drop: function Cell__drop(aEvent) {
    aEvent.preventDefault();
    Drop.drop(this, aEvent);
  }
};
