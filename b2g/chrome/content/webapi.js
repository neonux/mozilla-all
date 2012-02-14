/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

dump('======================= webapi+apps.js ======================= \n');

'use strict';

let { classes: Cc, interfaces: Ci, utils: Cu }  = Components;
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

XPCOMUtils.defineLazyGetter(Services, 'fm', function() {
  return Cc['@mozilla.org/focus-manager;1']
           .getService(Ci.nsIFocusManager);
});

(function() {
  function generateAPI(window) {
    let navigator = window.navigator;

    XPCOMUtils.defineLazyGetter(navigator, 'mozKeyboard', function() {
      return new MozKeyboard();
    });
  };

  let progressListener = {
    onStateChange: function onStateChange(progress, request,
                                          flags, status) {
    },

    onProgressChange: function onProgressChange(progress, request,
                                                curSelf, maxSelf,
                                                curTotal, maxTotal) {
    },

    onLocationChange: function onLocationChange(progress, request,
                                                locationURI, flags) {
      content.addEventListener('appwillopen', function(evt) {
        let appManager = content.wrappedJSObject.Gaia.AppManager;
        let topWindow = appManager.foregroundWindow.contentWindow;
        generateAPI(topWindow);
      });

      generateAPI(content.wrappedJSObject);
    },

    onStatusChange: function onStatusChange(progress, request,
                                            status, message) {
    },

    onSecurityChange: function onSecurityChange(progress, request,
                                                state) {
    },

    QueryInterface: function QueryInterface(aIID) {
      if (aIID.equals(Ci.nsIWebProgressListener) ||
          aIID.equals(Ci.nsISupportsWeakReference) ||
          aIID.equals(Ci.nsISupports)) {
          return this;
      }

      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };

  let flags = Ci.nsIWebProgress.NOTIFY_LOCATION;
  let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebProgress);
  flags = Ci.nsIWebProgress.NOTIFY_ALL;
  webProgress.addProgressListener(progressListener, flags);
})();

// MozKeyboard
(function VirtualKeyboardManager() {
  let activeElement = null;
  let isKeyboardOpened = false;
  
  function fireEvent(type, details) {
    let event = content.document.createEvent('CustomEvent');
    event.initCustomEvent(type, true, true, details ? details : {});
    content.dispatchEvent(event);
  }

  let constructor = {
    handleEvent: function vkm_handleEvent(evt) {
      switch (evt.type) {
        case 'keypress':
          if (evt.keyCode != evt.DOM_VK_ESCAPE || !isKeyboardOpened)
            return;

          fireEvent('hideime');
          isKeyboardOpened = false;

          evt.preventDefault();
          evt.stopPropagation();
          break;

        case 'mousedown':
          if (evt.target != activeElement || isKeyboardOpened)
            return;

          let type = activeElement.type;
          fireEvent('showime', { type: type });
          isKeyboardOpened = true;
          break;
      }
    },
    observe: function vkm_observe(subject, topic, data) {
      let shouldOpen = parseInt(data);
      if (shouldOpen && !isKeyboardOpened) {
        activeElement = Services.fm.focusedElement;
        if (!activeElement)
          return;

        let type = activeElement.type;
        fireEvent('showime', { type: type });
      } else if (!shouldOpen && isKeyboardOpened) {
        fireEvent('hideime');
      }
      isKeyboardOpened = shouldOpen;
    }
  };

  Services.obs.addObserver(constructor, 'ime-enabled-state-changed', false);
  ['keypress', 'mousedown'].forEach(function vkm_events(type) {
    addEventListener(type, constructor, true);
  });
})();


function MozKeyboard() {
}

MozKeyboard.prototype = {
  sendKey: function mozKeyboardSendKey(keyCode, charCode) {
    charCode = (charCode == undefined) ? keyCode : charCode;

    let utils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);
    ['keydown', 'keypress', 'keyup'].forEach(function sendKey(type) {
      utils.sendKeyEvent(type, keyCode, charCode, null);
    });
  }
};

let { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/Geometry.jsm');
Cu.import('resource://gre/modules/Services.jsm');

const ContentPanning = {
  init: function cp_init() {
    ['mousedown', 'mouseup', 'mousemove'].forEach(function(type) {
      addEventListener(type, ContentPanning, true);
    });
  },

  handleEvent: function cp_handleEvent(evt) {
    switch (evt.type) {
      case 'mousedown':
        this.onTouchStart(evt);
        break;
      case 'mousemove':
        this.onTouchMove(evt);
        break;
      case 'mouseup':
        this.onTouchEnd(evt);
        break;
      case 'click':
        evt.stopPropagation();
        evt.preventDefault();
        evt.target.removeEventListener('click', this, true);
        break;
    }
  },

  position: {
    origin: new Point(0, 0),
    current: new Point(0 , 0)
  },

  onTouchStart: function cp_onTouchStart(evt) {
    this.dragging = true;
    KineticPanning.stop();

    this.scrollCallback = this.getPannable(evt.originalTarget);
    this.position.origin.set(evt.screenX, evt.screenY);
    this.position.current.set(evt.screenX, evt.screenY);
    KineticPanning.record(new Point(0, 0));
  },

  onTouchEnd: function cp_onTouchEnd(evt) {
    if (!this.dragging)
      return;
    this.dragging = false;

    if (this.isPan()) {
      if (evt.detail) // The event will generate a click
        evt.target.addEventListener('click', this, true);

      KineticPanning.start(this);
    }
  },

  onTouchMove: function cp_onTouchMove(evt) {
    if (!this.dragging || !this.scrollCallback)
      return;

    let current = this.position.current;
    let delta = new Point(evt.screenX - current.x, evt.screenY - current.y);
    current.set(evt.screenX, evt.screenY);

    if (this.isPan()) {
      KineticPanning.record(delta);
      this.scrollCallback(delta.scale(-1));
    }
  },


  onKineticBegin: function cp_onKineticBegin(evt) {
  },

  onKineticPan: function cp_onKineticPan(delta) {
    return !this.scrollCallback(delta);
  },

  onKineticEnd: function cp_onKineticEnd() {
    if (!this.dragging)
      this.scrollCallback = null;
  },

  isPan: function cp_isPan() {
    let dpi = content.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils)
                     .displayDPI;

    let threshold = Services.prefs.getIntPref('ui.dragThresholdX') / 240 * dpi;

    let deltaX = this.position.origin.x - this.position.current.x;
    let deltaY = this.position.origin.y - this.position.current.y;
    return (Math.abs(deltaX) > threshold || Math.abs(deltaY) > threshold);
  },

  getPannable: function cp_getPannable(node) {
    if (!(node instanceof Ci.nsIDOMHTMLElement) || node.tagName == 'HTML')
      return null;

    let content = node.ownerDocument.defaultView;

    while (!(node instanceof Ci.nsIDOMHTMLBodyElement)) {
      let style = content.getComputedStyle(node, null);

      let overflow = [style.getPropertyValue('overflow'),
                      style.getPropertyValue('overflow-x'),
                      style.getPropertyValue('overflow-y')];

      let rect = node.getBoundingClientRect();
      let isAuto = (overflow.indexOf('auto') != -1 &&
                   (rect.height < node.scrollHeight ||
                    rect.width < node.scrollWidth));

      let isScroll = (overflow.indexOf('scroll') != -1);
      if (isScroll || isAuto)
        return this._generateCallback(node);

      node = node.parentNode;
    }

    return this._generateCallback(content);
  },

  _generateCallback: function cp_generateCallback(content) {
    function scroll(delta) {
      if (content instanceof Ci.nsIDOMHTMLElement) {
        let oldX = content.scrollLeft, oldY = content.scrollTop;
        content.scrollLeft += delta.x;
        content.scrollTop += delta.y;
        let newX = content.scrollLeft, newY = content.scrollTop;
        return (newX != oldX) || (newY != oldY);
      } else {
        let oldX = content.scrollX, oldY = content.scrollY;
        content.scrollBy(delta.x, delta.y);
        let newX = content.scrollX, newY = content.scrollY;
        return (newX != oldX) || (newY != oldY);
      }
    }
    return scroll;
  }
};

ContentPanning.init();


// Min/max velocity of kinetic panning. This is in pixels/millisecond.
const kMinVelocity = 0.4;
const kMaxVelocity = 6;

// Constants that affect the "friction" of the scroll pane.
const kExponentialC = 1400;
const kPolynomialC = 100 / 1000000;

