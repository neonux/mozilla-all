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
#endif

// the XUL namespace used to create elements in XUL documents
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// as long as there's bug 565388, we need some lightweight chrome page
// to use as a host frame where we can create some XUL <iframes>
const XUL_FRAME_SRC = "chrome://global/content/mozilla.xhtml";

// the minimum width of the window we're generating thumbnails from
const THUMB_MIN_WIDTH = 1024;

// the color our thumbnails are using for transparent areas
const THUMB_BG_COLOR = "#fff";

// ##########
// Class: Thumbnailer
// Creates thumbnails for a given URL by loading it in an <iframe> in a hidden
// window, drawing the window contents to a canvas and finally providing the
// data stream to be read from.
let Thumbnailer = {
  // ----------
  // Function: create
  // Creates a thumbnail for a given url.
  //
  // Parameters:
  //   aUrl - the url to create thumbnail for
  //   aWidth - the thumbnail width
  //   aHeight - the thumbnail height
  //   aCallback - the callback to be called when the thumbnail has been created
  create: function TN_create(aUrl, aWidth, aHeight, aCallback) {
    // calculate our iframe size for a nice aspect ratio
    let frameWidth = Math.max(aWidth, THUMB_MIN_WIDTH);
    let frameHeight = frameWidth / (aWidth / aHeight);

    let self = this;

    // create a hidden frame containing the url to create a thumbnail for
    HiddenFrame.create(aUrl, frameWidth, frameHeight, function (aFrame, aStatusCode) {
      let window = aFrame.contentWindow;

      // create a canvas in the hidden frame
      let canvas = self._createCanvas(window, aWidth, aHeight);
      let ctx = canvas.getContext("2d");

      // scale canvas accordingly
      let scaler = aWidth / frameWidth;
      ctx.scale(scaler, scaler);

      // draw to the canvas as soon as the frame is initialized
      ctx.drawWindow(window, 0, 0, frameWidth, frameHeight,
                     THUMB_BG_COLOR, ctx.DRAWWINDOW_DO_NOT_FLUSH);

      // read the image data from the canvas and pass it to the callback
      self._readCanvasData(canvas, function (aDataStream) {
        aCallback(aDataStream, aStatusCode);
        HiddenFrame.destroy(aFrame);
      });
    });
  },

  // ----------
  // Function: _createCanvas
  // Creates a canvas in the given DOM window.
  //
  // Parameters:
  //   aWindow - the DOM window that contains the canvas
  //   aWidth - the canvas width
  //   aHeight - the canvas height
  _createCanvas: function TN__createCanvas(aWindow, aWidth, aHeight) {
    let doc = aWindow.document;
    let canvas = doc.createElement("canvas");
    doc.body.appendChild(canvas);

    canvas.setAttribute("moz-opaque", "");
    canvas.style.width = aWidth + "px";
    canvas.style.height = aHeight + "px";
    canvas.style.visibility = "hidden";
    canvas.width = aWidth;
    canvas.height = aHeight;

    return canvas;
  },

  // ----------
  // Function: _readCanvasData
  // Reads the image data from a given canvas and provides a data stream.
  //
  // Parameters:
  //   aCanvas - the canvas to read the image data from
  //   aCallback - the callback that the image data is passed to
  _readCanvasData: function TN__readCanvasData(aCanvas, aCallback) {
    let dataUri = aCanvas.toDataURL(CONTENT_TYPE, "");
    let uri = Services.io.newURI(dataUri, "UTF8", null);

    gNetUtil.asyncFetch(uri, aCallback);
  }
};

