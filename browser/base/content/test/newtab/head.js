/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource:///modules/NewTabUtils.jsm");
registerCleanupFunction(function () NewTabUtils.Testing.reset());

let cells;

function test() {
  TestRunner.run();
}

// ----------
let TestRunner = {
  run: function () {
    waitForExplicitFinish();

    this._iter = runTests();
    this.next();
  },

  next: function () {
    try {
      TestRunner._iter.next();
    } catch (e if e instanceof StopIteration) {
      finish();
    }
  }
};

// ----------
function addNewTabPageTab(aLinks) {
  if (aLinks)
    NewTabUtils.Testing.mockLinks(aLinks);

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:newtab");
  registerCleanupFunction(function () gBrowser.removeTab(tab));

  let browser = tab.linkedBrowser;

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    let cw = browser.contentWindow;
    cells = cw.Grid.cells;

    TestRunner.next();
  }, true);
}

// ----------
function blockCell(aCell) {
  NewTabUtils.Testing.blockSite(aCell.site, function () {
    executeSoon(TestRunner.next)
  });
}

// ----------
function unpinCell(aCell) {
  NewTabUtils.Testing.unpinSite(aCell.site, function () {
    executeSoon(TestRunner.next)
  });
}

// ----------
function pinCell(aCell) {
  let site = aCell.site;
  site && site.pin();
}

// ----------
function checkCellIsEmpty(aCell, aMessage) {
  ok(aCell.isEmpty(), aMessage);
}

// ----------
function checkCellIsPinned(aCell, aMessage) {
  ok(aCell.containsPinnedSite(), aMessage);
}

// ----------
function checkCellTitle(aCell, aTitle, aMessage) {
  is(aCell.site.title, aTitle, aMessage);
}
