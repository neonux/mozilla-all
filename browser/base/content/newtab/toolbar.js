#ifdef 0
/*
 * This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/.
 */
#endif

/**
 * This singleton represents the page's toolbar that allows to enable/disable
 * the 'New Tab Page' feature and to reset the whole page.
 */
let gToolbar = {
  /**
   * Initializes the toolbar.
   * @param aSelector The query selector of the toolbar.
   */
  init: function Toolbar_init(aSelector) {
    this._node = document.querySelector(aSelector);
    let buttons = this._node.querySelectorAll("input");

    // Listen for 'click' events on the toolbar buttons.
    ["show", "hide", "reset"].forEach(function (aType, aIndex) {
      let self = this;
      let button = buttons[aIndex];
      let handler = function () self[aType]();

      button.addEventListener("click", handler, false);
    }, this);
  },

  /**
   * Enables the 'New Tab Page' feature.
   */
  show: function Toolbar_show() {
    gAllPages.enabled = true;
  },

  /**
   * Disables the 'New Tab Page' feature.
   */
  hide: function Toolbar_hide() {
    gAllPages.enabled = false;
  },

  /**
   * Resets the whole page and forces it to re-render its content.
   * @param aCallback The callback to call when the page has been reset.
   */
  reset: function Toolbar_reset(aCallback) {
    let node = gGrid.node;

    // animate the page reset
    gTransformation.fadeNodeOut(node, function () {
      NewTabUtils.reset();

      gLinks.populateCache(function () {
        gAllPages.update();

        // Without the setTimeout() we have a strange flicker.
        setTimeout(function () gTransformation.fadeNodeIn(node, aCallback));
      });
    });
  }
};

