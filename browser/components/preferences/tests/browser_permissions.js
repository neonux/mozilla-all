/* ***** BEGIN LICENSE BLOCK *****
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/PlacesUtils.jsm");

const ABOUT_PERMISSIONS_SPEC = "about:permissions";
const TEST_URI = Services.io.newURI("http://mozilla.org/", null, null);
// used to set permissions on test sites
// UNKNOWN = 0, ALLOW = 1, DENY = 2, SESSION = 8
const TEST_PERMS = {
  "password": 1,
  "cookie": 1,
  "geo": 0,
  "indexedDB": 0,
  "install": 2,
  "popup": 2,
  "image": 1
};

// number of managed permissions in the interface
const PERMISSIONS_COUNT = 7;

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(cleanUp);

  // add test history visit
  PlacesUtils.history.addVisit(TEST_URI, Date.now() * 1000, null,
    Ci.nsINavHistoryService.TRANSITION_LINK, false, 0);

  for (let type in TEST_PERMS) {
    if (type != "password")
      Services.perms.add(aURI, type, TEST_PERMS[type]);
  }

  // create permissions exceptions for some sites to test that those sites are
  // gathered when the permissions services are enumerated
  function observer() {
    Services.obs.removeObserver(observer, "browser-permissions-initialized", false);
    runNextTest();
  }
  Services.obs.addObserver(observer, "browser-permissions-initialized", false);

  // open about:permissions
  gBrowser.loadURI("about:permissions");
}

function cleanUp() {
  function finishCleanUp() {
    for (let type in TEST_PERMS) {
      if (type != "password")
        Services.perms.remove(aURI.host, type);
    }
  }

  waitForClearHistory(finishCleanUp);
}

function runNextTest() {
  if (gTestIndex == tests.length) {
    finish();
    return;
  }

  let nextTest = tests[gTestIndex++];
  info("[" + nextTest.name + "] running test");
  nextTest();
}

var gTestIndex = 0;
var tests = [
  function test_page_load() {
    is(gBrowser.currentURI.spec, ABOUT_PERMISSIONS_SPEC, "about:permissions loaded");

    runNextTest();
  },

  function test_sites_list() {
    let sitesList = gBrowser.contentDocument.getElementById("sites-list");
    ok(sitesList, "sites list exists");
    ok(sitesList.children.length, "sites list has children");
    is(sitesList.firstChild.value, TEST_URI.host, "first site is expected site from history");

    sitesList.selectedIndex = 0;
    let siteLabel = gBrowser.contentDocument.getElementById("site-label");
    is(siteLabel.value, TEST_URI.host, "header updated for selected site");

    runNextTest();
  },

  function test_permissions() {
    let menulists = gBrowser.contentDocument.getElementsByClassName("pref-menulist");
    is(menulists.length, PERMISSIONS_COUNT, "expected number of managed permissions");

    for (let i = 0; i < menulists.length; i++) {
      let item = menulists.item(i);
      let type = item.getAttribute("type");
      is(item.value, TEST_PERMS[type], "got expected value for " + type + " permission");
    }

    runNextTest();
  },

  function test_set_cookie() {
    // check that cookie count 0
    
    // set a cookie on the site
    
    // check that cookie count is 1
    
    // click "clear cookies"
    
    // check that cookie count is 0
    
    runNextTest();
  }

];

// copied from toolkit/components/places/tests/head_common.js
function waitForClearHistory(aCallback) {
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, PlacesUtils.TOPIC_EXPIRATION_FINISHED);
      aCallback();
    }
  };
  Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);
  PlacesUtils.bhistory.removeAllPages();
}
