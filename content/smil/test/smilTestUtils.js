/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is Mozilla SMIL Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com> (original author)
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

// Note: Class syntax roughly based on:
// https://developer.mozilla.org/en/Core_JavaScript_1.5_Guide/Inheritance
const SVG_NS = "http://www.w3.org/2000/svg";

function extend(child, supertype)
{
   child.prototype.__proto__ = supertype.prototype;
}

// General Utility Methods
var SMILUtil =
{
  // Returns true if SMIL is enabled, false otherwise
  // XXXdholbert There should be a "nicer" way to do this - right now this will
  // trigger a 'NotYetImplemented' assertion on STDOUT, if SMIL is disabled.
  isSMILEnabled : function()
  {
    var svg = SMILUtil.getSVGRoot();
    try {
      SMILUtil.getSVGRoot().animationsPaused();
    } catch(e) {
      // Exception --> SMIL disabled
      return false;
    }
    // No exceptions --> SMIL enabled
    return true;
  },

  // Returns the first matched <svg> node in the document
  getSVGRoot : function()
  {
    return SMILUtil.getFirstElemWithTag("svg");
  },

  // Returns the first element in the document with the matching tag
  getFirstElemWithTag : function(aTargetTag)
  {
    var elemList = document.getElementsByTagName(aTargetTag);
    return (elemList.length == 0 ? null : elemList[0]);
  },

  // Simple wrapper for getComputedStyle
  getComputedStyleSimple: function(elem, prop) {
    return window.getComputedStyle(elem, null).getPropertyValue(prop);
  },

  // Smart wrapper for getComputedStyle, which will generate a "fake" computed
  // style for recognized shorthand properties (font, overflow, marker)
  getComputedStyleWrapper : function(elem, propName)
  {
    // Special cases for shorthand properties (which aren't directly queriable
    // via getComputedStyle)
    var computedStyle;
    if (propName == "font") {
      var subProps = ["font-style", "font-variant", "font-weight",
                      "font-size", "line-height", "font-family"];
      for (var i in subProps) {
        var subPropStyle = SMILUtil.getComputedStyleSimple(elem, subProps[i]);
        if (subPropStyle) {
          if (subProps[i] == "line-height") {
            // There needs to be a "/" before line-height
            subPropStyle = "/ " + subPropStyle;
          }
          if (!computedStyle) {
            computedStyle = subPropStyle;
          } else {
            computedStyle = computedStyle + " " + subPropStyle;
          }
        }
      }
    } else if (propName == "marker") {
      var subProps = ["marker-end", "marker-mid", "marker-start"];
      for (var i in subProps) {
        if (!computedStyle) {
          computedStyle = SMILUtil.getComputedStyleSimple(elem, subProps[i]);
        } else {
          is(computedStyle, SMILUtil.getComputedStyleSimple(elem, subProps[i]),
             "marker sub-properties should match each other " +
             "(they shouldn't be individually set)");
        }
      }
    } else if (propName == "overflow") {
      var subProps = ["overflow-x", "overflow-y"];
      for (var i in subProps) {
        if (!computedStyle) {
          computedStyle = SMILUtil.getComputedStyleSimple(elem, subProps[i]);
        } else {
          is(computedStyle, SMILUtil.getComputedStyleSimple(elem, subProps[i]),
             "overflow sub-properties should match each other " +
             "(they shouldn't be individually set)");
        }
      }
    } else {
      computedStyle = SMILUtil.getComputedStyleSimple(elem, propName);
    }
    return computedStyle;
  },
  
  // This method hides (i.e. sets "display: none" on) all of the given node's
  // descendents.  It also hides the node itself, if requested.
  hideSubtree : function(node, hideNodeItself)
  {
    // Hide node, if requested
    if (hideNodeItself) {
      if (node.style) {
        node.style.display = "none";
      }
    }

    // Hide node's descendents
    var child = node.firstChild;
    while (child) {
      SMILUtil.hideSubtree(child, true);
      child = child.nextSibling;
    }
  },
}

