// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let baseURI = "http://mochi.test:8888/browser/mobile/chrome/";
let testURL_blank = baseURI + "browser_blank_01.html";

function testURL(n) {
  return baseURI + "browser_viewport.sjs?" + encodeURIComponent(gTestData[n].metadata);
}

let working_tab;

let loadUrl = function loadUrl(tab, url, callback) {
  messageManager.addMessageListener("MozScrolledAreaChanged", function(msg) {
    if (working_tab.browser.currentURI.spec == url) {
      messageManager.removeMessageListener(msg.name, arguments.callee);
      // HACK: Sometimes there are two MozScrolledAreaChanged messages in a
      // row, and this setTimeout is the only way I found to make sure the
      // browser responds to both of them before we do.
      setTimeout(callback, 0);
    }
  });
  BrowserUI.goToURI(url);
};

let gTestData = [
  { metadata: "", width: 980, scale: 1 },
  { metadata: "width=device-width, initial-scale=1", width: 533.33, scale: 1.5 },
  { metadata: "width=320, initial-scale=1", width: 533.33, scale: 1.5 },
  { metadata: "initial-scale=1.0, user-scalable=no", width: 533.33, scale: 1.5, disableZoom: true },
  { metadata: "width=200,height=500", width: 200, scale: 4 },
  { metadata: "width=2000, minimum-scale=0.75", width: 2000, scale: 1.125, minScale: 1.125 },
  { metadata: "width=100, maximum-scale=2.0", width: 266.67, scale: 3, maxScale: 3 },
  { metadata: "width=2000, initial-scale=0.75", width: 2000, scale: 1.125 },
  { metadata: "width=20000, initial-scale=100", width: 10000, scale: 4 },
  { metadata: "XHTML", width: 533.33, scale: 1.5, disableZoom: true },
  /* testing spaces between arguments (bug 572696) */
  { metadata: "width= 2000, minimum-scale=0.75", width: 2000, scale: 1.125 },
  { metadata: "width = 2000, minimum-scale=0.75", width: 2000, scale: 1.125 },
  { metadata: "width = 2000 , minimum-scale=0.75", width: 2000, scale: 1.125 },
  { metadata: "width = 2000 , minimum-scale =0.75", width: 2000, scale: 1.125 },
  { metadata: "width = 2000 , minimum-scale = 0.75", width: 2000, scale: 1.125 },
  { metadata: "width =  2000   ,    minimum-scale      =       0.75", width: 2000, scale: 1.125 }
];



//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();

  working_tab = Browser.addTab("about:blank", true);
  ok(working_tab, "Tab Opened");

  startTest(0);
}

function startTest(n) {
  BrowserUI.goToURI(testURL_blank);
  loadUrl(working_tab, testURL_blank, verifyBlank(n));
}

function verifyBlank(n) {
  return function() {
    // Do sanity tests
    var uri = working_tab.browser.currentURI.spec;
    is(uri, testURL_blank, "URL Matches blank page "+n);

    // Check viewport settings
    is(working_tab.browser.contentWindowWidth, 980, "Normal 'browser' width is 980 pixels");

    loadUrl(working_tab, testURL(n), verifyTest(n));
  }
}

function is_approx(actual, expected, fuzz, description) {
  ok(Math.abs(actual - expected) <= fuzz,
     description + " [got " + actual + ", expected " + expected + "]");
}

function verifyTest(n) {
  return function() {
    is(window.innerWidth, 800, "Test assumes window width is 800px");
    is(Services.prefs.getIntPref("zoom.dpiScale") / 100, 1.5, "Test assumes zoom.dpiScale is 1.5");

    // Do sanity tests
    var uri = working_tab.browser.currentURI.spec;
    is(uri, testURL(n), "URL is "+testURL(n));

    let data = gTestData[n];
    let actualWidth = working_tab.browser.contentWindowWidth;
    is_approx(actualWidth, parseFloat(data.width), .01, "Viewport width=" + data.width);

    let zoomLevel = getBrowser().scale;
    is_approx(zoomLevel, parseFloat(data.scale), .01, "Viewport scale=" + data.scale);

    // Test zooming
    if (data.disableZoom) {
      ok(!working_tab.allowZoom, "Zoom disabled");

      Browser.zoom(-1);
      is(getBrowser().scale, zoomLevel, "Zoom in does nothing");

      Browser.zoom(1);
      is(getBrowser().scale, zoomLevel, "Zoom out does nothing");
    }
    else {
      ok(Browser.selectedTab.allowZoom, "Zoom enabled");
    }


    if (data.minScale) {
      do { // Zoom out until we can't go any farther.
        zoomLevel = getBrowser().scale;
        Browser.zoom(1);
      } while (getBrowser().scale != zoomLevel);
      ok(getBrowser().scale >= data.minScale, "Zoom out limited");
    }

    if (data.maxScale) {
      do { // Zoom in until we can't go any farther.
        zoomLevel = getBrowser().scale;
        Browser.zoom(-1);
      } while (getBrowser().scale != zoomLevel);
      ok(getBrowser().scale <= data.maxScale, "Zoom in limited");
    }

    finishTest(n);
  }
}

function finishTest(n) {
  if (n+1 < gTestData.length) {
    startTest(n+1);
  } else {
    Browser.closeTab(working_tab);
    finish();
  }
}
