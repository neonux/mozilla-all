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
 * The Original Code is items.js.
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
// Title: items.js 

// ##########
// Class: Item
// Superclass for all visible objects (<TabItem>s and <Group>s).
//
// If you subclass, in addition to the things Item provides, you need to also provide these methods: 
//   reloadBounds - function() 
//   setBounds - function(rect, immediately)
//   setZ - function(value)
//   close - function() 
//   addOnClose - function(referenceObject, callback)
//   removeOnClose - function(referenceObject)
//   save - function()
//
// ... and this property: 
//   defaultSize - a Point
//   locked - an object (see below)
//
// Make sure to call _init() from your subclass's constructor. 
window.Item = function() {
  // Variable: isAnItem
  // Always true for Items
  this.isAnItem = true;
  
  // Variable: bounds
  // The position and size of this Item, represented as a <Rect>. 
  this.bounds = null;
  
  // Variable: debug
  // When set to true, displays a rectangle on the screen that corresponds with bounds.
  // May be used for additional debugging features in the future.
  this.debug = false;
  
  // Variable: $debug
  // If <debug> is true, this will be the iQ object for the visible rectangle. 
  this.$debug = null;
  
  // Variable: container
  // The outermost DOM element that describes this item on screen.
  this.container = null;
  
  // Variable: locked
  // Affects whether an item can be pushed, closed, renamed, etc
  //
  // The object may have properties to specify what can't be changed: 
  //   .bounds - true if it can't be pushed, dragged, resized, etc
  //   .close - true if it can't be closed
  //   .title - true if it can't be renamed
  this.locked = null;
  
  // Variable: parent
  // The group that this item is a child of
  this.parent = null;
  
  // Variable: userSize
  // A <Point> that describes the last size specifically chosen by the user.
  // Used by unsquish.
  this.userSize = null;
  
  // Variable: dragOptions
  // Used by <draggable>
  // 
  // Possible properties:
  //   cancelClass - A space-delimited list of classes that should cancel a drag
  //   start - A function to be called when a drag starts
  //   drag - A function to be called each time the mouse moves during drag
  //   stop - A function to be called when the drag is done
  this.dragOptions = null;
  
  // Variable: dropOptions
  // Used by <draggable> if the item is set to droppable.
  // 
  // Possible properties:
  //   accept - A function to determine if a particular item should be accepted for dropping 
  //   over - A function to be called when an item is over this item
  //   out - A function to be called when an item leaves this item
  //   drop - A function to be called when an item is dropped in this item  
  this.dropOptions = null;
  
  // Variable: resizeOptions
  // Used by <resizable>
  // 
  // Possible properties:
  //   minWidth - Minimum width allowable during resize 
  //   minHeight - Minimum height allowable during resize
  //   aspectRatio - true if we should respect aspect ratio; default false
  //   start - A function to be called when resizing starts
  //   resize - A function to be called each time the mouse moves during resize
  //   stop - A function to be called when the resize is done
  this.resizeOptions = null;
  
  // Variable: isDragging
  // Boolean for whether the item is currently being dragged or not.
  this.isDragging = false;
};

