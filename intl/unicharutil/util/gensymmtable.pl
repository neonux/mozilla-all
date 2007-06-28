#!/usr/local/bin/perl
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is IBM code.
#
# The Initial Developer of the Original Code is
# IBM. Portions created by IBM are Copyright (C) International Business Machines Corporation, 2000.  All Rights Reserved.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Simon Montagu
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# This program generates the header file symmtable.h from the Unicode
# informative data file BidiMirroring.txt.
# See the comments in that file for details of its structure and contents.
#
# At the moment we only handle cases where there is another Unicode
# character whose glyph can serve as at least an adequate version of
# the mirror image of the original character's glyph. This leaves open
# the problem of how to provide mirrored glyphs for characters where
# this is not the case.

# Process the input file
$ucp = "[0-9a-fA-F]{4}";               # Unicode code point (4 successive hex digits) as a pattern to match
open ( UNICODATA , "< BidiMirroring.txt") 
   || die "Cannot find BidiMirroring.txt.\
The file should be avaiable here:\
http://www.unicode.org/Public/UNIDATA/BidiMirroring.txt\n";

while (<UNICODATA>) {
	chop;
  if (/^($ucp); ($ucp) # (.+)/) {      # If the line looks like this pattern
                                       # (example: 0028; 0029 # LEFT PARENTHESIS)
    if (hex($1) > 0xFFFF) {
      printf "ALERT! %X There are now symmetric characters outside the BMP\n", $1;
    }
    @table[hex($1)]=hex($2);           # Enter the symmetric pair in the table
    @isblock[hex(substr($1, 0, 2))]=1; # Remember this block
  }
}
close(UNICODATA);

# Generate license and header
open ( OUT , "> ../base/symmtable.h") 
  || die "cannot open output ../base/src/symmtable.h file";
$npl = <<END_OF_NPL;
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is IBM code.
 *
 * The Initial Developer of the Original Code is
 * International Business Machines Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
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
 
/* 
    DO NOT EDIT THIS DOCUMENT !!! THIS DOCUMENT IS GENERATED BY
    mozilla/layout/tools/gensymmtable.pl
 */
END_OF_NPL
print OUT $npl;

# Generate data tables
$indexed_blocks = 0;
printf OUT "\n/* Indexes to symmtable by Unicode block */\n";
printf OUT "const static PRUint8 symmtable_index[256] = {\n";
print  OUT "/*       ";
foreach $byte (0 .. 0xf) {
  printf OUT "_%X ", $byte;
}
print OUT "*/\n";
foreach $row (0 .. 0xf) {
  printf OUT "/* %X_ */ ", $row;
  foreach $byte (0 .. 0xf) {
    if (@isblock[($row << 4) | ($byte)]) {
      printf OUT " %d,", ++$indexed_blocks;
    } else {
      printf OUT " 0,";
    }
  }
  print OUT "\n";
}
print OUT "};\n";

printf OUT "const static PRUint16 symmtable[%d] [256] = {\n", ($indexed_blocks);
foreach $block (0 .. 0xff) {
  if (@isblock[$block]) {
    if ($block != 0) {
      printf OUT ",\n"
    }
    printf OUT " {\n/* Block U%02X__ */\n", $block;
    print OUT "/*      ";
    foreach $byte (0 .. 0xf) {
      printf OUT "     _%X ", $byte;
    }
    print OUT "*/\n";
    foreach $row (0 .. 0xf) {
      printf OUT "/* %X_ */ ", $row;
      foreach $byte (0 .. 0xf) {
         $ix = ($block << 8) | ($row << 4) | ($byte);
         printf OUT ("%#6x", (@table[$ix]) ? @table[$ix] : $ix);
	 if ((($row << 4) | $byte) < 0xff) {
	   print OUT ", ";
	 }
      }
      print OUT "\n";
    }
    print OUT " }";
  }
}
print OUT "\n};\n";

# Generate conversion method
print OUT "\nstatic PRUint32 Mirrored(PRUint32 u)\n{\n";
print OUT "  if (u < 0x10000) {\n";
print OUT "    PRUint8 index = symmtable_index[(u & 0xFFFFFF00) >> 8];\n";
print OUT "    if (index) {\n";
print OUT "      return symmtable[index - 1] [u & 0xFF];\n";
print OUT "    }\n";
print OUT "  }\n  return u;\n}\n";
close(OUT);
