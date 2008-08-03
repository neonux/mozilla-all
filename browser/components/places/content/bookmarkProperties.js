/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Places Bookmark Properties dialog.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hughes <jhughes@google.com>
 *   Dietrich Ayala <dietrich@mozilla.com>
 *   Asaf Romano <mano@mozilla.com>
 *   Marco Bonardo <mak77@supereva.it>
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

/**
 * The panel is initialized based on data given in the js object passed
 * as window.arguments[0]. The object must have the following fields set:
 *   @ action (String). Possible values:
 *     - "add" - for adding a new item.
 *       @ type (String). Possible values:
 *         - "bookmark"
 *           @ loadBookmarkInSidebar - optional, the default state for the
 *             "Load this bookmark in the sidebar" field.
 *         - "folder"
 *           @ URIList (Array of nsIURI objects) - optional, list of uris to
 *             be bookmarked under the new folder.
 *         - "livemark"
 *       @ uri (nsIURI object) - optional, the default uri for the new item.
 *         The property is not used for the "folder with items" type.
 *       @ title (String) - optional, the defualt title for the new item.
 *       @ description (String) - optional, the default description for the new
 *         item.
 *       @ defaultInsertionPoint (InsertionPoint JS object) - optional, the
 *         default insertion point for the new item.
 *       @ keyword (String) - optional, the default keyword for the new item.
 *       @ postData (String) - optional, POST data to accompany the keyword.
 *      Notes:
 *        1) If |uri| is set for a bookmark/livemark item and |title| isn't,
 *           the dialog will query the history tables for the title associated
 *           with the given uri. If the dialog is set to adding a folder with
 *           bookmark items under it (see URIList), a default static title is
 *           used ("[Folder Name]").
 *        2) The index field of the the default insertion point is ignored if
 *           the folder picker is shown.
 *     - "edit" - for editing a bookmark item or a folder.
 *       @ type (String). Possible values:
 *         - "bookmark"
 *           @ itemId (Integer) - the id of the bookmark item.
 *         - "folder" (also applies to livemarks)
 *           @ itemId (Integer) - the id of the folder.
 *   @ hiddenRows (Strings array) - optional, list of rows to be hidden
 *     regardless of the item edited or added by the dialog.
 *     Possible values:
 *     - "title"
 *     - "location"
 *     - "description"
 *     - "keyword"
 *     - "loadInSidebar"
 *     - "feedURI"
 *     - "siteURI"
 *     - "folder picker" - hides both the tree and the menu.
 *
 * window.arguments[0].performed is set to true if any transaction has
 * been performed by the dialog.
 */

const LAST_USED_ANNO = "bookmarkPropertiesDialog/folderLastUsed";
const STATIC_TITLE_ANNO = "bookmarks/staticTitle";

// This doesn't include "static" special folders (first two menu items)
const MAX_FOLDER_ITEM_IN_MENU_LIST = 5;

const BOOKMARK_ITEM = 0;
const BOOKMARK_FOLDER = 1;
const LIVEMARK_CONTAINER = 2;

const ACTION_EDIT = 0;
const ACTION_ADD = 1;

