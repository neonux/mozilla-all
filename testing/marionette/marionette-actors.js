/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/**
 * Gecko-specific actors.
 */

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
             .getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://marionette/content/marionette-simpletest.js");
loader.loadSubScript("chrome://marionette/content/marionette-log-obj.js");
Cu.import("chrome://marionette/content/marionette-elements.js");

let prefs = Cc["@mozilla.org/preferences-service;1"]
            .getService(Ci.nsIPrefBranch);
prefs.setBoolPref("marionette.contentListener", false);

let xulAppInfo = Cc["@mozilla.org/xre/app-info;1"]
                 .getService(Ci.nsIXULAppInfo);
let appName = xulAppInfo.name;

// import logger
Cu.import("resource://gre/modules/services-sync/log4moz.js");
let logger = Log4Moz.repository.getLogger("Marionette");
logger.info('marionette-actors.js loaded');

/**
 * Creates the root actor once a connection is established
 */

function createRootActor(aConnection)
{
  return new MarionetteRootActor(aConnection);
}

/**
 * Root actor for Marionette. Add any future actors to its actor pool.
 * Implements methods needed by resource:///modules/devtools/dbg-server.jsm
 */

function MarionetteRootActor(aConnection)
{
  this.conn = aConnection;
  this._marionetteActor = new MarionetteDriverActor(this.conn);
  this._marionetteActorPool = null; //hold future actors

  this._marionetteActorPool = new ActorPool(this.conn);
  this._marionetteActorPool.addActor(this._marionetteActor);
  this.conn.addActorPool(this._marionetteActorPool);
}

MarionetteRootActor.prototype = {
  /**
   * Called when a client first makes a connection
   *
   * @return object
   *         returns the name of the actor, the application type, and any traits
   */
  sayHello: function MRA_sayHello() {
    return { from: "root",
             applicationType: "gecko",
             traits: [] };
  },

  /**
   * Called when client disconnects. Cleans up marionette driver actions.
   */
  disconnect: function MRA_disconnect() {
    this._marionetteActor.deleteSession();
  },

  /**
   * Used to get the running marionette actor, so we can issue commands
   *
   * @return object
   *         Returns the ID the client can use to communicate with the
   *         MarionetteDriverActor
   */
  getMarionetteID: function MRA_getMarionette() {
    return { "from": "root",
             "id": this._marionetteActor.actorID } ;
  },
}

// register the calls
MarionetteRootActor.prototype.requestTypes = {
  "getMarionetteID": MarionetteRootActor.prototype.getMarionetteID,
  "sayHello": MarionetteRootActor.prototype.sayHello
};

/**
 * This actor is responsible for all marionette API calls. It gets created
 * for each connection and manages all chrome and browser based calls. It 
 * mediates content calls by issuing appropriate messages to the content process.
 */
function MarionetteDriverActor(aConnection)
{
  this.uuidGen = Cc["@mozilla.org/uuid-generator;1"]
                 .getService(Ci.nsIUUIDGenerator);

  this.conn = aConnection;
  this.messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                             getService(Ci.nsIChromeFrameMessageManager);
  this.windowMediator = Cc['@mozilla.org/appshell/window-mediator;1'].getService(Ci.nsIWindowMediator);
  this.browsers = {}; //holds list of BrowserObjs
  this.curBrowser = null; // points to current browser
  this.context = "content";
  this.scriptTimeout = null;
  this.elementManager = new ElementManager([SELECTOR, NAME, LINK_TEXT, PARTIAL_LINK_TEXT]);
  this.timer = null;
  this.marionetteLog = new MarionetteLogObj();
  this.command_id = null;

  //register all message listeners
  this.messageManager.addMessageListener("Marionette:ok", this);
  this.messageManager.addMessageListener("Marionette:done", this);
  this.messageManager.addMessageListener("Marionette:error", this);
  this.messageManager.addMessageListener("Marionette:log", this);
  this.messageManager.addMessageListener("Marionette:testLog", this);
  this.messageManager.addMessageListener("Marionette:register", this);
  this.messageManager.addMessageListener("Marionette:goUrl", this);
}

