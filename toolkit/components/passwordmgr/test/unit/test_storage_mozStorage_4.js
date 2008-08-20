/*
 * Test suite for storage-mozStorage.js -- mailnews specific tests.
 *
 * This test interfaces directly with the mozStorage login storage module,
 * bypassing the normal login manager usage.
 *
 */

const Cm = Components.manager;
const BASE_CONTRACTID = "@mozilla.org/network/protocol;1?name=";
const LDAPPH_CID = Components.ID("{08eebb58-8d1a-4ab5-9fca-e35372697828}");
const MAILBOXPH_CID = Components.ID("{edb1dea3-b226-405a-b93d-2a678a68a198}");

const STORAGE_TYPE = "mozStorage";

function genericProtocolHandler(scheme, defaultPort) {
    this.scheme = scheme;
    this.defaultPort = defaultPort;
}

genericProtocolHandler.prototype = {
    scheme: "",
    defaultPort: 0,

    QueryInterface: function gph_QueryInterface(aIID) {
        if (!aIID.equals(Ci.nsISupports) &&
            !aIID.equals(Ci.nsIProtocolHandler)) {
            throw Cr.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    get protocolFlags() {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    },

    newURI: function gph_newURI(aSpec, anOriginalCharset, aBaseURI) {
        var uri = Components.classes["@mozilla.org/network/standard-url;1"].
                             createInstance(Ci.nsIStandardURL);

        uri.init(Ci.nsIStandardURL.URLTYPE_STANDARD, this.defaultPort, aSpec,
                  anOriginalCharset, aBaseURI);

        return uri;
    },

    newChannel: function gph_newChannel(aUri) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    },

    allowPort: function gph_allowPort(aPort, aScheme) {
        return false;
    }
}

function generateFactory(protocol, defaultPort)
{
    return {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            return (new genericProtocolHandler(protocol, defaultPort)).
                    QueryInterface(iid);
        }
    };
}

function run_test() {

Cm.nsIComponentRegistrar.registerFactory(LDAPPH_CID, "LDAPProtocolFactory",
                                         BASE_CONTRACTID + "ldap",
                                         generateFactory("ldap", 389));
Cm.nsIComponentRegistrar.registerFactory(MAILBOXPH_CID,
                                         "MailboxProtocolFactory",
                                         BASE_CONTRACTID + "mailbox",
                                         generateFactory("mailbox", 0));

try {
var storage, testnum = 0;

// Create a couple of dummy users to match what we expect to be translated
// from the input file.
var dummyuser1 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser2 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);
var dummyuser3 = Cc["@mozilla.org/login-manager/loginInfo;1"].
                 createInstance(Ci.nsILoginInfo);


dummyuser1.init("mailbox://localhost", null, "mailbox://localhost",
    "bugzilla", "testpass1", "", "");

dummyuser2.init("ldap://localhost1", null,
    "ldap://localhost1/dc=test",
    "", "testpass2", "", "");

dummyuser3.init("http://dummyhost.mozilla.org", "", null,
    "testuser1", "testpass1", "put_user_here", "put_pw_here");

LoginTest.deleteFile(OUTDIR, "signons.sqlite");

/*
 * ---------------------- Bug 403790 ----------------------
 * Migrating mailnews style username/passwords
 */

/* ========== 1 ========== */
testnum++;

testdesc = "checking reading of mailnews-like old logins";
storage = LoginTest.initStorage(INDIR, "signons-403790.txt",
                      OUTDIR, "output-403790.sqlite");
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2]);

storage.addLogin(dummyuser3); // trigger a write
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2, dummyuser3]);

testdesc = "[flush and reload for verification]";
storage = LoginTest.reloadStorage(OUTDIR, "output-403790.sqlite");
LoginTest.checkStorageData(storage, [], [dummyuser1, dummyuser2, dummyuser3]);

LoginTest.deleteFile(OUTDIR, "output-403790.sqlite");


/* ========== end ========== */
} catch (e) {
    throw ("FAILED in test #" + testnum + " -- " + testdesc + ": " + e);
}

};
