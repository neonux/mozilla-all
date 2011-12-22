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
 * The Original Code is Mozilla XUL Toolkit Testing Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init();

/**
 * Test for bug 471962 <https://bugzilla.mozilla.org/show_bug.cgi?id=471962>:
 * When saving an inner frame as file only, the POST data of the outer page is
 * sent to the address of the inner page.
 *
 * Test for bug 485196 <https://bugzilla.mozilla.org/show_bug.cgi?id=485196>:
 * Web page generated by POST is retried as GET when Save Frame As used, and the
 * page is no longer in the cache.
 */
function test() {
  waitForExplicitFinish();

  gBrowser.loadURI("http://mochi.test:8888/browser/toolkit/content/tests/browser/data/post_form_outer.sjs");

  registerCleanupFunction(function () {
    gBrowser.addTab();
    gBrowser.removeCurrentTab();
  });

  gBrowser.addEventListener("pageshow", function pageShown(event) {
    if (event.target.location == "about:blank")
      return;
    gBrowser.removeEventListener("pageshow", pageShown);

    // Submit the form in the outer page, then wait for both the outer
    // document and the inner frame to be loaded again.
    gBrowser.addEventListener("DOMContentLoaded", handleOuterSubmit);
    gBrowser.contentDocument.getElementById("postForm").submit();
  });

  var framesLoaded = 0;
  var innerFrame;

  function handleOuterSubmit() {
    if (++framesLoaded < 2)
      return;

    gBrowser.removeEventListener("DOMContentLoaded", handleOuterSubmit);

    innerFrame = gBrowser.contentDocument.getElementById("innerFrame");

    // Submit the form in the inner page.
    gBrowser.addEventListener("DOMContentLoaded", handleInnerSubmit);
    innerFrame.contentDocument.getElementById("postForm").submit();
  }

  function handleInnerSubmit() {
    gBrowser.removeEventListener("DOMContentLoaded", handleInnerSubmit);

    // Create the folder the page will be saved into.
    var destDir = createTemporarySaveDirectory();
    var file = destDir.clone();
    file.append("no_default_file_name");
    MockFilePicker.returnFiles = [file];
    MockFilePicker.showCallback = function(fp) {
      MockFilePicker.filterIndex = 1; // kSaveAsType_URL
    };

    mockTransferCallback = onTransferComplete;
    mockTransferRegisterer.register();

    registerCleanupFunction(function () {
      mockTransferRegisterer.unregister();
      MockFilePicker.cleanup();
      destDir.remove(true);
    });

    var docToSave = innerFrame.contentDocument;
    // We call internalSave instead of saveDocument to bypass the history
    // cache.
    internalSave(docToSave.location.href, docToSave, null, null,
                 docToSave.contentType, false, null, null,
                 docToSave.referrer ? makeURI(docToSave.referrer) : null,
                 false, null);
  }

  function onTransferComplete(downloadSuccess) {
    ok(downloadSuccess, "The inner frame should have been downloaded successfully");

    // Read the entire saved file.
    var file = MockFilePicker.returnFiles[0];
    var fileContents = readShortFile(file);

    // Check if outer POST data is found (bug 471962).
    is(fileContents.indexOf("inputfield=outer"), -1,
       "The saved inner frame does not contain outer POST data");

    // Check if inner POST data is found (bug 485196).
    isnot(fileContents.indexOf("inputfield=inner"), -1,
          "The saved inner frame was generated using the correct POST data");

    finish();
  }
}

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
                 this);

function createTemporarySaveDirectory() {
  var saveDir = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties)
                  .get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  if (!saveDir.exists())
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  return saveDir;
}

/**
 * Reads the contents of the provided short file (up to 1 MiB).
 *
 * @param aFile
 *        nsIFile object pointing to the file to be read.
 *
 * @return
 *        String containing the raw octets read from the file.
 */
function readShortFile(aFile) {
  var inputStream = Cc["@mozilla.org/network/file-input-stream;1"]
                      .createInstance(Ci.nsIFileInputStream);
  inputStream.init(aFile, -1, 0, 0);
  try {
    var scrInputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                           .createInstance(Ci.nsIScriptableInputStream);
    scrInputStream.init(inputStream);
    try {
      // Assume that the file is much shorter than 1 MiB.
      return scrInputStream.read(1048576);
    }
    finally {
      // Close the scriptable stream after reading, even if the operation
      // failed.
      scrInputStream.close();
    }
  }
  finally {
    // Close the stream after reading, if it is still open, even if the read
    // operation failed.
    inputStream.close();
  }
}
