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
 * The Original Code is iq.js.
 *
 * The Initial Developer of the Original Code is
 * Ian Gilman <ian@iangilman.com>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Aza Raskin <aza@mozilla.com>
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 *
 * Some portions copied from:
 * jQuery JavaScript Library v1.4.2
 * http://jquery.com/
 * Copyright 2010, John Resig
 * Dual licensed under the MIT or GPL Version 2 licenses.
 * http://jquery.org/license
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

// **********
// Title: iq.js
// jQuery, hacked down to just the bits we need, with a bunch of other stuff added.

(function( window, undefined ) {

var iQ = function(selector, context) {
		// The iQ object is actually just the init constructor 'enhanced'
		return new iQ.fn.init( selector, context );
	},

	// Map over iQ in case of overwrite
	_iQ = window.iQ,

	// Use the correct document accordingly with window argument (sandbox)
	document = window.document,

	// A central reference to the root iQ(document)
	rootiQ,

	// A simple way to check for HTML strings or ID strings
	// (both of which we optimize for)
	quickExpr = /^[^<]*(<[\w\W]+>)[^>]*$|^#([\w-]+)$/,

	// Is it a simple selector
	isSimple = /^.[^:#\[\.,]*$/,

	// Check if a string has a non-whitespace character in it
	rnotwhite = /\S/,

	// Used for trimming whitespace
	rtrim = /^(\s|\u00A0)+|(\s|\u00A0)+$/g,

	// Match a standalone tag
	rsingleTag = /^<(\w+)\s*\/?>(?:<\/\1>)?$/,

	// Save a reference to some core methods
	toString = Object.prototype.toString,
	hasOwnProperty = Object.prototype.hasOwnProperty,
	push = Array.prototype.push,
	slice = Array.prototype.slice,
	indexOf = Array.prototype.indexOf;

var rclass = /[\n\t]/g,
	rspace = /\s+/,
	rreturn = /\r/g,
	rspecialurl = /href|src|style/,
	rtype = /(button|input)/i,
	rfocusable = /(button|input|object|select|textarea)/i,
	rclickable = /^(a|area)$/i,
	rradiocheck = /radio|checkbox/;

// ##########
// Class: iQ.fn
// An individual element or group of elements.
iQ.fn = iQ.prototype = {
  // ----------
  // Function: init
  // You don't call this directly; this is what's called by iQ(). 
  // It works pretty much like jQuery(), with a few exceptions, 
  // most notably that you can't use strings with complex html, 
  // just simple tags like '<div>'.
	init: function( selector, context ) {
		var match, elem, ret, doc;

		// Handle $(""), $(null), or $(undefined)
		if ( !selector ) {
			return this;
		}

		// Handle $(DOMElement)
		if ( selector.nodeType ) {
			this.context = this[0] = selector;
			this.length = 1;
			return this;
		}
		
		// The body element only exists once, optimize finding it
		if ( selector === "body" && !context ) {
			this.context = document;
			this[0] = document.body;
			this.selector = "body";
			this.length = 1;
			return this;
		}

		// Handle HTML strings
		if ( typeof selector === "string" ) {
			// Are we dealing with HTML string or an ID?
			match = quickExpr.exec( selector );

			// Verify a match, and that no context was specified for #id
			if ( match && (match[1] || !context) ) {

				// HANDLE $(html) -> $(array)
				if ( match[1] ) {
					doc = (context ? context.ownerDocument || context : document);

					// If a single string is passed in and it's a single tag
					// just do a createElement and skip the rest
					ret = rsingleTag.exec( selector );

					if ( ret ) {
						if ( iQ.isPlainObject( context ) ) {
							Utils.assert('does not support HTML creation with context', false);
						} else {
							selector = [ doc.createElement( ret[1] ) ];
						}

					} else {
							Utils.assert('does not support complex HTML creation', false);
					}
					
					return iQ.merge( this, selector );
					
				// HANDLE $("#id")
				} else {
					elem = document.getElementById( match[2] );

					if ( elem ) {
						this.length = 1;
						this[0] = elem;
					}

					this.context = document;
					this.selector = selector;
					return this;
				}

			// HANDLE $("TAG")
			} else if ( !context && /^\w+$/.test( selector ) ) {
				this.selector = selector;
				this.context = document;
				selector = document.getElementsByTagName( selector );
				return iQ.merge( this, selector );

			// HANDLE $(expr, $(...))
			} else if ( !context || context.iq ) {
				return (context || rootiQ).find( selector );

			// HANDLE $(expr, context)
			// (which is just equivalent to: $(context).find(expr)
			} else {
				return iQ( context ).find( selector );
			}

		// HANDLE $(function)
		// Shortcut for document ready
		} else if ( iQ.isFunction( selector ) ) {
			Utils.log('iQ does not support ready functions');
			return null;
		}

		if (selector.selector !== undefined) {
			this.selector = selector.selector;
			this.context = selector.context;
		}

		return iQ.makeArray( selector, this );
	},
	
	// Start with an empty selector
	selector: "",

	// The current version of iQ being used
	iq: "1.4.2",

	// The default length of a iQ object is 0
	length: 0, 
	
  // ----------
  // Function: toArray
	toArray: function() {
		return slice.call( this, 0 );
	},

  // ----------
  // Function: get
	// Get the Nth element in the matched element set OR
	// Get the whole matched element set as a clean array
	get: function( num ) {
		return num == null ?

			// Return a 'clean' array
			this.toArray() :

			// Return just the object
			( num < 0 ? this.slice(num)[ 0 ] : this[ num ] );
	},

  // ----------
  // Function: pushStack
	// Take an array of elements and push it onto the stack
	// (returning the new matched element set)
	pushStack: function( elems, name, selector ) {
		// Build a new iQ matched element set
		var ret = iQ();

		if ( iQ.isArray( elems ) ) {
			push.apply( ret, elems );
		
		} else {
			iQ.merge( ret, elems );
		}

		// Add the old object onto the stack (as a reference)
		ret.prevObject = this;

		ret.context = this.context;

		if ( name === "find" ) {
			ret.selector = this.selector + (this.selector ? " " : "") + selector;
		} else if ( name ) {
			ret.selector = this.selector + "." + name + "(" + selector + ")";
		}

		// Return the newly-formed element set
		return ret;
	},

  // ----------
  // Function: each
	// Execute a callback for every element in the matched set.
	// (You can seed the arguments with an array of args, but this is
	// only used internally.)
	each: function( callback, args ) {
		return iQ.each( this, callback, args );
	},
	
  // ----------
  // Function: slice
	slice: function() {
		return this.pushStack( slice.apply( this, arguments ),
			"slice", slice.call(arguments).join(",") );
	},

  // ----------
  // Function: addClass
	addClass: function( value ) {
		if ( iQ.isFunction(value) ) {
		  Utils.assert('does not support function argument', false);
		  return null;
		}

		if ( value && typeof value === "string" ) {
			var classNames = (value || "").split( rspace );

			for ( var i = 0, l = this.length; i < l; i++ ) {
				var elem = this[i];

				if ( elem.nodeType === 1 ) {
					if ( !elem.className ) {
						elem.className = value;

					} else {
						var className = " " + elem.className + " ", setClass = elem.className;
						for ( var c = 0, cl = classNames.length; c < cl; c++ ) {
							if ( className.indexOf( " " + classNames[c] + " " ) < 0 ) {
								setClass += " " + classNames[c];
							}
						}
						elem.className = iQ.trim( setClass );
					}
				}
			}
		}

		return this;
	},

  // ----------
  // Function: removeClass
	removeClass: function( value ) {
		if ( iQ.isFunction(value) ) {
		  Utils.assert('does not support function argument', false);
		  return null;
		}

		if ( (value && typeof value === "string") || value === undefined ) {
			var classNames = (value || "").split(rspace);

			for ( var i = 0, l = this.length; i < l; i++ ) {
				var elem = this[i];

				if ( elem.nodeType === 1 && elem.className ) {
					if ( value ) {
						var className = (" " + elem.className + " ").replace(rclass, " ");
						for ( var c = 0, cl = classNames.length; c < cl; c++ ) {
							className = className.replace(" " + classNames[c] + " ", " ");
						}
						elem.className = iQ.trim( className );

					} else {
						elem.className = "";
					}
				}
			}
		}

		return this;
	},

  // ----------
  // Function: hasClass
	hasClass: function( selector ) {
		var className = " " + selector + " ";
		for ( var i = 0, l = this.length; i < l; i++ ) {
			if ( (" " + this[i].className + " ").replace(rclass, " ").indexOf( className ) > -1 ) {
				return true;
			}
		}

		return false;
	},

  // ----------
  // Function: find
	find: function( selector ) {
		var ret = [], length = 0;

		for ( var i = 0, l = this.length; i < l; i++ ) {
			length = ret.length;
      try {
        iQ.merge(ret, this[i].querySelectorAll( selector ) );
      } catch(e) {
        Utils.log('iQ.find error (bad selector)', e);
      }

			if ( i > 0 ) {
				// Make sure that the results are unique
				for ( var n = length; n < ret.length; n++ ) {
					for ( var r = 0; r < length; r++ ) {
						if ( ret[r] === ret[n] ) {
							ret.splice(n--, 1);
							break;
						}
					}
				}
			}
		}

		return iQ(ret);
	},

  // ----------
  // Function: remove
	remove: function(unused) {
	  Utils.assert('does not accept a selector', unused === undefined);
		for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
			if ( elem.parentNode ) {
				 elem.parentNode.removeChild( elem );
			}
		}
		
		return this;
	},

  // ----------
  // Function: empty
	empty: function() {
		for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
			while ( elem.firstChild ) {
				elem.removeChild( elem.firstChild );
			}
		}
		
		return this;
	},

  // ----------
  // Function: width
	width: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    return parseInt(this.css('width')); 
  },

  // ----------
  // Function: height
	height: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    return parseInt(this.css('height'));
  },

  // ----------
  // Function: position
  position: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    return {
      left: parseInt(this.css('left')),
      top: parseInt(this.css('top'))
    };
  },
  
  // ----------
  // Function: bounds
  bounds: function(unused) {
    Utils.assert('does not yet support setting', unused === undefined);
    var p = this.position();
    return new Rect(p.left, p.top, this.width(), this.height());
  },
  
  // ----------
  // Function: data
  data: function(key, value) {
    var data = null;
    if(value === undefined) {
      Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
      data = this[0].iQData;
      return (data ? data[key] : null);
    }
    
  	for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      data = elem.iQData;

      if(!data)
        data = elem.iQData = {};
        
      data[key] = value;
    }
    
    return this;    
  },
  
  // ----------
  // Function: html
  // TODO: security
  html: function(value) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    if(value === undefined)
      return this[0].innerHTML;
      
    this[0].innerHTML = value;
    return this;
  },  
  
  // ----------
  // Function: text
  text: function(value) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    if(value === undefined) {
      return this[0].textContent;
    }
      
		return this.empty().append( (this[0] && this[0].ownerDocument || document).createTextNode(value));
  },  
  
  // ----------
  // Function: val
  val: function(value) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    if(value === undefined) {
      return this[0].value;
    }
    
    this[0].value = value;  
		return this;
  },  
  
  // ----------
  // Function: appendTo
  appendTo: function(selector) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    iQ(selector).append(this);
    return this;
  },
  
  // ----------
  // Function: append
  append: function(selector) {
    Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);
    var object = iQ(selector);
    Utils.assert('does not yet support multi-objects (or null objects)', object.length == 1);
    this[0].appendChild(object[0]);
    return this;
  },
  
  // ----------
  // Function: attr
  // Sets or gets an attribute on the element(s).
  attr: function(key, value) {
    try {
      Utils.assert('string key', typeof key === 'string');
      if(value === undefined) {
        Utils.assert('retrieval does not support multi-objects (or null objects)', this.length == 1);      
        return this[0].getAttribute(key);
      } else {
    		for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
    		  elem.setAttribute(key, value);
    		}
      }    
    } catch(e) {
      Utils.log(e);
    }
    
    return this;
  },

  // ----------
  // Function: css
  css: function(a, b) {
    var properties = null;
    
    if(typeof a === 'string') {
      var key = a; 
      if(b === undefined) {
        Utils.assert('retrieval does not support multi-objects (or null objects)', this.length == 1);      

        var substitutions = {
          'MozTransform': '-moz-transform',
          'zIndex': 'z-index'
        };

        return window.getComputedStyle(this[0], null).getPropertyValue(substitutions[key] || key);  
      } else {
        properties = {};
        properties[key] = b;
      }
    } else
      properties = a;

		var pixels = {
		  'left': true,
		  'top': true,
		  'right': true,
		  'bottom': true,
		  'width': true,
		  'height': true
		};
		
		for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      iQ.each(properties, function(key, value) {
        if(pixels[key] && typeof(value) != 'string') 
          value += 'px';
        
        if(key.indexOf('-') != -1)
          elem.style.setProperty(key, value, '');
        else
          elem.style[key] = value;
      });
    }
    
    return this; 
  },

  // ----------
  // Function: animate
  // Uses CSS transitions to animate the element. 
  // 
  // Parameters: 
  //   css - an object map of the CSS properties to change
  //   options - an object with various properites (see below)
  //
  // Possible "options" properties: 
  //   duration - how long to animate, in milliseconds
  //   easing - easing function to use. Possibilities include 'tabcandyBounce', 'easeInQuad'. 
  //     Default is 'ease'. 
  //   complete - function to call once the animation is done, takes nothing in, but "this"
  //     is set to the element that was animated. 
  animate: function(css, options) {
    try {
      Utils.assert('does not yet support multi-objects (or null objects)', this.length == 1);

      if(!options)
        options = {};
      
      var easings = {
        tabcandyBounce: 'cubic-bezier(0.0, 0.63, .6, 1.29)', 
        easeInQuad: 'ease-in', // TODO: make it a real easeInQuad, or decide we don't care
        fast: 'cubic-bezier(0.7,0,1,1)'
      };
      
      var duration = (options.duration || 400);
      var easing = (easings[options.easing] || 'ease');

      // The latest versions of Firefox do not animate from a non-explicitly set
      // css properties. So for each element to be animated, go through and
      // explicitly define 'em.
      var rupper = /([A-Z])/g;    
      this.each(function(){
        var cStyle = window.getComputedStyle(this, null);      
        for(var prop in css){
          prop = prop.replace( rupper, "-$1" ).toLowerCase();
          iQ(this).css(prop, cStyle.getPropertyValue(prop));
        }    
      });


      this.css({
        '-moz-transition-property': 'all', // TODO: just animate the properties we're changing  
        '-moz-transition-duration': (duration / 1000) + 's',  
        '-moz-transition-timing-function': easing
      });

      this.css(css);
      
      var self = this;
      iQ.timeout(function() {
        self.css({
          '-moz-transition-property': 'none',  
          '-moz-transition-duration': '',  
          '-moz-transition-timing-function': ''
        });

        if(iQ.isFunction(options.complete))
          options.complete.apply(self);
      }, duration);
    } catch(e) {
      Utils.log(e);
    }
    
    return this;
  },
    
  // ----------
  // Function: fadeOut
  fadeOut: function(callback) {
    try {
      Utils.assert('does not yet support duration', iQ.isFunction(callback) || callback === undefined);
      this.animate({
        opacity: 0
      }, {
        duration: 400,
        complete: function() {
          iQ(this).css({display: 'none'});
          if(iQ.isFunction(callback))
            callback.apply(this);
        }
      });  
    } catch(e) {
      Utils.log(e);
    }
    
    return this;
  },
    
  // ----------
  // Function: fadeIn
  fadeIn: function() {
    try {
      this.css({display: ''});
      this.animate({
        opacity: 1
      }, {
        duration: 400
      });  
    } catch(e) {
      Utils.log(e);
    }
    
    return this;
  },
    
  // ----------
  // Function: hide
  hide: function() {
    try {
      this.css({display: 'none', opacity: 0});
    } catch(e) {
      Utils.log(e);
    }
    
    return this;
  },
    
  // ----------
  // Function: show
  show: function() {
    try {
      this.css({display: '', opacity: 1});
    } catch(e) {
      Utils.log(e);
    }
    
    return this;
  },
    
  // ----------
  // Function: bind
  // Binds the given function to the given event type. Also wraps the function 
  // in a try/catch block that does a Utils.log on any errors.
  bind: function(type, func) {
    Utils.assert('does not support eventData argument', iQ.isFunction(func));

    var handler = function(event) {
      try {
        return func.apply(this, [event]);
      } catch(e) {
        Utils.log(e);
      }
    };

  	for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      if(!elem.iQEventData)
        elem.iQEventData = {};
        
      if(!elem.iQEventData[type])
        elem.iQEventData[type] = [];
        
      elem.iQEventData[type].push({
        original: func, 
        modified: handler
      });
      
      elem.addEventListener(type, handler, false);
    }
    
    return this; 
  },
  
  // ----------
  // Function: one
  one: function(type, func) {
    Utils.assert('does not support eventData argument', iQ.isFunction(func));
    
    var handler = function(e) {
      iQ(this).unbind(type, handler);
      return func.apply(this, [e]);
    };
      
    return this.bind(type, handler);
  },
  
  // ----------
  // Function: unbind
  unbind: function(type, func) {
    Utils.assert('Must provide a function', iQ.isFunction(func));
    
  	for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
      var handler = func;
      if(elem.iQEventData && elem.iQEventData[type]) {
        for(var a = 0, count = elem.iQEventData[type].length; a < count; a++) {
          var pair = elem.iQEventData[type][a];
          if(pair.original == func) {
            handler = pair.modified; 
            elem.iQEventData[type].splice(a, 1);
            break;
          }
        }
      }
      
      elem.removeEventListener(type, handler, false);
    }
    
    return this; 
  }
};

