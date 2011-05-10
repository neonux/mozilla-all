/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
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
 * The Original Code is about:permissions code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Margaret Leibovic <margaret.leibovic@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/PluralForm.jsm");
Components.utils.import("resource://gre/modules/DownloadUtils.jsm");

let Ci = Components.interfaces;
let Cc = Components.classes;

let indexedDBService = Cc["@mozilla.org/dom/indexeddb/manager;1"].
                       getService(Ci.nsIIndexedDatabaseManager);

let bundleService = Cc["@mozilla.org/intl/stringbundle;1"].
                    getService(Ci.nsIStringBundleService);
let aboutPermissionsBundle =
  bundleService.createBundle("chrome://browser/locale/preferences/aboutPermissions.properties");

let TEST_EXACT_PERM_TYPES = ["geo"];

/**
 * Site object represents a single site, uniquely identified by a host.
 *
 * TODO: If sites are going to be uniquely identified by hosts, we need to deal
 * with them having multiple URIs (http://, https://). should we expose multiple URIs
 * to the user? -- it seems most permissions are set per URI, not host.
 */
function Site(host) {
  this.host = host;
  this.listitem = null;

  this.httpURI = Services.io.newURI("http://" + this.host, null, null);
  this.httpsURI = Services.io.newURI("https://" + this.host, null, null);

  this._favicon = "";
}

Site.prototype = {

  get favicon() {
    if (!this._favicon) {
      let fs = Cc["@mozilla.org/browser/favicon-service;1"].
               getService(Ci.nsIFaviconService);
      try {
        // First try to see if a favicon is stored for the http URI.
        this._favicon = fs.getFaviconForPage(this.httpURI).spec;
      } catch (e) {
        // getFaviconImageForPage returns the default favicon if no stored favicon is found.
        this._favicon = fs.getFaviconImageForPage(this.httpsURI).spec;
      }
    }
    return this._favicon;
  },

  /**
   * Gets the permission value stored for a specified permission type.
   * @param aType
   *        The permission type string stored in permission manager.
   *        e.g. "cookie", "geo", "indexedDB", "install", "popup", "image"
   * @param aResultObj
   *        An object that stores the permission value set for aType.
   *
   * @returns A boolean indicating whether or not a permission is set.
   */
  getPermission: function Site_getPermission(aType, aResultObj) {
    let permissionValue;
    if (TEST_EXACT_PERM_TYPES.indexOf(aType) == -1)
      permissionValue = Services.perms.testPermission(this.httpURI, aType);
    else
      permissionValue = Services.perms.testExactPermission(this.httpURI, aType);
    aResultObj.value = permissionValue;

    return permissionValue != Ci.nsIPermissionManager.UNKNOWN_ACTION;
  },

  setPermission: function Site_setPermission(aType, aPerm) {
    Services.perms.add(this.httpURI, aType, aPerm);
  },

  clearPermission: function Site_clearPermission(aType) {
    Services.perms.remove(this.host, aType);
  },

  /**
   * @returns An array of the cookies set for the site.
   */
  get cookies() {
    let cookies = [];
    let enumerator = Services.cookies.getCookiesFromHost(this.host);
    while (enumerator.hasMoreElements()) {
      let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
      // getCookiesFromHost returns cookies for base domain, but we only want
      // the cookies for the exact domain
      if (cookie.rawHost == this.host)
        cookies.push(cookie);
    }
    return cookies;
  },

  clearCookies: function Site_clearCookies(aCookieArray) {
    aCookieArray.forEach(function(aCookie) {
      Services.cookies.remove(aCookie.host, aCookie.name, aCookie.path, false);
    });
  },

  /**
   * @returns An array of the logins stored for the site.
   */
  get logins() {
    let httpLogins = Services.logins.findLogins({}, this.httpURI.prePath, "", null);
    let httpsLogins = Services.logins.findLogins({}, this.httpsURI.prePath, "", null);
    return httpLogins.concat(httpsLogins);
  },

  clearLogins: function Site_clearLogins(aLoginArray) {
    aLoginArray.forEach(function(aLogin) {
      Services.logins.removeLogin(aLogin);
    });
  },

  get loginSavingEnabled() {
    return Services.logins.getLoginSavingEnabled(this.httpURI.prePath) &&
           Services.logins.getLoginSavingEnabled(this.httpsURI.prePath);
  },

  set loginSavingEnabled(isEnabled) {
    Services.logins.setLoginSavingEnabled(this.httpURI.prePath, isEnabled);
    Services.logins.setLoginSavingEnabled(this.httpsURI.prePath, isEnabled);
  },

  /**
   * @param aCallback
   *        A function that implements nsIIndexedDatabaseUsageCallback.
   */
  getIndexedDBStorage: function Site_getIndexedDBStorage(aCallback) {
    indexedDBService.getUsageForURI(this.httpURI, aCallback);
  },

  clearIndexedDBStorage: function Site_clearIndexedDBStorage() {
    indexedDBService.clearDatabasesForURI(this.httpURI);
  },

  forgetSite: function Site_forgetSite() {
    let pb = Cc["@mozilla.org/privatebrowsing;1"].
             getService(Ci.nsIPrivateBrowsingService);
    pb.removeDataFromDomain(this.host);
  }
}

