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
##|         Copyright (c) 1998-2007 Texas Instruments Incorporated           |##
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

.PHONY: dummy all config build install uninstall clean distclean package

#
# Include the configuration and definition files
#
-include $(TARGET_HOME)/.config
include $(TARGET_HOME)/TI_Build.make

#
# Build directories
#
ifneq ($(BUILD_POST),y)
#build-y += $(CURDIR)/src/common/management/snmp/src/mibgen
endif
build-y += $(filter $(CURDIR)/%,$(shell find $(CURDIR) -maxdepth 1 -type d|grep src|sort))

#
# General definitions
#
BUILD_DIRS-$(CONFIG_TI_DOCSIS) := $(build-y)
CLEAN_DIRS := $(shell set t=""; for i in "$(BUILD_DIRS-y)"; do t="$$i $$t"; done; echo "$$t")
export DOCSIS_PREFIX  := $(shell pwd)
export CC_PREFIX := $(TARGET_HOME)/ti/common_components
export NETDK_PREFIX := $(TARGET_HOME)/ti/netdk
export DOCSIS_SHARED_LIBS_PREFIX := $(DOCSIS_PREFIX)/build
export TI_INC_PREFIX := $(TARGET_HOME)/ti/include
#
# Compile time macros
#
config-list := $(patsubst %, _config_%, $(BUILD_DIRS-y))
build-list := $(patsubst %, _build_%, $(build-y))
install-list := $(patsubst %, _install_%, $(BUILD_DIRS-y))
uninstall-list := $(patsubst %, _uninstall_%, $(CLEAN_DIRS))
clean-list := $(patsubst %, _clean_%, $(CLEAN_DIRS))
distclean-list := $(patsubst %, _distclean_%, $(CLEAN_DIRS))

#
# DOCSIS Targets that are supported (According to UA spec)
#


all: build install

dummy:

build: dummy $(build-list)

install: dummy $(install-list)

uninstall: dummy $(uninstall-list)

clean: dummy $(clean-list)
	@find ./build \( -name '*.[oas]' -o -name core -o -name '*~' -o -name '.*~' -o -name '.#*' -o -name '*.so' \) -follow -type f -print | xargs rm -f
	@rm -f $(TI_include)/docsis_pcd.h

distclean: dummy $(distclean-list)
	@find ./build \( -name '*.[oas]' -o -name core -o -name '*~' -o -name '.*~' -o -name '.#*' -o -name '*.so' \) -follow -type f -print | xargs rm -f
	@rm -f $(TI_include)/docsis_pcd.h

package:
	@ $(MAKE) -C $(filter-out $(CURDIR)/s%,$(patsubst _build_%,%,$(build-list))) package
	$(TARGET_HOME)/tools/scripts/mkpkg -d $(DOCSIS_PREFIX) -o $(DOCSIS_PREFIX)

config: config_pcd $(config-list)

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
else
$(build-list) : dummy
endif

ifdef CLEAN_DIRS
$(uninstall-list) : dummy
	@ $(MAKE) -C $(patsubst _uninstall_%,%,$@) uninstall

$(clean-list) : dummy
	@ $(MAKE) -C $(patsubst _clean_%,%,$@) clean

$(distclean-list) : dummy
	@ $(MAKE) -C $(patsubst _distclean_%,%,$@) distclean
endif

#
# Target for PCD header file generation
#
ifdef CONFIG_TI_PCD
$(TI_include)/docsis_pcd.h: $(TARGET_HOME)/ti/docsis/src/startup/src/docsis.pcd
	@if [ -f $(TARGET_HOME)/ti/common_components/bin/pcdparser ]; then $(TARGET_HOME)/ti/common_components/bin/pcdparser -f $(TARGET_HOME)/ti/docsis/src/startup/src/docsis.pcd -o $(TI_include)/docsis_pcd.h; fi

# ARRIS CHANGE - Create docsis_pcd.h in arris_libs config and not here since arris_libs needs this file. You'd
# think this would be built in ti docsis during initial config but pcdparser isn't built yet so it doesn't run
# till the second config right before the ti docsis build.  So moving it to arris_libs instead.
config_pcd:

#config_pcd: $(TI_include)/docsis_pcd.h
# END ARRIS CHANGE
else
config_pcd:
endif

