/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
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
 * The Original Code is Downloads Panel Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

var EXPORTED_SYMBOLS = [
  "DownloadsCommon",
];

/**
 * Handles the Downloads panel shared methods and data access.
 *
 * This file includes the following constructors and global objects:
 *
 * DownloadsCommon
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides shared methods for all the instances of the user interface.
 *
 * DownloadsData
 * Retrieves the list of past and completed downloads from the underlying
 * Download Manager data, and provides asynchronous notifications allowing
 * to build a consistent view of the available data.
 *
 * DownloadsDataItem
 * Represents a single item in the list of downloads.  This object either wraps
 * an existing nsIDownload from the Download Manager, or provides the same
 * information read directly from the downloads database, with the possibility
 * of querying the nsIDownload lazily, for performance reasons.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

const nsIDM = Ci.nsIDownloadManager;

const kDownloadsStringBundleUrl =
  "chrome://browser/locale/downloads/downloads.properties";

const kDownloadsStringsRequiringFormatting = {
  sizeWithUnits: true,
  shortTimeLeftSeconds: true,
  shortTimeLeftMinutes: true,
  shortTimeLeftHours: true,
  shortTimeLeftDays: true,
  statusSeparator: true,
  statusSeparatorBeforeNumber: true,
  fileExecutableSecurityWarning: true
};

XPCOMUtils.defineLazyGetter(this, "DownloadsLocalFileCtor", function () {
  return Components.Constructor("@mozilla.org/file/local;1",
                                "nsILocalFile", "initWithPath");
});

////////////////////////////////////////////////////////////////////////////////
//// DownloadsCommon

/**
 * This object is exposed directly to the consumers of this JavaScript module,
 * and provides shared methods for all the instances of the user interface.
 */
const DownloadsCommon = {
  /**
   * Returns an object whose keys are the string names from the downloads string
   * bundle, and whose values are either the translated strings or functions
   * returning formatted strings.
   */
  get strings()
  {
    let strings = {};
    let sb = Services.strings.createBundle(kDownloadsStringBundleUrl);
    let enumerator = sb.getSimpleEnumeration();
    while (enumerator.hasMoreElements()) {
      let string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
      let stringName = string.key;
      if (stringName in kDownloadsStringsRequiringFormatting) {
        strings[stringName] = function() {
          // Convert "arguments" to a real array before calling into XPCOM.
          return sb.formatStringFromName(stringName,
                                         Array.slice(arguments, 0),
                                         arguments.length);
        };
      } else {
        strings[stringName] = string.value;
      }
    }
    delete this.strings;
    return this.strings = strings;
  },

  /**
   * Generates a very short string representing the given time left.
   *
   * @param aSeconds
   *        Value to be formatted.  It represents the number of seconds, it must
   *        be positive but does not need to be an integer.
   *
   * @return Formatted string, for example "30s" or "2h".  The returned value is
   *         maximum three characters long, at least in English.
   */
  formatTimeLeft: function DC_formatTimeLeft(aSeconds)
  {
    // Decide what text to show for the time
    let seconds = Math.round(aSeconds);
    if (!seconds) {
      return "";
    } else if (seconds <= 30) {
      return DownloadsCommon.strings["shortTimeLeftSeconds"](seconds);
    }
    let minutes = Math.round(aSeconds / 60);
    if (minutes < 60) {
      return DownloadsCommon.strings["shortTimeLeftMinutes"](minutes);
    }
    let hours = Math.round(minutes / 60);
    if (hours < 48) { // two days
      return DownloadsCommon.strings["shortTimeLeftHours"](hours);
    }
    let days = Math.round(hours / 24);
    return DownloadsCommon.strings["shortTimeLeftDays"](Math.min(days, 99));
  },

  /**
   * Indicates whether we should show the full Download Manager window interface
   * instead of the simplified panel interface.  The behavior of downloads
   * across browsing session is consistent with the selected interface.
   */
  get useWindowUI()
  {
    try {
      return Services.prefs.getBoolPref("browser.download.manager.useWindowUI");
    } catch (ex) { }
    return false;
  },
  
  /**
   * Returns a reference to the DownloadsData singleton.
   *
   * This does not need to be a lazy getter, since no initialization is required
   * at present.
   */
  get data() DownloadsData
};

