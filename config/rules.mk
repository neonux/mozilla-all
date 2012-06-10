# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
#

ifndef topsrcdir
$(error topsrcdir was not set))
endif

ifndef MOZILLA_DIR
MOZILLA_DIR = $(topsrcdir)
endif

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

ifndef INCLUDED_VERSION_MK
include $(topsrcdir)/config/version.mk
endif

USE_AUTOTARGETS_MK = 1
include $(topsrcdir)/config/makefiles/makeutils.mk

ifdef SDK_XPIDLSRCS
XPIDLSRCS += $(SDK_XPIDLSRCS)
endif
ifdef SDK_HEADERS
EXPORTS += $(SDK_HEADERS)
endif

REPORT_BUILD = @echo $(notdir $<)

ifeq ($(OS_ARCH),OS2)
EXEC			=
else
EXEC			= exec
endif

# Don't copy xulrunner files at install time, when using system xulrunner
ifdef SYSTEM_LIBXUL
  SKIP_COPY_XULRUNNER=1
endif

# ELOG prints out failed command when building silently (gmake -s).
ifneq (,$(findstring s, $(filter-out --%, $(MAKEFLAGS))))
  ELOG := $(EXEC) sh $(BUILD_TOOLS)/print-failed-commands.sh
else
  ELOG :=
endif

_VPATH_SRCS = $(abspath $<)

# Add $(DIST)/lib to VPATH so that -lfoo dependencies are followed
VPATH += $(DIST)/lib
ifdef LIBXUL_SDK
VPATH += $(LIBXUL_SDK)/lib
endif

ifdef EXTRA_DSO_LIBS
EXTRA_DSO_LIBS	:= $(call EXPAND_MOZLIBNAME,$(EXTRA_DSO_LIBS))
endif

################################################################################
# Testing frameworks support
################################################################################

testxpcobjdir = $(DEPTH)/_tests/xpcshell

ifdef ENABLE_TESTS

# Add test directories to the regular directories list. TEST_DIRS should
# arguably have the same status as TOOL_DIRS and other *_DIRS variables. It is
# coded this way until Makefiles stop using the "ifdef ENABLE_TESTS; DIRS +="
# convention.
#
# The current developer workflow expects tests to be updated when processing
# the default target. If we ever change this implementation, the behavior
# should be preserved or the change should be widely communicated. A
# consequence of not processing test dir targets during the default target is
# that changes to tests may not be updated and code could assume to pass
# locally against non-current test code.
DIRS += $(TEST_DIRS)

ifndef INCLUDED_TESTS_XPCSHELL_MK #{
  include $(topsrcdir)/config/makefiles/xpcshell.mk
endif #}

ifdef CPP_UNIT_TESTS

# Compile the tests to $(DIST)/bin.  Make lots of niceties available by default
# through TestHarness.h, by modifying the list of includes and the libs against
# which stuff links.
CPPSRCS += $(CPP_UNIT_TESTS)
SIMPLE_PROGRAMS += $(CPP_UNIT_TESTS:.cpp=$(BIN_SUFFIX))
INCLUDES += -I$(DIST)/include/testing
LIBS += $(XPCOM_GLUE_LDOPTS) $(NSPR_LIBS) $(MOZ_JS_LIBS)

# ...and run them the usual way
check::
	@$(EXIT_ON_ERROR) \
	  for f in $(subst .cpp,$(BIN_SUFFIX),$(CPP_UNIT_TESTS)); do \
	    XPCOM_DEBUG_BREAK=stack-and-abort $(RUN_TEST_PROGRAM) $(DIST)/bin/$$f; \
	  done

endif # CPP_UNIT_TESTS

.PHONY: check

endif # ENABLE_TESTS


#
# Library rules
#
# If FORCE_STATIC_LIB is set, build a static library.
# Otherwise, build a shared library.
#

ifndef LIBRARY
ifdef STATIC_LIBRARY_NAME
REAL_LIBRARY		:= $(LIB_PREFIX)$(STATIC_LIBRARY_NAME).$(LIB_SUFFIX)
# Only build actual library if it is installed in DIST/lib or SDK
ifeq (,$(SDK_LIBRARY)$(DIST_INSTALL)$(NO_EXPAND_LIBS))
LIBRARY			:= $(REAL_LIBRARY).$(LIBS_DESC_SUFFIX)
else
LIBRARY			:= $(REAL_LIBRARY) $(REAL_LIBRARY).$(LIBS_DESC_SUFFIX)
endif
endif # STATIC_LIBRARY_NAME
endif # LIBRARY

ifndef HOST_LIBRARY
ifdef HOST_LIBRARY_NAME
HOST_LIBRARY		:= $(LIB_PREFIX)$(HOST_LIBRARY_NAME).$(LIB_SUFFIX)
endif
endif

ifdef LIBRARY
ifdef FORCE_SHARED_LIB
ifdef MKSHLIB

ifdef LIB_IS_C_ONLY
MKSHLIB			= $(MKCSHLIB)
endif

ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
IMPORT_LIBRARY		:= $(LIB_PREFIX)$(SHARED_LIBRARY_NAME).$(IMPORT_LIB_SUFFIX)
endif

ifeq (OS2,$(OS_ARCH))
ifdef SHORT_LIBNAME
SHARED_LIBRARY_NAME	:= $(SHORT_LIBNAME)
endif
endif

ifdef MAKE_FRAMEWORK
SHARED_LIBRARY		:= $(SHARED_LIBRARY_NAME)
else
SHARED_LIBRARY		:= $(DLL_PREFIX)$(SHARED_LIBRARY_NAME)$(DLL_SUFFIX)
endif

ifeq ($(OS_ARCH),OS2)
DEF_FILE		:= $(SHARED_LIBRARY:.dll=.def)
endif

EMBED_MANIFEST_AT=2

endif # MKSHLIB
endif # FORCE_SHARED_LIB
endif # LIBRARY

ifdef FORCE_STATIC_LIB
ifndef FORCE_SHARED_LIB
SHARED_LIBRARY		:= $(NULL)
DEF_FILE		:= $(NULL)
IMPORT_LIBRARY		:= $(NULL)
endif
endif

ifdef FORCE_SHARED_LIB
ifndef FORCE_STATIC_LIB
LIBRARY := $(NULL)
endif
endif

ifdef JAVA_LIBRARY_NAME
JAVA_LIBRARY := $(JAVA_LIBRARY_NAME).jar
endif

ifeq ($(OS_ARCH),WINNT)
ifndef GNU_CC

#
# Unless we're building SIMPLE_PROGRAMS, all C++ files share a PDB file per
# directory. For parallel builds, this PDB file is shared and locked by
# MSPDBSRV.EXE, starting with MSVC8 SP1. If you're using MSVC 7.1 or MSVC8
# without SP1, don't do parallel builds.
#
# The final PDB for libraries and programs is created by the linker and uses
# a different name from the single PDB file created by the compiler. See
# bug 462740.
#

ifdef SIMPLE_PROGRAMS
COMPILE_PDBFILE = $(basename $(@F)).pdb
else
COMPILE_PDBFILE = generated.pdb
endif

LINK_PDBFILE = $(basename $(@F)).pdb
ifdef MOZ_DEBUG
CODFILE=$(basename $(@F)).cod
endif

ifdef MOZ_MAPINFO
ifdef SHARED_LIBRARY_NAME
MAPFILE=$(SHARED_LIBRARY_NAME).map
else
MAPFILE=$(basename $(@F)).map
endif # SHARED_LIBRARY_NAME
endif # MOZ_MAPINFO

ifdef DEFFILE
OS_LDFLAGS += -DEF:$(call normalizepath,$(DEFFILE))
EXTRA_DEPS += $(DEFFILE)
endif

ifdef MAPFILE
OS_LDFLAGS += -MAP:$(MAPFILE)
endif

else #!GNU_CC

ifdef DEFFILE
OS_LDFLAGS += $(call normalizepath,$(DEFFILE))
EXTRA_DEPS += $(DEFFILE)
endif

endif # !GNU_CC

endif # WINNT

ifeq ($(SOLARIS_SUNPRO_CXX),1)
ifeq (86,$(findstring 86,$(OS_TEST)))
OS_LDFLAGS += -M $(topsrcdir)/config/solaris_ia32.map
endif # x86
endif # Solaris Sun Studio C++

ifeq ($(HOST_OS_ARCH),WINNT)
HOST_PDBFILE=$(basename $(@F)).pdb
endif

# Don't build SIMPLE_PROGRAMS during the MOZ_PROFILE_GENERATE pass
ifdef MOZ_PROFILE_GENERATE
SIMPLE_PROGRAMS :=
endif

ifndef TARGETS
TARGETS			= $(LIBRARY) $(SHARED_LIBRARY) $(PROGRAM) $(SIMPLE_PROGRAMS) $(HOST_LIBRARY) $(HOST_PROGRAM) $(HOST_SIMPLE_PROGRAMS) $(JAVA_LIBRARY)
endif

COBJS = $(CSRCS:.c=.$(OBJ_SUFFIX))
SOBJS = $(SSRCS:.S=.$(OBJ_SUFFIX))
CCOBJS = $(patsubst %.cc,%.$(OBJ_SUFFIX),$(filter %.cc,$(CPPSRCS)))
CPPOBJS = $(patsubst %.cpp,%.$(OBJ_SUFFIX),$(filter %.cpp,$(CPPSRCS)))
CMOBJS = $(CMSRCS:.m=.$(OBJ_SUFFIX))
CMMOBJS = $(CMMSRCS:.mm=.$(OBJ_SUFFIX))
ASOBJS = $(ASFILES:.$(ASM_SUFFIX)=.$(OBJ_SUFFIX))
ifndef OBJS
_OBJS = $(COBJS) $(SOBJS) $(CCOBJS) $(CPPOBJS) $(CMOBJS) $(CMMOBJS) $(ASOBJS)
OBJS = $(strip $(_OBJS))
endif

