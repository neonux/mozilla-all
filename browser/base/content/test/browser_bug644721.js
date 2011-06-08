/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the disablechrome attribute gets propogated to the main UI

const HTTPSRC = "http://example.com/browser/browser/base/content/test/";
let gGen; // test steps generator

function isVisible(element) {
  if (typeof element == "string")
    element = document.getElementById(element);

  let bo = element.boxObject;
  return (bo.height > 0 && bo.width > 0);
}

function testSteps() {

  // Part 1: test setup
  let regularTab = gBrowser.addTab("http://www.example.com", {skipAnimation: true});
  let pinnedTab = gBrowser.addTab("http://www.example.com", {skipAnimation: true});
  gBrowser.pinTab(pinnedTab);

  continueOn(regularTab.linkedBrowser, "pageshow"); 
  continueOn(pinnedTab.linkedBrowser, "pageshow");
  yield; yield;


  //Part 2 - test toolbar visibility for regular and pinned tabs 
  gBrowser.selectedTab = pinnedTab;
  ok(!isVisible("nav-bar"), "Navigation toolbar should be hidden for pinned tabs");

  gBrowser.selectedTab = regularTab;
  ok(isVisible("nav-bar"), "Navigation toolbar should be visible for regular tabs");


  // Part 3 - test special URLs
  gBrowser.selectedTab = pinnedTab;

  pinnedTab.linkedBrowser.loadURI("about:home");
  continueOn(pinnedTab.linkedBrowser, "pageshow"); yield;

  ok(isVisible("nav-bar"), 'Navigation toolbar should be visible for about:home');

  pinnedTab.linkedBrowser.loadURI("about:blank");
  continueOn(pinnedTab.linkedBrowser, "pageshow"); yield;

  ok(isVisible("nav-bar"), 'Navigation toolbar should be visible for about:blank');

  pinnedTab.linkedBrowser.loadURI("about:certerror?");
  continueOn(pinnedTab.linkedBrowser, "pageshow"); yield;

  ok(isVisible("nav-bar"), 'Navigation toolbar should be visible for about:certerror');

  pinnedTab.linkedBrowser.loadURI("about:neterror?");
  continueOn(pinnedTab.linkedBrowser, "pageshow"); yield;

  ok(isVisible("nav-bar"), 'Navigation toolbar should be visible for about:neterror');

  pinnedTab.linkedBrowser.loadURI("about:blocked?");
  continueOn(pinnedTab.linkedBrowser, "pageshow"); yield;

  ok(isVisible("nav-bar"), 'Navigation toolbar should be visible for about:blocked');


  // Part 4 - test site-specific prefs
  pinnedTab.linkedBrowser.loadURI(HTTPSRC + "dummy_page.html");
  continueOn(pinnedTab.linkedBrowser, "pageshow"); yield;

  ok(!isVisible("nav-bar"), "Navigation toolbar should be hidden for non-preffed-off site");

  ChromelessAppTabs.togglePrefForTab({getAttribute: function() "true"}, pinnedTab);
  ok(isVisible("nav-bar"), "Navigation toolbar should be visible for site-specific pref");
  
  ChromelessAppTabs.togglePrefForTab({getAttribute: function() ""}, pinnedTab);
  ok(!isVisible("nav-bar"), "Pref-off pref-on works");


  // Part 5 - test global pref
  // the pref is not live, so simulate it changing
  ChromelessAppTabs.enabled = false;
  gBrowser.selectedTab = regularTab;
  gBrowser.selectedTab = pinnedTab;
  ok(isVisible("nav-bar"), "Navigation toolbar should be visible with global pref turned off");

  ChromelessAppTabs.enabled = true;
  gBrowser.selectedTab = regularTab;
  gBrowser.selectedTab = pinnedTab;
  ok(!isVisible("nav-bar"), "Navigation toolbar should be hidden with global pref turned off");


  // Part 6 - test temporary display on Ctrl+L
  EventUtils.synthesizeKey("l", { accelKey: true });
  ok(isVisible("nav-bar"), "Navigation toolbar should be shown on Ctrl+L");

  gBrowser.selectedTab = regularTab;
  gBrowser.selectedTab = pinnedTab;
  ok(!isVisible("nav-bar"), "Switching tabs hides nav bar");

  EventUtils.synthesizeKey("l", { accelKey: true });
  ok(isVisible("nav-bar"), "Navigation toolbar should be shown on Ctrl+L [2]");
  pinnedTab.click();
  ok(!isVisible("nav-bar"), "Clicking outside hides nav bar");


  // Test clean-up
  gBrowser.removeTab(regularTab);
  gBrowser.removeTab(pinnedTab);
  finish();
}

function continueTest() {
  executeSoon(function() gGen.next());
}

function continueOn(element, eventName) {
  element.addEventListener(eventName, function listener() {
    element.removeEventListener(eventName, listener, false);
    continueTest();
  }, false);
}

function test() {
  waitForExplicitFinish();
  gGen = testSteps();
  continueTest();
}
