/*
 * Bug 486490 - Fennec browser-chrome tests to verify correct implementation of chrome 
 *              code in mobile/chrome/content in terms of integration with Places
 *              component, specifically for bookmark management.
 */

var testURL_01 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_01.html";
var testURL_02 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_02.html";

// A queue to order the tests and a handle for each test
var gTests = [];
var gCurrentTest = null;

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();
  
  // Start the tests
  runNextTest();
}

//------------------------------------------------------------------------------
// Iterating tests by shifting test out one by one as runNextTest is called.
function runNextTest() {
  // Run the next test until all tests completed
  if (gTests.length > 0) {
    gCurrentTest = gTests.shift();
    info(gCurrentTest.desc);
    gCurrentTest.run();
  }
  else {
    // Cleanup. All tests are completed at this point
    try {
      PlacesUtils.bookmarks.removeFolderChildren(BookmarkList.mobileRoot);
    }
    finally {
      // We must finialize the tests
      finish();
    }
  }
}

//------------------------------------------------------------------------------
// Case: Test adding a bookmark with the Star button
gTests.push({
  desc: "Test adding a bookmark with the Star button",
  _currenttab: null,

  run: function() {
    this._currenttab = Browser.addTab(testURL_01, true);

    // Need to wait until the page is loaded
    messageManager.addMessageListener("pageshow",
    function() {
      if (gCurrentTest._currenttab.browser.currentURI.spec != "about:blank") {
        messageManager.removeMessageListener("pageshow", arguments.callee);
        gCurrentTest.onPageReady();
      }
    });
  },
  
  onPageReady: function() {
    var starbutton = document.getElementById("tool-star");
    starbutton.click();
    
    var bookmarkItem = PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_01));
    ok(bookmarkItem != -1, testURL_01 + " should be added.");

    Browser.closeTab(gCurrentTest._currenttab);
    
    runNextTest();
  }  
});

//------------------------------------------------------------------------------
// Case: Test clicking on a bookmark loads the web page
gTests.push({
  desc: "Test clicking on a bookmark loads the web page",
  _currenttab: null,

  run: function() {
    this._currenttab = Browser.addTab("about:blank", true);

    // Need to wait until the page is loaded
    messageManager.addMessageListener("pageshow",
    function() {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      gCurrentTest.onPageReady();
    });
  },

  onPageReady: function() {
    // Open the bookmark list
    BookmarkList.show();

    waitFor(gCurrentTest.onBookmarksReady, function() { return document.getElementById("bookmarklist-container").hidden == false; });
  },
  
  onBookmarksReady: function() {
    // Create a listener for the opening bookmark  
    messageManager.addMessageListener("pageshow", 
      function() {
        messageManager.removeMessageListener("pageshow", arguments.callee);
        todo_is(gCurrentTest._currenttab.browser.currentURI.spec, testURL_01, "Opened the right bookmark");      

        Browser.closeTab(gCurrentTest._currenttab);

        runNextTest();
      }, 
      true);

    var bookmarkitems = document.getElementById("bookmark-items");    
    var bookmarkitem = document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_01);
    isnot(bookmarkitem, null, "Found the bookmark");
    is(bookmarkitem.getAttribute("uri"), testURL_01, "Bookmark has the right URL via attribute");
    is(bookmarkitem.spec, testURL_01, "Bookmark has the right URL via property");

    EventUtils.synthesizeMouse(bookmarkitem, bookmarkitem.clientWidth / 2, bookmarkitem.clientHeight / 2, {});
  }  
});

//------------------------------------------------------------------------------
// Case: Test editing URI of existing bookmark
gTests.push({
  desc: "Test editing URI of existing bookmark",

  run: function() {
    // Open the bookmark list
    BookmarkList.show();

    // Go into edit mode
    BookmarkList.toggleManage();

    waitFor(gCurrentTest.onBookmarksReady, function() { return document.getElementById("bookmark-items").manageUI == true; });
  },
  
  onBookmarksReady: function() {
    var bookmarkitems = document.getElementById("bookmark-items");
    var bookmarkitem = document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_01);
    EventUtils.synthesizeMouse(bookmarkitem, bookmarkitem.clientWidth / 2, bookmarkitem.clientHeight / 2, {});

    var uritextbox = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "uri");
    uritextbox.value = testURL_02;

    var donebutton = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();

    var bookmark = PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_01));
    is(bookmark, -1, testURL_01 + " should no longer in bookmark");
    bookmark = PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_02));
    isnot(bookmark, -1, testURL_02 + " is in bookmark");
    
    BookmarkList.close();
    
    runNextTest();
  }  
});

