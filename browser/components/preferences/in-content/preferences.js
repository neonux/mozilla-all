/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

function init_all() {
  gMainPane.init();
#ifdef XP_WIN
  gTabsPane.init();
#endif
  gPrivacyPane.init();
  gAdvancedPane.init();
  gApplicationsPane.init();
  gContentPane.init();
  gSyncPane.init();
  gSecurityPane.init();

  window.history.replaceState("landing",document.title);
  window.addEventListener("popstate", onStatePopped, true);
  updateCommands();
}

function gotoPref(page) {
  search(page, 'data-category');
  window.history.pushState(page,document.title);
  updateCommands();
}

function cmd_back() {
  window.history.back();
}

function cmd_forward() {
  window.history.forward();
}

function onStatePopped(aEvent) {
  updateCommands();
  // TODO To ensure we can't go forward again we put an additional entry
  // for the current state into the history. Ideally we would just strip
  // the history but there doesn't seem to be a way to do that. Bug 590661
  search(aEvent.state, 'data-category');
}

function updateCommands() {
  if(canGoBack()) {
    document.getElementById("back-btn").disabled = false;
  } else {
    document.getElementById("back-btn").disabled = true;
  }

  if(canGoForward()) {
    document.getElementById("forward-btn").disabled = false;
  } else {
    document.getElementById("forward-btn").disabled = true;
  }
}

function canGoBack() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .canGoBack;
}

function canGoForward() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .canGoForward;
}
