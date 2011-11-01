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

// TODO
function DragHandler(aSite) {
  this._site = aSite;
  this._node = this._site.node;

  // listen for 'dragstart'
  this._dragstart = this._dragstart.bind(this);
  this._node.addEventListener("dragstart", this._dragstart.bind(this), false);
}

DragHandler.prototype = {
  _dragstart: function DragHandler__dragstart(aEvent) {
    let node = this._node;

    // TODO
    node.classList.add("ontop");
    document.body.classList.add("dragging");

    let self = this;

    Transformation.dragSite(this._site, aEvent, function () {
      node.classList.remove("ontop");
      document.body.classList.remove("dragging");
      document.body.removeChild(self._dragImage);
    });

    this._setDragData(aEvent);

    return true;
  },

  _setDragData: function DragHandler__setDragData(aEvent) {
    let {url, title} = this._site;

    let dt = aEvent.dataTransfer;
    dt.setData("text/plain", url);
    dt.setData("text/uri-list", url);
    dt.setData("text/x-moz-url", url + "\n" + title);
    dt.setData("text/html", "<a href=\"" + url + "\">" + url + "</a>");

    // create and use an empty drag image
    let img = this._dragImage = document.createElement("div");
    img.classList.add("drag-img");
    document.body.appendChild(img);
    dt.setDragImage(img, 0, 0);

    dt.effectAllowed = "move";
  }
};
