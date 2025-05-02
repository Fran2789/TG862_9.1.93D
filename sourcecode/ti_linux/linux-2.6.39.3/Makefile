##*******************************************************************************
#*+--------------------------------------------------------------------------+**
#*|                            ****                                          |**
#*|                            ****                                          |**
#*|                            ******o***                                    |**
#*|                      ********_///_****                                   |**
#*|                      ***** /_//_/ ****                                   |**
#*|                       ** ** (__/ ****                                    |**
#*|                           *********                                      |**
#*|                            ****                                          |**
#*|                            ***                                           |**
#*|                                                                          |**
#*|         Copyright (c) 1998-2008 Texas Instruments Incorporated           |**
#*|                        ALL RIGHTS RESERVED                               |**
#*|                                                                          |**
#*| Permission is hereby granted to licensees of Texas Instruments           |**
#*| Incorporated (TI) products to use this computer program for the sole     |**
#*| purpose of implementing a licensee product based on TI products.         |**
#*| No other rights to reproduce, use, or disseminate this computer          |**
#*| program, whether in part or in whole, are granted.                       |**
#*|                                                                          |**
#*| TI makes no representation or warranties with respect to the             |**
#*| performance of this computer program, and specifically disclaims         |**
#*| any responsibility for any damages, special or consequential,            |**
#*| connected with the use of this program.                                  |**
#*|                                                                          |**
#*+--------------------------------------------------------------------------+**
#*******************************************************************************

# ******************************************************************************
# * FILE PURPOSE:    PSP Linux package  Makefile
# ******************************************************************************
# * FILE NAME:       Makefile
# *
# * DESCRIPTION:     PSP Linux package Makefile
# *
# * (C) Copyright 2008, Texas Instruments, Inc
# *****************************************************************************/

include $(TI_BUILD_ROOT)/TI_Build.make
-include $(TI_BUILD_ROOT)/.config

#TI_IMAGES_ROOT:=$(TI_images_path)
TI_IMAGES_ROOT:=$(TI_BUILD_ROOT)/build/$(TI_PRODUCT)/images

.PHONY: build config install uninstall clean distclean package

LINUXPREFIX  := $(shell pwd)
CP=/bin/cp
RM=/bin/rm

export LINUXPREFIX DEBUG CP RM

CROSS_FLAGS=CROSS_COMPILE=$(CROSS)

src/.config: $(TI_BUILD_ROOT)/.config
	@$(CP) -f $(TI_BUILD_ROOT)/.config src/
	@echo $(CROSS) > src/.mvl_cross_compile
	$(MAKE) -C src oldconfig ARCH=arm CROSS_COMPILE=$(CROSS)
install: .compiled
	@$(CP) -f src/arch/arm/boot/zImage $(TI_IMAGES_ROOT)/Image.bin
	@$(MAKE) -C src modules_install INSTALL_MOD_PATH=$(TI_filesystem_path) ARCH=arm


build:config
	$(MAKE) -C src ARCH=arm zImage modules SEVENZIP=$(TI_tools_path)/bin/7zip $(CROSS_FLAGS) V=1 
	touch .compiled

config: src/.config

uninstall:
	@$(RM) -f $(TI_IMAGES_ROOT)/Image.bin
	@$(RM) -f .compiled

clean:uninstall
	-@$(CP) -fv $(TI_BUILD_ROOT)/.config src/
	@$(MAKE) -C src clean 

distclean:clean
	@$(MAKE) -C src distclean

package:
	@$(TI_BUILD_ROOT)/tools/scripts/mkpkg -s -d $(LINUXPREFIX) -o $(LINUXPREFIX)

