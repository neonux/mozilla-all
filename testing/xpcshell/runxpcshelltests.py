#!/usr/bin/env python
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is The Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Serge Gautherie <sgautherie.bz@free.fr>
#  Ted Mielczarek <ted.mielczarek@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** */

import re, sys, os, os.path, logging
import tempfile
from glob import glob
from optparse import OptionParser
from subprocess import Popen, PIPE, STDOUT

from automationutils import addCommonOptions, checkForCrashes

# Init logging
log = logging.getLogger()
handler = logging.StreamHandler(sys.stdout)
log.setLevel(logging.INFO)
log.addHandler(handler)

def readManifest(manifest):
  """Given a manifest file containing a list of test directories,
  return a list of absolute paths to the directories contained within."""
  manifestdir = os.path.dirname(manifest)
  testdirs = []
  try:
    f = open(manifest, "r")
    for line in f:
      dir = line.rstrip()
      path = os.path.join(manifestdir, dir)
      if os.path.isdir(path):
        testdirs.append(path)
    f.close()
  except:
    pass # just eat exceptions
  return testdirs

def runTests(xpcshell, testdirs=[], xrePath=None, testPath=None,
             manifest=None, interactive=False, symbolsPath=None):
  """Run the tests in |testdirs| using the |xpcshell| executable.

  |xrePath|, if provided, is the path to the XRE to use.
  |testPath|, if provided, indicates a single path and/or test to run.
  |manifest|, if provided, is a file containing a list of
    test directories to run.
  |interactive|, if set to True, indicates to provide an xpcshell prompt
    instead of automatically executing  the test.
  |symbolsPath|, if provided is the path to a directory containing
    breakpad symbols for processing crashes in tests.
  """

  if not testdirs and not manifest:
    # nothing to test!
    print >>sys.stderr, "Error: No test dirs or test manifest specified!"
    return False

  testharnessdir = os.path.dirname(os.path.abspath(__file__))
  xpcshell = os.path.abspath(xpcshell)
  # we assume that httpd.js lives in components/ relative to xpcshell
  httpdJSPath = os.path.join(os.path.dirname(xpcshell), "components", "httpd.js").replace("\\", "/");

  env = dict(os.environ)
  # Make assertions fatal
  env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
  # Don't launch the crash reporter client
  env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"

  # Enable leaks (only) detection to its own log file.
  # Each test will overwrite it.
  leakLogFile = os.path.join(tempfile.gettempdir(), "runxpcshelltests_leaks.log")
  env["XPCOM_MEM_LEAK_LOG"] = leakLogFile

  def processLeakLog(leakLogFile):
    """Process the leak log."""
    # For the time being, don't warn (nor "info") if the log file is not there. (Bug 469523)
    if not os.path.exists(leakLogFile):
      return None

    leaks = open(leakLogFile, "r")
    leakReport = leaks.read()
    leaks.close()

    # Only check whether an actual leak was reported.
    if "0 TOTAL " in leakReport:
      # For the time being, simply copy the log. (Bug 469523)
      print leakReport.rstrip("\n")

    return leakReport

  if xrePath is None:
    xrePath = os.path.dirname(xpcshell)
  else:
    xrePath = os.path.abspath(xrePath)
  if sys.platform == 'win32':
    env["PATH"] = env["PATH"] + ";" + xrePath
  elif sys.platform in ('os2emx', 'os2knix'):
    os.environ["BEGINLIBPATH"] = xrePath + ";" + env["BEGINLIBPATH"]
    os.environ["LIBPATHSTRICT"] = "T"
  elif sys.platform == 'osx':
    env["DYLD_LIBRARY_PATH"] = xrePath
  else: # unix or linux?
    env["LD_LIBRARY_PATH"] = xrePath
  args = [xpcshell, '-g', xrePath, '-j', '-s']

  headfiles = ['-f', os.path.join(testharnessdir, 'head.js'),
               '-e', 'function do_load_httpd_js() {load("%s");}' % httpdJSPath]
  tailfiles = ['-f', os.path.join(testharnessdir, 'tail.js')]
  if not interactive:
    tailfiles += ['-e', '_execute_test();']

  # |testPath| will be the optional path only, or |None|.
  # |singleFile| will be the optional test only, or |None|.
  singleFile = None
  if testPath:
    if testPath.endswith('.js'):
      # Split into path and file.
      if testPath.find('/') == -1:
        # Test only.
        singleFile = testPath
        testPath = None
      else:
        # Both path and test.
        # Reuse |testPath| temporarily.
        testPath = testPath.rsplit('/', 1)
        singleFile = testPath[1]
        testPath = testPath[0]
    else:
      # Path only.
      # Simply remove optional ending separator.
      testPath = testPath.rstrip("/")

  if manifest is not None:
    testdirs = readManifest(os.path.abspath(manifest))

  # Process each test directory individually.
  success = True
  for testdir in testdirs:
    if testPath and not testdir.endswith(testPath):
      continue

    testdir = os.path.abspath(testdir)

    # get the list of head and tail files from the directory
    testheadfiles = []
    for f in sorted(glob(os.path.join(testdir, "head_*.js"))):
      if os.path.isfile(f):
        testheadfiles += ['-f', f]
    testtailfiles = []
    for f in sorted(glob(os.path.join(testdir, "tail_*.js"))):
      if os.path.isfile(f):
        testtailfiles += ['-f', f]

    # if a single test file was specified, we only want to execute that test
    testfiles = sorted(glob(os.path.join(testdir, "test_*.js")))
    if singleFile:
      if singleFile in [os.path.basename(x) for x in testfiles]:
        testfiles = [os.path.join(testdir, singleFile)]
      else: # not in this dir? skip it
        continue

    # Now execute each test individually.
    for test in testfiles:
      if sys.platform == 'os2emx':
        pstdout = None 
      else:
        pstdout = PIPE
      pstderr = STDOUT
      interactiveargs = []
      if interactive:
        pstdout = None
        pstderr = None
        interactiveargs = ['-e', 'print("To start the test, type _execute_test();")', '-i']
      full_args = args + headfiles + testheadfiles \
                  + ['-f', test] \
                  + tailfiles + testtailfiles + interactiveargs
      proc = Popen(full_args, stdout=pstdout, stderr=pstderr,
                   env=env, cwd=testdir)
      # |stderr == None| as |pstderr| was either |None| or redirected to |stdout|.
      stdout, stderr = proc.communicate()

      if interactive:
        # not sure what else to do here...
        return True

      if proc.returncode != 0 or (stdout is not None and re.search("^TEST-UNEXPECTED-FAIL", stdout, re.MULTILINE)):
        print """TEST-UNEXPECTED-FAIL | %s | test failed (with xpcshell return code: %d), see following log:
  >>>>>>>
  %s
  <<<<<<<""" % (test, proc.returncode, stdout)
        checkForCrashes(testdir, symbolsPath, testName=test)
        success = False
      else:
        print "TEST-PASS | %s | test passed" % test

      leakReport = processLeakLog(leakLogFile)

      if stdout is not None:
        try:
          f = open(test + '.log', 'w')
          f.write(stdout)
          if leakReport:
            f.write(leakReport)
        finally:
          if f:
            f.close()

      # Remove the leak detection file (here) so it can't "leak" to the next test.
      # The file is not there if leak logging was not enabled in the xpcshell build.
      if os.path.exists(leakLogFile):
        os.remove(leakLogFile)

  return success

