#! gmake
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
# The Original Code is the Netscape Portable Runtime (NSPR).
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
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

################################################################################
# We used to have a 4 pass build process.  Now we do everything in one pass.
#
# export - Create generated headers and stubs. Publish public headers to
#          dist/<arch>/include.
#          Create libraries. Publish libraries to dist/<arch>/lib.
#          Create programs. 
#
# libs - obsolete.  Now a synonym of "export".
#
# all - the default makefile target.  Now a synonym of "export".
#
# install - Install headers, libraries, and programs on the system.
#
# Parameters to this makefile (set these before including):
#
# a)
#	TARGETS	-- the target to create 
#			(defaults to $LIBRARY $PROGRAM)
# b)
#	DIRS	-- subdirectories for make to recurse on
#			(the 'all' rule builds $TARGETS $DIRS)
# c)
#	CSRCS   -- .c files to compile
#			(used to define $OBJS)
# d)
#	PROGRAM	-- the target program name to create from $OBJS
#			($OBJDIR automatically prepended to it)
# e)
#	LIBRARY	-- the target library name to create from $OBJS
#			($OBJDIR automatically prepended to it)
#
################################################################################

ifndef topsrcdir
topsrcdir=$(MOD_DEPTH)
endif

ifndef srcdir
srcdir=.
endif

ifndef NSPR_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

ifdef USE_AUTOCONF
ifdef CROSS_COMPILE
ifdef INTERNAL_TOOLS
CC=$(HOST_CC)
CCC=$(HOST_CXX)
CFLAGS=$(HOST_CFLAGS)
CXXFLAGS=$(HOST_CXXFLAGS)
LDFLAGS=$(HOST_LDFLAGS)
endif
endif
endif

#
# This makefile contains rules for building the following kinds of
# libraries:
# - LIBRARY: a static (archival) library
# - SHARED_LIBRARY: a shared (dynamic link) library
# - IMPORT_LIBRARY: an import library, used only on Windows and OS/2
#
# The names of these libraries can be generated by simply specifying
# LIBRARY_NAME and LIBRARY_VERSION.
#

ifdef LIBRARY_NAME
ifeq (,$(filter-out WINNT WINCE OS2,$(OS_ARCH)))

#
# Win95 and OS/2 require library names conforming to the 8.3 rule.
# other platforms do not.
#
ifeq (,$(filter-out WIN95 WINCE OS2,$(OS_TARGET)))
LIBRARY		= $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)_s.$(LIB_SUFFIX)
SHARED_LIBRARY	= $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
IMPORT_LIBRARY	= $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).$(LIB_SUFFIX)
SHARED_LIB_PDB	= $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).pdb
else
LIBRARY		= $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION)_s.$(LIB_SUFFIX)
SHARED_LIBRARY	= $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
IMPORT_LIBRARY	= $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(LIB_SUFFIX)
SHARED_LIB_PDB	= $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION).pdb
endif

else

LIBRARY		= $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(LIB_SUFFIX)
ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
SHARED_LIBRARY	= $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION)_shr.a
else
ifdef MKSHLIB
SHARED_LIBRARY	= $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)
endif
endif

endif
endif

ifndef TARGETS
ifeq (,$(filter-out WINNT WINCE OS2,$(OS_ARCH)))
TARGETS		= $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY)
ifndef BUILD_OPT
ifdef MSC_VER
ifneq (,$(filter-out 1100 1200,$(MSC_VER)))
TARGETS		+= $(SHARED_LIB_PDB)
endif
endif
endif
else
TARGETS		= $(LIBRARY) $(SHARED_LIBRARY)
endif
endif

#
# OBJS is the list of object files.  It can be constructed by
# specifying CSRCS (list of C source files) and ASFILES (list
# of assembly language source files).
#

ifndef OBJS
OBJS		= $(addprefix $(OBJDIR)/,$(CSRCS:.c=.$(OBJ_SUFFIX))) \
		  $(addprefix $(OBJDIR)/,$(ASFILES:.$(ASM_SUFFIX)=.$(OBJ_SUFFIX)))
endif

ALL_TRASH		= $(TARGETS) $(OBJS) $(RES) $(filter-out . .., $(OBJDIR)) LOGS TAGS $(GARBAGE) \
			  $(NOSUCHFILE) \
			  so_locations

