################################################################################
# 
# Copyright(c) 2007 Texas Instruments, Inc. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation; either version 2 of the License, or (at your option) 
# any later version.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT 
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
# more details.
# 
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 
# Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# 
# The full GNU General Public License is included in this distribution in the
# file called LICENSE.
# 
################################################################################

# Target module name
TARGET := docsis_fltr_class

# Required definitions
ifndef TARGET_HOME
TARGET_HOME := /vobs/ua_sdk
endif

ifndef KERNELRELEASE
# Include TI build definition file (optional) only in non-kernel compilation mode
-include $(TARGET_HOME)/TI_Build.make
endif

#---- Hitron add: enable compile flag ----#
-include $(TARGET_HOME)/.config
#------------- end of Hitron add ---------#

# Kernel directory
ifndef KERNEL_DIR
KERNEL_DIR := $(TARGET_HOME)/kernel/ti/linux-2.6.39.3/src
endif

# File system path, where to install the module
ifndef TI_filesystem_path
TI_filesystem_path := $(TARGET_HOME)/build/dsdk/fs
endif

# TI include export directory
ifndef TI_include
TI_include := $(TARGET_HOME)/ti/include
endif

# Include directories
INCLUDES := \
    -I$(PWD)/../include \
     -I$(PWD)/../../../docsis_pp/dpp_dlm/include \
     -I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/puma5 \
     -I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/generic \
     -I$(TI_include)

ifeq ($(DOCSIS_SOC), PUMA6)
INCLUDES +=	-I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/puma6
else
INCLUDES +=	-I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/puma5
endif

export EXTRA_CFLAGS += $(INCLUDES)

# Module extra compilation flags
ifeq ($(DFILTER_DEBUG),y)
EXTRA_CFLAGS += -DDFILTER_DEBUG
endif
ifeq ($(DCLASS_DEBUG),y)
EXTRA_CFLAGS += -DDCLASS_DEBUG
endif
# DSG
ifeq ($(CONFIG_DSG),y)
EXTRA_CFLAGS += -DDSG
endif

# Installation module directory
export INSTALL_MOD_DIR := /drivers/net

# Extra symbol versions (External symbols used by this module)
export KBUILD_EXTSYMVERS := $(CURDIR)/../../../docsis_bridge/dbridge_dlm/src/

# Filesystem path
export INSTALL_MOD_PATH=$(TI_filesystem_path)

# Kernel module compilation
ifneq ($(KERNELRELEASE),)

obj-m += $(TARGET).o

# Target objects
$(TARGET)-objs := \
	dfilter.o dfltr_class_ctrl.o qos_classifier.o dfltr_class_utils.o us_phs_verify.o l2vpn.o

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
	@install --preserve-timestamps -m 0644 ../include/dfltr_class_utils.h $(TI_include)
	@install --preserve-timestamps -m 0644 ../include/dfilter.h $(TI_include)
	@install --preserve-timestamps -m 0644 ../include/qos_classifier.h $(TI_include)
	
build_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules

install_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules_install
	@install -m 755 dfltr_class_init.sh $(TI_filesystem_path)/usr/sbin

uninstall_rule:
	@find $(TI_filesystem_path)/lib/modules/ -name $(TARGET).ko | xargs rm -rf
	@rm -f $(TI_filesystem_path)/usr/sbin/dfltr_class_init.sh
	@rm -f $(TI_include)/dfltr_class_utils.h
	@rm -f $(TI_include)/dfilter.h
	@rm -f $(TI_include)/qos_classifier.h

clean_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm clean
	@rm -f Module.symvers

endif

