/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Downloads Panel Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * This component implements the nsIDownloadManagerUI interface and opens the
 * downloads panel in the most recent browser window when requested.
 *
 * If a specific preference is set, this component transparently forwards all
 * calls to the original implementation in Toolkit, that shows the window UI.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gBrowserGlue",
                                   "@mozilla.org/browser/browserglue;1",
                                   "nsIBrowserGlue");

////////////////////////////////////////////////////////////////////////////////
//// DownloadsUI

function DownloadsUI()
{
  XPCOMUtils.defineLazyGetter(this, "_toolkitUI", function() {
    // Create Toolkit's nsIDownloadManagerUI implementation.
    return Components.classesByID["{7dfdf0d1-aff6-4a34-bad1-d0fe74601642}"]
                     .getService(Ci.nsIDownloadManagerUI);
  });
}

DownloadsUI.prototype = {
  classID: Components.ID("{4d99321e-d156-455b-81f7-e7aa2308134f}"),

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadManagerUI]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadManagerUI

  show: function DUI_show(aWindowContext, aID, aReason)
  {
    if (DownloadsCommon.useWindowUI) {
      this._toolkitUI.show(aWindowContext, aID, aReason);
      return;
    }

    if (!aReason) {
      aReason = Ci.nsIDownloadManagerUI.REASON_USER_INTERACTED;
    }

    // Show the panel in the most recent browser window, if present.
    let browserWin = gBrowserGlue.getMostRecentBrowserWindow();
    if (browserWin) {
      browserWin.focus();
      if (aReason == Ci.nsIDownloadManagerUI.REASON_NEW_DOWNLOAD) {
        if (this.firstDownloadShown) {
          // For new downloads after the first one in the session, don't show
          // the panel automatically, but provide a visible notification in the
          // topmost browser window, if the status indicator is already visible.
          browserWin.DownloadsIndicatorView.showEventNotification();
          return;
        }
        this.firstDownloadShown = true;
      }
      browserWin.DownloadsPanel.showPanel();
      return;
    }

    // If no browser window is visible and the user requested to show the
    // current downloads, try and open a new window.
    if (aReason == Ci.nsIDownloadManagerUI.REASON_USER_INTERACTED) {
      // We'll open the panel when delayed loading is finished.
      Services.obs.addObserver(function DUIO_observe(aSubject, aTopic, aData) {
        Services.obs.removeObserver(DUIO_observe, aTopic);
        aSubject.DownloadsPanel.showPanel();
      }, "browser-delayed-startup-finished", false);

      // We must really build an empty arguments list for the new window.
      let windowFirstArg = Cc["@mozilla.org/supports-string;1"]
                           .createInstance(Ci.nsISupportsString);
      let windowArgs = Cc["@mozilla.org/supports-array;1"]
                       .createInstance(Ci.nsISupportsArray);
      windowArgs.AppendElement(windowFirstArg);
      Services.ww.openWindow(null, "chrome://browser/content/browser.xul",
                             null, "chrome,dialog=no,all", windowArgs);
    }
  },

  get visible()
  {
    if (DownloadsCommon.useWindowUI) {
      return this._toolkitUI.visible;
    }

    let browserWin = gBrowserGlue.getMostRecentBrowserWindow();
    return browserWin ? browserWin.DownloadsPanel.isPanelShowing : false;
  },

  getAttention: function DUI_getAttention()
  {
    if (DownloadsCommon.useWindowUI) {
      this._toolkitUI.getAttention();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Private

  /**
   * Set to true after the first download in the session caused the downloads
   * panel to be displayed.
   */
  firstDownloadShown: false
};

////////////////////////////////////////////////////////////////////////////////
//// Module

const NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadsUI]);
