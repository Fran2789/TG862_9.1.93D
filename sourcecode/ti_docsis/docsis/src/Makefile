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

#
# Include the configuration file and build file
#
include $(DOCSIS_PREFIX)/build/docsis_config.make
include $(TARGET_HOME)/TI_Build.make
# ARRIS ADD
-include $(TARGET_HOME)/.config

# Share .h files that do not adhere to the standard ?
export CFLAGS += \
	-I$(CURDIR)/common/docsis_mac/cm_status/lib_cmstatus/src \
	-I$(CURDIR)/common/docsis_mac/docsis_driver/downstream_manager/src/downstream_manager_app \
	-I$(CURDIR)/common/docsis_mac/docsis_mac_manager/cmts_registration/src \
	-I$(CURDIR)/common/docsis_mac/docsis_mac_manager/ip_provisioning/src \
	-I$(CURDIR)/common/management/snmp/src \
	-I$(CURDIR)/pform/hal_infrastructure/hal_isr \
	-I$(CURDIR)/pform/hal_services/hal_mng_q \

ifeq ($(CONFIG_DSG),y)
export CFLAGS += \
	-I$(CURDIR)/common/dsg \
	-I$(CURDIR)/common/dsg/dsg_dbs
endif
export CFLAGS += -DSTATUS_PROCESS_EXITCODE_ERROR=1

#export CFLAGS += -Werror -Wall
export CFLAGS += -Wall


ifeq ($(INCLUDE_SERCOMM_GW),y)
export CFLAGS += -DINCLUDE_SERCOMM_GW
endif

#
# Modules directories to be built
#

#
# First we build all libraries
#
ifeq ($(CONFIG_DSG),y)
# DSG Shared DBs
subdir-y += common/dsg/dsg_dbs
endif

# ARRIS ADD
# need to build arris libraries for creating the arris mibs
subdir-y += $(TARGET_HOME)/ti/arris_docsis/libs
# ARRIS ADD END

# NVRAM storage libraries
subdir-y += common/management/nvram_storage/src

#SNMP Util library
#subdir-y += common/management/snmp/src/snmp_util/src
# Utils library - system, network, nvram, sme
subdir-y += common/utils/src/

# Shared DBs

subdir-y += common/docsis_mac/channels_db
subdir-y += common/docsis_mac/cm_control_db/src
subdir-y += common/docsis_mac/docsis_cfg_db/src
subdir-y += common/docsis_mac/docsis_db/src
subdir-y += common/docsis_mac/dsid_db/src
subdir-y += common/docsis_mac/ip_db/src
subdir-y += common/docsis_mac/mdd_db/src
subdir-y += common/docsis_mac/qos/src/qos_main_db/src
subdir-y += common/docsis_mac/ucd_db/src
subdir-y += common/management/docsis_config_to_text/src
subdir-y += common/management/esafe_db/src
subdir-y += common/management/if_cfg_db/src
subdir-y += common/management/sys_cfg_db/src
subdir-y += common/docsis_mac/dpv_db/src
subdir-y += docsis_interface/src/docsis_interface_db/src
subdir-y += common/docsis_mac/energy_management/energy_management_db/src


# Consolidate DOCSIS Shared DBs
subdir-y += common/shared_dbs/src

# HAL makefile
subdir-y += pform
# DOCSIS US database
subdir-y += common/docsis_mac/us_db/src
# DOCSIS L2VPN database
subdir-y += common/docsis_mac/l2vpn/src
# Downstream and upstream manager libraries
subdir-y += common/docsis_mac/docsis_driver/downstream_manager/src/downstream_manager_lib
subdir-y += common/docsis_mac/docsis_driver/upstream_manager/src/us_manager_lib
# UCD hadler
subdir-y += common/docsis_mac/docsis_driver/ucd_manager/src
# DSID hadler
subdir-y += common/docsis_mac/docsis_driver/dsid_handler/src

# SW DL libraries
subdir-y += common/management/sw_dl/sw_dl_db/src
subdir-y += common/management/sw_dl/sw_dl_aux/src
subdir-y += common/management/sw_dl/cvc_verification/src
# ARRIS Remove to save flash memory.
# subdir-y += common/management/sw_dl/sw_dl_lib/src
#Docsis External Interface
subdir-y += docsis_interface/src

	# Dispatcher API library
	#subdir-y += common/docsis_mac/libdispatcher/src
# Range response parser
subdir-y += common/docsis_mac/docsis_driver/rng_rsp_parser/src

