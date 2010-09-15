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
 * The Original Code is Feedback.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mark.finkle@gmail.com>
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

var Feedback = {
  init: function(aEvent) {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
    document.getElementById("feedback-about").setAttribute("desc", appInfo.version);

    // A simple frame script to fill in the referrer page
    messageManager.loadFrameScript("data:,addMessageListener('Feedback:InitPage', function(m) { content.document.getElementById('id_url').value = m.json.referrer; });", true);

    // Try to delay the widget initialization during startup
    messageManager.addMessageListener("DOMContentLoaded", function() {
      // We only want to delay one time
      messageManager.removeMessageListener("DOMContentLoaded", arguments.callee, true);

      // We unhide the panelUI so the XBL and settings can initialize
      document.getElementById("feedback-container").hidden = false;
    });
  },

  openFeedback: function(aURL) {
    let currentURL = Browser.selectedBrowser.currentURI.spec;
    let newTab = BrowserUI.newTab(aURL);

    // Tell the feedback page to fill in the referrer URL
    newTab.browser.messageManager.addMessageListener("DOMContentLoaded", function() {
      newTab.browser.messageManager.removeMessageListener("DOMContentLoaded", arguments.callee, true);
      newTab.browser.messageManager.sendAsyncMessage("Feedback:InitPage", { referrer: currentURL });
    });
  },

  openReadme: function() {
    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
    let url = formatter.formatURLPref("app.releaseNotesURL");
    BrowserUI.newTab(url);
  },

  updateRestart: function updateRestart() {
    let msg = document.getElementById("feedback-messages");
    if (msg) {
      let strings = Elements.browserBundle;

      let value = "restart-app";
      let notification = msg.getNotificationWithValue(value);
      if (notification)
        return;
  
      let restartCallback = function(aNotification, aDescription) {
        // Notify all windows that an application quit has been requested
        var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
        Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");
  
        // If nothing aborted, quit the app
        if (cancelQuit.data == false) {
          let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
          appStartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
        }
      };

      let buttons = [ {
        label: strings.getString("notificationRestart.button"),
        accessKey: "",
        callback: restartCallback
      } ];
  
      let message = strings.getString("notificationRestart.normal");
      msg.appendNotification(message, value, "", msg.PRIORITY_WARNING_LOW, buttons);
    }
  }
};

window.addEventListener("load", Feedback.init, false);