// Wrapper for timing information
function SMILTimingData(aBegin, aDur)
{
  this._begin = aBegin;
  this._dur = aDur;
}
SMILTimingData.prototype =
{
  _begin: null,
  _dur: null,
  getBeginTime      : function() { return this._begin; },
  getDur            : function() { return this._dur; },
  getEndTime        : function() { return this._begin + this._dur; },
  getFractionalTime : function(aPortion)
  {
    return this._begin + aPortion * this._dur;
  },
}

/**
 * Attribute: a container for information about an attribute we'll
 * attempt to animate with SMIL in our tests.
 *
 * See also the factory methods below: NonAnimatableAttribute(),
 * NonAdditiveAttribute(), and AdditiveAttribute().
 *
 * @param aAttrName The name of the attribute
 * @param aAttrType The type of the attribute ("CSS" vs "XML")
 * @param aTargetTag The name of an element that this attribute could be
 *                   applied to.
 * @param aIsAnimatable A bool indicating whether this attribute is defined as
 *                      animatable in the SVG spec.
 * @param aIsAdditive   A bool indicating whether this attribute is defined as
 *                      additive (i.e. supports "by" animation) in the SVG spec.
 */
function Attribute(aAttrName, aAttrType, aTargetTag,
                   aIsAnimatable, aIsAdditive)
{
  this.attrName = aAttrName;
  this.attrType = aAttrType;
  this.targetTag = aTargetTag;
  this.isAnimatable = aIsAnimatable;
  this.isAdditive = aIsAdditive;
}
Attribute.prototype =
{
  // Member variables
  attrName     : null,
  attrType     : null,
  isAnimatable : null,
  testcaseList : null,
};

// Generators for Attribute objects.  These allow lists of attribute
// definitions to be more human-readible than if we were using Attribute() with
// boolean flags, e.g. "Attribute(..., true, true), Attribute(..., true, false)
function NonAnimatableAttribute(aAttrName, aAttrType, aTargetTag)
{
  return new Attribute(aAttrName, aAttrType, aTargetTag, false, false);
}
function NonAdditiveAttribute(aAttrName, aAttrType, aTargetTag)
{
  return new Attribute(aAttrName, aAttrType, aTargetTag, true, false);
}
function AdditiveAttribute(aAttrName, aAttrType, aTargetTag)
{
  return new Attribute(aAttrName, aAttrType, aTargetTag, true, true);
}

/**
 * TestcaseBundle: a container for a group of tests for a particular attribute
 *
 * @param aAttribute An Attribute object for the attribute
 * @param aTestcaseList An array of AnimTestcase objects
 */
function TestcaseBundle(aAttribute, aTestcaseList, aSkipReason)
{
  this.animatedAttribute = aAttribute;
  this.testcaseList = aTestcaseList;
  this.skipReason = aSkipReason;
}
TestcaseBundle.prototype =
{
  // Member variables
  animatedAttribute : null,
  testcaseList      : null,
  skipReason        : null,

  // Methods
  go : function(aTimingData) {
    if (this.skipReason) {
      todo(false, "Skipping a bundle for '" + this.animatedAttribute.attrName +
           "' because: " + this.skipReason);
    } else {
      // Sanity Check: Bundle should have > 0 testcases
      if (!this.testcaseList || !this.testcaseList.length) {
        ok(false, "a bundle for '" + this.animatedAttribute.attrName +
           "' has no testcases");
      }

      var targetElem =
        SMILUtil.getFirstElemWithTag(this.animatedAttribute.targetTag);

      if (!targetElem) {
        ok(false, "Error: can't find an element of type '" +
           this.animatedAttribute.targetTag +
           "', so I can't test property '" +
           this.animatedAttribute.attrName + "'");
        return;
      }

      for (var testcaseIdx in this.testcaseList) {
        var testcase = this.testcaseList[testcaseIdx];
        if (testcase.skipReason) {
          todo(false, "Skipping a testcase for '" +
               this.animatedAttribute.attrName +
               "' because: " + testcase.skipReason);
        } else {
          testcase.runTest(targetElem, this.animatedAttribute,
                           aTimingData, false);
          testcase.runTest(targetElem, this.animatedAttribute,
                           aTimingData, true);
        }
      }
    }
  },
};