var BookmarkPropertiesPanel = {

  /** UI Text Strings */
  __strings: null,
  get _strings() {
    if (!this.__strings) {
      this.__strings = document.getElementById("stringBundle");
    }
    return this.__strings;
  },

  _action: null,
  _itemType: null,
  _itemId: -1,
  _uri: null,
  _loadBookmarkInSidebar: false,
  _itemTitle: "",
  _itemDescription: "",
  _microsummaries: null,
  _URIList: null,
  _postData: null,
  _charSet: "",

  // sizeToContent is not usable due to bug 90276, so we'll use resizeTo
  // instead and cache the bookmarks tree view size. See WSucks in the legacy
  // UI code (addBookmark2.js).
  //
  // XXXmano: this doesn't work as expected yet, need to figure out if we're
  // facing cocoa-widget resizeTo issue here.
  _folderTreeHeight: null,

  /**
   * This method returns the correct label for the dialog's "accept"
   * button based on the variant of the dialog.
   */
  _getAcceptLabel: function BPP__getAcceptLabel() {
    if (this._action == ACTION_ADD) {
      if (this._URIList)
        return this._strings.getString("dialogAcceptLabelAddMulti");

      return this._strings.getString("dialogAcceptLabelAddItem");
    }
    return this._strings.getString("dialogAcceptLabelEdit");
  },

  /**
   * This method returns the correct title for the current variant
   * of this dialog.
   */
  _getDialogTitle: function BPP__getDialogTitle() {
    if (this._action == ACTION_ADD) {
      if (this._itemType == BOOKMARK_ITEM)
        return this._strings.getString("dialogTitleAddBookmark");
      if (this._itemType == LIVEMARK_CONTAINER)
        return this._strings.getString("dialogTitleAddLivemark");

      // folder
      NS_ASSERT(this._itemType == BOOKMARK_FOLDER, "bogus item type");
      if (this._URIList)
        return this._strings.getString("dialogTitleAddMulti");

      return this._strings.getString("dialogTitleAddFolder");
    }
    if (this._action == ACTION_EDIT) {
      return this._strings
                 .getFormattedString("dialogTitleEdit", [this._itemTitle]);
    }
    return "";
  },

  /**
   * Determines the initial data for the item edited or added by this dialog
   */
  _determineItemInfo: function BPP__determineItemInfo() {
    var dialogInfo = window.arguments[0];
    NS_ASSERT("action" in dialogInfo, "missing action property");
    var action = dialogInfo.action;

    if (action == "add") {
      NS_ASSERT("type" in dialogInfo, "missing type property for add action");

      if ("title" in dialogInfo)
        this._itemTitle = dialogInfo.title;
      if ("defaultInsertionPoint" in dialogInfo)
        this._defaultInsertionPoint = dialogInfo.defaultInsertionPoint;
      else {
        // default to the bookmarks root
        this._defaultInsertionPoint =
          new InsertionPoint(PlacesUtils.bookmarksMenuFolderId, -1);
      }

      switch(dialogInfo.type) {
        case "bookmark":
          this._action = ACTION_ADD;
          this._itemType = BOOKMARK_ITEM;
          if ("uri" in dialogInfo) {
            NS_ASSERT(dialogInfo.uri instanceof Ci.nsIURI,
                      "uri property should be a uri object");
            this._uri = dialogInfo.uri;
          }
          if (typeof(this._itemTitle) != "string") {
            if (this._uri) {
              this._itemTitle =
                this._getURITitleFromHistory(this._uri);
              if (!this._itemTitle)
                this._itemTitle = this._uri.spec;
            }
            else
              this._itemTitle = this._strings.getString("newBookmarkDefault");
          }

          if ("loadBookmarkInSidebar" in dialogInfo)
            this._loadBookmarkInSidebar = dialogInfo.loadBookmarkInSidebar;

          if ("keyword" in dialogInfo) {
            this._bookmarkKeyword = dialogInfo.keyword;
            if ("postData" in dialogInfo)
              this._postData = dialogInfo.postData;
            if ("charSet" in dialogInfo)
              this._charSet = dialogInfo.charSet;
          }

          break;
        case "folder":
          this._action = ACTION_ADD;
          this._itemType = BOOKMARK_FOLDER;
          if (!this._itemTitle) {
            if ("URIList" in dialogInfo) {
              this._itemTitle =
                this._strings.getString("bookmarkAllTabsDefault");
              this._URIList = dialogInfo.URIList;
            }
            else
              this._itemTitle = this._strings.getString("newFolderDefault");
          }
          break;
        case "livemark":
          this._action = ACTION_ADD;
          this._itemType = LIVEMARK_CONTAINER;
          if ("feedURI" in dialogInfo)
            this._feedURI = dialogInfo.feedURI;
          if ("siteURI" in dialogInfo)
            this._siteURI = dialogInfo.siteURI;

          if (!this._itemTitle) {
            if (this._feedURI) {
              this._itemTitle =
                this._getURITitleFromHistory(this._feedURI);
              if (!this._itemTitle)
                this._itemTitle = this._feedURI.spec;
            }
            else
              this._itemTitle = this._strings.getString("newLivemarkDefault");
          }
      }

      if ("description" in dialogInfo)
        this._itemDescription = dialogInfo.description;
    }
    else { // edit
      const annos = PlacesUtils.annotations;
      const bookmarks = PlacesUtils.bookmarks;

      switch (dialogInfo.type) {
        case "bookmark":
          NS_ASSERT("itemId" in dialogInfo);

          this._action = ACTION_EDIT;
          this._itemType = BOOKMARK_ITEM;
          this._itemId = dialogInfo.itemId;

          this._uri = bookmarks.getBookmarkURI(this._itemId);
          this._itemTitle = bookmarks.getItemTitle(this._itemId);

          // keyword
          this._bookmarkKeyword =
            bookmarks.getKeywordForBookmark(this._itemId);

          // Load In Sidebar
          this._loadBookmarkInSidebar =
            annos.itemHasAnnotation(this._itemId, LOAD_IN_SIDEBAR_ANNO);

          break;
        case "folder":
          NS_ASSERT("itemId" in dialogInfo);

          this._action = ACTION_EDIT;
          this._itemId = dialogInfo.itemId;

          const livemarks = PlacesUtils.livemarks;
          if (livemarks.isLivemark(this._itemId)) {
            this._itemType = LIVEMARK_CONTAINER;
            this._feedURI = livemarks.getFeedURI(this._itemId);
            this._siteURI = livemarks.getSiteURI(this._itemId);
          }
          else
            this._itemType = BOOKMARK_FOLDER;
          this._itemTitle = bookmarks.getItemTitle(this._itemId);
          break;
      }

      // Description
      if (annos.itemHasAnnotation(this._itemId, DESCRIPTION_ANNO)) {
        this._itemDescription = annos.getItemAnnotation(this._itemId,
                                                        DESCRIPTION_ANNO);
      }
    }
  },

  /**
   * This method returns the title string corresponding to a given URI.
   * If none is available from the bookmark service (probably because
   * the given URI doesn't appear in bookmarks or history), we synthesize
   * a title from the first 100 characters of the URI.
   *
   * @param aURI
   *        nsIURI object for which we want the title
   *
   * @returns a title string
   */
  _getURITitleFromHistory: function BPP__getURITitleFromHistory(aURI) {
    NS_ASSERT(aURI instanceof Ci.nsIURI);

    // get the title from History
    return PlacesUtils.history.getPageTitle(aURI);
  },

  /**
   * This method should be called by the onload of the Bookmark Properties
   * dialog to initialize the state of the panel.
   */
  onDialogLoad: function BPP_onDialogLoad() {
    this._determineItemInfo();
    this._populateProperties();
    this.validateChanges();

    this._folderMenuList = this._element("folderMenuList");
    this._folderTree = this._element("folderTree");
    if (!this._element("folderRow").hidden)
      this._initFolderMenuList();

    window.sizeToContent();

    // read the persisted attribute.
    this._folderTreeHeight = parseInt(this._folderTree.getAttribute("height"));
  },

  /**
   * Appends a menu-item representing a bookmarks folder to a menu-popup.
   * @param aMenupopup
   *        The popup to which the menu-item should be added.
   * @param aFolderId
   *        The identifier of the bookmarks folder.
   * @return the new menu item.
   */
  _appendFolderItemToMenupopup:
  function BPP__appendFolderItemToMenupopup(aMenupopup, aFolderId) {
    try {
      var folderTitle = PlacesUtils.bookmarks.getItemTitle(aFolderId);
    }
    catch (ex) {
      NS_ASSERT(folderTitle, "no title found for folderId of " + aFolderId);
      return null;
    }

    // First make sure the folders-separator is visible
    this._element("foldersSeparator").hidden = false;

    var folderMenuItem = document.createElement("menuitem");
    folderMenuItem.folderId = aFolderId;
    folderMenuItem.setAttribute("label", folderTitle);
    folderMenuItem.className = "menuitem-iconic folder-icon";
    aMenupopup.appendChild(folderMenuItem);
    return folderMenuItem;
  },

  _initFolderMenuList: function BPP__initFolderMenuList() {
    // Build the static list
    var bms = PlacesUtils.bookmarks;
    var bmMenuItem = this._element("bookmarksRootItem");
    bmMenuItem.label = bms.getItemTitle(PlacesUtils.bookmarksMenuFolderId);
    bmMenuItem.folderId = PlacesUtils.bookmarksMenuFolderId;
    var toolbarItem = this._element("toolbarFolderItem");
    toolbarItem.label = bms.getItemTitle(PlacesUtils.toolbarFolderId);
    toolbarItem.folderId = PlacesUtils.toolbarFolderId;

    // List of recently used folders:
    var annos = PlacesUtils.annotations;
    var folderIds = annos.getItemsWithAnnotation(LAST_USED_ANNO, { });

    // Hide the folders-separator if no folder is annotated as recently-used
    if (folderIds.length == 0) {
      this._element("foldersSeparator").hidden = true;
      return;
    }

    /**
     * The value of the LAST_USED_ANNO annotation is the time (in the form of
     * Date.getTime) at which the folder has been last used.
     *
     * First we build the annotated folders array, each item has both the
     * folder identifier and the time at which it was last-used by this dialog
     * set. Then we sort it descendingly based on the time field.
     */
    var folders = [];
    for (var i=0; i < folderIds.length; i++) {
      var lastUsed = annos.getItemAnnotation(folderIds[i], LAST_USED_ANNO);
      folders.push({ folderId: folderIds[i], lastUsed: lastUsed });
    }
    folders.sort(function(a, b) {
      if (b.lastUsed < a.lastUsed)
        return -1;
      if (b.lastUsed > a.lastUsed)
        return 1;
      return 0;
    });

    var numberOfItems = Math.min(MAX_FOLDER_ITEM_IN_MENU_LIST, folders.length);
    var menupopup = this._folderMenuList.menupopup;
    for (i=0; i < numberOfItems; i++) {
      this._appendFolderItemToMenupopup(menupopup, folders[i].folderId);
    }

    var defaultItem =
      this._getFolderMenuItem(this._defaultInsertionPoint.itemId);

    // if we fail to get a menuitem for the default insertion point
    // use the Bookmarks root
    if (!defaultItem)
      defaultItem = this._element("bookmarksRootItem");

    this._folderMenuList.selectedItem = defaultItem;
  },

  QueryInterface: function BPP_QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIMicrosummaryObserver) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  _element: function BPP__element(aID) {
    return document.getElementById(aID);
  },

  /**
   * Show or hides fields based on item type.
   */
  _showHideRows: function BPP__showHideRows() {
    var hiddenRows = window.arguments[0].hiddenRows || new Array();

    var isBookmark = this._itemType == BOOKMARK_ITEM;
    var isLivemark = this._itemType == LIVEMARK_CONTAINER;

    var isQuery = false;
    if (this._uri)
      isQuery = this._uri.schemeIs("place");

    this._element("namePicker").hidden =
      hiddenRows.indexOf("title") != -1;
    this._element("locationRow").hidden =
      hiddenRows.indexOf("location") != -1 || isQuery || !isBookmark;
    this._element("keywordRow").hidden =
      hiddenRows.indexOf("keyword") != -1 || isQuery || !isBookmark;
    this._element("descriptionRow").hidden =
      hiddenRows.indexOf("description")!= -1
    this._element("folderRow").hidden =
      hiddenRows.indexOf("folder picker") != -1 || this._action == ACTION_EDIT;
    this._element("livemarkFeedLocationRow").hidden =
      hiddenRows.indexOf("feedURI") != -1 || !isLivemark;
    this._element("livemarkSiteLocationRow").hidden =
      hiddenRows.indexOf("siteURI") != -1 || !isLivemark;
    this._element("loadInSidebarCheckbox").hidden =
      hiddenRows.indexOf("loadInSidebar") != -1 || isQuery || !isBookmark;
  },

  /**
   * This method fills in the data values for the fields in the dialog.
   */
  _populateProperties: function BPP__populateProperties() {
    document.title = this._getDialogTitle();
    document.documentElement.getButton("accept").label = this._getAcceptLabel();

    this._initNamePicker();
    this._element("descriptionTextfield").value = this._itemDescription;

    if (this._itemType == BOOKMARK_ITEM) {
      if (this._uri)
        this._element("editURLBar").value = this._uri.spec;

      if (typeof(this._bookmarkKeyword) == "string")
        this._element("keywordTextfield").value = this._bookmarkKeyword;

      if (this._loadBookmarkInSidebar)
        this._element("loadInSidebarCheckbox").checked = true;
    }

    if (this._itemType == LIVEMARK_CONTAINER) {
      if (this._feedURI)
        this._element("feedLocationTextfield").value = this._feedURI.spec;
      if (this._siteURI)
        this._element("feedSiteLocationTextfield").value = this._siteURI.spec;
    }

    this._showHideRows();
  },

  _createMicrosummaryMenuItem:
  function BPP__createMicrosummaryMenuItem(aMicrosummary) {
    var menuItem = document.createElement("menuitem");

    // Store a reference to the microsummary in the menu item, so we know
    // which microsummary this menu item represents when it's time to
    // save changes or load its content.
    menuItem.microsummary = aMicrosummary;

    // Content may have to be generated asynchronously; we don't necessarily
    // have it now.  If we do, great; otherwise, fall back to the generator
    // name, then the URI, and we trigger a microsummary content update. Once
    // the update completes, the microsummary will notify our observer to
    // update the corresponding menu-item.
    // XXX Instead of just showing the generator name or (heaven forbid)
    // its URI when we don't have content, we should tell the user that
    // we're loading the microsummary, perhaps with some throbbing to let
    // her know it is in progress.
    if (aMicrosummary.content)
      menuItem.setAttribute("label", aMicrosummary.content);
    else {
      menuItem.setAttribute("label", aMicrosummary.generator.name ||
                                     aMicrosummary.generator.uri.spec);
      aMicrosummary.update();
    }

    return menuItem;
  },

  _initNamePicker: function BPP_initNamePicker() {
    var userEnteredNameField = this._element("userEnteredName");
    var namePicker = this._element("namePicker");
    const annos = PlacesUtils.annotations;

    if (annos.itemHasAnnotation(this._itemId, STATIC_TITLE_ANNO)) {
      userEnteredNameField.label = annos.getItemAnnotation(this._itemId,
                                                           STATIC_TITLE_ANNO);
    }
    else
      userEnteredNameField.label = this._itemTitle;

    // Non-bookmark items always use the item-title itself
    if (this._itemType != BOOKMARK_ITEM || !this._uri) {
      namePicker.selectedItem = userEnteredNameField;
      return;
    }

    var itemToSelect = userEnteredNameField;
    try {
      this._microsummaries =
        PlacesUIUtils.microsummaries.getMicrosummaries(this._uri,
                                                       this._itemId);
    }
    catch(ex) {
      // getMicrosummaries will throw an exception if the page to which the URI
      // refers isn't HTML or XML (the only two content types the service knows
      // how to summarize).
      this._microsummaries = null;
    }
    if (this._microsummaries) {
      var enumerator = this._microsummaries.Enumerate();

      if (enumerator.hasMoreElements()) {
        // Show the drop marker if there are microsummaries
        namePicker.setAttribute("droppable", "true");

        var menupopup = namePicker.menupopup;
        while (enumerator.hasMoreElements()) {
          var microsummary = enumerator.getNext()
                                       .QueryInterface(Ci.nsIMicrosummary);
          var menuItem = this._createMicrosummaryMenuItem(microsummary);

          if (this._action == ACTION_EDIT &&
              PlacesUIUtils.microsummaries
                           .isMicrosummary(this._itemId, microsummary))
            itemToSelect = menuItem;

          menupopup.appendChild(menuItem);
        }
      }

      this._microsummaries.addObserver(this);
    }

    namePicker.selectedItem = itemToSelect;
  },

  // nsIMicrosummaryObserver
  onContentLoaded: function BPP_onContentLoaded(aMicrosummary) {
    var namePicker = this._element("namePicker");
    var childNodes = namePicker.menupopup.childNodes;

    // 0: user-entered item; 1: separator
    for (var i = 2; i < childNodes.length; i++) {
      if (childNodes[i].microsummary == aMicrosummary) {
        var newLabel = aMicrosummary.content;
        // XXXmano: non-editable menulist would do this for us, see bug 360220
        // We should fix editable-menulsits to set the DOMAttrModified handler
        // as well.
        //
        // Also note the order importance: if the label of the menu-item is
        // set the something different than the menulist's current value,
        // the menulist no longer has selectedItem set
        if (namePicker.selectedItem == childNodes[i])
          namePicker.value = newLabel;

        childNodes[i].label = newLabel;
        return;
      }
    }
  },

  onElementAppended: function BPP_onElementAppended(aMicrosummary) {
    var namePicker = this._element("namePicker");
    namePicker.menupopup
              .appendChild(this._createMicrosummaryMenuItem(aMicrosummary));

    // Make sure the drop-marker is shown
    namePicker.setAttribute("droppable", "true");
  },

  onError: function BPP_onError(aMicrosummary) {
    var namePicker = this._element("namePicker");
    var childNodes = namePicker.menupopup.childNodes;

    // 0: user-entered item; 1: separator
    for (var i = 2; i < childNodes.length; i++) {
      if (childNodes[i].microsummary == aMicrosummary &&
          aMicrosummary.needsRemoval)
          namePicker.menupopup.removeChild(childNodes[i]);
    }
  },

  onDialogUnload: function BPP_onDialogUnload() {
    if (this._microsummaries)
      this._microsummaries.removeObserver(this);

    // persist the folder tree height
    if (!this._folderTree.collapsed) {
      this._folderTree.setAttribute("height",
                                    this._folderTree.boxObject.height);
    }
  },

  onDialogAccept: function BPP_onDialogAccept() {
    if (this._action == ACTION_ADD)
      this._createNewItem();
    else
      this._saveChanges();
  },

  /**
   * This method checks the current state of the input fields in the
   * dialog, and if any of them are in an invalid state, it will disable
   * the submit button.  This method should be called after every
   * significant change to the input.
   */
  validateChanges: function BPP_validateChanges() {
    document.documentElement.getButton("accept").disabled = !this._inputIsValid();
  },

  /**
   * This method checks to see if the input fields are in a valid state.
   *
   * @returns  true if the input is valid, false otherwise
   */
  _inputIsValid: function BPP__inputIsValid() {
    if (this._itemType == BOOKMARK_ITEM && !this._containsValidURI("editURLBar"))
      return false;

    // Feed Location has to be a valid URI;
    // Site Location has to be a valid URI or empty
    if (this._itemType == LIVEMARK_CONTAINER) {
      if (!this._containsValidURI("feedLocationTextfield"))
        return false;
      if (!this._containsValidURI("feedSiteLocationTextfield") &&
          (this._element("feedSiteLocationTextfield").value.length > 0))
        return false;
    }

    return true;
  },

  /**
   * Determines whether the XUL textbox with the given ID contains a
   * string that can be converted into an nsIURI.
   *
   * @param aTextboxID
   *        the ID of the textbox element whose contents we'll test
   *
   * @returns true if the textbox contains a valid URI string, false otherwise
   */
  _containsValidURI: function BPP__containsValidURI(aTextboxID) {
    try {
      var value = this._element(aTextboxID).value;
      if (value) {
        var uri = PlacesUIUtils.createFixedURI(value);
        return true;
      }
    } catch (e) { }
    return false;
  },

  /**
   * Get an edit title transaction for the item edit/added in the dialog
   */
  _getEditTitleTransaction:
  function BPP__getEditTitleTransaction(aItemId, aNewTitle) {
    return PlacesUIUtils.ptm.editItemTitle(aItemId, aNewTitle);
  },

  /**
   * XXXmano todo:
   *  1. Make setAnnotationsForURI unset a given annotation if the value field
   *     is not set.
   *  2. Replace PlacesEditItemDescriptionTransaction and
   *     PlacesSetLoadInSidebarTransaction transaction with a generic
   *     transaction to set/unset an annotation object.
   *  3. Use the two helpers below with this new generic transaction in
   *     _saveChanges.
   */

  /**
   * Returns an object which could then be used to set/unset the
   * description annotation for an item (any type).
   *
   * @param aDescription
   *        The description of the item.
   * @returns an object representing the annotation which could then be used
   *          with get/setAnnotationsForURI of PlacesUtils.
   */
  _getDescriptionAnnotation:
  function BPP__getDescriptionAnnotation(aDescription) {
    var anno = { name: DESCRIPTION_ANNO,
                 type: Ci.nsIAnnotationService.TYPE_STRING,
                 flags: 0,
                 value: aDescription,
                 expires: Ci.nsIAnnotationService.EXPIRE_NEVER };

    /**
     * See todo note above
     * if (aDescription)
     *   anno.value = aDescription;
     */
    return anno;
  },

  /**
   * Returns an object which could then be used to set/unset the
   * load-in-sidebar annotation for a bookmark item.
   *
   * @param aLoadInSidebar
   *        Whether to load the bookmark item in the sidebar in default
   *        conditions.
   * @returns an object representing the annotation which could then be used
   *          with get/setAnnotationsForURI of PlacesUtils.
   */
  _getLoadInSidebarAnnotation:
  function BPP__getLoadInSidebarAnnotation(aLoadInSidebar) {
    var anno = { name: LOAD_IN_SIDEBAR_ANNO,
                 type: Ci.nsIAnnotationService.TYPE_INT32,
                 flags: 0,
                 value: aLoadInSidebar,
                 expires: Ci.nsIAnnotationService.EXPIRE_NEVER };

    /**
     * See todo note above
     * if (anno)
     *   anno.value = aLoadInSidebar;
     */
    return anno;
  },

  /**
   * Dialog-accept code path when editing an item (any type).
   *
   * Save any changes that might have been made while the properties dialog
   * was open.
   */
  _saveChanges: function BPP__saveChanges() {
    var itemId = this._itemId;

    var transactions = [];

    // title
    var newTitle = this._element("userEnteredName").label;
    if (newTitle != this._itemTitle)
      transactions.push(this._getEditTitleTransaction(itemId, newTitle));

    // description
    var description = this._element("descriptionTextfield").value;
    if (description != this._itemDescription) {
      transactions.push(PlacesUIUtils.ptm.
                        editItemDescription(itemId, description,
                        this._itemType != BOOKMARK_ITEM));
    }

    if (this._itemType == BOOKMARK_ITEM) {
      // location
      var url = PlacesUIUtils.createFixedURI(this._element("editURLBar").value);
      if (!this._uri.equals(url))
        transactions.push(PlacesUIUtils.ptm.editBookmarkURI(itemId, url));

      // keyword transactions
      var newKeyword = this._element("keywordTextfield").value;
      if (newKeyword != this._bookmarkKeyword) {
        transactions.push(PlacesUIUtils.ptm.
                          editBookmarkKeyword(itemId, newKeyword));
      }

      // microsummaries
      var namePicker = this._element("namePicker");
      var newMicrosummary = namePicker.selectedItem.microsummary;

      // Only add a microsummary update to the transaction if the
      // microsummary has actually changed, i.e. the user selected no
      // microsummary, but the bookmark previously had one, or the user
      // selected a microsummary which is not the one the bookmark previously
      // had.
      if ((newMicrosummary == null &&
           PlacesUIUtils.microsummaries.hasMicrosummary(itemId)) ||
          (newMicrosummary != null &&
           !PlacesUIUtils.microsummaries
                         .isMicrosummary(itemId, newMicrosummary))) {
        transactions.push(
          PlacesUIUtils.ptm.editBookmarkMicrosummary(itemId, newMicrosummary));
      }

      // load in sidebar
      var loadInSidebarChecked = this._element("loadInSidebarCheckbox").checked;
      if (loadInSidebarChecked != this._loadBookmarkInSidebar) {
        transactions.push(
          PlacesUIUtils.ptm.setLoadInSidebar(itemId, loadInSidebarChecked));
      }
    }
    else if (this._itemType == LIVEMARK_CONTAINER) {
      var feedURIString = this._element("feedLocationTextfield").value;
      var feedURI = PlacesUIUtils.createFixedURI(feedURIString);
      if (!this._feedURI.equals(feedURI)) {
        transactions.push(
          PlacesUIUtils.ptm.editLivemarkFeedURI(this._itemId, feedURI));
      }

      // Site Location is empty, we can set its URI to null
      var newSiteURIString = this._element("feedSiteLocationTextfield").value;
      var newSiteURI = null;
      if (newSiteURIString)
        newSiteURI = PlacesUIUtils.createFixedURI(newSiteURIString);

      if ((!newSiteURI && this._siteURI)  ||
          (newSiteURI && (!this._siteURI || !this._siteURI.equals(newSiteURI)))) {
        transactions.push(
          PlacesUIUtils.ptm.editLivemarkSiteURI(this._itemId, newSiteURI));
      }
    }

    // If we have any changes to perform, do them via the
    // transaction manager passed by the opener so they can be undone.
    if (transactions.length > 0) {
      window.arguments[0].performed = true;
      var aggregate =
        PlacesUIUtils.ptm.aggregateTransactions(this._getDialogTitle(), transactions);
      PlacesUIUtils.ptm.doTransaction(aggregate);
    }
  },

  /**
   * [New Item Mode] Get the insertion point details for the new item, given
   * dialog state and opening arguments.
   *
   * The container-identifier and insertion-index are returned separately in
   * the form of [containerIdentifier, insertionIndex]
   */
  _getInsertionPointDetails: function BPP__getInsertionPointDetails() {
    var containerId, indexInContainer = -1;
    if (!this._element("folderRow").hidden)
      containerId = this._getFolderIdFromMenuList();
    else {
      containerId = this._defaultInsertionPoint.itemId;
      indexInContainer = this._defaultInsertionPoint.index;
    }

    return [containerId, indexInContainer];
  },

  /**
   * Returns a transaction for creating a new bookmark item representing the
   * various fields and opening arguments of the dialog.
   */
  _getCreateNewBookmarkTransaction:
  function BPP__getCreateNewBookmarkTransaction(aContainer, aIndex) {
    var uri = PlacesUIUtils.createFixedURI(this._element("editURLBar").value);
    var title = this._element("userEnteredName").label;
    var keyword = this._element("keywordTextfield").value;
    var annotations = [];
    var description = this._element("descriptionTextfield").value;
    if (description)
      annotations.push(this._getDescriptionAnnotation(description));

    var loadInSidebar = this._element("loadInSidebarCheckbox").checked;
    if (loadInSidebar)
      annotations.push(this._getLoadInSidebarAnnotation(true));

    var childTransactions = [];
    var microsummary = this._element("namePicker").selectedItem.microsummary;
    if (microsummary) {
      childTransactions.push(
        PlacesUIUtils.ptm.editBookmarkMicrosummary(-1, microsummary));
    }

    if (this._postData) {
      childTransactions.push(
        PlacesUIUtils.ptm.editBookmarkPostData(-1, this._postData));
    }

    if (this._charSet)
      PlacesUtils.history.setCharsetForURI(this._uri, this._charSet);

    var transactions = [PlacesUIUtils.ptm.createItem(uri, aContainer, aIndex,
                                                     title, keyword,
                                                     annotations,
                                                     childTransactions)];

    return PlacesUIUtils.ptm.aggregateTransactions(this._getDialogTitle(), transactions);
  },

  /**
   * Returns a childItems-transactions array representing the URIList with
   * which the dialog has been opened.
   */
  _getTransactionsForURIList: function BPP__getTransactionsForURIList() {
    var transactions = [];
    for (var i = 0; i < this._URIList.length; ++i) {
      var uri = this._URIList[i];
      var title = this._getURITitleFromHistory(uri);
      transactions.push(PlacesUIUtils.ptm.createItem(uri, -1, -1, title));
    }
    return transactions; 
  },

  /**
   * Returns a transaction for creating a new folder item representing the
   * various fields and opening arguments of the dialog.
   */
  _getCreateNewFolderTransaction:
  function BPP__getCreateNewFolderTransaction(aContainer, aIndex) {
    var folderName = this._element("namePicker").value;
    var annotations = [];
    var childItemsTransactions;
    if (this._URIList)
      childItemsTransactions = this._getTransactionsForURIList();
    var description = this._element("descriptionTextfield").value;
    if (description)
      annotations.push(this._getDescriptionAnnotation(description));

    return PlacesUIUtils.ptm.createFolder(folderName, aContainer, aIndex,
                                          annotations, childItemsTransactions);
  },

  /**
   * Returns a transaction for creating a new live-bookmark item representing
   * the various fields and opening arguments of the dialog.
   */
  _getCreateNewLivemarkTransaction:
  function BPP__getCreateNewLivemarkTransaction(aContainer, aIndex) {
    var feedURIString = this._element("feedLocationTextfield").value;
    var feedURI = PlacesUIUtils.createFixedURI(feedURIString);

    var siteURIString = this._element("feedSiteLocationTextfield").value;
    var siteURI = null;
    if (siteURIString)
      siteURI = PlacesUIUtils.createFixedURI(siteURIString);

    var name = this._element("namePicker").value;
    return PlacesUIUtils.ptm.createLivemark(feedURI, siteURI, name,
                                            aContainer, aIndex);
  },

  /**
   * Dialog-accept code-path for creating a new item (any type)
   */
  _createNewItem: function BPP__getCreateItemTransaction() {
    var [container, index] = this._getInsertionPointDetails();
    var createTxn;
    if (this._itemType == BOOKMARK_FOLDER)
      createTxn = this._getCreateNewFolderTransaction(container, index);
    else if (this._itemType == LIVEMARK_CONTAINER)
      createTxn = this._getCreateNewLivemarkTransaction(container, index);
    else // BOOKMARK_ITEM
      createTxn = this._getCreateNewBookmarkTransaction(container, index);

    // Mark the containing folder as recently-used if it isn't in the static
    // list
    if (container != PlacesUtils.toolbarFolderId &&
        container != PlacesUtils.bookmarksMenuFolderId)
      this._markFolderAsRecentlyUsed(container);

    // perfrom our transaction do via the transaction manager passed by the
    // opener so it can be undone.
    window.arguments[0].performed = true;
    PlacesUIUtils.ptm.doTransaction(createTxn);
  },

  onNamePickerInput: function BPP_onNamePickerInput() {
    this._element("userEnteredName").label = this._element("namePicker").value;
  },

  toggleTreeVisibility: function BPP_toggleTreeVisibility() {
    var expander = this._element("expander");
    if (!this._folderTree.collapsed) { // if (willCollapse)
      expander.className = "down";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextdown"));
      document.documentElement.buttons = "accept,cancel";

      this._folderTreeHeight = this._folderTree.boxObject.height;
      this._folderTree.setAttribute("height", this._folderTreeHeight);
      this._folderTree.collapsed = true;
      resizeTo(window.outerWidth, window.outerHeight - this._folderTreeHeight);
    }
    else {
      expander.className = "up";
      expander.setAttribute("tooltiptext",
                            expander.getAttribute("tooltiptextup"));
      document.documentElement.buttons = "accept,cancel,extra2";

      this._folderTree.collapsed = false;

      if (!this._folderTree.place) {
        const FOLDER_TREE_PLACE_URI =
          "place:excludeItems=1&excludeQueries=1&excludeReadOnlyFolders=1&folder=" +
          PlacesUIUtils.allBookmarksFolderId;
        this._folderTree.place = FOLDER_TREE_PLACE_URI;
      }

      var currentFolder = this._getFolderIdFromMenuList();
      this._folderTree.selectItems([currentFolder]);
      this._folderTree.focus();

      resizeTo(window.outerWidth, window.outerHeight + this._folderTreeHeight);
    }
  },

  _getFolderIdFromMenuList:
  function BPP__getFolderIdFromMenuList() {
    var selectedItem = this._folderMenuList.selectedItem;
    NS_ASSERT("folderId" in selectedItem,
              "Invalid menuitem in the folders-menulist");
    return selectedItem.folderId;
  },

  /**
   * Get the corresponding menu-item in the folder-menu-list for a bookmarks
   * folder if such an item exists. Otherwise, this creates a menu-item for the
   * folder. If the items-count limit (see MAX_FOLDERS_IN_MENU_LIST) is reached,
   * the new item replaces the last menu-item.
   * @param aFolderId
   *        The identifier of the bookmarks folder.
   */
  _getFolderMenuItem:
  function BPP__getFolderMenuItem(aFolderId) {
    var menupopup = this._folderMenuList.menupopup;

    for (var i=0; i < menupopup.childNodes.length; i++) {
      if (menupopup.childNodes[i].folderId == aFolderId)
        return menupopup.childNodes[i];
    }

    // 2 special folders + separator + folder-items-count limit
    if (menupopup.childNodes.length == 3 + MAX_FOLDER_ITEM_IN_MENU_LIST)
      menupopup.removeChild(menupopup.lastChild);

    return this._appendFolderItemToMenupopup(menupopup, aFolderId);
  },

  onMenuListFolderSelect: function BPP_onMenuListFolderSelect(aEvent) {
    if (this._folderTree.hidden)
      return;

    this._folderTree.selectItems([this._getFolderIdFromMenuList()]);
  },

  onFolderTreeSelect: function BPP_onFolderTreeSelect() {
    var selectedNode = this._folderTree.selectedNode;
    if (!selectedNode)
      return;

    var folderId = PlacesUtils.getConcreteItemId(selectedNode);
    if (this._getFolderIdFromMenuList() == folderId)
      return;

    var folderItem = this._getFolderMenuItem(folderId);
    this._folderMenuList.selectedItem = folderItem;
  },

  _markFolderAsRecentlyUsed:
  function BPP__markFolderAsRecentlyUsed(aFolderId) {
    // We'll figure out when/if to expire the annotation if it turns out
    // we keep this recently-used-folders implementation
    PlacesUtils.annotations
               .setItemAnnotation(aFolderId, LAST_USED_ANNO,
                                  new Date().getTime(), 0,
                                  Ci.nsIAnnotationService.EXPIRE_NEVER);
  },

  newFolder: function BPP_newFolder() {
    // The command is disabled when the tree is not focused
    this._folderTree.focus();
    goDoCommand("placesCmd_new:folder");
  }
};
