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
 * The Original Code is Maintenance service service installer code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian R. Bondy <netzen@gmail.com>
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

#include <windows.h>
#include <aclapi.h>
#include <stdlib.h>

#include <nsAutoPtr.h>
#include <nsWindowsHelpers.h>
#include <nsMemory.h>

#include "serviceinstall.h"
#include "servicebase.h"
#include "updatehelper.h"
#include "shellapi.h"

#pragma comment(lib, "version.lib")

/**
 * Obtains the version number from the specified PE file's version information
 * Version Format: A.B.C.D (Example 10.0.0.300)
 *  
 * @param  path The path of the file to check the version on
 * @param  A    The first part of the version number
 * @param  B    The second part of the version number
 * @param  C    The third part of the version number
 * @param  D    The fourth part of the version number
 * @return TRUE if successful
 */
static BOOL
GetVersionNumberFromPath(LPWSTR path, DWORD &A, DWORD &B, 
                         DWORD &C, DWORD &D) 
{
  DWORD fileVersionInfoSize = GetFileVersionInfoSizeW(path, 0);
  nsAutoArrayPtr<char> fileVersionInfo = new char[fileVersionInfoSize];
  if (!GetFileVersionInfoW(path, 0, fileVersionInfoSize,
                           fileVersionInfo.get())) {
      LOG(("Could not obtain file info of old service.  (%d)\n", 
           GetLastError()));
      return FALSE;
  }

  VS_FIXEDFILEINFO *fixedFileInfo = 
    reinterpret_cast<VS_FIXEDFILEINFO *>(fileVersionInfo.get());
  UINT size;
  if (!VerQueryValueW(fileVersionInfo.get(), L"\\", 
    reinterpret_cast<LPVOID*>(&fixedFileInfo), &size)) {
      LOG(("Could not query file version info of old service.  (%d)\n", 
           GetLastError()));
      return FALSE;
  }  

  A = HIWORD(fixedFileInfo->dwFileVersionMS);
  B = LOWORD(fixedFileInfo->dwFileVersionMS);
  C = HIWORD(fixedFileInfo->dwFileVersionLS);
  D = LOWORD(fixedFileInfo->dwFileVersionLS);
  return TRUE;
}

/**
 * Installs or upgrades the SVC_NAME service.
 * If an existing service is already installed, we replace it with the
 * currently running process.
 *
 * @param  action The action to perform.
 * @return TRUE if the service was installed/upgraded
 */
