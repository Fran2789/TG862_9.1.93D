#
# File Name: Makefile
#
# Description: Makefile to build kernel modules for TI voice software.
#
# Copyright (C) 2008 Texas Instruments, Incorporated
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation version 2.
#
# This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
# whether express or implied; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#

.PHONY: all gpl_build $(gpl_MODULE) gpl_package clean distclean gpl_clean gpl_distclean

export TI_MASDK_gpl_ROOT ?= $(shell pwd)
export CPU=ARM11
export TOOL=sfgnu
export MASDK_PFORM:=puma

export REPORT ?= @echo

# Include MASDK gpl Definitions
include $(TI_MASDK_gpl_ROOT)/build/gpl.def

GPL_MODULES_CLEAN := $(addsuffix _clean, $(gpl_MODULE))

all gpl_build: $(gpl_MODULE)
	$(REPORT) Done building MAS GPL MODULES
	$(REPORT) ' '

$(gpl_MODULE):
	$(REPORT) Building $@ kernel modules
	$(QUIET_CMD)make --directory=$($@_MODULE_PATH)

%_clean:
	$(QUIET_CMD)make --directory=$($*_MODULE_PATH) clean

gpl_package:
	$(REPORT) Creating MAS GPL package
	$(TI_MASDK_PACKAGE_SCRIPTS_DIR)/mkpkg -s -o $(TI_MASDK_PACKAGE_OUT_DIR)

clean distclean gpl_clean gpl_distclean: $(GPL_MODULES_CLEAN)
	$(REPORT) Done cleaning MAS GPL MODULES

