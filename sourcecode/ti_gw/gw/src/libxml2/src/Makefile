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
# * Makefile Wrapper file for building Libxml2 library
# *************************************************************************

include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

export CFLAGS := -Os -mthumb -mthumb-interwork -I$(TARGET_HOME)/ti/include

PACKAGE_VERSION := 2.7.7
PACKAGE_DIR := libxml2-$(PACKAGE_VERSION)
PACKAGE_CONF := \
	--host=$(patsubst %-gcc,%,$(CROSS)gcc) \
	--exec-prefix=$(TI_filesystem_path) \
	--prefix=$(TARGET_HOME)/ti \
	--includedir=$(TARGET_HOME)/ti/include \
	--without-iconv \
	--without-python \
	--with-zlib=$(TI_filesystem_path)/lib \
	--with-pic --without-debug \
	--without-docbook --without-ftp --without-history --without-legacy


#        --with-pic --without-c14n --without-catalog --without-debug \
#        --without-docbook --without-ftp --without-history --without-legacy \
#        --with-minimum --disable-static
	
# UNIHAN ADD START
PACKAGE_CONF += --without-threads
# UNIHAN ADD END
	
export CFLAGS  := $(CFLAGS_optimize) -g $(TI_cflags_ARMThumb)
export LDFLAGS := $(LDLAGS_optimize)

# -mthumb -mthumb-interwork -I$(TARGET_HOME)/ti/include
#unexport CFLAGS
unexport LDFLAGS
unexport CPPFLAGS 
	
first_rule: all

all: build

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	@# ARRIS CHANGE
	@cd $(CURDIR)/$(PACKAGE_DIR) && autoreconf -fi
	@cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc ./configure INSTALL="/usr/bin/install -p -c" $(PACKAGE_CONF)
	@# END ARRIS CHANGE

build_lib: $(CURDIR)/$(PACKAGE_DIR)/Makefile
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) all

# Need to install the library now because it is required by the next applications
build: install_rule

config_rule:

$(CURDIR)/$(PACKAGE_DIR)/.libs/libxml2.so: build_lib

install_rule: $(CURDIR)/$(PACKAGE_DIR)/.libs/libxml2.so
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install
	@mkdir -p $(TARGET_HOME)/ti/lib
	@if [ -f $(TI_filesystem_path)/lib/libxml2.a ]; then mv -f $(TI_filesystem_path)/lib/libxml2.a $(TARGET_HOME)/ti/lib/libxml2_pic.a; fi; 
	@mkdir -p $(TARGET_HOME)/ti/lib/bin/
	@if [ -f $(TI_filesystem_path)/bin/xml2-config ]; then mv -f $(TI_filesystem_path)/bin/xml2-config $(TARGET_HOME)/ti/lib/bin/xml2-config; fi;
	@if [ -f $(TI_filesystem_path)/bin/xmlcatalog ]; then rm -f $(TI_filesystem_path)/bin/xmlcatalog; fi;
	@if [ -f $(TI_filesystem_path)/bin/xmllint ]; then rm -f $(TI_filesystem_path)/bin/xmllint; fi;
	@if [ -f $(TI_filesystem_path)/bin/xmlwf ]; then rm -f $(TI_filesystem_path)/bin/xmlwf; fi;

clean_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) uninstall -i; fi