HOST_COBJS = $(addprefix host_,$(HOST_CSRCS:.c=.$(OBJ_SUFFIX)))
HOST_CCOBJS = $(addprefix host_,$(patsubst %.cc,%.$(OBJ_SUFFIX),$(filter %.cc,$(HOST_CPPSRCS))))
HOST_CPPOBJS = $(addprefix host_,$(patsubst %.cpp,%.$(OBJ_SUFFIX),$(filter %.cpp,$(HOST_CPPSRCS))))
HOST_CMOBJS = $(addprefix host_,$(HOST_CMSRCS:.m=.$(OBJ_SUFFIX)))
HOST_CMMOBJS = $(addprefix host_,$(HOST_CMMSRCS:.mm=.$(OBJ_SUFFIX)))
ifndef HOST_OBJS
_HOST_OBJS = $(HOST_COBJS) $(HOST_CCOBJS) $(HOST_CPPOBJS) $(HOST_CMOBJS) $(HOST_CMMOBJS)
HOST_OBJS = $(strip $(_HOST_OBJS))
endif

ifndef MOZ_AUTO_DEPS
ifneq (,$(OBJS)$(XPIDLSRCS)$(SIMPLE_PROGRAMS))
MDDEPFILES		= $(addprefix $(MDDEPDIR)/,$(OBJS:.$(OBJ_SUFFIX)=.pp))
ifndef NO_GEN_XPT
MDDEPFILES		+= $(addprefix $(MDDEPDIR)/,$(XPIDLSRCS:.idl=.h.pp) $(XPIDLSRCS:.idl=.xpt.pp))
endif
endif
endif

ALL_TRASH = \
	$(GARBAGE) $(TARGETS) $(OBJS) $(PROGOBJS) LOGS TAGS a.out \
	$(filter-out $(ASFILES),$(OBJS:.$(OBJ_SUFFIX)=.s)) $(OBJS:.$(OBJ_SUFFIX)=.ii) \
	$(OBJS:.$(OBJ_SUFFIX)=.i) $(OBJS:.$(OBJ_SUFFIX)=.i_o) \
	$(HOST_PROGOBJS) $(HOST_OBJS) $(IMPORT_LIBRARY) $(DEF_FILE)\
	$(EXE_DEF_FILE) so_locations _gen _stubs $(wildcard *.res) $(wildcard *.RES) \
	$(wildcard *.pdb) $(CODFILE) $(MAPFILE) $(IMPORT_LIBRARY) \
	$(SHARED_LIBRARY:$(DLL_SUFFIX)=.exp) $(wildcard *.ilk) \
	$(PROGRAM:$(BIN_SUFFIX)=.exp) $(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.exp) \
	$(PROGRAM:$(BIN_SUFFIX)=.lib) $(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.lib) \
	$(SIMPLE_PROGRAMS:$(BIN_SUFFIX)=.$(OBJ_SUFFIX)) \
	$(wildcard gts_tmp_*) $(LIBRARY:%.a=.%.timestamp)
ALL_TRASH_DIRS = \
	$(GARBAGE_DIRS) /no-such-file

ifdef QTDIR
GARBAGE                 += $(MOCSRCS)
GARBAGE                 += $(RCCSRCS)
endif

ifdef SIMPLE_PROGRAMS
GARBAGE			+= $(SIMPLE_PROGRAMS:%=%.$(OBJ_SUFFIX))
endif

ifdef HOST_SIMPLE_PROGRAMS
GARBAGE			+= $(HOST_SIMPLE_PROGRAMS:%=%.$(OBJ_SUFFIX))
endif

#
# the Solaris WorkShop template repository cache.  it occasionally can get
# out of sync, so targets like clobber should kill it.
#
ifeq ($(SOLARIS_SUNPRO_CXX),1)
GARBAGE_DIRS += SunWS_cache
endif

XPIDL_GEN_DIR		= _xpidlgen

ifdef MOZ_UPDATE_XTERM
# Its good not to have a newline at the end of the titlebar string because it
# makes the make -s output easier to read.  Echo -n does not work on all
# platforms, but we can trick sed into doing it.
UPDATE_TITLE = sed -e "s!Y!$(1) in $(shell $(BUILD_TOOLS)/print-depth-path.sh)/$(2)!" $(MOZILLA_DIR)/config/xterm.str;
endif

define SUBMAKE # $(call SUBMAKE,target,directory)
+@$(UPDATE_TITLE)
+$(MAKE) $(if $(2),-C $(2)) $(1)

endef # The extra line is important here! don't delete it

ifneq (,$(strip $(DIRS)))
LOOP_OVER_DIRS = \
  $(foreach dir,$(DIRS),$(call SUBMAKE,$@,$(dir)))
endif

# we only use this for the makefiles target and other stuff that doesn't matter
ifneq (,$(strip $(PARALLEL_DIRS)))
LOOP_OVER_PARALLEL_DIRS = \
  $(foreach dir,$(PARALLEL_DIRS),$(call SUBMAKE,$@,$(dir)))
endif

ifneq (,$(strip $(STATIC_DIRS)))
LOOP_OVER_STATIC_DIRS = \
  $(foreach dir,$(STATIC_DIRS),$(call SUBMAKE,$@,$(dir)))
endif

ifneq (,$(strip $(TOOL_DIRS)))
LOOP_OVER_TOOL_DIRS = \
  $(foreach dir,$(TOOL_DIRS),$(call SUBMAKE,$@,$(dir)))
endif

#
# Now we can differentiate between objects used to build a library, and
# objects used to build an executable in the same directory.
#
ifndef PROGOBJS
PROGOBJS		= $(OBJS)
endif

ifndef HOST_PROGOBJS
HOST_PROGOBJS		= $(HOST_OBJS)
endif

# MAKE_DIRS: List of directories to build while looping over directories.
# A Makefile that needs $(MDDEPDIR) created but doesn't set any of these
# variables we know to check can just set NEED_MDDEPDIR explicitly.
ifneq (,$(OBJS)$(XPIDLSRCS)$(SIMPLE_PROGRAMS)$(NEED_MDDEPDIR))
MAKE_DIRS       += $(CURDIR)/$(MDDEPDIR)
GARBAGE_DIRS    += $(CURDIR)/$(MDDEPDIR)
endif

#
# Tags: emacs (etags), vi (ctags)
# TAG_PROGRAM := ctags -L -
#
TAG_PROGRAM		= xargs etags -a

#
# Turn on C++ linking if we have any .cpp or .mm files
# (moved this from config.mk so that config.mk can be included
#  before the CPPSRCS are defined)
#
ifneq ($(HOST_CPPSRCS)$(HOST_CMMSRCS),)
HOST_CPP_PROG_LINK	= 1
endif

#
# This will strip out symbols that the component should not be
# exporting from the .dynsym section.
#
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += $(MOZ_COMPONENTS_VERSION_SCRIPT_LDFLAGS)
endif # IS_COMPONENT

#
# Enforce the requirement that MODULE_NAME must be set
# for components in static builds
#
ifdef IS_COMPONENT
ifdef EXPORT_LIBRARY
ifndef FORCE_SHARED_LIB
ifndef MODULE_NAME
$(error MODULE_NAME is required for components which may be used in static builds)
endif
endif
endif
endif

#
# MacOS X specific stuff
#

ifeq ($(OS_ARCH),Darwin)
ifdef SHARED_LIBRARY
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS	+= -bundle
else
EXTRA_DSO_LDOPTS	+= -dynamiclib -install_name @executable_path/$(SHARED_LIBRARY) -compatibility_version 1 -current_version 1 -single_module
endif
endif
endif

#
# On NetBSD a.out systems, use -Bsymbolic.  This fixes what would otherwise be
# fatal symbol name clashes between components.
#
ifeq ($(OS_ARCH),NetBSD)
ifeq ($(DLL_SUFFIX),.so.1.0)
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
endif
endif
endif

ifeq ($(OS_ARCH),FreeBSD)
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
endif
endif

ifeq ($(OS_ARCH),NetBSD)
ifneq (,$(filter arc cobalt hpcmips mipsco newsmips pmax sgimips,$(OS_TEST)))
ifeq ($(MODULE),layout)
OS_CFLAGS += -Wa,-xgot
OS_CXXFLAGS += -Wa,-xgot
endif
endif
endif

#
# HP-UXBeOS specific section: for COMPONENTS only, add -Bsymbolic flag
# which uses internal symbols first
#
ifeq ($(OS_ARCH),HP-UX)
ifdef IS_COMPONENT
ifeq ($(GNU_CC)$(GNU_CXX),)
EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
ifneq ($(HAS_EXTRAEXPORTS),1)
MKSHLIB  += -Wl,+eNSGetModule -Wl,+eerrno
MKCSHLIB += +eNSGetModule +eerrno
ifneq ($(OS_TEST),ia64)
MKSHLIB  += -Wl,+e_shlInit
MKCSHLIB += +e_shlInit
endif # !ia64
endif # !HAS_EXTRAEXPORTS
endif # non-gnu compilers
endif # IS_COMPONENT
endif # HP-UX

ifeq ($(OS_ARCH),AIX)
ifdef IS_COMPONENT
ifneq ($(HAS_EXTRAEXPORTS),1)
MKSHLIB += -bE:$(MOZILLA_DIR)/build/unix/aix.exp -bnoexpall
MKCSHLIB += -bE:$(MOZILLA_DIR)/build/unix/aix.exp -bnoexpall
endif # HAS_EXTRAEXPORTS
endif # IS_COMPONENT
endif # AIX

