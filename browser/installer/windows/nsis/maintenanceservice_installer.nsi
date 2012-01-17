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
# The Original Code is an NSIS installer for the maintenance service
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Brian R. Bondy <netzen@gmail.com>
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
# ***** END LICENSE BLOCK *****

; Set verbosity to 3 (e.g. no script) to lessen the noise in the build logs
!verbose 3

; 7-Zip provides better compression than the lzma from NSIS so we add the files
; uncompressed and use 7-Zip to create a SFX archive of it
SetDatablockOptimize on
SetCompress off
CRCCheck on

RequestExecutionLevel admin
!addplugindir ./

; Variables
Var TempMaintServiceName
Var BrandFullNameDA
Var BrandFullName

; Other included files may depend upon these includes!
; The following includes are provided by NSIS.
!include FileFunc.nsh
!include LogicLib.nsh
!include MUI.nsh
!include WinMessages.nsh
!include WinVer.nsh
!include WordFunc.nsh

!insertmacro GetOptions
!insertmacro GetParameters
!insertmacro GetSize

; The test slaves use this fallback key to run tests.
; And anyone that wants to run tests themselves should already have 
; this installed.
!define FallbackKey \
  "SOFTWARE\Mozilla\MaintenanceService\3932ecacee736d366d6436db0f55bce4"

!define CompanyName "Mozilla Corporation"
!define BrandFullNameInternal ""

; The following includes are custom.
!include defines.nsi
; We keep defines.nsi defined so that we get other things like 
; the version number, but we redefine BrandFullName
!define MaintFullName "Mozilla Maintenance Service"
!undef BrandFullName
!define BrandFullName "${MaintFullName}"

!include common.nsh
!include locales.nsi

VIAddVersionKey "FileDescription" "${MaintFullName} Installer"
VIAddVersionKey "OriginalFilename" "maintenanceservice_installer.exe"

Name "${MaintFullName}"
OutFile "maintenanceservice_installer.exe"

; Get installation folder from registry if available
InstallDirRegKey HKLM "Software\Mozilla\MaintenanceService" ""

SetOverwrite on

!define MaintUninstallKey \
 "Software\Microsoft\Windows\CurrentVersion\Uninstall\MozillaMaintenanceService"

; The HAVE_64BIT_OS define also means that we have an x64 build,
; not just an x64 OS.
!ifdef HAVE_64BIT_OS
  ; See below, we actually abort the install for x64 builds currently.
  InstallDir "$PROGRAMFILES64\${MaintFullName}\"
!else
  InstallDir "$PROGRAMFILES32\${MaintFullName}\"
!endif
ShowUnInstDetails nevershow

################################################################################
# Modern User Interface - MUI

!define MUI_ICON setup.ico
!define MUI_UNICON setup.ico
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_UNWELCOMEFINISHPAGE_BITMAP wizWatermark.bmp

;Interface Settings
!define MUI_ABORTWARNING

; Uninstaller Pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

################################################################################
# Language

!insertmacro MOZ_MUI_LANGUAGE 'baseLocale'
!verbose push
!verbose 3
!include "overrideLocale.nsh"
!include "customLocale.nsh"
!verbose pop

; Set this after the locale files to override it if it is in the locale
; using " " for BrandingText will hide the "Nullsoft Install System..." branding
BrandingText " "

Function .onInit
  SetSilent silent
!ifdef HAVE_64BIT_OS
  ; We plan to eventually enable 64bit native builds to use the maintenance
  ; service, but for the initial release, to reduce testing and development,
  ; 64-bit builds will not install the maintenanceservice.
  Abort
!endif

  ; On Windows 2000 we do not install the maintenance service.
  ; We won't run this installer from the parent installer, but just in case 
  ; someone tries to execute it on Windows 2000...
  ${Unless} ${AtLeastWinXP}
    Abort
  ${EndUnless}
FunctionEnd

Function un.onInit
  StrCpy $BrandFullNameDA "${MaintFullName}"
  StrCpy $BrandFullName "${MaintFullName}"
FunctionEnd

