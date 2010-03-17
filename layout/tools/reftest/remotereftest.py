import sys
import os
import time

SCRIPT_DIRECTORY = os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0])))
sys.path.append(SCRIPT_DIRECTORY)
#os.chdir(SCRIPT_DIRECTORY)         

from runreftest import RefTest
from runreftest import ReftestOptions
from automation import Automation
from devicemanager import DeviceManager

class RemoteAutomation(Automation):
    _devicemanager = None

    def __init__(self, deviceManager, product = ''):
        self._devicemanager = deviceManager
        self._product = product
        Automation.__init__(self)

    def setDeviceManager(self, deviceManager):
        self._devicemanager = deviceManager

    def setProduct(self, productName):
        self._product = productName

    def setRemoteApp(self, remoteAppName):
        self._remoteAppName = remoteAppName

    def setTestRoot(self, testRoot):
        self._testRoot = testRoot

    def waitForFinish(self, proc, utilityPath, timeout, maxTime, startTime):
        status = proc.wait()
        print proc.stdout
        # todo: consider pulling log file from remote
        return status

    def buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs):
        remoteProfileDir = self._testRoot + 'profile'
        cmd, args = Automation.buildCommandLine(self, app, debuggerInfo, remoteProfileDir, testURL, extraArgs)
        return app, ['--environ:NO_EM_RESTART=1'] + args

    def Process(self, cmd, stdout = None, stderr = None, env = None, cwd = '.'):
        return self.RProcess(self._devicemanager, self._remoteAppName, cmd, stdout, stderr, env, cwd)

    class RProcess(object):
        #device manager process
        dm = None
        def __init__(self, dm, appName, cmd, stdout = None, stderr = None, env = None, cwd = '.'):
            self.dm = dm
            print "going to launch process: " + str(self.dm.host)
            self.proc = dm.launchProcess(cmd)
            self.procName = appName
            self.timeout = 600
            time.sleep(5)

        @property
        def pid(self):
            hexpid = self.dm.processExist(self.procName)
            if (hexpid == '' or hexpid == None):
                hexpid = 0
            return int(hexpid, 0)

        @property
        def stdout(self):
            return self.dm.getFile(self.proc)

        def wait(self):
            timer = 0
            while (self.dm.process.isAlive()):
                time.sleep(1)
                timer += 1
                if (timer > self.timeout):
                    break

            if (timer >= self.timeout):
                return 1
            return 0

        def kill(self):
            self.dm.killProcess(self.procName)
 

class RemoteOptions(ReftestOptions):
    def __init__(self, automation):
        ReftestOptions.__init__(self, automation)

        defaults = {}
        defaults["logFile"] = "reftest.log"
        # app, xrePath and utilityPath variables are set in main function
        defaults["testRoot"] = "/tests/"
        defaults["app"] = ""
        defaults["xrePath"] = ""
        defaults["utilityPath"] = ""

        self.add_option("--device", action="store",
                    type = "string", dest = "device",
                    help = "ip address of remote device to test")
        defaults["device"] = None

        self.add_option("--devicePort", action="store",
                    type = "string", dest = "devicePort",
                    help = "port of remote device to test")
        defaults["devicePort"] = 27020

        self.add_option("--remoteProductName", action="store",
                    type = "string", dest = "remoteProductName",
                    help = "Name of remote product to test - either fennec or firefox, defaults to fennec")
        defaults["remoteProductName"] = "fennec"

        self.add_option("--remoteAppName", action="store",
                    type = "string", dest = "remoteAppName",
                    help = "Executable name for remote device, OS dependent, defaults to fennec.exe")
        defaults["remoteAppName"] = "fennec.exe"

        self.add_option("--remote-webserver", action="store",
                    type = "string", dest = "remoteWebServer",
                    help = "IP Address of the webserver hosting the reftest content")
        defaults["remoteWebServer"] = "127.0.0.1"

        self.set_defaults(**defaults)

class RemoteReftest(RefTest):
    remoteApp = ''

    def __init__(self, automation, devicemanager, options, scriptDir):
        RefTest.__init__(self, automation)
        self._devicemanager = devicemanager
        self.scriptDir = scriptDir
        self.remoteApp = options.app
        self.remoteTestRoot = options.testRoot
        self.remoteProfileDir = options.testRoot + 'profile'

    def createReftestProfile(self, options, profileDir):
        RefTest.createReftestProfile(self, options, profileDir)

        self.remoteTestRoot += "reftest/"

        # install the reftest extension bits into the remoteProfile
        profileExtensionsPath = os.path.join(profileDir, "extensions")
        reftestExtensionPath = self.remoteTestRoot.replace('/', '\\')
        extFile = open(os.path.join(profileExtensionsPath, "reftest@mozilla.org"), "w")
        extFile.write(reftestExtensionPath)
        extFile.close()

        if (self._devicemanager.pushDir(profileDir, self.remoteProfileDir) == None):
          raise devicemanager.FileError("Failed to copy profiledir to device")

        if (self._devicemanager.pushDir(self.scriptDir + '/reftest', self.remoteTestRoot) == None):
          raise devicemanager.FileError("Failed to copy extension dir to device")


    def copyExtraFilesToProfile(self, options, profileDir):
        RefTest.copyExtraFilesToProfile(self, options, profileDir)
        if (self._devicemanager.pushDir(profileDir, self.remoteProfileDir) == None):
          raise devicemanager.FileError("Failed to copy extra files in profile dir to device")

    def registerExtension(self, browserEnv, options, profileDir):
        """
          It appears that we do not need to do the extension registration on winmo.
          This is something we should look into for winmo as the -silent option isn't working
        """
        pass

    def getManifestPath(self, path):
        return path

    def cleanup(self, profileDir):
        self._devicemanager.removeDir(self.remoteProfileDir)
        self._devicemanager.removeDir(self.remoteTestRoot)
        RefTest.cleanup(self, profileDir)

def main():
    dm = DeviceManager(None, None)
    automation = RemoteAutomation(dm)
    parser = RemoteOptions(automation)
    options, args = parser.parse_args()

    if (options.device == None):
        print "Error: you must provide a device IP to connect to via the --device option"
        sys.exit(1)

    if options.remoteAppName.rfind('/') < 0:
      options.app = options.testRoot + options.remoteProductName + '/'
    options.app += str(options.remoteAppName)

    options.xrePath = options.testRoot + options.remoteProductName + '/xulrunner'
    options.utilityPath = options.testRoot + options.remoteProductName + '/bin'

    dm = DeviceManager(options.device, options.devicePort)
    automation.setDeviceManager(dm)
    automation.setProduct(options.remoteProductName)
    automation.setRemoteApp(options.remoteAppName)
    automation.setTestRoot(options.testRoot)
    reftest = RemoteReftest(automation, dm, options, SCRIPT_DIRECTORY)

    if (options.remoteWebServer == "127.0.0.1"):
        print "Error: remoteWebServer must be non localhost"
        sys.exit(1)

#an example manifest name to use on the cli
#    manifest = "http://" + options.remoteWebServer + "/reftests/layout/reftests/reftest-sanity/reftest.list"
    reftest.runTests(args[0], options)

if __name__ == "__main__":
    main()
