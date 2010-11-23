// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Vivien Nicolas <vnicolas@mozilla.com>
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

let Ci = Components.interfaces;
let Cc = Components.classes;

dump("###################################### forms.js loaded\n");

let HTMLTextAreaElement = Ci.nsIDOMHTMLTextAreaElement;
let HTMLInputElement = Ci.nsIDOMHTMLInputElement;
let HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
let HTMLIFrameElement = Ci.nsIDOMHTMLIFrameElement;
let HTMLBodyElement = Ci.nsIDOMHTMLBodyElement;
let HTMLLabelElement = Ci.nsIDOMHTMLLabelElement;
let HTMLButtonElement = Ci.nsIDOMHTMLButtonElement;
let HTMLOptGroupElement = Ci.nsIDOMHTMLOptGroupElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;
let XULMenuListElement = Ci.nsIDOMXULMenuListElement;

/**
 * Responsible of navigation between forms fields and of the opening of the assistant
 */
function FormAssistant() {
  addMessageListener("FormAssist:Closed", this);
  addMessageListener("FormAssist:Previous", this);
  addMessageListener("FormAssist:Next", this);
  addMessageListener("FormAssist:ChoiceSelect", this);
  addMessageListener("FormAssist:ChoiceChange", this);
  addMessageListener("FormAssist:AutoComplete", this);
  addMessageListener("Content:SetWindowSize", this);

  addEventListener("keyup", this, false);
  addEventListener("focus", this, true);

  this._enabled = Services.prefs.getBoolPref("formhelper.enabled");
};

