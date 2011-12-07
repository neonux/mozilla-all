/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource:///modules/NewTabUtils.jsm");

registerCleanupFunction(function () {
  reset();

  while (gBrowser.tabs.length > 1)
    gBrowser.removeTab(gBrowser.tabs[1]);
});

let cw;
let cells;

// ----------
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
function setLinks(aLinksPattern) {
  let links = aLinksPattern.split(/\s*,\s*/).map(function (id) {
    return {url: "about:blank#" + id, title: "site#" + id};
  });

  NewTabUtils.Testing.mockLinks(links);
}

// ----------
function setPinnedLinks(aLinksPattern) {
  let pinnedLinks = [];

  aLinksPattern.split(/\s*,\s*/).forEach(function (id, index) {
    let link;

    if (id)
      link = {url: "about:blank#" + id, title: "site#" + id};

    pinnedLinks[index] = link;
  });

  NewTabUtils.Testing.mockPinnedLinks(pinnedLinks);
}

// ----------
function reset() {
  NewTabUtils.reset();
}

// ----------
function addNewTabPageTab() {
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:newtab");
  let browser = tab.linkedBrowser;

  // wait for the new tab page to be loaded
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    cw = browser.contentWindow;
    doc = browser.contentDocument;

    if (cw.Page.isEnabled())
      cells = cw.Grid.cells;

    TestRunner.next();
  }, true);
}

// ----------
function checkGrid(aSitesPattern, aSites) {
  let valid = true;

  aSites = aSites || cw.Grid.sites;

  aSitesPattern.split(/\s*,\s*/).forEach(function (id, index) {
    let site = aSites[index];
    let match = id.match(/^\d+/);

    // the cell should be empty
    if (!match) {
      if (site) {
        valid = false;
        ok(false, "expected cell#" + index + " to be empty");
      }

      return;
    }

    // the cell should not be empty
    if (!site) {
      valid = false;
      ok(false, "didn't expect cell#" + index + " to be empty");

      return;
    }

    let num = match[0];

    // check site url
    if (site.url != "about:blank#" + num) {
      valid = false;
      is(site.url, "about:blank#" + num, "cell#" + index + " has the wrong url");
    }

    let shouldBePinned = /p$/.test(id);
    let cellContainsPinned = site.isPinned();
    let cssClassPinned = site.node && site.node.classList.contains("site-pinned");

    // check if site should be pinned
    if (shouldBePinned) {
      if (!cellContainsPinned) {
        valid = false;
        ok(false, "expected cell#" + index + " to be pinned");
      } else if (!cssClassPinned) {
        valid = false;
        ok(false, "expected cell#" + index + " to have css class 'pinned'");
      }
    } else {
      if (cellContainsPinned) {
        valid = false;
        ok(false, "didn't expect cell#" + index + " to be pinned");
      } else if (cssClassPinned) {
        valid = false;
        ok(false, "didn't expect cell#" + index + " to have css class 'pinned'");
      }
    }
  });

  if (valid)
    ok(true, "grid status = " + aSitesPattern);
}

// ----------
function blockCell(aCell) {
  aCell.site.block(function () executeSoon(TestRunner.next));
}

// ----------
function pinCell(aCell, aIndex) {
  aCell.site.pin(aIndex);
}

// ----------
function unpinCell(aCell) {
  aCell.site.unpin(function () executeSoon(TestRunner.next));
}

// ----------
function simulateDrop(aDropTarget, aDragSource) {
  let event = {
    dataTransfer: {
      mozUserCancelled: false,
      setData: function () null,
      setDragImage: function () null,
      getData: function () "about:blank#99\nblank"
    }
  };

  if (aDragSource)
    cw.Drag.start(aDragSource.site, event);

  cw.Drop.drop(aDropTarget, event, function () executeSoon(TestRunner.next));

  if (aDragSource)
    cw.Drag.end(aDragSource.site);
}
