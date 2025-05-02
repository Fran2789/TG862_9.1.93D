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
# * Makefile Wrapper file for building udns package
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config
ifndef CURDIR
CURDIR=$(shell pwd)
endif

PACKAGE_VERSION := 0.8.2
PACKAGE_DIR := ipp2p-$(PACKAGE_VERSION)
BUILD_DIR := $(PACKAGE_DIR)

export CFLAGS += -Os


first_rule: all

all: build


build: 
	@$(MAKE) -C $(CURDIR)/$(BUILD_DIR) all
	@$(MAKE) -C $(CURDIR)/$(BUILD_DIR) install
	ln -sf $(CURDIR)/$(BUILD_DIR)/ipt_ipp2p.h ../include/ipt_ipp2p.h

config_rule:

install_rule:
	@$(MAKE) -C $(CURDIR)/$(BUILD_DIR) install
	

clean_rule:
	@if [ -f $(CURDIR)/$(BUILD_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(BUILD_DIR) clean -i; fi

uninstall_rule:
	@if [ -f $(CURDIR)/$(BUILD_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(BUILD_DIR) uninstall -i; fi	

