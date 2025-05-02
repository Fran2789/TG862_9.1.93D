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
# * Makefile Wrapper file for building ssmtp
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

PACKAGE_VERSION := 2.64
PACKAGE_DIR := ssmtp-$(PACKAGE_VERSION)
PACKAGE_CONF := \
	--host=$(patsubst %-gcc,%,$(CROSS)gcc) \
	--exec-prefix=$(TI_filesystem_path) \
	--prefix="" \
	--enable-inet6 \
	--enable-logfile \
	--enable-ssl

export CFLAGS = $(CFLAGS_basic)

first_rule: all

all: build_bin

install: install_rule

uninstall: uninstall_rule

clean: clean_rule

config:
$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	@cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc ./configure $(PACKAGE_CONF) 

# ARRIS MOD : revert to TI_filesystem_basefs_path to unbreak build
build_bin: $(CURDIR)/$(PACKAGE_DIR)/Makefile
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) LDFLAGS+="-L$(TI_lib_path) -Wl,--rpath -Wl,$(TI_filesystem_basefs_path)/lib "

config_rule:

install_rule:
	@cp $(CURDIR)/$(PACKAGE_DIR)/ssmtp $(TI_filesystem_path)/sbin/
	@install -m 0644 $(CURDIR)/$(PACKAGE_DIR)/ssmtp.conf $(TI_filesystem_path)$(TI_vardir)/tmp/
	@install -m 0644 $(CURDIR)/$(PACKAGE_DIR)/revaliases $(TI_filesystem_path)$(TI_vardir)/tmp/
	@(cd  $(TI_filesystem_path)$(TI_etcdir); ln -sf /nvram/8/ssmtp.conf ssmtp.conf)
	@(cd  $(TI_filesystem_path)$(TI_etcdir); ln -sf /nvram/8/revaliases revaliases)

clean_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) clean; $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean; fi

uninstall_rule:
	@if [ -f $(TI_filesystem_path)/sbin/ssmtp ]; then rm -f $(TI_filesystem_path)$(TI_etcdir)/ssmtp.conf; rm -f $(TI_filesystem_path)/etc/revaliases; rm -f $(TI_filesystem_path)$(TI_vardir)/tmp/ssmtp.conf; rm -f $(TI_filesystem_path)$(TI_vardir)/tmp/revaliases; fi