/**
 * PermissionDefaults object keeps track of default permissions for sites based
 * on global preferences.
 *
 * Inspired by pageinfo/permissions.js
 */
let PermissionDefaults = {
  UNKNOWN: Ci.nsIPermissionManager.UNKNOWN_ACTION, // 0
  ALLOW: Ci.nsIPermissionManager.ALLOW_ACTION, // 1
  DENY: Ci.nsIPermissionManager.DENY_ACTION, // 2
  SESSION: Ci.nsICookiePermission.ACCESS_SESSION, // 8

  get password() {
    if (Services.prefs.getBoolPref("signon.rememberSignons"))
      return this.ALLOW;
    return this.DENY;
  },
  set password(aValue) {
    let value = (aValue == this.ALLOW);
    Services.prefs.setBoolPref("signon.rememberSignons", value);
  },
  
  // network.cookie.cookieBehavior: 0-Accept, 1-Don't accept foreign, 2-Don't use
  get cookie() {
    if (Services.prefs.getIntPref("network.cookie.cookieBehavior") == 2)
      return this.DENY;

    if (Services.prefs.getIntPref("network.cookie.lifetimePolicy") == 2)
      return this.SESSION;
    return this.ALLOW;
  },
  set cookie(aValue) {
    let value = (aValue == this.DENY) ? 2 : 0;
    Services.prefs.setIntPref("network.cookie.cookieBehavior", value);

    if (aValue == this.SESSION)
      Services.prefs.setIntPref("network.cookie.lifetimePolicy", 2);
  },

  get geo() {
    if (!Services.prefs.getBoolPref("geo.enabled"))
      return this.DENY;
    // We always ask for permission to share location with a specific site, so
    // there is no global ALLOW.
    return this.UNKNOWN;
  },
  set geo(aValue) {
    let value = (aValue == this.ALLOW);
    Services.prefs.setBoolPref("geo.enabled", value);
  },

  get indexedDB() {
    if (!Services.prefs.getBoolPref("dom.indexedDB.enabled"))
      return this.DENY;
    // We always ask for permission to enable indexedDB storage for a specific
    // site, so there is no global ALLOW.
    return this.UNKNOWN;
  },
  set indexedDB(aValue) {
    let value = (aValue == this.ALLOW);
    Services.prefs.setBoolPref("dom.indexedDB.enabled", value);
  },

  get popup() {
    if (Services.prefs.getBoolPref("dom.disable_open_during_load"))
      return this.DENY;
    return this.ALLOW;
  },
  set popup(aValue) {
    let value = (aValue == this.DENY);
    Services.prefs.setBoolPref("dom.disable_open_during_load", value);
  },

  get install() {
    try {
      if (!Services.prefs.getBoolPref("xpinstall.whitelist.required"))
        return this.ALLOW;
    }
    catch (e) {
    }
    return this.DENY;
  },
  set install(aValue) {
    // TODO: Do we want to expose this? It makes it easy for users to unknowingly
    // expose themselves to vulnerability.
    let value = (aValue == this.DENY);
    Services.prefs.setBoolPref("xpinstall.whitelist.required", value);
  },

  // permissions.default.image: 1-Accept, 2-Deny, 3-Don't accept foreign
  get image() {
    if (Services.prefs.getIntPref("permissions.default.image") == 2)
      return this.DENY;
    return this.ALLOW;
  },
  set image(aValue) {
    let value = (aValue == this.ALLOW) ? 1 : 2;
    Services.prefs.setIntPref("permissions.default.image", value);
  }
}

/**
 * AboutPermissions manages the about:permissions page.
 */
