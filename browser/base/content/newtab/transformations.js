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
let Transformation = {
  blockSite: function Trans_blockSite(aSite, aCallback) {
    return new BlockSiteTransformation(aSite, aCallback);
  },

  unpinSite: function Trans_unpinSite(aSite, aCallback) {
    return new UnpinSiteTransformation(aSite, aCallback);
  },

  dragSite: function Trans_dragSite(aSite, aCell, aCallback) {
    return new DragSiteTransformation(aSite, aCell, aCallback);
  }
};

// TODO
let BaseTransformation = {
  _getNodePosition: function BaseTrans__getNodePosition(aNode) {
    let rect = aNode.getBoundingClientRect();
    return new Rect(scrollX + rect.left, scrollY + rect.top,
                    rect.width, rect.height);
  },

  _setSitePosition: function BaseTrans__setSitePosition(aSite, aPosition) {
    let style = aSite.node.style;
    let {top, left} = aPosition;

    style.top = top + "px";
    style.left = left + "px";
  },

  _freezeSitePosition: function BaseTrans__freezeSitePosition(aSite) {
    this._setSitePosition(aSite, this._getNodePosition(aSite.node));
    aSite.node.style.position = "absolute";
  },

  _unfreezeSitePosition: function BaseTrans__unfreezeSitePosition(aSite) {
    let style = aSite.node.style;
    style.top = style.left = style.position = "";
  },

  _slideSiteTo: function BaseTrans__slideSiteTo(aSite, aTarget, aCallback) {
    this._setSitePosition(aSite, this._getNodePosition(aTarget.node));

    if (aCallback)
      this._whenTransitionEnded(aSite.node, aCallback);
  },

  _whenTransitionEnded: function BaseTrans__whenTransitionEnded(aNode, aCallback) {
    aNode.addEventListener("transitionend", function onEnd() {
      aNode.removeEventListener("transitionend", onEnd, false);
      aCallback();
    }, false);
  },

  _lock: function BaseTrans__lock() {
    NewTabUtils.Pages.lock();
  },

  _unlock: function BaseTrans__unlock() {
    NewTabUtils.Pages.unlock();
  }
};

// TODO
function BlockSiteTransformation(aSite, aCallback) {
  this._site = aSite;
  this._callback = aCallback;

  this._start();
}

BlockSiteTransformation.prototype = extend(BaseTransformation, {
  _start: function BSTrans__start() {
    this._lock();

    let cells = Grid.cells.slice(this._site.index);

    // get all unpinned and non-empty cells
    this._movableCells = cells.filter(function (cell) {
      return !cell.isEmpty() && !cell.containsPinnedSite();
    });

    this._movableSites = this._movableCells.map(function (cell) cell.site);

    // freeze the site positions
    this._movableSites.forEach(this._freezeSitePosition, this);

    // fade out the blocked site and continue with step 2
    Animations.fadeOut(this._site.node, this._step2.bind(this));
  },

  // TODO better name
  _step2: function BSTrans__step2() {
    let movableCells = this._movableCells;
    let movableSites = this._movableSites;

    let numCells = movableCells.length;
    let numSlides = numCells - 1;

    let self = this;

    function slideFinished() {
      if (!--numSlides)
        self._step3();
    }

    for (let i = 1; i < numCells; i++) {
      let targetSite = movableSites[i - 1];
      let targetCell = movableCells[i - 1];
      let site = movableSites[i];

      // move site the its new DOM position
      targetCell.node.appendChild(site.node);

      // slide site into its new position
      this._slideSiteTo(site, targetSite, slideFinished);
    }

    // remove the blocked site's DOM node
    let node = this._site.node;
    node.parentNode.removeChild(node);

    // no cells will be slided around, go to step 3
    if (numCells < 2)
      this._step3();
  },

  // TODO better name
  _step3: function BSTrans__step3() {
    let movableCells = this._movableCells;
    let lastCell = movableCells[movableCells.length - 1];
    let index = Grid.cells.indexOf(lastCell);

    // unfreeze the site positions
    this._movableSites.forEach(this._unfreezeSitePosition, this);

    let self = this;

    // find the next link that will appear
    Links.getLinks(function (links) {
      if (links[index]) {
        let site = Grid.createSite(links[index], lastCell);
        Animations.fadeIn(site.node, self._finish.bind(self));
      } else {
        self._finish();
      }
    });
  },

  _finish: function BSTrans__finish() {
    this._unlock();

    if (this._callback)
      this._callback.callback();
  }
});