#
# OSF1: add -B symbolic flag for components
#
ifeq ($(OS_ARCH),OSF1)
ifdef IS_COMPONENT
ifeq ($(GNU_CC)$(GNU_CXX),)
EXTRA_DSO_LDOPTS += -B symbolic
endif
endif
endif

#
# Linux: add -Bsymbolic flag for components
#
ifeq ($(OS_ARCH),Linux)
ifdef IS_COMPONENT
EXTRA_DSO_LDOPTS += -Wl,-Bsymbolic
endif
endif

#
# GNU doesn't have path length limitation
#

ifeq ($(OS_ARCH),GNU)
OS_CPPFLAGS += -DPATH_MAX=1024 -DMAXPATHLEN=1024
endif

#
# MINGW32
#
ifeq ($(OS_ARCH),WINNT)
ifdef GNU_CC
ifndef IS_COMPONENT
DSO_LDOPTS += -Wl,--out-implib -Wl,$(IMPORT_LIBRARY)
endif
endif
endif

ifeq ($(USE_TVFS),1)
IFLAGS1 = -rb
IFLAGS2 = -rb
else
IFLAGS1 = -m 644
IFLAGS2 = -m 755
endif

ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
OUTOPTION = -Fo# eol
else
OUTOPTION = -o # eol
endif # WINNT && !GNU_CC

ifneq (,$(filter ml%,$(AS)))
ASOUTOPTION = -Fo# eol
else
ASOUTOPTION = -o # eol
endif

ifeq (,$(CROSS_COMPILE))
HOST_OUTOPTION = $(OUTOPTION)
else
HOST_OUTOPTION = -o # eol
endif
################################################################################

# SUBMAKEFILES: List of Makefiles for next level down.
#   This is used to update or create the Makefiles before invoking them.
SUBMAKEFILES += $(addsuffix /Makefile, $(DIRS) $(TOOL_DIRS) $(PARALLEL_DIRS))

# The root makefile doesn't want to do a plain export/libs, because
# of the tiers and because of libxul. Suppress the default rules in favor
# of something else. Makefiles which use this var *must* provide a sensible
# default rule before including rules.mk
ifndef SUPPRESS_DEFAULT_RULES
ifdef TIERS
default all alldep::
	$(foreach tier,$(TIERS),$(call SUBMAKE,tier_$(tier)))
else

default all::
ifneq (,$(strip $(STATIC_DIRS)))
	$(foreach dir,$(STATIC_DIRS),$(call SUBMAKE,,$(dir)))
endif
	$(MAKE) export
	$(MAKE) libs
	$(MAKE) tools

# Do depend as well
alldep::
	$(MAKE) export
	$(MAKE) depend
	$(MAKE) libs
	$(MAKE) tools

endif # TIERS
endif # SUPPRESS_DEFAULT_RULES

ifeq ($(filter s,$(MAKEFLAGS)),)
ECHO := echo
QUIET :=
else
ECHO := true
QUIET := -q
endif

MAKE_TIER_SUBMAKEFILES = +$(if $(tier_$*_dirs),$(MAKE) $(addsuffix /Makefile,$(tier_$*_dirs)))

$(foreach tier,$(TIERS),tier_$(tier))::
	@$(ECHO) "$@: $($@_staticdirs) $($@_dirs)"
	$(foreach dir,$($@_staticdirs),$(call SUBMAKE,,$(dir)))
	$(MAKE) export_$@
	$(MAKE) libs_$@
	$(MAKE) tools_$@

# Do everything from scratch
everything::
	$(MAKE) clean
	$(MAKE) alldep

# Add dummy depend target for tinderboxes
depend::

# Target to only regenerate makefiles
makefiles: $(SUBMAKEFILES)
ifneq (,$(DIRS)$(TOOL_DIRS)$(PARALLEL_DIRS))
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)
endif

include $(topsrcdir)/config/makefiles/target_export.mk
include $(topsrcdir)/config/makefiles/target_tools.mk

#
# Rule to create list of libraries for final link
#
export::
ifdef LIBRARY_NAME
ifdef EXPORT_LIBRARY
ifdef IS_COMPONENT
else # !IS_COMPONENT
	$(PYTHON) $(MOZILLA_DIR)/config/buildlist.py $(FINAL_LINK_LIBS) $(STATIC_LIBRARY_NAME)
endif # IS_COMPONENT
endif # EXPORT_LIBRARY
endif # LIBRARY_NAME

ifneq (,$(filter-out %.$(LIB_SUFFIX),$(SHARED_LIBRARY_LIBS)))
$(error SHARED_LIBRARY_LIBS must contain .$(LIB_SUFFIX) files only)
endif

# Create dependencies on static (and shared EXTRA_DSO_LIBS) libraries
ifneq (,$(strip $(filter %.$(LIB_SUFFIX),$(LIBS) $(EXTRA_DSO_LDOPTS)) $(SHARED_LIBRARY_LIBS) $(EXTRA_DSO_LIBS)))
$(MDDEPDIR)/libs: Makefile.in
	@mkdir -p $(MDDEPDIR)
	@$(EXPAND_LIBS_DEPS) LIBS_DEPS = $(filter %.$(LIB_SUFFIX),$(LIBS)) , \
	                     SHARED_LIBRARY_LIBS_DEPS = $(SHARED_LIBRARY_LIBS) , \
	                     DSO_LDOPTS_DEPS = $(EXTRA_DSO_LIBS) $(filter %.$(LIB_SUFFIX), $(EXTRA_DSO_LDOPTS)) > $@

ifneq (,$(wildcard $(MDDEPDIR)/libs))
include $(MDDEPDIR)/libs
endif

$(MDDEPDIR)/libs: $(wildcard $(filter %.$(LIBS_DESC_SUFFIX),$(LIBS_DEPS) $(SHARED_LIBRARY_LIBS_DEPS) $(DSO_LDOPTS_DEPS)))

EXTRA_DEPS += $(MDDEPDIR)/libs
endif

HOST_LIBS_DEPS = $(filter %.$(LIB_SUFFIX),$(HOST_LIBS))

# Dependencies which, if modified, should cause everything to rebuild
GLOBAL_DEPS += Makefile Makefile.in $(DEPTH)/config/autoconf.mk $(topsrcdir)/config/config.mk

##############################################
include $(topsrcdir)/config/makefiles/target_libs.mk

##############################################
ifndef NO_PROFILE_GUIDED_OPTIMIZE
ifdef MOZ_PROFILE_USE
ifeq ($(OS_ARCH)_$(GNU_CC), WINNT_)
# When building with PGO, we have to make sure to re-link
# in the MOZ_PROFILE_USE phase if we linked in the
# MOZ_PROFILE_GENERATE phase. We'll touch this pgo.relink
# file in the link rule in the GENERATE phase to indicate
# that we need a relink.
ifdef SHARED_LIBRARY
$(SHARED_LIBRARY): pgo.relink
endif
ifdef PROGRAM
$(PROGRAM): pgo.relink
endif

# In the second pass, we need to merge the pgc files into the pgd file.
# The compiler would do this for us automatically if they were in the right
# place, but they're in dist/bin.
ifneq (,$(SHARED_LIBRARY)$(PROGRAM))
export::
ifdef PROGRAM
	$(PYTHON) $(topsrcdir)/build/win32/pgomerge.py \
	  $(PROGRAM:$(BIN_SUFFIX)=) $(DIST)/bin
endif
ifdef SHARED_LIBRARY
	$(PYTHON) $(topsrcdir)/build/win32/pgomerge.py \
	  $(SHARED_LIBRARY_NAME) $(DIST)/bin
endif
endif # SHARED_LIBRARY || PROGRAM
endif # WINNT_
endif # MOZ_PROFILE_USE
ifdef MOZ_PROFILE_GENERATE
# Clean up profiling data during PROFILE_GENERATE phase
export::
ifeq ($(OS_ARCH)_$(GNU_CC), WINNT_)
	$(foreach pgd,$(wildcard *.pgd),pgomgr -clear $(pgd);)
else
ifdef GNU_CC
	-$(RM) *.gcda
endif
endif
endif

ifneq (,$(MOZ_PROFILE_GENERATE)$(MOZ_PROFILE_USE))
ifdef GNU_CC
# Force rebuilding libraries and programs in both passes because each
# pass uses different object files.
$(PROGRAM) $(SHARED_LIBRARY) $(LIBRARY): FORCE
endif
endif

endif # NO_PROFILE_GUIDED_OPTIMIZE

##############################################

checkout:
	$(MAKE) -C $(topsrcdir) -f client.mk checkout

clean clobber realclean clobber_all:: $(SUBMAKEFILES)
	-$(RM) $(ALL_TRASH)
	-$(RM) -r $(ALL_TRASH_DIRS)
	$(foreach dir,$(PARALLEL_DIRS) $(DIRS) $(STATIC_DIRS) $(TOOL_DIRS),-$(call SUBMAKE,$@,$(dir)))

distclean:: $(SUBMAKEFILES)
	$(foreach dir,$(PARALLEL_DIRS) $(DIRS) $(STATIC_DIRS) $(TOOL_DIRS),-$(call SUBMAKE,$@,$(dir)))
	-$(RM) -r $(ALL_TRASH_DIRS)
	-$(RM) $(ALL_TRASH)  \
	Makefile .HSancillary \
	$(wildcard *.$(OBJ_SUFFIX)) $(wildcard *.ho) $(wildcard host_*.o*) \
	$(wildcard *.$(LIB_SUFFIX)) $(wildcard *$(DLL_SUFFIX)) \
	$(wildcard *.$(IMPORT_LIB_SUFFIX))
