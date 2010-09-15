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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#filter substitution

// for browser.xml binding
pref("toolkit.browser.cachePixelX", 800);
pref("toolkit.browser.cachePixelY", 2000);
pref("toolkit.browser.recacheRatio", 60);

pref("toolkit.defaultChromeURI", "chrome://browser/content/browser.xul");
pref("general.useragent.compatMode.firefox", true);
pref("browser.chromeURL", "chrome://browser/content/");

pref("browser.tabs.warnOnClose", true);
#ifdef MOZ_IPC
pref("browser.tabs.remote", true);
#else
pref("browser.tabs.remote", false);
#endif

pref("toolkit.screen.lock", false);

// From libpref/src/init/all.js, extended to allow a slightly wider zoom range.
pref("zoom.minPercent", 20);
pref("zoom.maxPercent", 400);
pref("toolkit.zoomManager.zoomValues", ".2,.3,.5,.67,.8,.9,1,1.1,1.2,1.33,1.5,1.7,2,2.4,3,4");
pref("zoom.dpiScale", 150);

/* use custom widget for html:select */
pref("ui.use_native_popup_windows", true);

/* use long press to display a context menu */
pref("ui.click_hold_context_menus", true);

/* cache prefs */
pref("browser.cache.disk.enable", false);
pref("browser.cache.disk.capacity", 0); // kilobytes

pref("browser.cache.memory.enable", true);
pref("browser.cache.memory.capacity", 1024); // kilobytes

/* tile cache prefs */
pref("tile.cache.size", 10); // tiles

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes

/* offline cache prefs */
pref("browser.offline-apps.notify", true);
pref("browser.cache.offline.enable", true);
pref("browser.cache.offline.capacity", 5120); // kilobytes
pref("offline-apps.quota.max", 2048); // kilobytes
pref("offline-apps.quota.warn", 1024); // kilobytes

/* protocol warning prefs */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.mailto", false);

/* http prefs */
pref("network.http.pipelining", true);
pref("network.http.pipelining.ssl", true);
pref("network.http.proxy.pipelining", true);
pref("network.http.pipelining.maxrequests" , 6);
pref("network.http.keep-alive.timeout", 600);
pref("network.http.max-connections", 6);
pref("network.http.max-connections-per-server", 4);
pref("network.http.max-persistent-connections-per-server", 4);
pref("network.http.max-persistent-connections-per-proxy", 4);
#ifdef MOZ_PLATFORM_MAEMO
pref("network.autodial-helper.enabled", true);
#endif

/* history max results display */
pref("browser.display.history.maxresults", 100);

/* session history */
pref("browser.sessionhistory.max_total_viewers", 1);
pref("browser.sessionhistory.max_entries", 50);

/* these should help performance */
pref("mozilla.widget.force-24bpp", true);
pref("mozilla.widget.use-buffer-pixmap", true);
pref("mozilla.widget.disable-native-theme", true);

/* download manager (don't show the window or alert) */
pref("browser.download.useDownloadDir", true);
pref("browser.download.folderList", 1); // Default to ~/Downloads
pref("browser.download.manager.showAlertOnComplete", false);
pref("browser.download.manager.showAlertInterval", 2000);
pref("browser.download.manager.retention", 2);
pref("browser.download.manager.showWhenStarting", false);
pref("browser.download.manager.closeWhenDone", true);
pref("browser.download.manager.openDelay", 0);
pref("browser.download.manager.focusWhenStarting", false);
pref("browser.download.manager.flashCount", 2);
pref("browser.download.manager.displayedHistoryDays", 7);

/* download alerts (disabled above) */
pref("alerts.slideIncrement", 1);
pref("alerts.slideIncrementTime", 10);
pref("alerts.totalOpenTime", 6000);
pref("alerts.height", 50);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.expireMasterPassword", false);
pref("signon.SignonFileName", "signons.txt");

