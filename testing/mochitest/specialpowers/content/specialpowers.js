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
 * The Original Code is Special Powers code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Clint Talbert cmtalbert@gmail.com
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
 * ***** END LICENSE BLOCK *****/
/* This code is loaded in every child process that is started by mochitest in
 * order to be used as a replacement for UniversalXPConnect
 */
function SpecialPowers() {}

var SpecialPowers = {
  sanityCheck: function() { return "foo"; },
  
  // Mimic the get*Pref API
  getBoolPref: function(aPrefName) {
    return (this._getPref(aPrefName, 'BOOL'));
  },
  getIntPref: function(aPrefName) {
    return (this._getPref(aPrefName, 'INT'));
  },
  getCharPref: function(aPrefName) {
    return (this._getPref(aPrefName, 'CHAR'));
  },
  getComplexValue: function(aPrefName, aIid) {
    return (this._getPref(aPrefName, 'COMPLEX', aIid));
  },

  // Mimic the set*Pref API
  setBoolPref: function(aPrefName, aValue) {
    return (this._setPref(aPrefName, 'BOOL', aValue));
  },
  setIntPref: function(aPrefName, aValue) {
    return (this._setPref(aPrefName, 'INT', aValue));
  },
  setCharPref: function(aPrefName, aValue) {
    return (this._setPref(aPrefName, 'CHAR', aValue));
  },
  setComplexValue: function(aPrefName, aIid, aValue) {
    return (this._setPref(aPrefName, 'COMPLEX', aValue, aIid));
  },

  // Private pref functions to communicate to chrome
  _getPref: function(aPrefName, aPrefType, aIid) {
    var msg = {};
    if (aIid) {
      // Overloading prefValue to handle complex prefs
      msg = {'op':'get', 'prefName': aPrefName, 'prefType':aPrefType, 'prefValue':[aIid]};
    } else {
      msg = {'op':'get', 'prefName': aPrefName,'prefType': aPrefType};
    }
    return(sendSyncMessage('SPPrefService', msg)[0]);
  },
  _setPref: function(aPrefName, aPrefType, aValue, aIid) {
    var msg = {};
    if (aIid) {
      msg = {'op':'set','prefName':aPrefName, 'prefType': aPrefType, 'prefValue': [aIid,aValue]};
    } else {
      msg = {'op':'set', 'prefName': aPrefName, 'prefType': aPrefType, 'prefValue': aValue};
    }
    return(sendSyncMessage('SPPrefService', msg)[0]);
  },

  _getTopChromeWindow: function(window) {
    var Ci = Components.interfaces;
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIDocShellTreeItem)
                 .rootTreeItem
                 .QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindow)
                 .QueryInterface(Ci.nsIDOMChromeWindow);
  },
  _getAutoCompletePopup: function(window) {
    return this._getTopChromeWindow(window).document
                                           .getElementById("PopupAutoComplete");
  },
  addAutoCompletePopupEventListener: function(window, listener) {
    this._getAutoCompletePopup(window).addEventListener("popupshowing",
                                                        listener,
                                                        false);
  },
  removeAutoCompletePopupEventListener: function(window, listener) {
    this._getAutoCompletePopup(window).removeEventListener("popupshowing",
                                                           listener,
                                                           false);
  },
  isBackButtonEnabled: function(window) {
    return !this._getTopChromeWindow(window).document
                                      .getElementById("Browser:Back")
                                      .hasAttribute("disabled")
  },
}

SpecialPowers.__exposedProps__ = {};
for each (i in Object.keys(SpecialPowers).filter(function(v) {return v.charAt(0) != "_"})) {
  SpecialPowers.__exposedProps__[i] = "r";
}


// Attach our API to the window
function attachSpecialPwrToWindow(aSubject) {
  try {
    if ((aSubject !== null) && 
        (aSubject !== undefined) &&
        (aSubject.wrappedJSObject) &&
        !(aSubject.wrappedJSObject.SpecialPowers)) {
      aSubject.wrappedJSObject.SpecialPowers = SpecialPowers;
    }
  } catch(ex) {
    dump("TEST-INFO | specialpowers.js |  Failed to attach specialpowers to window exception: " + ex + "\n");
  }
}

// In true IPC, this loads in the child process so we need our own observer here
// to ensure we actually get attached, otherwise we'll miss content-document-global-created
// notifications
// NOTE: The observers are GC'd when the window dies, so while this looks like it should
// leak, it actually doesn't. And if you add in an observer for dom-window-destroyed or 
// xpcom-shutdown and upon capturing either of those notifications you unregister the 
// content-document-global-created observer, then in the child process you will never
// be able to capture another content-document-global-created on the next page load.  Essentially,
// this is registered inside the child process once by our SpecialPowersObserver, and after that,
// we will no longer trip that chrome code again.
function frameScriptObserver() {
  // Then we need to register the observers
  this.register();
}

frameScriptObserver.prototype = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "content-document-global-created") {
      attachSpecialPwrToWindow(aSubject);
    } 
  },
  register: function() {
    var obsSvc = Components.classes["@mozilla.org/observer-service;1"]
                 .getService(Components.interfaces.nsIObserverService);
    obsSvc.addObserver(this, "content-document-global-created", false);
  },
  unregister: function() {
    var obsSvc = Components.classes["@mozilla.org/observer-service;1"]
                 .getService(Components.interfaces.nsIObserverService);
    obsSvc.removeObserver(this, "content-document-global-created");
  }
};

// VERY IMPORTANT: Only add observers if they are needed
if (content && !content.wrappedJSObject.SpecialPowers)
  var frameScriptObsv = new frameScriptObserver();
