/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
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

/*
 * Date: 2001-07-12
 *
 * SUMMARY: Regression test for bug 90445
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=90445
 *
 * Just seeing if this script will compile without crashing.
 */
//-----------------------------------------------------------------------------
var BUGNUMBER = 90445;
var summary = 'Testing this script will compile without crashing';

printBugNumber(BUGNUMBER);
printStatus (summary);


// The big function -
function compte() {
  var mois = getValueFromOption(document.formtest.mois);
  var region = document.formtest.region.options.selectedIndex;
  var confort = document.formtest.confort.options.selectedIndex;
  var encadrement = document.formtest.encadrement.options.selectedIndex;
  var typeVillage = document.formtest.type_village.options.selectedIndex;
  var budget = document.formtest.budget.value;
  var sport1 = document.formtest.sport1.options.selectedIndex;
  var sport2 = document.formtest.sport2.options.selectedIndex;
  var sport3 = document.formtest.sport3.options.selectedIndex;
  var activite1 = document.formtest.activite1.options.selectedIndex;
  var activite2 = document.formtest.activite2.options.selectedIndex;
  var activite3 = document.formtest.activite3.options.selectedIndex;
   
  var ret = 0;
  var liste;
  var taille;
  var bl;
  var i;
  var j;
  V=[
    [1,14,19,1,3,3,3,0,[10,13,17,18,22,23,26,27,29,9],[13,17,18,20,4,5,6,7,8]],
    [1,14,18,1,1,3,3,0,[1,11,13,22,23,26,27,28,29,3,4,9],[13,17,18,20,6]],
    [1,14,19,1,3,4,3,0,[13,17,18,22,23,25,26,27,4,9],[11,17,12,2,20,3,21,9,6]],
    [1,14,19,1,1,3,3,0,[1,10,13,22,23,24,25,26,27,28,4,8,9],[13,17,6,9]],
    [1,14,18,1,3,4,3,0,[12,13,22,23,27,28,7,9],[13,17,2,20,6,7,9]],
    [1,14,19,5,4,2,3,0,[12,13,17,18,2,21,22,23,24,26,27,28,3,5,8,9],[1,10,13,17,18,19,20,5,21,7,8,9]],
    [1,14,20,6,2,2,3,0,[11,13,2,22,23,26,27,3,4,5,8,9],[13,17,18,20,6,9]],
    [1,14,19,6,3,4,3,0,[10,13,2,22,23,26,27,3,4,5,8,9],[13,17,18,19,20,21,9,6,5]],
    [1,14,19,4,2,4,3,0,[13,17,2,22,26,28,3,5,6,7,8,9],[10,13,15,17,19,2,20,3,5,6,9]],
    [1,14,19,8,4,4,3,0,[13,2,22,26,3,28,4,5,6,8,9],[14,15,17,18,19,2,20,21,9,7]],
    [1,15,18,6,1,4,3,0,[10,11,13,14,15,2,23,26,27,5,8,9,6],[13,17,2,20,6,9]],
    [1,14,19,2,3,5,3,0,[10,13,17,2,22,26,27,28,3,5,6,7,8,9],[1,10,13,15,17,18,19,20,6,7,9,22]],
    [1,15,18,6,1,3,3,0,[12,13,15,2,22,23,26,4,5,9],[13,15,6,17,2,21,9]],
    [1,19,21,1,4,4,3,0,[11,13,18,22,23,27,28,4],[17,2,20,21,9]],
    [1,14,19,4,3,3,3,0,[10,13,17,2,22,23,24,26,27,3,4,6,8,9],[10,13,17,19,20,3,5,6,7,9]],
    [1,13,19,6,3,3,3,0,[11,13,15,2,22,23,26,27,28,3,4,8,9],[1,13,17,18,20,6,9,22]],
    [1,15,19,6,1,5,3,0,[10,13,2,22,23,26,27,4,5,8,11],[10,13,17,20,5,6,9]],
    [1,15,18,6,3,2,1,0,[10,17,21,22,23,25,26,9],[13,16,17,20,21,8,9]],
    [1,14,19,8,2,2,3,0,[13,16,21,22,23,24,26,27,3,4,5,6,8,9],[15,17,20,3,6,9]],
    [1,14,19,5,3,2,3,0,[10,11,13,16,17,2,21,22,23,24,26,27,3,4,8,9],[1,10,13,17,18,19,20,5,6,7,9]],
    [1,14,19,4,4,4,3,0,[10,13,2,22,23,26,27,3,4,5,6,7,8,9],[13,14,17,19,20,21,7,9]],
    [1,14,19,1,3,2,3,0,[10,13,22,23,26,27,28,3,4,5,6,8,9],[15,17,18,2,20,6,8,9]],
    [1,14,19,6,1,5,3,0,[10,11,13,22,25,26,4,5,7,8,9],[10,13,17,2,20,5,6,9]],
    [1,14,19,6,4,4,3,0,[10,13,17,18,22,23,26,27,4,9,5],[1,13,14,17,18,20,3,21,7,8,9]],
    [1,12,20,6,3,4,2,0,[10,13,14,17,18,22,23,25,26,27,29,9],[13,14,16,17,20,3,8,7,9]],
    [1,14,19,1,3,3,3,0,[10,11,13,17,2,22,23,26,27,29,3,4,8,9],[1,10,13,14,15,17,18,20,5,21,7,8,9,25]],
    [1,14,20,1,2,5,3,0,[12,13,17,22,23,26,28,3,4,8,9,27],[10,13,17,18,20,5,6,9,22]],
    [1,14,19,1,2,3,3,0,[13,17,22,23,26,27,28,29,3,4,8,19],[13,17,18,20,6,7,8,9]],
    [1,14,18,6,1,3,3,0,[12,13,15,2,22,23,26,27,28,4,9],[13,17,6,9]],
    [1,14,19,5,3,4,3,0,[13,2,26,27,28,3,4,5,6],[1,10,13,15,17,18,2,20,3,21,7]],
    [1,14,18,6,3,3,3,0,[13,2,22,23,26,27,8,3,4,5,9],[1,13,17,18,19,2,20,6,8,9,22]],
    [1,14,19,6,4,3,2,0,[10,13,17,18,22,23,25,26,27,29],[13,14,16,17,3,7,8]],
    [1,14,18,6,3,3,3,0,[13,22,23,26,27,28,3,4,5,7,8,9],[13,17,18,2,20,3,5,6,9]],
    [1,14,20,1,3,2,3,0,[10,13,15,17,19,2,22,23,24,26,27,3,4,8,9],[13,17,18,20,6,7,8,9]],
    [1,14,20,6,3,1,3,0,[10,13,16,2,22,23,26,27,3,4,6,8,9],[13,17,20,5,6,9]],
    [1,14,19,3,3,3,3,0,[10,12,13,18,21,22,23,24,26,27,29,3,4,8,9],[1,10,17,20,6,8,9]],
    [1,14,19,2,3,1,3,0,[12,13,17,2,22,23,24,26,27,28,4,8,9,19],[1,13,17,18,19,20,6,9]],
    [1,14,19,5,4,2,3,0,[10,11,13,17,2,21,3,22,23,24,25,26,27,5,6,8,9],[13,17,20,3,21,9]],
    [1,14,19,6,2,3,3,0,[13,22,23,26,27,28,3,4,5,7,8,9],[13,17,18,20,5,6,9]],
    [1,14,20,6,3,2,3,0,[10,12,13,19,2,22,23,24,25,1,26,27,28,3,4,8,9],[10,13,17,18,20,3,5,6,7,8,9]],
    [1,14,19,5,3,4,3,0,[12,13,2,26,27,28,3,4,5,6,8,9],[10,13,17,18,2,20,3,21,7]],
    [1,14,19,6,3,4,3,0,[13,16,22,23,26,27,28,3,5,7,8,9],[10,13,17,18,19,2,20,5,6,7,8,9]],
    [1,14,19,6,3,2,3,0,[10,13,22,23,24,25,26,27,3,4,7,8,9],[1,13,17,20,5,21,9,23]],
    [1,14,18,6,2,2,3,0,[11,13,2,22,23,26,27,3,4,8,9],[1,13,14,15,17,18,19,2,20,6,7,8,5]],
    [1,14,19,6,2,3,3,0,[13,17,2,22,23,26,27,4,5,7,9],[13,14,16,17,9,22]],
    [1,14,19,8,3,2,3,0,[13,18,22,23,24,27,28,3,4,5,8,9],[15,16,17,18,19,2,20,6,9]],
    [1,14,19,1,3,4,2,0,[13,17,18,22,23,26,27,29,9,11,8],[13,17,20,3,21,7,8,9,6]],
    [1,15,18,6,4,4,1,0,[13,17,22,25,27,29],[14,16,3,7,8]],
    [1,14,18,6,3,1,3,0,[12,13,16,17,18,19,2,22,23,26,27,3,4,9],[13,17,20,3,6,9]],
    [1,14,19,6,2,3,2,0,[13,2,22,23,26,27,9],[13,16,17,9]],
    [1,14,19,8,3,4,3,0,[13,22,26,5,6,7,8,9],[13,15,17,18,19,2,20,6,9]],
    [1,14,20,1,2,2,3,0,[11,13,22,23,26,27,28,29,3,4,9],[13,17,18,20,6,9]],
    [1,14,19,6,3,2,2,0,[10,13,17,18,22,23,24,25,26,27,9],[1,13,14,16,17,20,3,6,7,8,9]],
    [1,14,18,6,3,5,3,0,[11,13,18,22,23,26,27,4,5,2],[1,10,13,17,2,20,5,6,9,22]],
    [1,14,19,1,3,4,2,0,[17,22,23,26,27,13],[13,16,17,18,2,20,11,6,7,8,9]],
    [1,15,18,6,1,2,3,0,[13,15,2,22,23,26,3,4,5,6,9],[1,13,17,20,6,9,5]],
    [1,14,20,6,3,2,3,0,[10,11,24,13,2,22,23,26,27,3,4,5,7,8,9],[1,10,13,17,18,19,2,20,5,6,7,8,9,23]],
    [1,14,19,4,4,4,3,0,[10,13,17,18,2,22,26,27,28,3,4,5,6,8,9,23],[14,15,17,19,2,20,3,21,7,9,10]],
    [1,14,19,5,4,3,3,0,[10,13,17,21,22,23,26,27,5,8,9],[10,13,17,18,19,2,20,21,7,9]],
    [1,14,19,7,4,4,3,0,[13,17,2,22,23,26,27,18,28,3,4,5,7,9],[10,13,17,18,19,2,20,3,21,9]],
    [1,12,20,6,2,2,2,0,[10,11,13,17,18,22,23,25,26,27,29,8,9],[1,13,16,17,3,6,8,9,24]],
    [1,14,18,6,3,3,1,0,[13,16,17,22,25,26,27,9],[13,16,17,20,21,8]],
    [1,14,19,6,2,5,3,0,[13,17,2,22,23,26,27,4,5,9],[10,13,17,2,20,5,6,9]],
    [1,14,19,6,2,3,3,0,[1,13,17,22,23,26,27,28,4,6,9],[13,17,18,20,6,9]],
    [1,14,19,4,3,2,3,0,[12,13,17,19,2,22,23,24,26,27,28,3,4,5,8,9],[13,15,17,19,20,21,9,5]],
    [1,14,19,5,4,2,3,0,[10,17,2,21,22,23,24,26,27,28,3,4,6,8,9],[1,10,13,14,17,19,20,3,21,7,8,9]],
    [1,14,19,3,4,3,3,0,[10,12,13,2,21,22,23,26,27,4,7,8,9],[1,13,17,20,6,8,9]],
    [1,14,19,5,3,2,1,0,[13,17,19,21,22,23,24,25,26,27,8,9],[1,14,17,3,6,8,13]],
    [1,14,19,2,3,1,3,0,[10,11,13,17,18,19,22,24,26,27,4,7,8,9],[13,17,19,20,3,6,9]],
    [1,14,19,6,2,3,3,0,[13,2,22,23,26,27,4,5,9],[1,13,17,18,20,6,9,24]],
    [1,14,18,6,2,2,3,0,[11,13,22,23,24,25,26,27,3,4,5,6,8,9],[1,13,17,20,3,5,6,9]],
    [1,14,18,1,2,2,3,0,[1,11,13,15,17,2,22,23,26,27,4,9],[1,13,17,18,20,6,9]],
    [1,14,19,2,3,4,3,0,[10,13,22,26,27,28,29,3,4,5,6,7,8,9,36],[1,13,15,17,18,19,20,5,6,7,8,9]],
    [1,14,19,6,2,1,2,0,[13,17,21,22,26,27,3,8,9],[13,17,20,6]],
    [1,15,18,6,4,3,1,0,[10,13,16,17,20,22,25,27],[16,17,3,6,7,8,9]],
    [1,14,19,4,3,5,3,0,[10,11,13,17,22,24,26,27,28,3,4,5,6,7,8,9],[10,13,15,17,2,20,3,6,7,9]],
    [1,14,20,6,3,1,2,0,[10,11,17,18,19,22,23,25,26,27,29,8,9],[1,10,13,14,16,17,3,4,6,7,8,9]],
    [1,15,18,6,3,3,1,0,[13,16,22,26,27,9,23],[13,16,17,20,21,6]],
    [1,15,18,1,3,4,3,0,[10,13,17,2,22,23,25,26,27,28,29,4,9,12],[13,17,18,20,6,9,5]],
    [1,21,25,6,2,1,4,0,[30,31,34,35],[6,3]],
    [1,21,25,6,3,4,4,0,[30,31,34,35],[6,3]],
    [1,20,25,1,3,3,3,0,[9,10,13,17,18,22,23,26,27,29],[20,18,17,13,8,7,6,5,4]],
    [1,21,25,6,2,5,4,0,[30,31,34,35],[6,3]],
    [1,21,25,6,3,3,4,0,[30,31,96,79,34,35],[6,7]],
    [1,21,25,1,3,4,3,0,[4,9,13,17,18,22,23,25,26,27],[21,20,17,12,11,9,3,2]],
    [1,21,25,6,2,3,4,0,[30,34],[6,3]],
    [1,22,25,1,3,4,3,0,[7,9,12,13,22,23,27,28],[20,17,13,9,7,6,2]],
    [1,21,25,6,3,3,4,0,[30,31,34,35],[6]],
    [1,20,25,5,4,2,3,0,[2,3,5,8,9,12,13,17,18,21,22,23,24,26,27,28],[21,20,19,18,17,13,10,9,8,7,5,1]],
    [1,20,25,8,4,4,3,0,[2,3,4,5,6,9,13,22,26,28],[21,20,19,18,17,15,9]],
    [1,24,25,6,2,2,3,0,[2,3,4,5,6,8,9,11,13,22,23,26,27,29],[20,18,17,13,9,8,7,6]],
    [1,20,25,4,2,4,3,0,[2,3,5,6,7,8,9,12,13,17,22,26,28],[20,19,17,15,13,10,9,6,5,3,2]],
    [1,20,25,2,3,5,3,0,[2,3,5,6,7,8,9,10,13,17,22,26,27,28],[20,19,18,17,13,10,9,7,6,1]],
    [1,20,25,4,3,3,3,0,[2,3,4,6,8,9,13,17,22,23,24,26,27],[20,19,17,13,10,9,7,6,5,3]],
    [1,20,25,1,3,2,3,0,[3,4,5,6,8,9,10,13,22,23,26,27,28],[20,18,17,15,9,8,6,2]],
    [1,21,25,2,3,3,4,0,[30,31,34,35],[6,3,7]],
    [1,21,25,6,3,2,4,0,[30,34],[6,3]],
    [1,20,25,5,3,2,3,0,[8,9,10,11,13,16,17,21,22,23,24,26,27],[20,19,18,17,13,10,9,7,6,5,1]],
    [1,20,25,2,2,5,4,0,[30,31,34],[6,3,7]],
    [1,20,25,4,4,4,3,0,[2,3,4,5,6,8,9,10,13,22,23,26,27],[21,20,19,17,14,13,9,7]],
    [1,24,25,6,3,3,3,0,[2,3,4,8,9,11,13,15,22,23,26,27,28],[20,18,17,13,9,6,1]],
    [1,22,25,1,4,4,3,0,[4,11,13,18,22,23,27,28],[21,20,17,9]],
    [1,21,25,6,2,1,4,0,[30,34,35],[6,3]],
    [1,20,25,6,4,4,3,0,[9,10,13,17,18,22,23,26,27],[21,20,18,17,14,13,9,8,7,3,1]],
    [1,24,25,6,3,4,2,0,[9,10,13,17,18,22,23,25,26,27,29],[20,17,16,14,13,9,8,7,3]],
    [1,20,25,1,3,3,3,0,[2,3,4,8,9,10,11,13,17,22,23,26,27,29],[21,20,18,17,15,14,13,10,9,8,7,5,1]],
    [1,20,25,1,2,3,3,0,[3,4,8,11,13,17,22,23,26,27,28,29],[20,18,17,13,9,8,7,6]],
    [1,20,25,5,3,4,3,0,[2,3,4,5,6,13,26,27,28],[21,20,18,17,15,13,10,3,2,1]],
    [1,20,25,6,4,3,2,0,[10,13,17,18,22,23,25,26,27,29],[17,16,14,13,8,7,3]],
    [1,21,25,6,2,1,4,0,[30,34,35],[6,3]],
    [1,21,25,1,3,2,3,0,[2,3,4,8,9,10,13,15,17,19,22,23,24,26,27],[20,18,17,13,8,9,7,6]],
    [1,20,25,2,3,3,3,0,[11,13,2,21,22,23,24,26,27,28,29,3,4,5,8,9],[1,13,17,18,19,2,20,3,6,7,9]],
    [1,24,25,6,3,1,3,0,[2,3,4,6,8,9,10,13,16,22,23,26,27],[20,17,13,9,6,5]],
    [1,20,25,3,3,3,3,0,[3,4,8,9,10,12,13,18,21,22,23,24,26,27,29],[20,17,10,9,8,6,1]],
    [1,20,25,2,3,1,3,0,[2,4,8,9,12,13,17,22,23,24,26,27,28],[20,19,18,17,13,9,6,1]],
    [1,20,25,5,4,2,3,0,[2,3,5,6,8,9,10,11,13,17,21,22,23,24,25,26,27],[21,20,17,13,9,3]],
    [1,24,25,6,3,2,3,0,[2,3,4,8,9,10,13,19,22,23,24,25,26,27,28],[20,18,17,13,10,9,8,7,6,5,3]],
    [1,21,25,6,2,1,4,0,[30,31,33,34],[6]],
    [1,20,25,8,3,2,3,0,[3,4,5,8,9,13,17,18,22,23,24,27,28],[20,19,18,17,15,9,6]],
    [1,21,25,6,4,4,4,0,[30,31,27,10,34],[6,3,7]],
    [1,21,25,6,4,4,4,0,[30,31,34],[6,3,7]],
    [1,20,25,1,3,4,2,0,[9,13,17,18,22,23,26,27,29],[20,17,13,9,8,7,6,3]],
    [1,20,25,7,4,4,3,0,[2,3,4,5,7,9,13,17,18,22,23,26,27,28],[21,20,19,18,17,13,10,9,3,2]],
    [1,21,25,6,4,4,4,0,[30,31,34],[6,7]],
    [1,21,25,6,3,3,4,0,[30,31,35],[6,3]],
    [1,20,25,8,3,4,3,0,[5,6,7,8,9,13,22,26,28],[20,19,18,17,15,13,9,6,2]],
    [1,21,25,1,2,2,3,0,[3,4,9,11,13,22,23,26,27,28,29],[20,18,17,13,9,6]],
    [1,20,25,6,3,2,2,0,[9,10,13,17,18,22,23,25,26,27],[20,17,16,14,13,9,8,7,6,3,1]],
    [1,24,25,6,3,2,3,0,[2,3,4,5,7,8,9,10,11,13,22,23,24,26,27],[20,19,18,17,13,10,9,8,7,6,5,2,1]],
    [1,20,25,2,3,5,3,0,[29,24,13,4,5,22,23,16,2,28,9,10],[7,13,5,1,15,3,9,6,17]],
    [1,20,25,4,3,2,3,0,[2,3,4,5,6,8,9,12,13,17,19,22,23,24,26,27,28],[20,19,17,13,9]],
    [1,20,25,5,4,3,3,0,[3,4,5,8,9,10,13,17,21,22,23,26,27],[21,20,19,18,17,13,10,9,7,2]],
    [1,20,25,4,4,4,3,0,[2,3,4,5,6,8,9,10,13,17,18,22,23,26,27,28],[21,20,19,17,15,14,9,7,3,2]],
    [1,21,25,6,3,3,4,0,[30,31,10,34],[6,3,7]],
    [1,20,25,6,2,3,3,0,[1,4,6,9,13,17,22,23,26,27,28],[20,18,17,13,9,6]],
    [1,21,25,6,2,2,2,0,[8,9,10,11,13,17,18,22,23,25,26,27,29],[17,16,13,9,8,6,3,1]],
    [1,21,25,6,3,4,4,0,[30,32,33,27,34],[6,7]],
    [1,20,25,3,4,3,3,0,[2,4,7,8,9,10,12,13,21,22,23,26,27],[20,17,13,9,8,6,1]],
    [1,20,25,6,2,3,3,0,[],[20,18,17,13,9,6,1]],
    [1,20,25,2,3,1,3,0,[4,7,8,9,10,11,13,17,18,19,22,24,26,27],[20,19,17,13,9,6,3]],
    [1,20,25,5,3,2,4,0,[30,31,96],[6,3,7]],
    [1,20,25,2,3,4,3,0,[3,4,5,6,7,8,9,10,13,22,26,27,28,29],[20,19,18,17,13,9,8,7,6,5,1]],
    [1,21,25,6,3,3,4,0,[30,31,27,10,34],[6,7]],
    [1,21,25,4,3,2,3,0,[3,4,5,6,8,9,10,11,13,18,19,22,23,26,27],[20,17,13,9]],
    [1,21,25,6,4,3,4,0,[30,31,27,10,35],[6,7]],
    [1,21,25,6,2,2,4,0,[30,34],[6,3]],
    [1,24,25,6,2,1,2,0,[3,8,9,13,17,21,22,26,27],[20,17,13,6]],
    [1,20,25,4,3,5,3,0,[3,4,5,6,7,8,9,10,11,13,17,22,24,26,27,28],[20,17,15,13,10,9,7,6,3,2]],
    [1,20,26,6,4,3,4,0,[30,31,27,10,34,35],[6,3,7]],
    [1,21,25,6,2,3,4,0,[30,33,34],[6]],
    [1,21,25,6,3,1,2,0,[8,9,10,11,17,18,19,22,23,25,26,27,29],[17,16,14,13,10,9,8,7,6,4,3,1]],
    [1,21,25,6,4,4,4,0,[30,31,10,34,35],[6,7]],
    [1,21,25,6,3,2,4,0,[30,31,33,34],[6,3]],
    [1,21,26,6,3,5,4,0,[30,31],[6,3]],
    [1,21,25,6,2,1,4,0,[30,34,35],[6,3]],
    [1,21,25,6,3,3,4,0,[30,34],[6]],
    [1,20,25,8,2,2,3,0,[3,4,5,6,8,9,13,16,21,22,23,24,26,27],[20,17,15,9,6,3]],
    [1,20,25,5,3,4,3,0,[2,3,4,5,6,8,9,12,13,26,27,28],[21,20,18,17,13,10,3,2]],
    [1,20,25,5,4,2,3,0,[3,4,6,8,9,10,17,21,22,23,24,26,27,28],[21,20,19,17,14,13,10,9,8,7,3,1]],
    [1,13,19,6,2,3,3,0,[2,3,13,22,19,8,9,12,27],[9,6,17,20]],
    [1,15,18,2,3,3,1,0,[25,24,13,16,19,8,9,10,17,22,29,23],[16,8,3,21,17,19]],
    [1,14,19,2,3,3,3,0,[13,2,21,22,23,24,26,27,28,29,3,4,5,8,9,17],[1,13,17,18,19,2,20,3,6,7,9]],
    [1,15,18,6,4,4,1,0,[29,16,17,22,27,10],[16,8,3,14,7]],
    [1,14,20,6,3,4,3,0,[3,4,2,5,13,22,23,27,8,9],[20,7,8,2,9,21,17]],
    [1,14,19,2,3,5,3,0,[29,24,13,4,5,22,23,16,2,28,9,10],[7,13,5,1,15,3,9,6,17]],
    [1,15,18,6,3,2,1,0,[25,13,22,23,17,29,16,9],[16,21,17,20]],
    [1,24,25,6,2,3,3,0,[],[]],
    [1,21,25,1,2,5,3,0,[],[]],
    [1,21,25,6,3,4,3,0,[],[]]
    ];
   
  var nbVillages = V.length;
   
  for (i=0; i<nbVillages; i++) {
    if ((((mois   == 0) && (1==V[i][0])) || ((mois >= V[i][1]) && (mois <= V[i][2]))) &&
	((region  == 0)                  || (region  == V[i][3]                    )) &&
	((confort == 0)                  || (confort == V[i][4]                    )) &&
	((encadrement == 0)                                               ||
	 ((encadrement==3)&&((V[i][5]==1)||(V[i][5]==2)||(V[i][5] == 3))) ||
	 ((encadrement==2)&&((V[i][5]==1)||(V[i][5]==2)                )) ||
	 ((encadrement==1)&&(encadrement==V[i][5])                      ) ||
	 ((encadrement>3)&&(encadrement==V[i][5])                       )) &&
	((typeVillage == 0) || (typeVillage == V[i][6]                ))   &&
	((budget == 0)      || (budget      == V[i][7]                ))) {
           
      bl = 1;
      if ((sport1 != 0) || (sport2 != 0) || (sport3 != 0)) {
	bl = 0;
	liste  = V[i][8];
	taille = liste.length;
	for (j=0; j<taille; j++) {
	  if ((sport1 == 0) || ((sport1 != 0) && (sport1 == liste[j]))) {
	    bl = 1;
	    break;
	  }
	}
	if (bl == 1) {
	  bl = 0;
	  for (j=0; j<taille; j++) {
	    if ((sport2 == 0) || ((sport2 != 0) && (sport2 == liste[j]))) {
	      bl = 1;
	      break;
	    }
	  }
	}
	if (bl == 1) {
	  bl = 0;
	  for (j=0; j<taille; j++) {
	    if ((sport3 == 0) || ((sport3 != 0) && (sport3 == liste[j]))) {
	      bl = 1;
	      break;
	    }
	  }
	}
      }
      if ((bl==1) && ((activite1 != 0) || (activite2 != 0) || (activite3 != 0))) {
	bl = 0;
	liste  = V[i][9];
	taille = liste.length;
	for (j=0; j<taille; j++) {
	  if ((activite1 == 0) || ((activite1 != 0) && (activite1 == liste[j]))) {
	    bl = 1;
	    break;
	  }
	}
	if (bl == 1) {
	  bl = 0;
	  for (j=0; j<taille; j++) {
	    if ((activite2 == 0) || ((activite2 != 0) && (activite2 == liste[j]))) {
	      bl = 1;
	      break;
	    }
	  }
	}
	if (bl == 1) {
	  bl = 0;
	  for (j=0; j<taille; j++) {
	    if ((activite3 == 0) || ((activite3 != 0) && (activite3 == liste[j]))) {
	      bl = 1;
	      break;
	    }
	  }
	}
      }
      if (1 == bl) {
	ret++;
      }
    }
  }
}

reportCompare('No Crash', 'No Crash', '');