/**
 * AnimTestcase: an abstract class that represents an animation testcase.
 * (e.g. a set of "from"/"to" values to test)
 */
function AnimTestcase() {} // abstract => no constructor
AnimTestcase.prototype =
{
  // Member variables
  _animElementTagName : "animate", // Can be overridden for e.g. animateColor
  computedValMap      : null,
  skipReason          : null,
  
  // Methods
  /**
   * runTest: Runs this AnimTestcase
   *
   * @param aTargetElem The node to be targeted in our test animation.
   * @param aAnimAttr An Attribute object representing the attribute
   *                  to be targeted in our test animation.
   * @param aTimeData A SMILTimingData object with timing information for
   *                  our test animation.
   * @param aIsFreeze If true, indicates that our test animation should use
   *                  fill="freeze"; otherwise, we'll default to fill="remove".
   */
  runTest : function(aTargetElem, aAnimAttr, aTimeData, aIsFreeze)
  {
    // SANITY CHECKS
    if (!SMILUtil.getSVGRoot().animationsPaused()) {
      ok(false, "Should start each test with animations paused");
    }
    if (SMILUtil.getSVGRoot().getCurrentTime() != 0) {
      ok(false, "Should start each test at time = 0");
    }

    // SET UP
    // Cache initial computed value
    var baseVal = SMILUtil.getComputedStyleWrapper(aTargetElem,
                                                   aAnimAttr.attrName);

    // Create & append animation element
    var anim = this.setupAnimationElement(aAnimAttr, aTimeData, aIsFreeze);
    aTargetElem.appendChild(anim);

    // Build a list of [seek-time, expectedValue, errorMessage] triplets
    var seekList = this.buildSeekList(aAnimAttr, baseVal, aTimeData, aIsFreeze);

    // DO THE ACTUAL TESTING
    this.seekAndTest(seekList, aTargetElem, aAnimAttr.attrName);

    // CLEAN UP
    aTargetElem.removeChild(anim);
    SMILUtil.getSVGRoot().setCurrentTime(0);
  },

  // HELPER FUNCTIONS
  // setupAnimationElement: <animate> element
  // Subclasses should extend this parent method
  setupAnimationElement : function(aAnimAttr, aTimeData, aIsFreeze)
  {
    var animElement = document.createElementNS(SVG_NS,
                                               this._animElementTagName);
    animElement.setAttribute("attributeName", aAnimAttr.attrName);
    animElement.setAttribute("attributeType", aAnimAttr.attrType);
    animElement.setAttribute("begin", aTimeData.getBeginTime());
    animElement.setAttribute("dur", aTimeData.getDur());
    if (aIsFreeze) {
      animElement.setAttribute("fill", "freeze");
    }
    return animElement;
  },

  buildSeekList : function(aAnimAttr, aBaseVal, aTimeData, aIsFreeze)
  {
    if (!aAnimAttr.isAnimatable) {
      return this.buildSeekListStatic(aAnimAttr, aBaseVal, aTimeData,
                                      "defined as non-animatable in SVG spec");
    }
    return this.buildSeekListAnimated(aAnimAttr, aBaseVal,
                                      aTimeData, aIsFreeze)
  },

  seekAndTest : function(aSeekList, aTargetElem, aTargetAttr)
  {
    var svg = document.getElementById("svg");
    for (var i in aSeekList) {
      var entry = aSeekList[i];
      SMILUtil.getSVGRoot().setCurrentTime(entry[0]);
      is(SMILUtil.getComputedStyleWrapper(aTargetElem, aTargetAttr),
         entry[1], entry[2]);
    }
  },

  // methods that expect to be overridden in subclasses
  buildSeekListStatic : function(aAnimAttr, aBaseVal,
                                 aTimeData, aReasonStatic) {},
  buildSeekListAnimated : function(aAnimAttr, aBaseVal,
                                   aTimeData, aIsFreeze) {},
};


