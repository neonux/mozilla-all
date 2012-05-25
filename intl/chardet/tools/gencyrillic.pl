#!/usr/local/bin/perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use StatKoi '.' ;

open(FILE, "> ../src/nsCyrillicProb.h") or die "cannot open nsCyrillicDetector.h";

print FILE <<EOF;
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
 * The Original Code is is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsCyrillicDetector_h__
#define nsCyrillicDetector_h__
/*
    DO NOT EDIT THIS FILE !!!
    This file is generated by the perl script in 
    mozilla/intl/chardet/tools/gencyrillic.pl

    To ues that script, you need to grab StatKoi.pm file from
    the "Cyrillic Software Suite" written by John Neystdt.
    http://www.neystadt.org/cyrillic (You can also find it from CPAN)
 */
EOF
$table = \%Lingua::DetectCharset::StatKoi::StatsTableKoi;
print FILE "const PRUint16 gCyrillicProb[33][33] = {";
  print FILE "{ \n";
  print FILE "0,\n";
  for($j = 0xc0; $j < 0xe0; $j++)
  {
     print FILE "0, \t";
     if( 7 == ( $j % 8) )
     {
           print FILE "\n";
     }
  }
  print FILE "\n}, \n";
for($i = 0xc0; $i < 0xe0; $i++)
{
  print FILE "{ \n";
  print FILE "0,\n";
  for($j = 0xc0; $j < 0xe0; $j++)
  {
     $key = chr($i) . chr($j);
     if(exists($table->{$key}))
     {
          $v = $table->{$key};
     } else {
          $v = 0;
     }
     print FILE $v . ", \t";
     if( 7 == ( $j % 8) )
     {
           print FILE "\n";
     }
  }
  print FILE "\n}, \n";
}
print FILE "};\n";
print FILE "#endif\n";