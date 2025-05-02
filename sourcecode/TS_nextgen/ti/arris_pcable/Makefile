###########################################################################
##                                                                       ##
##                AAA   RRRRRR  RRRRRR  IIIII  SSSSS                     ##
##               AAAAA  RR   RR RR   RR  III  SS                         ##
##              AA   AA RRRRRR  RRRRRR   III   SSSSS                     ##
##              AAAAAAA RR  RR  RR  RR   III       SS                    ##
##              AA   AA RR   RR RR   RR IIIII  SSSSS                     ##
##                                                                       ##
###########################################################################

###########################################################################
# FILE NAME: Makefile                                                     #
#                                                                         #
# DESCRIPTION:                                                            #
#                                                                         #
###########################################################################

.PHONY: dummy all config build install uninstall clean distclean package


#
# Include the configuration and definition files
#
-include $(TARGET_HOME)/.config
include $(TARGET_HOME)/TI_Build.make

# Build directories
#                                       
build-y += src

#
# General definitions
#
BUILD_DIRS-$(CONFIG_ARRIS_PCABLE) := $(build-y)
CLEAN_DIRS := $(build-y)

export ARRIS_PCABLE_INC_PREFIX := $(TARGET_HOME)/ti/arris_pcable/include
export ARRIS_PCABLE_DIR  := $(shell pwd)
export DOCSIS_PREFIX  := $(TARGET_HOME)/ti/docsis
export CC_PREFIX := $(TARGET_HOME)/ti/common_components
export PACM_PREFIX := $(TARGET_HOME)/ti/pacm
export MASDK_PREFIX := $(TARGET_HOME)/ti/masdk
export NETDK_PREFIX := $(TARGET_HOME)/ti/netdk
export DOCSIS_SHARED_LIBS_PREFIX := $(DOCSIS_PREFIX)/build
export TI_INC_PREFIX := $(TARGET_HOME)/ti/include
export ARRIS_DOCSIS_PREFIX := $(TARGET_HOME)/ti/arris_docsis

#
# Compile time macros
#
config-list := $(patsubst %, _config_%, $(BUILD_DIRS-y))
build-list := $(patsubst %, _build_%, $(BUILD_DIRS-y))
install-list := $(patsubst %, _install_%, $(BUILD_DIRS-y))
uninstall-list := $(patsubst %, _uninstall_%, $(CLEAN_DIRS))
clean-list := $(patsubst %, _clean_%, $(CLEAN_DIRS))
distclean-list := $(patsubst %, _distclean_%, $(CLEAN_DIRS))

#
# Compile targets
#
all: build install

dummy:

build: dummy $(build-list)

install: dummy $(install-list)

uninstall: dummy $(uninstall-list)

# Remove all objects created from compilation
clean: dummy $(clean-list)
	@find ./build \( -name '*.[oas]' -o -name core -o -name '*~' -o -name '.*~' -o -name '.#*' -o -name '*.so' \) -follow -type f -print | xargs rm -f

distclean: dummy clean $(distclean-list)
      
package:
	$(TARGET_HOME)/tools/scripts/mkpkg -d $(ARRIS_PCABLE_DIR) -o $(ARRIS_PCABLE_DIR)

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
ifeq ($(CONFIG_ARRIS_DECT),y)
	@ echo 'Installing DCX81 DECT Basestation binaries'
	@ install -m 0777 $(TARGET_HOME)/../ti_thirdparty/thirdparty/dspg/DCX81_MOD_UART.BIN $(TI_filesystem_path)$(TI_sbindir)
	@ install -m 0777 $(TARGET_HOME)/../ti_thirdparty/thirdparty/dspg/DCX81_MOD_UART.BOOTLOADER.bin $(TI_filesystem_path)$(TI_sbindir)
	@ install -m 0777 $(TARGET_HOME)/../ti_thirdparty/thirdparty/dspg/firebird/src/dspg/test/testout/cmbs_tcx $(TI_filesystem_path)$(TI_sbindir)
endif
endif

ifdef CLEAN_DIRS
$(uninstall-list) : dummy
	@ $(MAKE) -C $(patsubst _uninstall_%,%,$@) uninstall

$(clean-list) : dummy
	@ $(MAKE) -C $(patsubst _clean_%,%,$@) clean

$(distclean-list) : dummy
	@ $(MAKE) -C $(patsubst _distclean_%,%,$@) distclean
endif