// ----------
// Give the init function the iQ prototype for later instantiation
iQ.fn.init.prototype = iQ.fn;

// ----------
// Function: extend
iQ.extend = iQ.fn.extend = function() {
	// copy reference to target object
	var target = arguments[0] || {}, i = 1, length = arguments.length, deep = false, options, name, src, copy;

	// Handle a deep copy situation
	if ( typeof target === "boolean" ) {
		deep = target;
		target = arguments[1] || {};
		// skip the boolean and the target
		i = 2;
	}

	// Handle case when target is a string or something (possible in deep copy)
	if ( typeof target !== "object" && !iQ.isFunction(target) ) {
		target = {};
	}

	// extend iQ itself if only one argument is passed
	if ( length === i ) {
		target = this;
		--i;
	}

	for ( ; i < length; i++ ) {
		// Only deal with non-null/undefined values
		if ( (options = arguments[ i ]) != null ) {
			// Extend the base object
			for ( name in options ) {
				src = target[ name ];
				copy = options[ name ];

				// Prevent never-ending loop
				if ( target === copy ) {
					continue;
				}

				// Recurse if we're merging object literal values or arrays
				if ( deep && copy && ( iQ.isPlainObject(copy) || iQ.isArray(copy) ) ) {
					var clone = src && ( iQ.isPlainObject(src) || iQ.isArray(src) ) ? src
						: iQ.isArray(copy) ? [] : {};

					// Never move original objects, clone them
					target[ name ] = iQ.extend( deep, clone, copy );

				// Don't bring in undefined values
				} else if ( copy !== undefined ) {
					target[ name ] = copy;
				}
			}
		}
	}

	// Return the modified object
	return target;
};

