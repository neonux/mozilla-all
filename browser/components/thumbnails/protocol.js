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
 *   Dietrich Ayala <dietrich@mozilla.com> (Original Author)
 *   Tim Taubert <ttaubert@mozilla.com>
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

// a white 1x1 image that is delivered as a thumbnail for about:blank
const ABOUT_BLANK_URI = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAA" +
                        "BCAYAAAAfFcSJAAAADUlEQVQImWP4////fwAJ+wP9CNHoHgAAAABJ" +
                        "RU5ErkJggg==";

// specifies the flags specific to our protocol implementation
const PROTOCOL_FLAGS = Ci.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD |
                       Ci.nsIProtocolHandler.URI_NORELATIVE |
                       Ci.nsIProtocolHandler.URI_NOAUTH;

// ##########
// Class: Protocol
// Implements the thumbnail protocol handler responsible for moz-thumb:// URIs.
function Protocol() {
}

Protocol.prototype = {
  get scheme() SCHEME,
  get defaultPort() -1,
  get protocolFlags() PROTOCOL_FLAGS,

  // ----------
  // Function: newURI
  // Creates a new URI object that is suitable for loading by this protocol.
  //
  // Parameters:
  //   aSpec - URI string in UTF8 encoding
  //   aOriginCharset - charset of the document from which the URI originated
  newURI: function Proto_newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  // ----------
  // Function: newChannel
  // Constructs a new channel from the given URI for this protocol handler.
  //
  // Parameters:
  //   aUri - the URI for which to construct a channel
  newChannel: function Proto_newChannel(aUri) {
    let {url} = parseUri(aUri);

    // don't try to load thumbnails for about:blank pages, just deliver a
    // white 1x1 image. so we make sure that tests can use about:blank
    // and don't have to wait until all the images have been rendered.
    if (0 == url.indexOf("about:blank"))
      return Services.io.newChannel(ABOUT_BLANK_URI, "UTF8", null);

    // prevent recursion if a thumbnail for a thumbnail is requested
    if (0 == url.indexOf(SCHEME + "://"))
      aUri = Services.io.newURI(url, "UTF8", null);

    // return a redirect channel that will asynchronously decide which channel
    // to redirect to.
    return new Channel(aUri);
  },

  // ----------
  // Function: allowPort
  // Returns always false because we'll never want to allow blacklisted ports.
  //
  // Parameters:
  //   aPort - the blacklisted port
  //   aScheme - the scheme the port is blacklisted for
  allowPort: function Proto_allowPort(aPort, aScheme) {
    return false;
  },

  classID: Components.ID("{5a4ae9b5-f475-48ae-9dce-0b4c1d347884}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler])
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([Protocol]);