/**
 * Returns true if we are executing on Windows Vista or a later version.
 */
XPCOMUtils.defineLazyGetter(DownloadsCommon, "isWinVistaOrHigher", function () {
  let os = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
  if (os != "WINNT") {
    return false;
  }
  let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
  return parseFloat(sysInfo.getProperty("version")) >= 6;
});

////////////////////////////////////////////////////////////////////////////////
//// DownloadsData

/**
 * Retrieves the list of past and completed downloads from the underlying
 * Download Manager data, and provides asynchronous notifications allowing to
 * build a consistent view of the available data.
 *
 * This object responds to real-time changes in the underlying Download Manager
 * data.  For example, the deletion of one or more downloads is notified through
 * the nsIObserver interface, while any state or progress change is notified
 * through the nsIDownloadProgressListener interface.
 *
 * Note that using this object does not automatically start the Download Manager
 * service.  Consumers will see an empty list of downloads until the service is
 * actually started.  This is useful to display a neutral progress indicator in
 * the main browser window until the autostart timeout elapses.
 */
const DownloadsData = {
  //////////////////////////////////////////////////////////////////////////////
  //// Initialization and termination

  /**
   * Starts receiving events for current downloads.
   *
   * @param aDownloadManagerService
   *        Reference to the service implementing nsIDownloadManager.  We need
   *        this because getService isn't available for us when this method is
   *        called, and we must ensure to register our listeners before the
   *        getService call for the Download Manager returns.
   */
  initializeDataLink: function DD_initializeDataLink(aDownloadManagerService)
  {
    // Start receiving real-time events.
    aDownloadManagerService.addListener(this);
    Services.obs.addObserver(this, "download-manager-remove-download", false);
    Services.obs.addObserver(this, "download-manager-database-type-changed",
                             false);
  },

  /**
   * Stops receiving events for current downloads and cancels any pending read.
   */
  terminateDataLink: function DD_terminateDataLink()
  {
    this._terminateDataAccess();

    // Stop receiving real-time events.
    Services.obs.removeObserver(this, "download-manager-database-type-changed");
    Services.obs.removeObserver(this, "download-manager-remove-download");
    Services.downloads.removeListener(this);
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Registration of views

  /**
   * Array of view objects that should be notified when the available download
   * data changes.
   */
  _views: [],

  /**
   * Adds an object to be notified when the available download data changes.
   * The specified object is initialized with the currently available downloads.
   *
   * @param aView
   *        DownloadsView object to be added.  This reference must be passed to
   *        removeView before termination.
   */
  addView: function DD_addView(aView)
  {
    this._views.push(aView);
    this._updateView(aView);
  },

  /**
   * Removes an object previously added using addView.
   *
   * @param aView
   *        DownloadsView object to be removed.
   */
  removeView: function DD_removeView(aView)
  {
    let index = this._views.indexOf(aView);
    if (index != -1) {
      this._views.splice(index, 1);
    }
  },

  /**
   * Ensures that the currently loaded data is added to the specified view.
   *
   * @param aView
   *        DownloadsView object to be initialized.
   */
  _updateView: function DD_updateView(aView)
  {
    // Indicate to the view that a batch loading operation is in progress.
    aView.onDataLoadStarting();

    // Sort backwards by download identifier, ensuring that the most recent
    // downloads are added first regardless of their state.
    let loadedItemsArray = [dataItem
                            for each (dataItem in this.dataItems)
                            if (dataItem)];
    loadedItemsArray.sort(function(a, b) b.downloadId - a.downloadId);
    for (let i = 0, dataItem; dataItem = loadedItemsArray[i]; i++) {
      aView.onDataItemAdded(dataItem, false);
    }

    // Notify the view that all data is available unless loading is in progress.
    if (!this._pendingStatement) {
      aView.onDataLoadCompleted();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// In-memory downloads data store

  /**
   * Object containing all the available DownloadsDataItem objects, indexed by
   * their numeric download identifier.  The identifiers of downloads that have
   * been removed from the Download Manager data are still present, however the
   * associated objects are replaced with the value "null".  This is required to
   * prevent race conditions when populating the list asynchronously.
   */
  dataItems: {},

  /**
   * While operating in Private Browsing Mode, persistent data items are parked
   * here until we return to the normal mode.
   */
  _persistentDataItems: {},

  /**
   * Clears the loaded data.
   */
  clear: function DD_clear()
  {
    this._terminateDataAccess();
    this.dataItems = {};
  },

  /**
   * Returns the data item associated with the provided source object.  The
   * source can be a download object that we received from the Download Manager
   * because of a real-time notification, or a row from the downloads database,
   * during the asynchronous data load.
   *
   * In case we receive download status notifications while we are still
   * populating the list of downloads from the database, we want the real-time
   * status to take precedence over the state that is read from the database,
   * which might be older.  This is achieved by creating the download item if
   * it's not already in the list, but never updating the returned object using
   * the data from the database, if the object already exists.
   *
   * @param aSource
   *        Object containing the data with which the item should be initialized
   *        if it doesn't already exist in the list.  This should implement
   *        either nsIDownload or mozIStorageRow.  If the item exists, this
   *        argument is only used to retrieve the download identifier.
   * @param aMayReuseId
   *        If false, indicates that the download should not be added if a
   *        download with the same identifier was removed in the meantime.  This
   *        ensures that, while loading the list asynchronously, downloads that
   *        have been removed in the meantime do no reappear inadvertently.
   *
   * @return New or existing data item, or null if the item was deleted from the
   *         list of available downloads.
   */
  _getOrAddDataItem: function DD_getOrAddDataItem(aSource, aMayReuseId)
  {
    let downloadId = (aSource instanceof Ci.nsIDownload)
                     ? aSource.id
                     : aSource.getResultByName("id");
    if (downloadId in this.dataItems) {
      let existingItem = this.dataItems[downloadId];
      if (existingItem || !aMayReuseId) {
        // Returns null if the download was removed and we can't reuse the item.
        return existingItem;
      }
    }

    let dataItem = new DownloadsDataItem(aSource);
    this.dataItems[downloadId] = dataItem;

    // Create the view items before returning.
    let addToStartOfList = aSource instanceof Ci.nsIDownload;
    for each (let view in this._views) {
      view.onDataItemAdded(dataItem, addToStartOfList);
    }
    return dataItem;
  },

  /**
   * Removes the data item with the specified identifier.
   *
   * This method can be called at most once per download identifier.
   */
  _removeDataItem: function DD_removeDataItem(aDownloadId)
  {
    if (aDownloadId in this.dataItems) {
      let dataItem = this.dataItems[aDownloadId];
      for each (let view in this._views) {
        view.onDataItemRemoved(dataItem);
      }
    }
    this.dataItems[aDownloadId] = null;
  },

  //////////////////////////////////////////////////////////////////////////////
  //// Persistent data loading

  /**
   * Asynchronous database statement used to read the list of downloads.
   */
  _statement: null,

  /**
   * Represents an executing statement, allowing its cancellation.
   */
  _pendingStatement: null,

  /**
   * Indicates which kind of items from the persistent downloads database have
   * been fully loaded in memory and are available to the views.  This can
   * assume the value of one of the kLoad constants.
   */
  _loadState: 0,

  /** No downloads have been fully loaded yet. */
  get kLoadNone() 0,
  /** All the active downloads in the database are loaded in memory. */
  get kLoadActive() 1,
  /** All the downloads in the database are loaded in memory. */
  get kLoadAll() 2,

  /**
   * Reloads the specified kind of downloads from the persistent database.  This
   * method must only be called when Private Browsing Mode is disabled.
   *
   * @param aActiveOnly
   *        True to load only active downloads from the database.
   */
  ensurePersistentDataLoaded:
  function DD_ensurePersistentDataLoaded(aActiveOnly)
  {
    if (this._pendingStatement) {
      // We are already in the process of reloading all downloads.
      return;
    }

    if (aActiveOnly) {
      if (this._loadState == this.kLoadNone) {
        // Indicate to the views that a batch loading operation is in progress.
        for each (let view in this._views) {
          view.onDataLoadStarting();
        }

        // Reload the list using the Download Manager service.
        let downloads = Services.downloads.activeDownloads;
        while (downloads.hasMoreElements()) {
          let download = downloads.getNext().QueryInterface(Ci.nsIDownload);
          this._getOrAddDataItem(download, true);
        }
        this._loadState = this.kLoadActive;

        // Indicate to the views that the batch loading operation is complete.
        for each (let view in this._views) {
          view.onDataLoadCompleted();
        }
      }
    } else {
      if (this._loadState != this.kLoadAll) {
        // Reload the list from the database asynchronously.
        this._statement = Services.downloads.DBConnection.createAsyncStatement(
                          "SELECT * FROM moz_downloads ORDER BY id DESC");
        this._pendingStatement = this._statement.executeAsync(this);
      }
    }
  },

  /**
   * Called after shutdown, deletes from the database all the downloads that
   * have not been loaded in memory yet.
   */
  deleteOldPersistentData: function DD_deleteOldPersistentData()
  {
    let loadedDownloadIds = [dataItem.downloadId
                             for each (dataItem in this.dataItems)
                             if (dataItem)];

    // Since we are shutting down, we must execute this operation synchronously,
    // otherwise the connection will be closed while the statement is running.
    let statement = Services.downloads.DBConnection.createStatement(
                    "DELETE FROM moz_downloads" +
                    " WHERE id NOT IN (" + loadedDownloadIds.join(",") + ")");
    statement.execute();
    statement.finalize();
  },

  /**
   * Cancels any pending data access and ensures views are notified.
   */
  _terminateDataAccess: function DD_terminateDataAccess()
  {
    if (this._pendingStatement) {
      this._pendingStatement.cancel();
      this._pendingStatement = null;
    }
    if (this._statement) {
      this._statement.finalize();
      this._statement = null;
    }

    // Close all the views on the current data.  Create a copy of the array
    // because some views might unregister while processing this event.
    for each (let view in Array.slice(this._views, 0)) {
      view.onDataInvalidated();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  handleResult: function DD_handleResult(aResultSet)
  {
    for (let row = aResultSet.getNextRow();
         row;
         row = aResultSet.getNextRow()) {
      // Add the download to the list and initialize it with the data we read,
      // unless we already received a notification providing more reliable
      // information for this download.
      this._getOrAddDataItem(row, false);
    }
  },

  handleError: function DD_handleError(aError)
  {
    Cu.reportError("Database statement execution error (" + aError.result +
                   "): " + aError.message);
  },

  handleCompletion: function DD_handleCompletion(aReason)
  {
    this._pendingStatement = null;

    // To ensure that we don't inadvertently delete more downloads from the
    // database than needed on shutdown, we should update the load state only if
    // the operation completed successfully.
    if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
      this._loadState = this.kLoadAll;
    }

    // Indicate to the views that the batch loading operation is complete, even
    // if the lookup failed or was canceled.  The only possible glitch happens
    // in case the database backend changes while loading data, when the views
    // would open and immediately close.  This case is rare enough not to need a
    // special treatment.
    for each (let view in this._views) {
      view.onDataLoadCompleted();
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DD_observe(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case "download-manager-remove-download":
        // If a single download was removed, remove the corresponding data item.
        if (aSubject) {
          this._removeDataItem(aSubject.QueryInterface(Ci.nsISupportsPRUint32));
          break;
        }

        // Multiple downloads have been removed.  Iterate over known downloads
        // and remove those that don't exist anymore.
        for each (let dataItem in this.dataItems) {
          if (dataItem) {
            try {
              Services.downloads.getDownload(dataItem.downloadId);
            } catch (ex) {
              this._removeDataItem(dataItem.downloadId);
            }
          }
        }
        break;

      case "download-manager-database-type-changed":
        let pbs = Cc["@mozilla.org/privatebrowsing;1"]
                  .getService(Ci.nsIPrivateBrowsingService);
        if (pbs.privateBrowsingEnabled) {
          // Save a reference to the persistent store before terminating access.
          this._persistentDataItems = this.dataItems;
          this.clear();
        } else {
          // Terminate data access, then restore the persistent store.
          this.clear();
          this.dataItems = this._persistentDataItems;
          this._persistentDataItems = null;
        }
        // Reinitialize the views with the current items.  View data has been
        // already invalidated by the previous calls.
        for each (let view in this._views) {
          this._updateView(view);
        }
        break;
    }
  },

  //////////////////////////////////////////////////////////////////////////////
  //// nsIDownloadProgressListener

  onDownloadStateChange: function DD_onDownloadStateChange(aState, aDownload)
  {
    let dataItem = this._getOrAddDataItem(aDownload, true);
    if (!dataItem) {
      return;
    }

    dataItem.state = aDownload.state;
    dataItem.referrer = aDownload.referrer && aDownload.referrer.spec;
    dataItem.resumable = aDownload.resumable;
    dataItem.startTime = Math.round(aDownload.startTime / 1000);
    dataItem.currBytes = aDownload.amountTransferred;
    dataItem.maxBytes = aDownload.size;

    for each (let view in this._views) {
      view.getViewItem(dataItem).onStateChange();
    }
  },

  onProgressChange: function DD_onProgressChange(aWebProgress, aRequest,
                                                  aCurSelfProgress,
                                                  aMaxSelfProgress,
                                                  aCurTotalProgress,
                                                  aMaxTotalProgress, aDownload)
  {
    let dataItem = this._getOrAddDataItem(aDownload, false);
    if (!dataItem) {
      return;
    }

    dataItem.currBytes = aDownload.amountTransferred;
    dataItem.maxBytes = aDownload.size;
    dataItem.speed = aDownload.speed;
    dataItem.percentComplete = aDownload.percentComplete;

    for each (let view in this._views) {
      view.getViewItem(dataItem).onProgressChange();
    }
  },

  onStateChange: function() { },

  onSecurityChange: function() { }
};

////////////////////////////////////////////////////////////////////////////////
//// DownloadsDataItem

/**
 * Represents a single item in the list of downloads.  This object either wraps
 * an existing nsIDownload from the Download Manager, or provides the same
 * information read directly from the downloads database, with the possibility
 * of querying the nsIDownload lazily, for performance reasons.
 *
 * @param aSource
 *        Object containing the data with which the item should be initialized.
 *        This should implement either nsIDownload or mozIStorageRow.
 */
function DownloadsDataItem(aSource)
{
  if (aSource instanceof Ci.nsIDownload) {
    this._initFromDownload(aSource);
  } else {
    this._initFromDataRow(aSource);
  }
}

DownloadsDataItem.prototype = {
  /**
   * Initializes this object from a download object of the Download Manager.
   *
   * The endTime property is initialized to the current date and time.
   *
   * @param aDownload
   *        The nsIDownload with the current state.
   */
  _initFromDownload: function DDI_initFromDownload(aDownload)
  {
    this.download = aDownload;

    // Fetch all the download properties eagerly.
    this.downloadId = aDownload.id;
    this.file = aDownload.target.spec;
    this.target = aDownload.displayName;
    this.uri = aDownload.source.spec;
    this.referrer = aDownload.referrer && aDownload.referrer.spec;
    this.state = aDownload.state;
    this.startTime = Math.round(aDownload.startTime / 1000);
    this.endTime = Date.now();
    this.currBytes = aDownload.amountTransferred;
    this.maxBytes = aDownload.size;
    this.resumable = aDownload.resumable;
    this.speed = aDownload.speed;
    this.percentComplete = aDownload.percentComplete;
  },

  /**
   * Initializes this object from a data row in the downloads database, without
   * querying the associated nsIDownload object, to improve performance when
   * loading the list of downloads asynchronously.
   *
   * When this object is initialized in this way, accessing the "download"
   * property loads the underlying nsIDownload object synchronously, and should
   * be avoided unless the object is really required.
   *
   * @param aStorageRow
   *        The mozIStorageRow from the downloads database.
   */
  _initFromDataRow: function DDI_initFromDataRow(aStorageRow)
  {
    // Get the download properties from the data row.
    this.downloadId = aStorageRow.getResultByName("id");
    this.file = aStorageRow.getResultByName("target");
    this.target = aStorageRow.getResultByName("name");
    this.uri = aStorageRow.getResultByName("source");
    this.referrer = aStorageRow.getResultByName("referrer");
    this.state = aStorageRow.getResultByName("state");
    this.startTime = Math.round(aStorageRow.getResultByName("startTime") / 1000);
    this.endTime = Math.round(aStorageRow.getResultByName("endTime") / 1000);
    this.currBytes = aStorageRow.getResultByName("currBytes");
    this.maxBytes = aStorageRow.getResultByName("maxBytes");

    // Allows accessing the underlying download object lazily.
    XPCOMUtils.defineLazyGetter(this, "download", function()
                                Services.downloads.getDownload(this.downloadId));

    // Now we have to determine if the download is resumable, but don't want to
    // access the underlying download object unnecessarily.  The only case where
    // the property is relevant is when we are currently downloading data, and
    // in this case the download object is already loaded in memory or will be
    // loaded very soon in any case.  In all the other cases, including a paused
    // download, we assume that the download is resumable.  The property will be
    // updated as soon as the underlying download state changes.
    if (this.state == nsIDM.DOWNLOAD_DOWNLOADING) {
      this.resumable = this.download.resumable;
    } else {
      this.resumable = true;
    }

    // Compute the other properties without accessing the download object.
    this.speed = 0;
    this.percentComplete = this.maxBytes <= 0
                           ? -1
                           : Math.round(this.currBytes / this.maxBytes * 100);
  },

  /**
   * Indicates whether the download is proceeding normally, and not finished
   * yet.  This includes paused downloads.  When this property is true, the
   * "progress" property represents the current progress of the download.
   */
  get inProgress()
  {
    return [
      nsIDM.DOWNLOAD_NOTSTARTED,
      nsIDM.DOWNLOAD_QUEUED,
      nsIDM.DOWNLOAD_DOWNLOADING,
      nsIDM.DOWNLOAD_PAUSED,
      nsIDM.DOWNLOAD_SCANNING,
    ].indexOf(this.state) != -1;
  },

  /**
   * This is true during the initial phases of a download, before the actual
   * download of data bytes starts.
   */
  get starting()
  {
    return this.state == nsIDM.DOWNLOAD_NOTSTARTED ||
           this.state == nsIDM.DOWNLOAD_QUEUED;
  },

  /**
   * Indicates whether the download is paused.
   */
  get paused()
  {
    return this.state == nsIDM.DOWNLOAD_PAUSED;
  },

  /**
   * Indicates whether the download is in a final state, either because it
   * completed successfully or because it was blocked.
   */
  get done()
  {
    return [
      nsIDM.DOWNLOAD_FINISHED,
      nsIDM.DOWNLOAD_BLOCKED_PARENTAL,
      nsIDM.DOWNLOAD_BLOCKED_POLICY,
      nsIDM.DOWNLOAD_DIRTY,
    ].indexOf(this.state) != -1;
  },

  /**
   * Indicates whether the download is finished and can be opened.
   */
  get openable()
  {
    return this.state == nsIDM.DOWNLOAD_FINISHED;
  },

  /**
   * Indicates whether the download stopped because of an error, and can be
   * resumed manually.
   */
  get canRetry()
  {
    return this.state == nsIDM.DOWNLOAD_CANCELED ||
           this.state == nsIDM.DOWNLOAD_FAILED;
  },

  /**
   * Returns the nsILocalFile for the download target.
   *
   * @throws if the native path is not valid.  This can happen if the same
   *         profile is used on different platforms, for example if a native
   *         Windows path is stored and then the item is accessed on a Mac.
   */
  get localFile()
  {
    // The download database may contain targets stored as file URLs or native
    // paths.  This can still be true for previously stored items, even if new
    // items are stored using their file URL.  See also bug 239948 comment 12.
    if (/^file:/.test(this.file)) {
      // Assume the file URL we obtained from the downloads database or from the
      // "spec" property of the target has the UTF-8 charset.
      let fileUrl = NetUtil.newURI(this.file).QueryInterface(Ci.nsIFileURL);
      return fileUrl.file.clone().QueryInterface(Ci.nsILocalFile);
    } else {
      // The downloads database contains a native path.  Try to create a local
      // file, though this may throw an exception if the path is invalid.
      return new DownloadsLocalFileCtor(this.file);
    }
  }
};
