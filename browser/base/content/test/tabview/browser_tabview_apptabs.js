/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;

  // establish initial state
  is(contentWindow.GroupItems.groupItems.length, 1,
      "we start with one group (the default)");
  is(gBrowser.tabs.length, 1, "we start with one tab");
  let originalTab = gBrowser.tabs[0];

  // create a group
  let box = new contentWindow.Rect(20, 20, 180, 180);
  let groupItemOne = new contentWindow.GroupItem([],
      { bounds: box, title: "test1" });
  is(contentWindow.GroupItems.groupItems.length, 2, "we now have two groups");
  contentWindow.GroupItems.setActiveGroupItem(groupItemOne);

  // create a tab
  let xulTab = gBrowser.loadOneTab("about:blank");
  is(gBrowser.tabs.length, 2, "we now have two tabs");
  is(groupItemOne._children.length, 1, "the new tab was added to the group");

  // make sure the group has no app tabs
  is(appTabCount(groupItemOne), 0, "there are no app tab icons");

  // pin the tab, make sure the TabItem goes away and the icon comes on
  gBrowser.pinTab(xulTab);
  is(groupItemOne._children.length, 0,
      "the app tab's TabItem was removed from the group");
  is(appTabCount(groupItemOne), 1, "there's now one app tab icon");

  // create a second group and make sure it gets the icon too
  box.offset(box.width + 20, 0);
  let groupItemTwo = new contentWindow.GroupItem([],
      { bounds: box, title: "test2" });
  is(contentWindow.GroupItems.groupItems.length, 3, "we now have three groups");
  is(appTabCount(groupItemTwo), 1,
      "there's an app tab icon in the second group");

  // unpin the tab, make sure the icon goes away and the TabItem comes on
  gBrowser.unpinTab(xulTab);
  is(groupItemOne._children.length, 1, "the app tab's TabItem is back");
  is(appTabCount(groupItemOne), 0, "the icon is gone from group one");
  is(appTabCount(groupItemTwo), 0, "the icon is gone from group 2");

  // pin the tab again
  gBrowser.pinTab(xulTab);

  // close the second group
  groupItemTwo.close();

  // find app tab in group and hit it
  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    ok(!TabView.isVisible(),
        "Tab View is hidden because we clicked on the app tab");

    // delete the app tab and make sure its icon goes away
    gBrowser.removeTab(xulTab);
    is(appTabCount(groupItemOne), 0, "closing app tab removes its icon");

    // clean up
    groupItemOne.close();

    is(contentWindow.GroupItems.groupItems.length, 1,
        "we finish with one group");
    is(gBrowser.tabs.length, 1, "we finish with one tab");
    ok(!TabView.isVisible(), "we finish with Tab View not visible");

    finish();
  };

  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  let appTabIcons = groupItemOne.container.getElementsByClassName("appTabIcon");
  EventUtils.sendMouseEvent({ type: "click" }, appTabIcons[0], contentWindow);
}

function appTabCount(groupItem) {
  return groupItem.container.getElementsByClassName("appTabIcon").length;
}