// Abstract parent class to share code between from-to & from-by testcases.
function AnimTestcaseFrom() {} // abstract => no constructor
AnimTestcaseFrom.prototype =
{
  // Member variables
  from           : null,

  // Methods
  setupAnimationElement : function(aAnimAttr, aTimeData, aIsFreeze)
  {
    // Call super, and then add my own customization
    var animElem = AnimTestcase.prototype.setupAnimationElement.apply(this,
                                         [aAnimAttr, aTimeData, aIsFreeze]);
    animElem.setAttribute("from", this.from)
    return animElem;
  },

  buildSeekListStatic : function(aAnimAttr, aBaseVal, aTimeData, aReasonStatic)
  {
    var seekList = new Array();
    var msgPrefix = aAnimAttr.attrName +
      ": shouldn't be affected by animation ";
    seekList.push([aTimeData.getBeginTime(), aBaseVal,
                   msgPrefix + "(at animation begin) - " + aReasonStatic]);
    seekList.push([aTimeData.getFractionalTime(1/2), aBaseVal,
                   msgPrefix + "(at animation mid) - " + aReasonStatic]);
    seekList.push([aTimeData.getEndTime(), aBaseVal,
                   msgPrefix + "(at animation end) - " + aReasonStatic]);
    seekList.push([aTimeData.getEndTime() + aTimeData.getDur(), aBaseVal,
                   msgPrefix + "(after animation end) - " + aReasonStatic]);
    return seekList;
  },

  buildSeekListAnimated : function(aAnimAttr, aBaseVal, aTimeData, aIsFreeze)
  {
    var seekList = new Array();
    var msgPrefix = aAnimAttr.attrName + ": ";
    seekList.push([aTimeData.getBeginTime(),
                   this.computedValMap.fromComp || this.from,
                   msgPrefix + "checking that 'from' value is set " +
                   "at start of animation"]);
    seekList.push([aTimeData.getFractionalTime(1/2),
                   this.computedValMap.midComp ||
                   this.computedValMap.toComp || this.to,
                   msgPrefix + "checking value halfway through animation"]);

    var finalMsg;
    var expectedEndVal;
    if (aIsFreeze) {
      expectedEndVal = this.computedValMap.toComp || this.to;
      finalMsg = msgPrefix + "[freeze-mode] checking that final value is set ";
    } else {
      expectedEndVal = aBaseVal;
      finalMsg = msgPrefix +
        "[remove-mode] checking that animation is cleared ";
    }
    seekList.push([aTimeData.getEndTime(),
                   expectedEndVal, finalMsg + "at end of animation"]);
    seekList.push([aTimeData.getEndTime() + aTimeData.getDur(),
                   expectedEndVal, finalMsg + "after end of animation"]);
    return seekList;
  },
}
extend(AnimTestcaseFrom, AnimTestcase);

/*
 * A testcase for a simple "from-to" animation
 * @param aFrom  The 'from' value
 * @param aTo    The 'to' value
 * @param aComputedValMap  A hash-map that contains some computed values,
 *                         if they're needed, as follows:
 *    - fromComp: Computed value version of |aFrom| (if different from |aFrom|)
 *    - midComp:  Computed value that we expect to visit halfway through the
 *                animation (if different from |aTo|)
 *    - toComp:   Computed value version of |aTo| (if different from |aTo|)
 * @param aSkipReason  If this test-case is known to currently fail, this
 *                     parameter should be a string explaining why.
 *                     Otherwise, this value should be null (or omitted).
 *
 */
