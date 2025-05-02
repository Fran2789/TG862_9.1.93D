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
# * Makefile Wrapper file for building dnsmasq-2.57 Package
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

PACKAGE_VERSION := 2.57
PACKAGE_DIR := dnsmasq-$(PACKAGE_VERSION)

export LDFLAGS += $(LDFLAGS_optimize)

first_rule: all

all: build

build: 
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) \
            KERNEL_DIR=$(TI_kernel_path)/$(TI_kernel_version) \
            LIBC_DIR=$(TI_uclibc_base) \
            CROSS_COMPILE=$(CROSS) \
            PREFIX=$(TARGET_HOME)/ti \
            COPTS=-DNO_DHCP \
            CFLAGS="$(CFLAGS_optimize) $(TI_cflags_ARMThumb)"

config_rule:

install_rule:
	$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install 
	#install -m 0644 $(CURDIR)/$(PACKAGE_DIR)/dnsmasq.conf.example $(TI_filesystem_path)$(TI_vardir)/tmp/dnsmasq.conf; 
	mkdir -p $(TI_filesystem_path)$(TI_etcdir)/default 
	rm -f $(TI_filesystem_path)$(TI_etcdir)/default/dnsmasq.conf 
	cp   $(CURDIR)/dnsmasq.conf $(TI_filesystem_path)$(TI_etcdir)/default/dnsmasq.conf 
	chmod 655 $(TI_filesystem_path)$(TI_etcdir)/default/dnsmasq.conf 
	cd $(TI_filesystem_path)$(TI_etcdir); ln -sf $(TI_vardir)/tmp/resolv.conf resolv.conf
	

clean_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) clean -i; fi	


uninstall_rule:
	@rm -f $(TI_filesystem_path)$(TI_sbindir)/dnsmasq
	@rm -f $(TI_filesystem_path)$(TI_etcdir)/default/dnsmasq.conf
	@rm -f $(TI_filesystem_path)$(TI_etcdir)/resolv.conf
	@rm -f $(TI_filesystem_path)$(TI_vardir)/tmp/dnsmasq.conf
	@rm -f $(TI_filesystem_path)$(TI_vardir)/tmp/resolv.conf
	
