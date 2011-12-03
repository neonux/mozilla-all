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
// Class: Transformation
// This singleton allows to transform the grid by moving sites around.
let Transformation = {
  // ----------
  // Function: getNodePosition
  // Returns a Rect instance with the DOM node's position.
  //
  // Parameters:
  //   aNode - the DOM node
  getNodePosition: function Transformation_getNodePosition(aNode) {
    let {left, top, width, height} = aNode.getBoundingClientRect();
    return new Rect(left + scrollX, top + scrollY, width, height);
  },

  // ----------
  // Function: setSitePosition
  // Allows to set a site's position.
  //
  // Parameters:
  //   aSite - the site to re-position
  //   aPosition - the desired position for the given site
  setSitePosition: function Transformation_setSitePosition(aSite, aPosition) {
    let style = aSite.node.style;
    let {top, left} = aPosition;

    style.top = top + "px";
    style.left = left + "px";
  },

  // ----------
  // Function: freezeSitePosition
  // Freezes a site in its current position by positioning it absolute.
  //
  // Parameters:
  //   aSite - the site to freeze
  freezeSitePosition: function Transformation_freezeSitePosition(aSite) {
    aSite.node.classList.add("site-frozen");
    this.setSitePosition(aSite, this.getNodePosition(aSite.node));
  },

  // ----------
  // Function: unfreezeSitePosition
  // Unfreezes a site in by removing its absolute positioning.
  //
  // Parameters:
  //   aSite - the site to unfreeze
  unfreezeSitePosition: function Transformation_unfreezeSitePosition(aSite) {
    let style = aSite.node.style;
    style.left = style.top = "";
    aSite.node.classList.remove("site-frozen");
  },

  // ----------
  // Function: slideSiteTo
  // Slides the given site to the target node's position.
  //
  // Parameters:
  //   aSite - the site to move
  //   aTarget - the slide target
  //   aOptions - set of options (see below)
  //     unfreeze - unfreeze the site after sliding
  //     callback - the callback to call when finished
  slideSiteTo: function Transformation_slideSiteTo(aSite, aTarget, aOptions) {
    let currentPosition = this.getNodePosition(aSite.node);
    let targetPosition = this.getNodePosition(aTarget.node)
    let callback = aOptions && aOptions.callback;

    let self = this;

    function finish() {
      if (aOptions && aOptions.unfreeze)
        self.unfreezeSitePosition(aSite);

      if (callback)
        callback();
    }

    // nothing to do here if the positions already match
    if (currentPosition.equals(targetPosition)) {
      finish();
    } else {
      this.setSitePosition(aSite, targetPosition);
      this._whenTransitionEnded(aSite.node, finish);
    }
  },

  // ----------
  // Function: rearrangeSites
  // Rearranges a given array of sites and moves them to their new positions or
  // fades in/out new/removed sites.
  //
  // Parameters:
  //   aSites - an array of sites to rearrange
  //   aOptions - set of options (see below)
  //     unfreeze - unfreeze the site after rearranging
  //     callback - the callback to call when finished
  rearrangeSites:
    function Transformation_rearrangeSites(aSites, aOptions) {

    let batch;
    let cells = Grid.cells;
    let callback = aOptions && aOptions.callback;
    let unfreeze = aOptions && aOptions.unfreeze;

    if (callback) {
      batch = new Batch(callback);
      callback = function () batch.pop();
    }

    aSites.forEach(function (aSite, aIndex) {
      // do not re-arrange empty cells or the dragged site
      if (!aSite || aSite == Drag.draggedSite)
        return;

      if (batch)
        batch.push();

      if (!cells[aIndex])
        // the site disappeared from the grid, hide it
        this._hideSite(aSite, callback);
      else if (aSite.node.classList.contains("site-hidden"))
        // the site disappeared before but is now back, show it
        this._showSite(aSite, callback);
      else
        // the site's position has changed, move it around
        this._moveSite(aSite, aIndex, {unfreeze: unfreeze, callback: callback});
    }, this);

    if (batch)
      batch.close();
  },

  // ----------
  // Function: _whenTransitionEnded
  // Listens for the 'transitionend' event on a given node and calls the given
  // callback.
  //
  // Parameters:
  //   aNode - the node that is transitioned
  //   aCallback - the callback to call when finished
  _whenTransitionEnded:
    function Transformation__whenTransitionEnded(aNode, aCallback) {

    aNode.addEventListener("transitionend", function onEnd() {
      aNode.removeEventListener("transitionend", onEnd, false);
      aCallback();
    }, false);
  },

  // ----------
  // Function: _showSite
  // Fades a given site from zero to full opacity.
  //
  // Parameters:
  //   aSite - the site to fade
  //   aCallback - the callback to call when finished
  _showSite: function Transformation__showSite(aSite, aCallback) {
    aSite.node.classList.remove("site-hidden");
    Animations.fadeIn(aSite.node, aCallback);
  },

  // ----------
  // Function: _hideSite
  // Fades a given site from full to zero opacity.
  //
  // Parameters:
  //   aSite - the site to fade
  //   aCallback - the callback to call when finished
  _hideSite: function Transformation__hideSite(aSite, aCallback) {
    Animations.fadeOut(aSite.node, function () {
      aSite.node.classList.add("site-hidden");
      aCallback();
    });
  },

  // ----------
  // Function: _moveSite
  // Moves a site to the cell with the given index.
  //
  // Parameters:
  //   aSite - the site to move
  //   aIndex - the target cell's index
  //   aOptions - options that are directly passed to slideSiteTo()
  _moveSite: function Transformation__moveSite(aSite, aIndex, aOptions) {
    this.freezeSitePosition(aSite);
    this.slideSiteTo(aSite, Grid.cells[aIndex], aOptions);
  }
};
