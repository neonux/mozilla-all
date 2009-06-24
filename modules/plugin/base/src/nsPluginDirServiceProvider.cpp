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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
 *  Ere Maijala <emaijala@kolumbus.fi>
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

#include <tchar.h>

#include "nsPluginDirServiceProvider.h"

#include "nsCRT.h"
#include "nsILocalFile.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsDependentString.h"
#include "nsXPIDLString.h"
#include "prmem.h"
#include "nsArrayEnumerator.h"

typedef struct structVer
{
  WORD wMajor;
  WORD wMinor;
  WORD wRelease;
  WORD wBuild;
} verBlock;

#ifdef UNICODE
static nsresult
t_NS_NewNativeLocalFile(wchar_t *path, PRBool b, nsILocalFile **retval)
{
  return NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(path), b, retval);
}
#endif

#ifndef TEXT
#define TEXT(_x)  _x
#endif

static nsresult
t_NS_NewNativeLocalFile(char *path, PRBool b, nsILocalFile **retval)
{
  return NS_NewNativeLocalFile(nsDependentCString(path), b, retval);
}

static void
ClearVersion(verBlock *ver)
{
  ver->wMajor   = 0;
  ver->wMinor   = 0;
  ver->wRelease = 0;
  ver->wBuild   = 0;
}

static BOOL
FileExists(LPTSTR szFile)
{
  return GetFileAttributes(szFile) != 0xFFFFFFFF;
}

// Get file version information from a file
static BOOL
GetFileVersion(LPTSTR szFile, verBlock *vbVersion)
{
  UINT              uLen;
  UINT              dwLen;
  BOOL              bRv;
  DWORD             dwHandle;
  LPVOID            lpData;
  LPVOID            lpBuffer;
  VS_FIXEDFILEINFO  *lpBuffer2;

  ClearVersion(vbVersion);
  if (FileExists(szFile)) {
    bRv    = TRUE;
    dwLen  = GetFileVersionInfoSize(szFile, &dwHandle);
    lpData = (LPVOID)malloc(dwLen);
    uLen   = 0;

    if (lpData && GetFileVersionInfo(szFile, dwHandle, dwLen, lpData) != 0) {
      if (VerQueryValue(lpData, TEXT("\\"), &lpBuffer, &uLen) != 0) {
        lpBuffer2 = (VS_FIXEDFILEINFO *)lpBuffer;

        vbVersion->wMajor   = HIWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->wMinor   = LOWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->wRelease = HIWORD(lpBuffer2->dwFileVersionLS);
        vbVersion->wBuild   = LOWORD(lpBuffer2->dwFileVersionLS);
      }
    }

    free(lpData);
  } else {
    /* File does not exist */
    bRv = FALSE;
  }

  return bRv;
}

// Will deep copy ver2 into ver1
static void
CopyVersion(verBlock *ver1, verBlock *ver2)
{
  ver1->wMajor   = ver2->wMajor;
  ver1->wMinor   = ver2->wMinor;
  ver1->wRelease = ver2->wRelease;
  ver1->wBuild   = ver2->wBuild;
}

// Convert a string version to a version struct
static void
TranslateVersionStr(const TCHAR* szVersion, verBlock *vbVersion)
{
  LPTSTR szNum1 = NULL;
  LPTSTR szNum2 = NULL;
  LPTSTR szNum3 = NULL;
  LPTSTR szNum4 = NULL;
  LPTSTR szJavaBuild = NULL;

  TCHAR *strVer = nsnull;
  if (szVersion) {
    strVer = _tcsdup(szVersion);
  }

  if (!strVer) {
    // Out of memory
    ClearVersion(vbVersion);
    return;
  }

  // Java may be using an underscore instead of a dot for the build ID
  szJavaBuild = _tcschr(strVer, '_');
  if (szJavaBuild) {
    szJavaBuild[0] = '.';
  }

  szNum1 = _tcstok(strVer, TEXT("."));
  szNum2 = _tcstok(NULL,   TEXT("."));
  szNum3 = _tcstok(NULL,   TEXT("."));
  szNum4 = _tcstok(NULL,   TEXT("."));

  vbVersion->wMajor   = szNum1 ? (WORD) _ttoi(szNum1) : 0;
  vbVersion->wMinor   = szNum2 ? (WORD) _ttoi(szNum2) : 0;
  vbVersion->wRelease = szNum3 ? (WORD) _ttoi(szNum3) : 0;
  vbVersion->wBuild   = szNum4 ? (WORD) _ttoi(szNum4) : 0;

  free(strVer);
}