// ##########
// Class: HiddenFrame
// Creates a hidden <iframe> element that can be used to load and process
// web pages in the background.
let HiddenFrame = {
  // prevent a race condition and queue all _getHostFrame() requests if we're
  // creating a new one
  _hostFrameCallbacks: [],

  // ----------
  // Function: create
  // Creates a new hidden frame for a given uri with a given size.
  //
  // Parameters:
  //   aUri - the uri loaded in the hidden frame
  //   aWidth - the hidden frame's width
  //   aHeight - the hidden frame's height
  //   aCallback - the callback that the new hidden frame is passed to
  create: function HF_create(aUri, aWidth, aHeight, aCallback) {
    let self = this;

    this._createNewFrame(function (aFrame) {
      // set the iframe size
      let style = aFrame.style;
      style.width = aWidth + "px";
      style.height = aHeight + "px";

      // hide the scrollbars
      style.overflow = "hidden";

      // start loading the frame
      aFrame.setAttribute("src", aUri);

      // wait until the iframe's document has finished loading
      aFrame.addEventListener("pageshow", function onLoad(aEvent) {
        // we want the top document to be loaded and not some iframe contained
        // in our iframe
        if (aFrame.contentDocument != aEvent.target)
          return;

        aFrame.removeEventListener("pageshow", onLoad, false);

        // delay calling the callback to give the rendering engine time to
        // finish displaying the content in the iframe
        aFrame._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        aFrame._timer.initWithCallback(function () {
          delete aFrame._timer;
          aCallback(aFrame, self._getFrameStatusCode(aFrame));
        }, 0, Ci.nsITimer.TYPE_ONE_SHOT);
      }, false);
    });
  },

  // ----------
  // Function: destroy
  // Destroys the given hidden frame.
  //
  // Parameters:
  //   aFrame - the hidden frame to destroy
  destroy: function HF_destroy(aFrame) {
    aFrame.parentNode.removeChild(aFrame);
  },

  // ----------
  // Function: _createNewFrame
  // Creates a new hidden frame.
  //
  // Parameters:
  //   aCallback - the callback that the new hidden frame is passed to
  _createNewFrame: function HF__createNewFrame(aCallback) {
    this._getHostFrame(function (aFrame) {
      let doc = aFrame.contentDocument;
      let iframe = doc.createElementNS(XUL_NS, "iframe");
      iframe.setAttribute("type", "content");
      doc.documentElement.appendChild(iframe);

      // set docShell properties to prevent the hidden iframe from misbehaving
      let docShell = iframe.docShell;
      docShell.allowAuth = false;
      docShell.allowWindowControl = false;

      aCallback(iframe);
    });
  },

  // ----------
  // Function: _getHostFrame
  // Gets the host frame that contains the hidden frames.
  //
  // Parameters:
  //   aCallback - the callback that the host frame is passed to
  _getHostFrame: function HF__getHostFrame(aCallback) {
    // return the cached value, if any
    if (this._hostFrame) {
      aCallback(this._hostFrame);
      return;
    }

    // if we're not the first item in the queue we don't need to do a thing
    // but wait for the host frame to be loaded
    if (this._hostFrameCallbacks.push(aCallback) > 1)
      return;

    // create the host frame
    let doc = gAppShellService.hiddenDOMWindow.document;
    let hostFrame = doc.createElement("iframe");
    doc.documentElement.appendChild(hostFrame);
    hostFrame.setAttribute("src", XUL_FRAME_SRC);

    let self = this;

    // wait until the host frame is loaded
    hostFrame.addEventListener("DOMContentLoaded", function onLoad() {
      hostFrame.removeEventListener("DOMContentLoaded", onLoad, false);

      // cache the new host frame
      self._hostFrame = hostFrame;

      // pass the newly created host frame to every callback in the queue
      self._hostFrameCallbacks.forEach(function (callback) callback(hostFrame));
      self._hostFrameCallbacks = null;
    }, false);
  },

  // ----------
  // Function: _getFrameStatusCode
  // Returns the http status code for a given iframe.
  //
  // Parameters:
  //   aFrame - the iframe to get the status code for
  _getFrameStatusCode: function HF__getFrameStatusCode(aFrame) {
    let channel = aFrame.docShell.currentDocumentChannel;

    try {
      // if the channel is a nsIHttpChannel get its http status
      let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
      return httpChannel.responseStatus;
    } catch (e) {
      // not a http channel, we just assume a success status code
      return "200";
    }
  }
};
