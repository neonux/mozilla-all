/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

/* static functions */
let DEBUG = 0;
if (DEBUG)
  debug = function (s) { dump("-*- ContactManager: " + s + "\n"); }
else
  debug = function (s) {}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

XPCOMUtils.defineLazyGetter(Services, "rs", function() {
  return Cc["@mozilla.org/dom/dom-request-service;1"].getService(Ci.nsIDOMRequestService);
});

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
});

const nsIClassInfo            = Ci.nsIClassInfo;
const CONTACTPROPERTIES_CID   = Components.ID("{53ed7c20-ceda-11e0-9572-0800200c9a66}");
const nsIDOMContactProperties = Ci.nsIDOMContactProperties;

// ContactProperties is not directly instantiated. It is used as interface.

ContactProperties.prototype = {

  classID : CONTACTPROPERTIES_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACTPROPERTIES_CID,
                                     contractID:"@mozilla.org/contactProperties;1",
                                     classDescription: "ContactProperties",
                                     interfaces: [nsIDOMContactProperties],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactProperties])
}

//ContactAddress

const CONTACTADDRESS_CONTRACTID = "@mozilla.org/contactAddress;1";
const CONTACTADDRESS_CID        = Components.ID("{27a568b0-cee1-11e0-9572-0800200c9a66}");
const nsIDOMContactAddress      = Components.interfaces.nsIDOMContactAddress;

function ContactAddress(aStreetAddress, aLocality, aRegion, aPostalCode, aCountryName) {
  this.streetAddress = aStreetAddress || null;
  this.locality = aLocality || null;
  this.region = aRegion || null;
  this.postalCode = aPostalCode || null;
  this.countryName = aCountryName || null;
};

function ContactProperties(aProp) { debug("ContactProperties Constructor"); }

ContactAddress.prototype = {

  classID : CONTACTADDRESS_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACTADDRESS_CID,
                                     contractID: CONTACTADDRESS_CONTRACTID,
                                     classDescription: "ContactAddress",
                                     interfaces: [nsIDOMContactAddress],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactAddress])
}

//ContactFindOptions

const CONTACTFINDOPTIONS_CONTRACTID = "@mozilla.org/contactFindOptions;1";
const CONTACTFINDOPTIONS_CID        = Components.ID("{e31daea0-0cb6-11e1-be50-0800200c9a66}");
const nsIDOMContactFindOptions      = Components.interfaces.nsIDOMContactFindOptions;

function ContactFindOptions(aFilterValue, aFilterBy, aFilterOp, aFilterLimit) {
  this.filterValue = aFilterValue || '';

  this.filterBy = new Array();
  for (let field in aFilterBy)
    this.filterBy.push(field);

  this.filterOp = aFilterOp || '';
  this.filterLimit = aFilterLimit || 0;
};

ContactFindOptions.prototype = {

  classID : CONTACTFINDOPTIONS_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACTFINDOPTIONS_CID,
                                     contractID: CONTACTFINDOPTIONS_CONTRACTID,
                                     classDescription: "ContactFindOptions",
                                     interfaces: [nsIDOMContactFindOptions],
                                     flags: nsIClassInfo.DOM_OBJECT}),
              
  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactFindOptions])
}

//Contact

const CONTACT_CONTRACTID = "@mozilla.org/contact;1";
const CONTACT_CID        = Components.ID("{da0f7040-388b-11e1-b86c-0800200c9a66}");
const nsIDOMContact      = Components.interfaces.nsIDOMContact;

function Contact() { debug("Contact constr: "); };

