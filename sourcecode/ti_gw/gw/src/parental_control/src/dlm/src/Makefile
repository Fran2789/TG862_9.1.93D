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
TARGET := gw_parental_control

# Required definitions
ifndef TARGET_HOME
TARGET_HOME := /vobs/ua_sdk
endif

ifndef KERNELRELEASE
# Include TI build definition file (optional) only in non-kernel compilation mode
#-include $(TARGET_HOME)/TI_Build.make
endif

# Kernel directory
ifndef KERNEL_DIR
KERNEL_DIR := $(TARGET_HOME)/kernel/ti/linux-2.6.39.3/src
endif

# File system path, where to install the module
ifndef TI_filesystem_path
#TI_filesystem_path := $(TARGET_HOME)/build/dsdk/fs
$(error TI_filesystem_path is not defined in gw/src/parental_control/src/dlm/src/Makefile)
endif

# TI include export directory
ifndef TI_include
TI_include := $(TARGET_HOME)/ti/include
endif

# Include directories
INCLUDES := \
	-I$(PWD)/../include \
	-I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/generic \
	-I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/puma5 \
	-I$(TI_include)

export EXTRA_CFLAGS += $(INCLUDES)

# Module extra compilation flags
ifeq ($(GWPRNTC_DEBUG),y)
EXTRA_CFLAGS += -DGWPRNTC_DEBUG
endif

# Installation module directory
export INSTALL_MOD_DIR := /drivers/net

# Extra symbol versions (External symbols used by this module)
export KBUILD_EXTSYMVERS := $(PWD)/../../../../../../docsis/src/common/data_path/docsis_bridge/dbridge_dlm/src/

# Filesystem path
export INSTALL_MOD_PATH=$(TI_filesystem_path)

# Kernel module compilation
ifneq ($(KERNELRELEASE),)

obj-m += $(TARGET).o

# Target objects
$(TARGET)-objs := \
	gw_parental_control_ctl.o gw_parental_control_db.o

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
	@install --preserve-timestamps -m 0644 ../include/gw_parental_control_ctl.h $(TI_include)
	@install --preserve-timestamps -m 0644 ../include/gw_parental_control_db.h $(TI_include)
	
build_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules

install_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules_install
	@install -m 755 gw_parental_control_init.sh $(TI_filesystem_path)/usr/sbin

uninstall_rule:
	@find $(TI_filesystem_path)/lib/modules/ -name $(TARGET).ko | xargs rm -rf
	@rm -f $(TI_filesystem_path)/usr/sbin/gw_parental_control_init.sh
	@rm -f $(TI_include)/gw_parental_control_ctl.h
	@rm -f $(TI_include)/gw_parental_control_db.h

clean_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm clean
	@rm -f Module.symvers

endif