// TODO
function UnpinSiteTransformation(aSite, aCallback) {
  this._site = aSite;
  this._callback = aCallback;

  this._start();
}

UnpinSiteTransformation.prototype = extend(BaseTransformation, {
  _start: function USTrans__start() {
    let firstEmptyCell = this._findFirstEmptyCell();

    if (firstEmptyCell) {
      this._moveSiteToEmptyCell(firstEmptyCell);
    } else {
      // no empty cells before the unpinned cell
      this._finish();
    }
  },

  _findFirstEmptyCell: function USTrans__findFirstEmptyCell() {
    let cells = Grid.cells;
    let maxIndex = this._site.index;

    for (let i = 0; i < maxIndex; i++) {
      let cell = cells[i];

      if (cell.isEmpty())
        return cell;
    }

    return null;
  },

  _moveSiteToEmptyCell: function USTrans__moveSiteToEmptyCell(aCell) {
    this._lock();

    // freeze our site position
    this._freezeSitePosition(this._site);

    // move site the its new DOM position
    aCell.node.appendChild(this._site.node);

    let self = this;

    // slide site into its new position
    this._slideSiteTo(this._site, aCell, function () {
      self._unfreezeSitePosition(self._site);
      self._finish();
    });
  },

  _finish: function USTrans__finish() {
    this._unlock();

    if (this._callback)
      this._callback();
  }
});

// TODO
function DragSiteTransformation(aSite, aEvent, aCallback) {
  this._site = aSite;
  this._callback = aCallback;

  this._start(aEvent);
}