/* form helper */
pref("formhelper.enabled", true);
pref("formhelper.autozoom", true);
pref("formhelper.restore", false);
pref("formhelper.caretLines.portrait", 4);
pref("formhelper.caretLines.landscape", 1);
pref("formhelper.harmonizeValue", 10);
pref("formhelper.margin", 15);

/* find helper */
pref("findhelper.autozoom", true);

/* autocomplete */
pref("browser.formfill.enable", true);

#ifdef WINCE
pref("layout.css.devPixelsPerPx", "1");
#endif

/* spellcheck */
pref("layout.spellcheckDefault", 1);

/* extension manager and xpinstall */
pref("xpinstall.whitelist.add", "addons.mozilla.org");

pref("extensions.autoupdate.enabled", true);
pref("extensions.autoupdate.interval", 86400);
pref("extensions.update.enabled", false);
pref("extensions.update.interval", 86400);
pref("extensions.dss.enabled", false);
pref("extensions.dss.switchPending", false);
pref("extensions.ignoreMTimeChanges", false);
pref("extensions.logging.enabled", false);
pref("extensions.hideInstallButton", true);
pref("extensions.showMismatchUI", false);
pref("extensions.hideUpdateButton", false);

pref("extensions.update.url", "https://versioncheck.addons.mozilla.org/update/VersionCheck.php?reqVersion=%REQ_VERSION%&id=%ITEM_ID%&version=%ITEM_VERSION%&maxAppVersion=%ITEM_MAXAPPVERSION%&status=%ITEM_STATUS%&appID=%APP_ID%&appVersion=%APP_VERSION%&appOS=%APP_OS%&appABI=%APP_ABI%&locale=%APP_LOCALE%&currentAppVersion=%CURRENT_APP_VERSION%&updateType=%UPDATE_TYPE%");

/* preferences for the Get Add-ons pane */
pref("extensions.getAddons.cache.enabled", true);
pref("extensions.getAddons.maxResults", 15);
pref("extensions.getAddons.recommended.browseURL", "https://addons.mozilla.org/%LOCALE%/mobile/recommended/");
pref("extensions.getAddons.recommended.url", "https://services.addons.mozilla.org/%LOCALE%/mobile/api/%API_VERSION%/list/featured/all/%MAX_RESULTS%/%OS%/%VERSION%");
pref("extensions.getAddons.search.browseURL", "https://addons.mozilla.org/%LOCALE%/mobile/search?q=%TERMS%");
pref("extensions.getAddons.search.url", "https://services.addons.mozilla.org/%LOCALE%/mobile/api/%API_VERSION%/search/%TERMS%/all/%MAX_RESULTS%/%OS%/%VERSION%");
pref("extensions.getAddons.browseAddons", "https://addons.mozilla.org/%LOCALE%/mobile/");

/* blocklist preferences */
pref("extensions.blocklist.enabled", true);
pref("extensions.blocklist.interval", 86400);
pref("extensions.blocklist.url", "https://addons.mozilla.org/blocklist/3/%APP_ID%/%APP_VERSION%/%PRODUCT%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/");
pref("extensions.blocklist.detailsURL", "https://www.mozilla.com/%LOCALE%/blocklist/");

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

pref("keyword.enabled", true);
pref("keyword.URL", "http://www.google.com/search?ie=UTF-8&oe=UTF-8&sourceid=navclient&gfns=1&q=");

pref("accessibility.typeaheadfind", false);
pref("accessibility.typeaheadfind.timeout", 5000);
pref("accessibility.typeaheadfind.flashBar", 1);
pref("accessibility.typeaheadfind.linksonly", false);
pref("accessibility.typeaheadfind.casesensitive", 0);
// zoom key(F7) conflicts with caret browsing on maemo
pref("accessibility.browsewithcaret_shortcut.enabled", false);

// Whether or not we show a dialog box informing the user that the update was
// successfully applied.
pref("app.update.showInstalledUI", false);

// pointer to the default engine name
pref("browser.search.defaultenginename", "chrome://browser/locale/region.properties");
// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// disable logging for the search service by default
pref("browser.search.log", false);