window.Item.prototype = { 
  // ----------  
  // Function: _init
  // Initializes the object. To be called from the subclass's intialization function. 
  //
  // Parameters: 
  //   container - the outermost DOM element that describes this item onscreen. 
  _init: function(container) {
    Utils.assert('container must be a DOM element', Utils.isDOMElement(container));
    Utils.assert('Subclass must provide reloadBounds', typeof(this.reloadBounds) == 'function');
    Utils.assert('Subclass must provide setBounds', typeof(this.setBounds) == 'function');
    Utils.assert('Subclass must provide setZ', typeof(this.setZ) == 'function');
    Utils.assert('Subclass must provide close', typeof(this.close) == 'function');
    Utils.assert('Subclass must provide addOnClose', typeof(this.addOnClose) == 'function');
    Utils.assert('Subclass must provide removeOnClose', typeof(this.removeOnClose) == 'function');
    Utils.assert('Subclass must provide save', typeof(this.save) == 'function');
    Utils.assert('Subclass must provide defaultSize', isPoint(this.defaultSize));
    Utils.assert('Subclass must provide locked', this.locked);
    
    this.container = container;
    
    if(this.debug) {
      this.$debug = iQ('<div>')
        .css({
          border: '2px solid green',
          zIndex: -10,
          position: 'absolute'
        })
        .appendTo('body');
    }
    
    this.reloadBounds();        
    Utils.assert('reloadBounds must set up this.bounds', this.bounds);

    iQ(this.container).data('item', this);

    // ___ drag
    this.dragOptions = {
      cancelClass: 'close stackExpander',
      start: function(e, ui) {
        drag.info = new Drag(this, e);
      },
      drag: function(e, ui) {
        drag.info.drag(e, ui);
      },
      stop: function() {
        drag.info.stop();
        drag.info = null;
      }
    };
    
    // ___ drop
    this.dropOptions = {
  		over: function(){},
  		out: function(){
  			var group = drag.info.item.parent;
  			if(group) {
  				group.remove(drag.info.$el, {dontClose: true});
  			}
  				
  			iQ(this.container).removeClass("acceptsDrop");
  		},
  		drop: function(event){
  			iQ(this.container).removeClass("acceptsDrop");
  		},
  		// Function: dropAcceptFunction
  		// Given a DOM element, returns true if it should accept tabs being dropped on it.
  		// Private to this file.
  		accept: function dropAcceptFunction(item) {
				return (item && item.isATabItem && (!item.parent || !item.parent.expanded));
  		}
  	};
  	
  	// ___ resize
  	var self = this;
  	var resizeInfo = null;
    this.resizeOptions = {
      aspectRatio: self.keepProportional,
      minWidth: 90,
      minHeight: 90,
      start: function(e,ui){
      	resizeInfo = new Drag(this, e, true); // true = isResizing
      },
      resize: function(e,ui){
        resizeInfo.snap(e,ui, false, self.keepProportional);
      },
      stop: function(){
        self.setUserSize();
        self.pushAway();
        resizeInfo.stop();
        resizeInfo = null;
      } 
    };
  	
  },
  
  // ----------
  // Function: getBounds
  // Returns a copy of the Item's bounds as a <Rect>.
  getBounds: function() {
    Utils.assert('this.bounds', isRect(this.bounds));
    return new Rect(this.bounds);    
  },

  // ----------
  // Function: overlapsWithOtherItems
  // Returns true if this Item overlaps with any other Item on the screen.
  overlapsWithOtherItems: function() {
		var self = this;
		var items = Items.getTopLevelItems();
		var bounds = this.getBounds();
		return items.some(function(item) {
			if (item == self) // can't overlap with yourself.
				return false;
			var myBounds = item.getBounds();
			return myBounds.intersects(bounds);
		} );
  },
  
  // ----------
  // Function: setPosition
  // Moves the Item to the specified location. 
  // 
  // Parameters: 
  //   left - the new left coordinate relative to the window
  //   top - the new top coordinate relative to the window
  //   immediately - if false or omitted, animates to the new position;
  //   otherwise goes there immediately
  setPosition: function(left, top, immediately) {
    Utils.assert('this.bounds', isRect(this.bounds));
    this.setBounds(new Rect(left, top, this.bounds.width, this.bounds.height), immediately);
  },

  // ----------  
  // Function: setSize
  // Resizes the Item to the specified size. 
  // 
  // Parameters: 
  //   width - the new width in pixels
  //   height - the new height in pixels
  //   immediately - if false or omitted, animates to the new size;
  //   otherwise resizes immediately
  setSize: function(width, height, immediately) {
    Utils.assert('this.bounds', isRect(this.bounds));
    this.setBounds(new Rect(this.bounds.left, this.bounds.top, width, height), immediately);
  },

  // ----------
  // Function: setUserSize
  // Remembers the current size as one the user has chosen. 
  setUserSize: function() {
    Utils.assert('this.bounds', isRect(this.bounds));
    this.userSize = new Point(this.bounds.width, this.bounds.height);
    this.save();
  },
  
  // ----------
  // Function: getZ
  // Returns the zIndex of the Item.
  getZ: function() {
    return parseInt(iQ(this.container).css('zIndex'));
  },

  // ----------
  // Function: setRotation
  // Rotates the object to the given number of degrees.
  setRotation: function(degrees) {
    var value = "rotate(%deg)".replace(/%/, degrees);
    iQ(this.container).css({"-moz-transform": value});
  },
    
  // ----------
  // Function: setParent
  //
  setParent: function(parent) {
    this.parent = parent;
    this.removeTrenches();
    this.save();
  },

  // ----------  
  // Function: pushAway
  // Pushes all other items away so none overlap this Item.
  pushAway: function() {
    var buffer = Math.floor( Items.defaultGutter / 2 );
    
    var items = Items.getTopLevelItems();
		// setup each Item's pushAwayData attribute:
    iQ.each(items, function pushAway_setupPushAwayData(index, item) {
      var data = {};
      data.bounds = item.getBounds();
      data.startBounds = new Rect(data.bounds);
			// Infinity = (as yet) unaffected
      data.generation = Infinity;
      item.pushAwayData = data;
    });
    
    // The first item is a 0-generation pushed item. It all starts here.
    var itemsToPush = [this];
    this.pushAwayData.generation = 0;

    var pushOne = function(baseItem) {
    	// the baseItem is an n-generation pushed item. (n could be 0)
      var baseData = baseItem.pushAwayData;
      var bb = new Rect(baseData.bounds);

			// make the bounds larger, adding a +buffer margin to each side.
      bb.inset(-buffer, -buffer);
			// bbc = center of the base's bounds
      var bbc = bb.center();
    
      iQ.each(items, function(index, item) {
        if(item == baseItem || item.locked.bounds)
          return;
          
        var data = item.pushAwayData;
        // if the item under consideration has already been pushed, or has a lower
        // "generation" (and thus an implictly greater placement priority) then don't move it.
        if(data.generation <= baseData.generation)
          return;
        
        // box = this item's current bounds, with a +buffer margin.
        var bounds = data.bounds;
        var box = new Rect(bounds);
        box.inset(-buffer, -buffer);
        
        // if the item under consideration overlaps with the base item...
        if(box.intersects(bb)) {
        
        	// Let's push it a little.
        	
        	// First, decide in which direction and how far to push. This is the offset.
          var offset = new Point();
          // center = the current item's center.
          var center = box.center();
          
          // Consider the relationship between the current item (box) + the base item.
          // If it's more vertically stacked than "side by side"...
          if(Math.abs(center.x - bbc.x) < Math.abs(center.y - bbc.y)) {
          	// push vertically.
            if(center.y > bbc.y)
              offset.y = bb.bottom - box.top; 
            else
              offset.y = bb.top - box.bottom;
          } else { // if they're more "side by side" than stacked vertically...
          	// push horizontally.
            if(center.x > bbc.x)
              offset.x = bb.right - box.left; 
            else
              offset.x = bb.left - box.right;
          }
          
          // Actually push the Item.
          bounds.offset(offset); 
          
          // This item now becomes an (n+1)-generation pushed item.
          data.generation = baseData.generation + 1;
          // keep track of who pushed this item.
          data.pusher = baseItem;
          // add this item to the queue, so that it, in turn, can push some other things.
          itemsToPush.push(item);
        }
      });
    };   
    
    // push each of the itemsToPush, one at a time.
    // itemsToPush starts with just [this], but pushOne can add more items to the stack.
    // Maximally, this could run through all Items on the screen.
    while(itemsToPush.length)
      pushOne(itemsToPush.shift());         

    // ___ Squish!
    var pageBounds = Items.getSafeWindowBounds();
		iQ.each(items, function(index, item) {
			var data = item.pushAwayData;
			if(data.generation == 0 || item.locked.bounds)
				return;

			function apply(item, posStep, posStep2, sizeStep) {
				var data = item.pushAwayData;
				if(data.generation == 0)
					return;
					
				var bounds = data.bounds;
				bounds.width -= sizeStep.x; 
				bounds.height -= sizeStep.y;
				bounds.left += posStep.x;
				bounds.top += posStep.y;
				
				if(!item.isAGroup) {
					if(sizeStep.y > sizeStep.x) {
						var newWidth = bounds.height * (TabItems.tabWidth / TabItems.tabHeight);
						bounds.left += (bounds.width - newWidth) / 2;
						bounds.width = newWidth;
					} else {
						var newHeight = bounds.width * (TabItems.tabHeight / TabItems.tabWidth);
						bounds.top += (bounds.height - newHeight) / 2;
						bounds.height = newHeight;
					}
				}
				
				var pusher = data.pusher;
				if(pusher)  
					apply(pusher, posStep.plus(posStep2), posStep2, sizeStep);
			}

			var bounds = data.bounds;
			var posStep = new Point();
			var posStep2 = new Point();
			var sizeStep = new Point();

			if(bounds.left < pageBounds.left) {      
				posStep.x = pageBounds.left - bounds.left;
				sizeStep.x = posStep.x / data.generation;
				posStep2.x = -sizeStep.x;                
			} else if(bounds.right > pageBounds.right) {      
				posStep.x = pageBounds.right - bounds.right;
				sizeStep.x = -posStep.x / data.generation;
				posStep.x += sizeStep.x;
				posStep2.x = sizeStep.x;
			}

			if(bounds.top < pageBounds.top) {      
				posStep.y = pageBounds.top - bounds.top;
				sizeStep.y = posStep.y / data.generation;
				posStep2.y = -sizeStep.y;                
			} else if(bounds.bottom > pageBounds.bottom) {      
				posStep.y = pageBounds.bottom - bounds.bottom;
				sizeStep.y = -posStep.y / data.generation;
				posStep.y += sizeStep.y;
				posStep2.y = sizeStep.y;
			}

			if(posStep.x || posStep.y || sizeStep.x || sizeStep.y) 
				apply(item, posStep, posStep2, sizeStep);
		});

    // ___ Unsquish
    var pairs = [];
    iQ.each(items, function(index, item) {
      var data = item.pushAwayData;
      pairs.push({
        item: item,
        bounds: data.bounds
      });
    });
    
    Items.unsquish(pairs);

    // ___ Apply changes
    iQ.each(items, function(index, item) {
      var data = item.pushAwayData;
      var bounds = data.bounds;
      if(!bounds.equals(data.startBounds)) {
        item.setBounds(bounds);
      }
    });
  },
  
  // ----------  
  // Function: _updateDebugBounds
  // Called by a subclass when its bounds change, to update the debugging rectangles on screen.
  // This functionality is enabled only by the debug property.
  _updateDebugBounds: function() {
    if(this.$debug) {
      this.$debug.css({
        left: this.bounds.left,
        top: this.bounds.top,
        width: this.bounds.width,
        height: this.bounds.height
      });
    }
  },
  
  setTrenches: function(rect) {

		if (this.parent !== null)
			return;

		var container = this.container;

		if (!this.borderTrenches) {
			var bT = this.borderTrenches = {};
			bT.left = Trenches.register(container,"x","border","left");
			bT.right = Trenches.register(container,"x","border","right");
			bT.top = Trenches.register(container,"y","border","top");
			bT.bottom = Trenches.register(container,"y","border","bottom");
		}
		var bT = this.borderTrenches;
		Trenches.getById(bT.left).setWithRect(rect);
		Trenches.getById(bT.right).setWithRect(rect);
		Trenches.getById(bT.top).setWithRect(rect);
		Trenches.getById(bT.bottom).setWithRect(rect);
				
		if (!this.guideTrenches) {
			var gT = this.guideTrenches = {};
			gT.left = Trenches.register(container,"x","guide","left");
			gT.right = Trenches.register(container,"x","guide","right");
			gT.top = Trenches.register(container,"y","guide","top");
			gT.bottom = Trenches.register(container,"y","guide","bottom");
		}
		var gT = this.guideTrenches;
		Trenches.getById(gT.left).setWithRect(rect);
		Trenches.getById(gT.right).setWithRect(rect);
		Trenches.getById(gT.top).setWithRect(rect);
		Trenches.getById(gT.bottom).setWithRect(rect);

  },
  removeTrenches: function() {
		for (let edge in this.borderTrenches) {
			Trenches.unregister(this.borderTrenches[edge]); // unregister can take an array
		}
		this.borderTrenches = null;
		for (let edge in this.guideTrenches) {
			Trenches.unregister(this.guideTrenches[edge]); // unregister can take an array
		}
		this.guideTrenches = null;
  },
  
  // ----------
  // Function: draggable
  // Enables dragging on this item. Note: not to be called multiple times on the same item!
  draggable: function() {
    try {
      Utils.assert('dragOptions', this.dragOptions);
        
      var cancelClasses = [];
      if(typeof(this.dragOptions.cancelClass) == 'string')
        cancelClasses = this.dragOptions.cancelClass.split(' ');
        
      var self = this;
      var $container = iQ(this.container);
      var startMouse;
      var startPos;
      var startSent;
      var startEvent;
      var droppables;
      var dropTarget;
      
      // ___ mousemove
      var handleMouseMove = function(e) {
        // positioning 
        var mouse = new Point(e.pageX, e.pageY);
        var box = self.getBounds();
        box.left = startPos.x + (mouse.x - startMouse.x);
        box.top = startPos.y + (mouse.y - startMouse.y);
        
        self.setBounds(box, true);

        // drag events
        if(!startSent) {
          if(iQ.isFunction(self.dragOptions.start)) {
            self.dragOptions.start.apply(self, 
                [startEvent, {position: {left: startPos.x, top: startPos.y}}]);
          }
          
          startSent = true;
        }

        if(iQ.isFunction(self.dragOptions.drag))
          self.dragOptions.drag.apply(self, [e, {position: box.position()}]);
          
        // drop events
        var best = {
          dropTarget: null,
          score: 0
        };
        
        iQ.each(droppables, function(index, droppable) {
          var intersection = box.intersection(droppable.bounds);
          if(intersection && intersection.area() > best.score) {
            var possibleDropTarget = droppable.item;
            var accept = true;
            if(possibleDropTarget != dropTarget) {
              var dropOptions = possibleDropTarget.dropOptions;
              if(dropOptions && iQ.isFunction(dropOptions.accept))
                accept = dropOptions.accept.apply(possibleDropTarget, [self]);
            }
            
            if(accept) {
              best.dropTarget = possibleDropTarget;
              best.score = intersection.area();
            }
          }
        });

        if(best.dropTarget != dropTarget) {
          var dropOptions;
          if(dropTarget) {
            dropOptions = dropTarget.dropOptions;
            if(dropOptions && iQ.isFunction(dropOptions.out))
              dropOptions.out.apply(dropTarget, [e]);
          }
          
          dropTarget = best.dropTarget; 

          if(dropTarget) {
            dropOptions = dropTarget.dropOptions;
            if(dropOptions && iQ.isFunction(dropOptions.over))
              dropOptions.over.apply(dropTarget, [e]);
          }
        }
          
        e.preventDefault();
      };
        
      // ___ mouseup
      var handleMouseUp = function(e) {
        iQ(window)
          .unbind('mousemove', handleMouseMove)
          .unbind('mouseup', handleMouseUp);
          
        if(dropTarget) {
          var dropOptions = dropTarget.dropOptions;
          if(dropOptions && iQ.isFunction(dropOptions.drop))
            dropOptions.drop.apply(dropTarget, [e]);
        }

        if(startSent && iQ.isFunction(self.dragOptions.stop))
          self.dragOptions.stop.apply(self, [e]);
          
        e.preventDefault();    
      };
      
      // ___ mousedown
      $container.mousedown(function(e) {
        if(Utils.isRightClick(e))
          return;
        
        var cancel = false;
        var $target = iQ(e.target);
        iQ.each(cancelClasses, function(index, class) {
          if($target.hasClass(class)) {
            cancel = true;
            return false;
          }
        });
        
        if(cancel) {
          e.preventDefault();
          return;
        }
          
        startMouse = new Point(e.pageX, e.pageY);
        startPos = self.getBounds().position();
        startEvent = e;
        startSent = false;
        dropTarget = null;
        
        droppables = [];
        iQ('.iq-droppable').each(function() {
          if(this != self.container) {
            var item = Items.item(this);
            droppables.push({
              item: item, 
              bounds: item.getBounds()
            });
          }
        });

        iQ(window)
          .mousemove(handleMouseMove)
          .mouseup(handleMouseUp);          
                    
        e.preventDefault();
      });
    } catch(e) {
      Utils.log(e);
    }  
  },

  // ----------
  // Function: droppable
  // Enables or disables dropping on this item.
  droppable: function(value) {
    try {
      var $container = iQ(this.container);
      if(value)
        $container.addClass('iq-droppable');
      else {
        Utils.assert('dropOptions', this.dropOptions);
        
        $container.removeClass('iq-droppable');
      }
    } catch(e) {
      Utils.log(e);
    }
  },
  
  // ----------
  // Function: resizable
  // Enables or disables resizing of this item.
  resizable: function(value) {
    try {
      var $container = iQ(this.container);
      iQ('.iq-resizable-handle', $container).remove();

      if(!value) {
        $container.removeClass('iq-resizable');
      } else {
        Utils.assert('resizeOptions', this.resizeOptions);
        
        $container.addClass('iq-resizable');

        var self = this;
        var startMouse;
        var startSize;
        
        // ___ mousemove
        var handleMouseMove = function(e) {
          var mouse = new Point(e.pageX, e.pageY);
          var box = self.getBounds();
          box.width = Math.max(self.resizeOptions.minWidth || 0, startSize.x + (mouse.x - startMouse.x));
          box.height = Math.max(self.resizeOptions.minHeight || 0, startSize.y + (mouse.y - startMouse.y));

          if(self.resizeOptions.aspectRatio) {
            if(startAspect < 1)
              box.height = box.width * startAspect;
            else
              box.width = box.height / startAspect;
          }
                        
          self.setBounds(box, true);
  
          if(iQ.isFunction(self.resizeOptions.resize))
            self.resizeOptions.resize.apply(self, [e]);
            
          e.preventDefault();
          e.stopPropagation();
        };
          
        // ___ mouseup
        var handleMouseUp = function(e) {
          iQ(window)
            .unbind('mousemove', handleMouseMove)
            .unbind('mouseup', handleMouseUp);
            
          if(iQ.isFunction(self.resizeOptions.stop))
            self.resizeOptions.stop.apply(self, [e]);
            
          e.preventDefault();    
          e.stopPropagation();
        };
        
        // ___ handle + mousedown
        iQ('<div>')
          .addClass('iq-resizable-handle iq-resizable-se')
          .appendTo($container)
          .mousedown(function(e) {
            if(Utils.isRightClick(e))
              return;
            
            startMouse = new Point(e.pageX, e.pageY);
            startSize = self.getBounds().size();
            startAspect = startSize.y / startSize.x;
            
						if(iQ.isFunction(self.resizeOptions.start))
							self.resizeOptions.start.apply(self, [e]);
            
            iQ(window)
              .mousemove(handleMouseMove)
              .mouseup(handleMouseUp);          
                        
            e.preventDefault();
            e.stopPropagation();
          });
        }
    } catch(e) {
      Utils.log(e);
    }
  }
};  

