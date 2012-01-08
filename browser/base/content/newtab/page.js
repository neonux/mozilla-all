#ifdef 0
/*
 * This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/.
 */
#endif

/**
 * This singleton represents the whole 'New Tab Page' and takes care of
 * initializing all its components.
 */
let gPage = {
  /**
   * Initializes the page.
   * @param aToolbarSelector The query selector for the page toolbar.
   * @param aGridSelector The query selector for the grid.
   */
  init: function Page_init(aToolbarSelector, aGridSelector) {
    gToolbar.init(aToolbarSelector);
    this._gridSelector = aGridSelector;

    // Add ourselves to the list of pages to receive notifications.
    gAllPages.register(this);

    // Listen for 'unload' to unregister this page.
    function unload() gAllPages.unregister(self);
    addEventListener("unload", unload, false);

    // Check if the new tab feature is enabled.
    if (gAllPages.enabled)
      this._init();
    else
      this._updateAttributes(false);
  },

  /**
   * Listens for notifications specific to this page.
   */
  observe: function Page_observe() {
    let enabled = gAllPages.enabled;
    this._updateAttributes(enabled);

    // Initialize the whole page if we haven't done that, yet.
    if (enabled)
      this._init();
  },

  /**
   * Updates the whole page and the grid when the storage has changed.
   */
  update: function Page_update() {
    this.checkIfModified();
    gGrid.refresh();
  },

  /**
   * Checks if the page is modified and sets the CSS class accordingly
   */
  checkIfModified: function Page_checkIfModified() {
    // The page is considered modified only if sites have been removed.
    this._setModified(!gBlockedLinks.isEmpty());
  },

  /**
   * Internally initializes the page. This runs only when/if the feature
   * is/gets enabled.
   */
  _init: function Page_init() {
    if (this._initialized)
      return;

    this._initialized = true;

    let self = this;

    // Check if the grid is modified.
    this.checkIfModified();

    // Initialize and render the grid.
    gGrid.init(this._gridSelector);

    // Initialize the drop target shim.
    gDropTargetShim.init();

    // Workaround to prevent a delay on MacOSX due to a slow drop animation.
    let doc = document.documentElement;
    doc.addEventListener("dragover", this._onDragOver, false);
    doc.addEventListener("drop", this._onDrop, false);
  },

  /**
   * Updates the 'page-disabled' attributes of the respective DOM nodes.
   * @param aValue Whether to set or remove attributes.
   */
  _updateAttributes: function Page_updateAttributes(aValue) {
    let nodes = document.querySelectorAll("#grid, #vbox, #toolbar, .toolbar-button");

    // Set the nodes' states.
    for (let i = 0; i < nodes.length; i++) {
      let node = nodes[i];
      if (aValue)
        node.removeAttribute("page-disabled");
      else
        node.setAttribute("page-disabled", "true");
    }
  },

  /**
   * Sets the modified status of the New Tab Page.
   * @param aValue Whether the page is modified or not.
   */
  _setModified: function Page_setModified(aValue) {
    let node = document.getElementById("toolbar-button-reset");

    if (aValue)
      node.setAttribute("modified", "true");
    else
      node.removeAttribute("modified");
  },

  /**
   * Handles the 'dragover' event. Workaround to prevent a delay on MacOSX
   * due to a slow drop animation.
   * @param aEvent The 'dragover' event.
   */
  _onDragOver: function Page_onDragOver(aEvent) {
    if (gDrag.isValid(aEvent))
      aEvent.preventDefault();
  },

  /**
   * Handles the 'drop' event. Workaround to prevent a delay on MacOSX due to
   * a slow drop animation.
   * @param aEvent The 'drop' event.
   */
  _onDrop: function Page_onDrop(aEvent) {
    if (gDrag.isValid(aEvent)) {
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  }
};