ifndef RELEASE_LIBS_DEST
RELEASE_LIBS_DEST	= $(RELEASE_LIB_DIR)
endif

ifdef DIRS
LOOP_OVER_DIRS		=					\
	@for d in $(DIRS); do					\
		if test -d $$d; then				\
			set -e;					\
			echo "cd $$d; $(MAKE) $@";		\
			$(MAKE) -C $$d $@;			\
			set +e;					\
		else						\
			echo "Skipping non-directory $$d...";	\
		fi;						\
	done
endif

################################################################################

all:: export

export::
	+$(LOOP_OVER_DIRS)

libs:: export

clean::
	rm -rf $(OBJS) $(RES) so_locations $(NOSUCHFILE) $(GARBAGE)
	+$(LOOP_OVER_DIRS)

clobber::
	rm -rf $(OBJS) $(RES) $(TARGETS) $(filter-out . ..,$(OBJDIR)) $(GARBAGE) so_locations $(NOSUCHFILE)
	+$(LOOP_OVER_DIRS)

realclean clobber_all::
	rm -rf $(wildcard *.OBJ *.OBJD) dist $(ALL_TRASH)
	+$(LOOP_OVER_DIRS)

distclean::
	rm -rf $(wildcard *.OBJ *.OBJD) dist $(ALL_TRASH) $(DIST_GARBAGE)
	+$(LOOP_OVER_DIRS)

install:: $(RELEASE_BINS) $(RELEASE_HEADERS) $(RELEASE_LIBS)
ifdef RELEASE_BINS
	$(NSINSTALL) -t -m 0755 $(RELEASE_BINS) $(DESTDIR)$(bindir)
endif
ifdef RELEASE_HEADERS
	$(NSINSTALL) -t -m 0644 $(RELEASE_HEADERS) $(DESTDIR)$(includedir)/$(include_subdir)
endif
ifdef RELEASE_LIBS
	$(NSINSTALL) -t -m 0755 $(RELEASE_LIBS) $(DESTDIR)$(libdir)/$(lib_subdir)
endif
	+$(LOOP_OVER_DIRS)

release:: export
ifdef RELEASE_BINS
	@echo "Copying executable programs and scripts to release directory"
	@if test -z "$(BUILD_NUMBER)"; then \
		echo "BUILD_NUMBER must be defined"; \
		false; \
	else \
		true; \
	fi
	@if test ! -d $(RELEASE_BIN_DIR); then \
		rm -rf $(RELEASE_BIN_DIR); \
		$(NSINSTALL) -D $(RELEASE_BIN_DIR);\
	else \
		true; \
	fi
	cp $(RELEASE_BINS) $(RELEASE_BIN_DIR)
endif
ifdef RELEASE_LIBS
	@echo "Copying libraries to release directory"
	@if test -z "$(BUILD_NUMBER)"; then \
		echo "BUILD_NUMBER must be defined"; \
		false; \
	else \
		true; \
	fi
	@if test ! -d $(RELEASE_LIBS_DEST); then \
		rm -rf $(RELEASE_LIBS_DEST); \
		$(NSINSTALL) -D $(RELEASE_LIBS_DEST);\
	else \
		true; \
	fi
	cp $(RELEASE_LIBS) $(RELEASE_LIBS_DEST)
endif
ifdef RELEASE_HEADERS
	@echo "Copying header files to release directory"
	@if test -z "$(BUILD_NUMBER)"; then \
		echo "BUILD_NUMBER must be defined"; \
		false; \
	else \
		true; \
	fi
	@if test ! -d $(RELEASE_HEADERS_DEST); then \
		rm -rf $(RELEASE_HEADERS_DEST); \
		$(NSINSTALL) -D $(RELEASE_HEADERS_DEST);\
	else \
		true; \
	fi
	cp $(RELEASE_HEADERS) $(RELEASE_HEADERS_DEST)
endif
	+$(LOOP_OVER_DIRS)

alltags:
	rm -f TAGS tags
	find . -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' \) -print | xargs etags -a
	find . -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' \) -print | xargs ctags -a

$(NFSPWD):
	cd $(@D); $(MAKE) $(@F)

