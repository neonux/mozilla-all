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
 * The Original Code is Telephony.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Sinker Li <thinker@codemud.net>
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

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const DEBUG = false; // set to true to see debug messages

const RADIOINTERFACELAYER_CID =
  Components.ID("{2d831c8d-6017-435b-a80c-e5d422810cea}");
const DATACALLINFO_CID =
  Components.ID("{ef474cd9-94f7-4c05-a31b-29b9de8a10d2}");

const nsIAudioManager = Ci.nsIAudioManager;
const nsIRadioInterfaceLayer = Ci.nsIRadioInterfaceLayer;

const kSmsReceivedObserverTopic          = "sms-received";
const kSmsDeliveredObserverTopic         = "sms-delivered";
const DOM_SMS_DELIVERY_RECEIVED          = "received";
const DOM_SMS_DELIVERY_SENT              = "sent";

XPCOMUtils.defineLazyServiceGetter(this, "gSmsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

XPCOMUtils.defineLazyServiceGetter(this, "gSmsRequestManager",
                                   "@mozilla.org/sms/smsrequestmanager;1",
                                   "nsISmsRequestManager");

XPCOMUtils.defineLazyServiceGetter(this, "gSmsDatabaseService",
                                   "@mozilla.org/sms/rilsmsdatabaseservice;1",
                                   "nsISmsDatabaseService");

function convertRILCallState(state) {
  switch (state) {
    case RIL.CALL_STATE_ACTIVE:
      return nsIRadioInterfaceLayer.CALL_STATE_CONNECTED;
    case RIL.CALL_STATE_HOLDING:
      return nsIRadioInterfaceLayer.CALL_STATE_HELD;
    case RIL.CALL_STATE_DIALING:
      return nsIRadioInterfaceLayer.CALL_STATE_DIALING;
    case RIL.CALL_STATE_ALERTING:
      return nsIRadioInterfaceLayer.CALL_STATE_ALERTING;
    case RIL.CALL_STATE_INCOMING:
    case RIL.CALL_STATE_WAITING:
      return nsIRadioInterfaceLayer.CALL_STATE_INCOMING; 
    default:
      throw new Error("Unknown rilCallState: " + state);
  }
}

/**
 * Fake nsIAudioManager implementation so that we can run the telephony
 * code in a non-Gonk build.
 */
let FakeAudioManager = {
  microphoneMuted: false,
  masterVolume: 1.0,
  masterMuted: false,
  phoneState: nsIAudioManager.PHONE_STATE_CURRENT,
  _forceForUse: {},
  setForceForUse: function setForceForUse(usage, force) {
    this._forceForUse[usage] = force;
  },
  getForceForUse: function setForceForUse(usage) {
    return this._forceForUse[usage] || nsIAudioManager.FORCE_NONE;
  }
};

XPCOMUtils.defineLazyGetter(this, "gAudioManager", function getAudioManager() {
  try {
    return Cc["@mozilla.org/telephony/audiomanager;1"]
             .getService(nsIAudioManager);
  } catch (ex) {
    //TODO on the phone this should not fall back as silently.
    debug("Using fake audio manager.");
    return FakeAudioManager;
  }
});


function DataCallInfo(state, cid, apn) {
  this.callState = state;
  this.cid = cid;
  this.apn = apn;
}
DataCallInfo.protoptype = {
  classID:      DATACALLINFO_CID,
  classInfo:    XPCOMUtils.generateCI({classID: DATACALLINFO_CID,
                                       classDescription: "DataCallInfo",
                                       interfaces: [Ci.nsIDataCallInfo]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallInfo]),
};


function RadioInterfaceLayer() {
  this.worker = new ChromeWorker("resource://gre/modules/ril_worker.js");
  this.worker.onerror = this.onerror.bind(this);
  this.worker.onmessage = this.onmessage.bind(this);
  debug("Starting Worker\n");
  this.radioState = {
    radioState:     RIL.GECKO_RADIOSTATE_UNAVAILABLE,
    cardState:      RIL.GECKO_CARDSTATE_UNAVAILABLE,
    connected:      null,
    roaming:        null,
    signalStrength: null,
    bars:           null,
    operator:       null,
    type:           null,
    msisdn:         null,
  };
  this._sentSmsEnvelopes = {};
}
RadioInterfaceLayer.prototype = {

  classID:   RADIOINTERFACELAYER_CID,
  classInfo: XPCOMUtils.generateCI({classID: RADIOINTERFACELAYER_CID,
                                    classDescription: "RadioInterfaceLayer",
                                    interfaces: [Ci.nsIWorkerHolder,
                                                 Ci.nsIRadioInterfaceLayer]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWorkerHolder,
                                         Ci.nsIRadioInterfaceLayer]),

  onerror: function onerror(event) {
    debug("Got an error: " + event.filename + ":" +
          event.lineno + ": " + event.message + "\n");
    event.preventDefault();
  },

  /**
   * Process the incoming message from the RIL worker:
   * (1) Update the current state. This way any component that hasn't
   *     been listening for callbacks can easily catch up by looking at
   *     this.radioState.
   * (2) Update state in related systems such as the audio.
   * (3) Multiplex the message to telephone callbacks.
   */
  onmessage: function onmessage(event) {
    let message = event.data;
    debug("Received message: " + JSON.stringify(message));
    switch (message.type) {
      case "callStateChange":
        // This one will handle its own notifications.
        this.handleCallStateChange(message.call);
        break;
      case "callDisconnected":
        // This one will handle its own notifications.
        this.handleCallDisconnected(message.call);
        break;
      case "enumerateCalls":
        // This one will handle its own notifications.
        this.handleEnumerateCalls(message.calls);
        break;
      case "voiceregistrationstatechange":
        this.updateDataConnection(message.voiceRegistrationState);
        break;
      case "dataregistrationstatechange":
        let state = message.dataRegistrationState;
        this.updateDataConnection(state);

        //TODO for simplicity's sake, for now we only look at
        // dataRegistrationState for the radio registration state.

        if (!state || state.regState == RIL.NETWORK_CREG_STATE_UNKNOWN) {
          this.resetRadioState();
          this.notifyRadioStateChanged();
          return;
        }

        this.radioState.connected =
          (state.regState == RIL.NETWORK_CREG_STATE_REGISTERED_HOME) ||
          (state.regState == RIL.NETWORK_CREG_STATE_REGISTERED_ROAMING);
        this.radioState.roaming =
          this.radioState.connected &&
          (state.regState == RIL.NETWORK_CREG_STATE_REGISTERED_ROAMING);
        this.radioState.type = RIL.GECKO_RADIO_TECH[state.radioTech] || null;
        this.notifyRadioStateChanged();
        break;
      case "signalstrengthchange":
        //TODO GSM only?
        let signalStrength = message.signalStrength.gsmSignalStrength;
        if (signalStrength == 99) {
          signalStrength = null;
        }
        this.radioState.signalStrength = signalStrength;
        if (message.signalStrength.bars) {
          this.radioState.bars = message.signalStrength.bars;
        } else if (signalStrength != null) {
          //TODO pretty sure that the bars aren't linear, but meh...
          // Convert signal strength (0...31) to bars (0...4).
          this.radioState.bars = Math.round(signalStrength / 7.75);
        } else {
          this.radioState.bars = null;
        }
        this.notifyRadioStateChanged();
        break;
      case "operatorchange":
        this.radioState.operator = message.operator.alphaLong;
        this.notifyRadioStateChanged();
        break;
      case "radiostatechange":
        this.radioState.radioState = message.radioState;
        this.notifyRadioStateChanged();
        break;
      case "cardstatechange":
        this.radioState.cardState = message.cardState;
        if (!message.cardState || message.cardState == "absent") {
          this.resetRadioState();
        }
        this.notifyRadioStateChanged();
        break;
      case "sms-received":
        this.handleSmsReceived(message);
        return;
      case "sms-sent":
        this.handleSmsSent(message);
        return;
      case "sms-delivered":
        this.handleSmsDelivered(message);
        return;
      case "sms-send-failed":
        this.handleSmsSendFailed(message);
        return;
      case "datacallstatechange":
        this.handleDataCallState(message.datacall);
        break;
      case "datacalllist":
        this.handleDataCallList(message);
        break;
      case "nitzTime":
        // TODO bug 714349
        // Send information to time manager to decide what to do with it
        // Message contains networkTimeInSeconds, networkTimeZoneInMinutes,
        // dstFlag,localTimeStampInMS
        // indicating the time, daylight savings flag, and timezone
        // sent from the network and a timestamp of when the message was received
        // so an offset can be added if/when the time is actually set.
        if (DEBUG) {
          debug("nitzTime networkTime=" + message.networkTimeInSeconds
               + " timezone=" + message.networkTimeZoneInMinutes
               + " dst=" + message.dstFlag
               + " timestamp=" + message.localTimeStampInMS);
        }
        break;
      case "siminfo":
        this.radioState.msisdn = message.msisdn;
        break;
      default:
        throw new Error("Don't know about this message type: " + message.type);
    }
  },

  _isDataEnabled: function _isDataEnabled() {
    try {
      return Services.prefs.getBoolPref("ril.data.enabled");
    } catch(ex) {
      return false;
    }
  },

  _isDataRoamingEnabled: function _isDataRoamingEnabled() {
    try {
      return Services.prefs.getBoolPref("ril.data.roaming.enabled");
    } catch(ex) {
      return false;
    }
  },

  updateDataConnection: function updateDataConnection(state) {
    if (!this._isDataEnabled()) {
      return;
    }

    let isRegistered =
      state.regState == RIL.NETWORK_CREG_STATE_REGISTERED_HOME ||
        (this._isDataRoamingEnabled() &&
         state.regState == RIL.NETWORK_CREG_STATE_REGISTERED_ROAMING);
    let haveDataConnection =
      state.radioTech != RIL.NETWORK_CREG_TECH_UNKNOWN;

    if (isRegistered && haveDataConnection) {
      debug("Radio is ready for data connection.");
      // RILNetworkInterface will ignore this if it's already connected.
      RILNetworkInterface.connect();
    }
  },

  /**
   * Track the active call and update the audio system as its state changes.
   *
   * XXX Needs some more work to support hold/resume.
   */
  _activeCall: null,
  updateCallAudioState: function updateCallAudioState() {
    if (!this._activeCall) {
      // Disable audio.
      gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
      debug("No active call, put audio system into PHONE_STATE_NORMAL.");
      return;
    }
    switch (this._activeCall.state) {
      case nsIRadioInterfaceLayer.CALL_STATE_INCOMING:
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_RINGTONE;
        debug("Incoming call, put audio system into PHONE_STATE_RINGTONE.");
        break;
      case nsIRadioInterfaceLayer.CALL_STATE_DIALING: // Fall through...
      case nsIRadioInterfaceLayer.CALL_STATE_CONNECTED:
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL;
        gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION,
                                     nsIAudioManager.FORCE_NONE);
        debug("Active call, put audio system into PHONE_STATE_IN_CALL.");
        break;
    }
  },

  /**
   * Handle call state changes by updating our current state and the audio
   * system.
   */
  handleCallStateChange: function handleCallStateChange(call) {
    debug("handleCallStateChange: " + JSON.stringify(call));
    call.state = convertRILCallState(call.state);
    if (call.state == nsIRadioInterfaceLayer.CALL_STATE_DIALING ||
        call.state == nsIRadioInterfaceLayer.CALL_STATE_ALERTING ||
        call.state == nsIRadioInterfaceLayer.CALL_STATE_CONNECTED) {
      // This is now the active call.
      this._activeCall = call;
    }
    this.updateCallAudioState();
    this._deliverCallback("callStateChanged",
                          [call.callIndex, call.state, call.number]);
  },

  /**
   * Handle call disconnects by updating our current state and the audio system.
   */
  handleCallDisconnected: function handleCallDisconnected(call) {
    debug("handleCallDisconnected: " + JSON.stringify(call));
    if (this._activeCall && this._activeCall.callIndex == call.callIndex) {
      this._activeCall = null;
    }
    this.updateCallAudioState();
    this._deliverCallback("callStateChanged",
                          [call.callIndex,
                           nsIRadioInterfaceLayer.CALL_STATE_DISCONNECTED,
                           call.number]);
  },

  /**
   * Handle calls delivered in response to a 'enumerateCalls' request.
   */
  handleEnumerateCalls: function handleEnumerateCalls(calls) {
    debug("handleEnumerateCalls: " + JSON.stringify(calls));
    let callback = this._enumerationCallbacks.shift();
    let activeCallIndex = this._activeCall ? this._activeCall.callIndex : -1;
    for (let i in calls) {
      let call = calls[i];
      let state = convertRILCallState(call.state);
      let keepGoing;
      try {
        keepGoing =
          callback.enumerateCallState(call.callIndex, state, call.number,
                                      call.callIndex == activeCallIndex);
      } catch (e) {
        debug("callback handler for 'enumerateCallState' threw an " +
              " exception: " + e);
        keepGoing = true;
      }
      if (!keepGoing) {
        break;
      }
    }
  },

  handleSmsReceived: function handleSmsReceived(message) {
    debug("handleSmsReceived: " + JSON.stringify(message));
    let id = gSmsDatabaseService.saveReceivedMessage(message.sender || null,
                                                     message.fullBody || null,
                                                     message.timestamp);
    let sms = gSmsService.createSmsMessage(id,
                                           DOM_SMS_DELIVERY_RECEIVED,
                                           message.sender || null,
                                           message.receiver || null,
                                           message.fullBody || null,
                                           message.timestamp);
    Services.obs.notifyObservers(sms, kSmsReceivedObserverTopic, null);
  },

  /**
   * Local storage for sent SMS messages.
   */
  _sentSmsEnvelopes: null,
  createSmsEnvelope: function createSmsEnvelope(options) {
    let i;
    for (i = 1; this._sentSmsEnvelopes[i]; i++) {
      // Do nothing.
    }

    debug("createSmsEnvelope: assigned " + i);
    options.envelopeId = i;
    this._sentSmsEnvelopes[i] = options;
  },

  handleSmsSent: function handleSmsSent(message) {
    debug("handleSmsSent: " + JSON.stringify(message));

    let options = this._sentSmsEnvelopes[message.envelopeId];
    if (!options) {
      return;
    }

    let timestamp = Date.now();
    let id = gSmsDatabaseService.saveSentMessage(options.number,
                                                 options.fullBody,
                                                 timestamp);
    let sms = gSmsService.createSmsMessage(id,
                                           DOM_SMS_DELIVERY_SENT,
                                           null,
                                           options.number,
                                           options.fullBody,
                                           timestamp);

    if (!options.requestStatusReport) {
      // No more used if STATUS-REPORT not requested.
      delete this._sentSmsEnvelopes[message.envelopeId];
    } else {
      options.sms = sms;
    }

    gSmsRequestManager.notifySmsSent(options.requestId, sms);
  },

  handleSmsDelivered: function handleSmsDelivered(message) {
    debug("handleSmsDelivered: " + JSON.stringify(message));

    let options = this._sentSmsEnvelopes[message.envelopeId];
    if (!options) {
      return;
    }
    delete this._sentSmsEnvelopes[message.envelopeId];

    Services.obs.notifyObservers(options.sms, kSmsDeliveredObserverTopic, null);
  },

  handleSmsSendFailed: function handleSmsSendFailed(message) {
    debug("handleSmsSendFailed: " + JSON.stringify(message));

    let options = this._sentSmsEnvelopes[message.envelopeId];
    if (!options) {
      return;
    }
    delete this._sentSmsEnvelopes[message.envelopeId];

    let error = gSmsRequestManager.UNKNOWN_ERROR;
    switch (message.error) {
      case RIL.ERROR_RADIO_NOT_AVAILABLE:
        error = gSmsRequestManager.NO_SIGNAL_ERROR;
        break;
    }

    gSmsRequestManager.notifySmsSendFailed(options.requestId, error);
  },

  /**
   * Handle data call state changes.
   */
  handleDataCallState: function handleDataCallState(datacall) {
    this._deliverDataCallCallback("dataCallStateChanged",
                                  [datacall.cid, datacall.ifname, datacall.state]);
  },

  /**
   * Handle data call list.
   */
  handleDataCallList: function handleDataCallList(message) {
    let datacalls = [];
    for each (let datacall in message.datacalls) {
      datacalls.push(new DataCallInfo(datacall.state,
                                      datacall.cid,
                                      datacall.apn));
    }
    this._deliverDataCallCallback("receiveDataCallList",
                                  [datacalls, datacalls.length]);
  },

  resetRadioState: function resetRadioState() {
    this.radioState.connected = null;
    this.radioState.roaming = null;
    this.radioState.signalStrength = null;
    this.radioState.bars = null;
    this.radioState.operator = null;
    this.radioState.type = null;
  },

  notifyRadioStateChanged: function notifyRadioStateChanged() {
    debug("Radio state changed: " + JSON.stringify(this.radioState));
    Services.obs.notifyObservers(null, "ril-radiostate-changed", null);
  },

  // nsIRadioWorker

  worker: null,

  // nsIRadioInterfaceLayer

  radioState: null,

  dial: function dial(number) {
    debug("Dialing " + number);
    this.worker.postMessage({type: "dial", number: number});
  },

  hangUp: function hangUp(callIndex) {
    debug("Hanging up call no. " + callIndex);
    this.worker.postMessage({type: "hangUp", callIndex: callIndex});
  },

  startTone: function startTone(dtmfChar) {
    debug("Sending Tone for " + dtmfChar);
    this.worker.postMessage({type: "startTone", dtmfChar: dtmfChar});
  },

  stopTone: function stopTone() {
    debug("Stopping Tone");
    this.worker.postMessage({type: "stopTone"});
  },

  answerCall: function answerCall(callIndex) {
    this.worker.postMessage({type: "answerCall", callIndex: callIndex});
  },

  rejectCall: function rejectCall(callIndex) {
    this.worker.postMessage({type: "rejectCall", callIndex: callIndex});
  },
 
  holdCall: function holdCall(callIndex) {
    this.worker.postMessage({type: "holdCall", callIndex: callIndex});
  },

  resumeCall: function resumeCall(callIndex) {
    this.worker.postMessage({type: "resumeCall", callIndex: callIndex});
  },

  get microphoneMuted() {
    return gAudioManager.microphoneMuted;
  },
  set microphoneMuted(value) {
    if (value == this.microphoneMuted) {
      return;
    }
    gAudioManager.phoneState = value ?
      nsIAudioManager.PHONE_STATE_IN_COMMUNICATION :
      nsIAudioManager.PHONE_STATE_IN_CALL;  //XXX why is this needed?
    gAudioManager.microphoneMuted = value;
  },

  get speakerEnabled() {
    return (gAudioManager.getForceForUse(nsIAudioManager.USE_COMMUNICATION) ==
            nsIAudioManager.FORCE_SPEAKER);
  },
  set speakerEnabled(value) {
    if (value == this.speakerEnabled) {
      return;
    }
    gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL; // XXX why is this needed?
    let force = value ? nsIAudioManager.FORCE_SPEAKER :
                        nsIAudioManager.FORCE_NONE;
    gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION, force);
  },

  /**
   * List of tuples of national language identifier pairs.
   *
   * TODO: Support static/runtime settings, see bug 733331.
   */
  enabledGsmTableTuples: [
    [RIL.PDU_NL_IDENTIFIER_DEFAULT, RIL.PDU_NL_IDENTIFIER_DEFAULT],
  ],

  /**
   * Use 16-bit reference number for concatenated outgoint messages.
   *
   * TODO: Support static/runtime settings, see bug 733331.
   */
  segmentRef16Bit: false,

  /**
   * Get valid SMS concatenation reference number.
   */
  _segmentRef: 0,
  get nextSegmentRef() {
    let ref = this._segmentRef++;

    this._segmentRef %= (this.segmentRef16Bit ? 65535 : 255);

    // 0 is not a valid SMS concatenation reference number.
    return ref + 1;
  },

  /**
   * Calculate encoded length using specified locking/single shift table
   *
   * @param message
   *        message string to be encoded.
   * @param langTable
   *        locking shift table string.
   * @param langShiftTable
   *        single shift table string.
   *
   * @return encoded length in septets.
   *
   * @note that the algorithm used in this function must match exactly with
   * GsmPDUHelper#writeStringAsSeptets.
   */
  _countGsm7BitSeptets: function _countGsm7BitSeptets(message, langTable, langShiftTable) {
    let length = 0;
    for (let msgIndex = 0; msgIndex < message.length; msgIndex++) {
      let septet = langTable.indexOf(message.charAt(msgIndex));

      // According to 3GPP TS 23.038, section 6.1.1 General notes, "The
      // characters marked '1)' are not used but are displayed as a space."
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        length++;
        continue;
      }

      septet = langShiftTable.indexOf(message.charAt(msgIndex));
      if (septet < 0) {
        return -1;
      }

      // According to 3GPP TS 23.038 B.2, "This code represents a control
      // character and therefore must not be used for language specific
      // characters."
      if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
        continue;
      }

      // The character is not found in locking shfit table, but could be
      // encoded as <escape><char> with single shift table. Note that it's
      // still possible for septet to has the value of PDU_NL_EXTENDED_ESCAPE,
      // but we can display it as a space in this case as said in previous
      // comment.
      length += 2;
    }

    return length;
  },

  /**
   * Calculate user data length of specified message string encoded in GSM 7Bit
   * alphabets.
   *
   * @param message
   *        a message string to be encoded.
   *
   * @return null or an options object with attributes `dcs`,
   *         `userDataHeaderLength`, `encodedFullBodyLength`, `langIndex`,
   *         `langShiftIndex`, `segmentMaxSeq` set.
   *
   * @see #_calculateUserDataLength().
   */
  _calculateUserDataLength7Bit: function _calculateUserDataLength7Bit(message) {
    let options = null;
    let minUserDataSeptets = Number.MAX_VALUE;
    for (let i = 0; i < this.enabledGsmTableTuples.length; i++) {
      let [langIndex, langShiftIndex] = this.enabledGsmTableTuples[i];

      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

      let bodySeptets = this._countGsm7BitSeptets(message,
                                                  langTable,
                                                  langShiftTable);
      if (bodySeptets < 0) {
        continue;
      }

      let headerLen = 0;
      if (langIndex != RIL.PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langIndex
      }
      if (langShiftIndex != RIL.PDU_NL_IDENTIFIER_DEFAULT) {
        headerLen += 3; // IEI + len + langShiftIndex
      }

      // Calculate full user data length, note the extra byte is for header len
      let headerSeptets = Math.ceil((headerLen ? headerLen + 1 : 0) * 8 / 7);
      let userDataSeptets = bodySeptets + headerSeptets;
      let segments = bodySeptets ? 1 : 0;
      if (userDataSeptets > RIL.PDU_MAX_USER_DATA_7BIT) {
        if (this.segmentRef16Bit) {
          headerLen += 6;
        } else {
          headerLen += 5;
        }

        headerSeptets = Math.ceil((headerLen + 1) * 8 / 7);
        let segmentSeptets = RIL.PDU_MAX_USER_DATA_7BIT - headerSeptets;
        segments = Math.ceil(bodySeptets / segmentSeptets);
        userDataSeptets = bodySeptets + headerSeptets * segments;
      }

      if (userDataSeptets >= minUserDataSeptets) {
        continue;
      }

      minUserDataSeptets = userDataSeptets;

      options = {
        dcs: RIL.PDU_DCS_MSG_CODING_7BITS_ALPHABET,
        encodedFullBodyLength: bodySeptets,
        userDataHeaderLength: headerLen,
        langIndex: langIndex,
        langShiftIndex: langShiftIndex,
        segmentMaxSeq: segments,
      };
    }

    return options;
  },

  /**
   * Calculate user data length of specified message string encoded in UCS2.
   *
   * @param message
   *        a message string to be encoded.
   *
   * @return an options object with attributes `dcs`, `userDataHeaderLength`,
   *         `encodedFullBodyLength`, `segmentMaxSeq` set.
   *
   * @see #_calculateUserDataLength().
   */
  _calculateUserDataLengthUCS2: function _calculateUserDataLengthUCS2(message) {
    let bodyChars = message.length;
    let headerLen = 0;
    let headerChars = Math.ceil((headerLen ? headerLen + 1 : 0) / 2);
    let segments = bodyChars ? 1 : 0;
    if ((bodyChars + headerChars) > RIL.PDU_MAX_USER_DATA_UCS2) {
      if (this.segmentRef16Bit) {
        headerLen += 6;
      } else {
        headerLen += 5;
      }

      headerChars = Math.ceil((headerLen + 1) / 2);
      let segmentChars = RIL.PDU_MAX_USER_DATA_UCS2 - headerChars;
      segments = Math.ceil(bodyChars / segmentChars);
    }

    return {
      dcs: RIL.PDU_DCS_MSG_CODING_16BITS_ALPHABET,
      encodedFullBodyLength: bodyChars * 2,
      userDataHeaderLength: headerLen,
      segmentMaxSeq: segments,
    };
  },

  /**
   * Calculate user data length and its encoding.
   *
   * @param message
   *        a message string to be encoded.
   *
   * @return an options object with some or all of following attributes set:
   *
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param fullBody
   *        Original unfragmented text message.
   * @param userDataHeaderLength
   *        Length of embedded user data header, in bytes. The whole header
   *        size will be userDataHeaderLength + 1; 0 for no header.
   * @param encodedFullBodyLength
   *        Length of the message body when encoded with the given DCS. For
   *        UCS2, in bytes; for 7-bit, in septets.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   * @param segmentMaxSeq
   *        Max sequence number of a multi-part messages, or 1 for single one.
   *        This number might not be accurate for a multi-part message until
   *        it's processed by #_fragmentText() again.
   */
  _calculateUserDataLength: function _calculateUserDataLength(message) {
    let options = this._calculateUserDataLength7Bit(message);
    if (!options) {
      options = this._calculateUserDataLengthUCS2(message);
    }

    if (options) {
      options.fullBody = message;
    }

    debug("_calculateUserDataLength: " + JSON.stringify(options));
    return options;
  },

  /**
   * Fragment GSM 7-Bit encodable string for transmission.
   *
   * @param text
   *        text string to be fragmented.
   * @param langTable
   *        locking shift table string.
   * @param langShiftTable
   *        single shift table string.
   * @param headerLen
   *        Length of prepended user data header.
   *
   * @return an array of objects. See #_fragmentText() for detailed definition.
   */
  _fragmentText7Bit: function _fragmentText7Bit(text, langTable, langShiftTable, headerLen) {
    const headerSeptets = Math.ceil((headerLen ? headerLen + 1 : 0) * 8 / 7);
    const segmentSeptets = RIL.PDU_MAX_USER_DATA_7BIT - headerSeptets;
    let ret = [];
    let begin = 0, len = 0;
    for (let i = 0, inc = 0; i < text.length; i++) {
      let septet = langTable.indexOf(text.charAt(i));
      if (septet == RIL.PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        inc = 1;
      } else {
        septet = langShiftTable.indexOf(text.charAt(i));
        if (septet < 0) {
          throw new Error("Given text cannot be encoded with GSM 7-bit Alphabet!");
        }

        if (septet == RIL.PDU_NL_RESERVED_CONTROL) {
          continue;
        }

        inc = 2;
      }

      if ((len + inc) > segmentSeptets) {
        ret.push({
          body: text.substring(begin, i),
          encodedBodyLength: len,
        });
        begin = i;
        len = 0;
      }

      len += inc;
    }

    if (len) {
      ret.push({
        body: text.substring(begin),
        encodedBodyLength: len,
      });
    }

    return ret;
  },

  /**
   * Fragment UCS2 encodable string for transmission.
   *
   * @param text
   *        text string to be fragmented.
   * @param headerLen
   *        Length of prepended user data header.
   *
   * @return an array of objects. See #_fragmentText() for detailed definition.
   */
  _fragmentTextUCS2: function _fragmentTextUCS2(text, headerLen) {
    const headerChars = Math.ceil((headerLen ? headerLen + 1 : 0) / 2);
    const segmentChars = RIL.PDU_MAX_USER_DATA_UCS2 - headerChars;
    let ret = [];
    for (let offset = 0; offset < text.length; offset += segmentChars) {
      let str = text.substr(offset, segmentChars);
      ret.push({
        body: str,
        encodedBodyLength: str.length * 2,
      });
    }

    return ret;
  },

  /**
   * Fragment string for transmission.
   *
   * Fragment input text string into an array of objects that contains
   * attributes `body`, substring for this segment, `encodedBodyLength`,
   * length of the encoded segment body in septets.
   *
   * @param text
   *        Text string to be fragmented.
   * @param options
   *        Optional pre-calculated option object. The output array will be
   *        stored at options.segments if there are multiple segments.
   *
   * @return Populated options object.
   */
  _fragmentText: function _fragmentText(text, options) {
    if (!options) {
      options = this._calculateUserDataLength(text);
    }

    if (options.segmentMaxSeq <= 1) {
      options.segments = null;
      return options;
    }

    if (options.dcs == RIL.PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      const langTable = RIL.PDU_NL_LOCKING_SHIFT_TABLES[options.langIndex];
      const langShiftTable = RIL.PDU_NL_SINGLE_SHIFT_TABLES[options.langShiftIndex];
      options.segments = this._fragmentText7Bit(options.fullBody,
                                                langTable, langShiftTable,
                                                options.userDataHeaderLength);
    } else {
      options.segments = this._fragmentTextUCS2(options.fullBody,
                                                options.userDataHeaderLength);
    }

    // Re-sync options.segmentMaxSeq with actual length of returning array.
    options.segmentMaxSeq = options.segments.length;

    return options;
  },

  getNumberOfMessagesForText: function getNumberOfMessagesForText(text) {
    return this._fragmentText(text).segmentMaxSeq;
  },

  sendSMS: function sendSMS(number, message, requestId, processId) {
    let options = this._calculateUserDataLength(message);
    options.type = "sendSMS";
    options.number = number;
    options.requestId = requestId;
    options.processId = processId;
    options.requestStatusReport = true;

    this._fragmentText(message, options);
    if (options.segmentMaxSeq > 1) {
      options.segmentRef16Bit = this.segmentRef16Bit;
      options.segmentRef = this.nextSegmentRef;
    }

    // Keep current SMS message info for sent/delivered notifications
    this.createSmsEnvelope(options);

    this.worker.postMessage(options);
  },

  _callbacks: null,
  _enumerationCallbacks: null,

  registerCallback: function registerCallback(callback) {
    if (this._callbacks) {
      if (this._callbacks.indexOf(callback) != -1) {
        throw new Error("Already registered this callback!");
      }
    } else {
      this._callbacks = [];
    }
    this._callbacks.push(callback);
    debug("Registered callback: " + callback);
  },

  unregisterCallback: function unregisterCallback(callback) {
    if (!this._callbacks) {
      return;
    }
    let index = this._callbacks.indexOf(callback);
    if (index != -1) {
      this._callbacks.splice(index, 1);
      debug("Unregistered callback: " + callback);
    }
  },

  enumerateCalls: function enumerateCalls(callback) {
    debug("Requesting enumeration of calls for callback: " + callback);
    this.worker.postMessage({type: "enumerateCalls"});
    if (!this._enumerationCallbacks) {
      this._enumerationCallbacks = [];
    }
    this._enumerationCallbacks.push(callback);
  },

  _deliverCallback: function _deliverCallback(name, args) {
    // We need to worry about callback registration state mutations during the
    // callback firing. The behaviour we want is to *not* call any callbacks
    // that are added during the firing and to *not* call any callbacks that are
    // removed during the firing. To address this, we make a copy of the
    // callback list before dispatching and then double-check that each callback
    // is still registered before calling it.
    if (!this._callbacks) {
      return;
    }
    let callbacks = this._callbacks.slice();
    for each (let callback in callbacks) {
      if (this._callbacks.indexOf(callback) == -1) {
        continue;
      }
      let handler = callback[name];
      if (typeof handler != "function") {
        throw new Error("No handler for " + name);
      }
      try {
        handler.apply(callback, args);
      } catch (e) {
        debug("callback handler for " + name + " threw an exception: " + e);
      }
    }
  },

  registerDataCallCallback: function registerDataCallCallback(callback) {
    if (this._datacall_callbacks) {
      if (this._datacall_callbacks.indexOf(callback) != -1) {
        throw new Error("Already registered this callback!");
      }
    } else {
      this._datacall_callbacks = [];
    }
    this._datacall_callbacks.push(callback);
    debug("Registering callback: " + callback);
  },

  unregisterDataCallCallback: function unregisterDataCallCallback(callback) {
    if (!this._datacall_callbacks) {
      return;
    }
    let index = this._datacall_callbacks.indexOf(callback);
    if (index != -1) {
      this._datacall_callbacks.splice(index, 1);
      debug("Unregistering callback: " + callback);
    }
  },

  _deliverDataCallCallback: function _deliverDataCallCallback(name, args) {
    // We need to worry about callback registration state mutations during the
    // callback firing. The behaviour we want is to *not* call any callbacks
    // that are added during the firing and to *not* call any callbacks that are
    // removed during the firing. To address this, we make a copy of the
    // callback list before dispatching and then double-check that each callback
    // is still registered before calling it.
    if (!this._datacall_callbacks) {
      return;
    }
    let callbacks = this._datacall_callbacks.slice();
    for each (let callback in callbacks) {
      if (this._datacall_callbacks.indexOf(callback) == -1) {
        continue;
      }
      let handler = callback[name];
      if (typeof handler != "function") {
        throw new Error("No handler for " + name);
      }
      try {
        handler.apply(callback, args);
      } catch (e) {
        debug("callback handler for " + name + " threw an exception: " + e);
      }
    }
  },

  setupDataCall: function setupDataCall(radioTech, apn, user, passwd, chappap, pdptype) {
    this.worker.postMessage({type: "setupDataCall",
                             radioTech: radioTech,
                             apn: apn,
                             user: user,
                             passwd: passwd,
                             chappap: chappap,
                             pdptype: pdptype});
  },

  deactivateDataCall: function deactivateDataCall(cid, reason) {
    this.worker.postMessage({type: "deactivateDataCall",
                             cid: cid,
                             reason: reason});
  },

  getDataCallList: function getDataCallList() {
    this.worker.postMessage({type: "getDataCallList"});
  },

};