MarionetteDriverActor.prototype = {

  //name of the actor
  actorPrefix: "marionette",

  /**
   * Helper method to send async messages to the content listener
   *
   * @param string name
   *        Suffix of the targetted message listener (Marionette:<suffix>)
   * @param object values
   *        Object to send to the listener
   */
  sendAsync: function MDA_sendAsync(name, values) {
    this.messageManager.sendAsyncMessage("Marionette:" + name + this.browsers[this.curBrowser].curFrameId, values);
  },

  /**
   * Helper methods:
   */

  /**
   * Generic method to pass a response to the client
   * 
   * @param object msg
   *        Response to send back to client
   * @param string command_id
   *        Unique identifier assigned to the client's request.
   *        Used to distinguish the asynchronous responses.
   */
  sendToClient: function MDA_sendToClient(msg, command_id) {
    logger.info("sendToClient: " + JSON.stringify(msg) + ", " + command_id + ", " + this.command_id);
    if (command_id == undefined || command_id == this.command_id) {
      this.conn.send(msg);
      this.command_id = null;
    }
  },

  /**
   * Send a value to client
   *
   * @param object value
   *        Value to send back to client
   * @param string command_id
   *        Unique identifier assigned to the client's request.
   *        Used to distinguish the asynchronous responses.
   */
  sendResponse: function MDA_sendResponse(value, command_id) {
    if (typeof(value) == 'undefined')
        value = null;
    this.sendToClient({from:this.actorID, value: value}, command_id);
  },

  /**
   * Send ack to client
   * 
   * @param string command_id
   *        Unique identifier assigned to the client's request.
   *        Used to distinguish the asynchronous responses.
   */
  sendOk: function MDA_sendOk(command_id) {
    this.sendToClient({from:this.actorID, ok: true}, command_id);
  },

  /**
   * Send error message to client
   *
   * @param string message
   *        Error message
   * @param number status
   *        Status number
   * @param string trace
   *        Stack trace
   * @param string command_id
   *        Unique identifier assigned to the client's request.
   *        Used to distinguish the asynchronous responses.
   */
  sendError: function MDA_sendError(message, status, trace, command_id) {
    let error_msg = {message: message, status: status, stacktrace: trace};
    this.sendToClient({from:this.actorID, error: error_msg}, command_id);
  },

  /**
   * Gets the current active window
   * 
   * @return nsIDOMWindow
   */
  getCurrentWindow: function MDA_getCurrentWindow() {
    let type = null;
    if (appName != "B2G") {
      type = 'navigator:browser';
    }
    return this.windowMediator.getMostRecentWindow(type);
  },

  /**
   * Gets the the window enumerator
   *
   * @return nsISimpleEnumerator
   */
  getWinEnumerator: function MDA_getWinEnumerator() {
    let type = null;
    if (appName != "B2G") {
      type = 'navigator:browser';
    }
    return this.windowMediator.getEnumerator(type);
  },

  /**
   * Create a new BrowserObj for window and add to known browsers
   * 
   * @param nsIDOMWindow win
   *        Window for which we will create a BrowserObj
   *
   * @return string
   *        Returns the unique server-assigned ID of the window
   */
  addBrowser: function MDA_addBrowser(win) {
    let browser = new BrowserObj(win);
    let winId = win.QueryInterface(Ci.nsIInterfaceRequestor).
                    getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
    winId = winId + ((appName == "B2G") ? '-b2g' : '');
    if (this.elementManager.seenItems[winId] == undefined) {
      //add this to seenItems so we can guarantee the user will get winId as this window's id
      this.elementManager.seenItems[winId] = win;
    }
    this.browsers[winId] = browser;
    return winId;
  },

  /**
   * Start a new session in a new browser. 
   *
   * If newSession is true, we will switch focus to the start frame 
   * when it registers. Also, if it is in desktop, then a new tab 
   * with the start page uri (about:blank) will be opened.
   *
   * @param nsIDOMWindow win
   *        Window whose browser we need to access
   * @param boolean newSession
   *        True if this is the first time we're talking to this browser
   */
  startBrowser: function MDA_startBrowser(win, newSession) {
    let winId = this.addBrowser(win);
    this.curBrowser = winId;
    this.browsers[this.curBrowser].newSession = newSession;
    this.browsers[this.curBrowser].startSession(newSession);
    this.browsers[this.curBrowser].loadFrameScript("chrome://marionette/content/marionette-listener.js", win);
  },

  /**
   * Marionette API:
   */

  /**
   * Create a new session. This creates a BrowserObj.
   *
   * In a desktop environment, this opens a new 'about:blank' tab for 
   * the client to test in.
   *
   */
  newSession: function MDA_newSession() {
    if (!prefs.getBoolPref("marionette.contentListener")) {
      this.startBrowser(this.getCurrentWindow(), true);
    }
    else if ((appName == "B2G")&& (this.curBrowser == null)) {
      //if there is a content listener, then we just wake it up
      let winId = this.addBrowser(this.getCurrentWindow());
      this.curBrowser = winId;
      this.browsers[this.curBrowser].startSession(false);
      this.messageManager.sendAsyncMessage("Marionette:restart", {});
    }
    else {
      this.sendError("Session already running", 500, null);
    }
  },

  /**
   * Log message. Accepts user defined log-level.
   *
   * @param object aRequest
   *        'value' member holds log message
   *        'level' member hold log level
   */
  log: function MDA_log(aRequest) {
    this.marionetteLog.log(aRequest.value, aRequest.level);
    this.sendOk();
  },

  /**
   * Return all logged messages.
   */
  getLogs: function MDA_getLogs() {
    this.sendResponse(this.marionetteLog.getLogs());
  },

  /**
   * Sets the context of the subsequent commands to be either 'chrome' or 'content'
   *
   * @param object aRequest
   *        'value' member holds the name of the context to be switched to
   */
  setContext: function MDA_setContext(aRequest) {
    let context = aRequest.value;
    if (context != "content" && context != "chrome") {
      this.sendError("invalid context", 500, null);
    }
    else {
      this.context = context;
      this.sendOk();
    }
  },

  /**
   * Returns a chrome sandbox that can be used by the execute_foo functions.
   *
   * @param nsIDOMWindow aWindow
   *        Window in which we will execute code
   * @param Marionette marionette
   *        Marionette test instance
   * @param object args
   *        Client given args
   * @return Sandbox
   *        Returns the sandbox
   */
  createExecuteSandbox: function MDA_createExecuteSandbox(aWindow, marionette, args) {
    try {
      args = this.elementManager.convertWrappedArguments(args, aWindow);
    }
    catch(e) {
      this.sendError(e.message, e.num, e.stack);
      return;
    }

    let _chromeSandbox = new Cu.Sandbox(aWindow,
       { sandboxPrototype: aWindow, wantXrays: false, sandboxName: ''});
    _chromeSandbox.__namedArgs = this.elementManager.applyNamedArgs(args);
    _chromeSandbox.__marionetteParams = args;

    marionette.exports.forEach(function(fn) {
      _chromeSandbox[fn] = marionette[fn].bind(marionette);
    });

    return _chromeSandbox;
  },

  /**
   * Executes a script in the given sandbox.
   *
   * @param Sandbox sandbox
   *        Sandbox in which the script will run
   * @param string script
   *        The script to run
   * @param boolean directInject
   *        If true, then the script will be run as is,
   *        and not as a function body (as you would
   *        do using the WebDriver spec)
   * @param boolean async
   *        True if the script is asynchronous
   */
  executeScriptInSandbox: function MDA_executeScriptInSandbox(sandbox, script,
     directInject, async) {
    try {
      if (directInject && async &&
          (this.scriptTimeout == null || this.scriptTimeout == 0)) {
        this.sendError("Please set a timeout", 21, null);
        return;
      }

      let res = Cu.evalInSandbox(script, sandbox, "1.8");

      if (directInject && !async &&
          (res == undefined || res.passed == undefined)) {
        this.sendError("finish() not called", 500, null);
        return;
      }

      if (!async) {
        this.sendResponse(this.elementManager.wrapValue(res));
      }
    }
    catch (e) {
      this.sendError(e.name + ': ' + e.message, 17, e.stack);
    }
  },

  /**
   * Execute the given script either as a function body (executeScript)
   * or directly (for 'mochitest' like JS Marionette tests)
   *
   * @param object aRequest
   *        'value' member is the script to run
   *        'args' member holds the arguments to the script
   * @param boolean directInject
   *        if true, it will be run directly and not as a 
   *        function body
   */
  execute: function MDA_execute(aRequest, directInject) {
    if (this.context == "content") {
      this.sendAsync("executeScript", {value: aRequest.value, args: aRequest.args});
      return;
    }

    let curWindow = this.getCurrentWindow();
    let marionette = new Marionette(false, curWindow, "chrome", this.marionetteLog);
    let _chromeSandbox = this.createExecuteSandbox(curWindow, marionette, aRequest.args);
    if (!_chromeSandbox)
      return;

    try {
      _chromeSandbox.finish = function chromeSandbox_finish() {
        return marionette.generate_results();
      };

      let script;
      if (directInject) {
        script = aRequest.value;
      }
      else {
        script = "let func = function() {" +
                       aRequest.value + 
                     "};" +
                     "func.apply(null, __marionetteParams);";
      }
      this.executeScriptInSandbox(_chromeSandbox, script, directInject, false);
    }
    catch (e) {
      this.sendError(e.name + ': ' + e.message, 17, e.stack);
    }
  },

  /**
   * Set the timeout for asynchronous script execution
   *
   * @param object aRequest
   *        'value' member is time in milliseconds to set timeout
   */
  setScriptTimeout: function MDA_setScriptTimeout(aRequest) {
    let timeout = parseInt(aRequest.value);
    if(isNaN(timeout)){
      this.sendError("Not a Number", 500, null);
    }
    else {
      this.scriptTimeout = timeout;
      this.sendAsync("setScriptTimeout", {value: timeout});
      this.sendOk();
    }
  },

  /**
   * execute pure JS script. Used to execute 'mochitest'-style Marionette tests.
   *
   * @param object aRequest
   *        'value' member holds the script to execute
   *        'args' member holds the arguments to the script
   *        'timeout' member will be used as the script timeout if it is given
   */
  executeJSScript: function MDA_executeJSScript(aRequest) {
    //all pure JS scripts will need to call Marionette.finish() to complete the test.
    if (this.context == "chrome") {
      if (aRequest.timeout) {
        this.executeWithCallback(aRequest, aRequest.timeout);
      }
      else {
        this.execute(aRequest, true);
      }
    }
    else {
      this.sendAsync("executeJSScript", {value:aRequest.value, args:aRequest.args, timeout:aRequest.timeout});
   }
  },

  /**
   * This function is used by executeAsync and executeJSScript to execute a script
   * in a sandbox. 
   * 
   * For executeJSScript, it will return a message only when the finish() method is called.
   * For executeAsync, it will return a response when marionetteScriptFinished/arguments[arguments.length-1] 
   * method is called, or if it times out.
   *
   * @param object aRequest
   *        'value' member holds the script to execute
   *        'args' member holds the arguments for the script
   * @param boolean directInject
   *        if true, it will be run directly and not as a 
   *        function body
   */
  executeWithCallback: function MDA_executeWithCallback(aRequest, directInject) {
    this.command_id = this.uuidGen.generateUUID().toString();

    if (this.context == "content") {
      this.sendAsync("executeAsyncScript", {value: aRequest.value,
                                            args: aRequest.args,
                                            id: this.command_id});
      return;
    }

    let curWindow = this.getCurrentWindow();
    let original_onerror = curWindow.onerror;
    let that = this;
    let marionette = new Marionette(true, curWindow, "chrome", this.marionetteLog);
    marionette.command_id = this.command_id;

    function chromeAsyncReturnFunc(value, status) {
      if (value == undefined)
        value = null;
      if (that.command_id == marionette.command_id) {
        if (that.timer != null) {
          that.timer.cancel();
          that.timer = null;
        }

        curWindow.onerror = original_onerror;

        if (status == 0 || status == undefined) {
          that.sendToClient({from: that.actorID, value: that.elementManager.wrapValue(value), status: status},
                            marionette.command_id);
        }
        else {
          let error_msg = {message: value, status: status, stacktrace: null};
          that.sendToClient({from: that.actorID, error: error_msg},
                            marionette.command_id);
        }
      }
    }

    curWindow.onerror = function (errorMsg, url, lineNumber) {
      chromeAsyncReturnFunc(errorMsg + " at: " + url + " line: " + lineNumber, 17);
      return true;
    };

    function chromeAsyncFinish() {
      chromeAsyncReturnFunc(marionette.generate_results(), 0);
    }

    let _chromeSandbox = this.createExecuteSandbox(curWindow, marionette, aRequest.args);
    if (!_chromeSandbox)
      return;

    try {

      this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      if (this.timer != null) {
        this.timer.initWithCallback(function() {
          chromeAsyncReturnFunc("timed out", 28);
        }, that.scriptTimeout, Ci.nsITimer.TYPE_ONESHOT);
      }

      _chromeSandbox.returnFunc = chromeAsyncReturnFunc;
      _chromeSandbox.finish = chromeAsyncFinish;

      let script;
      if (directInject) {
        script = aRequest.value;
      }
      else {
        script =  '__marionetteParams.push(returnFunc);'
                + 'let marionetteScriptFinished = returnFunc;'
                + 'let __marionetteFunc = function() {' + aRequest.value + '};'
                + '__marionetteFunc.apply(null, __marionetteParams);';
      }

      this.executeScriptInSandbox(_chromeSandbox, script, directInject, true);
    } catch (e) {
      this.sendError(e.name + ": " + e.message, 17, e.stack, marionette.command_id);
    }
  },

  /**
   * Navigates to given url
   *
   * @param object aRequest
   *        'value' member holds the url to navigate to
   */
  goUrl: function MDA_goUrl(aRequest) {
    this.sendAsync("goUrl", aRequest);
  },

  /**
   * Gets current url
   */
  getUrl: function MDA_getUrl() {
    if (this.context == "chrome") {
      this.sendResponse(this.getCurrentWindow().location.href);
    }
    else {
      this.sendAsync("getUrl", {});
    }
  },

  /**
   * Go back in history
   */
  goBack: function MDA_goBack() {
    this.sendAsync("goBack", {});
  },

  /**
   * Go forward in history
   */
  goForward: function MDA_goForward() {
    this.sendAsync("goForward", {});
  },

  /**
   * Refresh the page
   */
  refresh: function MDA_refresh() {
    this.sendAsync("refresh", {});
  },

  /**
   * Get the current window's server-assigned ID
   */
  getWindow: function MDA_getWindow() {
    this.sendResponse(this.curBrowser);
  },

  /**
   * Get the server-assigned IDs of all available windows
   */
  getWindows: function MDA_getWindows() {
    let res = [];
    let winEn = this.getWinEnumerator(); 
    while(winEn.hasMoreElements()) {
      let foundWin = winEn.getNext();
      let winId = foundWin.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils).outerWindowID;
      winId = winId + ((appName == "B2G") ? '-b2g' : '');
      res.push(winId)
    }
    this.sendResponse(res);
  },

  /**
   * Switch to a window based on name or server-assigned id.
   * Searches based on name, then id.
   *
   * @param object aRequest
   *        'value' member holds the id of the window to switch to
   */
  switchToWindow: function MDA_switchToWindow(aRequest) {
    let winEn = this.getWinEnumerator(); 
    while(winEn.hasMoreElements()) {
      let foundWin = winEn.getNext();
      let winId = foundWin.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils).outerWindowID;
      winId = winId + ((appName == "B2G") ? '-b2g' : '');
      if (aRequest.value == foundWin.name || aRequest.value == winId) {
        if (this.browsers[winId] == undefined) {
          //enable Marionette in that browser window
          this.startBrowser(foundWin, false);
        }
        foundWin.focus();
        this.curBrowser = winId;
        this.sendOk();
        return;
      }
    }
    this.sendError("Unable to locate window " + aRequest.value, 23, null);
  },

  /**
   * Switch to a given frame within the current window
   *
   * @param object aRequest
   *        'value' holds the id of the frame to switch to
   */
  switchToFrame: function MDA_switchToFrame(aRequest) {
    this.sendAsync("switchToFrame", aRequest);
  },

  /**
   * Set timeout for searching for elements
   *
   * @param object aRequest
   *        'value' holds the search timeout in milliseconds
   */
  setSearchTimeout: function MDA_setSearchTimeout(aRequest) {
    if (this.context == "chrome") {
      try {
        this.elementManager.setSearchTimeout(aRequest.value);
        this.sendOk();
      }
      catch (e) {
        this.sendError(e.message, e.num, e.stack);
      }
    }
    else {
      this.sendAsync("setSearchTimeout", {value: aRequest.value});
    }
  },

  /**
   * Find an element using the indicated search strategy.
   *
   * @param object aRequest
   *        'using' member indicates which search method to use
   *        'value' member is the value the client is looking for
   */
  findElement: function MDA_findElement(aRequest) {
    if (this.context == "chrome") {
      let id;
      try {
        let notify = this.sendResponse.bind(this);
        id = this.elementManager.find(aRequest, this.getCurrentWindow().document, notify, false);
      }
      catch (e) {
        this.sendError(e.message, e.num, e.stack);
        return;
      }
    }
    else {
      this.sendAsync("findElementContent", {value: aRequest.value, using: aRequest.using, element: aRequest.element});
    }
  },

  /**
   * Find elements using the indicated search strategy.
   *
   * @param object aRequest
   *        'using' member indicates which search method to use
   *        'value' member is the value the client is looking for
   */
  findElements: function MDA_findElements(aRequest) {
    if (this.context == "chrome") {
      let id;
      try {
        let notify = this.sendResponse.bind(this);
        id = this.elementManager.find(aRequest, this.getCurrentWindow().document, notify, true);
      }
      catch (e) {
        this.sendError(e.message, e.num, e.stack);
        return;
      }
    }
    else {
      this.sendAsync("findElementsContent", {value: aRequest.value, using: aRequest.using, element: aRequest.element});
    }
  },

  /**
   * Send click event to element
   * 
   * @param object aRequest
   *        'element' member holds the reference id to
   *        the element that will be clicked
   */
  clickElement: function MDA_clickElement(aRequest) {
    this.sendAsync("clickElement", {element: aRequest.element});
  },

  /**
   * Deletes the session.
   * 
   * If it is a desktop environment, it will close the session's tab and close all listeners
   *
   * If it is a B2G environment, it will make the main content listener sleep, and close
   * all other listeners. The main content listener persists after disconnect (it's the homescreen),
   * and can safely be reused.
   */
  deleteSession: function MDA_deleteSession() {
    if (this.browsers[this.curBrowser] != null) {
      if (appName == "B2G") {
        this.messageManager.sendAsyncMessage("Marionette:sleepSession" + this.browsers[this.curBrowser].mainContentId, {});
        this.browsers[this.curBrowser].knownFrames.splice(this.browsers[this.curBrowser].knownFrames.indexOf(this.browsers[this.curBrowser].mainContentId), 1);
      }
      else {
        //don't set this pref for B2G since the framescript can be safely reused
        prefs.setBoolPref("marionette.contentListener", false);
      }
      this.browsers[this.curBrowser].closeTab();
      //delete session in each frame in each browser
      for (let win in this.browsers) {
        for (let i in this.browsers[win].knownFrames) {
          this.messageManager.sendAsyncMessage("Marionette:deleteSession" + this.browsers[win].knownFrames[i], {});
        }
      }
      let winEnum = this.getWinEnumerator();
      while (winEnum.hasMoreElements()) {
        winEnum.getNext().messageManager.removeDelayedFrameScript("chrome://marionette/content/marionette-listener.js"); 
      }
    }
    this.sendOk();
    this.messageManager.removeMessageListener("Marionette:ok", this);
    this.messageManager.removeMessageListener("Marionette:done", this);
    this.messageManager.removeMessageListener("Marionette:error", this);
    this.messageManager.removeMessageListener("Marionette:log", this);
    this.messageManager.removeMessageListener("Marionette:testLog", this);
    this.messageManager.removeMessageListener("Marionette:register", this);
    this.messageManager.removeMessageListener("Marionette:goUrl", this);
    this.curBrowser = null;
    this.elementManager.reset();
  },

  /**
   * Receives all messages from content messageManager
   */
  receiveMessage: function MDA_receiveMessage(message) {
    switch (message.name) {
      case "DOMContentLoaded":
        this.sendOk();
        this.messageManager.removeMessageListener("DOMContentLoaded", this, true);
        break;
      case "Marionette:done":
        this.sendResponse(message.json.value, message.json.command_id);
        break;
      case "Marionette:ok":
        this.sendOk(message.json.command_id);
        break;
      case "Marionette:error":
        this.sendError(message.json.message, message.json.status, message.json.stacktrace, message.json.command_id);
        break;
      case "Marionette:log":
        //log server-side messages
        logger.info(message.json.message);
        break;
      case "Marionette:testLog":
        //log messages from tests
        this.marionetteLog.addLogs(message.json.value);
        break;
      case "Marionette:register":
        // This code processes the content listener's registration information
        // and either accepts the listener, or ignores it
        let nullPrevious= (this.browsers[this.curBrowser].curFrameId == null);
        let curWin = this.getCurrentWindow();
        let frameObject = curWin.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils).getOuterWindowWithId(message.json.value);
        let reg = this.browsers[this.curBrowser].register(message.json.value, message.json.href);
        if (reg) {
          this.elementManager.seenItems[reg] = frameObject; //add to seenItems
          if (nullPrevious && (this.browsers[this.curBrowser].curFrameId != null)) {
            this.sendAsync("newSession", {B2G: (appName == "B2G")});
            if (this.browsers[this.curBrowser].newSession) {
              this.sendResponse(reg);
            }
          }
        }
        return reg;
      case "Marionette:goUrl":
        // if content determines that the goUrl call is directed at a top level window (not an iframe)
        // it calls back into chrome to load the uri.
        this.browsers[this.curBrowser].loadURI(message.json.value, this);
        break;
    }
  },
  /**
   * for non-e10s eventListening
   */
  handleEvent: function MDA_handleEvent(evt) {
    if (evt.type == "DOMContentLoaded") {
      this.sendOk();
      this.browsers[this.curBrowser].browser.removeEventListener("DOMContentLoaded", this, false);
    }
  },
};