# Docsis Protocol Filters/QOS classifiers configuration library
subdir-y += common/data_path/docsis_fltr_class/cfg/lib/src
# QOS libraries
subdir-y += common/docsis_mac/qos/src/qos_internal_db/src
subdir-y += common/docsis_mac/qos/src

# Init library
subdir-y += startup/src/init_util/src
# Send management message API library
subdir-y += common/docsis_mac/send_mngt_msg/src
# DMG utils
subdir-y += common/docsis_mac/docsis_mac_manager/dmg_utils/src
# CMTS registration library
subdir-y += common/docsis_mac/docsis_mac_manager/cmts_registration/src
# Reinit MAC
subdir-y += common/docsis_mac/reinit_mac/src
# MDD collection
#subdir-y += common/docsis_mac/mdd_db/src
subdir-y += common/docsis_mac/docsis_driver/mdd_collect/src
#Primary Downstream Frequency Override
subdir-y += common/docsis_mac/primary_ds_freq_override_db/src
# Frequency scan
subdir-y += common/docsis_mac/docsis_driver/frequency_scan/src
subdir-y += common/docsis_mac/docsis_driver/frequency_scan/src/database
# DCC hadler
subdir-y += common/docsis_mac/docsis_driver/dcc_handler/src
# SG resolution
subdir-y += common/docsis_mac/docsis_driver/ds_sg_resolution/src
subdir-y += common/docsis_mac/docsis_driver/us_sg_resolution/src
# Channels manager
subdir-y += common/docsis_mac/docsis_driver/channels_manager/src
# Roll back
subdir-y += common/docsis_mac/roll_back/src
# Energy manager Lib
subdir-y += common/docsis_mac/energy_management/energy_management_lib/src
# DOCSIS provisioning
subdir-y += common/docsis_mac/docsis_mac_manager/provisioning/src/prov_api/src

#RCC database

# CM_STATUS
subdir-y += common/docsis_mac/cm_status/lib_cmstatus/src
# CM_CONTROL
subdir-y += common/docsis_mac/cm_control/src

# BPI libraries
subdir-y += common/docsis_mac/bpi/bpi_dbs/src
subdir-y += common/docsis_mac/bpi/bpi_crypto/src
subdir-y += common/docsis_mac/bpi/bpi_interface/src
# Docsis Bridge control libraries
subdir-y += common/data_path/docsis_bridge/dbridge_conf/src
# Docsis Packet Processor control libraries
subdir-y += common/data_path/docsis_pp/dpp_conf/src

# IP provisioning
subdir-y += common/docsis_mac/docsis_mac_manager/ip_provisioning/src/ipplib

subdir-y += common/docsis_mac/docsis_mac_manager/ip_provisioning/src

# Dispatcher API library
subdir-y += common/docsis_mac/msg_dispatcher/src/msg_dispatcher_lib


# Downstream and upstream managers
subdir-y += common/docsis_mac/docsis_driver/downstream_manager/src/downstream_manager_app
subdir-y += common/docsis_mac/docsis_driver/upstream_manager/src/us_manager_app
subdir-y += common/docsis_mac/docsis_driver/upstream_manager/src/us_manager_1q

# DPV
subdir-y += common/docsis_mac/dpv/src

# Error Sets
subdir-y += common/docsis_mac/error_sets/src

#
# Second, we build all applications
#

# Startup process
subdir-y += startup/src/init_once/src
# PHY test mode
subdir-y += common/docsis_mac/phy_test/src

#DSG makefile
ifeq ($(CONFIG_DSG),y)
subdir-y += common/dsg
subdir-y += common/dsg/dsg_app
# Docsis DSG Tunnel interface
subdir-y += common/data_path/dsg_ni/src
endif

# DOCSIS driver
subdir-y += common/docsis_mac/docsis_driver/docsis_driver_main/src
# DBC
subdir-y += common/docsis_mac/docsis_mac_manager/dbc/src
# DOCSIS manager
subdir-y += common/docsis_mac/docsis_mac_manager/docsis_mac_manager_main/src
# DOCSIS provisioning
subdir-y += common/docsis_mac/docsis_mac_manager/provisioning/src
# Dispatcher executable
subdir-y += common/docsis_mac/msg_dispatcher/src/msg_dispatcher_app
# SW DL executable
subdir-y += common/management/sw_dl/sw_dl_task/src
# ARRIS CHANGE - to save 2kB, for now compile out new Intel download code that we aren't using.
# subdir-y += common/management/sw_dl/sw_dl_app/src
# BPI executables
subdir-y += common/docsis_mac/bpi/bpi_auth/src
subdir-y += common/docsis_mac/bpi/bpi_tek/src
subdir-y += common/docsis_mac/bpi/bpi_sa_map/src
# Docsis Bridge executables
subdir-y += common/data_path/docsis_bridge/dbridge_init/src
# SNMP agent
subdir-y += common/management/snmp/src
# Event Manager
subdir-y += common/management/event_manager/src
# Energy manager 
subdir-y += common/docsis_mac/energy_management/energy_management_app/src

