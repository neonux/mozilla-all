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
// Class: Animations
// This singleton provides all kinds of animations.
let Animations = {
  // ----------
  // Function: fadeIn
  // Fades in the given node from 0 to 1 opacity.
  //
  // Parameters:
  //   aNode - the node to animate
  //   aCallback - the callback to call when finished
  fadeIn: function Animations_fadeIn(aNode, aCallback) {
    new CssAnimation(aNode, "anim-fadein", aCallback);
  },

  // ----------
  // Function: fadeOut
  // Fades out the given node from 1 to 0 opacity.
  //
  // Parameters:
  //   aNode - the node to animate
  //   aCallback - the callback to call when finished
  fadeOut: function Animations_fadeOut(aNode, aCallback) {
    new CssAnimation(aNode, "anim-fadeout", aCallback);
  }
};

// ##########
// Class: CssAnimations
// This class provides an easy way to run CSS animations and wait until they're
// finished.
function CssAnimation(aNode, aName, aCallback) {
  this._node = aNode;
  this._name = aName;
  this._callback = aCallback;

  this._start();
}

CssAnimation.prototype = {
  // ----------
  // Function: _start
  // Starts the CSS animation.
  _start: function CssAnimations__start() {
    let node = this._node;
    let name = this._name;
    let classes = node.classList;
    let callback = this._callback;

    this._whenAnimationEnded(node, function () {
      classes.remove(name);

      if (callback)
        callback();
    });

    classes.add(name);
  },

  // ----------
  // Function: _whenAnimationEnded
  // Calls the given callback when a CSS animation in the given node is finished.
  //
  // Parameters:
  //   aNode - the node that is animated
  //   aCallback - the callback to call when finished
  _whenAnimationEnded:
    function CssAnimations__whenAnimationEnded(aNode, aCallback) {

    aNode.addEventListener("animationend", function onEnd() {
      aNode.removeEventListener("animationend", onEnd, false);
      aCallback();
    }, false);
  }
};
