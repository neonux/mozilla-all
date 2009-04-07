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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net>
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
 * Tests the bookmarks Properties dialog.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

// DOM ids of Places sidebar trees.
const SIDEBAR_HISTORY_ID = "historyTree";
const SIDEBAR_BOOKMARKS_ID = "bookmarks-view";

// Action to execute on the current node.
const ACTION_EDIT = 0;
const ACTION_ADD = 1;

// If action is ACTION_ADD, set type to one of those, to define what do you
// want to create.
const TYPE_FOLDER = 0;
const TYPE_BOOKMARK = 1;

const TEST_URL = "http://www.mozilla.org/";

var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
         getService(Ci.nsIWindowMediator);
var win = wm.getMostRecentWindow("navigator:browser");
var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
         getService(Ci.nsIWindowWatcher);

function add_visit(aURI, aDate) {
  var visitId = PlacesUtils.history
                           .addVisit(aURI,
                                     aDate,
                                     null, // no referrer
                                     PlacesUtils.history.TRANSITION_TYPED,
                                     false, // not redirect
                                     0);
  return visitId;
}

function add_bookmark(aURI) {
  var bId = PlacesUtils.bookmarks
                       .insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       aURI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "bookmark/" + aURI.spec);
  return bId;
}

// Each test is an obj w/ a desc property and run method.
var gTests = [];
var gCurrentTest = null;

//------------------------------------------------------------------------------
// TEST SKELETON: use this template to add new tests.
/*
gTests.push({
  desc: "Bug Description",
  sidebar: SIDEBAR_BOOKMARKS_ID, // See SIDEBAR_ constants above.
  action: ACTION_EDIT, // See ACTION_ constants above.
  itemType: null, // See TYPE_ constants above, required for ACTION_ADD.
  window: null, // Will contain handle of dialog window

  setup: function() {
    // Setup everything needed for this test, runs before everything else.
  },

  selectNode: function(tree) {
    // Select the node to edit or to add to, runs when sidebar tree is visible.
  },

  run: function() {
    // Actual test, runs when dialog is open.
  },

  finish: function() {
    // Close window, toggle sidebar and goto next test.
    this.window.document.documentElement.cancelDialog();
    toggleSidebar("viewBookmarksSidebar", false);
    runNextTest();
  },

  cleanup: function() {
    // Undo everything added during setup, runs after dialog has been closed.
  }
});
*/

//------------------------------------------------------------------------------
// Bug 479348 - Properties on a root should be read-only

gTests.push({
  desc: "Bug 479348 - Properties on a root should be read-only",
  sidebar: SIDEBAR_BOOKMARKS_ID,
  action: ACTION_EDIT,
  itemType: null,
  window: null,

  setup: function() {
    // Nothing to do.
  },

  selectNode: function(tree) {
    // Select Unfiled Bookmarks root.
    var itemId = PlacesUIUtils.leftPaneQueries["UnfiledBookmarks"];
    tree.selectItems([itemId]);
    this.selectedNode = tree.selectedNode;
  },

  run: function() {
    // Check that the dialog is read-only.
    ok(this.window.BookmarkPropertiesPanel._readOnly, "Dialog is read-only");

    // Check that accept button is disabled
    var acceptButton = this.window.document.documentElement.getButton("accept");
    ok(acceptButton.disabled, "Accept button is disabled");

    // Check that name picker is read only
    var namepicker = this.window.document.getElementById("editBMPanel_namePicker");
    ok(namepicker.readOnly, "Name field is disabled");
    is(namepicker.value,
       PlacesUtils.bookmarks.getItemTitle(PlacesUtils.unfiledBookmarksFolderId),
       "Node title is correct");
    // Blur the field and ensure root's name has not been changed.
    this.window.gEditItemOverlay.onNamePickerChange();
    is(namepicker.value,
       PlacesUtils.bookmarks.getItemTitle(PlacesUtils.unfiledBookmarksFolderId),
       "Root title is correct");
    // Check the shortcut's title.
    is(PlacesUtils.bookmarks.getItemTitle(this.selectedNode.itemId), null,
       "Shortcut title is null");
    this.finish();
  },

  finish: function() {
    this.window.document.documentElement.cancelDialog();
    toggleSidebar("viewBookmarksSidebar", false);
    runNextTest();
  },

  cleanup: function() {
    // Nothing to do.
  }
});

//------------------------------------------------------------------------------
// Bug 462662 - Pressing Enter to select tag from autocomplete closes bookmarks properties dialog

