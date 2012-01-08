/*
 * This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/.
 */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Geometry.jsm");
Cu.import("resource:///modules/PageThumbs.jsm");
Cu.import("resource:///modules/NewTabUtils.jsm");

let {
  links: gLinks,
  allPages: gAllPages,
  pinnedLinks: gPinnedLinks,
  blockedLinks: gBlockedLinks
} = NewTabUtils;

XPCOMUtils.defineLazyGetter(this, "gStringBundle", function() {
  return Services.strings.
    createBundle("chrome://browser/locale/newTab.properties");
});

function newTabString(name) gStringBundle.GetStringFromName('newtab.' + name);

const HTML_NAMESPACE = "http://www.w3.org/1999/xhtml";
const THUMB_WIDTH = 201;
const THUMB_HEIGHT = 127;

#include batch.js
#include transformations.js
#include page.js
#include toolbar.js
#include grid.js
#include cells.js
#include sites.js
#include drag.js
#include drop.js
#include dropTargetShim.js
#include dropPreview.js
#include updater.js
