var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir;

var gTestBrowser = null;
var gNextTest = null;
var gClickToPlayPluginActualEvents = 0;
var gClickToPlayPluginExpectedEvents = 6;

function get_test_plugin() {
  var ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = ph.getPluginTags();

  // Find the test plugin
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in")
      return tags[i];
  }
  ok(false, "Unable to find plugin");
}

Components.utils.import("resource://gre/modules/Services.jsm");

// This listens for the next opened tab and checks it is of the right url.
// opencallback is called when the new tab is fully loaded
// closecallback is called when the tab is closed
function TabOpenListener(url, opencallback, closecallback) {
  this.url = url;
  this.opencallback = opencallback;
  this.closecallback = closecallback;

  gBrowser.tabContainer.addEventListener("TabOpen", this, false);
}

TabOpenListener.prototype = {
  url: null,
  opencallback: null,
  closecallback: null,
  tab: null,
  browser: null,

  handleEvent: function(event) {
    if (event.type == "TabOpen") {
      gBrowser.tabContainer.removeEventListener("TabOpen", this, false);
      this.tab = event.originalTarget;
      this.browser = this.tab.linkedBrowser;
      gBrowser.addEventListener("pageshow", this, false);
    } else if (event.type == "pageshow") {
      if (event.target.location.href != this.url)
        return;
      gBrowser.removeEventListener("pageshow", this, false);
      this.tab.addEventListener("TabClose", this, false);
      var url = this.browser.contentDocument.location.href;
      is(url, this.url, "Should have opened the correct tab");
      this.opencallback(this.tab, this.browser.contentWindow);
    } else if (event.type == "TabClose") {
      if (event.originalTarget != this.tab)
        return;
      this.tab.removeEventListener("TabClose", this, false);
      this.opencallback = null;
      this.tab = null;
      this.browser = null;
      // Let the window close complete
      executeSoon(this.closecallback);
      this.closecallback = null;
    }
  }
};

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() { Services.prefs.clearUserPref("plugins.click_to_play"); });
  Services.prefs.setBoolPref("plugins.click_to_play", false);

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  gTestBrowser.addEventListener("load", pageLoad, true);
  gTestBrowser.addEventListener("PluginClickToPlay", handlePluginClickToPlay, true);
  prepareTest(test1, gTestRoot + "plugin_unknown.html");
}

function finishTest() {
  gTestBrowser.removeEventListener("load", pageLoad, true);
  gTestBrowser.removeEventListener("PluginClickToPlay", handlePluginClickToPlay, true);
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function handlePluginClickToPlay() {
  gClickToPlayPluginActualEvents++;
}

function pageLoad() {
  // The plugin events are async dispatched and can come after the load event
  // This just allows the events to fire before we then go on to test the states
  executeSoon(gNextTest);
}

function prepareTest(nextTest, url) {
  gNextTest = nextTest;
  gTestBrowser.contentWindow.location = url;
}

// Tests a page with an unknown plugin in it.
function test1() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(notificationBox.getNotificationWithValue("missing-plugins"), "Test 1, Should have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 1, Should not have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 1, Should be a missing plugin list");
  ok("application/x-unknown" in gTestBrowser.missingPlugins, "Test 1, Should know about application/x-unknown");
  ok(!("application/x-test" in gTestBrowser.missingPlugins), "Test 1, Should not know about application/x-test");

  var plugin = get_test_plugin();
  ok(plugin, "Should have a test plugin");
  plugin.disabled = false;
  plugin.blocklisted = false;
  prepareTest(test2, gTestRoot + "plugin_test.html");
}

// Tests a page with a working plugin in it.
function test2() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 2, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 2, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 2, Should not be a missing plugin list");

  var plugin = get_test_plugin();
  ok(plugin, "Should have a test plugin");
  plugin.disabled = true;
  prepareTest(test3, gTestRoot + "plugin_test.html");
}

// Tests a page with a disabled plugin in it.
function test3() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 3, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 3, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 3, Should not be a missing plugin list");

  new TabOpenListener("about:addons", test4, prepareTest5);

  var pluginNode = gTestBrowser.contentDocument.getElementById("test");
  ok(pluginNode, "Test 3, Found plugin in page");
  var manageLink = gTestBrowser.contentDocument.getAnonymousElementByAttribute(pluginNode, "class", "managePluginsLink");
  ok(manageLink, "Test 3, found 'manage' link in plugin-problem binding");

  EventUtils.synthesizeMouse(manageLink,
                             5, 5, {}, gTestBrowser.contentWindow);
}

function test4(tab, win) {
  is(win.wrappedJSObject.gViewController.currentViewId, "addons://list/plugin", "Should have displayed the plugins pane");
  gBrowser.removeTab(tab);
}

function prepareTest5() {
  var plugin = get_test_plugin();
  plugin.disabled = false;
  plugin.blocklisted = true;
  prepareTest(test5, gTestRoot + "plugin_test.html");
}

// Tests a page with a blocked plugin in it.
function test5() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 5, Should not have displayed the missing plugin notification");
  ok(notificationBox.getNotificationWithValue("blocked-plugins"), "Test 5, Should have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 5, Should be a missing plugin list");
  ok("application/x-test" in gTestBrowser.missingPlugins, "Test 5, Should know about application/x-test");
  ok(!("application/x-unknown" in gTestBrowser.missingPlugins), "Test 5, Should not know about application/x-unknown");

  prepareTest(test6, gTestRoot + "plugin_both.html");
}

// Tests a page with a blocked and unknown plugin in it.
function test6() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(notificationBox.getNotificationWithValue("missing-plugins"), "Test 6, Should have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 6, Should not have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 6, Should be a missing plugin list");
  ok("application/x-unknown" in gTestBrowser.missingPlugins, "Test 6, Should know about application/x-unknown");
  ok("application/x-test" in gTestBrowser.missingPlugins, "Test 6, Should know about application/x-test");

  prepareTest(test7, gTestRoot + "plugin_both2.html");
}

