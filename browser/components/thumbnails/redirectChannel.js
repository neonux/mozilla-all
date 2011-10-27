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

// TODO
function RedirectChannel(aUri) {
  this._setup(aUri);
}

RedirectChannel.prototype = extend(BaseChannel, {
  asyncOpen: function RCH_asyncOpen(aListener, aContext) {
    if (this._wasOpened)
      throw Cr.NS_ERROR_ALREADY_OPENED;

    if (this.canceled)
      return;

    this._listener = aListener;
    this._context = aContext;

    this._isPending = true;
    this._wasOpened = true;

    this._fetchEntry();
  },

  _fetchEntry: function RCH__fetchEntry() {
    let self = this;
    let uri = this._uri;

    Cache.getReadEntry(this.name, function (aEntry) {
      let channel;

      if (aEntry)
        // entry found, deliver from cache
        channel = new CacheChannel(uri, aEntry);
      else
        // entry not found, create a thumbnail and deliver it when ready
        channel = new GenerateChannel(uri);

      self._redirect(channel);
    });
  },

  _redirect: function RCH__redirect(aChannel) {
    let sink = this.notificationCallbacks.QueryInterface(Ci.nsIChannelEventSink);
    let flags = Ci.nsIChannelEventSink.REDIRECT_INTERNAL;
    let callback = this._createRedirectCallback(aChannel);

    sink.asyncOnChannelRedirect(this, aChannel, flags, callback);
  },

  _createRedirectCallback: function RCH__createRedirectCallback(aChannel) {
    let self = this;

    return {
      onRedirectVerifyCallback: function (aStatus) {
        if (!Components.isSuccessCode(aStatus))
          throw Cr.NS_ERROR_UNEXPECTED;

        // cancel the current request before opening the new one
        self.cancel(Cr.NS_OK);

        aChannel.loadGroup = self.loadGroup;
        aChannel.asyncOpen(self._listener, self._context);
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.nsIAsyncVerifyRedirectCallback])
    };
  }
});