FormAssistant.prototype = {
  _selectWrapper: null,
  _currentIndex: -1,
  _elements: [],

  get currentElement() {
    return this._elements[this._currentIndex];
  },

  get currentIndex() {
    return this._currentIndex;
  },

  set currentIndex(aIndex) {
    let element = this._elements[aIndex];
    if (element) {
      gFocusManager.setFocus(element, Ci.nsIFocusManager.FLAG_NOSCROLL);
      this._currentIndex = aIndex;
      sendAsyncMessage("FormAssist:Show", this._getJSON());
    }
    return element;
  },

  _open: false,
  open: function(aElement) {
    // if the click is on an option element we want to check if the parent is a valid target
    if (aElement instanceof HTMLOptionElement && aElement.parentNode instanceof HTMLSelectElement) {
      aElement = aElement.parentNode;
    }

    // bug 526045 - the form assistant will close if a click happen:
    // * outside of the scope of the form helper
    // * hover a button of type=[image|submit]
    // * hover a disabled element
    if (!this._isValidElement(aElement)) {
      let passiveButtons = { button: true, checkbox: true, file: true, radio: true, reset: true };
      if ((aElement instanceof HTMLInputElement || aElement instanceof HTMLButtonElement) &&
          passiveButtons[aElement.type] && !aElement.disabled)
        return false;

      sendAsyncMessage("FormAssist:Hide", { });
      this._currentIndex = -1;
      this._elements = [];
      return this._open = false;
    }

    // If the element have the isContentEditable property we want to open the
    // top level element for it
    if (aElement.isContentEditable)
      aElement = this._getTopLevelEditable(aElement);

    // Checking if the element is the current focused one while the form assistant is open
    // allow the user to reposition the caret into an input element
    if (this._open && aElement == this.currentElement) {
      //hack bug 604351
      // if the element is the same editable element and the VKB is closed, reopen it
      let utils = Util.getWindowUtils(content);
      if (utils.IMEStatus == utils.IME_STATUS_DISABLED && aElement instanceof HTMLInputElement && aElement.mozIsTextField(false)) {
        aElement.blur();
        aElement.focus();
      }

      return false;
    }

    // If form assistant is disabled but the element of a type of choice list
    // we still want to show the simple select list
    this._enabled = Services.prefs.getBoolPref("formhelper.enabled");
    if (!this._enabled && !this._isSelectElement(aElement)) {
      sendAsyncMessage("FormAssist:Hide", { });
      return this._open = false;
    }

    if (this._enabled) {
      this._elements = [];
      this.currentIndex = this._getAllElements(aElement);
    }
    else {
      this._elements = [aElement];
      this.currentIndex = 0;
    }

    return this._open = true;
  },

  receiveMessage: function receiveMessage(aMessage) {
    let currentElement = this.currentElement;
    if ((!this._enabled && !getWrapperForElement(currentElement)) || !currentElement)
      return;

    let json = aMessage.json;
    switch (aMessage.name) {
      case "FormAssist:Previous":
        this.currentIndex--;
        break;

      case "FormAssist:Next":
        this.currentIndex++;
        break;

      case "Content:SetWindowSize":
        // If the CSS viewport change just show the current element to the new
        // position
        sendAsyncMessage("FormAssist:Show", this._getJSON());
        break;

      case "FormAssist:ChoiceSelect": {
        this._selectWrapper = getWrapperForElement(currentElement);
        this._selectWrapper.select(json.index, json.selected, json.clearAll);
        break;
      }

      case "FormAssist:ChoiceChange": {
        // ChoiceChange happened once we have move to an other element so we 
        // should remenber the used wrapper
        this._selectWrapper.fireOnChange();
        break;
      }

      case "FormAssist:AutoComplete": {
        currentElement.value = json.value;

        let event = currentElement.ownerDocument.createEvent("Events");
        event.initEvent("DOMAutoComplete", true, true);
        currentElement.dispatchEvent(event);
        break;
      }

      case "FormAssist:Closed":
        currentElement.blur();
        this._currentIndex = null;
        break;
    }
  },

  _els: Cc["@mozilla.org/eventlistenerservice;1"].getService(Ci.nsIEventListenerService),
  _hasKeyListener: function _hasKeyListener(aElement) {
    let els = this._els;
    let listeners = els.getListenerInfoFor(aElement, {});
    for (let i = 0; i < listeners.length; i++) {
      let listener = listeners[i];
      if (["keyup", "keydown", "keypress"].indexOf(listener.type) != -1
          && !listener.inSystemEventGroup) {
        return true;
      }
    }
    return false;
  },

  focusSync: false,
  handleEvent: function formHelperHandleEvent(aEvent) {
    if (!this._enabled || (!this.currentElement && (aEvent.type != "focus" || !this.focusSync)))
      return;

    switch (aEvent.type) {
      case "focus":
        let focusedElement = gFocusManager.getFocusedElementForWindow(content, true, {});

        // If a body element is editable and the body is the child of an
        // iframe we can assume this is an advanced HTML editor, so let's 
        // redirect the form helper selection to the iframe element
        if (focusedElement) {
          let editableElement = this._getTopLevelEditable(focusedElement);
          if (editableElement.isContentEditable && this._isValidElement(editableElement)) {
            let self = this;
            let timer = new Util.Timeout(function() {
              self.open(editableElement);
            });
            timer.once(0);
            return;
          }
        }

        // if an element is focused while we're closed but the element can be handle
        // by the assistant, try to activate it
        if (!this.currentElement) {
          if (focusedElement && this._isValidElement(focusedElement)) {
            let self = this;
            let timer = new Util.Timeout(function() {
              self.open(focusedElement);
            });
            timer.once(0);
          }
          return;
        }

        let focusedIndex = this._getIndexForElement(focusedElement);
        if (focusedIndex != -1 && this.currentIndex != focusedIndex)
          this.currentIndex = focusedIndex;
        break;

      case "keyup":
        let currentElement = this.currentElement;
        switch (aEvent.keyCode) {
          case aEvent.DOM_VK_DOWN:
            if (currentElement instanceof HTMLInputElement && !this._isAutocomplete(currentElement)) {
              if (this._hasKeyListener(currentElement))
                return;
            }
            else if (currentElement instanceof HTMLTextAreaElement) {
              let existSelection = currentElement.selectionEnd - currentElement.selectionStart;
              let isEnd = (currentElement.textLength == currentElement.selectionEnd);
              if (!isEnd || existSelection)
                return;
            }

            this.currentIndex++;
            break;

          case aEvent.DOM_VK_UP:
            if (currentElement instanceof HTMLInputElement && !this._isAutocomplete(currentElement)) {
              if (this._hasKeyListener(currentElement))
                return;
            }
            else if (currentElement instanceof HTMLTextAreaElement) {
              let existSelection = currentElement.selectionEnd - currentElement.selectionStart;
              let isStart = (currentElement.selectionEnd == 0);
              if (!isStart || existSelection)
                return;
            }

            this.currentIndex--;
            break;

          case aEvent.DOM_VK_RETURN:
            break;

          default:
            if (this._isAutocomplete(aEvent.target))
              sendAsyncMessage("FormAssist:AutoComplete", this._getJSON());
            break;
        }

        let caretRect = this._getCaretRect();
        if (!caretRect.isEmpty())
          sendAsyncMessage("FormAssist:Update", { caretRect: caretRect });
    }
  },

  _filterEditables: function formHelperFilterEditables(aNodes) {
    let result = [];
    for (let i = 0; i < aNodes.length; i++) {
      let node = aNodes[i];

      if (node.isContentEditable || node instanceof HTMLIFrameElement) {
        let editableElement = this._getTopLevelEditable(node);
        if (result.indexOf(editableElement) == -1)
          result.push(editableElement);
      }
      else {
        result.push(node);
      }
    }

    return result;
  },
  
  _getTopLevelEditable: function formHelperGetTopLevelEditable(aElement) {
    while (aElement && aElement.parentNode.isContentEditable)
      aElement = aElement.parentNode;

    if (aElement && aElement instanceof HTMLBodyElement && aElement.ownerDocument.defaultView != content.document.defaultView)
      return aElement.ownerDocument.defaultView.frameElement;

    return aElement;
  },

  _isAutocomplete: function formHelperIsAutocomplete(aElement) {
    if (aElement instanceof HTMLInputElement) {
      let autocomplete = aElement.getAttribute("autocomplete");
      let allowedValues = ["off", "false", "disabled"];
      if (allowedValues.indexOf(autocomplete) == -1)
        return true;
    }

    return false;
  },

  _isValidElement: function formHelperIsValidElement(aElement) {
    let formExceptions = { button: true, checkbox: true, file: true, image: true, radio: true, reset: true, submit: true };
    if (aElement instanceof HTMLInputElement && formExceptions[aElement.type])
      return false;

    if (aElement instanceof HTMLButtonElement ||
        (aElement.getAttribute("role") == "button" && aElement.hasAttribute("tabindex")))
      return false;

    return this._isNavigableElement(aElement) && this._isVisibleElement(aElement);
  },

  _isNavigableElement: function formHelperIsNavigableElement(aElement) {
    if (aElement.disabled || aElement.getAttribute("tabindex") == "-1")
      return false;

    if (aElement.getAttribute("role") == "button" && aElement.hasAttribute("tabindex"))
      return true;

    if (this._isSelectElement(aElement) || aElement instanceof HTMLTextAreaElement)
      return true;

    if (aElement instanceof HTMLInputElement || aElement instanceof HTMLButtonElement)
      return !(aElement.type == "hidden");

    if (aElement instanceof HTMLIFrameElement && aElement.contentDocument.body.isContentEditable)
      return true;

    return aElement.isContentEditable;
  },

  _isVisibleElement: function formHelperIsVisibleElement(aElement) {
    let style = aElement.ownerDocument.defaultView.getComputedStyle(aElement, null);
    if (!style)
      return false;

    let isVisible = (style.getPropertyValue("visibility") != "hidden");
    let isOpaque = (style.getPropertyValue("opacity") != 0);

    let rect = aElement.getBoundingClientRect();
    return isVisible && isOpaque && (rect.height != 0 || rect.width != 0);
  },

  _isSelectElement: function formHelperIsSelectElement(aElement) {
    return (aElement instanceof HTMLSelectElement || aElement instanceof XULMenuListElement);
  },

  /** Caret is used to input text for this element. */
  _getCaretRect: function _formHelperGetCaretRect() {
    let element = this.currentElement;
    if ((element instanceof HTMLTextAreaElement ||
        (element instanceof HTMLInputElement && element.type == "text")) &&
        gFocusManager.focusedElement == element) {
      let utils = Util.getWindowUtils(element.ownerDocument.defaultView);
      let rect = utils.sendQueryContentEvent(utils.QUERY_CARET_RECT, element.selectionEnd, 0, 0, 0);
      if (rect) {
        let scroll = Util.getScrollOffset(element.ownerDocument.defaultView);
        return new Rect(scroll.x + rect.left, scroll.y + rect.top, rect.width, rect.height);
      }
    }

    return new Rect(0, 0, 0, 0);
  },

  /** Gets a rect bounding important parts of the element that must be seen when assisting. */
  _getRect: function _formHelperGetRect() {
    const kDistanceMax = 100;
    let element = this.currentElement;
    let elRect = getBoundingContentRect(element);

    let labels = this._getLabels();
    for (let i=0; i<labels.length; i++) {
      let labelRect = getBoundingContentRect(labels[i]);
      if (labelRect.left < elRect.left) {
        let isClose = Math.abs(labelRect.left - elRect.left) - labelRect.width < kDistanceMax &&
                      Math.abs(labelRect.top - elRect.top) - labelRect.height < kDistanceMax;
        if (isClose) {
          let width = labelRect.width + elRect.width + (elRect.left - labelRect.left - labelRect.width);
          return new Rect(labelRect.left, labelRect.top, width, elRect.height).expandToIntegers();
        }
      }
    }
    return elRect;
  },

  _getLabels: function formHelperGetLabels() {
    let associatedLabels = [];

    let element = this.currentElement;
    let labels = element.ownerDocument.getElementsByTagName("label");
    for (let i=0; i<labels.length; i++) {
      if (labels[i].control == element)
        associatedLabels.push(labels[i]);
    }

    return associatedLabels.filter(this._isVisibleElement);
  },

  _getAllElements: function getAllElements(aElement) {
    // XXX good candidate for tracing if possible.
    // The tough ones are lenght and isVisibleElement.
    let document = aElement.ownerDocument;
    if (!document)
      return;

    let documents = Util.getAllDocuments(document);

    let elements = this._elements;
    for (let i = 0; i < documents.length; i++) {
      let selector = "input, button, select, textarea, [role=button], iframe[tabindex], [contenteditable=true]";
      let nodes = documents[i].querySelectorAll(selector);
      nodes = this._filterRadioButtons(nodes);

      for (let j = 0; j < nodes.length; j++) {
        let node = nodes[j];
        if (!this._isNavigableElement(node) || !this._isVisibleElement(node))
          continue;

        elements.push(node);
      }
    }
    this._elements = this._filterEditables(elements);

    function orderByTabIndex(a, b) {
      // for an explanation on tabbing navigation see
      // http://www.w3.org/TR/html401/interact/forms.html#h-17.11.1
      // In resume tab index navigation order is 1, 2, 3, ..., 32767, 0
      if (a.tabIndex == 0 || b.tabIndex == 0)
        return b.tabIndex;

      return a.tabIndex > b.tabIndex;
    }
    this._elements = elements.sort(orderByTabIndex);

    // retrieve the correct index
    let currentIndex = this._getIndexForElement(aElement);
    return currentIndex;
  },

  _getIndexForElement: function(aElement) {
    let currentIndex = -1;
    let elements = this._elements;
    for (let i = 0; i < elements.length; i++) {
      if (elements[i] == aElement)
        return i;
    }
    return -1;
  },

  _getJSON: function() {
    let element = this.currentElement;
    let list = getListForElement(element);
    return {
      current: {
        id: element.id,
        name: element.name,
        value: element.value,
        maxLength: element.maxLength,
        type: (element.getAttribute("type") || "").toLowerCase(),
        choices: list,
        isAutocomplete: this._isAutocomplete(this.currentElement),
        rect: this._getRect(),
        caretRect: this._getCaretRect()
      },
      hasPrevious: !!this._elements[this._currentIndex - 1],
      hasNext: !!this._elements[this._currentIndex + 1]
    };
  },

  /**
   * For each radio button group, remove all but the checked button
   * if there is one, or the first button otherwise.
   */
  _filterRadioButtons: function(aNodes) {
    // First pass: Find the checked or first element in each group.
    let chosenRadios = {};
    for (let i=0; i < aNodes.length; i++) {
      let node = aNodes[i];
      if (node.type == "radio" && (!chosenRadios.hasOwnProperty(node.name) || node.checked))
        chosenRadios[node.name] = node;
    }

    // Second pass: Exclude all other radio buttons from the list.
    let result = [];
    for (let i=0; i < aNodes.length; i++) {
      let node = aNodes[i];
      if (node.type == "radio" && chosenRadios[node.name] != node)
        continue;
      result.push(node);
    }
    return result;
  }
};


