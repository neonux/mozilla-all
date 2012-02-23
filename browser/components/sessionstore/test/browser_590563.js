/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let oldState = {
    windows: [{
      tabs: [
        { entries: [{ url: "about:mozilla" }], hidden: true },
        { entries: [{ url: "about:blank" }], hidden: false }
      ]
    }]
  };
  let pageData = {
    url: "about:sessionrestore",
    formdata: { "#sessionData": oldState }
  };
  let state = { windows: [{ tabs: [{ entries: [pageData] }] }] };

  waitForExplicitFinish();

  newWindowWithState(state, function (win) {
    registerCleanupFunction(function () win.close());

    is(gBrowser.tabs.length, 1, "The total number of tabs should be 1");
    is(gBrowser.visibleTabs.length, 1, "The total number of visible tabs should be 1");

    executeSoon(function () {
      waitForFocus(function () {
        middleClickTest(win);
        finish();
      }, win);
    });
  });
}

function middleClickTest(win) {
  let browser = win.gBrowser.selectedBrowser;
  let tree = browser.contentDocument.getElementById("tabList");
  is(tree.view.rowCount, 3, "There should be three items");

  let x = {}, y = {}, width = {}, height = {};

  // click on the first tab item
  tree.treeBoxObject.getCoordsForCellItem(1, tree.columns[1], "text", x, y, width, height);
  EventUtils.synthesizeMouse(tree.body, x.value, y.value, { button: 1 },
                             browser.contentWindow);
  // click on the second tab item
  tree.treeBoxObject.getCoordsForCellItem(2, tree.columns[1], "text", x, y, width, height);
  EventUtils.synthesizeMouse(tree.body, x.value, y.value, { button: 1 },
                             browser.contentWindow);

  is(win.gBrowser.tabs.length, 3,
     "The total number of tabs should be 3 after restoring 2 tabs by middle click.");
  is(win.gBrowser.visibleTabs.length, 3,
     "The total number of visible tabs should be 3 after restoring 2 tabs by middle click");
}

function newWindowWithState(state, callback) {
  let opts = "chrome,all,dialog=no,height=800,width=800";
  let win = window.openDialog(getBrowserURL(), "_blank", opts);

  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);

    let tab = win.gBrowser.selectedTab;

    // The form data will be restored before SSTabRestored, so we want to listen
    // for that on the currently selected tab (it will be reused)
    tab.addEventListener("SSTabRestored", function onRestored() {
      tab.removeEventListener("SSTabRestored", onRestored, true);
      callback(win);
    }, true);

    executeSoon(function () {
      ss.setWindowState(win, JSON.stringify(state), true);
    });
  }, false);
}
