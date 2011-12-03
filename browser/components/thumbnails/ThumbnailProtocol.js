#ifdef 0
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Thumbnails code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Taubert <ttaubert@mozilla.com> (Original Author)
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

/**
 * Thumbnail Protocol
 *
 * Overview
 * This service provides a protocol implementation that allows the simple
 * creation and caching of thumbnails for URLs.
 */
#endif

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Cr = Components.results;
const Ci = Components.interfaces;

// define the scheme to use - "moz-thumb://..."
const SCHEME = "moz-thumb";

// define the image format of our thumbnails
const CONTENT_TYPE = "image/png";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gAppShellService",
                                   "@mozilla.org/appshell/appShellService;1",
                                   "nsIAppShellService");

XPCOMUtils.defineLazyServiceGetter(this, "gCacheService",
                                   "@mozilla.org/network/cache-service;1",
                                   "nsICacheService");

XPCOMUtils.defineLazyGetter(this, "gNetUtil", function () {
  var obj = {};
  Cu.import("resource://gre/modules/NetUtil.jsm", obj);
  return obj.NetUtil;
});

#include helper.js
#include protocol.js
#include channel.js
#include cacheChannel.js
#include generateChannel.js
#include cache.js
#include thumbnailer.js