MarionetteDriverActor.prototype.requestTypes = {
  "newSession": MarionetteDriverActor.prototype.newSession,
  "log": MarionetteDriverActor.prototype.log,
  "getLogs": MarionetteDriverActor.prototype.getLogs,
  "setContext": MarionetteDriverActor.prototype.setContext,
  "executeScript": MarionetteDriverActor.prototype.execute,
  "setScriptTimeout": MarionetteDriverActor.prototype.setScriptTimeout,
  "executeAsyncScript": MarionetteDriverActor.prototype.executeWithCallback,
  "executeJSScript": MarionetteDriverActor.prototype.executeJSScript,
  "setSearchTimeout": MarionetteDriverActor.prototype.setSearchTimeout,
  "findElement": MarionetteDriverActor.prototype.findElement,
  "findElements": MarionetteDriverActor.prototype.findElements,
  "clickElement": MarionetteDriverActor.prototype.clickElement,
  "goUrl": MarionetteDriverActor.prototype.goUrl,
  "getUrl": MarionetteDriverActor.prototype.getUrl,
  "goBack": MarionetteDriverActor.prototype.goBack,
  "goForward": MarionetteDriverActor.prototype.goForward,
  "refresh":  MarionetteDriverActor.prototype.refresh,
  "getWindow":  MarionetteDriverActor.prototype.getWindow,
  "getWindows":  MarionetteDriverActor.prototype.getWindows,
  "switchToFrame": MarionetteDriverActor.prototype.switchToFrame,
  "switchToWindow": MarionetteDriverActor.prototype.switchToWindow,
  "deleteSession": MarionetteDriverActor.prototype.deleteSession
};

