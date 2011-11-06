/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is the Mozilla Inspector Module.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Walker (jwalker@mozilla.com) (Original Author)
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *   Michael Ratcliffe <mratcliffe@mozilla.com>
 *   Rob Campbell <rcampbell@mozilla.com>
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

const Cu = Components.utils;
const FILTER_CHANGED_TIMEOUT = 300;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/CssLogic.jsm");
Cu.import("resource:///modules/devtools/Templater.jsm");

var EXPORTED_SYMBOLS = ["CssHtmlTree", "PropertyView"];

/**
 * CssHtmlTree is a panel that manages the display of a table sorted by style.
 * There should be one instance of CssHtmlTree per style display (of which there
 * will generally only be one).
 *
 * @params {StyleInspector} aStyleInspector The owner of this CssHtmlTree
 * @constructor
 */
function CssHtmlTree(aStyleInspector)
{
  this.styleWin = aStyleInspector.iframe;
  this.styleInspector = aStyleInspector;
  this.cssLogic = aStyleInspector.cssLogic;
  this.doc = aStyleInspector.document;
  this.win = aStyleInspector.window;
  this.getRTLAttr = this.win.getComputedStyle(this.win.gBrowser).direction;
  this.propertyViews = [];

  // The document in which we display the results (csshtmltree.xul).
  this.styleDocument = this.styleWin.contentWindow.document;

  // Nodes used in templating
  this.root = this.styleDocument.getElementById("root");
  this.path = this.styleDocument.getElementById("path");
  this.templateRoot = this.styleDocument.getElementById("templateRoot");
  this.templatePath = this.styleDocument.getElementById("templatePath");
  this.propertyContainer = this.styleDocument.getElementById("propertyContainer");
  this.templateProperty = this.styleDocument.getElementById("templateProperty");
  this.panel = aStyleInspector.panel;

  // No results text.
  this.noResults = this.styleDocument.getElementById("noResults");

  // The element that we're inspecting, and the document that it comes from.
  this.viewedElement = null;
  this.createStyleViews();
}

/**
 * Memoized lookup of a l10n string from a string bundle.
 * @param {string} aName The key to lookup.
 * @returns A localized version of the given key.
 */
CssHtmlTree.l10n = function CssHtmlTree_l10n(aName)
{
  try {
    return CssHtmlTree._strings.GetStringFromName(aName);
  } catch (ex) {
    Services.console.logStringMessage("Error reading '" + aName + "'");
    throw new Error("l10n error with " + aName);
  }
};

/**
 * Clone the given template node, and process it by resolving ${} references
 * in the template.
 *
 * @param {nsIDOMElement} aTemplate the template note to use.
 * @param {nsIDOMElement} aDestination the destination node where the
 * processed nodes will be displayed.
 * @param {object} aData the data to pass to the template.
 * @param {Boolean} aPreserveDestination If true then the template will be
 * appended to aDestination's content else aDestination.innerHTML will be
 * cleared before the template is appended.
 */
CssHtmlTree.processTemplate = function CssHtmlTree_processTemplate(aTemplate,
                                  aDestination, aData, aPreserveDestination)
{
  if (!aPreserveDestination) {
    aDestination.innerHTML = "";
  }

  // All the templater does is to populate a given DOM tree with the given
  // values, so we need to clone the template first.
  let duplicated = aTemplate.cloneNode(true);
  new Templater().processNode(duplicated, aData);
  while (duplicated.firstChild) {
    aDestination.appendChild(duplicated.firstChild);
  }
};

XPCOMUtils.defineLazyGetter(CssHtmlTree, "_strings", function() Services.strings
        .createBundle("chrome://browser/locale/devtools/styleinspector.properties"));