function AnimTestcaseFromTo(aFrom, aTo, aComputedValMap, aSkipReason)
{
  this.from           = aFrom;
  this.to             = aTo;
  this.computedValMap = aComputedValMap || {}; // Let aComputedValMap be omitted
  this.skipReason     = aSkipReason;
}
AnimTestcaseFromTo.prototype =
{
  // Member variables
  to : null,

  // Methods
  setupAnimationElement : function(aAnimAttr, aTimeData, aIsFreeze)
  {
    // Call super, and then add my own customization
    var animElem = AnimTestcaseFrom.prototype.setupAnimationElement.apply(this,
                                            [aAnimAttr, aTimeData, aIsFreeze]);
    animElem.setAttribute("to", this.to)
    return animElem;
  },
}
extend(AnimTestcaseFromTo, AnimTestcaseFrom);

/*
 * A testcase for a simple "from-by" animation.
 *
 * @param aFrom  The 'from' value
 * @param aBy    The 'by' value
 * @param aComputedValMap  A hash-map that contains some computed values that
 *                         we expect to visit, as follows:
 *    - fromComp: Computed value version of |aFrom| (if different from |aFrom|)
 *    - midComp:  Computed value that we expect to visit halfway through the
 *                animation (|aFrom| + |aBy|/2)
 *    - toComp:   Computed value of the animation endpoint (|aFrom| + |aBy|)
 * @param aSkipReason  If this test-case is known to currently fail, this
 *                     parameter should be a string explaining why.
 *                     Otherwise, this value should be null (or omitted).
 */
function AnimTestcaseFromBy(aFrom, aBy, aComputedValMap, aSkipReason)
{
  this.from           = aFrom;
  this.by             = aBy;
  this.computedValMap = aComputedValMap;
  this.skipReason     = aSkipReason;
  if (!this.computedValMap.toComp) {
    ok(false, "AnimTestcaseFromBy needs expected computed final value");
  }
}
AnimTestcaseFromBy.prototype =
{
  // Member variables
  by : null,

  // Methods
  setupAnimationElement : function(aAnimAttr, aTimeData, aIsFreeze)
  {
    // Call super, and then add my own customization
    var animElem = AnimTestcaseFrom.prototype.setupAnimationElement.apply(this,
                                            [aAnimAttr, aTimeData, aIsFreeze]);
    animElem.setAttribute("by", this.by)
    return animElem;
  },
  buildSeekList : function(aAnimAttr, aBaseVal, aTimeData, aIsFreeze)
  {
    if (!aAnimAttr.isAdditive) {
      return this.buildSeekListStatic(aAnimAttr, aBaseVal, aTimeData,
                                      "defined as non-additive in SVG spec");
    }
    // Just use inherited method
    return AnimTestcaseFrom.prototype.buildSeekList.apply(this,
                                [aAnimAttr, aBaseVal, aTimeData, aIsFreeze]);
  },
}
extend(AnimTestcaseFromBy, AnimTestcaseFrom);

/*
 * A testcase for a "paced-mode" animation
 * @param aValues   An array of values, to be used as the "Values" list
 * @param aComputedValMap  A hash-map that contains some computed values,
 *                         if they're needed, as follows:
 *      - comp0:   The computed value at the start of the animation
 *      - comp1_6: The computed value exactly 1/6 through animation
 *      - comp1_3: The computed value exactly 1/3 through animation
 *      - comp2_3: The computed value exactly 2/3 through animation
 *      - comp1:   The computed value of the animation endpoint
 *  The math works out easiest if...
 *    (a) aValuesString has 3 entries in its values list: vA, vB, vC
 *    (b) dist(vB, vC) = 2 * dist(vA, vB)
 *  With this setup, we can come up with expected intermediate values according
 *  to the following rules:
 *    - comp0 should be vA
 *    - comp1_6 should be us halfway between vA and vB
 *    - comp1_3 should be vB
 *    - comp2_3 should be halfway between vB and vC
 *    - comp1 should be vC
 * @param aSkipReason  If this test-case is known to currently fail, this
 *                     parameter should be a string explaining why.
 *                     Otherwise, this value should be null (or omitted).
 */
