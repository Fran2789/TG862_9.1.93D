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
# * Makefile Wrapper file for building quagga package
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

PACKAGE_VERSION := 0.99.16
PACKAGE_DIR := quagga-$(PACKAGE_VERSION)
PACKAGE_CONF := \
	--host=$(patsubst %-gcc,%,$(CROSS)gcc) \
	--with-cflags="-Drpl_malloc=malloc -Drpl_realloc=realloc $(CFLAGS_basic) " \
	--enable-user=root --enable-group=root --enable-vty-group=root \
	--disable-largefile --disable-pie \
	--localstatedir=/var --exec-prefix=$(TI_filesystem_path) --prefix=/ \
	--disable-doc --enable-ipv6 --enable-rtadv

ifndef CONFIG_TI_QUAGGA_BGP
PACKAGE_CONF += --disable-bgpd
endif

ifndef CONFIG_TI_QUAGGA_RIP
PACKAGE_CONF += --disable-ripd
endif

ifndef CONFIG_TI_QUAGGA_RIPNG
PACKAGE_CONF += --disable-ripngd
endif

ifndef CONFIG_TI_QUAGGA_OSPF
PACKAGE_CONF += --disable-ospfd
endif

ifndef CONFIG_TI_QUAGGA_OSPF6
PACKAGE_CONF += --disable-ospf6d
endif

ifndef CONFIG_TI_QUAGGA_WATCH
PACKAGE_CONF += --disable-watchquagga
endif

export LDFLAGS += $(LDLAGS_optimize)

first_rule: all

all: build

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	@# ARRIS CHANGE
	@cd $(CURDIR)/$(PACKAGE_DIR) && autoreconf -fi
	@cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc ./configure INSTALL="/usr/bin/install -p -c" $(PACKAGE_CONF)
	@# END ARRIS CHANGE

build: $(CURDIR)/$(PACKAGE_DIR)/Makefile
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) all

config_rule:

install_rule:
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install prefix=$(TARGET_HOME)/ti
	@if [ -f $(TI_filesystem_path)/lib/libzebra.a ]; then mv -f $(TI_filesystem_path)/lib/libzebra.a $(TARGET_HOME)/ti/lib/libzebra_pic.a; fi;	 
	mkdir -p $(TI_filesystem_path)$(TI_etcdir)/default 
	@install -m 0644 $(CURDIR)/zebra.conf $(TI_filesystem_path)/etc/default/
ifdef CONFIG_TI_QUAGGA_RIP
	@install -m 0644 $(CURDIR)/ripd.conf $(TI_filesystem_path)/etc/default/
endif
ifdef CONFIG_TI_QUAGGA_RIPNG
	@install -m 0644 $(CURDIR)/ripngd.conf $(TI_filesystem_path)/etc/default/
endif

clean_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi
	@rm -f $(TARGET_HOME)/ti/lib/libzebra_pic.a

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) uninstall -i; fi
	@rm -rf $(TI_filesystem_path)/etc/zebra.conf $(TI_filesystem_path)/etc/ripd.conf $(TI_filesystem_path)/etc/ripngd.conf 