//------------------------------------------------------------------------------
// Case: Test editing title of existing bookmark
gTests.push({
  desc: "Test editing title of existing bookmark",
  
  run: function() {
    // Open the bookmark list
    BookmarkList.show();

    // Go into edit mode
    BookmarkList.toggleManage();

    waitFor(gCurrentTest.onBookmarksReady, function() { return document.getElementById("bookmark-items").manageUI == true; });
  },
  
  onBookmarksReady: function() {
    var bookmark = PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_02));
    is(PlacesUtils.bookmarks.getItemTitle(bookmark), "Browser Blank Page 01", "Title remains the same.");
    
    var bookmarkitems = document.getElementById("bookmark-items");
    var bookmarkitem = document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_02);
    EventUtils.synthesizeMouse(bookmarkitem, bookmarkitem.clientWidth / 2, bookmarkitem.clientHeight / 2, {});
    
    var titletextbox = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "name");
    var newtitle = "Changed Title";
    titletextbox.value = newtitle;
    
    var donebutton = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();

    isnot(PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_02)), -1, testURL_02 + " is still in bookmark.");
    is(PlacesUtils.bookmarks.getItemTitle(bookmark), newtitle, "Title is changed.");
    
    BookmarkList.close();
    
    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Case: Test removing existing bookmark
gTests.push({
  desc: "Test removing existing bookmark",
  bookmarkitem: null,
  
  run: function() {
    // Open the bookmark list
    BookmarkList.show();

    // Go into edit mode
    BookmarkList.toggleManage();

    waitFor(gCurrentTest.onBookmarksReady, function() { return document.getElementById("bookmark-items").manageUI == true; });
  },
  
  onBookmarksReady: function() {
    var bookmarkitems = document.getElementById("bookmark-items");
    gCurrentTest.bookmarkitem = document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_02);
    gCurrentTest.bookmarkitem.click();

    waitFor(gCurrentTest.onEditorReady, function() { return gCurrentTest.bookmarkitem.isEditing == true; });
  },
  
  onEditorReady: function() {
    var removebutton = document.getAnonymousElementByAttribute(gCurrentTest.bookmarkitem, "anonid", "remove-button");
    removebutton.click();
    
    var bookmark = PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_02));
    ok(bookmark == -1, testURL_02 + " should no longer in bookmark");
    bookmark = PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_01));
    ok(bookmark == -1, testURL_01 + " should no longer in bookmark");

    BookmarkList.close();

    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Case: Test editing title of desktop folder
gTests.push({
  desc: "Test editing title of desktop folder",
  bmId: null,
  
  run: function() {
    // Add a bookmark to the desktop area so the desktop folder is displayed
    gCurrentTest.bmId = PlacesUtils.bookmarks
                                   .insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                   makeURI(testURL_02),
                                                   Ci.nsINavBookmarksService.DEFAULT_INDEX,
                                                   testURL_02);

    // Open the bookmark list
    BookmarkList.show();

    // Go into edit mode
    BookmarkList.toggleManage();

    waitFor(gCurrentTest.onBookmarksReady, function() { return document.getElementById("bookmark-items").manageUI == true; });
  },
  
  onBookmarksReady: function() {
    // Is the "desktop" folder showing?
    var first = BookmarkList._bookmarks._children.firstChild;
    is(first.itemId, BookmarkList._bookmarks._desktopFolderId, "Desktop folder is showing");

    // Is the "desktop" folder in edit mode?
    is(first.isEditing, false, "Desktop folder is not in edit mode");


    // Do not allow the "desktop" folder to be editable by tap
    EventUtils.synthesizeMouse(first, first.clientWidth / 2, first.clientHeight / 2, {});

    // A tap on the "desktop" folder _should_ open the folder, not put it into edit mode.
    // So we need to get the first item again.
    first = BookmarkList._bookmarks._children.firstChild;

    // It should not be the "desktop" folder
    isnot(first.itemId, BookmarkList._bookmarks._desktopFolderId, "Desktop folder is not showing after mouse click");

    // But it should be one of the other readonly bookmark roots
    isnot(BookmarkList._bookmarks._readOnlyFolders.indexOf(parseInt(first.itemId)), -1, "Desktop subfolder is showing after mouse click");
    
    BookmarkList.close();

    PlacesUtils.bookmarks.removeItem(gCurrentTest.bmId);

    runNextTest();
  }
});
