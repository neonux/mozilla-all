var tabs;

function index(tab) Array.indexOf(gBrowser.tabs, tab);

function indexTest(tab, expectedIndex, msg) {
  var diag = "tab " + tab + " should be at index " + expectedIndex;
  if (msg)
    msg = msg + " (" + diag + ")";
  else
    msg = diag;
  is(index(tabs[tab]), expectedIndex, msg);
}

function PinUnpinHandler(tab, eventName) {
  this.eventCount = 0;
  var self = this;
  tab.addEventListener(eventName, function() {
    tab.removeEventListener(eventName, arguments.callee, true);

    self.eventCount++;
  }, true);
  gBrowser.tabContainer.addEventListener(eventName, function(e) {
    gBrowser.tabContainer.removeEventListener(eventName, arguments.callee, true);

    if (e.originalTarget == tab) {
      self.eventCount++;
    }
  }, true);
}

function test() {
  tabs = [gBrowser.selectedTab, gBrowser.addTab(), gBrowser.addTab(), gBrowser.addTab()];
  indexTest(0, 0);
  indexTest(1, 1);
  indexTest(2, 2);
  indexTest(3, 3);

  var eh = new PinUnpinHandler(tabs[3], "TabPin");
  gBrowser.pinTab(tabs[3]);
  is(eh.eventCount, 2, "TabPin event should be fired");
  indexTest(0, 1);
  indexTest(1, 2);
  indexTest(2, 3);
  indexTest(3, 0);

  eh = new PinUnpinHandler(tabs[1], "TabPin");
  gBrowser.pinTab(tabs[1]);
  is(eh.eventCount, 2, "TabPin event should be fired");
  indexTest(0, 2);
  indexTest(1, 1);
  indexTest(2, 3);
  indexTest(3, 0);

  gBrowser.moveTabTo(tabs[3], 3);
  indexTest(3, 1, "shouldn't be able to mix a pinned tab into normal tabs");

  gBrowser.moveTabTo(tabs[2], 0);
  indexTest(2, 2, "shouldn't be able to mix a normal tab into pinned tabs");

  eh = new PinUnpinHandler(tabs[1], "TabUnpin");
  gBrowser.unpinTab(tabs[1]);
  is(eh.eventCount, 2, "TabUnpin event should be fired");
  indexTest(1, 1, "unpinning a tab should move a tab to the start of normal tabs");

  eh = new PinUnpinHandler(tabs[3], "TabUnpin");
  gBrowser.unpinTab(tabs[3]);
  is(eh.eventCount, 2, "TabUnpin event should be fired");
  indexTest(3, 0, "unpinning a tab should move a tab to the start of normal tabs");

  gBrowser.removeTab(tabs[1]);
  gBrowser.removeTab(tabs[2]);
  gBrowser.removeTab(tabs[3]);
}