ifeq ($(OS_ARCH),OS2)
	-$(RM) $(PROGRAM:.exe=.map)
endif

alltags:
	$(RM) TAGS
	find $(topsrcdir) -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' -o -name '*.idl' \) -print | $(TAG_PROGRAM)

#
# PROGRAM = Foo
# creates OBJS, links with LIBS to create Foo
#
$(PROGRAM): $(PROGOBJS) $(LIBS_DEPS) $(EXTRA_DEPS) $(EXE_DEF_FILE) $(RESFILE) $(GLOBAL_DEPS)
	@$(RM) $@.manifest
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
	$(EXPAND_LD) -NOLOGO -OUT:$@ -PDB:$(LINK_PDBFILE) $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(MOZ_GLUE_PROGRAM_LDFLAGS) $(PROGOBJS) $(RESFILE) $(LIBS) $(EXTRA_LIBS) $(OS_LIBS)
ifdef MSMANIFEST_TOOL
	@if test -f $@.manifest; then \
		if test -f "$(srcdir)/$@.manifest"; then \
			echo "Embedding manifest from $(srcdir)/$@.manifest and $@.manifest"; \
			mt.exe -NOLOGO -MANIFEST "$(win_srcdir)/$@.manifest" $@.manifest -OUTPUTRESOURCE:$@\;1; \
		else \
			echo "Embedding manifest from $@.manifest"; \
			mt.exe -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		fi; \
	elif test -f "$(srcdir)/$@.manifest"; then \
		echo "Embedding manifest from $(srcdir)/$@.manifest"; \
		mt.exe -NOLOGO -MANIFEST "$(win_srcdir)/$@.manifest" -OUTPUTRESOURCE:$@\;1; \
	fi
endif	# MSVC with manifest tool
ifdef MOZ_PROFILE_GENERATE
# touch it a few seconds into the future to work around FAT's
# 2-second granularity
	touch -t `date +%Y%m%d%H%M.%S -d "now+5seconds"` pgo.relink
endif
else # !WINNT || GNU_CC
	$(EXPAND_CCC) -o $@ $(CXXFLAGS) $(PROGOBJS) $(RESFILE) $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(WRAP_LDFLAGS) $(LIBS_DIR) $(LIBS) $(MOZ_GLUE_PROGRAM_LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS) $(BIN_FLAGS) $(EXE_DEF_FILE)
	@$(call CHECK_STDCXX,$@)
endif # WINNT && !GNU_CC

ifdef ENABLE_STRIP
	$(STRIP) $@
endif
ifdef MOZ_POST_PROGRAM_COMMAND
	$(MOZ_POST_PROGRAM_COMMAND) $@
endif

$(HOST_PROGRAM): $(HOST_PROGOBJS) $(HOST_LIBS_DEPS) $(HOST_EXTRA_DEPS) $(GLOBAL_DEPS)
ifeq (_WINNT,$(GNU_CC)_$(HOST_OS_ARCH))
	$(HOST_LD) -NOLOGO -OUT:$@ -PDB:$(HOST_PDBFILE) $(HOST_OBJS) $(WIN32_EXE_LDFLAGS) $(HOST_LDFLAGS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
ifdef MSMANIFEST_TOOL
	@if test -f $@.manifest; then \
		if test -f "$(srcdir)/$@.manifest"; then \
			echo "Embedding manifest from $(srcdir)/$@.manifest and $@.manifest"; \
			mt.exe -NOLOGO -MANIFEST "$(win_srcdir)/$@.manifest" $@.manifest -OUTPUTRESOURCE:$@\;1; \
		else \
			echo "Embedding manifest from $@.manifest"; \
			mt.exe -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		fi; \
	elif test -f "$(srcdir)/$@.manifest"; then \
		echo "Embedding manifest from $(srcdir)/$@.manifest"; \
		mt.exe -NOLOGO -MANIFEST "$(win_srcdir)/$@.manifest" -OUTPUTRESOURCE:$@\;1; \
	fi
endif	# MSVC with manifest tool
else
ifeq ($(HOST_CPP_PROG_LINK),1)
	$(HOST_CXX) -o $@ $(HOST_CXXFLAGS) $(HOST_LDFLAGS) $(HOST_PROGOBJS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
else
	$(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_LDFLAGS) $(HOST_PROGOBJS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
endif # HOST_CPP_PROG_LINK
endif

#
# This is an attempt to support generation of multiple binaries
# in one directory, it assumes everything to compile Foo is in
# Foo.o (from either Foo.c or Foo.cpp).
#
# SIMPLE_PROGRAMS = Foo Bar
# creates Foo.o Bar.o, links with LIBS to create Foo, Bar.
#
$(SIMPLE_PROGRAMS): %$(BIN_SUFFIX): %.$(OBJ_SUFFIX) $(LIBS_DEPS) $(EXTRA_DEPS) $(GLOBAL_DEPS)
ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
	$(EXPAND_LD) -nologo -out:$@ -pdb:$(LINK_PDBFILE) $< $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(MOZ_GLUE_PROGRAM_LDFLAGS) $(LIBS) $(EXTRA_LIBS) $(OS_LIBS)
ifdef MSMANIFEST_TOOL
	@if test -f $@.manifest; then \
		mt.exe -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		rm -f $@.manifest; \
	fi
endif	# MSVC with manifest tool
else
	$(EXPAND_CCC) $(CXXFLAGS) -o $@ $< $(WIN32_EXE_LDFLAGS) $(LDFLAGS) $(WRAP_LDFLAGS) $(LIBS_DIR) $(LIBS) $(MOZ_GLUE_PROGRAM_LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS) $(BIN_FLAGS)
	@$(call CHECK_STDCXX,$@)
endif # WINNT && !GNU_CC

ifdef ENABLE_STRIP
	$(STRIP) $@
endif
ifdef MOZ_POST_PROGRAM_COMMAND
	$(MOZ_POST_PROGRAM_COMMAND) $@
endif

$(HOST_SIMPLE_PROGRAMS): host_%$(HOST_BIN_SUFFIX): host_%.$(OBJ_SUFFIX) $(HOST_LIBS_DEPS) $(HOST_EXTRA_DEPS) $(GLOBAL_DEPS)
ifeq (WINNT_,$(HOST_OS_ARCH)_$(GNU_CC))
	$(HOST_LD) -NOLOGO -OUT:$@ -PDB:$(HOST_PDBFILE) $< $(WIN32_EXE_LDFLAGS) $(HOST_LIBS) $(HOST_EXTRA_LIBS)
else
ifneq (,$(HOST_CPPSRCS)$(USE_HOST_CXX))
	$(HOST_CXX) $(HOST_OUTOPTION)$@ $(HOST_CXXFLAGS) $(INCLUDES) $< $(HOST_LIBS) $(HOST_EXTRA_LIBS)
else
	$(HOST_CC) $(HOST_OUTOPTION)$@ $(HOST_CFLAGS) $(INCLUDES) $< $(HOST_LIBS) $(HOST_EXTRA_LIBS)
endif
endif

ifdef DTRACE_PROBE_OBJ
EXTRA_DEPS += $(DTRACE_PROBE_OBJ)
OBJS += $(DTRACE_PROBE_OBJ)
endif

$(filter %.$(LIB_SUFFIX),$(LIBRARY)): $(OBJS) $(LOBJS) $(SHARED_LIBRARY_LIBS_DEPS) $(EXTRA_DEPS) $(GLOBAL_DEPS)
	$(RM) $(LIBRARY)
	$(EXPAND_AR) $(AR_FLAGS) $(OBJS) $(LOBJS) $(SHARED_LIBRARY_LIBS)
	$(RANLIB) $@

$(filter-out %.$(LIB_SUFFIX),$(LIBRARY)): $(filter %.$(LIB_SUFFIX),$(LIBRARY)) $(OBJS) $(LOBJS) $(SHARED_LIBRARY_LIBS_DEPS) $(EXTRA_DEPS) $(GLOBAL_DEPS)
# When we only build a library descriptor, blow out any existing library
	$(if $(filter %.$(LIB_SUFFIX),$(LIBRARY)),,$(RM) $(REAL_LIBRARY) $(EXPORT_LIBRARY:%=%/$(REAL_LIBRARY)))
	$(EXPAND_LIBS_GEN) $(OBJS) $(LOBJS) $(SHARED_LIBRARY_LIBS) > $@

ifeq ($(OS_ARCH),WINNT)
$(IMPORT_LIBRARY): $(SHARED_LIBRARY)
endif

ifeq ($(OS_ARCH),OS2)
$(DEF_FILE): $(OBJS) $(SHARED_LIBRARY_LIBS)
	$(RM) $@
	echo LIBRARY $(SHARED_LIBRARY_NAME) INITINSTANCE TERMINSTANCE > $@
	echo PROTMODE >> $@
	echo CODE    LOADONCALL MOVEABLE DISCARDABLE >> $@
	echo DATA    PRELOAD MOVEABLE MULTIPLE NONSHARED >> $@
	echo EXPORTS >> $@

	$(ADD_TO_DEF_FILE)

$(IMPORT_LIBRARY): $(SHARED_LIBRARY)
	$(RM) $@
	$(IMPLIB) $@ $^
	$(RANLIB) $@
endif # OS/2

$(HOST_LIBRARY): $(HOST_OBJS) Makefile
	$(RM) $@
	$(HOST_AR) $(HOST_AR_FLAGS) $(HOST_OBJS)
	$(HOST_RANLIB) $@

ifdef HAVE_DTRACE
ifndef XP_MACOSX
ifdef DTRACE_PROBE_OBJ
ifndef DTRACE_LIB_DEPENDENT
NON_DTRACE_OBJS := $(filter-out $(DTRACE_PROBE_OBJ),$(OBJS))
$(DTRACE_PROBE_OBJ): $(NON_DTRACE_OBJS)
	dtrace -G -C -s $(MOZILLA_DTRACE_SRC) -o $(DTRACE_PROBE_OBJ) $(NON_DTRACE_OBJS)
endif
endif
endif
endif

# On Darwin (Mac OS X), dwarf2 debugging uses debug info left in .o files,
# so instead of deleting .o files after repacking them into a dylib, we make
# symlinks back to the originals. The symlinks are a no-op for stabs debugging,
# so no need to conditionalize on OS version or debugging format.

$(SHARED_LIBRARY): $(OBJS) $(LOBJS) $(DEF_FILE) $(RESFILE) $(SHARED_LIBRARY_LIBS_DEPS) $(LIBRARY) $(EXTRA_DEPS) $(DSO_LDOPTS_DEPS) $(GLOBAL_DEPS)
ifndef INCREMENTAL_LINKER
	$(RM) $@
endif
ifdef DTRACE_LIB_DEPENDENT
ifndef XP_MACOSX
	dtrace -G -C -s $(MOZILLA_DTRACE_SRC) -o  $(DTRACE_PROBE_OBJ) $(shell $(EXPAND_LIBS) $(MOZILLA_PROBE_LIBS))
endif
	$(EXPAND_MKSHLIB) $(SHLIB_LDSTARTFILE) $(OBJS) $(LOBJS) $(SUB_SHLOBJS) $(DTRACE_PROBE_OBJ) $(MOZILLA_PROBE_LIBS) $(RESFILE) $(LDFLAGS) $(WRAP_LDFLAGS) $(SHARED_LIBRARY_LIBS) $(EXTRA_DSO_LDOPTS) $(MOZ_GLUE_LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE) $(SHLIB_LDENDFILE)
	@$(RM) $(DTRACE_PROBE_OBJ)
else # ! DTRACE_LIB_DEPENDENT
	$(EXPAND_MKSHLIB) $(SHLIB_LDSTARTFILE) $(OBJS) $(LOBJS) $(SUB_SHLOBJS) $(RESFILE) $(LDFLAGS) $(WRAP_LDFLAGS) $(SHARED_LIBRARY_LIBS) $(EXTRA_DSO_LDOPTS) $(MOZ_GLUE_LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS) $(DEF_FILE) $(SHLIB_LDENDFILE)
endif # DTRACE_LIB_DEPENDENT
	@$(call CHECK_STDCXX,$@)

ifeq (_WINNT,$(GNU_CC)_$(OS_ARCH))
ifdef MSMANIFEST_TOOL
ifdef EMBED_MANIFEST_AT
	@if test -f $@.manifest; then \
		mt.exe -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;$(EMBED_MANIFEST_AT); \
		rm -f $@.manifest; \
	fi
endif   # EMBED_MANIFEST_AT
endif	# MSVC with manifest tool
ifdef MOZ_PROFILE_GENERATE
	touch -t `date +%Y%m%d%H%M.%S -d "now+5seconds"` pgo.relink
endif
endif	# WINNT && !GCC
	@$(RM) foodummyfilefoo $(DELETE_AFTER_LINK)
	chmod +x $@
ifdef ENABLE_STRIP
	$(STRIP) $@
endif
ifdef MOZ_POST_DSO_LIB_COMMAND
	$(MOZ_POST_DSO_LIB_COMMAND) $@
endif

ifdef MOZ_AUTO_DEPS
ifdef COMPILER_DEPEND
ifeq ($(SOLARIS_SUNPRO_CC),1)
_MDDEPFILE = $(MDDEPDIR)/$(@F).pp

define MAKE_DEPS_AUTO_CC
if test -d $(@D); then \
	echo "Building deps for $< using Sun Studio cc"; \
	$(CC) $(COMPILE_CFLAGS) -xM  $< >$(_MDDEPFILE) ; \
fi
endef
define MAKE_DEPS_AUTO_CXX
if test -d $(@D); then \
	echo "Building deps for $< using Sun Studio CC"; \
	$(CXX) $(COMPILE_CXXFLAGS) -xM $< >$(_MDDEPFILE) ; \
fi
endef
endif # Sun Studio on Solaris
else # COMPILER_DEPEND
#
# Generate dependencies on the fly
#
_MDDEPFILE = $(MDDEPDIR)/$(@F).pp

define MAKE_DEPS_AUTO
if test -d $(@D); then \
	echo "Building deps for $<"; \
	$(MKDEPEND) -o'.$(OBJ_SUFFIX)' -f- $(DEFINES) $(ACDEFINES) $(XULPPFLAGS) $(INCLUDES) $< 2>/dev/null | sed -e "s|^[^ ]*/||" > $(_MDDEPFILE) ; \
fi
endef

MAKE_DEPS_AUTO_CC = $(MAKE_DEPS_AUTO)
MAKE_DEPS_AUTO_CXX = $(MAKE_DEPS_AUTO)

endif # COMPILER_DEPEND

endif # MOZ_AUTO_DEPS

$(OBJS) $(HOST_OBJS): $(GLOBAL_DEPS)

# Rules for building native targets must come first because of the host_ prefix
$(HOST_COBJS): host_%.$(OBJ_SUFFIX): %.c
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CC) $(HOST_OUTOPTION)$@ -c $(HOST_CFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CPPOBJS): host_%.$(OBJ_SUFFIX): %.cpp
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CXX) $(HOST_OUTOPTION)$@ -c $(HOST_CXXFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CCOBJS): host_%.$(OBJ_SUFFIX): %.cc
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CXX) $(HOST_OUTOPTION)$@ -c $(HOST_CXXFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CMOBJS): host_%.$(OBJ_SUFFIX): %.m
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CC) $(HOST_OUTOPTION)$@ -c $(HOST_CFLAGS) $(HOST_CMFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(HOST_CMMOBJS): host_%.$(OBJ_SUFFIX): %.mm
	$(REPORT_BUILD)
	$(ELOG) $(HOST_CXX) $(HOST_OUTOPTION)$@ -c $(HOST_CXXFLAGS) $(HOST_CMMFLAGS) $(INCLUDES) $(NSPR_CFLAGS) $(_VPATH_SRCS)

$(COBJS): %.$(OBJ_SUFFIX): %.c
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CC)
	$(ELOG) $(CC) $(OUTOPTION)$@ -c $(COMPILE_CFLAGS) $(_VPATH_SRCS)

