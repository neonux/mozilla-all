/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */

/*
 * This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/.
 */

/**
 * For the purposes of this test, flexbox items are specified as a hash with
 * an entry for each property that is to be set.  In these per-property
 * entries, the key is the property-name, and the value can be any of the
 * following:
 *  (a) the value string, if no verification is to be done for the property.
 *  (b) an array with 2 entries: [value, expectedComputedValue] if the
 *      property's computed value is to be verified. The first entry may be
 *      null; this means that no value will be explicitly set for this property.
 *  (c) an array with 3 entries: [value, expectedComputedValue, null] if the
 *      property is known to compute to the wrong value. This is a 'todo' form
 *      of option (b).
 */

var gDefaultFlexboxSize = "200px";
var gFlexboxTestcases =
[
 // No flex properties specified --> should just use 'width'
 {
   items:
     [
       { "width":            [ "40px", "40px" ] },
       { "width":            [ "65px", "65px" ] },
     ]
 },
 // flex-basis is specified:
 {
   items:
     [
       { "-moz-flex-basis": "50px",
         "width":            [ null,  "50px" ]
       },
       {
         "-moz-flex-basis": "20px",
         "width":            [ null, "20px" ] },
     ]
 },
 // flex-basis is *large* (w/ 0 flex-shrink so we don't shrink):
 {
   items:
     [
       {
         "-moz-flex": "0 0 150px",
         "width":            [ null, "150px" ]
       },
       {
         "-moz-flex": "0 0 90px",
         "width":            [ null, "90px" ] },
     ]
 },
 // flex-basis has percentage value:
 {
   items:
     [
       {
         "-moz-flex-basis": "30%",
         "width":            [ null, "60px" ]
       },
       {
         "-moz-flex-basis": "45%",
         "width":            [ null, "90px" ]
       },
     ]
 },
 // flex-basis has calc(percentage) value:
 {
   items:
     [
       {
         "-moz-flex-basis": "-moz-calc(20%)",
         "width":            [ null, "40px" ]
       },
       {
         "-moz-flex-basis": "-moz-calc(80%)",
         "width":            [ null, "160px" ]
       },
     ]
 },
 // flex-basis has calc(percentage +/- length) value:
 {
   items:
     [
       {
         "-moz-flex-basis": "-moz-calc(10px + 20%)",
         "width":            [ null, "50px" ]
       },
       {
         "-moz-flex-basis": "-moz-calc(60% - 1px)",
         "width":            [ null, "119px" ]
       },
     ]
 },
 // flex-grow is specified:
 {
   items:
     [
       {
         "-moz-flex": "1",
         "width":            [ null,  "60px" ]
       },
       {
         "-moz-flex": "2",
         "width":            [ null, "120px" ] },
       {
         "-moz-flex": "0 0 20px",
         "width":            [ null, "20px" ] }
     ]
 },
 // Same ratio as prev. testcase; making sure we handle float inaccuracy
 {
   items:
     [
       {
         "-moz-flex": "100000",
         "width": [ null,  "60px" ] },
       {
         "-moz-flex": "200000",
         "width": [ null, "120px" ] },
       {
         "-moz-flex": "0.000001 20px",
         "width": [ null,  "20px" ] }
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "auto",
         "width": [ null, "45px" ] },
       {
         "-moz-flex": "2",
         "width": [ null, "90px" ] },
       {
         "-moz-flex": "20px 1 0",
         "width": [ null, "65px" ] }
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "none",
         "width": [ "20px", "20px" ]
       },
       {
         "-moz-flex": "1",
         "width": [ null,   "60px" ] },
       {
         "-moz-flex": "2",
         "width": [ null,  "120px" ] }
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "0 0 20px",
         "width": [ null,  "20px" ] },
       {
         "-moz-flex": "1",
         "width": [ null,  "60px" ] },
       {
         "-moz-flex": "2",
         "width": [ null, "120px" ] }
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "20px",
         "width": [ null, "65px" ] },
       {
         "-moz-flex": "1",
         "width": [ null, "45px" ] },
       {
         "-moz-flex": "2",
         "width": [ null, "90px" ] }
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "2",
         "width": [ null,  "100px" ],
         "border": "0px dashed",
         "border-left-width":  [ "5px",  "5px" ],
         "border-right-width": [ "15px", "15px" ],
         "margin-left": [ "22px", "22px" ],
         "margin-right": [ "8px", "8px" ]
       },
       {
         "-moz-flex": "1",
         "width": [ null,  "50px" ],
         "margin-left":   [ "auto", "0px" ],
         "padding-right": [ "auto", "0px" ],
       }
     ]
 },
 // Test negative flexibility:
 {
   items:
     [
       {
         "-moz-flex": "4 2 50px",
         "width": [ null,  "30px" ] },
       {
         "-moz-flex": "5 3 50px",
         "width": [ null,  "20px" ] },
       {
         "-moz-flex": "0 0 150px",
         "width": [ null, "150px" ] }
     ]
 },
 // Same ratio as prev. testcase; making sure we handle float inaccuracy
 {
   items:
     [
       { "-moz-flex": "4 1 250px",
         "width": [ null,  "200px" ] },
     ],
 },
 {
   items:
     [
       {
         "-moz-flex": "4 20000000 50px",
         "width": [ null,  "30px" ] },
       {
         "-moz-flex": "5 30000000 50px",
         "width": [ null,  "20px" ] },
       {
         "-moz-flex": "0 0.0000001 150px",
         "width": [ null, "150px" ] }
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "4 1 100px",
         "width": [ null,  "80px" ]
       },
       {
         "-moz-flex": "5 1 50px",
         "width": [ null,  "40px" ]
       },
       {
         "-moz-flex": "0 1 100px",
         "width": [ null, "80px" ]
       }
     ]
 },
 {
   items:
     [
       { "width": [ "80px",   "40px" ] },
       { "width": [ "40px",   "20px" ] },
       { "width": [ "30px",   "15px" ] },
       { "width": [ "250px", "125px" ] },
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "4 2 115px",
         "width": [ null,  "69px" ]
       },
       {
         "-moz-flex": "5 1 150px",
         "width": [ null,  "120px" ]
       },
       {
         "-moz-flex": "1 4 30px",
         "width": [ null,  "6px" ]
       },
       {
         "-moz-flex": "1 0 5px",
         "width": [ null, "5px" ]
       },
     ]
 },

 // Test min-width (clamping the effects of negative flexibility on one item):
 {
   items:
     [
       {
         "-moz-flex": "4 5 75px",
         "min-width": "50px",
         "width": [ null,  "50px" ],
       },
       {
         "-moz-flex": "5 5 100px",
         "width": [ null,  "62.5px" ]
       },
       {
         "-moz-flex": "0 4 125px",
         "width": [ null, "87.5px" ]
       }
     ]
 },

 // Test min-width much larger than initial preferred size -- enough so
 // that even our positively-flexed size is less than min-width:
 {
   items:
     [
       {
         "-moz-flex": "auto",
         "width": [ "50px",  "150px" ],
         "min-width": [ "150px", "150px" ],
       },
       {
         "-moz-flex": "auto",
         "width": [ null, "50px" ] }
     ]
 },

 // Test min-width much larger than initial preferred size, but small enough
 // that our flexed size pushes us over it:
 {
   items:
     [
       {
         "-moz-flex": "auto",
         "width": [ "50px",  "125px" ],
         "min-width": [ "110px", "110px" ],
       },
       {
         "-moz-flex": "auto",
         "width": [ null, "75px" ] }
     ]
 },

 // Test min-width on multiple items simultaneously:
 {
   items:
     [
       {
         "-moz-flex": "auto",
         "min-width": [ "20px", "20px" ],
       },
       {
         "-moz-flex": "9 auto",
         "width": [ "50px",  "180px" ],
         "min-width": [ "150px", "150px" ],
       },
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "1 1 0px",
         "width": [ null, "90px" ],
         "min-width": [ "90px", "90px" ],
       },
       {
         "-moz-flex": "1 1 0px",
         "width": [ null, "80px" ],
         "min-width": [ "80px", "80px" ],
       },
       {
         "-moz-flex": "1 1 40px",
         "width": [ null, "30px" ] }
     ]
 },

 // Test a case where min-width will be violated on different items in
 // successive iterations of the "resolve the flexible lengths" loop
 {
   items:
     [
       {
         "-moz-flex": "1 2 100px",
         "width": [ null, "90px" ],
         "min-width": [ "90px", "90px" ],
       },
       {
         "-moz-flex": "1 1 100px",
         "width": [ null, "70px" ],
         "min-width": [ "70px", "70px" ],
       },
       {
         "-moz-flex": "1 1 100px",
         "width": [ null, "40px" ] }
     ]
 },

 // Test min-width violation on one item & max-width violation on another:
 {
   items:
     [
       {
         "-moz-flex": "auto",
         "width": [ null, "150px" ],
         "min-width": [ "130px", "130px" ],
       },
       {
         "-moz-flex": "auto",
         "width": [ null,  "50px" ],
         "max-width": [ "50px", "50px" ],
       },
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "auto",
         "width": [ null, "130px" ],
         "min-width": [ "130px", "130px" ],
       },
       {
         "-moz-flex": "auto",
         "width": [ null,  "70px" ],
         "max-width": [ "80px", "80px" ],
       },
     ]
 },
 {
   items:
     [
       {
         "-moz-flex": "auto",
         "width": [ null, "120px" ],
         "min-width": [ "120px", "120px" ],
       },
       {
         "-moz-flex": "auto",
         "width": [ null,  "80px" ],
         "max-width": [ "80px", "80px" ],
       },
     ]
 },
];
