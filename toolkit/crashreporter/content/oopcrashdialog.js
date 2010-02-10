// This code is TEMPORARY for submitting crashes via an ugly popup dialog:
// bug 525849 tracks the real implementation.

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/CrashSubmit.jsm");

var id;

function collectData() {
  let directoryService = Cc["@mozilla.org/file/directory_service;1"].
    getService(Ci.nsIProperties);
  pendingDir = directoryService.get("UAppData", Ci.nsIFile);
  pendingDir.append("Crash Reports");
  pendingDir.append("pending");
  if (!pendingDir.exists())
    pendingDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0770);

  reportsDir = directoryService.get("UAppData", Ci.nsIFile);
  reportsDir.append("Crash Reports");
  reportsDir.append("submitted");
  if (!reportsDir.exists())
      reportsDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0770);

  let dumpFile = window.arguments[0].QueryInterface(Ci.nsIFile);
  let extraFile = dumpFile.clone();
  id = dumpFile.leafName.replace(/.dmp$/, "");
  extraFile.leafName = id + ".extra";
  dumpFile.moveTo(pendingDir, "");
  extraFile.moveTo(pendingDir, "");
}

function submitDone()
{
  // we don't currently distinguish between success or failure here
  window.close();
}

function onSubmit()
{
  document.documentElement.getButton('accept').disabled = true;
  document.documentElement.getButton('accept').label = 'Sending';
  document.getElementById('throbber').src = 'chrome://global/skin/icons/loading_16.png';
  CrashSubmit.submit(id, document.getElementById('iframe-holder'),
                     submitDone, submitDone);
  return false;
}
