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
 * This class represents a cell contained in the grid.
 */
function Cell(aNode) {
  this._node = aNode;
  this._node.__newtabCell = this;

  // Register drag-and-drop event handlers.
  ["DragEnter", "DragOver", "DragExit", "Drop"].forEach(function (aType) {
    let method = "_on" + aType;
    this[method] = this[method].bind(this);
    this._node.addEventListener(aType.toLowerCase(), this[method], false);
  }, this);
}

Cell.prototype = {
  /**
   * The cell's DOM node.
   */
  get node() this._node,

  /**
   * The cell's offset in the grid.
   */
  get index() {
    let index = gGrid.cells.indexOf(this);

    // Cache this value, overwrite the getter.
    Object.defineProperty(this, "index", {value: index, enumerable: true});

    return index;
  },

  /**
   * The previous cell in the grid.
   */
  get previousSibling() {
    let prev = this.node.previousElementSibling;
    prev = prev && prev.__newtabCell;

    // Cache this value, overwrite the getter.
    Object.defineProperty(this, "previousSibling", {value: prev, enumerable: true});

    return prev;
  },

  /**
   * The next cell in the grid.
   */
  get nextSibling() {
    let next = this.node.nextElementSibling;
    next = next && next.__newtabCell;

    // Cache this value, overwrite the getter.
    Object.defineProperty(this, "nextSibling", {value: next, enumerable: true});

    return next;
  },

  /**
   * The site contained in the cell, if any.
   */
  get site() {
    let firstChild = this.node.firstElementChild;
    return firstChild && firstChild.__newtabSite;
  },

  /**
   * Checks whether the cell contains a pinned site.
   * @return Whether the cell contains a pinned site.
   */
  containsPinnedSite: function Cell_containsPinnedSite() {
    let site = this.site;
    return site && site.isPinned();
  },

  /**
   * Checks whether the cell contains a site (is empty).
   * @return Whether the cell is empty.
   */
  isEmpty: function Cell_isEmpty() {
    return !this.site;
  },

  /**
   * Event handler for the 'dragenter' event.
   * @param aEvent The dragenter event.
   */
  _onDragEnter: function Cell_onDragEnter(aEvent) {
    aEvent.preventDefault();
    gDrop.enter(this, aEvent);
  },

  /**
   * Event handler for the 'dragover' event.
   * @param aEvent The dragover event.
   */
  _onDragOver: function Cell_onDragOver(aEvent) {
    aEvent.preventDefault();
  },

  /**
   * Event handler for the 'dragexit' event.
   * @param aEvent The dragexit event.
   */
  _onDragExit: function Cell_onDragExit(aEvent) {
    gDrop.exit(this, aEvent);
  },

  /**
   * Event handler for the 'drop' event.
   * @param aEvent The drop event.
   */
  _onDrop: function Cell_onDrop(aEvent) {
    aEvent.preventDefault();
    gDrop.drop(this, aEvent);
  }
};
