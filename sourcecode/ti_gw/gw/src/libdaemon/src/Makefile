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
# * Makefile Wrapper file for building libdaemon library
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

BUILD_DIR := build
PACKAGE_VERSION := 0.14
PACKAGE_DIR := libdaemon-$(PACKAGE_VERSION)
PACKAGE_CONF := \
	--host=$(patsubst %-gcc,%,$(CROSS)gcc) \
	--exec-prefix=$(TI_filesystem_path) \
	--prefix=$(TARGET_HOME)/ti \
	--includedir=$(TARGET_HOME)/ti/include \
	--with-gnu-ld \
	--disable-largefile \
	--disable-lynx \
	--disable-examples \
	ac_cv_func_setpgrp_void=yes
    
export CFLAGS += $(CFLAGS_optimize)
export LDFLAGS += $(LDFLAGS_optimize)

first_rule: all

all: build install_rule

$(CURDIR)/$(BUILD_DIR)/Makefile:
	@cp -f $(CURDIR)/$(PACKAGE_DIR)/config.h.in_orig $(CURDIR)/$(PACKAGE_DIR)/config.h.in
	@chmod +w $(CURDIR)/$(PACKAGE_DIR)/config.h.in
	@test -d $(CURDIR)/$(BUILD_DIR) || mkdir $(CURDIR)/$(BUILD_DIR)	
	@cd $(CURDIR)/$(PACKAGE_DIR) && autoreconf -fi #ARRIS ADD
	@cd $(CURDIR)/$(BUILD_DIR) && ../$(PACKAGE_DIR)/configure $(PACKAGE_CONF)

build: $(CURDIR)/$(BUILD_DIR)/Makefile
	@$(MAKE) -C $(CURDIR)/$(BUILD_DIR) all

# Need to install the library now because it is required by the next applications
config_rule:

$(CURDIR)/$(BUILD_DIR)/libdaemon/.libs/libdaemon.so: build

install_rule: $(CURDIR)/$(BUILD_DIR)/libdaemon/.libs/libdaemon.so
	@$(MAKE) -C $(CURDIR)/$(BUILD_DIR) install
	@if [ -f $(TI_filesystem_path)/lib/libdaemon.a ]; then mv -f $(TI_filesystem_path)/lib/libdaemon.a $(TARGET_HOME)/ti/lib/libdaemon.a; fi; 

clean_rule:
	@if [ -f $(CURDIR)/$(BUILD_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(BUILD_DIR) distclean -i; rm -rf $(CURDIR)/$(BUILD_DIR)/Makefile; fi

uninstall_rule:
	@if [ -f $(CURDIR)/$(BUILD_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(BUILD_DIR) uninstall -i; fi