Contact.prototype = {
  
  init: function init(aProp) {
    // Accept non-array strings for DOMString[] properties and convert them.
    function _create(aField) {
      if (typeof aField == "string")
        return new Array(aField);
      return aField;
    };

    this.name =            _create(aProp.name) || null;
    this.honorificPrefix = _create(aProp.honorificPrefix) || null;
    this.givenName =       _create(aProp.givenName) || null;
    this.additionalName =  _create(aProp.additionalName) || null;
    this.familyName =      _create(aProp.familyName) || null;
    this.honorificSuffix = _create(aProp.honorificSuffix) || null;
    this.nickname =        _create(aProp.nickname) || null;
    this.email =           _create(aProp.email) || null;
    this.photo =           _create(aProp.photo) || null;
    this.url =             _create(aProp.url) || null;
    this.category =        _create(aProp.category) || null;

    if (aProp.adr) {
      // Make sure adr argument is an array. Instanceof doesn't work.
      aProp.adr = aProp.adr.length == undefined ? [aProp.adr] : aProp.adr;

      this.adr = new Array();
      for (let i = 0; i < aProp.adr.length; i++)
        this.adr.push(new ContactAddress(aProp.adr[i].streetAddress, aProp.adr[i].locality,
                                         aProp.adr[i].region, aProp.adr[i].postalCode,
                                         aProp.adr[i].countryName));
    } else {
      this.adr = null;
    }

    this.tel =             _create(aProp.tel) || null;
    this.org =             _create(aProp.org) || null;
    this.bday =            (aProp.bday == "undefined" || aProp.bday == null) ? null : new Date(aProp.bday);
    this.note =            _create(aProp.note) || null;
    this.impp =            _create(aProp.impp) || null;
    this.anniversary =     (aProp.anniversary == "undefined" || aProp.anniversary == null) ? null : new Date(aProp.anniversary);
    this.sex =             (aProp.sex != "undefined") ? aProp.sex : null;
    this.genderIdentity =  (aProp.genderIdentity != "undefined") ? aProp.genderIdentity : null;
  },

  get published () {
    return this._published;
  },

  set published(aPublished) {
    this._published = aPublished;
  },

  get updated () {
    return this._updated;
  },
 
  set updated(aUpdated) {
    this._updated = aUpdated;
  },

  classID : CONTACT_CID,
  classInfo : XPCOMUtils.generateCI({classID: CONTACT_CID,
                                     contractID: CONTACT_CONTRACTID,
                                     classDescription: "Contact",
                                     interfaces: [nsIDOMContact, nsIDOMContactProperties],
                                     flags: nsIClassInfo.DOM_OBJECT}),

  QueryInterface : XPCOMUtils.generateQI([nsIDOMContact, nsIDOMContactProperties])
}

// ContactManager

const CONTACTMANAGER_CONTRACTID = "@mozilla.org/contactManager;1";
const CONTACTMANAGER_CID        = Components.ID("{50a820b0-ced0-11e0-9572-0800200c9a66}");
const nsIDOMContactManager      = Components.interfaces.nsIDOMContactManager;

function ContactManager()
{
  debug("Constructor");
}

ContactManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  save: function save(aContact) {
    let request;
    if (this.hasPrivileges) {
      debug("save: " + JSON.stringify(aContact) + " :" + aContact.id);
      let newContact = {};
      newContact.properties = {
        name:            [],
        honorificPrefix: [],
        givenName:       [],
        additionalName:  [],
        familyName:      [],
        honorificSuffix: [],
        nickname:        [],
        email:           [],
        photo:           [],
        url:             [],
        category:        [],
        adr:             [],
        tel:             [],
        org:             [],
        bday:            null,
        note:            [],
        impp:            [],
        anniversary:     null,
        sex:             null,
        genderIdentity:  null
      };
      for (let field in newContact.properties)
        newContact.properties[field] = aContact[field];

      if (aContact.id == "undefined") {
        // for example {25c00f01-90e5-c545-b4d4-21E2ddbab9e0} becomes
        // 25c00f0190e5c545b4d421E2ddbab9e0
        aContact.id = this._getRandomId().replace('-', '').replace('{', '').replace('}', '');
      }

      this._setMetaData(newContact, aContact);
      debug("send: " + JSON.stringify(newContact));
      request = this.createRequest();
      cpmm.sendAsyncMessage("Contact:Save", {contact: newContact,
                                             requestID: this.getRequestId(request)});
      return request;
    } else {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  remove: function removeContact(aRecord) {
    let request;
    if (this.hasPrivileges) {
      request = this.createRequest();
      cpmm.sendAsyncMessage("Contact:Remove", {id: aRecord.id,
                                               requestID: this.getRequestId(request)});
      return request;
    } else {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  _setMetaData: function(aNewContact, aRecord) {
    aNewContact.id = aRecord.id;
    aNewContact.published = aRecord.published;
    aNewContact.updated = aRecord.updated;
  },

  _convertContactsArray: function(aContacts) {
    let contacts = new Array();
    for (let i in aContacts) {
      let newContact = new Contact();
      newContact.init(aContacts[i].properties);
      this._setMetaData(newContact, aContacts[i]);
      contacts.push(newContact);
    }
    return contacts;
  },

  receiveMessage: function(aMessage) {
    debug("Contactmanager::receiveMessage: " + aMessage.name);
    let msg = aMessage.json;
    let contacts = msg.contacts;

    switch (aMessage.name) {
      case "Contacts:Find:Return:OK":
        let req = this.getRequest(msg.requestID);
        if (req) {
          let result = this._convertContactsArray(contacts);
          debug("result: " + JSON.stringify(result));
          Services.rs.fireSuccess(req, result);
        } else {
          debug("no request stored!" + msg.requestID);
        }
        break;
      case "Contact:Save:Return:OK":
      case "Contacts:Clear:Return:OK":
      case "Contact:Remove:Return:OK":
        req = this.getRequest(msg.requestID);
        if (req)
          Services.rs.fireSuccess(req, null);
        break;
      case "Contacts:Find:Return:KO":
      case "Contact:Save:Return:KO":
      case "Contact:Remove:Return:KO":
      case "Contacts:Clear:Return:KO":
        req = this.getRequest(msg.requestID);
        if (req)
          Services.rs.fireError(req, msg.errorMsg);
        break;
      default: 
        debug("Wrong message: " + aMessage.name);
    }
    this.removeRequest(msg.requestID);
  },

  find: function(aOptions) {
    let request;
    if (this.hasPrivileges) {
      request = this.createRequest();
      cpmm.sendAsyncMessage("Contacts:Find", {findOptions: aOptions, 
                                              requestID: this.getRequestId(request)});
      return request;
    } else {
      debug("find not allowed");
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  clear: function() {
    let request;
    if (this.hasPrivileges) {
      request = this.createRequest();
      cpmm.sendAsyncMessage("Contacts:Clear", {requestID: this.getRequestId(request)});
      return request;
    } else {
      debug("clear not allowed");
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  init: function(aWindow) {
    // Set navigator.mozContacts to null.
    if (!Services.prefs.getBoolPref("dom.mozContacts.enabled"))
      return null;

    this.initHelper(aWindow, ["Contacts:Find:Return:OK", "Contacts:Find:Return:KO",
                     "Contacts:Clear:Return:OK", "Contacts:Clear:Return:KO",
                     "Contact:Save:Return:OK", "Contact:Save:Return:KO",
                     "Contact:Remove:Return:OK", "Contact:Remove:Return:KO"]);

    Services.obs.addObserver(this, "inner-window-destroyed", false);

    let principal = aWindow.document.nodePrincipal;
    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);

    let perm = principal == secMan.getSystemPrincipal() ? 
                 Ci.nsIPermissionManager.ALLOW_ACTION : 
                 Services.perms.testExactPermission(principal.URI, "webcontacts-manage");
 
    //only pages with perm set can use the contacts
    this.hasPrivileges = perm == Ci.nsIPermissionManager.ALLOW_ACTION;
    debug("has privileges :" + this.hasPrivileges);
  },

  classID : CONTACTMANAGER_CID,
  QueryInterface : XPCOMUtils.generateQI([nsIDOMContactManager, Ci.nsIDOMGlobalPropertyInitializer]),

  classInfo : XPCOMUtils.generateCI({classID: CONTACTMANAGER_CID,
                                     contractID: CONTACTMANAGER_CONTRACTID,
                                     classDescription: "ContactManager",
                                     interfaces: [nsIDOMContactManager],
                                     flags: nsIClassInfo.DOM_OBJECT})
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([Contact, ContactManager, ContactProperties, ContactAddress, ContactFindOptions])
