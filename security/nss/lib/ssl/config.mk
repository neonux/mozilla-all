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
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

ifdef NISCC_TEST
DEFINES += -DNISCC_TEST
endif

ifdef NSS_SURVIVE_DOUBLE_BYPASS_FAILURE
DEFINES += -DNSS_SURVIVE_DOUBLE_BYPASS_FAILURE
endif

CRYPTOLIB=$(SOFTOKEN_LIB_DIR)/$(LIB_PREFIX)freebl.$(LIB_SUFFIX)

EXTRA_LIBS += \
	$(CRYPTOLIB) \
	$(NULL)

ifeq (,$(filter-out WIN%,$(OS_TARGET)))

# don't want the 32 in the shared library name
SHARED_LIBRARY = $(OBJDIR)/$(DLL_PREFIX)$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
IMPORT_LIBRARY = $(OBJDIR)/$(IMPORT_LIB_PREFIX)$(LIBRARY_NAME)$(LIBRARY_VERSION)$(IMPORT_LIB_SUFFIX)

RES = $(OBJDIR)/ssl.res
RESNAME = ssl.rc

ifdef NS_USE_GCC
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-lnss3 \
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
else # ! NS_USE_GCC
EXTRA_SHARED_LIBS += \
	$(DIST)/lib/nss3.lib \
	$(DIST)/lib/nssutil3.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plc4.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plds4.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4.lib \
	$(NULL)
endif # NS_USE_GCC

else

# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-lnss3 \
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)

ifeq ($(OS_ARCH), BeOS)
EXTRA_SHARED_LIBS += -lbe
endif

endif

# Mozilla's mozilla/modules/zlib/src/zconf.h adds the MOZ_Z_ prefix to zlib
# exported symbols, which causes problem when NSS is built as part of Mozilla.
# So we add a NSS_ENABLE_ZLIB variable to allow Mozilla to turn this off.
NSS_ENABLE_ZLIB = 1
ifdef NSS_ENABLE_ZLIB

DEFINES += -DNSS_ENABLE_ZLIB

# If a platform has a system zlib, set USE_SYSTEM_ZLIB to 1 and
# ZLIB_LIBS to the linker command-line arguments for the system zlib
# (for example, -lz) in the platform's config file in coreconf.
ifdef USE_SYSTEM_ZLIB
OS_LIBS += $(ZLIB_LIBS)
else
ZLIB_LIBS = $(DIST)/lib/$(LIB_PREFIX)zlib.$(LIB_SUFFIX)
EXTRA_LIBS += $(ZLIB_LIBS)
endif

endif