// ##########
// Class: iQ
// Singleton
iQ.extend({
  // ----------
  // Variable: animationCount
  // For internal use only
  animationCount: 0,
  
  // ----------
  // Function: isAnimating
  isAnimating: function() {
    return (this.animationCount != 0);
  },
  
	// -----------
	// Function: isFunction
	// See test/unit/core.js for details concerning isFunction.
	// Since version 1.3, DOM methods and functions like alert
	// aren't supported. They return false on IE (#2968).
	isFunction: function( obj ) {
		return toString.call(obj) === "[object Function]";
	},

  // ----------
  // Function: isArray
	isArray: function( obj ) {
		return toString.call(obj) === "[object Array]";
	},

  // ----------
  // Function: isPlainObject
	isPlainObject: function( obj ) {
		// Must be an Object.
		// Because of IE, we also have to check the presence of the constructor property.
		// Make sure that DOM nodes and window objects don't pass through, as well
		if ( !obj || toString.call(obj) !== "[object Object]" || obj.nodeType || obj.setInterval ) {
			return false;
		}
		
		// Not own constructor property must be Object
		if ( obj.constructor
			&& !hasOwnProperty.call(obj, "constructor")
			&& !hasOwnProperty.call(obj.constructor.prototype, "isPrototypeOf") ) {
			return false;
		}
		
		// Own properties are enumerated firstly, so to speed up,
		// if last one is own, then all properties are own.
	
		var key;
		for ( key in obj ) {}
		
		return key === undefined || hasOwnProperty.call( obj, key );
	},

  // ----------
  // Function: isEmptyObject
	isEmptyObject: function( obj ) {
		for ( var name in obj ) {
			return false;
		}
		return true;
	},

	// ----------
  // Function: each
	// args is for internal usage only
	each: function( object, callback, args ) {
		var name, i = 0,
			length = object.length,
			isObj = length === undefined || iQ.isFunction(object);

		if ( args ) {
			if ( isObj ) {
				for ( name in object ) {
					if ( callback.apply( object[ name ], args ) === false ) {
						break;
					}
				}
			} else {
				for ( ; i < length; ) {
					if ( callback.apply( object[ i++ ], args ) === false ) {
						break;
					}
				}
			}

		// A special, fast, case for the most common use of each
		} else {
			if ( isObj ) {
				for ( name in object ) {
					if ( callback.call( object[ name ], name, object[ name ] ) === false ) {
						break;
					}
				}
			} else {
				for ( var value = object[0];
					i < length && callback.call( value, i, value ) !== false; value = object[++i] ) {}
			}
		}

		return object;
	},
	
  // ----------
  // Function: trim
	trim: function( text ) {
		return (text || "").replace( rtrim, "" );
	},

  // ----------
  // Function: makeArray
	// results is for internal usage only
	makeArray: function( array, results ) {
		var ret = results || [];

		if ( array != null ) {
			// The window, strings (and functions) also have 'length'
			// The extra typeof function check is to prevent crashes
			// in Safari 2 (See: #3039)
			if ( array.length == null || typeof array === "string" || iQ.isFunction(array) || (typeof array !== "function" && array.setInterval) ) {
				push.call( ret, array );
			} else {
				iQ.merge( ret, array );
			}
		}

		return ret;
	},

  // ----------
  // Function: inArray
	inArray: function( elem, array ) {
		if ( array.indexOf ) {
			return array.indexOf( elem );
		}

		for ( var i = 0, length = array.length; i < length; i++ ) {
			if ( array[ i ] === elem ) {
				return i;
			}
		}

		return -1;
	},

  // ----------
  // Function: merge
	merge: function( first, second ) {
		var i = first.length, j = 0;

		if ( typeof second.length === "number" ) {
			for ( var l = second.length; j < l; j++ ) {
				first[ i++ ] = second[ j ];
			}
		
		} else {
			while ( second[j] !== undefined ) {
				first[ i++ ] = second[ j++ ];
			}
		}

		first.length = i;

		return first;
	},

  // ----------
  // Function: grep
	grep: function( elems, callback, inv ) {
		var ret = [];

		// Go through the array, only saving the items
		// that pass the validator function
		for ( var i = 0, length = elems.length; i < length; i++ ) {
			if ( !inv !== !callback( elems[ i ], i ) ) {
				ret.push( elems[ i ] );
			}
		}

		return ret;
	},

  // ----------
  // Function: timeout
  // wraps setTimeout with try/catch
  timeout: function(func, delay) {
    setTimeout(function() { 
      try {
        func();
      } catch(e) {
        Utils.log(e);
      }
    }, delay);
  }
});

// ----------
// Create various event aliases
(function() {
  var events = [
    'keyup',
    'keydown',
    'mouseup',
    'mousedown',
    'mouseover',
    'mouseout',
    'mousemove',
    'click',
    'resize',
    'change',
    'blur',
    'focus'
  ];
  
  iQ.each(events, function(index, event) {
    iQ.fn[event] = function(func) {
      return this.bind(event, func);
    };
  });
})();

// ----------
// All iQ objects should point back to these
rootiQ = iQ(document);

// ----------
// Expose iQ to the global object
window.iQ = iQ;

})(window);