#ifdef UNICODE
static void
TranslateVersionStr(const char* szVersion, verBlock *vbVersion)
{
  TranslateVersionStr(NS_ConvertUTF8toUTF16(szVersion).get(), vbVersion);
}
#endif

// Compare two version struct, return zero if the same
static int
CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew)
{
  if (vbVersionOld.wMajor > vbVersionNew.wMajor) {
    return 4;
  } else if (vbVersionOld.wMajor < vbVersionNew.wMajor) {
    return -4;
  }

  if (vbVersionOld.wMinor > vbVersionNew.wMinor) {
    return 3;
  } else if (vbVersionOld.wMinor < vbVersionNew.wMinor) {
    return -3;
  }

  if (vbVersionOld.wRelease > vbVersionNew.wRelease) {
    return 2;
  } else if (vbVersionOld.wRelease < vbVersionNew.wRelease) {
    return -2;
  }

  if (vbVersionOld.wBuild > vbVersionNew.wBuild) {
    return 1;
  } else if (vbVersionOld.wBuild < vbVersionNew.wBuild) {
    return -1;
  }

  /* the versions are all the same */
  return 0;
}

//*****************************************************************************
// nsPluginDirServiceProvider::Constructor/Destructor
//*****************************************************************************

nsPluginDirServiceProvider::nsPluginDirServiceProvider()
{
}

nsPluginDirServiceProvider::~nsPluginDirServiceProvider()
{
}

//*****************************************************************************
// nsPluginDirServiceProvider::nsISupports
//*****************************************************************************

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPluginDirServiceProvider,
                              nsIDirectoryServiceProvider)