let AboutPermissions = {
  /**
   * Number of sites to return from the places database.
   */  
  PLACES_SITES_LIMIT: 50,

  /**
   * Stores a mapping of host strings to Site objects.
   */
  _sites: {},

  _selectedSite: null,

  // TODO: potential additions: "sts/use", "sts/subd"
  _supportedPermissions: ["password", "cookie", "geo", "indexedDB", "install",
                          "popup", "image"],

  /**
   * Called on page load.
   */
  init: function() {
    this.getSites(function(){
      AboutPermissions.enumerateServices();
      AboutPermissions.buildSitesList();
    });

    // Attach observers in case data changes while the page is open.
    Services.obs.addObserver(this, "perm-changed", false);
    Services.obs.addObserver(this, "passwordmgr-storage-changed", false);
    Services.obs.addObserver(this, "cookie-changed", false);
    Services.obs.addObserver(this, "browser:purge-domain-data", false);
  },

  /**
   * Called on page unload.
   */
  cleanUp: function() {
    Services.obs.removeObserver(this, "perm-changed", false);
    Services.obs.removeObserver(this, "passwordmgr-storage-changed", false);
    Services.obs.removeObserver(this, "cookie-changed", false);
    Services.obs.removeObserver(this, "browser:purge-domain-data", false);
  },

  observe: function (aSubject, aTopic, aData) {
    switch(aTopic) {
      case "perm-changed":
        let permission = aSubject.QueryInterface(Ci.nsIPermission);
        if (this._selectedSite.host && permission.host == this._selectedSite.host &&
            permission.type in PermissionDefaults)
          this.updatePermission(permission.type);
        break;
      case "passwordmgr-storage-changed":
        this.updatePermission("password");
        this.updatePasswordsCount();
        break;
      case "cookie-changed":
        this.updateCookiesCount();
        break;
      case "browser:purge-domain-data":
        this.deleteSite(aData);
        break;
    }
  },

  /**
   * Creates Site objects for the top-frecency sites in the places database and stores
   * them in _sites. The number of sites created is controlled by PLACES_SITES_LIMIT.
   */
  getSites: function(aCallback) {
    let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
             getService(Ci.nsINavHistoryService);
    let db = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;

    let query = "SELECT get_unreversed_host(rev_host) AS host " +
                "FROM moz_places WHERE rev_host <> '.' " +
                "AND visit_count > 0 GROUP BY rev_host " +
                "ORDER BY MAX(frecency) DESC LIMIT :limit";
    let stmt = db.createAsyncStatement(query);
    stmt.params.limit = this.PLACES_SITES_LIMIT;
    stmt.executeAsync({
      handleResult: function(aResults) {
        let row;
        while (row = aResults.getNextRow()) {
          let host = row.getResultByName("host");
          if (!(host in AboutPermissions._sites))
            AboutPermissions._sites[host] = new Site(host);
        }
      },
      handleError: function(aError) {
        // TODO: Add some error handling.
      },
      handleCompletion: function(aReason) {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED)
          aCallback();
      }
    });
    stmt.finalize();
  },

  /**
   * Finds sites that have non-default permissions and creates Site objects for
   * them if they are not already stored in _sites.
   */
  enumerateServices: function() {
    // Login manager
    let logins = Services.logins.getAllLogins();
    logins.forEach(function(aLogin) {
      // aLogin.hostname is a string in origin URL format (e.g. "http://foo.com")
      try {
        let uri = Services.io.newURI(aLogin.hostname, null, null);
        let host = uri.host;
        if (!(host in AboutPermissions._sites))
          AboutPermissions._sites[host] = new Site(host);
      } catch (e) {
        dump("login URI excpetion: " + e + "\n");
      }
    });

    let disabledHosts = Services.logins.getAllDisabledHosts();
    disabledHosts.forEach(function(aHostname) {
      // aHostname is a string in origin URL format (e.g. "http://foo.com")
      let uri = Services.io.newURI(aHostname, null, null);
      let host = uri.host;
      if (!(host in AboutPermissions._sites))
        AboutPermissions._sites[host] = new Site(host);
      
      AboutPermissions._sites[host].disabledHosts.push(aHostname);
    });

    // Permission manager
    let (enumerator = Services.perms.enumerator) {
      while (enumerator.hasMoreElements()) {
        let permission = enumerator.getNext().QueryInterface(Ci.nsIPermission);
        let host = permission.host;
        // Only include sites with exceptions set for supported permission types
        if (this._supportedPermissions.indexOf(permission.type) != -1 &&
            !(host in AboutPermissions._sites))
          AboutPermissions._sites[host] = new Site(host);
      }
    }
  },

  /**
   * Populates sites-list richlistbox from sites stored in _sites.
   */
  buildSitesList: function() {
    for each (let site in this._sites) {
      let item = document.createElement("richlistitem");
      item.setAttribute("class", "site");
      item.setAttribute("value", site.host);
      item.setAttribute("favicon", site.favicon);
      site.listitem = item;

      document.getElementById("sites-list").appendChild(item);      
    }

    Services.obs.notifyObservers(null, "browser-permissions-initialized", null);
  },

  /**
   * Hides sites in richlistbox based on search text in sites-filter textbox.
   */
  filterSitesList: function() {
    let filterValue = document.getElementById("sites-filter").value;
    if (filterValue == "") {
      for each (let site in this._sites)
        site.listitem.collapsed = false;
      return;
    }

    for each (let site in this._sites)
      site.listitem.collapsed = site.host.indexOf(filterValue) == -1;
  },

  /**
   * Erases all evidence of the selected site. The "forget this site" observer
   * will call deleteSite to update the UI.
   */
  forgetSite: function() {
    this._selectedSite.forgetSite();
  },

  /**
   * Deletes sites for a host and all of its sub-domains. Removes these sites
   * from _sites and removes their corresponding elements from the DOM.
   */
  deleteSite: function(aHost) {
    let sitesList = document.getElementById("sites-list");
    for each (let site in this._sites) {
      if (site.host.hasRootDomain(aHost)) {
        if (site == this._selectedSite) {
          // Clear site data from the DOM to maximize privacy.
          document.getElementById("site-label").value = "";
          document.getElementById("permissions-box").hidden = true;
          this._selectedSite = null;
        }
        
        sitesList.removeChild(site.listitem);
        delete this._sites[site.host];
      }
    }    
  },

  /**
   * @param aSite
   *        An optional paramter to restore default permissions to a specific
   *        site, as opposed to the selected site.
   */
  restoreDefaultPermissions: function(aSite) {
    let site = aSite || this._selectedSite;

    this._supportedPermissions.forEach(function(aType) {
      // The login manager does not provide a way to clear a user-set pref for
      // password saving, so we'll just restore the permission to the default value.
      if (aType == "password") {
        site.loginSavingEnabled =
          (PermissionDefaults["password"] == PermissionDefaults.ALLOW);
      } else {
        site.clearPermission(aType);
      }

      AboutPermissions.updatePermission(aType);
    });
  },

  restoreAllDefaultPermissions: function() {
    for each (let site in this._sites) {
      this.restoreDefaultPermissions(site);
    }
  },

  /**
   * Shows interface for managing site-specific permissions.
   */
  onSitesListSelect: function(event) {
    if (event.target.selectedItem.id == "all-sites-item") {
      this.manageDefaultPermissions();
      return;
    }

    let host = event.target.value;
    let site = this._selectedSite = this._sites[host];
    document.getElementById("site-label").value = host;
    document.getElementById("header-deck").selectedPanel =
      document.getElementById("site-header");

    this.updatePermissionsBox();
  },

  /**
   * Shows interface for managing default permissions.
   */
  manageDefaultPermissions: function() {
    this._selectedSite = null;

    document.getElementById("header-deck").selectedPanel =
      document.getElementById("defaults-header");

    this.updatePermissionsBox();
  },

  /**
   * Updates permissions interface based on selected site.
   */
  updatePermissionsBox: function() {
    this._supportedPermissions.forEach(function(aType){
      AboutPermissions.updatePermission(aType);
    });

    this.updatePasswordsCount();
    this.updateCookiesCount();
    this.updateIndexedDBUsage();

    document.getElementById("permissions-box").hidden = false;
  },

  /**
   * Sets menulist for a given permission to the correct state, based on the
   * stored permission.
   */
  updatePermission: function(aType) {
    let permissionMenulist = document.getElementById(aType + "-menulist");
    permissionMenulist.disabled = false;

    let permissionValue;    
    if (!this._selectedSite) {
      // If there is no selected site, we are updating the default permissions interface.
      permissionValue = PermissionDefaults[aType];
    } else if (aType == "password") {
      // Services.logins.getLoginSavingEnabled already looks at the default
      // permission, so we don't need to.
      permissionValue = this._selectedSite.loginSavingEnabled ?
                        PermissionDefaults.ALLOW : PermissionDefaults.DENY;
    } else {
      let result = {};
      permissionValue = this._selectedSite.getPermission(aType, result) ?
                        result.value : PermissionDefaults[aType];
    }

    permissionMenulist.selectedItem = document.getElementById(aType + "#" + permissionValue);
    
    if (permissionValue == PermissionDefaults[aType])
      permissionMenulist.setAttribute("default", true);
    else
      permissionMenulist.removeAttribute("default");
  },

  onPermissionCommand: function(event) {
    let permissionType = event.currentTarget.getAttribute("type");
    let permissionValue = event.target.value;

    if (!this._selectedSite) {
      // If there is no selected site, we are setting the default permission.
      PermissionDefaults[permissionType] = permissionValue;
    } else if (permissionType == "password") {
      let isEnabled = permissionValue == PermissionDefaults.ALLOW;
      this._selectedSite.loginSavingEnabled = isEnabled;
    } else {
      this._selectedSite.setPermission(permissionType, permissionValue);
    }

    let permissionMenulist = document.getElementById(permissionType + "-menulist");
    if (permissionValue == PermissionDefaults[permissionType])
      permissionMenulist.setAttribute("default", true);
    else
      permissionMenulist.removeAttribute("default");
  },

  updatePasswordsCount: function() {
    if (!this._selectedSite) {
      document.getElementById("passwords-count").hidden = true;
      document.getElementById("passwords-manage-all-button").hidden = false;
      return;
    }

    let passwordsCount = this._selectedSite.logins.length;
    let passwordsForm = aboutPermissionsBundle.GetStringFromName("passwordsCount");
    let passwordsLabel = PluralForm.get(passwordsCount, passwordsForm)
                                   .replace("#1", passwordsCount);

    document.getElementById("passwords-label").value = passwordsLabel;
    document.getElementById("passwords-manage-button").disabled = (passwordsCount < 1);
    document.getElementById("passwords-manage-all-button").hidden = true;
    document.getElementById("passwords-count").hidden = false;
  },

  updateCookiesCount: function() {
    if (!this._selectedSite) {
      document.getElementById("cookies-count").hidden = true;
      document.getElementById("cookies-clear-all-button").hidden = false;
      return;
    }

    let cookiesCount = this._selectedSite.cookies.length;
    let cookiesForm = aboutPermissionsBundle.GetStringFromName("cookiesCount");
    let cookiesLabel = PluralForm.get(cookiesCount, cookiesForm)
                                 .replace("#1", cookiesCount);

    document.getElementById("cookies-label").value = cookiesLabel;
    document.getElementById("cookies-clear-button").disabled = (cookiesCount < 1);
    document.getElementById("cookies-clear-all-button").hidden = true;
    document.getElementById("cookies-count").hidden = false;
  },

  updateIndexedDBUsage: function() {
    if (!this._selectedSite) {
      document.getElementById("indexedDB-usage").hidden = true;
      return;
    }

    let callback = {
      onUsageResult: function(aURI, aUsage) {    
        let indexedDBLabel =
          aboutPermissionsBundle.formatStringFromName("indexedDBUsage",
                                                      DownloadUtils.convertByteUnits(aUsage), 2);

        document.getElementById("indexedDB-label").value = indexedDBLabel;
        document.getElementById("indexedDB-clear-button").disabled = (aUsage == 0);
        document.getElementById("indexedDB-usage").hidden = false;
      }
    }

    this._selectedSite.getIndexedDBStorage(callback);
  },

  // not used
  clearPasswords: function() {
    let site = this._selectedSite;
    site.clearLogin(site.logins);
    this.updatePasswordsCount();
  },
  
  managePasswords: function() {
    let win = window.openDialog("chrome://passwordmgr/content/passwordManager.xul",
                                "Toolkit:PasswordManager", "");

    /* Filtering won't work until password manager is updated to handle it
    if (this._selectedSite) {
      let filterValue = this._selectedSite.host;
      win.addEventListener("load", function() {
        let filter = win.document.getElementById("filter");
        filter.value = filterValue;
        filter.doCommand();
      }, false);
    }*/
  },

  clearCookies: function() {
    let site = this._selectedSite;
    site.clearCookies(site.cookies);
    this.updateCookiesCount();
  },

  clearIndexedDBStorage: function() {
    this._selectedSite.clearIndexedDBStorage();
    this.updateIndexedDBUsage();
  }
}

// See nsPrivateBrowsingService.js
String.prototype.hasRootDomain = function hasRootDomain(aDomain) {
  let index = this.indexOf(aDomain);
  if (index == -1)
    return false;

  if (this == aDomain)
    return true;

  let prevChar = this[index - 1];
  return (index == (this.length - aDomain.length)) &&
         (prevChar == "." || prevChar == "/");
}
