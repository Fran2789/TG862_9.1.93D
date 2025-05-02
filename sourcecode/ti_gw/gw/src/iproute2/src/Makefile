################################################################################
##+--------------------------------------------------------------------------+##
##|                            ****                                          |##
##|                            ****                                          |##
##|                            ******o***                                    |##
##|                      ********_///_****                                   |##
##|                      ***** /_//_/ ****                                   |##
##|                       ** ** (__/ ****                                    |##
##|                           *********                                      |##
##|                            ****                                          |##
##|                            ***                                           |##
##|                                                                          |##
##|         Copyright (c) 1998-2010 Texas Instruments Incorporated           |##
##|                        ALL RIGHTS RESERVED                               |##
##|                                                                          |##
##| Permission is hereby granted to licensees of Texas Instruments           |##
##| Incorporated (TI) products to use this computer program for the sole     |##
##| purpose of implementing a licensee product based on TI products.         |##
##| No other rights to reproduce, use, or disseminate this computer          |##
##| program, whether in part or in whole, are granted.                       |##
##|                                                                          |##
##| TI makes no representation or warranties with respect to the             |##
##| performance of this computer program, and specifically disclaims         |##
##| any responsibility for any damages, special or consequential,            |##
##| connected with the use of this program.                                  |##
##|                                                                          |##
##+--------------------------------------------------------------------------+##
################################################################################

# *************************************************************************
# * Makefile Wrapper file for building IP Route2 Package
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

PACKAGE_VERSION := 2.6.39
PACKAGE_DIR := iproute2-$(PACKAGE_VERSION)
ifndef CURDIR
CURDIR=$(shell pwd)
endif
# IPT_LIB_DIR and LDFLAGS parameters are required for configure script
export IPT_LIB_DIR=$(TI_filesystem_path)/lib/xtables/
export CFLAGS += $(CFLAGS_optimize) 
export LDFLAGS="-L$(IPT_LIB_DIR) -L$(TI_filesystem_path)/lib/ $(LDFLAGS_optimize)"


first_rule: all

all: build install_rule

build: 
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR)

config_rule : # To avoid package iptables and iproute build order depencency, just create config file
	@echo  "Configure iproute2 : using xtables, iptables libs are in $$IPT_LIB_DIR"
	@echo  'TC_CONFIG_XT:=y' >$(CURDIR)/$(PACKAGE_DIR)/Config
	@echo  "IPT_LIB_DIR:=$$IPT_LIB_DIR" >>$(CURDIR)/$(PACKAGE_DIR)/Config

install_rule : build
	@install -m 0755 $(CURDIR)/$(PACKAGE_DIR)/ip/ip $(TI_filesystem_path)$(TI_sbindir)/ip
	@$(CROSS)strip $(TI_filesystem_path)$(TI_sbindir)/ip
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install
ifdef CONFIG_TI_DUAL_STACK_LITE
	install -m 0755 $(CURDIR)/$(PACKAGE_DIR)/ip6tnl1.sh $(TI_filesystem_path)$(TI_sbindir)/ip6tnl1
endif

clean_rule :
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) clean -i; fi
	@rm -rf $(CURDIR)/man
ifdef CONFIG_TI_DUAL_STACK_LITE
	@rm -f $(TI_filesystem_path)/usr/sbin/ip6tnl1
	@rm -f $(CURDIR)/$(PACKAGE_DIR)/Config
endif

uninstall_rule:
	rm -f $(TI_filesystem_path)$(TI_sbindir)/ip