DragSiteTransformation.prototype = extend(BaseTransformation, {
  get _cellPositions() {
    let excludeCell = this._site.cell;

    let cells = Grid.cells.filter(function (cell) {
      return cell != excludeCell;
    });

    let self = this;

    let cellPositions = cells.map(function (cell) {
      return {cell: cell, rect: self._getNodePosition(cell.node)};
    });

    // replace the getter with our cached value
    let getter = function DSTrans__getCellPositions() cellPositions;
    this.__defineGetter__("_cellPositions", getter);
    return cellPositions;
  },

  _start: function DSTrans__start(aEvent) {
    // listen for 'dragend'
    let node = this._site.node;
    this._dragend = this._dragend.bind(this);
    node.addEventListener("dragend", this._dragend, false);

    // listen for 'dragover'
    this._dragover = this._dragover.bind(this);
    document.addEventListener("dragover", this._dragover, false);

    // store cursor offset
    let rect = node.getBoundingClientRect();
    this._offsetX = aEvent.clientX - rect.left;
    this._offsetY = aEvent.clientY - rect.top;

    // store cell dimensions
    let cellNode = this._site.cellNode;
    this._cellWidth = cellNode.offsetWidth;
    this._cellHeight = cellNode.offsetHeight;

    // store minimum intersection values
    this._minWidth = this._cellWidth / 2;
    this._minHeight = this._cellHeight / 2;

    // TODO
    let node = this._site.node;
    node.classList.add("dragged");

    this._freezeSitePosition(this._site);
  },

  _dragover: function DSTrans__dragover(aEvent) {
    // get the viewport size
    let {clientWidth, clientHeight} = document.documentElement;

    // we'll want a padding of 5px
    let border = 5;

    // TODO add some good variable names
    // enforce minimum constraints to keep the drag image inside the window
    let left = Math.max(scrollX + aEvent.clientX - this._offsetX, border);
    let top = Math.max(scrollY + aEvent.clientY - this._offsetY, border);

    // TODO add some good variable names
    // enforce maximum constraints to keep the drag image inside the window
    left = Math.min(left, scrollX + clientWidth - this._cellWidth - border);
    top = Math.min(top, scrollY + clientHeight - this._cellHeight - border);

    // update the drag image's position
    this._setSitePosition(this._site, {left: left, top: top});

    // find a possible drop target for the current position
    let targetCell = this._findDropTarget(aEvent, this._site.cell);

    // TODO
    if (this._lastDropTarget != targetCell) {
      if (this._rearrangeTimeout)
        clearTimeout(this._rearrangeTimeout);

      let self = this;
      let callback = function () self._rearrange(targetCell);
      this._rearrangeTimeout = setTimeout(callback, 250);
    }

    this._lastDropTarget = targetCell;
  },

  _rearrange: function DSTrans__rearrange(aTarget) {
    let targetSite = aTarget && aTarget.site;
    let lastTargetSite = this._lastTarget && this._lastTarget.site;

    if (lastTargetSite)
      // TODO finish must not be called before this has finished
      this._slideSiteTo(lastTargetSite, this._lastTarget);

    if (targetSite) {
      this._freezeSitePosition(targetSite);
      // TODO finish must not be called before this has finished
      this._slideSiteTo(targetSite, this._site.cell);
    }

    this._lastTarget = aTarget;
  },

  _findDropTarget: function DSTrans__findDropTarget(aEvent) {
    let {_minWidth: minWidth, _minHeight: minHeight} = this;

    let rect = new Rect(aEvent.clientX - this._offsetX,
                        aEvent.clientY - this._offsetY,
                        this._cellWidth, this._cellHeight);

    let targetCell;

    this._cellPositions.some(function (pos) {
      let inter = rect.intersect(pos.rect);

      if (inter.width >= minWidth && inter.height >= minHeight) {
        targetCell = pos.cell;
        return true;
      }

      return false;
    });

    return targetCell;
  },


  _dragend: function DSTrans__dragend(aEvent) {
    // remove 'dragend' listener
    let node = this._site.node;
    node.removeEventListener("dragend", this._dragend, false);

    // remove 'dragover' listener
    document.removeEventListener("dragover", this._dragover, false);

    // TODO
    if (this._rearrangeTimeout)
      clearTimeout(this._rearrangeTimeout);

    // TODO
    node.classList.remove("dragged");

    this._lock();

    let finish = this._finish.bind(this);

    if (this._lastDropTarget) {
      let targetSite = this._lastDropTarget.site;

      if (targetSite) {
        // TODO
        this._freezeSitePosition(targetSite);
        this._site.cellNode.appendChild(targetSite.node);
        this._slideSiteTo(targetSite, this._site.cell);

        // TODO
        targetSite.unpin();
        targetSite.pin();
      } else {
        let cell = this._site.cell;

        // if we dragged the site onto an empty cell there might now be a gap
        // in the grid and we have to pin all cells after the gap
        while (cell = cell.nextSibling) {
          let site = cell.site;
          site && site.pin();
        }
      }

      // slide the dragged site into its place and pin it
      this._lastDropTarget.node.appendChild(this._site.node);
      this._slideSiteTo(this._site, this._lastDropTarget, finish);

      // TODO
      this._site.unpin();
      this._site.pin();
    } else {
      // snap back to original place
      this._slideSiteTo(this._site, this._site.cell, finish);
    }
  },

  _finish: function DSTrans__finish() {
    let self = this;

    // unfreeze all sites
    Grid.cells.forEach(function (cell) {
      let site = cell.site;
      if (site)
        self._unfreezeSitePosition(site);
    });

    this._unlock();

    if (this._callback)
      this._callback();
  }
});
