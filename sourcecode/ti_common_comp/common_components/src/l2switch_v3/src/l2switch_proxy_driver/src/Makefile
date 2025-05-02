#################################################################################
##                                                                             ##
## GPL LICENSE SUMMARY                                                         ##
##                                                                             ##                                                                                                                                                      
## Copyright(c) 2014 Intel Corporation.                                        ## 
##                                                                             ##                                                                          
## This program is free software; you can redistribute it and/or modify        ##                                                                           
## it under the terms of version 2 of the GNU General Public License as        ##                                                                          
## published by the Free Software Foundation.                                  ##                                                                                                                                                      
##                                                                             ## 
## This program is distributed in the hope that it will be useful, but         ##                                                                            
## WITHOUT ANY WARRANTY; without even the implied warranty of                  ##                                                                           
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU           ##                                                                            
## General Public License for more details.                                    ## 
##                                                                             ##                                                                                                                                                      
## You should have received a copy of the GNU General Public License           ##                                                                            
## along with this program; if not, write to the Free Software                 ##                                                                           
## Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.  ## 
##                                                                             ## 
## The full GNU General Public License is included in this distribution        ##                                                                           
## in the file called LICENSE.GPL.                                             ## 
##                                                                             ##                                                                                                                                                        
## Contact Information:                                                        ##                                                                            
##   Intel Corporation                                                         ##                                                                            
##   2200 Mission College Blvd.                                                ##                                                                          
##   Santa Clara, CA  97052                                                    ## 
##                                                                             ## 
################################################################################# 

TARGET := l2switch_proxy_driver

# Required definitions
ifndef TARGET_HOME
TARGET_HOME := /vobs/ua_sdk
endif

ifndef KERNELRELEASE
# Include TI build definition file (optional) only in non-kernel compilation mode
-include $(TARGET_HOME)/TI_Build.make
endif

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
	-I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/puma6 \
	-I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/generic \
	-I$(TI_include)

export EXTRA_CFLAGS += $(INCLUDES)

# Installation module directory
export INSTALL_MOD_DIR := /drivers/net

# Extra symbol versions (External symbols used by this module)
export KBUILD_EXTSYMVERS :=

# Filesystem path
export INSTALL_MOD_PATH=$(TI_filesystem_path)

# Kernel module compilation
ifneq ($(KERNELRELEASE),)

# Target object
obj-m += $(TARGET).o


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
	@install --preserve-timestamps -m 0644 ../include/l2switch_proxy_driver.h $(TI_include)

build_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules

install_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules_install
	
uninstall_rule:
	@find $(TI_filesystem_path)/lib/modules/ -name $(TARGET).ko | xargs rm -rf
	

clean_rule:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` clean
	@rm -f Module.symvers
endif