$(PROGRAM): $(OBJS)
	@$(MAKE_OBJDIR)
ifeq ($(NS_USE_GCC)_$(OS_ARCH),_WINNT)
	$(CC) $(OBJS) -Fe$@ -link $(LDFLAGS) $(OS_LIBS) $(EXTRA_LIBS)
ifdef MT
	@if test -f $@.manifest; then \
		$(MT) -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		rm -f $@.manifest; \
	fi
endif	# MSVC with manifest tool
else	# WINNT && !GCC
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(LDFLAGS)
endif	# WINNT && !GCC
ifdef ENABLE_STRIP
	$(STRIP) $@
endif

$(LIBRARY): $(OBJS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(AR) $(AR_FLAGS) $(OBJS) $(AR_EXTRA_ARGS)
	$(RANLIB) $@

ifeq ($(OS_TARGET), OS2)
$(IMPORT_LIBRARY): $(MAPFILE)
	rm -f $@
	$(IMPLIB) $@ $(MAPFILE)
else
ifeq (,$(filter-out WIN95 WINCE,$(OS_TARGET)))
$(IMPORT_LIBRARY): $(SHARED_LIBRARY)
endif
endif

$(SHARED_LIBRARY): $(OBJS) $(RES) $(MAPFILE)
	@$(MAKE_OBJDIR)
	rm -f $@
ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
	echo "#!" > $(OBJDIR)/lib$(LIBRARY_NAME)_syms
	nm -B -C -g $(OBJS) \
		| awk '/ [T,D] / {print $$3}' \
		| sed -e 's/^\.//' \
		| sort -u >> $(OBJDIR)/lib$(LIBRARY_NAME)_syms
	$(LD) $(XCFLAGS) -o $@ $(OBJS) -bE:$(OBJDIR)/lib$(LIBRARY_NAME)_syms \
		-bM:SRE -bnoentry $(OS_LIBS) $(EXTRA_LIBS)
else	# AIX 4.1
ifeq ($(NS_USE_GCC)_$(OS_ARCH),_WINNT)
	$(LINK_DLL) -MAP $(DLLBASE) $(DLL_LIBS) $(EXTRA_LIBS) $(OBJS) $(RES)
ifdef MT
	@if test -f $@.manifest; then \
		$(MT) -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;2; \
		rm -f $@.manifest; \
	fi
endif	# MSVC with manifest tool
else	# WINNT && !GCC
	$(MKSHLIB) $(OBJS) $(RES) $(LDFLAGS) $(EXTRA_LIBS)
endif	# WINNT && !GCC
endif	# AIX 4.1
ifdef ENABLE_STRIP
	$(STRIP) $@
endif

ifeq ($(OS_ARCH),WINNT)
$(RES): $(RESNAME)
	@$(MAKE_OBJDIR)
# The resource compiler does not understand the -U option.
ifdef NS_USE_GCC
	$(RC) $(RCFLAGS) $(filter-out -U%,$(DEFINES)) $(INCLUDES:-I%=--include-dir %) -o $@ $<
else
	$(RC) $(RCFLAGS) $(filter-out -U%,$(DEFINES)) $(INCLUDES) -Fo$@ $<
endif # GCC
	@echo $(RES) finished
endif

$(MAPFILE): $(LIBRARY_NAME).def
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),SunOS)
	grep -v ';-' $< | \
	sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@
endif
ifeq ($(OS_ARCH),OS2)
	echo LIBRARY $(LIBRARY_NAME)$(LIBRARY_VERSION) INITINSTANCE TERMINSTANCE > $@
	echo PROTMODE >> $@
	echo CODE    LOADONCALL MOVEABLE DISCARDABLE >> $@
	echo DATA    PRELOAD MOVEABLE MULTIPLE NONSHARED >> $@
	echo EXPORTS >> $@
	grep -v ';+' $< | grep -v ';-' | \
	sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' -e 's,\([\t ]*\),\1_,' | \
	awk 'BEGIN {ord=1;} { print($$0 " @" ord " RESIDENTNAME"); ord++;}'	>> $@
	$(ADD_TO_DEF_FILE)
endif

#
# Translate source filenames to absolute paths. This is required for
# debuggers under Windows and OS/2 to find source files automatically.
#

