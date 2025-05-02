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

.PHONY:  build clean distclean install uninstall

.NOTPARALLEL:

#
# Include the configuration file and build file
#
include $(PACM_PREFIX)/build/pacm_config.make 
include $(TARGET_HOME)/TI_Build.make

# ARRIS ADD
SIP_STACK_BASE = $(TARGET_HOME)/../SIP

FS_LIB_PATH= $(TI_FS_PATH)$(TI_FS_LIB_PATH)

RV_LIB_PATH= $(SIP_STACK_BASE)/binaries/linux_MontaVista_4.0/Debug/lib
# END ARRIS ADD

#
# Modules directories to be built
#
subdir-y += common/src
subdir-y += interface/src
subdir-y += mtacontrol/src
subdir-$(CONFIG_TI_PACM_SERVICES)   += services/src
subdir-$(CONFIG_TI_PACM_SNMP)       += snmp/src
subdir-$(CONFIG_TI_PACM_SECURITY)   += security/src
subdir-$(CONFIG_TI_PACM_VENDOR_APP) += vendor/src
subdir-$(CONFIG_TI_PACM_BUILD_TEST) += test/src
subdir-y += init/src
subdir-$(CONFIG_TI_PACM_DOIM_DOCSIS_IF) += interface/src/doim/src
subdir-y += cli_menus/src
subdir-y += cli_menus/cli_plugin/src
subdir-y += logger/src
subdir-y += logger/voice_plugin/src

#
# Targets that are supported according to UA spec,
# Please do not change anything below.
#
#pacm-headers-list := $(shell ls ./include/*.h 2> /dev/null)

all: build install

# ARRIS CHANGE - ADD SIP Stack to build
build: sipstack dep first_rule
# END ARRIS CHANGE

export COMPONENT_INCLUDES := $(patsubst %,-I%, $(shell find $(PACM_PREFIX) -name include -type d)) -I$(TARGET_HOME)/ti/arris_libs/src/llist
export COMPONENT_PREFIX := $(PACM_PREFIX)

include $(PACM_PREFIX)/build/rules.make

config: config-sub_dirs
#	@install --preserve-timestamps -m 0644 $(pacm-headers-list) $(TI_include)

install: install-sub_dirs

clean: clean_sipstack uninstall clean-sub_dirs
	@find . \( -name '*.[oas]' -o -name core -o -name '*~' -o -name '.*~' -o -name '.#*' -o -name '*.so' \) -follow -type f -print | xargs rm -f

distclean: clean
	@find . \( -name '.depend' \) -type f -print -follow | xargs rm -f

uninstall: uninstall-sub_dirs
	@rm -f $(patsubst ./include/%,$(TI_include)/%,$(pacm-headers-list))

build_pre:

build_post:

# ARRIS ADD - Add SIP Stack Build
ifdef CONFIG_SIP
sipstack:
	@echo --- BUILDING SIP STACK ---
	@$(MAKE) -C $(TARGET_HOME)/../SIP libs nosamples=on MVISTA=on VER=4.0
	@find $(RV_LIB_PATH) -type f -name librv* | xargs -i install -m 0755 {} $(FS_LIB_PATH)

clean_sipstack:
	@echo --- CLEANING SIP STACK ---
	@$(MAKE) -C $(TARGET_HOME)/../SIP clean
	@find $(FS_LIB_PATH) -type f -name librv* | xargs -i rm -f {}

else
sipstack:

clean_sipstack:
endif

# END ARRIS ADD
