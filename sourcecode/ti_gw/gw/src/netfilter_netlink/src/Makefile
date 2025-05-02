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
# * Makefile Wrapper file for building libnfnetlink
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
ifndef CURDIR
CURDIR=$(shell pwd)
endif

PACKAGE_VERSION := 1.0.0
PACKAGE_DIR := libnfnetlink-$(PACKAGE_VERSION)

PACKAGE_CONF := \
        --host=armeb-buildroot-linux-uclibcgnueabi --prefix=$(TI_filesystem_path) \
        --includedir=$(TI_include) \
        CFLAGS="$(CFLAGS_basic)" \
        --with-gnu-ld \
        --exec-prefix=$(TI_filesystem_path) --datarootdir=$(CURDIR)

first_rule: all

all: build

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	# For some reason these files must not be CI, so for working with VersionControl, we create new ones
	@cd $(CURDIR)/$(PACKAGE_DIR) && cp Makefile.in.orig Makefile.in
	@cd $(CURDIR)/$(PACKAGE_DIR) && cp aclocal.m4.orig aclocal.m4
	@cd $(CURDIR)/$(PACKAGE_DIR) && cp configure.orig configure
	# Now configure
	cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc ./configure $(PACKAGE_CONF) 

build_lib : $(CURDIR)/$(PACKAGE_DIR)/Makefile
	$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) all 

build: install_rule

config_rule: $(CURDIR)/$(PACKAGE_DIR)/Makefile

$(CURDIR)/$(PACKAGE_DIR)/src/.libs/libnfnetlink.so: build_lib

install_rule install: $(CURDIR)/$(PACKAGE_DIR)/src/.libs/libnfnetlink.so
	$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install 
	if [ -f $(TI_filesystem_path)/lib/libnfnetlink.so ]; then cp -f $(TI_filesystem_path)/lib/libnfnetlink.* $(TARGET_HOME)/ti/lib/ ; fi; 
	# ARRIS ADD BEGIN
	if [ -f $(TI_filesystem_path)/lib/libnfnetlink.a ]; then install --preserve-timestamps -m 0644 $(TI_filesystem_path)/lib/libnfnetlink.a $(COMPONENT_PREFIX)/build ; fi; 
	# ARRIS ADD END

clean_rule distclean clean:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi
	@rm -f $(TARGET_HOME)/ti/lib/libnfnetlink.*  
	@rm -f $(CURDIR)/$(PACKAGE_DIR)/Makefile.in  
	@rm -f $(CURDIR)/$(PACKAGE_DIR)/aclocal.m4  
	@rm -f $(CURDIR)/$(PACKAGE_DIR)/configure 
	@rm -rf $(CURDIR)/man

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) uninstall -i; fi