// Tests a page with a blocked and unknown plugin in it (alternate order to above).
function test7() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(notificationBox.getNotificationWithValue("missing-plugins"), "Test 7, Should have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 7, Should not have displayed the blocked plugin notification");
  ok(gTestBrowser.missingPlugins, "Test 7, Should be a missing plugin list");
  ok("application/x-unknown" in gTestBrowser.missingPlugins, "Test 7, Should know about application/x-unknown");
  ok("application/x-test" in gTestBrowser.missingPlugins, "Test 7, Should know about application/x-test");

  var plugin = get_test_plugin();
  plugin.disabled = false;
  plugin.blocklisted = false;
  Services.prefs.setBoolPref("plugins.click_to_play", true);

  prepareTest(test8, gTestRoot + "plugin_test.html");
}

// Tests a page with a working plugin that is click-to-play
function test8() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 8, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 8, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 8, Should not be a missing plugin list");
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 8, Should have a click-to-play notification");

  prepareTest(test9a, gTestRoot + "plugin_test2.html");
}

// Tests that activating one click-to-play plugin will activate the other plugins (part 1/1)
function test9a() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 9a, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 9a, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 9a, Should not be a missing plugin list");
  ok(PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 9a, Should have a click-to-play notification");
  var plugin1 = gTestBrowser.contentDocument.getElementById("test");
  var doc = gTestBrowser.contentDocument;
  var plugins = [];
  plugins.push(doc.getElementById("test"));
  plugins.push(doc.getElementById("test1"));
  plugins.push(doc.getElementById("test2"));
  plugins.forEach(function(plugin) {
    var rect = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox").getBoundingClientRect();
    ok(rect.width == 200, "Test 9a, Plugin with id=" + plugin.id + " overlay rect should have 200px width before being clicked");
    ok(rect.height == 200, "Test 9a, Plugin with id=" + plugin.id + " overlay rect should have 200px height before being clicked");
    var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(!objLoadingContent.activated, "Test 9a, Plugin with id=" + plugin.id + " should not be activated");
  });

  EventUtils.synthesizeMouse(plugin1, 100, 100, { });
  setTimeout(test9b, 0);
}

// Tests that activating one click-to-play plugin will activate the other plugins (part 2/2)
function test9b() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 9b, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 9b, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 9b, Should not be a missing plugin list");
  ok(!PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser), "Test 9b, Click to play notification should be removed now");
  var doc = gTestBrowser.contentDocument;
  var plugins = [];
  plugins.push(doc.getElementById("test"));
  plugins.push(doc.getElementById("test1"));
  plugins.push(doc.getElementById("test2"));
  plugins.forEach(function(plugin) {
    var pluginRect = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox").getBoundingClientRect();
    ok(pluginRect.width == 0, "Test 9b, Plugin with id=" + plugin.id + " should have click-to-play overlay with zero width");
    ok(pluginRect.height == 0, "Test 9b, Plugin with id=" + plugin.id + " should have click-to-play overlay with zero height");
    var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
    ok(objLoadingContent.activated, "Test 9b, Plugin with id=" + plugin.id + " should be activated");
  });

  prepareTest(test10a, gTestRoot + "plugin_test3.html");
}

// Tests that activating a hidden click-to-play plugin through the notification works (part 1/2)
function test10a() {
  var notificationBox = gBrowser.getNotificationBox(gTestBrowser);
  ok(!notificationBox.getNotificationWithValue("missing-plugins"), "Test 10a, Should not have displayed the missing plugin notification");
  ok(!notificationBox.getNotificationWithValue("blocked-plugins"), "Test 10a, Should not have displayed the blocked plugin notification");
  ok(!gTestBrowser.missingPlugins, "Test 10a, Should not be a missing plugin list");
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 10a, Should have a click-to-play notification");
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(!objLoadingContent.activated, "Test 10c, Plugin should not be activated");

  popupNotification.mainAction.callback();
  setTimeout(test10b, 0);
}

// Tests that activating a hidden click-to-play plugin through the notification works (part 2/2)
function test10b() {
  var plugin = gTestBrowser.contentDocument.getElementById("test");
  var objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
  ok(objLoadingContent.activated, "Test 10c, Plugin should be activated");

  prepareTest(test11a, gTestRoot + "plugin_test3.html");
}

// Tests that the going back will reshow the notification for click-to-play plugins (part 1/3)
function test11a() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(popupNotification, "Test 11a, Should have a click-to-play notification");

  prepareTest(test11b, "about:blank");
}

// Tests that the going back will reshow the notification for click-to-play plugins (part 2/3)
function test11b() {
  var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
  ok(!popupNotification, "Test 11b, Should not have a click-to-play notification");

  gTestBrowser.addEventListener("pageshow", test11c, false);
  gTestBrowser.contentWindow.history.back();
}

// Tests that the going back will reshow the notification for click-to-play plugins (part 3/3)
function test11c() {
  gTestBrowser.removeEventListener("pageshow", test11c, false);
  // Make sure that the event handlers for pageshow can execute before checking for their effects.
  executeSoon(function() {
    todo(false, "The following test that checks for the notification fails intermittently, bug 742619.");
    //var popupNotification = PopupNotifications.getNotification("click-to-play-plugins", gTestBrowser);
    //ok(popupNotification, "Test 11c, Should have a click-to-play notification");
    is(gClickToPlayPluginActualEvents, gClickToPlayPluginExpectedEvents,
       "There should be a PluginClickToPlay event for each plugin that was " +
       "blocked due to the plugins.click_to_play pref");
    finishTest();
  });
}