//*****************************************************************************
// nsPluginDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsPluginDirServiceProvider::GetFile(const char *charProp, PRBool *persistant,
                                    nsIFile **_retval)
{
  nsCOMPtr<nsILocalFile>  localFile;
  nsresult rv = NS_ERROR_FAILURE;

  NS_ENSURE_ARG(charProp);

#ifdef UNICODE
  NS_ConvertUTF8toUTF16 tprop(charProp);
  const wchar_t *prop = tprop.get();
#else
  const char *prop = charProp;
#endif

  *_retval = nsnull;
  *persistant = PR_FALSE;

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (!prefs)
    return NS_ERROR_FAILURE;

  if (nsCRT::strcmp(charProp, NS_WIN_4DOTX_SCAN_KEY) == 0) {
    // Check our prefs to see if scanning the 4.x folder has been
    // explictly overriden failure to get the pref is okay, we'll do
    // what we've been doing -- a filtered scan
    PRBool bScan4x;
    if (NS_SUCCEEDED(prefs->GetBoolPref(NS_WIN_4DOTX_SCAN_KEY, &bScan4x)) &&
        !bScan4x) {
      return NS_ERROR_FAILURE;
    }

    // Look for the plugin folder that the user has in their
    // Communicator 4x install
    HKEY keyloc;
    long result;
    DWORD type;
    TCHAR szKey[_MAX_PATH] = TEXT("Software\\Netscape\\Netscape Navigator");
    TCHAR path[_MAX_PATH];

    result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &keyloc);

    if (result == ERROR_SUCCESS) {
      TCHAR current_version[80];
      DWORD length = NS_ARRAY_LENGTH(current_version);

      result = ::RegQueryValueEx(keyloc, TEXT("CurrentVersion"), NULL, &type,
                                 (LPBYTE)&current_version, &length);

      ::RegCloseKey(keyloc);
      _tcscat(szKey, TEXT("\\"));
      _tcscat(szKey, current_version);
      _tcscat(szKey, TEXT("\\Main"));
      result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &keyloc);

      if (result == ERROR_SUCCESS) {
        DWORD pathlen = NS_ARRAY_LENGTH(path);

        result = ::RegQueryValueEx(keyloc, TEXT("Plugins Directory"), NULL, &type,
                                   (LPBYTE)&path, &pathlen);
        if (result == ERROR_SUCCESS) {
          rv = t_NS_NewNativeLocalFile(path, PR_TRUE, getter_AddRefs(localFile));
        }

        ::RegCloseKey(keyloc);
      }
    }
  } else if (nsCRT::strcmp(charProp, NS_WIN_JRE_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(charProp, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(strVer.get(), &minVer);

    // Look for the Java OJI plugin via the JRE install path
    HKEY baseloc;
    HKEY keyloc;
    HKEY entryloc;
    FILETIME modTime;
    DWORD type;
    DWORD index = 0;
    DWORD numChars = _MAX_PATH;
    DWORD pathlen;
    verBlock maxVer;
    ClearVersion(&maxVer);
    TCHAR curKey[_MAX_PATH] = TEXT("Software\\JavaSoft\\Java Runtime Environment");
    TCHAR path[_MAX_PATH];
    // Add + 15 to prevent buffer overrun when adding \bin (+ optionally
    // \new_plugin)
#define JAVA_PATH_SIZE _MAX_PATH + 15
    TCHAR newestPath[JAVA_PATH_SIZE];
    const TCHAR mozPath[_MAX_PATH] = TEXT("Software\\mozilla.org\\Mozilla");
    TCHAR browserJavaVersion[_MAX_PATH];

    newestPath[0] = 0;
    LONG result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, curKey, 0, KEY_READ,
                                 &baseloc);
    if (ERROR_SUCCESS != result)
      return NS_ERROR_FAILURE;

    // Look for "BrowserJavaVersion"
    if (ERROR_SUCCESS != ::RegQueryValueEx(baseloc, TEXT("BrowserJavaVersion"), NULL,
                                           NULL, (LPBYTE)&browserJavaVersion,
                                           &numChars))
      browserJavaVersion[0] = 0;

    // We must enumerate through the keys because what if there is
    // more than one version?
    do {
      path[0] = 0;
      numChars = _MAX_PATH;
      pathlen = NS_ARRAY_LENGTH(path);
      result = ::RegEnumKeyEx(baseloc, index, curKey, &numChars, NULL, NULL,
                              NULL, &modTime);
      index++;

      // Skip major.minor as it always points to latest in its family
      numChars = 0;
      for (TCHAR *p = curKey; *p; p++) {
        if (*p == '.') {
          numChars++;
        }
      }
      if (numChars < 2)
        continue;

      if (ERROR_SUCCESS == result) {
        if (ERROR_SUCCESS == ::RegOpenKeyEx(baseloc, curKey, 0,
                                            KEY_QUERY_VALUE, &keyloc)) {
          // We have a sub key
          if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, TEXT("JavaHome"), NULL,
                                                 &type, (LPBYTE)&path,
                                                 &pathlen)) {
            verBlock curVer;
            TranslateVersionStr(curKey, &curVer);
            if (CompareVersion(curVer, minVer) >= 0) {
              if (!_tcsncmp(browserJavaVersion, curKey, _MAX_PATH)) {
                _tcscpy(newestPath, path);
                ::RegCloseKey(keyloc);
                break;
              }

              if (CompareVersion(curVer, maxVer) >= 0) {
                _tcscpy(newestPath, path);
                CopyVersion(&maxVer, &curVer);
              }
            }
          }
          ::RegCloseKey(keyloc);
        }
      }
    } while (ERROR_SUCCESS == result);

    ::RegCloseKey(baseloc);

    // If nothing is found, then don't add \bin dir and don't set
    // CurrentVersion for Mozilla
    if (newestPath[0] != 0) {
      if (ERROR_SUCCESS == ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, mozPath, 0,
                                            NULL, REG_OPTION_NON_VOLATILE,
                                            KEY_SET_VALUE|KEY_QUERY_VALUE,
                                            NULL, &entryloc, NULL)) {
        if (ERROR_SUCCESS != ::RegQueryValueEx(entryloc, TEXT("CurrentVersion"), 0,
                                               NULL, NULL, NULL)) {
          ::RegSetValueEx(entryloc, TEXT("CurrentVersion"), 0, REG_SZ,
                          (const BYTE*) TEXT(MOZILLA_VERSION),
                          sizeof(TEXT(MOZILLA_VERSION)));
        }
        ::RegCloseKey(entryloc);
      }

      _tcscat(newestPath, TEXT("\\bin"));

      // See whether the "new_plugin" directory exists
      TCHAR tmpPath[JAVA_PATH_SIZE];
      nsCOMPtr<nsILocalFile> tmpFile;

      _tcscpy(tmpPath, newestPath);
      _tcscat(tmpPath, TEXT("\\new_plugin"));
      rv = t_NS_NewNativeLocalFile(tmpPath, PR_TRUE, getter_AddRefs(tmpFile));
      if (NS_SUCCEEDED(rv) && tmpFile) {
        PRBool exists = PR_FALSE;
        PRBool isDir = PR_FALSE;
        if (NS_SUCCEEDED(tmpFile->Exists(&exists)) && exists &&
            NS_SUCCEEDED(tmpFile->IsDirectory(&isDir)) && isDir) {
          // Assume we're supposed to use this as the search
          // directory for the Java Plug-In instead of the normal
          // one
          _tcscpy(newestPath, tmpPath);
        }
      }

      rv = t_NS_NewNativeLocalFile(newestPath, PR_TRUE, getter_AddRefs(localFile));
    }
  } else if (nsCRT::strcmp(charProp, NS_WIN_QUICKTIME_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(charProp, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(strVer.get(), &minVer);

    // Look for the Quicktime system installation plugins directory
    HKEY keyloc;
    long result;
    DWORD type;
    verBlock qtVer;
    ClearVersion(&qtVer);
    TCHAR path[_MAX_PATH];
    DWORD pathlen = NS_ARRAY_LENGTH(path);

    // First we need to check the version of Quicktime via checking
    // the EXE's version table
    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                        TEXT("software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\QuickTimePlayer.exe"),
                                        0, KEY_READ, &keyloc)) {
      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, NULL, NULL, &type,
                                             (LPBYTE)&path, &pathlen)) {
        GetFileVersion(path, &qtVer);
      }
      ::RegCloseKey(keyloc);
    }
    if (CompareVersion(qtVer, minVer) < 0)
      return rv;

    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                        TEXT("software\\Apple Computer, Inc.\\QuickTime"),
                                        0, KEY_READ, &keyloc)) {
      DWORD pathlen = NS_ARRAY_LENGTH(path);

      result = ::RegQueryValueEx(keyloc, TEXT("InstallDir"), NULL, &type,
                                 (LPBYTE)&path, &pathlen);
      _tcscat(path, TEXT("\\Plugins"));
      if (result == ERROR_SUCCESS)
        rv = t_NS_NewNativeLocalFile(path, PR_TRUE,
                                     getter_AddRefs(localFile));
      ::RegCloseKey(keyloc);
    }
  } else if (nsCRT::strcmp(charProp, NS_WIN_WMP_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(charProp, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(strVer.get(), &minVer);

    // Look for Windows Media Player system installation plugins directory
    HKEY keyloc;
    DWORD type;
    verBlock wmpVer;
    ClearVersion(&wmpVer);
    TCHAR path[_MAX_PATH];
    DWORD pathlen = NS_ARRAY_LENGTH(path);

    // First we need to check the version of WMP
    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                        TEXT("software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\wmplayer.exe"),
                                        0, KEY_READ, &keyloc)) {
      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, NULL, NULL, &type,
                                             (LPBYTE)&path, &pathlen)) {
        GetFileVersion(path, &wmpVer);
      }
      ::RegCloseKey(keyloc);
    }
    if (CompareVersion(wmpVer, minVer) < 0)
      return rv;

    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                        TEXT("software\\Microsoft\\MediaPlayer"), 0,
                                        KEY_READ, &keyloc)) {
      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, TEXT("Installation Directory"),
                                             NULL, &type, (LPBYTE)&path,
                                             &pathlen)) {
        rv = t_NS_NewNativeLocalFile(path, PR_TRUE, getter_AddRefs(localFile));
      }

      ::RegCloseKey(keyloc);
    }
  } else if (nsCRT::strcmp(charProp, NS_WIN_ACROBAT_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(charProp, getter_Copies(strVer)))) {
      return NS_ERROR_FAILURE;
    }

    verBlock minVer;
    TranslateVersionStr(strVer.get(), &minVer);

    // Look for Adobe Acrobat system installation plugins directory
    HKEY baseloc;
    HKEY keyloc;
    FILETIME modTime;
    DWORD type;
    DWORD index = 0;
    DWORD numChars = _MAX_PATH;
    DWORD pathlen;
    verBlock maxVer;
    ClearVersion(&maxVer);
    TCHAR curKey[_MAX_PATH] = TEXT("software\\Adobe\\Acrobat Reader");
    TCHAR path[_MAX_PATH];
    // Add + 8 to prevent buffer overrun when adding \browser
    TCHAR newestPath[_MAX_PATH + 8];

    newestPath[0] = 0;
    if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, curKey, 0,
                                        KEY_READ, &baseloc)) {
      _tcscpy(curKey, TEXT("software\\Adobe\\Adobe Acrobat"));
      if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, curKey, 0,
                                          KEY_READ, &baseloc)) {
        return NS_ERROR_FAILURE;
      }
    }

    // We must enumerate through the keys because what if there is
    // more than one version?
    LONG result = ERROR_SUCCESS;
    while (ERROR_SUCCESS == result) {
      path[0] = 0;
      numChars = _MAX_PATH;
      pathlen = NS_ARRAY_LENGTH(path);
      result = ::RegEnumKeyEx(baseloc, index, curKey, &numChars, NULL, NULL,
                              NULL, &modTime);
      index++;

      if (ERROR_SUCCESS == result) {
        verBlock curVer;
        TranslateVersionStr(curKey, &curVer);
        _tcscat(curKey, TEXT("\\InstallPath"));
        if (ERROR_SUCCESS == ::RegOpenKeyEx(baseloc, curKey, 0,
                                            KEY_QUERY_VALUE, &keyloc)) {
          // We have a sub key
          if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, NULL, NULL, &type,
                                                 (LPBYTE)&path, &pathlen)) {
            if (CompareVersion(curVer, maxVer) >= 0 &&
                CompareVersion(curVer, minVer) >= 0) {
              _tcscpy(newestPath, path);
              CopyVersion(&maxVer, &curVer);
            }
          }

          ::RegCloseKey(keyloc);
        }
      }
    }

    ::RegCloseKey(baseloc);

    if (newestPath[0] != 0) {
      _tcscat(newestPath, TEXT("\\browser"));
      rv = t_NS_NewNativeLocalFile(newestPath, PR_TRUE, getter_AddRefs(localFile));
    }

  }

  if (localFile && NS_SUCCEEDED(rv))
    return CallQueryInterface(localFile, _retval);

  return rv;
}