gTests.push({
  desc: "Bug 462662 - Pressing Enter to select tag from autocomplete closes bookmarks properties dialog",
  sidebar: SIDEBAR_BOOKMARKS_ID,
  action: ACTION_EDIT,
  itemType: null,
  window: null,
  _itemId: null,

  setup: function() {
    // Add a bookmark in unsorted bookmarks folder.
    this._itemId = add_bookmark(PlacesUtils._uri(TEST_URL));
    ok(this._itemId > 0, "Correctly added a bookmark");
    // Add a tag to this bookmark.
    PlacesUtils.tagging.tagURI(PlacesUtils._uri(TEST_URL),
                               ["testTag"]);
    var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(TEST_URL), {});
    is(tags[0], "testTag", "Correctly added a tag");
  },

  selectNode: function(tree) {
    tree.selectItems([this._itemId]);
    is(tree.selectedNode.itemId, this._itemId, "Bookmark has been selected");
  },

  run: function() {
    // open tags autocomplete and press enter
    var tagsField = this.window.document.getElementById("editBMPanel_tagsField");
    var self = this;
    tagsField.popup.addEventListener("popupshown", function (aEvent) {
        tagsField.popup.removeEventListener("popupshown", arguments.callee, true);
        tagsField.popup.focus();
        EventUtils.synthesizeKey("VK_RETURN", {}, self.window);
      }, true);
    tagsField.popup.addEventListener("popuphidden", function (aEvent) {
        tagsField.popup.removeEventListener("popuphidden", arguments.callee, true);
        self.finish();
      }, true);
    tagsField.focus();
    tagsField.value = "";
    EventUtils.synthesizeKey("t", {}, this.window);
  },

  finish: function() {
    isnot(this.window, null, "Window is still open");
    this.window.document.documentElement.cancelDialog();
    toggleSidebar("viewBookmarksSidebar", false);
    runNextTest();
  },

  cleanup: function() {
    // Check tags have not changed.
    var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(TEST_URL), {});
    is(tags[0], "testTag", "Tag on node has not changed");

    // Cleanup.
    PlacesUtils.tagging.untagURI(PlacesUtils._uri(TEST_URL), ["testTag"]);
    PlacesUtils.bookmarks.removeItem(this._itemId);
  }
});

//------------------------------------------------------------------------------
// Bug 475529 -  Add button in new folder dialog not default anymore

gTests.push({
  desc: "Bug 475529 - Add button in new folder dialog not default anymore",
  sidebar: SIDEBAR_BOOKMARKS_ID,
  action: ACTION_ADD,
  itemType: TYPE_FOLDER,
  window: null,
  _itemId: null,

  setup: function() {
    // Nothing to do.
  },

  selectNode: function(tree) {
    // Select Unfiled Bookmarks root.
    var itemId = PlacesUIUtils.leftPaneQueries["UnfiledBookmarks"];
    tree.selectItems([itemId]);
    this.selectedNode = tree.selectedNode;
  },

  run: function() {
    this._itemId = this.window.gEditItemOverlay._itemId;
    // Change folder name
    var namePicker = this.window.document.getElementById("editBMPanel_namePicker");
    namePicker.value = "";
    var self = this;
    this.window.addEventListener("unload", function(event) {
        this.window.removeEventListener("unload", arguments.callee, false);
        executeSoon(function() {
          self.finish();
        });
      }, false);
    namePicker.focus();
    EventUtils.synthesizeKey("n", {}, this.window);
    EventUtils.synthesizeKey("VK_RETURN", {}, this.window);
  },

  finish: function() {
    // Window is already closed.
    toggleSidebar("viewBookmarksSidebar", false);
    runNextTest();
  },

  cleanup: function() {
    // Check that folder name has been changed.
    is(PlacesUtils.bookmarks.getItemTitle(this._itemId), "n",
       "Folder name has been edited");

    // Cleanup.
    PlacesUtils.bookmarks.removeItem(this._itemId);
  }
});

//------------------------------------------------------------------------------
// Bug 476020 - Pressing Esc while having the tag autocomplete open closes the bookmarks panel