/******************************************************************************
 * The next classes wraps some forms elements such as different type of list to
 * abstract the difference between html and xul element while manipulating them
 *  - SelectWrapper   : <html:select>
 *  - MenulistWrapper : <xul:menulist>
 *****************************************************************************/

function getWrapperForElement(aElement) {
  let wrapper = null;
  if (aElement instanceof HTMLSelectElement) {
    wrapper = new SelectWrapper(aElement);
  }
  else if (aElement instanceof XULMenuListElement) {
    wrapper = new MenulistWrapper(aElement);
  }

  return wrapper;
}

function getListForElement(aElement) {
  let wrapper = getWrapperForElement(aElement);
  if (!wrapper)
    return null;

  let optionIndex = 0;
  let result = {
    multiple: wrapper.getMultiple(),
    choices: []
  };

  // Build up a flat JSON array of the choices. In HTML, it's possible for select element choices
  // to be under a group header (but not recursively). We distinguish between headers and entries
  // using the boolean "list.group".
  // XXX If possible, this would be a great candidate for tracing.
  let children = wrapper.getChildren();
  for (let i = 0; i < children.length; i++) {
    let child = children[i];
    if (wrapper.isGroup(child)) {
      // This is the group element. Add an entry in the choices that says that the following
      // elements are a member of this group.
      result.choices.push({ group: true,
                            text: child.label || child.firstChild.data,
                            disabled: child.disabled
                          });
      let subchildren = child.children;
      for (let j = 0; j < subchildren.length; j++) {
        let subchild = subchildren[j];
        result.choices.push({
          group: false,
          inGroup: true,
          text: wrapper.getText(subchild),
          disabled: child.disabled || subchild.disabled,
          selected: subchild.selected,
          optionIndex: optionIndex++
        });
      }
    }
    else if (wrapper.isOption(child)) {
      // This is a regular choice under no group.
      result.choices.push({
        group: false,
        inGroup: false,
        text: wrapper.getText(child),
        disabled: child.disabled,
        selected: child.selected,
        optionIndex: optionIndex++
      });
    }
  }

  return result;
};


