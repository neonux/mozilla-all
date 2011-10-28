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

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Cr = Components.results;
const Ci = Components.interfaces;

const SCHEME = "moz-thumb";
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

// TODO
function debug(aMsg) {
  aMsg = ("Thumbnails: " + aMsg).replace(/\S{80}/g, "$&\n");
  Services.console.logStringMessage(aMsg);
}

// TODO
function trace(aMsg) {
  // cut off the first line of the stack trace, because that's just this function.
  let stack = Error().stack.split("\n").slice(1);

  debug("trace: " + aMsg + "\n" + stack.join("\n"));
}

// TODO
function extend(aParent, aObject) {
  function copy(aSource, aDest) {
    for (let name in aSource) {
      let getter, setter;

      if (getter = aSource.__lookupGetter__(name))
        aDest.__defineGetter__(name, getter);
      else if (setter = aSource.__lookupSetter__(name))
        aDest.__defineSetter__(name, setter);
      else
       aDest[name] = aSource[name];
    }

    return aDest;
  }

  return copy(aObject, copy(aParent, {}));
}

// TODO
function URLParser(aUri) {
  this._uri = aUri;
}

URLParser.prototype = {
  parse: function UP_parse() {
    if (this._parsed)
      return this._parsed;

    return this._parsed = this._parseParams();
  },

  _parseParams: function UP_parseParams() {
    let params = {};
    let query = this._uri.spec.split("?")[1] || "";

    query.split("&").forEach(function (param) {
      let [key, value] = param.split("=").map(decodeURIComponent);
      params[key.toLowerCase()] = value;
    });

    return params;
  }
};

#include protocol.js
#include baseChannel.js
#include redirectChannel.js
#include cacheChannel.js
#include generateChannel.js
#include cache.js
#include thumbnailer.js
