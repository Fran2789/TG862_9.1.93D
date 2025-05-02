#*****************************************************************************
# FILE PURPOSE:  Battery charger telemetry kernel space makefile
#*****************************************************************************
# FILE NAME:     Makefile
#
# DESCRIPTION:   Builds the TI Battery Charger load kernel  module.
#
#                Author: Jarod Guo
#
# Copyright 2015, ARRIS Enterprises, Inc., All rights reserved
# Copyright 2016, ARRIS Enterprises, LLC,  All rights reserved
#
#******************************************************************************

# Target module name
TARGET := tibat

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
        -I$(TARGET_HOME)/ti/pacm/src/interface/src/mtani/include \
        -I$(KERNEL_DIR)/net/bridge/ti_docsis_bridge \
        -I$(KERNEL_DIR)/include \
        -I$(KERNEL_DIR)/include/asm/arch-avalanche \
        -I$(KERNEL_DIR)/include/asm/arch-avalanche/generic \
        -I$(KERNEL_DIR)/include/asm/arch-avalanche/generic/puma5 \
        -I$(TARGET_HOME)/ti/include

export EXTRA_CFLAGS += $(INCLUDES)

#export EXTRA_CFLAGS += -Werror

# Module extra compilation flags
EXTRA_CFLAGS += -DINIT_EXT_DMA

# Installation module directory
export INSTALL_MOD_DIR := /drivers/tlm

# Extra symbol versions (External symbols used by this module)
export KBUILD_EXTSYMVERS := 

# Filesystem path
export INSTALL_MOD_PATH=$(TI_filesystem_path)

# Kernel module compilation
ifneq ($(KERNELRELEASE),)

	obj-m += $(TARGET).o

	# Target objects
	$(TARGET)-objs := tibat_char.o jtagfunc430.o lowlevelfunc430.o

else

unexport LDFLAGS
unexport CFLAGS


STEC2BIN=$(TARGET_HOME)/tools/bin/srec2bin
INSTALL_BIN_DIR=/lib/modules/2.6.39.3/drivers/tlm
SREC_CAT=$(TARGET_HOME)/tools/bin/srec_cat
SREC_DIR=$(TARGET_HOME)/ti/arris_pcable/src/tlm/tibat_ko/srec
SREC_BIN_DIR=$(TI_filesystem_path)/$(INSTALL_BIN_DIR)
modemTypes=TM3202G TG1682G TG1652G

define intel2bin
if [ -f  $(SREC_DIR)/$1_$2.hex ]; then $(SREC_CAT) $(SREC_DIR)/$1_$2.hex  -Intel -o  $(SREC_BIN_DIR)/$1_$2_TI.bin_temp -Binary; \
dd if=$(SREC_BIN_DIR)/$1_$2_TI.bin_temp of=$(SREC_BIN_DIR)/$1_$2_TI.bin bs=1 skip=50176;\
rm $(SREC_BIN_DIR)/$1_$2_TI.bin_temp;\
fi
endef

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

build_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules

install_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules_install
	# if intel hex exists, intel version overrides moto version.
	@for modem in $(modemTypes); do $(call intel2bin,$${modem},AP); done
	@for modem in $(modemTypes); do $(call intel2bin,$${modem},ST); done


uninstall_rule:
	@find $(TI_filesystem_path)/lib/modules/ -name $(TARGET).ko | xargs rm -rf
	
clean_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm clean
	@rm -f Module.symvers
	rm -rf $(TI_filesystem_path)/$(INSTALL_BIN_DIR)/TM3202G_AP.bin
	rm -rf $(TI_filesystem_path)/$(INSTALL_BIN_DIR)/TM3202G_ST.bin
endif

