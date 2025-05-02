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
# * Makefile Wrapper file for building dibbler  package
# *************************************************************************

include $(TARGET_HOME)/.config

ifndef CURDIR
CURDIR=$(shell pwd)
endif

PACKAGE_VERSION := 1.5.1a
PACKAGE_DIR := nbtscan-$(PACKAGE_VERSION)
TARGET := nbtscan

PACKAGE_CONF := \
	--host=$(patsubst %-gcc,%,$(CROSS)gcc) \

# ARRIS ADD - Optimizations
export CFLAGS += -Os
# END ARRIS ADD

first_rule: all

all: build

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	#@cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc &&./configure $(PACKAGE_CONF)

build: $(CURDIR)/$(PACKAGE_DIR)/Makefile
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) $(TARGET) 

config_rule:
	@cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc &&./configure $(PACKAGE_CONF)

install_rule: 
	@cp $(CURDIR)/$(PACKAGE_DIR)/$(TARGET) $(TI_filesystem_path)/usr/sbin/
	
clean_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) clean -i; fi

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi



