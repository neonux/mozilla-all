/* -*- Mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil; -*- */
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
 * The Original Code is BrowserCLH.js
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Crowder <crowder@fiverocks.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function openWindow(aParent, aURL, aTarget, aFeatures, aArgs) {
  let argString = null;
  if (aArgs) {
    argString = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    argString.data = aArgs;
  }

  return Services.ww.openWindow(aParent, aURL, aTarget, aFeatures, argString);
}

function resolveURIInternal(aCmdLine, aArgument) {
  let uri = aCmdLine.resolveURI(aArgument);

  if (!(uri instanceof Ci.nsIFileURL))
    return uri;

  try {
    if (uri.file.exists())
      return uri;
  }
  catch (e) {
    Cu.reportError(e);
  }

  try {
    let urifixup = Cc["@mozilla.org/docshell/urifixup;1"].getService(Ci.nsIURIFixup);
    uri = urifixup.createFixupURI(aArgument, 0);
  }
  catch (e) {
    Cu.reportError(e);
  }

  return uri;
}

/**
 * Determines whether a home page override is needed.
 * Returns:
 *  "new profile" if this is the first run with a new profile.
 *  "new version" if this is the first run with a build with a different
 *                      Gecko milestone (i.e. right after an upgrade).
 *  "none" otherwise.
 */
function needHomepageOverride() {
  let savedmstone = null;
  try {
    savedmstone = Services.prefs.getCharPref("browser.startup.homepage_override.mstone");
  } catch (e) {}

  if (savedmstone == "ignore")
    return "none";

#expand    let ourmstone = "__MOZ_APP_VERSION__";

  if (ourmstone != savedmstone) {
    Services.prefs.setCharPref("browser.startup.homepage_override.mstone", ourmstone);

    return (savedmstone ? "new version" : "new profile");
  }

  return "none";
}

function BrowserCLH() { }

BrowserCLH.prototype = {
  //
  // nsICommandLineHandler
  //
  handle: function fs_handle(aCmdLine) {
    // Instantiate the search service so the search engine cache is created now
    // instead when the application is running. The install process will register
    // this component by using the -silent command line flag, thereby creating
    // the cache during install, not runtime.
    // NOTE: This code assumes this CLH is run before the nsDefaultCLH, which
    // consumes the "-silent" flag.
    if (aCmdLine.findFlag("silent", false) > -1) {
      let searchService = Services.search;
      let autoComplete = Cc["@mozilla.org/autocomplete/search;1?name=history"].
                         getService(Ci.nsIAutoCompleteSearch);
      return;
    }

    // Handle chrome windows loaded via commandline
    let chromeParam = aCmdLine.handleFlagWithParam("chrome", false);
    if (chromeParam) {
      try {
        // Only load URIs which do not inherit chrome privs
        let features = "chrome,dialog=no,all";
        let uri = resolveURIInternal(aCmdLine, chromeParam);
        let netutil = Cc["@mozilla.org/network/util;1"].getService(Ci.nsINetUtil);
        if (!netutil.URIChainHasFlags(uri, Ci.nsIHttpProtocolHandler.URI_INHERITS_SECURITY_CONTEXT)) {
          openWindow(null, uri.spec, "_blank", features, null);

          // Stop the normal commandline processing from continuing
          aCmdLine.preventDefault = true;
        }
      }
      catch (e) {
        Cu.reportError(e);
      }
      return;
    }

    // Keep an array of possible URL arguments
    let uris = [];

    // Check for the "url" flag
    let uriFlag = aCmdLine.handleFlagWithParam("url", false);
    if (uriFlag) {
      let uri = resolveURIInternal(aCmdLine, uriFlag);
      if (uri)
        uris.push(uri);
    }

    for (let i = 0; i < aCmdLine.length; i++) {
      let arg = aCmdLine.getArgument(i);
      if (!arg || arg[0] == '-')
        continue;

      let uri = resolveURIInternal(aCmdLine, arg);
      if (uri)
        uris.push(uri);
    }

    // Open the main browser window, if we don't already have one
    let win;
    try {
      win = Services.wm.getMostRecentWindow("navigator:browser");
      if (!win) {
        let defaultURL;

        // Override the default if we have a new profile
        if (needHomepageOverride() == "new profile")
            defaultURL = "about:firstrun";

        // Override the default if we have a URL passed on command line
        if (uris.length > 0) {
          defaultURL = uris[0].spec;
          uris = uris.slice(1);
        }

        win = openWindow(null, "chrome://browser/content/browser.xul", "_blank", "chrome,dialog=no,all", defaultURL);
      }

      win.focus();

      // Stop the normal commandline processing from continuing. We just opened the main browser window
      aCmdLine.preventDefault = true;
    } catch (e) { }

    // Assumption: All remaining command line arguments have been sent remotely (browser is already running)
    // Action: Open any URLs we find into an existing browser window

    // First, get a browserDOMWindow object
    while (!win.browserDOMWindow)
      Services.tm.currentThread.processNextEvent(true);

    // Open any URIs into new tabs
    for (let i = 0; i < uris.length; i++)
      win.browserDOMWindow.openURI(uris[i], null, Ci.nsIBrowserDOMWindow.OPEN_NEWTAB,
                                   Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),

  // XPCOMUtils factory
  classID: Components.ID("{be623d20-d305-11de-8a39-0800200c9a66}"),
};

var components = [ BrowserCLH ];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
