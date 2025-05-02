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
#
.PHONY: dummy all config build install uninstall clean distclean package

#
# Include the configuration and definition files
#
-include $(TARGET_HOME)/.config
include $(TARGET_HOME)/TI_Build.make
export BUILD_POST

# Change host FS directory
ifeq ($(CONFIG_TI_SEPARATE_FS_ENABLE),y)
    ifeq ($(CONFIG_TI_GW_SEPARATE_FS_ENABLE),y)
        # Remove enclosing "" from GW_FS_NAME string
        export TI_filesystem_path := $(TI_filesystem_basepath)$(subst ",,$(CONFIG_TI_GW_SEPARATE_FS_NAME))
	export GW_TARGET_FS_ROOT = $(CONFIG_TI_SEPARATE_FS_ROOT_NAME)$(CONFIG_TI_GW_SEPARATE_FS_NAME)
    endif
endif

#
# Build directories
#
# ARRIS MOD BEGIN
build-$(CONFIG_TI_GW_SNMP_OFF)  += src/snmp/src/mibgen
# build-$(CONFIG_TI_GW_SNMP)  += src/vendor/src/arris_gwsnmp/mibgen
# ARRIS MOD END
build-y += $(filter $(CURDIR)/%,$(shell find $(CURDIR) -maxdepth 1 -type d|grep src|sort))

#
# General definitions
#
BUILD_DIRS-$(CONFIG_TI_GW) := $(build-y)
CLEAN_DIRS := $(build-y)
export GW_PREFIX  := $(shell pwd)
export COMPONENT_PREFIX := $(GW_PREFIX)
export COMPONENT_INCLUDES := $(patsubst %,-I%, $(shell find  $(COMPONENT_PREFIX) -name include -type d 2> /dev/null))

#
# Compile time macros
#
config-list := $(patsubst %, _config_%, $(BUILD_DIRS-y))
build-list := $(patsubst %, _build_%, $(build-y))
install-list := $(patsubst %, _install_%, $(BUILD_DIRS-y))
uninstall-list := $(patsubst %, _uninstall_%, $(CLEAN_DIRS))
clean-list := $(patsubst %, _clean_%, $(CLEAN_DIRS))
distclean-list := $(patsubst %, _distclean_%, $(CLEAN_DIRS))
package-list := $(patsubst %, _package_%, $(filter-out $(CURDIR)/src%,$(build-y)))

#
# GW Targets that are supported (According to UA spec)
#
all: build install

dummy:

build: dummy $(build-list)
	@find ./build \( -name '*.[ao]' -o -name '*.so' -o -name core -o -name '*~' -o -name '.*~' -o -name '.#*' \) -follow -type f -print 2> /dev/null | xargs rm -f

install: dummy $(install-list)

uninstall: dummy $(uninstall-list)

clean: dummy $(clean-list)
	@find . \( -name core -o -name '*~' -o -name '.*~' -o -name '.#*' \) -follow -type f -print 2> /dev/null | xargs rm -f

distclean: dummy $(distclean-list)
	@find . \( -name core -o -name '*~' -o -name '.*~' -o -name '.#*' \) -follow -type f -print 2> /dev/null | xargs rm -f

package: $(package-list)
	$(TARGET_HOME)/tools/scripts/mkpkg -d $(COMPONENT_PREFIX) -o $(COMPONENT_PREFIX) 

config: dummy $(config-list)

#
# Generic internal targets
#
ifdef BUILD_DIRS-y
$(config-list) : dummy
	@ $(MAKE) -C $(patsubst _config_%,%,$@) config

$(build-list) : dummy
	@ $(MAKE) -C $(patsubst _build_%,%,$@) build

$(install-list) : dummy
	@ $(MAKE) -C $(patsubst _install_%,%,$@) install
endif

ifdef CLEAN_DIRS
$(uninstall-list) : dummy
	@ $(MAKE) -C $(patsubst _uninstall_%,%,$@) uninstall

$(clean-list) : dummy
	@ $(MAKE) -C $(patsubst _clean_%,%,$@) clean

$(distclean-list) : dummy
	@ $(MAKE) -C $(patsubst _distclean_%,%,$@) distclean
endif

$(package-list) : dummy
	@ $(MAKE) -C $(patsubst _package_%,%,$@) package