CssHtmlTree.prototype = {
  // Cache the list of properties that have matched and unmatched properties.
  _matchedProperties: null,
  _unmatchedProperties: null,

  htmlComplete: false,

  // Used for cancelling timeouts in the style filter.
  _filterChangedTimeout: null,

  // The search filter
  searchField: null,

  // Reference to the "Only user Styles" checkbox.
  onlyUserStylesCheckbox: null,

  // Holds the ID of the panelRefresh timeout.
  _panelRefreshTimeout: null,

  // Toggle for zebra striping
  _darkStripe: true,

  // Number of visible properties
  numVisibleProperties: 0,

  get showOnlyUserStyles()
  {
    return this.onlyUserStylesCheckbox.checked;
  },

  /**
   * Update the highlighted element. The CssHtmlTree panel will show the style
   * information for the given element.
   * @param {nsIDOMElement} aElement The highlighted node to get styles for.
   */
  highlight: function CssHtmlTree_highlight(aElement)
  {
    if (this.viewedElement == aElement) {
      return;
    }

    this.viewedElement = aElement;
    this._unmatchedProperties = null;
    this._matchedProperties = null;

    CssHtmlTree.processTemplate(this.templatePath, this.path, this);

    if (this.htmlComplete) {
      this.refreshPanel();
    } else {
      if (this._panelRefreshTimeout) {
        this.win.clearTimeout(this._panelRefreshTimeout);
      }

      CssHtmlTree.processTemplate(this.templateRoot, this.root, this);

      // We use a setTimeout loop to display the properties in batches of 15 at a
      // time. This results in a perceptibly more responsive UI.
      let i = 0;
      let batchSize = 15;
      let max = CssHtmlTree.propertyNames.length - 1;
      function displayProperties() {
        if (this.viewedElement == aElement && this.styleInspector.isOpen()) {
          // Display the next 15 properties
          for (let step = i + batchSize; i < step && i <= max; i++) {
            let name = CssHtmlTree.propertyNames[i];
            let propView = new PropertyView(this, name);
            CssHtmlTree.processTemplate(this.templateProperty,
              this.propertyContainer, propView, true);
            if (propView.visible) {
              this.numVisibleProperties++;
            }
            propView.refreshAllSelectors();
            this.propertyViews.push(propView);
          }
          if (i < max) {
            // There are still some properties to display. We loop here to display
            // the next batch of 15.
            this._panelRefreshTimeout =
              this.win.setTimeout(displayProperties.bind(this), 15);
          } else {
            this.htmlComplete = true;
            this._panelRefreshTimeout = null;
            this.noResults.hidden = this.numVisibleProperties > 0;
            Services.obs.notifyObservers(null, "StyleInspector-populated", null);
          }
        }
      }
      this._panelRefreshTimeout =
        this.win.setTimeout(displayProperties.bind(this), 15);
    }
  },

  /**
   * Refresh the panel content.
   */
  refreshPanel: function CssHtmlTree_refreshPanel()
  {
    if (this._panelRefreshTimeout) {
      this.win.clearTimeout(this._panelRefreshTimeout);
    }

    this.noResults.hidden = true;

    // Reset visible property count
    this.numVisibleProperties = 0;

    // Reset zebra striping.
    this._darkStripe = true;

    // We use a setTimeout loop to display the properties in batches of 15 at a
    // time. This results in a perceptibly more responsive UI.
    let i = 0;
    let batchSize = 15;
    let max = this.propertyViews.length - 1;
    function refreshView() {
      // Refresh the next 15 property views
      for (let step = i + batchSize; i < step && i <= max; i++) {
        this.propertyViews[i].refresh();
      }
      if (i < max) {
        // There are still some property views to refresh. We loop here to
        // display the next batch of 15.
        this._panelRefreshTimeout = this.win.setTimeout(refreshView.bind(this), 15);
      } else {
        this._panelRefreshTimeout = null;
        this.noResults.hidden = this.numVisibleProperties > 0;
        Services.obs.notifyObservers(null, "StyleInspector-populated", null);
      }
    }
    this._panelRefreshTimeout = this.win.setTimeout(refreshView.bind(this), 15);
  },

  /**
   * Called when the user clicks on a parent element in the "current element"
   * path.
   *
   * @param {Event} aEvent the DOM Event object.
   */
  pathClick: function CssHtmlTree_pathClick(aEvent)
  {
    aEvent.preventDefault();
    if (aEvent.target && this.viewedElement != aEvent.target.pathElement) {
      this.styleInspector.selectFromPath(aEvent.target.pathElement);
    }
  },

  /**
   * Called when the user enters a search term.
   *
   * @param {Event} aEvent the DOM Event object.
   */
  filterChanged: function CssHtmlTree_filterChanged(aEvent)
  {
    let win = this.styleWin.contentWindow;

    if (this._filterChangedTimeout) {
      win.clearTimeout(this._filterChangedTimeout);
    }

    this._filterChangedTimeout = win.setTimeout(function() {
      this.refreshPanel();
      this._filterChangeTimeout = null;
    }.bind(this), FILTER_CHANGED_TIMEOUT);
  },

  /**
   * The change event handler for the onlyUserStyles checkbox. When
   * onlyUserStyles.checked is true we do not display properties that have no
   * matched selectors, and we do not display UA styles. If .checked is false we
   * do display even properties with no matched selectors, and we include the UA
   * styles.
   *
   * @param {Event} aEvent the DOM Event object.
   */
  onlyUserStylesChanged: function CssHtmltree_onlyUserStylesChanged(aEvent)
  {
    this._matchedProperties = null;
    this.cssLogic.sourceFilter = this.showOnlyUserStyles ?
                                 CssLogic.FILTER.ALL :
                                 CssLogic.FILTER.UA;
    this.refreshPanel();
  },

  /**
   * Provide access to the path to get from document.body to the selected
   * element.
   *
   * @return {array} the array holding the path from document.body to the
   * selected element.
   */
  get pathElements()
  {
    return CssLogic.getShortNamePath(this.viewedElement);
  },

  /**
   * The CSS as displayed by the UI.
   */
  createStyleViews: function CssHtmlTree_createStyleViews()
  {
    if (CssHtmlTree.propertyNames) {
      return;
    }

    CssHtmlTree.propertyNames = [];

    // Here we build and cache a list of css properties supported by the browser
    // We could use any element but let's use the main document's root element
    let styles = this.styleWin.contentWindow.getComputedStyle(this.styleDocument.documentElement);
    let mozProps = [];
    for (let i = 0, numStyles = styles.length; i < numStyles; i++) {
      let prop = styles.item(i);
      if (prop.charAt(0) == "-") {
        mozProps.push(prop);
      } else {
        CssHtmlTree.propertyNames.push(prop);
      }
    }

    CssHtmlTree.propertyNames.sort();
    CssHtmlTree.propertyNames.push.apply(CssHtmlTree.propertyNames,
      mozProps.sort());
  },

  /**
   * Get a list of properties that have matched selectors.
   *
   * @return {object} the object maps property names (keys) to booleans (values)
   * that tell if the given property has matched selectors or not.
   */
  get matchedProperties()
  {
    if (!this._matchedProperties) {
      this._matchedProperties =
        this.cssLogic.hasMatchedSelectors(CssHtmlTree.propertyNames);
    }
    return this._matchedProperties;
  },

  /**
   * Check if a property has unmatched selectors. Result is cached.
   *
   * @param {string} aProperty the name of the property you want to check.
   * @return {boolean} true if the property has unmatched selectors, false
   * otherwise.
   */
  hasUnmatchedSelectors: function CssHtmlTree_hasUnmatchedSelectors(aProperty)
  {
    // Initially check all of the properties that return false for
    // hasMatchedSelectors(). This speeds-up the UI.
    if (!this._unmatchedProperties) {
      let properties = [];
      CssHtmlTree.propertyNames.forEach(function(aName) {
        if (!this.matchedProperties[aName]) {
          properties.push(aName);
        }
      }, this);

      if (properties.indexOf(aProperty) == -1) {
        properties.push(aProperty);
      }

      this._unmatchedProperties = this.cssLogic.hasUnmatchedSelectors(properties);
    }

    // Lazy-get the result for properties we do not have cached.
    if (!(aProperty in this._unmatchedProperties)) {
      let result = this.cssLogic.hasUnmatchedSelectors([aProperty]);
      this._unmatchedProperties[aProperty] = result[aProperty];
    }

    return this._unmatchedProperties[aProperty];
  },

  /**
   * Destructor for CssHtmlTree.
   */
  destroy: function CssHtmlTree_destroy()
  {
    delete this.viewedElement;

    // Remove event listeners
    this.onlyUserStylesCheckbox.removeEventListener("command",
      this.onlyUserStylesChanged);
    this.searchField.removeEventListener("command", this.filterChanged);

    // Nodes used in templating
    delete this.root;
    delete this.path;
    delete this.templatePath;
    delete this.propertyContainer;
    delete this.templateProperty;
    delete this.panel;

    // The document in which we display the results (csshtmltree.xul).
    delete this.styleDocument;

    // The element that we're inspecting, and the document that it comes from.
    delete this.propertyViews;
    delete this.styleWin;
    delete this.cssLogic;
    delete this.doc;
    delete this.win;
    delete this.styleInspector;
  },
};