/**
 * Creates a BrowserObj. BrowserObjs handle interactions with the
 * browser, according to the current environment (desktop, b2g, etc.)
 *
 * @param nsIDOMWindow win
 *        The window whose browser needs to be accessed
 */

function BrowserObj(win) {
  this.DESKTOP = "desktop";
  this.B2G = "B2G";
  this.browser;
  this.browser_mm;
  this.tab = null;
  this.knownFrames = [];
  this.curFrameId = null;
  this.startPage = "about:blank";
  this.mainContentId = null; // used in B2G to identify the homescreen content page
  this.messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                             getService(Ci.nsIChromeFrameMessageManager);
  this.newSession = true; //used to set curFrameId upon new session
  this.setBrowser(win);
}

BrowserObj.prototype = {
  /**
   * Set the browser if the application is not B2G
   *
   * @param nsIDOMWindow win
   *        current window reference
   */
  setBrowser: function BO_setBrowser(win) {
    if (appName != "B2G") {
      this.browser = win.gBrowser; 
    }
  },
  /**
   * Called when we start a session with this browser.
   *
   * In a desktop environment, if newTab is true, it will start 
   * a new 'about:blank' tab and change focus to this tab.
   *
   * This will also set the active messagemanager for this object
   *
   * @param boolean newTab
   *        If true, create new tab
   */
  startSession: function BO_startSession(newTab) {
    if (appName == "B2G") {
      return;
    }
    if (newTab) {
      this.addTab(this.startPage);
      //if we have a new tab, make it the selected tab and give it focus
      this.browser.selectedTab = this.tab;
      let newTabBrowser = this.browser.getBrowserForTab(this.tab);
      //focus the tab
      newTabBrowser.ownerDocument.defaultView.focus();
    }
    else {
      //set this.tab to the currently focused tab
      this.tab = this.browser.selectedTab;
      this.browser_mm = this.browser.getBrowserForTab(this.tab).messageManager;
    }
  },

  /**
   * Closes current tab
   */
  closeTab: function BO_closeTab() {
    if (this.tab != null && (appName != "B2G")) {
      this.browser.removeTab(this.tab);
      this.tab = null;
    }
  },

  /**
   * Opens a tab with given uri
   *
   * @param string uri
   *      URI to open
   */
  addTab: function BO_addTab(uri) {
    this.tab = this.browser.addTab(uri, true);
  },

  /**
   * Load a uri in the current tab
   *
   * @param string uri
   *      URI to load
   * @param EventListener listener
   *      event listener fired on load
   */
  loadURI: function BO_openURI(uri, listener) {
    if (appName != "B2G") {
      this.browser.addEventListener("DOMContentLoaded", listener, false);
      this.browser.loadURI(uri);
    }
    else {
      this.messageManager.addMessageListener("DOMContentLoaded", listener, true);
      this.browser.selectedBrowser.loadURI(uri);
    }
  },

  /**
   * Loads content listeners if we don't already have them
   *
   * @param string script
   *        path of script to load
   * @param nsIDOMWindow frame
   *        frame to load the script in
   */
  loadFrameScript: function BO_loadFrameScript(script, frame) {
    if (!prefs.getBoolPref("marionette.contentListener")) {
      frame.window.messageManager.loadFrameScript(script, true);
      prefs.setBoolPref("marionette.contentListener", true);
    }
  },

  /**
   * Registers a new frame, and sets its current frame id to this frame
   * if it is not already assigned, and if a) we already have a session 
   * or b) we're starting a new session and it is the right start frame.
   *
   * @param string id
   *        frame id
   * @param string href
   *        frame's href 
   */
  register: function BO_register(id, href) {
    let uid = id + ((appName == "B2G") ? '-b2g' : '');
    if (this.curFrameId == null) {
      if ((!this.newSession) || (this.newSession && ((appName == "B2G") || href.indexOf(this.startPage) > -1))) {
        this.curFrameId = uid;
        this.mainContentId = uid;
      }
    }
    this.knownFrames.push(uid); //used to deletesessions
    return uid;
  },
}
