function test() {
  while (gBrowser.mTabs.length > 1)
    gBrowser.removeCurrentTab();

  gBrowser.addTab();
  gBrowser.addTab();
  gBrowser.addTab();

  ctrlTabTest([2]      , 1, 0);
  ctrlTabTest([2, 3, 1], 2, 2);
  ctrlTabTest([]       , 4, 2);

  gBrowser.removeTab(gBrowser.tabContainer.lastChild);

  ctrlTabTest([2, 1, 0], 7, 1);

  gBrowser.removeTab(gBrowser.tabContainer.lastChild);

  ctrlTabTest([1]      , 1, 0);

  gBrowser.removeTab(gBrowser.tabContainer.lastChild);
}

function ctrlTabTest(tabsToSelect, tabTimes, expectedIndex) {
  tabsToSelect.forEach(function (index) {
    gBrowser.selectedTab = gBrowser.mTabs[index];
  });

  var indexStart = gBrowser.tabContainer.selectedIndex;
  var tabCount = gBrowser.mTabs.length;
  var normalized = tabTimes % tabCount;
  var where = normalized == 1 ? "back to the previously selected tab" :
              normalized + " tabs back in most-recently-selected order";

  for (let i = 0; i < tabTimes; i++) {
    EventUtils.synthesizeKey("VK_TAB", { ctrlKey: true });

    if (tabCount > 2)
     is(gBrowser.tabContainer.selectedIndex, indexStart,
       "Selected tab doesn't change while tabbing");
  }

  if (tabCount > 2) {
    ok(ctrlTab.panel.state == "showing" || ctrlTab.panel.state == "open",
       "With " + tabCount + " tabs open, Ctrl+Tab opens the preview panel");

    is(ctrlTab.label.value, gBrowser.mTabs[expectedIndex].label,
       "Preview panel displays label of expected tab");

    EventUtils.synthesizeKey("VK_CONTROL", { type: "keyup" });

    ok(ctrlTab.panel.state == "hiding" || ctrlTab.panel.state == "closed",
       "Releasing Ctrl closes the preview panel");
  } else {
    ok(ctrlTab.panel.state == "hiding" || ctrlTab.panel.state == "closed",
       "With " + tabCount + " tabs open, Ctrl+Tab doesn't open the preview panel");
  }

  is(gBrowser.tabContainer.selectedIndex, expectedIndex,
     "With "+ tabCount +" tabs open and tab " + indexStart
     + " selected, Ctrl+Tab*" + tabTimes + " goes " + where);
}