nsresult
nsPluginDirServiceProvider::GetPLIDDirectories(nsISimpleEnumerator **aEnumerator)
{
  NS_ENSURE_ARG_POINTER(aEnumerator);
  *aEnumerator = nsnull;

  nsCOMArray<nsILocalFile> dirs;

  GetPLIDDirectoriesWithHKEY(HKEY_CURRENT_USER, dirs);
  GetPLIDDirectoriesWithHKEY(HKEY_LOCAL_MACHINE, dirs);

  return NS_NewArrayEnumerator(aEnumerator, dirs);
}

nsresult
nsPluginDirServiceProvider::GetPLIDDirectoriesWithHKEY(HKEY aKey, nsCOMArray<nsILocalFile> &aDirs)
{
  TCHAR subkey[_MAX_PATH] = TEXT("Software\\MozillaPlugins");
  HKEY baseloc;

  if (ERROR_SUCCESS != ::RegOpenKeyEx(aKey, subkey, 0, KEY_READ, &baseloc))
    return NS_ERROR_FAILURE;

  DWORD index = 0;
  DWORD subkeylen = _MAX_PATH;
  FILETIME modTime;
  while (ERROR_SUCCESS == ::RegEnumKeyEx(baseloc, index++, subkey, &subkeylen,
                                         NULL, NULL, NULL, &modTime)) {
    subkeylen = _MAX_PATH;
    HKEY keyloc;

    if (ERROR_SUCCESS == ::RegOpenKeyEx(baseloc, subkey, 0, KEY_QUERY_VALUE,
                                        &keyloc)) {
      DWORD type;
      TCHAR path[_MAX_PATH];
      DWORD pathlen = NS_ARRAY_LENGTH(path);

      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, TEXT("Path"), NULL, &type,
                                             (LPBYTE)&path, &pathlen)) {
        nsCOMPtr<nsILocalFile> localFile;
        if (NS_SUCCEEDED(t_NS_NewNativeLocalFile(path,
                                                 PR_TRUE,
                                                 getter_AddRefs(localFile))) &&
            localFile)
        {
          // Some vendors use a path directly to the DLL so chop off
          // the filename
          PRBool isDir = PR_FALSE;
          if (NS_SUCCEEDED(localFile->IsDirectory(&isDir)) && !isDir) {
            nsCOMPtr<nsIFile> temp;
            localFile->GetParent(getter_AddRefs(temp));
            if (temp)
              localFile = do_QueryInterface(temp);
          }

          // Now we check to make sure it's actually on disk and
          // To see if we already have this directory in the array
          PRBool isFileThere = PR_FALSE;
          PRBool isDupEntry = PR_FALSE;
          if (NS_SUCCEEDED(localFile->Exists(&isFileThere)) && isFileThere) {
            PRInt32 c = aDirs.Count();
            for (PRInt32 i = 0; i < c; i++) {
              nsIFile *dup = static_cast<nsIFile*>(aDirs[i]);
              if (dup &&
                  NS_SUCCEEDED(dup->Equals(localFile, &isDupEntry)) &&
                  isDupEntry) {
                break;
              }
            }

            if (!isDupEntry) {
              aDirs.AppendObject(localFile);
            }
          }
        }
      }
      ::RegCloseKey(keyloc);
    }
  }
  ::RegCloseKey(baseloc);
  return NS_OK;
}

