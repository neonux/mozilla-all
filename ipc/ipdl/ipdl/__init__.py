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
# The Original Code is mozilla.org code.
#
# Contributor(s):
#   Chris Jones <jones.chris.g@gmail.com>
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

__all__ = [ 'gencxx', 'genipdl', 'parse', 'typecheck', 'writeifmodified' ]

import os, sys
from cStringIO import StringIO

from ipdl.cgen import IPDLCodeGen
from ipdl.lower import LowerToCxx
from ipdl.parser import Parser
from ipdl.type import TypeCheck

from ipdl.cxx.cgen import CxxCodeGen

def parse(specstring, filename='/stdin', includedirs=[ ]):
    return Parser().parse(specstring, os.path.abspath(filename), includedirs)

def typecheck(ast, errout=sys.stderr):
    '''Returns True iff |ast| is well typed.  Print errors to |errout| if
    it is not.'''
    return TypeCheck().check(ast, errout)

def gencxx(ast, outdir):
    for hdr in LowerToCxx().lower(ast):
        file = os.path.join(outdir,
                            *([ns.namespace for ns in ast.protocol.namespaces] + [hdr.filename]))

        tempfile = StringIO()
        CxxCodeGen(tempfile).cgen(hdr)
        writeifmodified(tempfile.getvalue(), file)

def genipdl(ast, outdir):
    return IPDLCodeGen().cgen(ast)

def writeifmodified(contents, file):
    dir = os.path.dirname(file)
    os.path.exists(dir) or os.makedirs(dir)

    oldcontents = None
    if os.path.exists(file):
        fd = open(file, 'rb')
        oldcontents = fd.read()
        fd.close()
    if oldcontents != contents:
        fd = open(file, 'wb')
        fd.write(contents)
        fd.close()
