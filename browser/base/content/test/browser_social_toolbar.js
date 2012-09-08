/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    workerURL: "https://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "chrome://branding/content/icon48.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testProfileSet: function(next) {
    let profile = {
      portrait: "https://example.com/portrait.jpg",
      userName: "trickster",
      displayName: "Kuma Lisa",
      profileURL: "http://en.wikipedia.org/wiki/Kuma_Lisa"
    }
    Social.provider.updateUserProfile(profile);
    // check dom values
    let portrait = document.getElementById("social-statusarea-user-portrait").getAttribute("src");
    is(profile.portrait, portrait, "portrait is set");
    let userButton = document.getElementById("social-statusarea-username");
    ok(!userButton.hidden, "username is visible");
    is(userButton.label, profile.userName, "username is set");
    next();
  },
  testAmbientNotifications: function(next) {
    let ambience = {
      name: "testIcon",
      iconURL: "chrome://branding/content/icon48.png",
      contentPanel: "about:blank",
      counter: 42
    };
    Social.provider.setAmbientNotification(ambience);

    let statusIcons = document.getElementById("social-status-iconbox");
    ok(!statusIcons.firstChild.collapsed, "status icon is visible");
    ok(!statusIcons.firstChild.lastChild.collapsed, "status value is visible");
    is(statusIcons.firstChild.lastChild.textContent, "42", "status value is correct");

    ambience.counter = 0;
    Social.provider.setAmbientNotification(ambience);
    ok(statusIcons.firstChild.lastChild.collapsed, "status value is not visible");
    is(statusIcons.firstChild.lastChild.textContent, "", "status value is correct");
    next();
  },
  testProfileUnset: function(next) {
    Social.provider.updateUserProfile({});
    // check dom values
    let portrait = document.getElementById("social-statusarea-user-portrait").getAttribute("src");
    is(portrait, "chrome://browser/skin/social/social.png", "portrait is generic");
    let userButton = document.getElementById("social-statusarea-username");
    ok(userButton.hidden, "username is not visible");
    let ambience = document.getElementById("social-status-iconbox").firstChild;
    while (ambience) {
      ok(ambience.collapsed, "ambient icon (" + ambience.id + ") is collapsed");
      ambience = ambience.nextSibling;
    }
    
    next();
  }
}

