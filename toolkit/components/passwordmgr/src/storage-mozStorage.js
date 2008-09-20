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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Paul O'Shannessy <poshannessy@mozilla.com> (primary author)
 *  Mrinal Kant <mrinal.kant@gmail.com> (original sqlite related changes)
 *  Justin Dolske <dolske@mozilla.com> (encryption/decryption functions are
 *                                     a lift from Justin's storage-Legacy.js)
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

const DB_VERSION = 1; // The database schema version

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function LoginManagerStorage_mozStorage() { };

LoginManagerStorage_mozStorage.prototype = {

    classDescription  : "LoginManagerStorage_mozStorage",
    contractID : "@mozilla.org/login-manager/storage/mozStorage;1",
    classID : Components.ID("{8c2023b9-175c-477e-9761-44ae7b549756}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManagerStorage]),

    __logService : null, // Console logging service, used for debugging.
    get _logService() {
        if (!this.__logService)
            this.__logService = Cc["@mozilla.org/consoleservice;1"].
                                getService(Ci.nsIConsoleService);
        return this.__logService;
    },

    __decoderRing : null,  // nsSecretDecoderRing service
    get _decoderRing() {
        if (!this.__decoderRing)
            this.__decoderRing = Cc["@mozilla.org/security/sdr;1"].
                                 getService(Ci.nsISecretDecoderRing);
        return this.__decoderRing;
    },

    __utfConverter : null, // UCS2 <--> UTF8 string conversion
    get _utfConverter() {
        if (!this.__utfConverter) {
            this.__utfConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                                  createInstance(Ci.nsIScriptableUnicodeConverter);
            this.__utfConverter.charset = "UTF-8";
        }
        return this.__utfConverter;
    },

    __profileDir: null,  // nsIFile for the user's profile dir
    get _profileDir() {
        if (!this.__profileDir)
            this.__profileDir = Cc["@mozilla.org/file/directory_service;1"].
                                getService(Ci.nsIProperties).
                                get("ProfD", Ci.nsIFile);
        return this.__profileDir;
    },

    __storageService: null, // Storage service for using mozStorage
    get _storageService() {
        if (!this.__storageService)
            this.__storageService = Cc["@mozilla.org/storage/service;1"].
                                    getService(Ci.mozIStorageService);
        return this.__storageService;
    },


    // The current database schema
    _dbSchema: {
        tables: {
            moz_logins:         "id                 INTEGER PRIMARY KEY," +
                                "hostname           TEXT NOT NULL,"       +
                                "httpRealm          TEXT,"                +
                                "formSubmitURL      TEXT,"                +
                                "usernameField      TEXT NOT NULL,"       +
                                "passwordField      TEXT NOT NULL,"       +
                                "encryptedUsername  TEXT NOT NULL,"       +
                                "encryptedPassword  TEXT NOT NULL",

            moz_disabledHosts:  "id                 INTEGER PRIMARY KEY," +
                                "hostname           TEXT UNIQUE ON CONFLICT REPLACE",
        },
        indices: {
          moz_logins_hostname_index: {
            table: "moz_logins",
            columns: ["hostname"]
          },
          moz_logins_hostname_formSubmitURL_index: {
            table: "moz_logins",
            columns: ["hostname", "formSubmitURL"]
          },
          moz_logins_hostname_httpRealm_index: {
              table: "moz_logins",
              columns: ["hostname", "httpRealm"]
            }
        }
    },
    _dbConnection : null,  // The database connection
    _dbStmts      : null,  // Database statements for memoization

    _prefBranch   : null,  // Preferences service
    _signonsFile  : null,  // nsIFile for "signons.sqlite"
    _importFile   : null,  // nsIFile for import from legacy
    _debug        : false, // mirrors signon.debug
    _initialized  : false, // have we initialized properly (for import failure)
    _initializing : false, // prevent concurrent initializations


    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console.
     */
    log : function (message) {
        if (!this._debug)
            return;
        dump("PwMgr mozStorage: " + message + "\n");
        this._logService.logStringMessage("PwMgr mozStorage: " + message);
    },


    /*
     * initWithFile
     *
     * Initialize the component, but override the default filename locations.
     * This is primarily used to the unit tests and profile migration.
     * aImportFile is legacy storage file, aDBFile is a sqlite/mozStorage file.
     */
    initWithFile : function(aImportFile, aDBFile) {
        if (aImportFile)
            this._importFile = aImportFile;
        if (aDBFile)
            this._signonsFile = aDBFile;

        this.init();
    },


    /*
     * init
     *
     * Initialize this storage component; import from legacy files, if
     * necessary. Most of the work is done in _deferredInit.
     */
    init : function () {
        this._dbStmts = [];

        // Connect to the correct preferences branch.
        this._prefBranch = Cc["@mozilla.org/preferences-service;1"].
                           getService(Ci.nsIPrefService);
        this._prefBranch = this._prefBranch.getBranch("signon.");
        this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);

        this._debug = this._prefBranch.getBoolPref("debug");

        // Check to see if the internal PKCS#11 token has been initialized.
        // If not, set a blank password.
        let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"].
                      getService(Ci.nsIPK11TokenDB);

        let token = tokenDB.getInternalKeyToken();
        if (token.needsUserInit) {
            this.log("Initializing key3.db with default blank password.");
            token.initPassword("");
        }

        // Most of the real work is done in _deferredInit, which will get
        // called upon first use of storage
    },


    /*
     * _deferredInit
     *
     * Tries to initialize the module. Adds protection layer so initialization
     * from different places will not conflict.  Also, ensures that we will try
     * to import again if import failed, specifically on cancellation of master
     * password.
     */
    _deferredInit : function () {
        let isFirstRun;
        // Check that we are not already in an initializing state
        if (this._initializing)
            throw "Already initializing";

        // Mark that we are initializing
        this._initializing = true;
        try {
            // If initWithFile is calling us, _signonsFile may already be set.
            if (!this._signonsFile) {
                // Initialize signons.sqlite
                this._signonsFile = this._profileDir.clone();
                this._signonsFile.append("signons.sqlite");
            }
            this.log("Opening database at " + this._signonsFile.path);

            // Initialize the database (create, migrate as necessary)
            isFirstRun = this._dbInit();

            // On first run we want to import the default legacy storage files.
            // Otherwise if passed a file, import from that.
            if (isFirstRun && !this._importFile)
                this._importLegacySignons();
            else if (this._importFile)
                this._importLegacySignons(this._importFile);

            this._initialized = true;
        } catch (e) {
            this.log("Initialization failed");
            // If the import fails on first run, we want to delete the db
            if (isFirstRun && e == "Import failed")
                this._dbCleanup(false);
            throw "Initialization failed";
        } finally {
            this._initializing = false;
        }
    },


    /*
     * _checkInitializationState
     *
     * This snippet is needed in all the public methods. It's essentially only
     * needed when we try to import a legacy file and the user refuses to enter
     * the master password. We don't want to start saving new info if there is
     * old info to import. Throws if attempt to initialize fails.
     */
    _checkInitializationState : function () {
        if (!this._initialized) {
            this.log("Trying to initialize.");
            this._deferredInit();
        }
    },


    /*
     * addLogin
     *
     */
    addLogin : function (login) {
        this._checkInitializationState();
        this._addLogin(login);
    },


    /*
     * _addLogin
     *
     * Private function wrapping core addLogin functionality.
     */
    _addLogin : function (login) {
        // Throws if there are bogus values.
        this._checkLoginValues(login);

        let userCanceled, encUsername, encPassword;
        // Get the encrypted value of the username and password.
        [encUsername, userCanceled] = this._encrypt(login.username);
        if (userCanceled)
            throw "User canceled master password entry, login not added.";

        [encPassword, userCanceled] = this._encrypt(login.password);
        // Probably can't hit this case, but for completeness...
        if (userCanceled)
            throw "User canceled master password entry, login not added.";

        let query =
            "INSERT INTO moz_logins " +
            "(hostname, httpRealm, formSubmitURL, usernameField, " +
             "passwordField, encryptedUsername, encryptedPassword) " +
            "VALUES (:hostname, :httpRealm, :formSubmitURL, :usernameField, " +
                    ":passwordField, :encryptedUsername, :encryptedPassword)";

        let params = {
            hostname:          login.hostname,
            httpRealm:         login.httpRealm,
            formSubmitURL:     login.formSubmitURL,
            usernameField:     login.usernameField,
            passwordField:     login.passwordField,
            encryptedUsername: encUsername,
            encryptedPassword: encPassword
        };

        let stmt;
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.execute();
        } catch (e) {
            this.log("_addLogin failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database, login not added.";
        } finally {
            stmt.reset();
        }
    },


    /*
     * removeLogin
     *
     */
    removeLogin : function (login) {
        this._checkInitializationState();

        let [logins, ids] =
            this._queryLogins(login.hostname, login.formSubmitURL, login.httpRealm);
        let idToDelete;

        // The specified login isn't encrypted, so we need to ensure
        // the logins we're comparing with are decrypted. We decrypt one entry
        // at a time, lest _decryptLogins return fewer entries and screw up
        // indices between the two.
        for (let i = 0; i < logins.length; i++) {
            let [[decryptedLogin], userCanceled] =
                        this._decryptLogins([logins[i]]);

            if (userCanceled)
                throw "User canceled master password entry, login not removed.";

            if (!decryptedLogin || !decryptedLogin.equals(login))
                continue;

            // We've found a match, set id and break
            idToDelete = ids[i];
            break;
        }

        if (!idToDelete)
            throw "No matching logins";

        // Execute the statement & remove from DB
        let query  = "DELETE FROM moz_logins WHERE id = :id";
        let params = { id: idToDelete };
        let stmt;
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.execute();
        } catch (e) {
            this.log("_removeLogin failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database, login not removed.";
        } finally {
            stmt.reset();
        }
    },


    /*
     * modifyLogin
     *
     */
    modifyLogin : function (oldLogin, newLogin) {
        this._checkInitializationState();

        // Throws if there are bogus values.
        this._checkLoginValues(newLogin);

        // Begin a transaction to wrap remove and add
        // This will throw if there is a transaction in progress
        this._dbConnection.beginTransaction();

        // Wrap add/remove in try-catch so we can rollback on error
        try {
            this.removeLogin(oldLogin);
            this.addLogin(newLogin);
        } catch (e) {
            this._dbConnection.rollbackTransaction();
            throw e;
        }

        // Commit the transaction
        this._dbConnection.commitTransaction();
    },


    /*
     * getAllLogins
     *
     * Returns an array of nsAccountInfo.
     */
    getAllLogins : function (count) {
        this._checkInitializationState();

        let userCanceled;
        let [logins, ids] = this._queryLogins("", "", "");

        // decrypt entries for caller.
        [logins, userCanceled] = this._decryptLogins(logins);

        if (userCanceled)
            throw "User canceled Master Password entry";

        this.log("_getAllLogins: returning " + logins.length + " logins.");
        count.value = logins.length; // needed for XPCOM
        return logins;
    },


    /*
     * removeAllLogins
     *
     * Removes all logins from storage.
     */
    removeAllLogins : function () {
        this._checkInitializationState();

        this.log("Removing all logins");
        // Delete any old, unused files.
        this._removeOldSignonsFiles();

        // Disabled hosts kept, as one presumably doesn't want to erase those.
        let query = "DELETE FROM moz_logins";
        let stmt;
        try {
            stmt = this._dbCreateStatement(query);
            stmt.execute();
        } catch (e) {
            this.log("_removeAllLogins failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database";
        } finally {
            stmt.reset();
        }
    },


    /*
     * getAllDisabledHosts
     *
     */
    getAllDisabledHosts : function (count) {
        this._checkInitializationState();

        let disabledHosts = this._queryDisabledHosts(null);

        this.log("_getAllDisabledHosts: returning " + disabledHosts.length + " disabled hosts.");
        count.value = disabledHosts.length; // needed for XPCOM
        return disabledHosts;
    },


    /*
     * getLoginSavingEnabled
     *
     */
    getLoginSavingEnabled : function (hostname) {
        this._checkInitializationState();

        this.log("Getting login saving is enabled for " + hostname);
        return this._queryDisabledHosts(hostname).length == 0
    },


    /*
     * setLoginSavingEnabled
     *
     */
    setLoginSavingEnabled : function (hostname, enabled) {
        this._checkInitializationState();
        this._setLoginSavingEnabled(hostname, enabled);
    },


    /*
     * _setLoginSavingEnabled
     *
     * Private function wrapping core setLoginSavingEnabled functionality.
     */
    _setLoginSavingEnabled : function (hostname, enabled) {
        // Throws if there are bogus values.
        this._checkHostnameValue(hostname);

        this.log("Setting login saving enabled for " + hostname + " to " + enabled);
        let query;
        if (enabled)
            query = "DELETE FROM moz_disabledHosts " +
                    "WHERE hostname = :hostname";
        else
            query = "INSERT INTO moz_disabledHosts " +
                    "(hostname) VALUES (:hostname)";
        let params = { hostname: hostname };

        let stmt
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.execute();
        } catch (e) {
            this.log("_setLoginSavingEnabled failed: " + e.name + " : " + e.message);
            throw "Couldn't write to database"
        } finally {
            stmt.reset();
        }
    },


    /*
     * findLogins
     *
     */
    findLogins : function (count, hostname, formSubmitURL, httpRealm) {
        this._checkInitializationState();

        let userCanceled;
        let [logins, ids] =
            this._queryLogins(hostname, formSubmitURL, httpRealm);

        // Decrypt entries found for the caller.
        [logins, userCanceled] = this._decryptLogins(logins);

        // We want to throw in this case, so that the Login Manager
        // knows to stop processing forms on the page so the user isn't
        // prompted multiple times.
        if (userCanceled)
            throw "User canceled Master Password entry";

        this.log("_findLogins: returning " + logins.length + " logins");
        count.value = logins.length; // needed for XPCOM
        return logins;
    },


    /*
     * countLogins
     *
     */
    countLogins : function (hostname, formSubmitURL, httpRealm) {
        this._checkInitializationState();

        // Do checks for null and empty strings, adjust conditions and params
        let [conditions, params] =
            this._buildConditionsAndParams(hostname, formSubmitURL, httpRealm);

        let query = "SELECT COUNT(1) AS numLogins FROM moz_logins";
        if (conditions.length) {
            conditions = conditions.map(function(c) "(" + c + ")");
            query += " WHERE " + conditions.join(" AND ");
        }

        let stmt, numLogins;
        try {
            stmt = this._dbCreateStatement(query, params);
            stmt.step();
            numLogins = stmt.row.numLogins;
        } catch (e) {
            this.log("_countLogins failed: " + e.name + " : " + e.message);
        } finally {
            stmt.reset();
        }

        this.log("_countLogins: counted logins: " + numLogins);
        return numLogins;
    },


    /*
     * _queryLogins
     *
     * Returns [logins, ids] for logins that match the arguments, where logins
     * is an array of encrypted nsLoginInfo and ids is an array of associated
     * ids in the database.
     */
    _queryLogins : function (hostname, formSubmitURL, httpRealm) {
        let logins = [], ids = [];

        let query = "SELECT * FROM moz_logins";
        let [conditions, params] =
            this._buildConditionsAndParams(hostname, formSubmitURL, httpRealm);

        if (conditions.length) {
            conditions = conditions.map(function(c) "(" + c + ")");
            query += " WHERE " + conditions.join(" AND ");
        }

        let stmt;
        try {
            stmt = this._dbCreateStatement(query, params);
            // We can't execute as usual here, since we're iterating over rows
            while (stmt.step()) {
                // Create the new nsLoginInfo object, push to array
                let login = Cc["@mozilla.org/login-manager/loginInfo;1"].
                            createInstance(Ci.nsILoginInfo);
                login.init(stmt.row.hostname, stmt.row.formSubmitURL,
                           stmt.row.httpRealm, stmt.row.encryptedUsername,
                           stmt.row.encryptedPassword, stmt.row.usernameField,
                           stmt.row.passwordField);
                logins.push(login);
                ids.push(stmt.row.id);
            }
        } catch (e) {
            this.log("_queryLogins failed: " + e.name + " : " + e.message);
        } finally {
            stmt.reset();
        }

        return [logins, ids];
    },


    /*
     * _queryDisabledHosts
     *
     * Returns an array of hostnames from the database according to the
     * criteria given in the argument. If the argument hostname is null, the
     * result array contains all hostnames
     */
    _queryDisabledHosts : function (hostname) {
        let disabledHosts = [];

        let query = "SELECT hostname FROM moz_disabledHosts";
        let params = {};
        if (hostname) {
            query += " WHERE hostname = :hostname";
            params = { hostname: hostname };
        }

        let stmt;
        try {
            stmt = this._dbCreateStatement(query, params);
            while (stmt.step())
                disabledHosts.push(stmt.row.hostname);
        } catch (e) {
            this.log("_queryDisabledHosts failed: " + e.name + " : " + e.message);
        } finally {
            stmt.reset();
        }

        return disabledHosts;
    },


    /*
     * _buildConditionsAndParams
     *
     * Adjusts the WHERE conditions and parameters for statements prior to the
     * statement being created. This fixes the cases where nulls are involved
     * and the empty string is supposed to be a wildcard match
     */
    _buildConditionsAndParams : function (hostname, formSubmitURL, httpRealm) {
        let conditions = [], params = {};

        if (hostname == null) {
            conditions.push("hostname isnull");
        } else if (hostname != '') {
            conditions.push("hostname = :hostname");
            params["hostname"] = hostname;
        }

        if (formSubmitURL == null) {
            conditions.push("formSubmitURL isnull");
        } else if (formSubmitURL != '') {
            conditions.push("formSubmitURL = :formSubmitURL OR formSubmitURL = ''");
            params["formSubmitURL"] = formSubmitURL;
        }

        if (httpRealm == null) {
            conditions.push("httpRealm isnull");
        } else if (httpRealm != '') {
            conditions.push("httpRealm = :httpRealm");
            params["httpRealm"] = httpRealm;
        }

        return [conditions, params];
    },


    /*
     * _checkLoginValues
     *
     * Due to the way the signons2.txt file is formatted, we need to make
     * sure certain field values or characters do not cause the file to
     * be parse incorrectly. Reject logins that we can't store correctly.
     */
    _checkLoginValues : function (aLogin) {
        function badCharacterPresent(l, c) {
            return ((l.formSubmitURL && l.formSubmitURL.indexOf(c) != -1) ||
                    (l.httpRealm     && l.httpRealm.indexOf(c)     != -1) ||
                                        l.hostname.indexOf(c)      != -1  ||
                                        l.usernameField.indexOf(c) != -1  ||
                                        l.passwordField.indexOf(c) != -1);
        }

        // Nulls are invalid, as they don't round-trip well.
        // Mostly not a formatting problem, although ".\0" can be quirky.
        if (badCharacterPresent(aLogin, "\0"))
            throw "login values can't contain nulls";

        // In theory these nulls should just be rolled up into the encrypted
        // values, but nsISecretDecoderRing doesn't use nsStrings, so the
        // nulls cause truncation. Check for them here just to avoid
        // unexpected round-trip surprises.
        if (aLogin.username.indexOf("\0") != -1 ||
            aLogin.password.indexOf("\0") != -1)
            throw "login values can't contain nulls";

        // Newlines are invalid for any field stored as plaintext.
        if (badCharacterPresent(aLogin, "\r") ||
            badCharacterPresent(aLogin, "\n"))
            throw "login values can't contain newlines";

        // A line with just a "." can have special meaning.
        if (aLogin.usernameField == "." ||
            aLogin.formSubmitURL == ".")
            throw "login values can't be periods";

        // A hostname with "\ \(" won't roundtrip.
        // eg host="foo (", realm="bar" --> "foo ( (bar)"
        // vs host="foo", realm=" (bar" --> "foo ( (bar)"
        if (aLogin.hostname.indexOf(" (") != -1)
            throw "bad parens in hostname";
    },


    /*
     * _checkHostnameValue
     *
     * Legacy storage prohibited newlines and nulls in hostnames, so we'll keep
     * that standard here. Throws on illegal format.
     */
    _checkHostnameValue : function (hostname) {
        // File format prohibits certain values. Also, nulls
        // won't round-trip with getAllDisabledHosts().
        if (hostname == "." ||
            hostname.indexOf("\r") != -1 ||
            hostname.indexOf("\n") != -1 ||
            hostname.indexOf("\0") != -1)
            throw "Invalid hostname";
    },


    /*
     * _importLegacySignons
     *
     * Imports a file that uses Legacy storage. Will use importFile if provided
     * else it will attempt to initialize the Legacy storage normally.
     *
     */
    _importLegacySignons : function (importFile) {
        this.log("Importing " + (importFile ? importFile.path : "legacy storage"));

        let legacy = Cc["@mozilla.org/login-manager/storage/legacy;1"].
                     createInstance(Ci.nsILoginManagerStorage);

        // Import all logins and disabled hosts
        try {
            if (importFile)
                legacy.initWithFile(importFile, null);
            else
                legacy.init();

            // Import logins and disabledHosts
            let logins = legacy.getAllLogins({});
            for each (let login in logins)
                this._addLogin(login);
            let disabledHosts = legacy.getAllDisabledHosts({});
            for each (let hostname in disabledHosts)
                this._setLoginSavingEnabled(hostname, false);
        } catch (e) {
            this.log("_importLegacySignons failed: " + e.name + " : " + e.message);
            throw "Import failed";
        }
    },


    /*
     * _removeOldSignonsFiles
     *
     * Deletes any storage files that we're not using any more.
     */
    _removeOldSignonsFiles : function () {
        // We've used a number of prefs over time due to compatibility issues.
        // We want to delete all files referenced in prefs, which are only for
        // importing and clearing logins from storage-Legacy.js.
        filenamePrefs = ["SignonFileName3", "SignonFileName2", "SignonFileName"];
        for each (let prefname in filenamePrefs) {
            let filename = this._prefBranch.getCharPref(prefname);
            let file = this._profileDir.clone();
            file.append(filename);

            if (file.exists()) {
                this.log("Deleting old " + filename + " (" + prefname + ")");
                try {
                    file.remove(false);
                } catch (e) {
                    this.log("NOTICE: Couldn't delete " + filename + ": " + e);
                }
            }
        }
    },


    /*
     * _decryptLogins
     *
     * Decrypts username and password fields in the provided array of
     * logins.
     *
     * The entries specified by the array will be decrypted, if possible.
     * An array of successfully decrypted logins will be returned. The return
     * value should be given to external callers (since still-encrypted
     * entries are useless), whereas internal callers generally don't want
     * to lose unencrypted entries (eg, because the user clicked Cancel
     * instead of entering their master password)
     */
    _decryptLogins : function (logins) {
        let result = [], userCanceled = false;

        for each (let login in logins) {
            let decryptedUsername, decryptedPassword;

            [decryptedUsername, userCanceled] = this._decrypt(login.username);

            if (userCanceled)
                break;

            [decryptedPassword, userCanceled] = this._decrypt(login.password);

            // Probably can't hit this case, but for completeness...
            if (userCanceled)
                break;

            // If decryption failed (corrupt entry?) skip it.
            // Note that we allow password-only logins, so username con be "".
            if (decryptedUsername == null || !decryptedPassword)
                continue;

            login.username = decryptedUsername;
            login.password = decryptedPassword;

            result.push(login);
        }

        return [result, userCanceled];
    },


    /*
     * _encrypt
     *
     * Encrypts the specified string, using the SecretDecoderRing.
     *
     * Returns [cipherText, userCanceled] where:
     *  cipherText   -- the encrypted string, or null if it failed.
     *  userCanceled -- if the encryption failed, this is true if the
     *                  user selected Cancel when prompted to enter their
     *                  Master Password. The caller should bail out, and not
     *                  not request that more things be encrypted (which
     *                  results in prompting the user for a Master Password
     *                  over and over.)
     */
    _encrypt : function (plainText) {
        let cipherText = null, userCanceled = false;

        try {
            let plainOctet = this._utfConverter.ConvertFromUnicode(plainText);
            plainOctet += this._utfConverter.Finish();
            cipherText = this._decoderRing.encryptString(plainOctet);
        } catch (e) {
            this.log("Failed to encrypt string. (" + e.name + ")");
            // If the user clicks Cancel, we get NS_ERROR_FAILURE.
            // (unlike decrypting, which gets NS_ERROR_NOT_AVAILABLE).
            if (e.result == Components.results.NS_ERROR_FAILURE)
                userCanceled = true;
        }

        return [cipherText, userCanceled];
    },


    /*
     * _decrypt
     *
     * Decrypts the specified string, using the SecretDecoderRing.
     *
     * Returns [plainText, userCanceled] where:
     *  plainText    -- the decrypted string, or null if it failed.
     *  userCanceled -- if the decryption failed, this is true if the
     *                  user selected Cancel when prompted to enter their
     *                  Master Password. The caller should bail out, and not
     *                  not request that more things be decrypted (which
     *                  results in prompting the user for a Master Password
     *                  over and over.)
     */
    _decrypt : function (cipherText) {
        let plainText = null, userCanceled = false;

        try {
            let plainOctet = this._decoderRing.decryptString(cipherText);
            plainText = this._utfConverter.ConvertToUnicode(plainOctet);
        } catch (e) {
            this.log("Failed to decrypt string: " + cipherText +
                " (" + e.name + ")");

            // If the user clicks Cancel, we get NS_ERROR_NOT_AVAILABLE.
            // If the cipherText is bad / wrong key, we get NS_ERROR_FAILURE
            // Wrong passwords are handled by the decoderRing reprompting;
            // we get no notification.
            if (e.result == Components.results.NS_ERROR_NOT_AVAILABLE)
                userCanceled = true;
        }

        return [plainText, userCanceled];
    },


    //**************************************************************************//
    // Database Creation & Access
    // Hijacked from /toolkit/components/contentprefs/src/nsContentPrefService.js
    // and modified to fit here. Look there for migration samples.

    /*
     * _dbCreateStatement
     *
     * Creates a statement, wraps it, and then does parameter replacement
     * Returns the wrapped statement for execution.  Will use memoization
     * so that statements can be reused.
     */
    _dbCreateStatement : function (query, params) {
        // Memoize the statements
        if (!this._dbStmts[query]) {
            this.log("Creating new statement for query: " + query);
            let stmt = this._dbConnection.createStatement(query);

            let wrappedStmt = Cc["@mozilla.org/storage/statement-wrapper;1"].
                              createInstance(Ci.mozIStorageStatementWrapper);
            wrappedStmt.initialize(stmt);
            this._dbStmts[query] = wrappedStmt;
        }
        // Replace parameters, must be done 1 at a time
        if (params)
            for (let i in params)
                this._dbStmts[query].params[i] = params[i];
        return this._dbStmts[query];
    },


    /*
     * _dbInit
     *
     * Attempts to initialize the database. This creates the file if it doesn't
     * exist, performs any migrations, etc. When database is first created, we
     * attempt to import legacy signons. Return if this is the first run.
     */
    _dbInit : function () {
        this.log("Initializing Database");
        let isFirstRun = false;
        try {
            this._dbConnection = this._storageService.openDatabase(this._signonsFile);
            // schemaVersion will be 0 if the database has not been created yet
            if (this._dbConnection.schemaVersion == 0) {
                this._dbCreate();
                isFirstRun = true;
            } else {
                // Get the version of the schema in the file.
                let version = this._dbConnection.schemaVersion;

                // Try to migrate the schema in the database to the current schema used by
                // the service.
                if (version != DB_VERSION) {
                    try {
                        this._dbMigrate(version, DB_VERSION);
                    }
                    catch (e) {
                        this.log("Migration Failed");
                        throw(e);
                    }
                }
            }
        } catch (e) {
            // Database is corrupted, so we backup the database, then throw
            // causing initialization to fail and a new db to be created next use
            if (e.result == Components.results.NS_ERROR_FILE_CORRUPTED)
                this._dbCleanup(true);
            throw e;
            // TODO handle migration failures
        }
        return isFirstRun;
    },


    _dbCreate: function () {
        this.log("Creating Database");
        this._dbCreateSchema();
        this._dbConnection.schemaVersion = DB_VERSION;
    },


    _dbCreateSchema : function () {
        this._dbCreateTables();
        this._dbCreateIndices();
    },


    _dbCreateTables : function () {
        this.log("Creating Tables");
        for (let name in this._dbSchema.tables)
            this._dbConnection.createTable(name, this._dbSchema.tables[name]);
    },


    _dbCreateIndices : function () {
        this.log("Creating Indices");
        for (let name in this._dbSchema.indices) {
            let index = this._dbSchema.indices[name];
            let statement = "CREATE INDEX IF NOT EXISTS " + name + " ON " + index.table +
                            "(" + index.columns.join(", ") + ")";
            this._dbConnection.executeSimpleSQL(statement);
        }
    },


    _dbMigrate : function (oldVersion, newVersion) {
        this.log("Attempting to migrate from v" + oldVersion + "to v" + newVersion);
        if (this["_dbMigrate" + oldVersion + "To" + newVersion]) {
            this._dbConnection.beginTransaction();
            try {
                this["_dbMigrate" + oldVersion + "To" + newVersion]();
                this._dbConnection.schemaVersion = newVersion;
                this._dbConnection.commitTransaction();
            }
            catch (e) {
                this._dbConnection.rollbackTransaction();
                throw e;
            }
        }
        else {
            throw("no migrator function from version " + oldVersion +
                  " to version " + newVersion);
        }
    },


    /*
     * _dbCleanup
     *
     * Called when database creation fails. Finalizes database statements,
     * closes the database connection, deletes the database file.
     */
    _dbCleanup : function (backup) {
        this.log("Cleaning up DB file - close & remove & backup=" + backup)

        // Create backup file
        if (backup) {
            let backupFile = this._signonsFile.leafName + ".corrupt";
            this._storageService.backupDatabaseFile(this._signonsFile, backupFile);
        }

        // Finalize all statements to free memory, avoid errors later
        for (let i = 0; i < this._dbStmts.length; i++)
            this._dbStmts[i].statement.finalize();
        this._dbStmts = [];

        // Close the connection, ignore 'already closed' error
        try { this._dbConnection.close() } catch(e) {}
        this._signonsFile.remove(false);
    }

}; // end of nsLoginManagerStorage_mozStorage implementation

let component = [LoginManagerStorage_mozStorage];
function NSGetModule(compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
