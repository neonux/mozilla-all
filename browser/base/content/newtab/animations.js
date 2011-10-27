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

// TODO
let Animations = {
  fadeIn: function Anim_fadeIn(aNode, aCallback) {
    new CssAnimation(aNode, "anim-fadein", aCallback);
  },

  fadeOut: function Anim_fadeOut(aNode, aCallback) {
    new CssAnimation(aNode, "anim-fadeout", aCallback);
  }
};

// TODO
function CssAnimation(aNode, aName, aCallback) {
  this._node = aNode;
  this._name = aName;
  this._callback = aCallback;

  this._start();
}

CssAnimation.prototype = {
  _start: function CssAnim__start() {
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

  _whenAnimationEnded: function CssAnim__whenAnimationEnded(aNode, aCallback) {
    aNode.addEventListener("animationend", function onEnd() {
      aNode.removeEventListener("animationend", onEnd, false);
      aCallback();
    }, false);
  }
};
