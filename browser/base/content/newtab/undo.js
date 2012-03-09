#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * Dialog allowing to undo the removal of single site or to completely restore
 * the grid's original state.
 */
let gUndoDialog = {
  /**
   * The undo dialog's timeout in miliseconds.
   */
  HIDE_TIMEOUT_MS: 15000,

  /**
   * Contains undo information.
   */
  _undoData: null,

  /**
   * Initializes the undo dialog.
   */
  init: function UndoDialog_init() {
    this._node = document.getElementById("newtab-undo");
    this._node.addEventListener("click", this, false);
  },

  /**
   * Shows the undo dialog.
   * @param aSite The site that just got removed.
   */
  show: function UndoDialog_show(aSite) {
    if (this._undoData)
      clearTimeout(this._undoData.timeout);

    this._undoData = {
      index: aSite.cell.index,
      wasPinned: aSite.isPinned(),
      blockedLink: aSite.link,
      timeout: setTimeout(this.hide.bind(this), this.HIDE_TIMEOUT_MS)
    };

    this._node.removeAttribute("tabindex");
  },

  /**
   * The undo dialog event handler.
   * @param aEvent The event to handle.
   */
  handleEvent: function UndoDialog_handleEvent(aEvent) {
    if (!this._undoData)
      return;

    let {index, wasPinned, blockedLink} = this._undoData;
    gBlockedLinks.unblock(blockedLink);

    if (wasPinned)
      gPinnedLinks.pin(blockedLink, index);

    gUpdater.updateGrid();
    this.hide();
  },

  /**
   * Hides the undo dialog.
   */
  hide: function UndoDialog_hide() {
    if (!this._undoData)
      return;

    clearTimeout(this._undoData.timeout);
    this._undoData = null;
    this._node.setAttribute("tabindex", "-1");
  }
};

gUndoDialog.init();