function AnimTestcasePaced(aValuesString, aComputedValMap, aSkipReason)
{
  this.valuesString   = aValuesString;
  this.computedValMap = aComputedValMap;
  this.skipReason     = aSkipReason;
  if (this.computedValMap &&
      (!this.computedValMap.comp0 ||
       !this.computedValMap.comp1_6 ||
       !this.computedValMap.comp1_3 ||
       !this.computedValMap.comp2_3 ||
       !this.computedValMap.comp1)) {
    ok(false, "This AnimTestcasePaced has an incomplete computed value map");
  }
}
AnimTestcasePaced.prototype =
{
  // Member variables
  valuesString : null,
  
  // Methods
  setupAnimationElement : function(aAnimAttr, aTimeData, aIsFreeze)
  {
    // Call super, and then add my own customization
    var animElem = AnimTestcase.prototype.setupAnimationElement.apply(this,
                                            [aAnimAttr, aTimeData, aIsFreeze]);
    animElem.setAttribute("values", this.valuesString)
    animElem.setAttribute("calcMode", "paced");
    return animElem;
  },
  buildSeekListAnimated : function(aAnimAttr, aBaseVal, aTimeData, aIsFreeze)
  {
    var seekList = new Array();
    var msgPrefix = aAnimAttr.attrName + ": checking value ";
    seekList.push([aTimeData.getBeginTime(),
                   this.computedValMap.comp0,
                   msgPrefix + "at start of animation"]);
    seekList.push([aTimeData.getFractionalTime(1/6),
                   this.computedValMap.comp1_6,
                   msgPrefix + "1/6 of the way through animation."]);
    seekList.push([aTimeData.getFractionalTime(1/3),
                   this.computedValMap.comp1_3,
                   msgPrefix + "1/3 of the way through animation."]);
    seekList.push([aTimeData.getFractionalTime(2/3),
                   this.computedValMap.comp2_3,
                   msgPrefix + "2/3 of the way through animation."]);

    var finalMsg;
    var expectedEndVal;
    if (aIsFreeze) {
      expectedEndVal = this.computedValMap.comp1;
      finalMsg = aAnimAttr.attrName +
        ": [freeze-mode] checking that final value is set ";
    } else {
      expectedEndVal = aBaseVal;
      finalMsg = aAnimAttr.attrName +
        ": [remove-mode] checking that animation is cleared ";
    }
    seekList.push([aTimeData.getEndTime(),
                   expectedEndVal, finalMsg + "at end of animation"]);
    seekList.push([aTimeData.getEndTime() + aTimeData.getDur(),
                   expectedEndVal, finalMsg + "after end of animation"]);
    return seekList;
  },
  buildSeekListStatic : function(aAnimAttr, aBaseVal, aTimeData, aReasonStatic)
  {
    var seekList = new Array();
    var msgPrefix =
      aAnimAttr.attrName + ": shouldn't be affected by animation ";
    seekList.push([aTimeData.getBeginTime(), aBaseVal,
                   msgPrefix + "(at animation begin) - " + aReasonStatic]);
    seekList.push([aTimeData.getFractionalTime(1/6), aBaseVal,
                   msgPrefix + "(1/6 of the way through animation) - " +
                   aReasonStatic]);
    seekList.push([aTimeData.getFractionalTime(1/3), aBaseVal,
                   msgPrefix + "(1/3 of the way through animation) - " +
                   aReasonStatic]);
    seekList.push([aTimeData.getFractionalTime(2/3), aBaseVal,
                   msgPrefix + "(2/3 of the way through animation) - " +
                   aReasonStatic]);
    seekList.push([aTimeData.getEndTime(), aBaseVal,
                   msgPrefix + "(at animation end) - " + aReasonStatic]);
    seekList.push([aTimeData.getEndTime() + aTimeData.getDur(), aBaseVal,
                   msgPrefix + "(after animation end) - " + aReasonStatic]);
    return seekList;
  },
};
extend(AnimTestcasePaced, AnimTestcase);

// MAIN METHOD
function testBundleList(aBundleList, aTimingData)
{
  for (var bundleIdx in aBundleList) {
    aBundleList[bundleIdx].go(aTimingData);
  }
}