// How often do we change the position of the scroll pane?
// Too often and panning may jerk near the end.
// Too little and panning will be choppy. In milliseconds.
const kUpdateInterval = 16;

const KineticPanning = {
  _position: new Point(0, 0),
  _velocity: new Point(0, 0),
  _acceleration: new Point(0, 0),

  _target: null,
  start: function kp_start(target) {
    this.target = target;

    // Calculate the initial velocity of the movement based on user input
    let momentums = this.momentums;

    let distance = new Point(0, 0);
    momentums.forEach(function(momentum) {
      distance.add(momentum.dx, momentum.dy);
    });

    let elapsed = momentums[momentums.length - 1].time - momentums[0].time;

    function clampFromZero(x, min, max) {
      if (x >= 0)
        return Math.max(min, Math.min(max, x));
      return Math.min(-min, Math.max(-max, x));
    }

    let velocityX = clampFromZero(distance.x / elapsed, 0, kMaxVelocity);
    let velocityY = clampFromZero(distance.y / elapsed, 0, kMaxVelocity);

    let velocity = this._velocity;
    velocity.set(Math.abs(velocityX) < kMinVelocity ? 0 : velocityX,
                 Math.abs(velocityY) < kMinVelocity ? 0 : velocityY);

    // Set acceleration vector to opposite signs of velocity
    function sign(x) {
      return x ? (x > 0 ? 1 : -1) : 0;
    }

    this._acceleration.set(velocity.clone().map(sign).scale(-kPolynomialC));

    // Reset the position
    this._position.set(0, 0);

    this._startAnimation();

    this.target.onKineticBegin();
  },

  stop: function kp_stop() {
    if (!this.target)
      return;

    this.momentums.splice(0);

    this.target.onKineticEnd();
    this.target = null;
  },

  momentums: [],
  record: function kp_record(delta) {
    // If the panning direction has changed, stop the current activity.
    if (this.target && ((delta.x * this._velocity.x < 0) ||
                        (delta.y * this._velocity.y < 0)))
      this.stop();

    this.momentums.push({ 'time': Date.now(), 'dx' : delta.x, 'dy' : delta.y });
  },

  _startAnimation: function kp_startAnimation() {
    let c = kExponentialC;
    function getNextPosition(position, v, a, t) {
      // Important traits for this function:
      //   p(t=0) is 0
      //   p'(t=0) is v0
      //
      // We use exponential to get a smoother stop, but by itself exponential
      // is too smooth at the end. Adding a polynomial with the appropriate
      // weight helps to balance
      position.set(v.x * Math.exp(-t / c) * -c + a.x * t * t + v.x * c,
                   v.y * Math.exp(-t / c) * -c + a.y * t * t + v.y * c);
    }

    let startTime = content.mozAnimationStartTime;
    let elapsedTime = 0, targetedTime = 0, averageTime = 0;

    let velocity = this._velocity;
    let acceleration = this._acceleration;

    let position = this._position;
    let nextPosition = new Point(0, 0);
    let delta = new Point(0, 0);

    let callback = (function(timestamp) {
      if (!this.target)
        return;

      // To make animation end fast enough but to keep smoothness, average the
      // ideal time frame (smooth animation) with the actual time lapse
      // (end fast enough).
      // Animation will never take longer than 2 times the ideal length of time.
      elapsedTime = timestamp - startTime;
      targetedTime += kUpdateInterval;
      averageTime = (targetedTime + elapsedTime) / 2;

      // Calculate new position.
      getNextPosition(nextPosition, velocity, acceleration, averageTime);
      delta.set(Math.round(nextPosition.x - position.x),
                Math.round(nextPosition.y - position.y));

      // Test to see if movement is finished for each component.
      if (delta.x * acceleration.x > 0)
        delta.x = position.x = velocity.x = acceleration.x = 0;

      if (delta.y * acceleration.y > 0)
        delta.y = position.y = velocity.y = acceleration.y = 0;

      if (velocity.equals(0, 0) || delta.equals(0, 0)) {
        this.stop();
        return;
      }

      position.add(delta);
      if (this.target.onKineticPan(delta.scale(-1))) {
        this.stop();
        return;
      }

      content.mozRequestAnimationFrame(callback);
    }).bind(this);

    content.mozRequestAnimationFrame(callback);
  }
};

