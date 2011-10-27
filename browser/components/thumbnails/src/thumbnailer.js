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

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XUL_FRAME_SRC = "chrome://global/content/mozilla.xhtml";

const THUMB_MIN_WIDTH = 1024;
const THUMB_BG_COLOR = "#fff";

// TODO
let Thumbnailer = {
  create: function TN_create(aUrl, aWidth, aHeight, aCallback) {
    let frameWidth = Math.max(aWidth, THUMB_MIN_WIDTH);
    let frameHeight = frameWidth / (aWidth / aHeight);

    let self = this;

    HiddenFrame.create(aUrl, frameWidth, frameHeight, function (aFrame, aStatusCode) {
      // draw to the canvas as soon as the frame is initialized
      ctx.drawWindow(aFrame.contentWindow, 0, 0, frameWidth, frameHeight,
                     THUMB_BG_COLOR, ctx.DRAWWINDOW_DO_NOT_FLUSH);

      // read the image data from the canvas and pass it to the callback
      self._readCanvasData(canvas, function (aDataStream) {
        aCallback(aDataStream, aStatusCode);

        HiddenCanvas.destroy(canvas);
        HiddenFrame.destroy(aFrame);
      });
    });

    // creating the hidden frame takes a while so let's create the canvas in
    // the meantime while we're waiting for the hidden frame to load
    let canvas = HiddenCanvas.create(aWidth, aHeight);
    let ctx = canvas.getContext("2d");

    // scale canvas accordingly
    let scaler = aWidth / frameWidth;
    ctx.scale(scaler, scaler);
  },

  _readCanvasData: function TN__readCanvasData(aCanvas, aCallback) {
    let dataUri = aCanvas.toDataURL(CONTENT_TYPE, "");
    let uri = Services.io.newURI(dataUri, "UTF8", null);

    gNetUtil.asyncFetch(uri, function (aDataStream, aStatus, aRequest) {
      aCallback(aDataStream);
    });
  }
};

// TODO
let HiddenFrame = {
  _hostFrameCallbacks: [],

  create: function HF_create(aUri, aWidth, aHeight, aCallback) {
    let self = this;

    this._getHostFrame(function (aFrame) {
      let doc = aFrame.contentDocument;
      let iframe = doc.createElementNS(XUL_NS, "iframe");
      iframe.setAttribute("type", "content");
      doc.documentElement.appendChild(iframe);

      iframe.style.width = aWidth + "px";
      iframe.style.height = aHeight + "px";

      // hide the scrollbars
      iframe.style.overflow = "hidden";

      // TODO
      let docShell = iframe.docShell;
      docShell.allowAuth = false;
      docShell.allowWindowControl = false;

      self._whenDocumentLoaded(iframe, function () {
        let statusCode = 200;

        try {
          // if the channel is a nsIHttpChannel get its http status
          let channel = docShell.currentDocumentChannel;
          let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
          statusCode = httpChannel.responseStatus;
        } catch (e) {
          // not a http channel, ignore
        }

        aCallback(iframe, statusCode);
      });

      iframe.setAttribute("src", aUri);
    });
  },

  destroy: function HF_destroy(aFrame) {
    aFrame.parentNode.removeChild(aFrame);
  },

  _getHostFrame: function HF__getHostFrame(aCallback) {
    // return the cached value if any
    if (this._hostFrame) {
      aCallback(this._hostFrame);
      return;
    }

    if (this._hostFrameCallbacks.push(aCallback) > 1)
      return;

    // create the host frame
    let window = gAppShellService.hiddenDOMWindow;
    let doc = window.document;

    let hostFrame = doc.createElement("iframe");
    doc.body.appendChild(hostFrame);

    // TODO
    hostFrame.setAttribute("src", XUL_FRAME_SRC);

    let self = this;

    this._whenContentLoaded(hostFrame, function () {
      self._hostFrame = hostFrame;
      self._hostFrameCallbacks.forEach(function (callback) callback(hostFrame));
      self._hostFrameCallbacks = null;
    });
  },

  _whenContentLoaded: function HF__whenContentLoaded(aElement, aCallback) {
    aElement.addEventListener("DOMContentLoaded", function onLoad() {
      aElement.removeEventListener("DOMContentLoaded", onLoad, false);
      aCallback();
    }, false);
  },

  _whenDocumentLoaded: function HF__whenDocumentLoaded(aIframe, aCallback) {
    aIframe.addEventListener("load", function onLoad(aEvent) {
      if (aIframe.contentDocument == aEvent.target) {
        aIframe.removeEventListener("load", onLoad, true);
        aCallback();
      }
    }, true);
  }
};

// TODO
let HiddenCanvas = {
  create: function HC_create(aWidth, aHeight) {
    let window = gAppShellService.hiddenDOMWindow;
    let doc = window.document;

    let canvas = doc.createElement("canvas");
    doc.body.appendChild(canvas);

    canvas.setAttribute("moz-opaque", "");
    canvas.style.width = aWidth + "px";
    canvas.style.height = aHeight + "px";
    canvas.width = aWidth;
    canvas.height = aHeight;

    return canvas;
  },

  destroy: function HC_destroy(aCanvas) {
    aCanvas.parentNode.removeChild(aCanvas);
  }
};
