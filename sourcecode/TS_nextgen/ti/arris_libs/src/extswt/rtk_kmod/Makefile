#*****************************************************************************
# FILE PURPOSE:     - Realtek switch LKM
#*****************************************************************************
# FILE NAME:     Makefile
#
# DESCRIPTION:   Builds The kernel module for the realtek switch
#
#                Author: Peter Sadrozinski
#
# Copyright 2012, ARRIS Group, Inc., All rights reserved
#
#******************************************************************************

# Target module name
TARGET := arris_switch

# ned to read config for which switch we need to define
-include $(TI_BUILD_ROOT)/.config

ifndef KERNELRELEASE
# Include TI build definition file (optional) only in non-kernel compilation mode
-include $(TARGET_HOME)/TI_Build.make
endif

# Kernel directory
ifndef KERNEL_DIR
KERNEL_DIR := $(TARGET_HOME)/kernel/ti/linux-2.6.39.3/src
endif

# TI include export directory
ifndef TI_include
TI_include := $(TARGET_HOME)/ti/include
endif

# Include directories
INCLUDES := \
        -I$(KERNEL_DIR)/include \
        -I$(KERNEL_DIR)/asm/arch-avalanche \
        -I$(KERNEL_DIR)/include/asm/arch-avalanche/generic \
        -I$(KERNEL_DIR)/include/asm/arch-avalanche/generic/puma6 \
        -I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/generic \
        -I$(TARGET_HOME)/ti/include \
        -I$(TARGET_HOME)/ti/arris_libs/src/extswt/extswt_api \
        -I$(TARGET_HOME)/ti/arris_libs/src/extswt/rtk_kmod/rtk_api

        
# RTK API configuration
ifdef CONFIG_VENDOR_ARRIS_P6MG
RTK_API_FLAGS = -DCHIP_RTL8367MB
else
ifdef CONFIG_VENDOR_ARRIS_TG1682
RTK_API_FLAGS = -DCHIP_RTL8367MB
else
RTK_API_FLAGS = -DCHIP_RTL8365MB
endif
endif

export EXTRA_CFLAGS += $(INCLUDES) $(RTK_API_FLAGS)

# Installation module directory
export INSTALL_MOD_DIR := /drivers/net

# Extra symbol versions (External symbols used by this module)
export KBUILD_EXTSYMVERS := 

# Filesystem path
export INSTALL_MOD_PATH=$(TI_filesystem_path)

# Kernel module compilation
ifneq ($(KERNELRELEASE),)

	obj-m += $(TARGET).o

	# Target objects
	$(TARGET)-objs := rtk83xx_mod.o \
			  stp.o \
			  stp_bpdu.o \
			  stp_if.o \
			  stp_timer.o \
  			  rldp.o


	# TODO : Get this working...
	# $(patsubst %.c,%.o,$(shell ls rtk_api/*.c 2> /dev/null))

	$(TARGET)-objs += 	rtk_api/rtk_api.o				\
				rtk_api/rtl8367b_asicdrv_acl.o			\
				rtk_api/rtl8367b_asicdrv.o			\
				rtk_api/rtl8367b_asicdrv_cputag.o		\
				rtk_api/rtl8367b_asicdrv_dot1x.o		\
				rtk_api/rtl8367b_asicdrv_eav.o			\
				rtk_api/rtl8367b_asicdrv_eee.o			\
				rtk_api/rtl8367b_asicdrv_fc.o			\
				rtk_api/rtl8367b_asicdrv_green.o		\
				rtk_api/rtl8367b_asicdrv_hsb.o			\
				rtk_api/rtl8367b_asicdrv_igmp.o			\
				rtk_api/rtl8367b_asicdrv_inbwctrl.o		\
				rtk_api/rtl8367b_asicdrv_interrupt.o		\
				rtk_api/rtl8367b_asicdrv_led.o			\
				rtk_api/rtl8367b_asicdrv_lut.o			\
				rtk_api/rtl8367b_asicdrv_meter.o		\
				rtk_api/rtl8367b_asicdrv_mib.o			\
				rtk_api/rtl8367b_asicdrv_mirror.o		\
				rtk_api/rtl8367b_asicdrv_misc.o			\
				rtk_api/rtl8367b_asicdrv_phy.o			\
				rtk_api/rtl8367b_asicdrv_port.o			\
				rtk_api/rtl8367b_asicdrv_portIsolation.o 	\
				rtk_api/rtl8367b_asicdrv_qos.o			\
				rtk_api/rtl8367b_asicdrv_rma.o			\
				rtk_api/rtl8367b_asicdrv_scheduling.o		\
				rtk_api/rtl8367b_asicdrv_storm.o		\
				rtk_api/rtl8367b_asicdrv_svlan.o		\
				rtk_api/rtl8367b_asicdrv_trunking.o		\
				rtk_api/rtl8367b_asicdrv_unknownMulticast.o	\
				rtk_api/rtl8367b_asicdrv_vlan.o			\
				rtk_api/rtl8367b_asicdrv_rldp.o			\
				rtk_api/smi.o

else

# Makefile targets
build: 		build_rule

config: 	config_rule

install: 	install_rule

uninstall: 	uninstall_rule

clean:		clean_rule

distclean:	uninstall clean

all: 		config build install


# UMF Rules
config_rule:
	@install --preserve-timestamps -m 0644 ./rtk_api/rtk_api.h $(TI_include)
	@install --preserve-timestamps -m 0644 ./rtk_api/rtk_types.h $(TI_include)
	@install --preserve-timestamps -m 0644 ./rtk_api/rtk_error.h $(TI_include)
build_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules

install_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules_install

uninstall_rule:
	@find $(TI_filesystem_path)/lib/modules/ -name $(TARGET).ko | xargs rm -rf
	
clean_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm clean
	@rm -f Module.symvers

endif
