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
PACKAGE_VERSION := 1.0.0RC1

PACKAGE_DIR := dibbler-$(PACKAGE_VERSION)

export CPPFLAGS += $(TI_cflags_ARMThumb) $(CFLAGS_optimize)
export CFLAGS += $(TI_cflags_ARMThumb) $(CFLAGS_optimize)

# UNIHAN ADD
ifeq (${CONFIG_VENDOR_ARRIS_GATEWAY_TR69},y)
export CPPFLAGS += -DINCLUDE_ARRIS_GW_TR69 -I$(TARGET_HOME)/ti/include
export CFLAGS += -DINCLUDE_ARRIS_GW_TR69 -I$(TARGET_HOME)/ti/include
endif

ifeq (${CONFIG_VENDOR_COMCAST},y)
export CPPFLAGS += -DCONFIG_VENDOR_COMCAST
export CFLAGS += -DCONFIG_VENDOR_COMCAST
endif

ifeq (${CONFIG_VENDOR_TIME_WARNER},y)
export CPPFLAGS += -DCONFIG_VENDOR_TIME_WARNER
export CFLAGS += -DCONFIG_VENDOR_TIME_WARNER
endif
# UNIHAN ADD END

export LDFLAGS += $(LDLAGS_optimize)

first_rule: all

all: build 

$(PACKAGE_DIR)/include/dibbler-config.h: $(PACKAGE_DIR)/configure
	cd $(PACKAGE_DIR); ac_cv_func_realloc_0_nonnull=yes ac_cv_func_malloc_0_nonnull=yes ./configure --prefix=$(TI_filesystem_path) --host=$(patsubst %-gcc,%,$(CROSS)gcc) --disable-dns-update --disable-link-state

build: $(PACKAGE_DIR)/include/dibbler-config.h
	CC=$(CC) CXX=$(CROSS)g++ LIBS=$(LIBS) CPPFLAGS="$(CPPFLAGS)" CFLAGS="$(CFLAGS)" $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) all

config_rule:

install_rule:
	install $(CURDIR)/$(PACKAGE_DIR)/dibbler-server $(TI_filesystem_path)/usr/sbin/
	@mkdir -p $(TI_filesystem_path)/var/lib/dibbler
	@mkdir -p $(TI_filesystem_path)/etc/dibbler
	@ln -sf /var/tmp/dhcp6s.conf $(TI_filesystem_path)/etc/dibbler/server.conf
	@install   $(CURDIR)/scripts/dibbler-server-notify.sh $(TI_filesystem_path)/var/lib/dibbler/
		


clean_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) clean -i; fi
	@rm -rf $(TI_filesystem_path)/usr/sbin/dibbler-server
	@rm -rf $(TI_filesystem_path)/etc/dibbler/
	@rm -rf $(TI_filesystem_path)/var/lib/dibbler/

uninstall_rule:
	@rm -rf $(TI_filesystem_path)/usr/sbin/dibbler-server
	@rm -rf $(TI_filesystem_path)/etc/dibbler/
	@rm -rf $(TI_filesystem_path)/var/lib/dibbler/


