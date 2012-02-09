/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Panos Astithas <past@mozilla.com>
 *   Victor Porof <vporof@mozilla.com>
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

"use strict";

var gInitialized = false;
var gClient = null;
var gTabClient = null;


function initDebugger()
{
  window.removeEventListener("DOMContentLoaded", initDebugger, false);
  if (gInitialized) {
    return;
  }
  gInitialized = true;

  DebuggerView.Stackframes.initialize();
  DebuggerView.Properties.initialize();
  DebuggerView.Scripts.initialize();
}

/**
 * Called by chrome to set up a debugging session.
 *
 * @param DebuggerClient aClient
 *        The debugger client.
 * @param object aTabGrip
 *        The remote protocol grip of the tab.
 */
function startDebuggingTab(aClient, aTabGrip)
{
  gClient = aClient;

  gClient.attachTab(aTabGrip.actor, function(aResponse, aTabClient) {
    if (aTabClient) {
      gTabClient = aTabClient;
      gClient.attachThread(aResponse.threadActor, function(aResponse, aThreadClient) {
        if (!aThreadClient) {
          dump("Couldn't attach to thread: "+aResponse.error+"\n");
          return;
        }
        ThreadState.connect(aThreadClient, function() {
          StackFrames.connect(aThreadClient, function() {
            SourceScripts.connect(aThreadClient, function() {
              aThreadClient.resume();
            });
          });
        });
      });
    }
  });
}

function shutdownDebugger()
{
  window.removeEventListener("unload", shutdownDebugger, false);

  SourceScripts.disconnect();
  StackFrames.disconnect();
  ThreadState.disconnect();
  ThreadState.activeThread = false;

  DebuggerView.Stackframes.destroy();
  DebuggerView.Properties.destroy();
  DebuggerView.Scripts.destroy();
}


/**
 * ThreadState keeps the UI up to date with the state of the
 * thread (paused/attached/etc.).
 */
var ThreadState = {
  activeThread: null,

  /**
   * Connect to a thread client.
   * @param object aThreadClient
   *        The thread client.
   * @param function aCallback
   *        The next function in the initialization sequence.
   */
  connect: function TS_connect(aThreadClient, aCallback) {
    this.activeThread = aThreadClient;
    aThreadClient.addListener("paused", ThreadState.update);
    aThreadClient.addListener("resumed", ThreadState.update);
    aThreadClient.addListener("detached", ThreadState.update);
    this.update();
    aCallback && aCallback();
  },

  /**
   * Update the UI after a thread state change.
   */
  update: function TS_update(aEvent) {
    DebuggerView.Stackframes.updateState(this.activeThread.state);
    if (aEvent == "detached") {
      ThreadState.activeThread = false;
    }
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function TS_disconnect() {
    this.activeThread.removeListener("paused", ThreadState.update);
    this.activeThread.removeListener("resumed", ThreadState.update);
    this.activeThread.removeListener("detached", ThreadState.update);
  }
};

ThreadState.update = ThreadState.update.bind(ThreadState);

/**
 * Keeps the stack frame list up-to-date, using the thread client's
 * stack frame cache.
 */
var StackFrames = {
  pageSize: 25,
  activeThread: null,
  selectedFrame: null,

  /**
   * Watch a given thread client.
   * @param object aThreadClient
   *        The thread client.
   * @param function aCallback
   *        The next function in the initialization sequence.
   */
  connect: function SF_connect(aThreadClient, aCallback) {
    DebuggerView.Stackframes.addClickListener(this.onClick);

    this.activeThread = aThreadClient;
    aThreadClient.addListener("paused", this.onPaused);
    aThreadClient.addListener("framesadded", this.onFrames);
    aThreadClient.addListener("framescleared", this.onFramesCleared);
    this.onFramesCleared();
    aCallback && aCallback();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function TS_disconnect() {
    this.activeThread.removeListener("paused", this.onPaused);
    this.activeThread.removeListener("framesadded", this.onFrames);
    this.activeThread.removeListener("framescleared", this.onFramesCleared);
  },

  /**
   * Handler for the thread client's paused notification.
   */
  onPaused: function SF_onPaused() {
    this.activeThread.fillFrames(this.pageSize);
  },

  /**
   * Handler for the thread client's framesadded notification.
   */
  onFrames: function SF_onFrames() {
    DebuggerView.Stackframes.empty();

    for each (let frame in this.activeThread.cachedFrames) {
      this._addFramePanel(frame);
    }

    if (this.activeThread.moreFrames) {
      DebuggerView.Stackframes.dirty = true;
    }

    if (!this.selectedFrame) {
      this.selectFrame(0);
    }
  },

  /**
   * Handler for the thread client's framescleared notification.
   */
  onFramesCleared: function SF_onFramesCleared() {
    DebuggerView.Stackframes.emptyText();
    this.selectedFrame = null;
    // Clear the properties as well.
    DebuggerView.Properties.localScope.empty();
    DebuggerView.Properties.globalScope.empty();
  },

  /**
   * Event handler for clicks on stack frames.
   */
  onClick: function SF_onClick(aEvent) {
    let target = aEvent.target;
    while (target) {
      if (target.stackFrame) {
        this.selectFrame(target.stackFrame.depth);
        return;
      }
      target = target.parentNode;
    }
  },

  /**
   * Marks the stack frame in the specified depth as selected and updates the
   * properties view with the stack frame's data.
   *
   * @param number aDepth
   *        The depth of the frame in the stack.
   */
  selectFrame: function SF_selectFrame(aDepth) {
    if (this.selectedFrame !== null) {
      DebuggerView.Stackframes.highlightFrame(this.selectedFrame, false);
    }

    this.selectedFrame = aDepth;
    if (aDepth !== null) {
      DebuggerView.Stackframes.highlightFrame(this.selectedFrame, true);
    }

    // Display the local variables.
    let frame = this.activeThread.cachedFrames[aDepth];
    if (!frame) {
      return;
    }
    let localScope = DebuggerView.Properties.localScope;
    localScope.empty();
    // Add "this".
    if (frame["this"]) {
      let thisVar = localScope.addVar("this");
      thisVar.setGrip({ "type": frame["this"].type,
                        "class": frame["this"].class });
      this._addExpander(thisVar, frame["this"]);
    }

    if (frame.arguments && frame.arguments.length > 0) {
      // Add "arguments".
      let argsVar = localScope.addVar("arguments");
      argsVar.setGrip({ "type": "object", "class": "Arguments" });
      this._addExpander(argsVar, frame.arguments);

      // Add variables for every argument.
      let objClient = this.activeThread.pauseGrip(frame.callee);
      objClient.getSignature(function SF_getSignature(aResponse) {
        for (let i = 0; i < aResponse.parameters.length; i++) {
          let param = aResponse.parameters[i];
          let paramVar = localScope.addVar(param);
          let paramVal = frame.arguments[i];
          paramVar.setGrip(paramVal);
          this._addExpander(paramVar, paramVal);
        }
      }.bind(this));
    }
  },

  _addExpander: function SF_addExpander(aVar, aObject) {
    // No need for expansion for null and undefined values, but we do need them
    // for frame.arguments which is a regular array.
    if (!aObject || typeof aObject != "object" ||
        (aObject.type != "object" && !Array.isArray(aObject))) {
      return;
    }
    // Add a dummy property to force the twisty to show up.
    aVar.addProperties({ " ": { value: " " }});
    aVar.onexpand = this._addVarProperties.bind(this, aVar, aObject);
  },

  _addVarProperties: function SF_addVarProperties(aVar, aObject) {
    // Retrieve the properties only once.
    if (aVar.fetched) {
      return;
    }
    // Clear the placeholder property put in place to display the twisty.
    aVar.empty();

    // For arrays we have to construct a grip-like object to pass into
    // addProperties.
    if (Array.isArray(aObject)) {
      let properties = { length: { writable: true, value: aObject.length } };
      for (let i = 0; i < aObject.length; i++) {
        properties[i + ""] = { value: aObject[i] };
      }
      aVar.addProperties(properties);
      // Expansion handlers must be set after the properties are added.
      for (let i = 0; i < aObject.length; i++) {
        this._addExpander(aVar[i + ""], aObject[i]);
      }
      aVar.fetched = true;
      return;
    }

    let objClient = this.activeThread.pauseGrip(aObject);
    objClient.getPrototypeAndProperties(function SF_onProtoAndProps(aResponse) {
      // Add __proto__.
      if (aResponse.prototype.type != "null") {
        let properties = {};
        properties["__proto__ "] = { value: aResponse.prototype };
        aVar.addProperties(properties);
        // Expansion handlers must be set after the properties are added.
        this._addExpander(aVar["__proto__ "], aResponse.prototype);
      }

      // Sort the rest of the properties before adding them, for better UX.
      let properties = {};
      for each (let prop in Object.keys(aResponse.ownProperties).sort()) {
        properties[prop] = aResponse.ownProperties[prop];
      }
      aVar.addProperties(properties);
      // Expansion handlers must be set after the properties are added.
      for (let prop in aResponse.ownProperties) {
        this._addExpander(aVar[prop], aResponse.ownProperties[prop].value);
      }

      aVar.fetched = true;
    }.bind(this));
  },

  /**
   * Adds the specified stack frame to the list.
   *
   * @param Debugger.Frame aFrame
   *        The new frame to add.
   */
  _addFramePanel: function SF_addFramePanel(aFrame) {
    let depth = aFrame.depth;
    let idText = "#" + aFrame.depth + " ";
    let nameText = this._frameTitle(aFrame);

    let panel = DebuggerView.Stackframes.addFrame(depth, idText, nameText);

    if (panel) {
      panel.stackFrame = aFrame;
    }
  },

  /**
   * Loads more stack frames from the debugger server cache.
   */
  _addMoreFrames: function SF_addMoreFrames() {
    this.activeThread.fillFrames(
      this.activeThread.cachedFrames.length + this.pageSize);
  },

  /**
   * Create a textual representation for the stack frame specified, for
   * displaying in the stack frame list.
   *
   * @param Debugger.Frame aFrame
   *        The stack frame to label.
   */
  _frameTitle: function SF_frameTitle(aFrame) {
    if (aFrame.type == "call") {
      return aFrame["calleeName"] ? aFrame["calleeName"] + "()" : "(anonymous)";
    }

    return "(" + aFrame.type + ")";
  }
};

StackFrames.onPaused = StackFrames.onPaused.bind(StackFrames);
StackFrames.onFrames = StackFrames.onFrames.bind(StackFrames);
StackFrames.onFramesCleared = StackFrames.onFramesCleared.bind(StackFrames);
StackFrames.onClick = StackFrames.onClick.bind(StackFrames);

/**
 * Keeps the source script list up-to-date, using the thread client's
 * source script cache.
 */
var SourceScripts = {
  pageSize: 25,
  activeThread: null,

  /**
   * Watch a given thread client.
   * @param object aThreadClient
   *        The thread client.
   * @param function aCallback
   *        The next function in the initialization sequence.
   */
  connect: function SS_connect(aThreadClient, aCallback) {
    DebuggerView.Scripts.addChangeListener(this.onChange);

    this.activeThread = aThreadClient;
    aThreadClient.addListener("paused", this.onPaused);
    aThreadClient.addListener("scriptsadded", this.onScripts);
    aThreadClient.addListener("scriptscleared", this.onScriptsCleared);
    this.onScriptsCleared();
    aCallback && aCallback();
  },

  /**
   * Disconnect from the client.
   */
  disconnect: function TS_disconnect() {
    this.activeThread.removeListener("paused", this.onPaused);
    this.activeThread.removeListener("scriptsadded", this.onScripts);
    this.activeThread.removeListener("scriptscleared", this.onScriptsCleared);
  },

  /**
   * Handler for the thread client's paused notification.
   */
  onPaused: function SS_onPaused() {
    this.activeThread.fillScripts();
  },

  /**
   * Handler for the debugger client's unsolicited newScript notification.
   */
  onNewScript: function SS_onNewScript(aNotification, aPacket) {
    this._addScript({ url: aPacket.url, startLine: aPacket.startLine });
  },

  /**
   * Handler for the thread client's scriptsadded notification.
   */
  onScripts: function SS_onScripts() {
    this.onScriptsCleared();
    for each (let script in this.activeThread.cachedScripts) {
      this._addScript(script);
    }
  },

  /**
   * Handler for the thread client's scriptscleared notification.
   */
  onScriptsCleared: function SS_onScriptsCleared() {
    DebuggerView.Scripts.empty();
  },

  /**
   * Handler for changes on the selected source script.
   */
  onChange: function SS_onClick(aEvent) {
    let scripts = aEvent.target;
    let script = scripts.selectedItem.getUserData("sourceScript");
    this._showScript(script);
  },

  /**
   * Add the specified script to the list and display it in the editor if the
   * editor is empty.
   */
  _addScript: function SS_addScript(aScript) {
    DebuggerView.Scripts.addScript(aScript.url, aScript);

    if (window.editor.getCharCount() == 0) {
      this._showScript(aScript);
    }
  },

  /**
   * Load the editor with the script text if available, otherwise fire an event
   * to load and display the script text.
   */
  _showScript: function SS_showScript(aScript) {
    if (!aScript.loaded) {
      // Notify the chrome code that we need to load a script file.
      var evt = document.createEvent("CustomEvent");
      evt.initCustomEvent("Debugger:LoadSource", true, false, aScript.url);
      document.documentElement.dispatchEvent(evt);
      window.editor.setText(DebuggerView.getStr("loadingText"));
    } else {
      window.editor.setText(aScript.text);
    }
  }
};

SourceScripts.onPaused = SourceScripts.onPaused.bind(SourceScripts);
SourceScripts.onScripts = SourceScripts.onScripts.bind(SourceScripts);
SourceScripts.onNewScript = SourceScripts.onNewScript.bind(SourceScripts);
SourceScripts.onScriptsCleared = SourceScripts.onScriptsCleared.bind(SourceScripts);
SourceScripts.onChange = SourceScripts.onChange.bind(SourceScripts);

window.addEventListener("DOMContentLoaded", initDebugger, false);
window.addEventListener("unload", shutdownDebugger, false);