Section "MaintenanceService"
  AllowSkipFiles off

  CreateDirectory $INSTDIR
  SetOutPath $INSTDIR

  ; If the service already exists, then it will be stopped when upgrading it
  ; via the maintenanceservice_tmp.exe command executed below.
  ; The maintenanceservice_tmp.exe command will rename the file to
  ; maintenanceservice.exe if maintenanceservice_tmp.exe is newer.
  ; If the service does not exist yet, we install it and drop the file on
  ; disk as maintenanceservice.exe directly.
  StrCpy $TempMaintServiceName "maintenanceservice.exe"
  IfFileExists "$INSTDIR\maintenanceservice.exe" 0 skipAlreadyExists
    StrCpy $TempMaintServiceName "maintenanceservice_tmp.exe"
  skipAlreadyExists:

  ; We always write out a copy and then decide whether to install it or 
  ; not via calling its 'install' cmdline which works by version comparison.
  CopyFiles "$EXEDIR\maintenanceservice.exe" "$INSTDIR\$TempMaintServiceName"

  ; Install the application maintenance service.
  ; If a service already exists, the command line parameter will stop the
  ; service and only install itself if it is newer than the already installed
  ; service.  If successful it will remove the old maintenanceservice.exe
  ; and replace it with maintenanceservice_tmp.exe.
  ClearErrors
  ${GetParameters} $0
  ${GetOptions} "$0" "/Upgrade" $0
  ${If} ${Errors}
    nsExec::Exec '"$INSTDIR\$TempMaintServiceName" install'
  ${Else}
    ; The upgrade cmdline is the same as install except
    ; It will fail if the service isn't already installed.
    nsExec::Exec '"$INSTDIR\$TempMaintServiceName" upgrade'
  ${EndIf}

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${MaintUninstallKey}" "DisplayName" "${MaintFullName}"
  WriteRegStr HKLM "${MaintUninstallKey}" "UninstallString" \
                   '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${MaintUninstallKey}" "DisplayIcon" \
                   "$INSTDIR\Uninstall.exe,0"
  WriteRegStr HKLM "${MaintUninstallKey}" "DisplayVersion" "${AppVersion}"
  WriteRegStr HKLM "${MaintUninstallKey}" "Publisher" "Mozilla"
  WriteRegStr HKLM "${MaintUninstallKey}" "Comments" \
                   "${BrandFullName} ${AppVersion} (${ARCH} ${AB_CD})"
  WriteRegDWORD HKLM "${MaintUninstallKey}" "NoModify" 1
  ${GetSize} "$INSTDIR" "/S=0K" $R2 $R3 $R4
  WriteRegDWORD HKLM "${MaintUninstallKey}" "EstimatedSize" $R2

  ; Write out that a maintenance service was attempted.
  ; We do this because on upgrades we will check this value and we only
  ; want to install once on the first upgrade to maintenance service.
  ; Also write out that we are currently installed, preferences will check
  ; this value to determine if we should show the service update pref.
  ; Since the Maintenance service can be installed either x86 or x64,
  ; always use the 64-bit registry for checking if an attempt was made.
  SetRegView 64
  WriteRegDWORD HKLM "Software\Mozilla\MaintenanceService" "Attempted" 1
  WriteRegDWORD HKLM "Software\Mozilla\MaintenanceService" "Installed" 1

  ; Included here for debug purposes only.  
  ; These keys are used to bypass the installation dir is a valid installation
  ; check from the service so that tests can be run.
  ; WriteRegStr HKLM "${FallbackKey}\0" "name" "Mozilla Corporation"
  ; WriteRegStr HKLM "${FallbackKey}\0" "issuer" "Thawte Code Signing CA - G2"
  SetRegView lastused
SectionEnd

; By renaming before deleting we improve things slightly in case
; there is a file in use error. In this case a new install can happen.
Function un.RenameDelete
  Pop $9
  ; If the .moz-delete file already exists previously, delete it
  ; If it doesn't exist, the call is ignored.
  ; We don't need to pass /REBOOTOK here since it was already marked that way
  ; if it exists.
  Delete "$9.moz-delete"
  Rename "$9" "$9.moz-delete"
  ${If} ${Errors}
    Delete /REBOOTOK "$9"
  ${Else} 
    Delete /REBOOTOK "$9.moz-delete"
  ${EndIf}
  ClearErrors
FunctionEnd

Section "Uninstall"
  ; Delete the service so that no updates will be attempted
  nsExec::Exec '"$INSTDIR\maintenanceservice.exe" uninstall'

  Push "$INSTDIR\maintenanceservice.exe"
  Call un.RenameDelete
  Push "$INSTDIR\maintenanceservice_tmp.exe"
  Call un.RenameDelete
  Push "$INSTDIR\maintenanceservice.old"
  Call un.RenameDelete
  Push "$INSTDIR\Uninstall.exe"
  Call un.RenameDelete
  RMDir /REBOOTOK "$INSTDIR"

  DeleteRegKey HKLM "${MaintUninstallKey}"

  SetRegView 64
  DeleteRegValue HKLM "Software\Mozilla\MaintenanceService" "Installed"
  DeleteRegKey HKLM "${FallbackKey}\"
  SetRegView lastused
SectionEnd