let RILNetworkInterface = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRILDataCallback]),

  state: RIL.GECKO_NETWORK_STATE_UNKNOWN,
  name: null,

  worker: null,
  cid: null,
  registeredAsDataCallCallback: false,
  connecting: false,

  initWorker: function initWorker() {
    debug("Starting net_worker.");
    this.worker = new ChromeWorker("resource://gre/modules/net_worker.js");
    this.worker.onerror = function onerror(event) {
      debug("Received error from worker: " + event.filename +
            ":" + event.lineno + ": " + event.message + "\n");
      // Prevent the event from bubbling any further.
      event.preventDefault();
    };
  },

  // nsIRILDataCallback

  dataCallStateChanged: function dataCallStateChanged(cid, interfaceName, callState) {
    if (this.connecting &&
        (callState == RIL.GECKO_NETWORK_STATE_CONNECTING ||
         callState == RIL.GECKO_NETWORK_STATE_CONNECTED)) {
      this.connecting = false;
      this.cid = cid;
      this.name = interfaceName;
      debug("Data call ID: " + cid + ", interface name: " + interfaceName);
    }
    if (this.cid != cid) {
      return;
    }
    if (this.state == callState) {
      return;
    }

    this.state = callState;

    if (callState == RIL.GECKO_NETWORK_STATE_CONNECTED) {
      debug("Data call is connected, going to configure networking bits.");
      this.worker.postMessage({cmd: "setDefaultRouteAndDNS",
                               ifname: this.name});
    }
  },

  receiveDataCallList: function receiveDataCallList(dataCalls, length) {
  },

  // Helpers

  get mRIL() {
    delete this.mRIL;
    return this.mRIL = Cc["@mozilla.org/telephony/system-worker-manager;1"]
                         .getService(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIRadioInterfaceLayer);
  },

  connect: function connect() {
    if (this.connecting ||
        this.state == RIL.GECKO_NETWORK_STATE_CONNECTED ||
        this.state == RIL.GECKO_NETWORK_STATE_SUSPENDED ||
        this.state == RIL.GECKO_NETWORK_STATE_DISCONNECTING) {
      return;
    }
    if (!this.registeredAsDataCallCallback) {
      this.mRIL.registerDataCallCallback(this);
      this.registeredAsDataCallCallback = true;
    }

    if (!this.worker) {
      this.initWorker();
    }

    let apn, user, passwd;
    // Eventually these values would be retrieved from the user's preferences
    // via the settings API. For now we just use Gecko's preferences.
    try {
      apn = Services.prefs.getCharPref("ril.data.apn");
      user = Services.prefs.getCharPref("ril.data.user");
      passwd = Services.prefs.getCharPref("ril.data.passwd");
    } catch (ex) {
      debug("No APN settings found, not going to set up data connection.");
      return;
    }
    debug("Going to set up data connection with APN " + apn);
    this.mRIL.setupDataCall(RIL.DATACALL_RADIOTECHNOLOGY_GSM,
                            apn, user, passwd,
                            RIL.DATACALL_AUTH_PAP_OR_CHAP, "IP");
    this.connecting = true;
  },

  disconnect: function disconnect() {
    this.mRIL.deactivateDataCall(this.cid);
  },

};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([RadioInterfaceLayer]);

let debug;
if (DEBUG) {
  debug = function (s) {
    dump("-*- RadioInterfaceLayer: " + s + "\n");
  };
} else {
  debug = function (s) {};
}