# DEFINES and ACDEFINES are needed here to enable conditional compilation of Q_OBJECTs:
# 'moc' only knows about #defines it gets on the command line (-D...), not in
# included headers like mozilla-config.h
moc_%.cpp: %.h
	$(ELOG) $(MOC) $(DEFINES) $(ACDEFINES) $< $(OUTOPTION)$@

moc_%.cc: %.cc
	$(ELOG) $(MOC) $(DEFINES) $(ACDEFINES) $(_VPATH_SRCS:.cc=.h) $(OUTOPTION)$@

qrc_%.cpp: %.qrc
	$(ELOG) $(RCC) -name $* $< $(OUTOPTION)$@

ifdef ASFILES
# The AS_DASH_C_FLAG is needed cause not all assemblers (Solaris) accept
# a '-c' flag.
$(ASOBJS): %.$(OBJ_SUFFIX): %.$(ASM_SUFFIX)
	$(AS) $(ASOUTOPTION)$@ $(ASFLAGS) $(AS_DASH_C_FLAG) $(_VPATH_SRCS)
endif

$(SOBJS): %.$(OBJ_SUFFIX): %.S
	$(AS) -o $@ $(ASFLAGS) -c $<

#
# Please keep the next two rules in sync.
#
$(CCOBJS): %.$(OBJ_SUFFIX): %.cc
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CXX)
	$(ELOG) $(CCC) $(OUTOPTION)$@ -c $(COMPILE_CXXFLAGS) $(_VPATH_SRCS)

$(CPPOBJS): %.$(OBJ_SUFFIX): %.cpp
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CXX)
ifdef STRICT_CPLUSPLUS_SUFFIX
	echo "#line 1 \"$*.cpp\"" | cat - $*.cpp > t_$*.cc
	$(ELOG) $(CCC) -o $@ -c $(COMPILE_CXXFLAGS) t_$*.cc
	$(RM) t_$*.cc
else
	$(ELOG) $(CCC) $(OUTOPTION)$@ -c $(COMPILE_CXXFLAGS) $(_VPATH_SRCS)
endif #STRICT_CPLUSPLUS_SUFFIX

$(CMMOBJS): $(OBJ_PREFIX)%.$(OBJ_SUFFIX): %.mm
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CXX)
	$(ELOG) $(CCC) -o $@ -c $(COMPILE_CXXFLAGS) $(COMPILE_CMMFLAGS) $(_VPATH_SRCS)

$(CMOBJS): $(OBJ_PREFIX)%.$(OBJ_SUFFIX): %.m
	$(REPORT_BUILD)
	@$(MAKE_DEPS_AUTO_CC)
	$(ELOG) $(CC) -o $@ -c $(COMPILE_CFLAGS) $(COMPILE_CMFLAGS) $(_VPATH_SRCS)

%.s: %.cpp
	$(CCC) -S $(COMPILE_CXXFLAGS) $(_VPATH_SRCS)

%.s: %.cc
	$(CCC) -S $(COMPILE_CXXFLAGS) $(_VPATH_SRCS)

%.s: %.c
	$(CC) -S $(COMPILE_CFLAGS) $(_VPATH_SRCS)

%.i: %.cpp
	$(CCC) -C -E $(COMPILE_CXXFLAGS) $(_VPATH_SRCS) > $*.i

%.i: %.cc
	$(CCC) -C -E $(COMPILE_CXXFLAGS) $(_VPATH_SRCS) > $*.i

%.i: %.c
	$(CC) -C -E $(COMPILE_CFLAGS) $(_VPATH_SRCS) > $*.i

%.i: %.mm
	$(CCC) -C -E $(COMPILE_CXXFLAGS) $(COMPILE_CMMFLAGS) $(_VPATH_SRCS) > $*.i

%.res: %.rc
	@echo Creating Resource file: $@
ifeq ($(OS_ARCH),OS2)
	$(RC) $(RCFLAGS:-D%=-d %) -i $(subst /,\,$(srcdir)) -r $< $@
else
ifdef GNU_CC
	$(RC) $(RCFLAGS) $(filter-out -U%,$(DEFINES)) $(INCLUDES:-I%=--include-dir %) $(OUTOPTION)$@ $(_VPATH_SRCS)
else
	$(RC) $(RCFLAGS) -r $(DEFINES) $(INCLUDES) $(OUTOPTION)$@ $(_VPATH_SRCS)
endif
endif

# need 3 separate lines for OS/2
%:: %.pl
	$(RM) $@
	cp $< $@
	chmod +x $@

%:: %.sh
	$(RM) $@
	cp $< $@
	chmod +x $@

# Cancel these implicit rules
#
%: %,v

%: RCS/%,v

%: s.%

%: SCCS/s.%

###############################################################################
# Java rules
###############################################################################
ifneq (,$(filter OS2 WINNT,$(OS_ARCH)))
SEP := ;
else
SEP := :
endif

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)

# MSYS has its own special path form, but javac expects the source and class
# paths to be in the DOS form (i.e. e:/builds/...).  This function does the
# appropriate conversion on Windows, but is a noop on other systems.
ifeq ($(HOST_OS_ARCH),WINNT)
#  We use 'pwd -W' to get DOS form of the path.  However, since the given path
#  could be a file or a non-existent path, we cannot call 'pwd -W' directly
#  on the path.  Instead, we extract the root path (i.e. "c:/"), call 'pwd -W'
#  on it, then merge with the rest of the path.
root-path = $(shell echo $(1) | sed -e "s|\(/[^/]*\)/\?\(.*\)|\1|")
non-root-path = $(shell echo $(1) | sed -e "s|\(/[^/]*\)/\?\(.*\)|\2|")
normalizepath = $(foreach p,$(1),$(if $(filter /%,$(1)),$(patsubst %/,%,$(shell cd $(call root-path,$(1)) && pwd -W))/$(call non-root-path,$(1)),$(1)))
else
normalizepath = $(1)
endif

_srcdir = $(call normalizepath,$(srcdir))
ifdef JAVA_SOURCEPATH
SP = $(subst $(SPACE),$(SEP),$(call normalizepath,$(strip $(JAVA_SOURCEPATH))))
_JAVA_SOURCEPATH = ".$(SEP)$(_srcdir)$(SEP)$(SP)"
else
_JAVA_SOURCEPATH = ".$(SEP)$(_srcdir)"
endif

ifdef JAVA_CLASSPATH
CP = $(subst $(SPACE),$(SEP),$(call normalizepath,$(strip $(JAVA_CLASSPATH))))
_JAVA_CLASSPATH = ".$(SEP)$(CP)"
else
_JAVA_CLASSPATH = .
endif

_JAVA_DIR = _java
$(_JAVA_DIR)::
	$(NSINSTALL) -D $@

$(_JAVA_DIR)/%.class: %.java $(GLOBAL_DEPS) $(_JAVA_DIR)
	$(JAVAC) $(JAVAC_FLAGS) -classpath $(_JAVA_CLASSPATH) \
			-sourcepath $(_JAVA_SOURCEPATH) -d $(_JAVA_DIR) $(_VPATH_SRCS)

$(JAVA_LIBRARY): $(addprefix $(_JAVA_DIR)/,$(JAVA_SRCS:.java=.class)) $(GLOBAL_DEPS)
	$(JAR) cf $@ -C $(_JAVA_DIR) .

GARBAGE_DIRS += $(_JAVA_DIR)

###############################################################################
# Update Makefiles
###############################################################################

# Note: Passing depth to make-makefile is optional.
#       It saves the script some work, though.
Makefile: Makefile.in
	@$(PERL) $(AUTOCONF_TOOLS)/make-makefile -t $(topsrcdir) -d $(DEPTH)

ifdef SUBMAKEFILES
# VPATH does not work on some machines in this case, so add $(srcdir)
$(SUBMAKEFILES): % : $(srcdir)/%.in
	$(PERL) $(AUTOCONF_TOOLS)/make-makefile -t $(topsrcdir) -d $(DEPTH) $@
endif

ifdef AUTOUPDATE_CONFIGURE
$(topsrcdir)/configure: $(topsrcdir)/configure.in
	(cd $(topsrcdir) && $(AUTOCONF)) && (cd $(DEPTH) && ./config.status --recheck)
endif

$(DEPTH)/config/autoconf.mk: $(topsrcdir)/config/autoconf.mk.in
	cd $(DEPTH) && CONFIG_HEADERS= CONFIG_FILES=config/autoconf.mk ./config.status

###############################################################################
# Bunch of things that extend the 'export' rule (in order):
###############################################################################

################################################################################
# Copy each element of EXPORTS to $(DIST)/include

ifneq ($(XPI_NAME),)
$(FINAL_TARGET):
	$(NSINSTALL) -D $@

export:: $(FINAL_TARGET)
endif

ifndef NO_DIST_INSTALL
ifneq (,$(EXPORTS))
export:: $(EXPORTS)
	$(INSTALL) $(IFLAGS1) $^ $(DIST)/include
endif
endif # NO_DIST_INSTALL

define EXPORT_NAMESPACE_RULE
ifndef NO_DIST_INSTALL
export:: $(EXPORTS_$(namespace))
	$(INSTALL) $(IFLAGS1) $$^ $(DIST)/include/$(namespace)
endif # NO_DIST_INSTALL
endef

$(foreach namespace,$(EXPORTS_NAMESPACES),$(eval $(EXPORT_NAMESPACE_RULE)))

################################################################################
# Copy each element of PREF_JS_EXPORTS

ifdef GRE_MODULE
PREF_DIR = greprefs
else
ifneq (,$(XPI_NAME)$(LIBXUL_SDK)$(MOZ_PHOENIX))
PREF_DIR = defaults/preferences
else
PREF_DIR = defaults/pref
endif
endif

ifneq ($(PREF_JS_EXPORTS),)
# on win32, pref files need CRLF line endings... see bug 206029
ifeq (WINNT,$(OS_ARCH))
PREF_PPFLAGS = --line-endings=crlf
endif

ifndef NO_DIST_INSTALL
$(FINAL_TARGET)/$(PREF_DIR):
	$(NSINSTALL) -D $@

libs:: $(FINAL_TARGET)/$(PREF_DIR)
libs:: $(PREF_JS_EXPORTS)
	$(EXIT_ON_ERROR)  \
	for i in $^; do \
	  dest=$(FINAL_TARGET)/$(PREF_DIR)/`basename $$i`; \
	  $(RM) -f $$dest; \
	  $(PYTHON) $(topsrcdir)/config/Preprocessor.py $(PREF_PPFLAGS) $(DEFINES) $(ACDEFINES) $(XULPPFLAGS) $$i > $$dest; \
	done
endif
endif

################################################################################
# Copy each element of AUTOCFG_JS_EXPORTS to $(FINAL_TARGET)/defaults/autoconfig

ifneq ($(AUTOCFG_JS_EXPORTS),)
$(FINAL_TARGET)/defaults/autoconfig::
	$(NSINSTALL) -D $@

ifndef NO_DIST_INSTALL
export:: $(AUTOCFG_JS_EXPORTS) $(FINAL_TARGET)/defaults/autoconfig
	$(INSTALL) $(IFLAGS1) $^
endif

endif

################################################################################
# Export the elements of $(XPIDLSRCS)
# generating .h and .xpt files and moving them to the appropriate places.

ifneq ($(XPIDLSRCS),)

export:: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.h, $(XPIDLSRCS))

ifndef XPIDL_MODULE
XPIDL_MODULE		= $(MODULE)
endif

ifeq ($(XPIDL_MODULE),) # we need $(XPIDL_MODULE) to make $(XPIDL_MODULE).xpt
export:: FORCE
	@echo
	@echo "*** Error processing XPIDLSRCS:"
	@echo "Please define MODULE or XPIDL_MODULE when defining XPIDLSRCS,"
	@echo "so we have a module name to use when creating MODULE.xpt."
	@echo; sleep 2; false
endif

# generate .h files from into $(XPIDL_GEN_DIR), then export to $(DIST)/include;
# warn against overriding existing .h file.

XPIDL_DEPS = \
  $(LIBXUL_DIST)/sdk/bin/header.py \
  $(LIBXUL_DIST)/sdk/bin/typelib.py \
  $(LIBXUL_DIST)/sdk/bin/xpidl.py \
  $(NULL)

xpidl-preqs = \
  $(call mkdir_deps,$(XPIDL_GEN_DIR)) \
  $(call mkdir_deps,$(MDDEPDIR)) \
  $(NULL)

