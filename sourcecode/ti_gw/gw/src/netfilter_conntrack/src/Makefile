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
# * Makefile Wrapper file for building libnetfilter_conntrack
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
ifndef CURDIR
CURDIR=$(shell pwd)
endif

PACKAGE_VERSION := 1.0.1
PACKAGE_DIR := libnetfilter_conntrack-$(PACKAGE_VERSION)
PACKAGE_CONF := \
        --host=armeb-buildroot-linux-uclibcgnueabi --prefix=$(TI_filesystem_path) \
        LIBNFNETLINK_CFLAGS="-I$(TARGET_HOME)/ti/include" \
        LIBNFNETLINK_LIBS="-L$(TARGET_HOME)/ti/lib -L$(TI_filesystem_path)/lib -lnfnetlink " \
        --with-gnu-ld \
        --exec-prefix=$(TI_filesystem_path) --datarootdir=$(CURDIR)

export CFLAGS += $(CFLAGS_optimize)

first_rule: all

all: build

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	# For some reason these files must not be CI, so for working with VersionControl, we create new ones
	@cd $(CURDIR)/$(PACKAGE_DIR) && cp Makefile.in.orig Makefile.in
	@cd $(CURDIR)/$(PACKAGE_DIR) && cp aclocal.m4.orig aclocal.m4
	@cd $(CURDIR)/$(PACKAGE_DIR) && cp configure.orig configure
	# Now configure
	@cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc ./configure $(PACKAGE_CONF)

build: $(CURDIR)/$(PACKAGE_DIR)/Makefile
	@$(MAKE) -j1 -C $(CURDIR)/$(PACKAGE_DIR) all check
	# ARRIS ADD BEGIN 
	if [ -f $(CURDIR)/$(PACKAGE_DIR)/src/.libs/libnetfilter_conntrack.so ]; then install --preserve-timestamps -m 0644 $(CURDIR)/$(PACKAGE_DIR)/src/.libs/libnetfilter_conntrack.* $(TI_lib_path) ; fi; 
	# ARRIS ADD END
	install -p $(CURDIR)/$(PACKAGE_DIR)/utils/.libs/conntrack_dump $(TI_filesystem_path)$(TI_sbindir)
	install -p $(CURDIR)/$(PACKAGE_DIR)/utils/.libs/conntrack_flush $(TI_filesystem_path)$(TI_sbindir)
	install -p $(CURDIR)/$(PACKAGE_DIR)/utils/.libs/expect_dump $(TI_filesystem_path)$(TI_sbindir)
	install -p $(CURDIR)/$(PACKAGE_DIR)/utils/.libs/expect_flush $(TI_filesystem_path)$(TI_sbindir)

config_rule: $(CURDIR)/$(PACKAGE_DIR)/Makefile

$(CURDIR)/$(PACKAGE_DIR)/src/.libs/libnetfilter_conntrack.so: build

install_rule install:  $(CURDIR)/$(PACKAGE_DIR)/src/.libs/libnetfilter_conntrack.so
	$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install 

clean_rule distclean clean:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi
	rm -f $(TI_filesystem_path)$(TI_sbindir)conntrack_dump
	rm -f $(TI_filesystem_path)$(TI_sbindir)conntrack_flush
	rm -f $(CURDIR)/$(PACKAGE_DIR)/Makefile.in  
	rm -f $(CURDIR)/$(PACKAGE_DIR)/aclocal.m4  
	rm -f $(CURDIR)/$(PACKAGE_DIR)/configure 
	rm -f $(TI_filesystem_path)$(TI_sbindir)expect_dump
	rm -f $(TI_filesystem_path)$(TI_sbindir)expect_flush

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) uninstall -i; fi
	rm -f $(TI_filesystem_path)$(TI_sbindir)conntrack_dump
	rm -f $(TI_filesystem_path)$(TI_sbindir)conntrack_flush
	rm -f $(TI_filesystem_path)$(TI_sbindir)expect_dump
	rm -f $(TI_filesystem_path)$(TI_sbindir)expect_flush

