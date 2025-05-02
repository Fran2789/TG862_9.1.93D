# 
#  GPL LICENSE SUMMARY
#
#  Copyright(c) 2005-2011 Intel Corporation.
#
#  This program is free software; you can redistribute it and/or modify 
#  it under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful, but 
#  WITHOUT ANY WARRANTY; without even the implied warranty of 
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License 
#  along with this program; if not, write to the Free Software 
#  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#  The full GNU General Public License is included in this distribution 
#  in the file called LICENSE.GPL.
#
#  Contact Information:
#    Intel Corporation
#    2200 Mission College Blvd.
#    Santa Clara, CA  97052
#

# *************************************************************************
# * Makefile Wrapper file for building EB Tables
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

PACKAGE_VERSION := v2.0.10-4
PACKAGE_DIR := ebtables-$(PACKAGE_VERSION)
BUILD_DIR := $(PACKAGE_DIR)
ifndef CURDIR
CURDIR=$(shell pwd)
endif

export CFLAGS += $(CFLAGS_basic) 
export LDFLAGS="-L$(TARGET_HOME)/ti/lib -L$(TI_filesystem_path)/lib/ $(LDLAGS_optimize)"




first_rule: all

all: build


build: 
	@$(MAKE) -C $(CURDIR)/$(BUILD_DIR) all
	@$(MAKE) -C $(CURDIR)/$(BUILD_DIR) install


config_rule: 

install_rule install:
	$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install 
	@echo "***"
	@echo "***"
	@echo "*** Ebtables prefix : $(TI_filesystem_path)"
	@echo "***"
	@echo "***"

clean_rule :
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) clean -i; fi

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) uninstall -i; fi