$(XPIDL_GEN_DIR)/%.h: %.idl $(XPIDL_DEPS) $(xpidl-preqs)
	$(REPORT_BUILD)
	$(PYTHON_PATH) \
	  $(PLY_INCLUDE) \
	  $(LIBXUL_DIST)/sdk/bin/header.py $(XPIDL_FLAGS) $(_VPATH_SRCS) -d $(MDDEPDIR)/$(@F).pp -o $@
	@if test -n "$(findstring $*.h, $(EXPORTS))"; \
	  then echo "*** WARNING: file $*.h generated from $*.idl overrides $(srcdir)/$*.h"; else true; fi

ifndef NO_GEN_XPT
# generate intermediate .xpt files into $(XPIDL_GEN_DIR), then link
# into $(XPIDL_MODULE).xpt and export it to $(FINAL_TARGET)/components.
$(XPIDL_GEN_DIR)/%.xpt: %.idl $(XPIDL_DEPS) $(xpidl-preqs)
	$(REPORT_BUILD)
	$(PYTHON_PATH) \
	  $(PLY_INCLUDE) \
	  -I$(topsrcdir)/xpcom/typelib/xpt/tools \
	  $(LIBXUL_DIST)/sdk/bin/typelib.py $(XPIDL_FLAGS) $(_VPATH_SRCS) -d $(MDDEPDIR)/$(@F).pp -o $@

# no need to link together if XPIDLSRCS contains only XPIDL_MODULE
ifneq ($(XPIDL_MODULE).idl,$(strip $(XPIDLSRCS)))
$(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.xpt,$(XPIDLSRCS)) $(GLOBAL_DEPS)
	$(XPIDL_LINK) $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.xpt,$(XPIDLSRCS))
endif # XPIDL_MODULE.xpt != XPIDLSRCS

libs:: $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $(XPIDL_GEN_DIR)/$(XPIDL_MODULE).xpt $(FINAL_TARGET)/components
ifndef NO_INTERFACES_MANIFEST
	@$(PYTHON) $(MOZILLA_DIR)/config/buildlist.py $(FINAL_TARGET)/components/interfaces.manifest "interfaces $(XPIDL_MODULE).xpt"
	@$(PYTHON) $(MOZILLA_DIR)/config/buildlist.py $(FINAL_TARGET)/chrome.manifest "manifest components/interfaces.manifest"
endif
endif

endif # NO_GEN_XPT

GARBAGE_DIRS		+= $(XPIDL_GEN_DIR)

endif # XPIDLSRCS

ifneq ($(XPIDLSRCS),)
# export .idl files to $(IDL_DIR)
ifndef NO_DIST_INSTALL
export:: $(XPIDLSRCS) $(IDL_DIR)
	$(INSTALL) $(IFLAGS1) $^

export:: $(patsubst %.idl,$(XPIDL_GEN_DIR)/%.h, $(XPIDLSRCS)) $(DIST)/include
	$(INSTALL) $(IFLAGS1) $^
endif # NO_DIST_INSTALL

endif # XPIDLSRCS



# General rules for exporting idl files.
$(IDL_DIR):
	$(NSINSTALL) -D $@

export-idl:: $(SUBMAKEFILES) $(MAKE_DIRS)

ifneq ($(XPIDLSRCS),)
ifndef NO_DIST_INSTALL
export-idl:: $(XPIDLSRCS) $(IDL_DIR)
	$(INSTALL) $(IFLAGS1) $^
endif
endif
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)

################################################################################
# Copy each element of EXTRA_COMPONENTS to $(FINAL_TARGET)/components
ifneq (,$(filter %.js,$(EXTRA_COMPONENTS) $(EXTRA_PP_COMPONENTS)))
ifeq (,$(filter %.manifest,$(EXTRA_COMPONENTS) $(EXTRA_PP_COMPONENTS)))
ifndef NO_JS_MANIFEST
$(error .js component without matching .manifest. See https://developer.mozilla.org/en/XPCOM/XPCOM_changes_in_Gecko_2.0)
endif
endif
endif

ifdef EXTRA_COMPONENTS
libs:: $(EXTRA_COMPONENTS)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^ $(FINAL_TARGET)/components
endif

endif

ifdef EXTRA_PP_COMPONENTS
libs:: $(EXTRA_PP_COMPONENTS)
ifndef NO_DIST_INSTALL
	$(EXIT_ON_ERROR) \
	$(NSINSTALL) -D $(FINAL_TARGET)/components; \
	for i in $^; do \
	  fname=`basename $$i`; \
	  dest=$(FINAL_TARGET)/components/$${fname}; \
	  $(RM) -f $$dest; \
	  $(PYTHON) $(topsrcdir)/config/Preprocessor.py $(DEFINES) $(ACDEFINES) $(XULPPFLAGS) $$i > $$dest; \
	done
endif
endif

EXTRA_MANIFESTS = $(filter %.manifest,$(EXTRA_COMPONENTS) $(EXTRA_PP_COMPONENTS))
ifneq (,$(EXTRA_MANIFESTS))
libs::
	$(PYTHON) $(MOZILLA_DIR)/config/buildlist.py $(FINAL_TARGET)/chrome.manifest $(patsubst %,"manifest components/%",$(notdir $(EXTRA_MANIFESTS)))
endif

################################################################################
# Copy each element of EXTRA_JS_MODULES to $(FINAL_TARGET)/modules
ifdef EXTRA_JS_MODULES
libs:: $(EXTRA_JS_MODULES)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS1) $^ $(FINAL_TARGET)/modules
endif

endif

ifdef EXTRA_PP_JS_MODULES
libs:: $(EXTRA_PP_JS_MODULES)
ifndef NO_DIST_INSTALL
	$(EXIT_ON_ERROR) \
	$(NSINSTALL) -D $(FINAL_TARGET)/modules; \
	for i in $^; do \
	  dest=$(FINAL_TARGET)/modules/`basename $$i`; \
	  $(RM) -f $$dest; \
	  $(PYTHON) $(topsrcdir)/config/Preprocessor.py $(DEFINES) $(ACDEFINES) $(XULPPFLAGS) $$i > $$dest; \
	done
endif

endif

################################################################################
# Copy testing-only JS modules to appropriate destination.
#
# For each file defined in TESTING_JS_MODULES, copy it to
# objdir/_tests/modules/. If TESTING_JS_MODULE_DIR is defined, that path
# wlll be appended to the output directory.

ifdef TESTING_JS_MODULES
testmodulesdir = $(DEPTH)/_tests/modules/$(TESTING_JS_MODULE_DIR)

GENERATED_DIRS += $(testmodulesdir)

libs:: $(TESTING_JS_MODULES)
ifndef NO_DIST_INSTALL
	$(INSTALL) $(IFLAGS) $^ $(testmodulesdir)
endif

endif

################################################################################
# SDK

ifneq (,$(SDK_LIBRARY))
$(SDK_LIB_DIR)::
	$(NSINSTALL) -D $@

ifndef NO_DIST_INSTALL
libs:: $(SDK_LIBRARY) $(SDK_LIB_DIR)
	$(INSTALL) $(IFLAGS2) $^
endif

endif # SDK_LIBRARY

ifneq (,$(strip $(SDK_BINARY)))
$(SDK_BIN_DIR)::
	$(NSINSTALL) -D $@

ifndef NO_DIST_INSTALL
libs:: $(SDK_BINARY) $(SDK_BIN_DIR)
	$(INSTALL) $(IFLAGS2) $^
endif

endif # SDK_BINARY

################################################################################
# CHROME PACKAGING

JAR_MANIFEST := $(srcdir)/jar.mn

chrome::
	$(MAKE) realchrome
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)

$(FINAL_TARGET)/chrome: $(call mkdir_deps,$(FINAL_TARGET)/chrome)

ifneq (,$(wildcard $(JAR_MANIFEST)))
ifndef NO_DIST_INSTALL
libs realchrome:: $(CHROME_DEPS) $(FINAL_TARGET)/chrome
	$(PYTHON) $(MOZILLA_DIR)/config/JarMaker.py \
	  $(QUIET) -j $(FINAL_TARGET)/chrome \
	  $(MAKE_JARS_FLAGS) $(XULPPFLAGS) $(DEFINES) $(ACDEFINES) \
	  $(JAR_MANIFEST)
endif
endif

ifneq ($(DIST_FILES),)
$(DIST)/bin:
	$(NSINSTALL) -D $@

libs:: $(DIST)/bin
libs:: $(DIST_FILES)
	@$(EXIT_ON_ERROR) \
	for f in $^; do \
	  dest=$(FINAL_TARGET)/`basename $$f`; \
	  $(RM) -f $$dest; \
	  $(PYTHON) $(MOZILLA_DIR)/config/Preprocessor.py \
	    $(XULAPP_DEFINES) $(DEFINES) $(ACDEFINES) $(XULPPFLAGS) \
	    $$f > $$dest; \
	done
endif

ifneq ($(DIST_CHROME_FILES),)
libs:: $(DIST_CHROME_FILES)
	@$(EXIT_ON_ERROR) \
	for f in $^; do \
	  dest=$(FINAL_TARGET)/chrome/`basename $$f`; \
	  $(RM) -f $$dest; \
	  $(PYTHON) $(MOZILLA_DIR)/config/Preprocessor.py \
	    $(XULAPP_DEFINES) $(DEFINES) $(ACDEFINES) $(XULPPFLAGS) \
	    $$f > $$dest; \
	done
endif