/**
 * A container to give easy access to property data from the template engine.
 *
 * @constructor
 * @param {CssHtmlTree} aTree the CssHtmlTree instance we are working with.
 * @param {string} aName the CSS property name for which this PropertyView
 * instance will render the rules.
 */
function PropertyView(aTree, aName)
{
  this.tree = aTree;
  this.name = aName;
  this.getRTLAttr = aTree.getRTLAttr;

  this.link = "https://developer.mozilla.org/en/CSS/" + aName;

  this.templateMatchedSelectors = aTree.styleDocument.getElementById("templateMatchedSelectors");
}

PropertyView.prototype = {
  // The parent element which contains the open attribute
  element: null,

  // Property header node
  propertyHeader: null,

  // Destination for property values
  valueNode: null,

  // Are matched rules expanded?
  matchedExpanded: false,

  // Are unmatched rules expanded?
  unmatchedExpanded: false,

  // Unmatched selector table
  unmatchedSelectorTable: null,

  // Matched selector container
  matchedSelectorsContainer: null,

  // Matched selector expando
  matchedExpander: null,

  // Unmatched selector expando
  unmatchedExpander: null,

  // Unmatched selector container
  unmatchedSelectorsContainer: null,

  // Unmatched title block
  unmatchedTitleBlock: null,

  // Cache for matched selector views
  _matchedSelectorViews: null,

  // Cache for unmatched selector views
  _unmatchedSelectorViews: null,

  // The previously selected element used for the selector view caches
  prevViewedElement: null,

  /**
   * Get the computed style for the current property.
   *
   * @return {string} the computed style for the current property of the
   * currently highlighted element.
   */
  get value()
  {
    return this.propertyInfo.value;
  },

  /**
   * An easy way to access the CssPropertyInfo behind this PropertyView.
   */
  get propertyInfo()
  {
    return this.tree.cssLogic.getPropertyInfo(this.name);
  },

  /**
   * Does the property have any matched selectors?
   */
  get hasMatchedSelectors()
  {
    return this.tree.matchedProperties[this.name];
  },

  /**
   * Does the property have any unmatched selectors?
   */
  get hasUnmatchedSelectors()
  {
    return this.tree.hasUnmatchedSelectors(this.name);
  },

  /**
   * Should this property be visible?
   */
  get visible()
  {
    if (this.tree.showOnlyUserStyles && !this.hasMatchedSelectors) {
      return false;
    }

    let searchTerm = this.tree.searchField.value.toLowerCase();
    if (searchTerm && this.name.toLowerCase().indexOf(searchTerm) == -1 &&
      this.value.toLowerCase().indexOf(searchTerm) == -1) {
      return false;
    }

    return true;
  },

  /**
   * Returns the className that should be assigned to the propertyView.
   *
   * @return string
   */
  get className()
  {
    if (this.visible) {
      this.tree._darkStripe = !this.tree._darkStripe;
      let darkValue = this.tree._darkStripe ?
                      "property-view darkrow" : "property-view";
      return darkValue;
    }
    return "property-view-hidden";
  },

  /**
   * Refresh the panel's CSS property value.
   */
  refresh: function PropertyView_refresh()
  {
    this.element.className = this.className;

    if (this.prevViewedElement != this.tree.viewedElement) {
      this._matchedSelectorViews = null;
      this._unmatchedSelectorViews = null;
      this.prevViewedElement = this.tree.viewedElement;
    }

    if (!this.tree.viewedElement || !this.visible) {
      this.valueNode.innerHTML = "";
      this.matchedSelectorsContainer.hidden = true;
      this.matchedSelectorsContainer.innerHTML = "";
      this.matchedExpander.removeAttribute("open");
      return;
    }

    this.tree.numVisibleProperties++;
    this.valueNode.innerHTML = this.propertyInfo.value;
    this.refreshAllSelectors();
  },

  /**
   * Refresh the panel matched rules.
   */
  refreshMatchedSelectors: function PropertyView_refreshMatchedSelectors()
  {
    let hasMatchedSelectors = this.hasMatchedSelectors;
    this.matchedSelectorsContainer.hidden = !hasMatchedSelectors;

    if (hasMatchedSelectors) {
      this.propertyHeader.classList.add("expandable");
    } else {
      this.propertyHeader.classList.remove("expandable");
    }

    if (this.matchedExpanded && hasMatchedSelectors) {
      CssHtmlTree.processTemplate(this.templateMatchedSelectors,
        this.matchedSelectorsContainer, this);
      this.matchedExpander.setAttribute("open", "");
    } else {
      this.matchedSelectorsContainer.innerHTML = "";
      this.matchedExpander.removeAttribute("open");
    }
  },

  /**
   * Refresh the panel unmatched rules.
   */
  refreshUnmatchedSelectors: function PropertyView_refreshUnmatchedSelectors()
  {
    let hasMatchedSelectors = this.hasMatchedSelectors;

    this.unmatchedSelectorTable.hidden = !this.unmatchedExpanded;

    if (hasMatchedSelectors) {
      this.unmatchedSelectorsContainer.hidden = !this.matchedExpanded ||
        !this.hasUnmatchedSelectors;
      this.unmatchedTitleBlock.hidden = false;
    } else {
      this.unmatchedSelectorsContainer.hidden = !this.unmatchedExpanded;
      this.unmatchedTitleBlock.hidden = true;
    }

    if (this.unmatchedExpanded && this.hasUnmatchedSelectors) {
      CssHtmlTree.processTemplate(this.templateUnmatchedSelectors,
        this.unmatchedSelectorTable, this);
      if (!hasMatchedSelectors) {
        this.matchedExpander.setAttribute("open", "");
        this.unmatchedSelectorTable.classList.add("only-unmatched");
      } else {
        this.unmatchedExpander.setAttribute("open", "");
        this.unmatchedSelectorTable.classList.remove("only-unmatched");
      }
    } else {
      if (!hasMatchedSelectors) {
        this.matchedExpander.removeAttribute("open");
      }
      this.unmatchedExpander.removeAttribute("open");
      this.unmatchedSelectorTable.innerHTML = "";
    }
  },

  /**
   * Refresh the panel matched and unmatched rules
   */
  refreshAllSelectors: function PropertyView_refreshAllSelectors()
  {
    this.refreshMatchedSelectors();
  },

  /**
   * Provide access to the matched SelectorViews that we are currently
   * displaying.
   */
  get matchedSelectorViews()
  {
    if (!this._matchedSelectorViews) {
      this._matchedSelectorViews = [];
      this.propertyInfo.matchedSelectors.forEach(
        function matchedSelectorViews_convert(aSelectorInfo) {
          this._matchedSelectorViews.push(new SelectorView(this.tree, aSelectorInfo));
        }, this);
    }

    return this._matchedSelectorViews;
  },

    /**
   * Provide access to the unmatched SelectorViews that we are currently
   * displaying.
   */
  get unmatchedSelectorViews()
  {
    if (!this._unmatchedSelectorViews) {
      this._unmatchedSelectorViews = [];
      this.propertyInfo.unmatchedSelectors.forEach(
        function unmatchedSelectorViews_convert(aSelectorInfo) {
          this._unmatchedSelectorViews.push(new SelectorView(this.tree, aSelectorInfo));
        }, this);
    }

    return this._unmatchedSelectorViews;
  },

  /**
   * The action when a user expands matched selectors.
   *
   * @param {Event} aEvent Used to determine the class name of the targets click
   * event. If the class name is "helplink" then the event is allowed to bubble
   * to the mdn link icon.
   */
  propertyHeaderClick: function PropertyView_propertyHeaderClick(aEvent)
  {
    if (aEvent.target.className != "helplink") {
      this.matchedExpanded = !this.matchedExpanded;
      this.refreshAllSelectors();
      aEvent.preventDefault();
    }
  },

  /**
   * The action when a user expands unmatched selectors.
   */
  unmatchedSelectorsClick: function PropertyView_unmatchedSelectorsClick(aEvent)
  {
    this.unmatchedExpanded = !this.unmatchedExpanded;
    this.refreshUnmatchedSelectors();
    aEvent.preventDefault();
  },

  /**
   * The action when a user clicks on the MDN help link for a property.
   */
  mdnLinkClick: function PropertyView_mdnLinkClick(aEvent)
  {
    this.tree.win.openUILinkIn(this.link, "tab");
    aEvent.preventDefault();
  },
};

