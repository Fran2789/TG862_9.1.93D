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

# MTC - ARRIS MODIFICATION
# Edited path for xml-prefix to not include \lib - Line 43
# added autoreconf to build on any platform - Line 61
# *************************************************************************
# * Makefile Wrapper file for building dibbler  package
# *************************************************************************

include $(TARGET_HOME)/.config

PACKAGE_VERSION := 1.1.0
PACKAGE_DIR := libsoap-$(PACKAGE_VERSION)
#ARRIS CHANGE - preserve timestamps
PACKAGE_CONF := \
	--host=$(patsubst %-gcc,%,$(CROSS)gcc) \
	--target=$(patsubst %-gcc,%,$(CROSS)gcc) \
	--prefix=$(TARGET_HOME)/ti \
	--includedir=$(TARGET_HOME)/ti/include \
	--with-ssl=$(TARGET_HOME)/ti \
	--with-xml-prefix=$(TARGET_HOME)/ti/lib \
	--exec-prefix=$(TI_filesystem_path) \
	INSTALL_DATA=" install -p -m 0644"
#END ARRIS CHANGE

export LDFLAGS = -L$(TI_filesystem_basefs_path)/lib -L$(TARGET_HOME)/ti/lib $(LDFLAGS_optimize)
export CFLAGS = -I$(TARGET_HOME)/ti/include -I$(TARGET_HOME)/ti/include/libxml2 $(CFLAGS_optimize)
export CROSS_COMPILE = armeb-buildroot-linux-uclibcgnueabi-
export ARCH=arm
nanohttp-headers-list := $(shell ls $(CURDIR)/$(PACKAGE_DIR)/nanohttp/*.h 2>  /dev/null)
csoap-headers-list := $(shell ls $(CURDIR)/$(PACKAGE_DIR)/libcsoap/*.h 2>  /dev/null)


first_rule: all

all: build

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	@cd $(CURDIR)/$(PACKAGE_DIR) && autoreconf -ivf
	@cd $(CURDIR)/$(PACKAGE_DIR) &&  CC=$(CROSS)gcc \
					 ac_cv_func_malloc_0_nonnull=yes \
					 ac_cv_func_realloc_0_nonnull=yes \
					 ./configure $(PACKAGE_CONF)

build: $(CURDIR)/$(PACKAGE_DIR)/Makefile
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) all

config_rule:

install_rule: 
	@echo "***"
	@echo "***"
	@echo "*** libsoap exec-prefix : $(TI_filesystem_path)"
	@echo "***"
	@echo "***"
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install
	@mkdir -p $(TI_include)/nanohttp
	@mkdir -p $(TI_include)/libcsoap
	@echo nanohttp-headers-list $(nanohttp-headers-list)
	@echo csoap-headers-list $(csoap-headers-list)
	@install --preserve-timestamps -m 0644 $(nanohttp-headers-list) $(TI_include)/nanohttp/
	@install --preserve-timestamps -m 0644 $(csoap-headers-list) $(TI_include)/libcsoap/
	@if [ -f $(TI_filesystem_path)/lib/libcsoap.so ]; then mv -f $(TI_filesystem_path)/lib/libcsoap.so $(TARGET_HOME)/ti/lib/libcsoap.so; fi;
	@if [ -f $(TI_filesystem_path)/lib/libnanohttp.so ]; then mv -f $(TI_filesystem_path)/lib/libnanohttp.so $(TARGET_HOME)/ti/lib/libnanohttp.so; fi;
		
clean_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi
	@rm -rf $(TI_filesystem_path)$(TI_rlibdir)/libcsoap.so
	@rm -rf $(TI_filesystem_path)$(TI_rlibdir)/libnanohttp.so

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi
	@rm -rf $(TI_include)/nanohttp
	@rm -rf $(TI_include)/libcsoap