# I ADDED IT RMHERE
subdir-y += common/docsis_mac/docsis_mac_manager/ip_provisioning/src/plugins/common/src

subdir-y += common/all_docsis

subdir-y += common/docsis_mac/docsis_mac_manager/ip_provisioning/src/plugins

#DSG CCIF plugin makefile
ifeq ($(CONFIG_DSG),y)
ifeq ($(CONFIG_DSG_CCIF_PROXY),y)
subdir-y += common/data_path/cablecard_ni/src
subdir-y += common/dsg/ccif_dhcp_plugin
endif
endif

# CLI menu build AFTER build all_docsis lib!!!
# DOCSIS CLI
subdir-y += vendor/cli_menus/src 
# CLI and Logger plugin's build AFTER build docsis_cli lib!!!
# DOCSIS CLI plugin makefile
subdir-y += vendor/cli_menus/cli_plugin/src
# DOCSIS Logger plugin makefile
subdir-y += vendor/logger/src

# Net Utils tests
#subdir-y += common/utils/test/utils_net/
# CM_STATUS application
subdir-y += common/docsis_mac/cm_status/app_cmstatus/src
# CM_STATUS test
#subdir-y += common/docsis_mac/cm_status/test/src
# CM_CONTROL
#subdir-y += common/docsis_mac/cm_control/test/src
# IP_DB test
#subdir-y += common/docsis_mac/ip_db/test

# DOCSIS LED
subdir-y += common/management/docsis_led/src

ifdef CONFIG_ARRIS_DECT
# DOCSIS Button Keys
subdir-y += common/management/docsis_button/src/button_ko
endif

#QOS DSX SM application
subdir-y += common/docsis_mac/qos/src/qos_dsx_ta/src

# CHANNELS DB TEST
#subdir-y += common/docsis_mac/channels_db/chdb_test

# Kernel modules
subdir-$(BUILD_POST) += common/data_path/cable_ni/src # Cable network interface
subdir-$(BUILD_POST) += common/data_path/docsis_fltr_class/core/src #Docsis Protocol Filters/ QOS classifiers kernel module
subdir-$(BUILD_POST) += common/data_path/docsis_bridge/dbridge_dlm/src #Docsis Bridge kernel module
subdir-$(BUILD_POST) += common/data_path/docsis_pp/dpp_dlm/src #Docsis Packet Processor kernel module

ifneq ($(DOCSIS_BUILDONLY),)
    # Build only targets on the list
    subdir-y = $(foreach s,$(DOCSIS_BUILDONLY),$(s)/src )
endif

#
# Targets that are supported according to UA spec,
# Please do not change anything below.
#
subdirutil := $(patsubst %/src,/%_util/src,$(subdir-y))
subdirutil-y := $(join $(subdir-y),$(subdirutil))

docsis-headers-list := $(shell ls ./include/*.h 2> /dev/null)


all: prnt build install

prnt:
	echo $(subdir-y)

build: first_rule

export COMPONENT_INCLUDES := $(patsubst %,-I%, $(shell find $(DOCSIS_PREFIX) -name include -type d))

export COMPONENT_PREFIX := $(DOCSIS_PREFIX)

include $(DOCSIS_PREFIX)/build/rules.make

config: config-sub_dirs
	@install --preserve-timestamps -m 0644 $(docsis-headers-list) $(TI_include)

install: install-sub_dirs
#	@install -m 0755 $(DOCSIS_PREFIX)/tools/leds/ledcfg $(TI_FS_PATH)$(TI_sbindir)
#	@install -m 0755 $(DOCSIS_PREFIX)/tools/rfs/rfs $(TI_FS_PATH)$(TI_sbindir)
#	@install -m 0755 $(DOCSIS_PREFIX)/tools/rfs/rfs.cfg $(TI_FS_PATH)$(TI_sbindir)

clean: uninstall clean-sub_dirs

distclean: clean
	@find . \( -name '.depend' \) -type f -print -follow | xargs rm -f

uninstall: uninstall-sub_dirs
	@rm -f $(patsubst ./include/%,$(TI_include)/%,$(docsis-headers-list))

build_pre:

build_post:

