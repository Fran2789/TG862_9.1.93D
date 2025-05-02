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
# * Makefile Wrapper file for building IP Tables
# * Makefile Wrapper file for building libmnl
# *************************************************************************

.NOTPARALLEL:

#include $(TARGET_HOME)/TI_Build.make
ifndef CURDIR
CURDIR=$(shell pwd)
endif

PACKAGE_VERSION := 1.0.3
PACKAGE_DIR := libmnl-$(PACKAGE_VERSION)
PACKAGE_CONF := \
        --host=armeb-buildroot-linux-uclibcgnueabi --prefix=$(TI_filesystem_path) \
         --includedir=$(TARGET_HOME)/ti/include \
        --with-gnu-ld \
        --exec-prefix=$(TI_filesystem_path) --datarootdir=$(CURDIR)

export CFLAGS += $(CFLAGS_optimize)
export LDFLAGS += $(LDFLAGS_optimize)

first_rule: all

all: build

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	# For some reason these files must not be CI, so for working with VersionControl, we create new ones
	#@cd $(CURDIR)/$(PACKAGE_DIR) && cp Makefile.in.orig Makefile.in
	#@cd $(CURDIR)/$(PACKAGE_DIR) && cp aclocal.m4.orig aclocal.m4
	#@cd $(CURDIR)/$(PACKAGE_DIR) && cp configure.orig configure
	# Now configure
	rm -rf $(CURDIR)/$(PACKAGE_DIR)
	tar -xjf libmnl-$(PACKAGE_VERSION).tar.bz2
	@cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc ./configure $(PACKAGE_CONF)

build_lib : $(CURDIR)/$(PACKAGE_DIR)/Makefile
	$(MAKE) -j1 -C $(CURDIR)/$(PACKAGE_DIR) all

build: build_lib install_rule
	@$(MAKE) -j1 -C $(CURDIR)/$(PACKAGE_DIR) all check
	#install -D $(CURDIR)/$(PACKAGE_DIR)/include/libmnl/libmnl.h $(TI_include)/libmnl/libmnl.h
	#cp -f $(CURDIR)/$(PACKAGE_DIR)/src/.libs/libmnl.* $(TARGET_HOME)/ti/lib/
	#cp -f $(CURDIR)/$(PACKAGE_DIR)/src/.libs/libmnl.* $(TI_filesystem_path)/lib/

config_rule: $(CURDIR)/$(PACKAGE_DIR)/Makefile

install_rule install:
	$(MAKE) -j1 -C $(CURDIR)/$(PACKAGE_DIR) install
	if [ -f $(TI_filesystem_path)/lib/libmnl.so ]; then cp -f $(TI_filesystem_path)/lib/libmnl.* $(TARGET_HOME)/ti/lib/ ; fi; 

clean_rule distclean clean:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi
	rm -rf $(CURDIR)/$(PACKAGE_DIR)
	rm -f $(TARGET_HOME)/ti/lib/libmnl.* 

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) uninstall -i; fi