function SelectWrapper(aControl) {
  this._control = aControl;
}

SelectWrapper.prototype = {
  getSelectedIndex: function() {
    return this._control.selectedIndex;
  },

  getMultiple: function() {
    return this._control.multiple;
  },

  getOptions: function() {
    return this._control.options;
  },

  getChildren: function() {
    return this._control.children;
  },

  getText: function(aChild) {
    return aChild.text;
  },

  isOption: function(aChild) {
    return aChild instanceof HTMLOptionElement;
  },

  isGroup: function(aChild) {
    return aChild instanceof HTMLOptGroupElement;
  },

  select: function(aIndex, aSelected, aClearAll) {
    let selectElement = this._control.QueryInterface(Ci.nsISelectElement);
    selectElement.setOptionsSelectedByIndex(aIndex, aIndex, aSelected, aClearAll, false, true);
  },

  fireOnChange: function() {
    let control = this._control;
    let evt = this._control.ownerDocument.createEvent("Events");
    evt.initEvent("change", true, true, this._control.ownerDocument.defaultView, 0,
                  false, false,
                  false, false, null);
    content.setTimeout(function() {
      control.dispatchEvent(evt);
    }, 0);
  }
};


// bug 559792
// Use wrappedJSObject when control is in content for extra protection
function MenulistWrapper(aControl) {
  this._control = aControl;
}

MenulistWrapper.prototype = {
  getSelectedIndex: function() {
    let control = this._control.wrappedJSObject || this._control;
    let result = control.selectedIndex;
    return ((typeof result == "number" && !isNaN(result)) ? result : -1);
  },

  getMultiple: function() {
    return false;
  },

  getOptions: function() {
    let control = this._control.wrappedJSObject || this._control;
    return control.menupopup.children;
  },

  getChildren: function() {
    let control = this._control.wrappedJSObject || this._control;
    return control.menupopup.children;
  },

  getText: function(aChild) {
    return aChild.label;
  },

  isOption: function(aChild) {
    return aChild instanceof Ci.nsIDOMXULSelectControlItemElement;
  },

  isGroup: function(aChild) {
    return false;
  },

  select: function(aIndex, aSelected, aClearAll) {
    let control = this._control.wrappedJSObject || this._control;
    control.selectedIndex = aIndex;
  },

  fireOnChange: function() {
    let control = this._control;
    let evt = document.createEvent("XULCommandEvent");
    evt.initCommandEvent("command", true, true, window, 0,
                         false, false,
                         false, false, null);
    content.setTimeout(function() {
      control.dispatchEvent(evt);
    }, 0);
  }
};