ifneq ($(XPI_PKGNAME),)
libs realchrome::
ifdef STRIP_XPI
ifndef MOZ_DEBUG
	@echo "Stripping $(XPI_PKGNAME) package directory..."
	@echo $(FINAL_TARGET)
	@cd $(FINAL_TARGET) && find . ! -type d \
			! -name "*.js" \
			! -name "*.xpt" \
			! -name "*.gif" \
			! -name "*.jpg" \
			! -name "*.png" \
			! -name "*.xpm" \
			! -name "*.txt" \
			! -name "*.rdf" \
			! -name "*.sh" \
			! -name "*.properties" \
			! -name "*.dtd" \
			! -name "*.html" \
			! -name "*.xul" \
			! -name "*.css" \
			! -name "*.xml" \
			! -name "*.jar" \
			! -name "*.dat" \
			! -name "*.tbl" \
			! -name "*.src" \
			! -name "*.reg" \
			$(PLATFORM_EXCLUDE_LIST) \
			-exec $(STRIP) $(STRIP_FLAGS) {} >/dev/null 2>&1 \;
endif
endif
	@echo "Packaging $(XPI_PKGNAME).xpi..."
	cd $(FINAL_TARGET) && $(ZIP) -qr ../$(XPI_PKGNAME).xpi *
endif

ifdef INSTALL_EXTENSION_ID
ifndef XPI_NAME
$(error XPI_NAME must be set for INSTALL_EXTENSION_ID)
endif

libs::
	$(RM) -r "$(DIST)/bin/extensions/$(INSTALL_EXTENSION_ID)"
	$(NSINSTALL) -D "$(DIST)/bin/extensions/$(INSTALL_EXTENSION_ID)"
	cd $(FINAL_TARGET) && tar $(TAR_CREATE_FLAGS) - . | (cd "../../bin/extensions/$(INSTALL_EXTENSION_ID)" && tar -xf -)

endif

ifneq (,$(filter flat symlink,$(MOZ_CHROME_FILE_FORMAT)))
_JAR_REGCHROME_DISABLE_JAR=1
else
_JAR_REGCHROME_DISABLE_JAR=0
endif

REGCHROME = $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/add-chrome.pl \
	$(if $(filter gtk2,$(MOZ_WIDGET_TOOLKIT)),-x) \
	$(if $(CROSS_COMPILE),-o $(OS_ARCH)) $(FINAL_TARGET)/chrome/installed-chrome.txt \
	$(_JAR_REGCHROME_DISABLE_JAR)

REGCHROME_INSTALL = $(PERL) -I$(MOZILLA_DIR)/config $(MOZILLA_DIR)/config/add-chrome.pl \
	$(if $(filter gtk2,$(MOZ_WIDGET_TOOLKIT)),-x) \
	$(if $(CROSS_COMPILE),-o $(OS_ARCH)) $(DESTDIR)$(mozappdir)/chrome/installed-chrome.txt \
	$(_JAR_REGCHROME_DISABLE_JAR)


#############################################################################
# Dependency system
#############################################################################
ifdef COMPILER_DEPEND
depend::
	@echo "$(MAKE): No need to run depend target.\
			Using compiler-based depend." 1>&2
ifeq ($(GNU_CC)$(GNU_CXX),)
# Non-GNU compilers
	@echo "`echo '$(MAKE):'|sed 's/./ /g'`"\
	'(Compiler-based depend was turned on by "--enable-md".)' 1>&2
else
# GNU compilers
	@space="`echo '$(MAKE): '|sed 's/./ /g'`";\
	echo "$$space"'Since you are using a GNU compiler,\
		it is on by default.' 1>&2; \
	echo "$$space"'To turn it off, pass --disable-md to configure.' 1>&2
endif

else # ! COMPILER_DEPEND

ifndef MOZ_AUTO_DEPS

define MAKE_DEPS_NOAUTO
	$(MKDEPEND) -w1024 -o'.$(OBJ_SUFFIX)' -f- $(DEFINES) $(ACDEFINES) $(INCLUDES) $< 2>/dev/null | sed -e "s|^[^ ]*/||" > $@
endef

$(MDDEPDIR)/%.pp: %.c
	$(REPORT_BUILD)
	@$(MAKE_DEPS_NOAUTO)

$(MDDEPDIR)/%.pp: %.cpp
	$(REPORT_BUILD)
	@$(MAKE_DEPS_NOAUTO)

$(MDDEPDIR)/%.pp: %.s
	$(REPORT_BUILD)
	@$(MAKE_DEPS_NOAUTO)

ifneq (,$(OBJS)$(XPIDLSRCS)$(SIMPLE_PROGRAMS))
depend:: $(SUBMAKEFILES) $(MAKE_DIRS) $(MDDEPFILES)
else
depend:: $(SUBMAKEFILES)
endif
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)

dependclean:: $(SUBMAKEFILES)
	$(RM) $(MDDEPFILES)
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)

endif # MOZ_AUTO_DEPS

endif # COMPILER_DEPEND


#############################################################################
# MDDEPDIR is the subdirectory where all the dependency files are placed.
#   This uses a make rule (instead of a macro) to support parallel
#   builds (-jN). If this were done in the LOOP_OVER_DIRS macro, two
#   processes could simultaneously try to create the same directory.
#
#   We use $(CURDIR) in the rule's target to ensure that we don't find
#   a dependency directory in the source tree via VPATH (perhaps from
#   a previous build in the source tree) and thus neglect to create a
#   dependency directory in the object directory, where we really need
#   it.

$(CURDIR)/$(MDDEPDIR):
	$(MKDIR) -p $@

ifneq (,$(filter-out all chrome default export realchrome tools clean clobber clobber_all distclean realclean,$(MAKECMDGOALS)))
ifneq (,$(OBJS)$(XPIDLSRCS)$(SIMPLE_PROGRAMS))
MDDEPEND_FILES		:= $(strip $(wildcard $(MDDEPDIR)/*.pp))

ifneq (,$(MDDEPEND_FILES))
# The script mddepend.pl checks the dependencies and writes to stdout
# one rule to force out-of-date objects. For example,
#   foo.o boo.o: FORCE
# The script has an advantage over including the *.pp files directly
# because it handles the case when header files are removed from the build.
# 'make' would complain that there is no way to build missing headers.
ALL_PP_RESULTS = $(shell $(PERL) $(BUILD_TOOLS)/mddepend.pl - $(MDDEPEND_FILES))
$(eval $(ALL_PP_RESULTS))
endif

endif
endif
#############################################################################

-include $(topsrcdir)/$(MOZ_BUILD_APP)/app-rules.mk
-include $(MY_RULES)

#
# Generate Emacs tags in a file named TAGS if ETAGS was set in $(MY_CONFIG)
# or in $(MY_RULES)
#
ifdef ETAGS
ifneq ($(CSRCS)$(CPPSRCS)$(HEADERS),)
all:: TAGS
TAGS:: $(CSRCS) $(CPPSRCS) $(HEADERS)
	$(ETAGS) $(CSRCS) $(CPPSRCS) $(HEADERS)
endif
endif

################################################################################
# Special gmake rules.
################################################################################


#
# Disallow parallel builds with MSVC < 8
#
ifneq (,$(filter 1200 1300 1310,$(_MSC_VER)))
.NOTPARALLEL:
endif

#
# Re-define the list of default suffixes, so gmake won't have to churn through
# hundreds of built-in suffix rules for stuff we don't need.
#
.SUFFIXES:

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all alltags boot checkout chrome realchrome clean clobber clobber_all export install libs makefiles realclean run_apprunner tools $(DIRS) $(TOOL_DIRS) FORCE

# Used as a dependency to force targets to rebuild
FORCE:

# Delete target if error occurs when building target
.DELETE_ON_ERROR:

# Properly set LIBPATTERNS for the platform
.LIBPATTERNS = $(if $(IMPORT_LIB_SUFFIX),$(LIB_PREFIX)%.$(IMPORT_LIB_SUFFIX)) $(LIB_PREFIX)%.$(LIB_SUFFIX) $(DLL_PREFIX)%$(DLL_SUFFIX)

tags: TAGS

TAGS: $(SUBMAKEFILES) $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	-etags $(CSRCS) $(CPPSRCS) $(wildcard *.h)
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)

ifndef INCLUDED_DEBUGMAKE_MK #{
  ## Only parse when an echo* or show* target is requested
  ifneq (,$(call isTargetStem,echo,show))
    include $(topsrcdir)/config/makefiles/debugmake.mk
  endif #}
endif #}

documentation:
	@cd $(DEPTH)
	$(DOXYGEN) $(DEPTH)/config/doxygen.cfg

ifdef ENABLE_TESTS
check:: $(SUBMAKEFILES) $(MAKE_DIRS)
	$(LOOP_OVER_PARALLEL_DIRS)
	$(LOOP_OVER_DIRS)
	$(LOOP_OVER_TOOL_DIRS)
endif


FREEZE_VARIABLES = \
  CSRCS \
  CPPSRCS \
  EXPORTS \
  XPIDLSRCS \
  DIRS \
  LIBRARY \
  MODULE \
  SHORT_LIBNAME \
  TIERS \
  EXTRA_COMPONENTS \
  EXTRA_PP_COMPONENTS \
  $(NULL)

$(foreach var,$(FREEZE_VARIABLES),$(eval $(var)_FROZEN := '$($(var))'))

CHECK_FROZEN_VARIABLES = $(foreach var,$(FREEZE_VARIABLES), \
  $(if $(subst $($(var)_FROZEN),,'$($(var))'),$(error Makefile variable '$(var)' changed value after including rules.mk. Was $($(var)_FROZEN), now $($(var)).)))

libs export::
	$(CHECK_FROZEN_VARIABLES)

default all::
	if test -d $(DIST)/bin ; then touch $(DIST)/bin/.purgecaches ; fi


#############################################################################
# Derived targets and dependencies

include $(topsrcdir)/config/makefiles/autotargets.mk
ifneq ($(NULL),$(AUTO_DEPS))
  default all libs tools export:: $(AUTO_DEPS)
endif

