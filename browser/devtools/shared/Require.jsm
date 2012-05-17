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
 * The Original Code is Dryice.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Walker <jwalker@mozilla.com> (Original Author)
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

/**
 * Require.jsm is a small module loader that loads JavaScript modules as
 * defined by AMD/RequireJS and CommonJS, or specifically as used by:
 * GCLI, Orion, Firebug, CCDump, NetPanel/HTTPMonitor and others.
 *
 * To date, no attempt has been made to ensure that Require.jsm closely follows
 * either the AMD or CommonJS specs. It is hoped that a more formal JavaScript
 * module standard will arrive before this is necessary. In the mean time it
 * serves the projects it loads.
 */

const EXPORTED_SYMBOLS = [ "define", "require" ];

const console = (function() {
  const tempScope = {};
  Components.utils.import("resource:///modules/devtools/Console.jsm", tempScope);
  return tempScope.console;
})();

/**
 * Define a module along with a payload.
 * @param moduleName Name for the payload
 * @param deps Ignored. For compatibility with CommonJS AMD Spec
 * @param payload Function with (require, exports, module) params
 */
function define(moduleName, deps, payload) {
  if (typeof moduleName != "string") {
    console.error(this.depth + " Error: Module name is not a string.");
    console.trace();
    return;
  }

  if (arguments.length == 2) {
    payload = deps;
  }
  else {
    payload.deps = deps;
  }

  if (define.debugDependencies) {
    console.log("define: " + moduleName + " -> " + payload.toString()
        .slice(0, 40).replace(/\n/, '\\n').replace(/\r/, '\\r') + "...");
  }

  if (moduleName in define.modules) {
    console.error(this.depth + " Error: Redefining module: " + moduleName);
  }
  define.modules[moduleName] = payload;
}

/**
 * The global store of un-instantiated modules
 */
define.modules = {};

/**
 * Should we console.log on module definition/instantiation/requirement?
 */
define.debugDependencies = false;


/**
 * Self executing function in which Domain is defined, and attached to define
 */
var Syntax = {
  COMMON_JS: 'commonjs',
  AMD: 'amd'
};

/**
 * We invoke require() in the context of a Domain so we can have multiple
 * sets of modules running separate from each other.
 * This contrasts with JSMs which are singletons, Domains allows us to
 * optionally load a CommonJS module twice with separate data each time.
 * Perhaps you want 2 command lines with a different set of commands in each,
 * for example.
 */
function Domain() {
  this.modules = {};
  this.syntax = Syntax.COMMON_JS;

  if (define.debugDependencies) {
    this.depth = "";
  }
}

/**
 * Lookup module names and resolve them by calling the definition function if
 * needed.
 * There are 2 ways to call this, either with an array of dependencies and a
 * callback to call when the dependencies are found (which can happen
 * asynchronously in an in-page context) or with a single string an no
 * callback where the dependency is resolved synchronously and returned.
 * The API is designed to be compatible with the CommonJS AMD spec and
 * RequireJS.
 * @param deps A name, or array of names for the payload
 * @param callback Function to call when the dependencies are resolved
 * @return The module required or undefined for array/callback method
 */
Domain.prototype.require = function(config, deps, callback) {
  if (arguments.length <= 2) {
    callback = deps;
    deps = config;
    config = undefined;
  }

  if (Array.isArray(deps)) {
    this.syntax = Syntax.AMD;
    var params = deps.map(function(dep) {
      return this.lookup(dep);
    }, this);
    if (callback) {
      callback.apply(null, params);
    }
    return undefined;
  }
  else {
    return this.lookup(deps);
  }
};

/**
 * Lookup module names and resolve them by calling the definition function if
 * needed.
 * @param moduleName A name for the payload to lookup
 * @return The module specified by aModuleName or null if not found
 */
Domain.prototype.lookup = function(moduleName) {
  if (moduleName in this.modules) {
    var module = this.modules[moduleName];
    if (define.debugDependencies) {
      console.log(this.depth + " Using module: " + moduleName);
    }
    return module;
  }

  if (!(moduleName in define.modules)) {
    console.error(this.depth + " Missing module: " + moduleName);
    return null;
  }

  var module = define.modules[moduleName];

  if (define.debugDependencies) {
    console.log(this.depth + " Compiling module: " + moduleName);
  }

  if (typeof module == "function") {
    if (define.debugDependencies) {
      this.depth += ".";
    }

    var exports;
    try {
      if (this.syntax === Syntax.COMMON_JS) {
        exports = {};
        module(this.require.bind(this), exports, { id: moduleName, uri: "" });
      }
      else {
        var modules = module.deps.map(function(dep) {
          return this.lookup(dep);
        }.bind(this));
        exports = module.apply(null, modules);
      }
    }
    catch (ex) {
      console.error("Error using module: " + moduleName, ex);
      throw ex;
    }
    module = exports;

    if (define.debugDependencies) {
      this.depth = this.depth.slice(0, -1);
    }
  }

  // cache the resulting module object for next time
  this.modules[moduleName] = module;

  return module;
};

/**
 * Expose the Domain constructor and a global domain (on the define function
 * to avoid exporting more than we need. This is a common pattern with
 * require systems)
 */
define.Domain = Domain;
define.globalDomain = new Domain();

/**
 * Expose a default require function which is the require of the global
 * sandbox to make it easy to use.
 */
const require = define.globalDomain.require.bind(define.globalDomain);