// ordering of search engines in the engine list.
pref("browser.search.order.1", "chrome://browser/locale/region.properties");
pref("browser.search.order.2", "chrome://browser/locale/region.properties");

// disable updating
pref("browser.search.update", false);
pref("browser.search.update.log", false);
pref("browser.search.updateinterval", 6);

// enable search suggestions by default
pref("browser.search.suggest.enabled", true);

// Tell the search service to load search plugins from the locale JAR
pref("browser.search.loadFromJars", true);
pref("browser.search.jarURIs", "chrome://browser/locale/searchplugins/");

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

// enable xul error pages
pref("browser.xul.error_pages.enabled", true);

// Specify emptyRestriction = 0 so that bookmarks appear in the list by default
pref("browser.urlbar.default.behavior", 0);
pref("browser.urlbar.default.behavior.emptyRestriction", 0);

// Let the faviconservice know that we display favicons as 32x32px so that it
// uses the right size when optimizing favicons
pref("places.favicons.optimizeToDimension", 32);

// various and sundry awesomebar prefs (should remove/re-evaluate
// these once bug 447900 is fixed)
pref("browser.urlbar.clickSelectsAll", true);
pref("browser.urlbar.doubleClickSelectsAll", true);
pref("browser.urlbar.autoFill", false);
pref("browser.urlbar.matchOnlyTyped", false);
pref("browser.urlbar.matchBehavior", 1);
pref("browser.urlbar.filter.javascript", true);
pref("browser.urlbar.maxRichResults", 24); // increased so we see more results when portrait
pref("browser.urlbar.search.chunkSize", 1000);
pref("browser.urlbar.search.timeout", 100);
pref("browser.urlbar.restrict.history", "^");
pref("browser.urlbar.restrict.bookmark", "*");
pref("browser.urlbar.restrict.tag", "+");
pref("browser.urlbar.match.title", "#");
pref("browser.urlbar.match.url", "@");
pref("browser.history.grouping", "day");
pref("browser.history.showSessions", false);
pref("browser.sessionhistory.max_entries", 50);
pref("browser.history_expire_days", 180);
pref("browser.history_expire_days_min", 90);
pref("browser.history_expire_sites", 40000);
pref("privacy.item.history", true);
pref("browser.places.migratePostDataAnnotations", true);
pref("browser.places.updateRecentTagsUri", true);
pref("places.frecency.numVisits", 10);
pref("places.frecency.numCalcOnIdle", 50);
pref("places.frecency.numCalcOnMigrate", 50);
pref("places.frecency.updateIdleTime", 60000);
pref("places.frecency.firstBucketCutoff", 4);
pref("places.frecency.secondBucketCutoff", 14);
pref("places.frecency.thirdBucketCutoff", 31);
pref("places.frecency.fourthBucketCutoff", 90);
pref("places.frecency.firstBucketWeight", 100);
pref("places.frecency.secondBucketWeight", 70);
pref("places.frecency.thirdBucketWeight", 50);
pref("places.frecency.fourthBucketWeight", 30);
pref("places.frecency.defaultBucketWeight", 10);
pref("places.frecency.embedVisitBonus", 0);
pref("places.frecency.linkVisitBonus", 100);
pref("places.frecency.typedVisitBonus", 2000);
pref("places.frecency.bookmarkVisitBonus", 150);
pref("places.frecency.downloadVisitBonus", 0);
pref("places.frecency.permRedirectVisitBonus", 0);
pref("places.frecency.tempRedirectVisitBonus", 0);
pref("places.frecency.defaultVisitBonus", 0);
pref("places.frecency.unvisitedBookmarkBonus", 140);
pref("places.frecency.unvisitedTypedBonus", 200);

// disable color management
pref("gfx.color_management.mode", 0);

// don't allow JS to move and resize existing windows
pref("dom.disable_window_move_resize", true);

// prevent click image resizing for nsImageDocument
pref("browser.enable_click_image_resizing", false);

