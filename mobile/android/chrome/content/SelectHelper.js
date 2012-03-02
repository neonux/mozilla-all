/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var SelectHelper = {
  _uiBusy: false,

  handleClick: function(aTarget) {
    // if we're busy looking at a select we want to eat any clicks that
    // come to us, but not to process them
    if (this._uiBusy)
        return true;

    let target = aTarget;
    while (target) {
      if (this._isMenu(target) && !target.disabled) {
        this._uiBusy = true;
        target.focus();
        let list = this.getListForElement(target);
        this.show(list, target);
        target = null;
        this._uiBusy = false;
        return true;
      }
      if (target)
        target = target.parentNode;
    }
    return false;
  },

  show: function(aList, aElement) {
    let data = JSON.parse(sendMessageToJava({ gecko: aList }));
    let selected = data.button;
    if (selected == -1)
        return;

    if (aElement instanceof Ci.nsIDOMXULMenuListElement) {
      aElement.selectedIndex = selected;
    } else if (aElement instanceof HTMLSelectElement) {
      if (!(selected instanceof Array)) {
        let temp = [];
        for (let i = 0; i < aList.listitems.length; i++) {
          temp[i] = (i == selected);
        }
        selected = temp;
      }
      let i = 0;
      this.forOptions(aElement, function(aNode) {
        aNode.selected = selected[i++];
      });
    }
    this.fireOnChange(aElement);
  },

  _isMenu: function(aElement) {
    return (aElement instanceof HTMLSelectElement ||
            aElement instanceof Ci.nsIDOMXULMenuListElement);
  },

  getListForElement: function(aElement) {
    let result = {
      type: "Prompt:Show",
      multiple: aElement.multiple,
      selected: [],
      listitems: []
    };

    if (aElement.multiple) {
      result.buttons = [
        { label: Strings.browser.GetStringFromName("selectHelper.closeMultipleSelectDialog") },
      ];
    }

    let index = 0;
    this.forOptions(aElement, function(aNode, aOptions) {
      let item = {
        label: aNode.text || aNode.label,
        isGroup: aOptions.isGroup,
        inGroup: aOptions.inGroup,
        disabled: aNode.disabled,
        id: index
      }
      if (aOptions.inGroup)
        item.disabled = item.disabled || aNode.parentNode.disabled;

      result.listitems[index] = item;
      result.selected[index] = aNode.selected;
      index++;
    });
    return result;
  },

  forOptions: function(aElement, aFunction) {
    let parent = aElement;
    if (aElement instanceof Ci.nsIDOMXULMenuListElement)
      parent = aElement.menupopup;
    let children = parent.children;
    let numChildren = children.length;

    // if there are no children in this select, we add a dummy row so that at least something appears
    if (numChildren == 0)
      aFunction.call(this, { label: "" }, { isGroup: false, inGroup: false });

    for (let i = 0; i < numChildren; i++) {
      let child = children[i];
      if (child instanceof HTMLOptionElement ||
          child instanceof Ci.nsIDOMXULSelectControlItemElement) {
        // This is a regular choice under no group.
        aFunction.call(this, child, {
          isGroup: false, inGroup: false
        });
      } else if (child instanceof HTMLOptGroupElement) {
        aFunction.call(this, child, {
          isGroup: true, inGroup: false
        });

        let subchildren = child.children;
        let numSubchildren = subchildren.length;
        for (let j = 0; j < numSubchildren; j++) {
          let subchild = subchildren[j];
          aFunction.call(this, subchild, {
            isGroup: false, inGroup: true
          });
        }
      }
    }
  },

  fireOnChange: function(aElement) {
    let evt = aElement.ownerDocument.createEvent("Events");
    evt.initEvent("change", true, true, aElement.defaultView, 0,
                  false, false,
                  false, false, null);
    setTimeout(function() {
      aElement.dispatchEvent(evt);
    }, 0);
  }
};
