#*****************************************************************************
# FILE PURPOSE:     - Battery charger telemetry kernel space makefile
#*****************************************************************************
# FILE NAME:     Makefile
#
# DESCRIPTION:   Builds telemetry driver module.
#
#                Author: Bill Mohr
#
# Copyright 2008, ARRIS Group, Inc., All rights reserved
# Copyright 2016, ARRIS Enterprises, LLC, All rights reserved
#
#******************************************************************************

# Target module name
TARGET := tlm

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
        -I$(KERNEL_DIR)/asm/arch-avalanche \
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
	$(TARGET)-objs := charger.o charger_fw.o telemetry.o ups_manager.o

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