// ##########
// Class: Items
// Keeps track of all Items. 
window.Items = {
  // ----------
  // Variable: defaultGutter
  // How far apart Items should be from each other and from bounds
  defaultGutter: 15,
  
  // ----------
  // Function: init
  // Initialize the object
  init: function() {
  },
  
  // ----------  
  // Function: item
  // Given a DOM element representing an Item, returns the Item. 
  item: function(el) {
    return iQ(el).data('item');
  },
  
  // ----------  
  // Function: getTopLevelItems
  // Returns an array of all Items not grouped into groups. 
  getTopLevelItems: function() {
    var items = [];
    
    iQ('.tab, .group').each(function() {
      var $this = iQ(this);
      var item = $this.data('item');  
      if(item && !item.parent && !$this.hasClass('phantom'))
        items.push(item);
    });
    
    return items;
  }, 

  // ----------
  // Function: getPageBounds
  // Returns a <Rect> defining the area of the page <Item>s should stay within. 
  getPageBounds: function() {
    var top = 0;
    var bottom = TabItems.tabHeight + 10; // MAGIC NUMBER: giving room for the "new tabs" group
    var width = Math.max(100, window.innerWidth);
    var height = Math.max(100, window.innerHeight - (top + bottom));
    return new Rect(0, top, width, height);
  },
  
  // ----------
  // Function: getSafeWindowBounds
  // Returns the bounds within which it is safe to place all non-stationary <Item>s.
  getSafeWindowBounds: function( dontCountNewTabGroup ) {
    // the safe bounds that would keep it "in the window"
    var gutter = Items.defaultGutter;
    var newTabGroupBounds = Groups.getBoundsForNewTabGroup();
    // Here, I've set the top gutter separately, as the top of the window has its own
    // extra chrome which makes a large top gutter unnecessary.
    // TODO: set top gutter separately, elsewhere.
    var topGutter = 5;
    if (dontCountNewTabGroup)
			return new Rect( gutter, topGutter, window.innerWidth - 2 * gutter, window.innerHeight - gutter - topGutter );
		else
			return new Rect( gutter, topGutter, window.innerWidth - 2 * gutter, newTabGroupBounds.top -  gutter - topGutter );

  },
  
  // ----------  
  // Function: arrange
  // Arranges the given items in a grid within the given bounds, 
  // maximizing item size but maintaining standard tab aspect ratio for each
  // 
  // Parameters: 
  //   items - an array of <Item>s. Can be null if the pretend and count options are set.
  //   bounds - a <Rect> defining the space to arrange within
  //   options - an object with various properites (see below)
  //
  // Possible "options" properties: 
  //   animate - whether to animate; default: true.
  //   z - the z index to set all the items; default: don't change z.
  //   pretend - whether to collect and return the rectangle rather than moving the items; default: false
  //   count - overrides the item count for layout purposes; default: the actual item count
  //   padding - pixels between each item
  //     
  // Returns: 
  //   the list of rectangles if the pretend option is set; otherwise null
  arrange: function(items, bounds, options) {
    var animate;
    if(!options || typeof(options.animate) == 'undefined') 
      animate = true;
    else 
      animate = options.animate;

    if(typeof(options) == 'undefined')
      options = {};
    
    var rects = null;
    if(options.pretend)
      rects = [];
      
    var tabAspect = TabItems.tabHeight / TabItems.tabWidth;
    var count = options.count || (items ? items.length : 0);
    if(!count)
      return rects;
      
    var columns = 1;
    var padding = options.padding || 0;
    var yScale = 1.1; // to allow for titles
    var rows;
    var tabWidth;
    var tabHeight;
    var totalHeight;

    function figure() {
      rows = Math.ceil(count / columns);          
      tabWidth = (bounds.width - (padding * (columns - 1))) / columns;
      tabHeight = tabWidth * tabAspect; 
      totalHeight = (tabHeight * yScale * rows) + (padding * (rows - 1)); 
    } 
    
    figure();
    
    while(rows > 1 && totalHeight > bounds.height) {
      columns++; 
      figure();
    }
    
    if(rows == 1) {
      var maxWidth = Math.max(TabItems.tabWidth, bounds.width / 2);
      tabWidth = Math.min(Math.min(maxWidth, bounds.width / count), bounds.height / tabAspect);
      tabHeight = tabWidth * tabAspect;
    }
    
    var box = new Rect(bounds.left, bounds.top, tabWidth, tabHeight);
    var row = 0;
    var column = 0;
    var immediately;
    
    var a;
    for(a = 0; a < count; a++) {
/*
      if(animate == 'sometimes')
        immediately = (typeof(item.groupData.row) == 'undefined' || item.groupData.row == row);
      else
*/
        immediately = !animate;
        
      if(rects)
        rects.push(new Rect(box));
      else if(items && a < items.length) {
        var item = items[a];
        if(!item.locked.bounds) {
          item.setBounds(box, immediately);
          item.setRotation(0);
          if(options.z)
            item.setZ(options.z);
        }
      }
      
/*
      item.groupData.column = column;
      item.groupData.row = row;
*/
      
      box.left += box.width + padding;
      column++;
      if(column == columns) {
        box.left = bounds.left;
        box.top += (box.height * yScale) + padding;
        column = 0;
        row++;
      }
    }
    
    return rects;
  },
  
  // ----------
  // Function: unsquish
  // Checks to see which items can now be unsquished. 
  //
  // Parameters: 
  //   pairs - an array of objects, each with two properties: item and bounds. The bounds are 
  //     modified as appropriate, but the items are not changed. If pairs is null, the
  //     operation is performed directly on all of the top level items. 
  //   ignore - an <Item> to not include in calculations (because it's about to be closed, for instance)
  unsquish: function(pairs, ignore) {
    var pairsProvided = (pairs ? true : false);
    if(!pairsProvided) {
      var items = Items.getTopLevelItems();
      pairs = [];
      iQ.each(items, function(index, item) {
        pairs.push({
          item: item,
          bounds: item.getBounds()
        });
      });
    }
  
    var pageBounds = Items.getSafeWindowBounds();
    iQ.each(pairs, function(index, pair) {
      var item = pair.item;
      if(item.locked.bounds || item == ignore)
        return;
        
      var bounds = pair.bounds;
      var newBounds = new Rect(bounds);

      var newSize;
      if(isPoint(item.userSize)) 
        newSize = new Point(item.userSize);
      else
        newSize = new Point(TabItems.tabWidth, TabItems.tabHeight);
        
      if(item.isAGroup) {
          newBounds.width = Math.max(newBounds.width, newSize.x);
          newBounds.height = Math.max(newBounds.height, newSize.y);
      } else {
        if(bounds.width < newSize.x) {
          newBounds.width = newSize.x;
          newBounds.height = newSize.y;
        }
      }

      newBounds.left -= (newBounds.width - bounds.width) / 2;
      newBounds.top -= (newBounds.height - bounds.height) / 2;
      
      var offset = new Point();
      if(newBounds.left < pageBounds.left)
        offset.x = pageBounds.left - newBounds.left;
      else if(newBounds.right > pageBounds.right)
        offset.x = pageBounds.right - newBounds.right;

      if(newBounds.top < pageBounds.top)
        offset.y = pageBounds.top - newBounds.top;
      else if(newBounds.bottom > pageBounds.bottom)
        offset.y = pageBounds.bottom - newBounds.bottom;
        
      newBounds.offset(offset);

      if(!bounds.equals(newBounds)) {        
        var blocked = false;
        iQ.each(pairs, function(index, pair2) {
          if(pair2 == pair || pair2.item == ignore)
            return;
            
          var bounds2 = pair2.bounds;
          if(bounds2.intersects(newBounds)) {
            blocked = true;
            return false;
          }
        });
        
        if(!blocked) {
          pair.bounds.copy(newBounds);
        }
      }
    });

    if(!pairsProvided) {
      iQ.each(pairs, function(index, pair) {
        pair.item.setBounds(pair.bounds);
      });
    }
  }
};

window.Items.init();

