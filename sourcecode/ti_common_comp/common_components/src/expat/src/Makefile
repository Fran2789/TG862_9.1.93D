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
# * Makefile Wrapper file for building expat library
# *************************************************************************

include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

PACKAGE_VERSION := 2.0.1
PACKAGE_DIR := expat-$(PACKAGE_VERSION)
PACKAGE_CONF := \
	--host=$(patsubst %-gcc,%,$(CROSS)gcc) \
	--exec-prefix=$(TI_filesystem_path) \
	--prefix=$(TARGET_HOME)/ti \
	--includedir=$(TARGET_HOME)/ti/include \
	--with-gnu-ld

export CFLAGS += -Os

first_rule: all

all: build install_rule

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	@#ARRIS CHANGE
	@cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc ./configure INSTALL="/usr/bin/install -p -c" $(PACKAGE_CONF)
	@#END ARRIS CHANGE

build: $(CURDIR)/$(PACKAGE_DIR)/Makefile
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) all

# Need to install the library now because it is required by the next applications
config_rule:

install_rule:
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install
	@if [ -f $(TI_filesystem_path)/lib/libexpat.a ]; then mv -f $(TI_filesystem_path)/lib/libexpat.a $(TARGET_HOME)/ti/lib/libexpat.a; fi; 

clean_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; rm -rf $(CURDIR)/$(PACKAGE_DIR)/Makefile; fi

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) uninstall -i; fi