ifeq (,$(filter-out AIX OS2,$(OS_ARCH)))
NEED_ABSOLUTE_PATH = 1
endif

ifeq ($(NS_USE_GCC)_$(OS_ARCH),_WINNT)
NEED_ABSOLUTE_PATH = 1
endif

ifdef NEED_ABSOLUTE_PATH
PWD := $(shell pwd)
# The quotes allow absolute paths to contain spaces.
pr_abspath = "$(if $(findstring :,$(1)),$(1),$(if $(filter /%,$(1)),$(1),$(PWD)/$(1)))"
endif

$(OBJDIR)/%.$(OBJ_SUFFIX): %.cpp
	@$(MAKE_OBJDIR)
ifeq ($(NS_USE_GCC)_$(OS_ARCH),_WINNT)
	$(CCC) -Fo$@ -c $(CCCFLAGS) $(call pr_abspath,$<)
else
ifeq ($(NS_USE_GCC)_$(OS_ARCH),_WINCE)
	$(CCC) -Fo$@ -c $(CCCFLAGS) $<
else
ifdef NEED_ABSOLUTE_PATH
	$(CCC) -o $@ -c $(CCCFLAGS) $(call pr_abspath,$<)
else
	$(CCC) -o $@ -c $(CCCFLAGS) $<
endif
endif
endif

WCCFLAGS1 = $(subst /,\\,$(CFLAGS))
WCCFLAGS2 = $(subst -I,-i=,$(WCCFLAGS1))
WCCFLAGS3 = $(subst -D,-d,$(WCCFLAGS2))
$(OBJDIR)/%.$(OBJ_SUFFIX): %.c
	@$(MAKE_OBJDIR)
ifeq ($(NS_USE_GCC)_$(OS_ARCH),_WINNT)
	$(CC) -Fo$@ -c $(CFLAGS) $(call pr_abspath,$<)
else
ifeq ($(NS_USE_GCC)_$(OS_ARCH),_WINCE)
	$(CC) -Fo$@ -c $(CFLAGS) $<
else
ifdef NEED_ABSOLUTE_PATH
	$(CC) -o $@ -c $(CFLAGS) $(call pr_abspath,$<)
else
	$(CC) -o $@ -c $(CFLAGS) $<
endif
endif
endif


$(OBJDIR)/%.$(OBJ_SUFFIX): %.s
	@$(MAKE_OBJDIR)
	$(AS) -o $@ $(ASFLAGS) -c $<

%.i: %.c
	$(CC) -C -E $(CFLAGS) $< > $*.i

%: %.pl
	rm -f $@; cp $< $@; chmod +x $@

#
# HACK ALERT
#
# The only purpose of this rule is to pass Mozilla's Tinderbox depend
# builds (http://tinderbox.mozilla.org/showbuilds.cgi).  Mozilla's
# Tinderbox builds NSPR continuously as part of the Mozilla client.
# Because NSPR's make depend is not implemented, whenever we change
# an NSPR header file, the depend build does not recompile the NSPR
# files that depend on the header.
#
# This rule makes all the objects depend on a dummy header file.
# Touch this dummy header file to force the depend build to recompile
# everything.
#
# This rule should be removed when make depend is implemented.
#

DUMMY_DEPEND_H = $(topsrcdir)/config/prdepend.h

$(filter $(OBJDIR)/%.$(OBJ_SUFFIX),$(OBJS)): $(OBJDIR)/%.$(OBJ_SUFFIX): $(DUMMY_DEPEND_H)

# END OF HACK

################################################################################
# Special gmake rules.
################################################################################

#
# Re-define the list of default suffixes, so gmake won't have to churn through
# hundreds of built-in suffix rules for stuff we don't need.
#
.SUFFIXES:
.SUFFIXES: .a .$(OBJ_SUFFIX) .c .cpp .s .h .i .pl

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all alltags clean export install libs realclean release

#
# List the target pattern of an implicit rule as a dependency of the
# special target .PRECIOUS to preserve intermediate files made by
# implicit rules whose target patterns match that file's name.
# (See GNU Make documentation, Edition 0.51, May 1996, Sec. 10.4,
# p. 107.)
#
.PRECIOUS: $(OBJDIR)/%.$(OBJ_SUFFIX)