/**
 * A container to view us easy access to display data from a CssRule
 * @param CssHtmlTree aTree, the owning CssHtmlTree
 * @param aSelectorInfo
 */
function SelectorView(aTree, aSelectorInfo)
{
  this.tree = aTree;
  this.selectorInfo = aSelectorInfo;
  this._cacheStatusNames();
}

/**
 * Decode for cssInfo.rule.status
 * @see SelectorView.prototype._cacheStatusNames
 * @see CssLogic.STATUS
 */
SelectorView.STATUS_NAMES = [
  // "Unmatched", "Parent Match", "Matched", "Best Match"
];

SelectorView.CLASS_NAMES = [
  "unmatched", "parentmatch", "matched", "bestmatch"
];

SelectorView.prototype = {
  /**
   * Cache localized status names.
   *
   * These statuses are localized inside the styleinspector.properties string
   * bundle.
   * @see CssLogic.jsm - the CssLogic.STATUS array.
   *
   * @return {void}
   */
  _cacheStatusNames: function SelectorView_cacheStatusNames()
  {
    if (SelectorView.STATUS_NAMES.length) {
      return;
    }

    for (let status in CssLogic.STATUS) {
      let i = CssLogic.STATUS[status];
      if (i > -1) {
        let value = CssHtmlTree.l10n("rule.status." + status);
        // Replace normal spaces with non-breaking spaces
        SelectorView.STATUS_NAMES[i] = value.replace(/ /g, '\u00A0');
      }
    }
  },

  /**
   * A localized version of cssRule.status
   */
  get statusText()
  {
    return SelectorView.STATUS_NAMES[this.selectorInfo.status];
  },

  /**
   * Get class name for selector depending on status
   */
  get statusClass()
  {
    return SelectorView.CLASS_NAMES[this.selectorInfo.status];
  },

  /**
   * A localized Get localized human readable info
   */
  humanReadableText: function SelectorView_humanReadableText(aElement)
  {
    if (this.tree.getRTLAttr == "rtl") {
      return this.selectorInfo.value + " \u2190 " + this.text(aElement);
    } else {
      return this.text(aElement) + " \u2192 " + this.selectorInfo.value;
    }
  },

  text: function SelectorView_text(aElement) {
    let result = this.selectorInfo.selector.text;
    if (this.selectorInfo.elementStyle) {
      let source = this.selectorInfo.sourceElement;
      let IUI = this.tree.styleInspector.IUI;
      if (IUI && IUI.selection == source) {
        result = "this";
      } else {
        result = CssLogic.getShortName(source);
      }

      aElement.parentNode.querySelector(".rule-link > a").
        addEventListener("click", function(aEvent) {
          this.tree.styleInspector.selectFromPath(source);
          aEvent.preventDefault();
        }.bind(this), false);
      result += ".style";
    }

    return result;
  },
};
