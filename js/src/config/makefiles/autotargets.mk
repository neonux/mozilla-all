# -*- makefile -*-
# vim:set ts=8 sw=8 sts=8 noet:
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
# The Initial Developer of the Original Code is
# The Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Joey Armstrong <joey@mozilla.com>
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

SPACE ?= $(NULL) $(NULL)

  get_auto_arg = $(word $(2),$(subst ^,$(SPACE),$(1))) # get(1=var, 2=offset)
gen_auto_macro = $(addsuffix ^$(1),$(2))  # gen(1=target_pattern, 2=value)

###########################################################################
## Automatic dependency macro generation.
## Macros should be defined prior to the inclusion of rules.mk
##  GENERATED_DIRS - a list of directories to create
##  AUTO_DEPS      - [returned] a list of generated deps targets can depend on
##  Usage:
##    all bootstrap: $(AUTO_DEPS)
##    target: $(dir)/.dir.done $(dir)/foobar
###########################################################################
ifdef GENERATED_DIRS
  GENERATED_DIRS_DEP = $(foreach dir,$(GENERATED_DIRS),$(dir)/.dir.done)
  AUTO_DEPS += $(GENERATED_DIRS_DEP)
endif

.SECONDARY: $(GENERATED_DIRS) # preserve intermediates: .dir.done

###################################################################
## Thread safe directory creation
##   - targets suffixed by a slash will match and be processed
##   - macro AUTO_DEPS can be used to explicitly add a list of deps
##   - single: $(call threadsafe_mkdir,$(var))
##   - $(foreach dir,$(list),$(call threadsafe_mkdir,$(dir)))
###################################################################
$(GENERATED_DIRS_DEP):
	$(MKDIR) -p $(dir $@)
	@touch $@