def main():
  """Process command line arguments and call runTests() to do the real work."""
  parser = OptionParser()

  addCommonOptions(parser)
  parser.add_option("--test-path",
                    action="store", type="string", dest="testPath",
                    default=None, help="single path and/or test filename to test")
  parser.add_option("--interactive",
                    action="store_true", dest="interactive", default=False,
                    help="don't automatically run tests, drop to an xpcshell prompt")
  parser.add_option("--manifest",
                    action="store", type="string", dest="manifest",
                    default=None, help="Manifest of test directories to use")
  options, args = parser.parse_args()

  if len(args) < 2 and options.manifest is None or \
     (len(args) < 1 and options.manifest is not None):
    print >>sys.stderr, """Usage: %s <path to xpcshell> <test dirs>
  or: %s --manifest=test.manifest <path to xpcshell>""" % (sys.argv[0],
                                                           sys.argv[0])
    sys.exit(1)

  if options.interactive and not options.testPath:
    print >>sys.stderr, "Error: You must specify a test filename in interactive mode!"
    sys.exit(1)

  if not runTests(args[0], testdirs=args[1:],
                  xrePath=options.xrePath,
                  testPath=options.testPath,
                  interactive=options.interactive,
                  manifest=options.manifest,
                  symbolsPath=options.symbolsPath):
    sys.exit(1)

if __name__ == '__main__':
  main()