BOOL
SvcInstall(SvcInstallAction action)
{
  // Get a handle to the local computer SCM database with full access rights.
  nsAutoServiceHandle schSCManager(OpenSCManager(NULL, NULL, 
                                                 SC_MANAGER_ALL_ACCESS));
  if (!schSCManager) {
    LOG(("Could not open service manager.  (%d)\n", GetLastError()));
    return FALSE;
  }

  WCHAR newServiceBinaryPath[MAX_PATH + 1];
  if (!GetModuleFileNameW(NULL, newServiceBinaryPath, 
                          sizeof(newServiceBinaryPath) / 
                          sizeof(newServiceBinaryPath[0]))) {
    LOG(("Could not obtain module filename when attempting to "
         "install service. (%d)\n",
         GetLastError()));
    return FALSE;
  }

  // Check if we already have an open service
  BOOL serviceAlreadyExists = FALSE;
  nsAutoServiceHandle schService(OpenServiceW(schSCManager, 
                                              SVC_NAME, 
                                              SERVICE_ALL_ACCESS));
  DWORD lastError = GetLastError();
  if (!schService && ERROR_SERVICE_DOES_NOT_EXIST != lastError) {
    // The service exists but we couldn't open it
    LOG(("Could not open service.  (%d)\n", GetLastError()));
    return FALSE;
  }
  
  if (schService) {
    serviceAlreadyExists = TRUE;

    // The service exists and we opened it
    DWORD bytesNeeded;
    if (!QueryServiceConfigW(schService, NULL, 0, &bytesNeeded) && 
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      LOG(("Could not determine buffer size for query service config.  (%d)\n", 
           GetLastError()));
      return FALSE;
    }

    // Get the service config information, in particular we want the binary 
    // path of the service.
    nsAutoArrayPtr<char> serviceConfigBuffer = new char[bytesNeeded];
    if (!QueryServiceConfigW(schService, 
        reinterpret_cast<QUERY_SERVICE_CONFIGW*>(serviceConfigBuffer.get()), 
        bytesNeeded, &bytesNeeded)) {
      LOG(("Could open service but could not query service config.  (%d)\n", 
           GetLastError()));
      return FALSE;
    }
    QUERY_SERVICE_CONFIGW &serviceConfig = 
      *reinterpret_cast<QUERY_SERVICE_CONFIGW*>(serviceConfigBuffer.get());

    // Obtain the existing maintenanceservice file's version number and
    // the new file's version number.  Versions are in the format of
    // A.B.C.D.
    DWORD existingA, existingB, existingC, existingD;
    DWORD newA, newB, newC, newD; 
    BOOL obtainedExistingVersionInfo = 
      GetVersionNumberFromPath(serviceConfig.lpBinaryPathName, 
                               existingA, existingB, 
                               existingC, existingD);
    if (!GetVersionNumberFromPath(newServiceBinaryPath, newA, 
                                 newB, newC, newD)) {
      LOG(("Could not obtain version number from new path\n"));
      return FALSE;
    }

    schService.reset(); //Explicitly close the handle so we can delete it

    // Check if we need to replace the old binary with the new one
    // If we couldn't get the old version info then we assume we should 
    // replace it.
    if (ForceInstallSvc == action ||
        !obtainedExistingVersionInfo || 
        (existingA < newA) ||
        (existingA == newA && existingB < newB) ||
        (existingA == newA && existingB == newB && 
         existingC < newC) ||
        (existingA == newA && existingB == newB && 
         existingC == newC && existingD < newD)) {
      if (!StopService()) {
        return FALSE;
      }

      if (!DeleteFileW(serviceConfig.lpBinaryPathName)) {
        LOG(("Could not delete old service binary file %ls.  (%d)\n", 
             serviceConfig.lpBinaryPathName, GetLastError()));
        return FALSE;
      }

      if (!CopyFileW(newServiceBinaryPath, 
                    serviceConfig.lpBinaryPathName, FALSE)) {
        LOG(("Could not overwrite old service binary file. "
             "This should never happen, but if it does the next upgrade will fix"
             " it, the service is not a critical component that needs to be "
             " installed for upgrades to work. (%d)\n", 
             GetLastError()));
        return FALSE;
      }

      // We made a copy of ourselves to the existing location.
      // The tmp file (the process of which we are executing right now) will be
      // left over.  Attempt to delete the file on the next reboot.
      MoveFileExW(newServiceBinaryPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
      
      // Setup the new module path
      wcsncpy(newServiceBinaryPath, serviceConfig.lpBinaryPathName, MAX_PATH);
      // Fall through so we replace the service
    } else {
      // We don't need to copy ourselves to the existing location.
      // The tmp file (the process of which we are executing right now) will be
      // left over.  Attempt to delete the file on the next reboot.
      MoveFileExW(newServiceBinaryPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
      
      return TRUE; // nothing to do, we already have a newer service installed
    }
  } else if (UpgradeSvc == action) {
    // The service does not exist and we are upgrading, so don't install it
    return TRUE;
  }

  if (!serviceAlreadyExists) {
    // Create the service as on demand
    schService.own(CreateServiceW(schSCManager, SVC_NAME, SVC_DISPLAY_NAME,
                                  SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                  SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
                                  newServiceBinaryPath, NULL, NULL, NULL, 
                                  NULL, NULL));
    if (!schService) {
      LOG(("Could not create Windows service. "
           "This error should never happen since a service install "
           "should only be called when elevated. (%d)\n", GetLastError()));
      return FALSE;
    } 
  }

  if (schService) {
    if (!SetUserAccessServiceDACL(schService)) {
      LOG(("Could not set security ACE on service handle, the service will not"
           "be able to be started from unelevated processes. "
           "This error should never happen.  (%d)\n", 
           GetLastError()));
    }
  }

  return TRUE;
}

/**
 * Stops the Maintenance service.
 *
 * @return TRUE if successful.
 */
BOOL
StopService()
{
  // Get a handle to the local computer SCM database with full access rights.
  nsAutoServiceHandle schSCManager(OpenSCManager(NULL, NULL, 
                                                 SC_MANAGER_ALL_ACCESS));
  if (!schSCManager) {
    LOG(("Could not open service manager.  (%d)\n", GetLastError()));
    return FALSE;
  }

  // Open the service
  nsAutoServiceHandle schService(OpenServiceW(schSCManager, SVC_NAME, 
                                              SERVICE_ALL_ACCESS));
  if (!schService) {
    LOG(("Could not open service.  (%d)\n", GetLastError()));
    return FALSE;
  } 

  LOG(("Sending stop request...\n"));
  SERVICE_STATUS status;
  if (!ControlService(schService, SERVICE_CONTROL_STOP, &status)) {
    LOG(("Error sending stop request: %d\n", GetLastError()));
  }

  schSCManager.reset();
  schService.reset();

  LOG(("Waiting for service stop...\n"));
  DWORD lastState = WaitForServiceStop(SVC_NAME, 30);

  // The service can be in a stopped state but the exe still in use
  // so make sure the process is really gone before proceeding
  WaitForProcessExit(L"maintenanceservice.exe", 30);
  LOG(("Done waiting for service stop, last service state: %d\n", lastState));

  return lastState == SERVICE_STOPPED;
}

/**
 * Uninstalls the Maintenance service.
 *
 * @return TRUE if successful.
 */
BOOL
SvcUninstall()
{
  // Get a handle to the local computer SCM database with full access rights.
  nsAutoServiceHandle schSCManager(OpenSCManager(NULL, NULL, 
                                                 SC_MANAGER_ALL_ACCESS));
  if (!schSCManager) {
    LOG(("Could not open service manager.  (%d)\n", GetLastError()));
    return FALSE;
  }

  // Open the service
  nsAutoServiceHandle schService(OpenServiceW(schSCManager, SVC_NAME, 
                                              SERVICE_ALL_ACCESS));
  if (!schService) {
    LOG(("Could not open service.  (%d)\n", GetLastError()));
    return FALSE;
  } 

  //Stop the service so it deletes faster and so the uninstaller
  // can actually delete its EXE.
  DWORD totalWaitTime = 0;
  SERVICE_STATUS status;
  static const int maxWaitTime = 1000 * 60; // Never wait more than a minute
  if (ControlService(schService, SERVICE_CONTROL_STOP, &status)) {
    do {
      Sleep(status.dwWaitHint);
      totalWaitTime += (status.dwWaitHint + 10);
      if (status.dwCurrentState == SERVICE_STOPPED) {
        break;
      } else if (totalWaitTime > maxWaitTime) {
        break;
      }
    } while (QueryServiceStatus(schService, &status));
  }

  // Delete the service or mark it for deletion
  BOOL deleted = DeleteService(schService);
  if (!deleted) {
    deleted = (GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE);
  }

  return deleted;
}

/**
 * Sets the access control list for user access for the specified service.
 *
 * @param  hService The service to set the access control list on
 * @return TRUE if successful
 */
BOOL
SetUserAccessServiceDACL(SC_HANDLE hService)
{
  PACL pNewAcl = NULL;
  PSECURITY_DESCRIPTOR psd = NULL;
  DWORD lastError = SetUserAccessServiceDACL(hService, pNewAcl, psd);
  if (pNewAcl) {
    LocalFree((HLOCAL)pNewAcl);
  }
  if (psd) {
    LocalFree((LPVOID)psd);
  }
  return ERROR_SUCCESS == lastError;
}

/**
 * Sets the access control list for user access for the specified service.
 *
 * @param  hService  The service to set the access control list on
 * @param  pNewAcl   The out param ACL which should be freed by caller
 * @param  psd       out param security descriptor, should be freed by caller
 * @return ERROR_SUCCESS if successful
 */
DWORD
SetUserAccessServiceDACL(SC_HANDLE hService, PACL &pNewAcl, 
                         PSECURITY_DESCRIPTOR psd)
{
  // Get the current security descriptor needed size
  DWORD needed = 0;
  if (!QueryServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, 
                                  &psd, 0, &needed)) {
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      LOG(("Warning: Could not query service object security size.  (%d)\n", 
           GetLastError()));
      return GetLastError();
    }

    DWORD size = needed;
    psd = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, size);
    if (!psd) {
      LOG(("Warning: Could not allocate security descriptor.  (%d)\n", 
           GetLastError()));
      return ERROR_INSUFFICIENT_BUFFER;
    }

    // Get the actual security descriptor now
    if (!QueryServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, 
                                    psd, size, &needed)) {
      LOG(("Warning: Could not allocate security descriptor.  (%d)\n", 
           GetLastError()));
      return GetLastError();
    }
  }

  // Get the current DACL from the security descriptor.
  PACL pacl = NULL;
  BOOL bDaclPresent = FALSE;
  BOOL bDaclDefaulted = FALSE;
  if ( !GetSecurityDescriptorDacl(psd, &bDaclPresent, &pacl, 
                                  &bDaclDefaulted)) {
    LOG(("Warning: Could not obtain DACL.  (%d)\n", GetLastError()));
    return GetLastError();
  }

  // Build the ACE, BuildExplicitAccessWithName cannot fail so it is not logged.
  EXPLICIT_ACCESS ea;
  BuildExplicitAccessWithName(&ea, TEXT("Users"), 
                              SERVICE_START | SERVICE_STOP | GENERIC_READ, 
                              SET_ACCESS, NO_INHERITANCE);
  DWORD lastError = SetEntriesInAcl(1, (PEXPLICIT_ACCESS)&ea, pacl, &pNewAcl);
  if (ERROR_SUCCESS != lastError) {
    LOG(("Warning: Could not set entries in ACL.  (%d)\n", lastError));
    return lastError;
  }

  // Initialize a new security descriptor.
  SECURITY_DESCRIPTOR sd;
  if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
    LOG(("Warning: Could not initialize security descriptor.  (%d)\n", 
         GetLastError()));
    return GetLastError();
  }

  // Set the new DACL in the security descriptor.
  if (!SetSecurityDescriptorDacl(&sd, TRUE, pNewAcl, FALSE)) {
    LOG(("Warning: Could not set security descriptor DACL.  (%d)\n", 
         GetLastError()));
    return GetLastError();
  }

  // Set the new security descriptor for the service object.
  if (!SetServiceObjectSecurity(hService, DACL_SECURITY_INFORMATION, &sd)) {
    LOG(("Warning: Could not set object security.  (%d)\n", 
         GetLastError()));
    return GetLastError();
  }

  // Woohoo, raise the roof
  LOG(("User access was set successfully on the service.\n"));
  return ERROR_SUCCESS;
}
