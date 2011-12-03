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
#endif

// ----------
// Function: debug
// Logs a given debug message to the console.
//
// Parameters:
//   aMsg - the log message
function debug(aMsg) {
  aMsg = ("Thumbnails: " + aMsg).replace(/\S{80}/g, "$&\n");
  Services.console.logStringMessage(aMsg);
}

// ----------
// Function: trace
// Logs a given debug message to the console and prints the current stack trace.
//
// Parameters:
//   aMsg - the log message
function trace(aMsg) {
  // cut off the first line of the stack trace, because that's just this function.
  let stack = Error().stack.split("\n").slice(1);

  debug("trace: " + aMsg + "\n" + stack.join("\n"));
}

// ----------
// Function: parseUri
// Parses the given URI and returns its parameters.
//
// Parameters:
//   aUri - the URI to parse
function parseUri(aUri) {
  let params = {};
  let [host, query] = aUri.spec.split("?");

  query.split("&").forEach(function (param) {
    let [key, value] = param.split("=").map(decodeURIComponent);
    params[key.toLowerCase()] = value;
  });

  params.algorithm = host.replace(/^.*\/+/, "").replace(/\/+.*$/, "");

  return params;
}

// ----------
// Function: getCacheKey
// Returns the cache key to use for the given thumbnail URI. This puts params
// in a fixed order and removes params that must not be included in the key.
//
// Parameters:
//   aUri - the URI to get the cache key for
function getCacheKey(aUri) {
  let keys = ["algorithm", "url", "width", "height"];
  let params = parseUri(aUri);

  let pairs = keys.map(function (key) {
    let value = encodeURIComponent(params[key] || "");
    return encodeURIComponent(key) + "=" + value;
  });

  return pairs.join("&");
}