// open in tab preferences
// 0=default window, 1=current window/tab, 2=new window, 3=new tab in most window
pref("browser.link.open_external", 3);
pref("browser.link.open_newwindow", 3);
// 0=force all new windows to tabs, 1=don't force, 2=only force those with no features set
pref("browser.link.open_newwindow.restriction", 0);

// controls which bits of private data to clear. by default we clear them all.
pref("privacy.item.cache", true);
pref("privacy.item.cookies", true);
pref("privacy.item.offlineApps", true);
pref("privacy.item.history", true);
pref("privacy.item.formdata", true);
pref("privacy.item.downloads", true);
pref("privacy.item.passwords", true);
pref("privacy.item.sessions", true);
pref("privacy.item.geolocation", true);
pref("privacy.item.siteSettings", true);

#ifdef MOZ_PLATFORM_MAEMO
pref("plugins.force.wmode", "opaque");
#endif

// URL to the Learn More link XXX this is the firefox one.  Bug 495578 fixes this.
pref("browser.geolocation.warning.infoURL", "http://www.mozilla.com/%LOCALE%/firefox/geolocation/");

// base url for the wifi geolocation network provider
pref("geo.wifi.uri", "https://www.google.com/loc/json");

// enable geo
pref("geo.enabled", true);

// content sink control -- controls responsiveness during page load
// see https://bugzilla.mozilla.org/show_bug.cgi?id=481566#c9
// use interactive mode rather than perf mode since that prioritizes user events while loading
pref("content.sink.enable_perf_mode", 1); // 0 - switch, 1 - interactive, 2 - perf
pref("content.sink.pending_event_mode", 2);
pref("content.sink.event_probe_rate", 5);
pref("content.sink.interactive_deflect_count", 1000);
pref("content.sink.perf_deflect_count", 10);
pref("content.sink.interactive_parse_time",5000);
pref("content.sink.perf_parse_time", 5000);
pref("content.sink.interactive_time", 0);
pref("content.sink.initial_perf_time", 0);

pref("javascript.options.jit.content", true);
pref("javascript.options.jit.chrome", true);
pref("javascript.options.mem.gc_frequency", 300);

pref("dom.max_chrome_script_run_time", 30);
pref("dom.max_script_run_time", 20);

// JS error console
pref("browser.console.showInPanel", false);

// kinetic tweakables
pref("browser.ui.kinetic.updateInterval", 30);
pref("browser.ui.kinetic.decelerationRate", 20);
pref("browser.ui.kinetic.speedSensitivity", 80);
pref("browser.ui.kinetic.swipeLength", 160);

// zooming
pref("browser.ui.zoom.pageFitGranularity", 9); // don't zoom to fit by less than 1/9
pref("browser.ui.zoom.animationDuration", 200); // ms duration of double-tap zoom animation

// pinch gesture
pref("browser.ui.pinch.maxGrowth", 150);     // max pinch distance growth
pref("browser.ui.pinch.maxShrink", 200);     // max pinch distance shrinkage
pref("browser.ui.pinch.scalingFactor", 500); // scaling factor for above pinch limits

// Touch radius (area around the touch location to look for target elements):
pref("browser.ui.touch.left", 8);
pref("browser.ui.touch.right", 8);
pref("browser.ui.touch.top", 12);
pref("browser.ui.touch.bottom", 4);
pref("browser.ui.touch.weight.visited", 120); // percentage

// plugins
#if ANDROID
pref("plugin.disable", true);
pref("dom.ipc.plugins.enabled", false);
#else
pref("plugin.disable", false);
pref("dom.ipc.plugins.enabled", true);
#endif

// product URLs
// The breakpad report server to link to in about:crashes
pref("breakpad.reportURL", "http://crash-stats.mozilla.com/report/index/");
pref("app.releaseNotesURL", "http://www.mozilla.com/%LOCALE%/mobile/%VERSION%/releasenotes/");
//pref("app.support.baseURL", "http://support.mozilla.com/1/mobile/%VERSION%/%OS%/%LOCALE%/");
pref("app.support.baseURL", "http://mobile.support.mozilla.com/");
pref("app.faqURL", "http://www.mozilla.com/%LOCALE%/mobile/faq/");
pref("app.privacyURL", "http://www.mozilla.com/%LOCALE%/legal/privacy/firefox/mobile/");
pref("app.creditsURL", "http://www.mozilla.com/%LOCALE%/mobile/credits/");

// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");

// Override some named colors to avoid inverse OS themes
pref("ui.-moz-dialog", "#efebe7");
pref("ui.-moz-dialogtext", "#101010");
pref("ui.-moz-field", "#fff");
pref("ui.-moz-fieldtext", "#1a1a1a");
pref("ui.-moz-buttonhoverface", "#f3f0ed");
pref("ui.-moz-buttonhovertext", "#101010");
pref("ui.-moz-combobox", "#fff");
pref("ui.-moz-comboboxtext", "#101010");
pref("ui.buttonface", "#ece7e2");
pref("ui.buttonhighlight", "#fff");
pref("ui.buttonshadow", "#aea194");
pref("ui.buttontext", "#101010");
pref("ui.captiontext", "#101010");
pref("ui.graytext", "#b1a598");
pref("ui.highlight", "#fad184");
pref("ui.highlighttext", "#1a1a1a");
pref("ui.infobackground", "#f5f5b5");
pref("ui.infotext", "#000");
pref("ui.menu", "#f7f5f3");
pref("ui.menutext", "#101010");
pref("ui.threeddarkshadow", "#000");
pref("ui.threedface", "#ece7e2");
pref("ui.threedhighlight", "#fff");
pref("ui.threedlightshadow", "#ece7e2");
pref("ui.threedshadow", "#aea194");
pref("ui.window", "#efebe7");
pref("ui.windowtext", "#101010");
pref("ui.windowframe", "#efebe7");

#ifdef MOZ_OFFICIAL_BRANDING
pref("browser.search.param.yahoo-fr", "moz35");
pref("browser.search.param.yahoo-fr-cjkt", "moz35");
pref("browser.search.param.yahoo-fr-ja", "mozff");
#endif

/* app update prefs */
pref("app.update.timer", 60000); // milliseconds (1 min)

#ifdef MOZ_UPDATER
pref("app.update.enabled", true);
pref("app.update.timerFirstInterval", 20000); // milliseconds
pref("app.update.auto", false);
pref("app.update.channel", "@MOZ_UPDATE_CHANNEL@");
pref("app.update.mode", 1);
pref("app.update.silent", false);
pref("app.update.url", "https://aus2.mozilla.org/update/4/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/%PLATFORM_VERSION%/update.xml");
pref("app.update.nagTimer.restart", 86400);
pref("app.update.promptWaitTime", 43200);
pref("app.update.idletime", 60);
pref("app.update.showInstalledUI", false);
pref("app.update.incompatible.mode", 0);
pref("app.update.download.backgroundInterval", 0);

#ifdef MOZ_OFFICIAL_BRANDING
pref("app.update.interval", 86400);
pref("app.update.url.manual", "http://www.mozilla.com/%LOCALE%/m/");
pref("app.update.url.details", "http://www.mozilla.com/%LOCALE%/mobile/releases/");
#else
pref("app.update.interval", 28800);
#endif
#endif

// replace newlines with spaces on paste into single-line text boxes
pref("editor.singleLine.pasteNewlines", 2);

#ifdef MOZ_PLATFORM_MAEMO
// update fonts for better readability
pref("font.default.x-baltic", "SwissA");
pref("font.default.x-central-euro", "SwissA");
pref("font.default.x-cyrillic", "SwissA");
pref("font.default.x-unicode", "SwissA");
pref("font.default.x-user-def", "SwissA");
pref("font.default.x-western", "SwissA");
#endif

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

// sync service
pref("services.sync.client.type", "mobile");
pref("services.sync.registerEngines", "Tab,Bookmarks,Form,History,Password");
pref("services.sync.autoconnectDelay", 5);

// Drag thresholds
pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);