gTests.push({
  desc: "Bug 476020 - Pressing Esc while having the tag autocomplete open closes the bookmarks panel",
  sidebar: SIDEBAR_BOOKMARKS_ID,
  action: ACTION_EDIT,
  itemType: null,
  window: null,
  _itemId: null,

  setup: function() {
    // Add a bookmark in unsorted bookmarks folder.
    this._itemId = add_bookmark(PlacesUtils._uri(TEST_URL));
    ok(this._itemId > 0, "Correctly added a bookmark");
    // Add a tag to this bookmark.
    PlacesUtils.tagging.tagURI(PlacesUtils._uri(TEST_URL),
                               ["testTag"]);
    var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(TEST_URL), {});
    is(tags[0], "testTag", "Correctly added a tag");
  },

  selectNode: function(tree) {
    tree.selectItems([this._itemId]);
    is(tree.selectedNode.itemId, this._itemId, "Bookmark has been selected");
  },

  run: function() {
    // open tags autocomplete and press enter
    var tagsField = this.window.document.getElementById("editBMPanel_tagsField");
    var self = this;
    tagsField.popup.addEventListener("popupshown", function (aEvent) {
        tagsField.popup.removeEventListener("popupshown", arguments.callee, true);
        tagsField.popup.focus();
        EventUtils.synthesizeKey("VK_ESCAPE", {}, self.window);
      }, true);
    tagsField.popup.addEventListener("popuphidden", function (aEvent) {
        tagsField.popup.removeEventListener("popuphidden", arguments.callee, true);
        self.finish();
      }, true);
    tagsField.focus();
    tagsField.value = "";
    EventUtils.synthesizeKey("t", {}, this.window);
  },

  finish: function() {
    isnot(this.window, null, "Window is still open");
    this.window.document.documentElement.cancelDialog();
    toggleSidebar("viewBookmarksSidebar", false);
    runNextTest();
  },

  cleanup: function() {
    // Check tags have not changed.
    var tags = PlacesUtils.tagging.getTagsForURI(PlacesUtils._uri(TEST_URL), {});
    is(tags[0], "testTag", "Tag on node has not changed");

    // Cleanup.
    PlacesUtils.tagging.untagURI(PlacesUtils._uri(TEST_URL),
                                 ["testTag"]);
    PlacesUtils.bookmarks.removeItem(this._itemId);
  }
});

//------------------------------------------------------------------------------

function test() {
  waitForExplicitFinish();
  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils in context");
  ok(PlacesUIUtils, "PlacesUIUtils in context");

  // kick off tests
  runNextTest();
}

function runNextTest() {
  // Cleanup from previous test.
  if (gCurrentTest) {
    gCurrentTest.cleanup();
    ok(true, "*** FINISHED TEST ***");
  }

  if (gTests.length > 0) {
    // Goto next tests.
    gCurrentTest = gTests.shift();
    ok(true, "*** TEST: " + gCurrentTest.desc);
    gCurrentTest.setup();
    execute_test_in_sidebar();
  }
  else {
    // Finished all tests.
    finish();
  }
}

/**
 * Global functions to run a test in Properties dialog context.
 */

function execute_test_in_sidebar() {
    var sidebar = document.getElementById("sidebar");
    sidebar.addEventListener("load", function() {
      sidebar.removeEventListener("load", arguments.callee, true);
      sidebar.focus();
      // Need to executeSoon since the tree is initialized on sidebar load.
      executeSoon(open_properties_dialog);
    }, true);
    toggleSidebar("viewBookmarksSidebar", true);
}

function open_properties_dialog() {
    var sidebar = document.getElementById("sidebar");
    // Get sidebar's Places tree.
    var tree = sidebar.contentDocument.getElementById(gCurrentTest.sidebar);
    ok(tree, "Sidebar tree has been loaded");
    // Ask current test to select the node to edit.
    gCurrentTest.selectNode(tree);
    ok(tree.selectedNode,
       "We have a places node selected: " + tree.selectedNode.title);

    // Wait for the Properties dialog.
    var windowObserver = {
      observe: function(aSubject, aTopic, aData) {
        if (aTopic === "domwindowopened") {
          ww.unregisterNotification(this);
          var win = aSubject.QueryInterface(Ci.nsIDOMWindow);
          win.addEventListener("load", function onLoad(event) {
            win.removeEventListener("load", onLoad, false);
            // Windows has been loaded, execute our test now.
            executeSoon(function () {
              // Ensure overlay is loaded
              ok(win.gEditItemOverlay._initialized, "EditItemOverlay is initialized");
              gCurrentTest.window = win;
              try {
                gCurrentTest.run();
              } catch (ex) {
                ok(false, "An error occured during test run: " + ex.message);
              }
            });
          }, false);
        }
      }
    };
    ww.registerNotification(windowObserver);

    var command = null;
    switch(gCurrentTest.action) {
      case ACTION_EDIT:
        command = "placesCmd_show:info";
        break;
      case ACTION_ADD:
        if (gCurrentTest.itemType == TYPE_FOLDER)
          command = "placesCmd_new:folder";
        else if (gCurrentTest.itemType == TYPE_BOOKMARK)
          command = "placesCmd_new:bookmark";
        else
          ok(false, "You didn't set a valid itemType for adding an item");
        break;
      default:
        ok(false, "You didn't set a valid action for this test");
    }
    // Ensure command is enabled for this node.
    ok(tree.controller.isCommandEnabled(command),
       "Properties command on current selected node is enabled");

    // This will open the dialog.
    tree.controller.doCommand(command);
}
