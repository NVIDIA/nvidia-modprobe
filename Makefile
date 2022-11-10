#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


##############################################################################
# include common variables and functions
##############################################################################

include utils.mk


##############################################################################
# assign variables
##############################################################################

NVIDIA_MODPROBE = $(OUTPUTDIR)/nvidia-modprobe

NVIDIA_MODPROBE_PROGRAM_NAME = "nvidia-modprobe"

NVIDIA_MODPROBE_VERSION := $(NVIDIA_VERSION)

MANPAGE_GZIP ?= 1

MANPAGE_not_gzipped     = $(OUTPUTDIR)/nvidia-modprobe.1
MANPAGE_gzipped     = $(OUTPUTDIR)/nvidia-modprobe.1.gz

ifeq ($(MANPAGE_GZIP),1)
  MANPAGE     = $(MANPAGE_gzipped)
else
  MANPAGE     = $(MANPAGE_not_gzipped)
endif
GEN_MANPAGE_OPTS   = $(OUTPUTDIR_ABSOLUTE)/gen-manpage-opts
OPTIONS_1_INC      = $(OUTPUTDIR)/options.1.inc


##############################################################################
# The common-utils directory may be in one of two places: either
# elsewhere in the driver source tree when building nvidia-modprobe as
# part of the NVIDIA driver build (in which case, COMMON_UTILS_DIR
# should be defined by the calling makefile), or directly in the
# source directory when building from the nvidia-modprobe source
# tarball (in which case, the below conditional assignment should be
# used)
##############################################################################

COMMON_UTILS_DIR          ?= common-utils

##############################################################################
# The modprobe-utils directory may be in one of two places: either
# elsewhere in the driver source tree when building as part of the
# NVIDIA driver build (in which case, MODPROBE_UTILS_DIR should be
# defined by the calling makefile), or directly in the source
# directory when building from source tarball (in which case, the
# below conditional assignment should be used)
##############################################################################

MODPROBE_UTILS_DIR        ?= modprobe-utils

# include the list of source files; defines SRC
include dist-files.mk

include $(COMMON_UTILS_DIR)/src.mk
SRC += $(addprefix $(COMMON_UTILS_DIR)/,$(COMMON_UTILS_SRC))

include $(MODPROBE_UTILS_DIR)/nvidia-modprobe-utils.mk
SRC += $(addprefix $(MODPROBE_UTILS_DIR)/,$(MODPROBE_UTILS_SRC))

OBJS = $(call BUILD_OBJECT_LIST,$(SRC))

common_cflags += -I $(OUTPUTDIR)
common_cflags += -I $(COMMON_UTILS_DIR)
common_cflags += -I $(MODPROBE_UTILS_DIR)
common_cflags += -DPROGRAM_NAME=\"$(NVIDIA_MODPROBE_PROGRAM_NAME)\"
# Enable gnu99 for use of functions like snprintf(3).
common_cflags += -std=gnu99
common_cflags += -pedantic

CFLAGS += $(common_cflags)
HOST_CFLAGS += $(common_cflags)


##############################################################################
# build rules
##############################################################################

.PHONY: all
all: $(NVIDIA_MODPROBE) $(MANPAGE)

.PHONY: install
install: NVIDIA_MODPROBE_install MANPAGE_install

.PHONY: NVIDIA_MODPROBE_install
NVIDIA_MODPROBE_install: $(NVIDIA_MODPROBE)
	$(MKDIR) $(BINDIR)
	$(INSTALL) $(INSTALL_BIN_ARGS) $< $(BINDIR)/$(notdir $<)

.PHONY: MANPAGE_install
MANPAGE_install: $(MANPAGE)
	$(MKDIR) $(MANDIR)
	$(INSTALL) $(INSTALL_BIN_ARGS) $< $(MANDIR)/$(notdir $<)

$(eval $(call DEBUG_INFO_RULES, $(NVIDIA_MODPROBE)))
$(NVIDIA_MODPROBE).unstripped: $(OBJS)
	$(call quiet_cmd,LINK) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ \
	  $(BIN_LDFLAGS)

# define the rule to build each object file
$(foreach src,$(SRC),$(eval $(call DEFINE_OBJECT_RULE,TARGET,$(src))))

.PHONY: clean clobber
clean clobber:
	rm -rf $(NVIDIA_MODPROBE) $(MANPAGE) *~ \
	  $(OUTPUTDIR)/*.o $(OUTPUTDIR)/*.d \
	  $(GEN_MANPAGE_OPTS) $(OPTIONS_1_INC)


##############################################################################
# Documentation
##############################################################################

AUTO_TEXT = ".\\\" WARNING: THIS FILE IS AUTO-GENERATED!  Edit $< instead."

.PHONY: doc
doc: $(MANPAGE)

GEN_MANPAGE_OPTS_SRC = gen-manpage-opts.c
GEN_MANPAGE_OPTS_SRC += $(COMMON_UTILS_DIR)/gen-manpage-opts-helper.c

GEN_MANPAGE_OPTS_OBJS = $(call BUILD_OBJECT_LIST,$(GEN_MANPAGE_OPTS_SRC))

$(foreach src,$(GEN_MANPAGE_OPTS_SRC), \
    $(eval $(call DEFINE_OBJECT_RULE,HOST,$(src))))

$(GEN_MANPAGE_OPTS): $(GEN_MANPAGE_OPTS_OBJS)
	$(call quiet_cmd,HOST_LINK) \
	    $(HOST_CFLAGS) $(HOST_LDFLAGS) $(HOST_BIN_LDFLAGS) $^ -o $@

$(OPTIONS_1_INC): $(GEN_MANPAGE_OPTS)
	@$< > $@

$(MANPAGE_not_gzipped): nvidia-modprobe.1.m4 $(OPTIONS_1_INC) $(VERSION_MK)
	$(call quiet_cmd,M4) -D__HEADER__=$(AUTO_TEXT) -I $(OUTPUTDIR) \
	  -D__VERSION__=$(NVIDIA_VERSION) \
	  -D__DATE__="`$(DATE) +%F`" \
	  -D__BUILD_OS__=$(TARGET_OS) \
	  $< > $@

$(MANPAGE_gzipped): $(MANPAGE_not_gzipped)
	$(GZIP_CMD) -9nf < $< > $@
