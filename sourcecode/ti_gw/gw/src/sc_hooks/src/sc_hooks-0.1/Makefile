#*****************************************************************************
# FILE PURPOSE:     - Gateway kernel Modification Hooks
#*****************************************************************************
# FILE NAME:     Makefile
#
# DESCRIPTION:   Builds Gateway Kernel hooks module.
#
# Copyright (c) 2013-2014, ARRIS Enterprises, LLC, All rights reserved
#
#******************************************************************************
#ifndef PATCHLEVEL
#include ../../Rules.mak
#endif 

CROSS_COMPILE = $(CROSS)

include $(TARGET_HOME)/.config

# Kernel directory
ifndef KERNEL_DIR
KERNELDIR := $(TARGET_HOME)/kernel/ti/linux-2.6.39.3/src
endif

# File system path, where to install the module
ifndef TI_filesystem_path
$(error TI_filesystem_path is not defined in gw/src/sc_hooks/src/sc_hooks-0.1/Makefile)
endif

ifeq ($(CONFIG_VENDOR_COMCAST),y)
MODULES = sc_nf_bridge_forward_hook.ko sc_hotspot_vlan_pri_hook.ko sc_gre_tx_filter_hook.ko sc_BrHttp.ko sc_daylightsaving.ko

obj-m:= sc_nf_bridge_forward_hook.o sc_hotspot_vlan_pri_hook.o sc_gre_tx_filter_hook.o sc_BrHttp.o sc_daylightsaving.o
else
MODULES = sc_nf_bridge_forward_hook.ko sc_hotspot_vlan_pri_hook.ko sc_gre_tx_filter_hook.ko sc_daylightsaving.ko

obj-m:= sc_nf_bridge_forward_hook.o sc_hotspot_vlan_pri_hook.o sc_gre_tx_filter_hook.o sc_daylightsaving.o
endif

all:modules

modules:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules

clean:
	rm -rf $(MODULES) *.o *.ko *.cmd .*.cmd

install:
	@if [ ! -d $(TI_filesystem_path)/lib/modules ]; then mkdir $(TI_filesystem_path)/lib/modules; fi
	@if [ ! -d $(TI_filesystem_path)/lib/modules/sc_hooks ]; then mkdir $(TI_filesystem_path)/lib/modules/sc_hooks; fi
	@for module in $(MODULES); do \
	  cp -f $$module $(TI_filesystem_path)/lib/modules/sc_hooks; \
	done

uninstall:
	@for module in $(MODULES); do\
	  find $(TI_filesystem_path)/lib/modules/sc_hooks -name $$module | xargs rm -f; \
	done


