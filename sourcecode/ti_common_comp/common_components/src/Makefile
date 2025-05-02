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
#ARRIS MOD START
include $(TARGET_HOME)/.config
#ARRIs MOD END
include $(COMCOMP_PREFIX)/build/common_components_config.make

#
# Modules directories to be built
#
# Must be near the beginning since H files are installed using BUILD_PRE
# Also this has no dependencies on anything else
subdir-$(CONFIG_TI_STATEMACHINE) += sme/src
#
subdir-$(CONFIG_TI_PACKET_PROCESSOR) += pp_fw
ifeq ($(DOCSIS_SOC), PUMA6)
subdir-$(CONFIG_TI_PACKET_PROCESSOR) += pp_fw/ppd_driver 
endif
subdir-$(CONFIG_TI_SHMDB)        += shmdb/src
subdir-$(CONFIG_TI_ICC)          += icc/src
subdir-$(CONFIG_TI_LOGGER)       += logger/src
subdir-$(CONFIG_TI_TLVPARSER)    += tlv_parser/src
ifeq ($(DOCSIS_SOC), PUMA6)
subdir-y                         += sync_app_np_reboot/src
endif
subdir-y                         += tlp_lib/src
subdir-$(CONFIG_TI_PCD)          += pcd/src
subdir-$(CONFIG_TI_NVM)          += nvram_mngr/src
# ARRIS MOD BEGIN - Allow using envoy source
subdir-$(CONFIG_TI_SNMP_AGENT)   += $(ENVOY_DIR)/src
# ARRIS MOD END
subdir-$(CONFIG_TI_SNMP_AGENT)   += ti_snmp/src
subdir-$(CONFIG_TI_EVENT_MGR)    += event_mngr/src
subdir-$(CONFIG_TI_NET_STATS)    += net_stats/src
subdir-$(CONFIG_TI_NETUTILS)     += netutils/src
subdir-$(CONFIG_TI_GPTIMER)      += gptimer/src
subdir-$(CONFIG_TI_ENVUTILS)     += envutils/src
subdir-$(CONFIG_TI_COMMONUTILS)	 += commonutils/src
subdir-$(CONFIG_TI_GIM)          += gim/src
subdir-$(CONFIG_INTEL_EXPAT)     += expat/src
subdir-$(CONFIG_INTEL_LUA)       += lua/src
subdir-$(CONFIG_INTEL_LUAEXT)    += lua-ext/src
subdir-y                         += startup/src
subdir-$(CONFIG_COMMON_COMPONENTS_IMAGE_SIGNATURE) += image_signature/src
subdir-$(CONFIG_COMMON_LIBELF)   += libelf/src
subdir-$(CONFIG_SYSTEM_L2SWITCH) += l2switch_v3/src
#subdir-$(CONFIG_SYSTEM_L2SWITCH) += l2switch_mngr/src

# For L2SW need to compile external switch init
# ARRIS MOD START - compile ext switch if EXT_SWITCH OR L2_SWITCH defined
# subdir-$(CONFIG_SYSTEM_L2SWITCH) += ext_switch_manager/src
ifeq ($(CONFIG_SYSTEM_L2SWITCH),y)
subdir-y                         += ext_switch_manager/src
else 
ifeq ($(CONFIG_SYSTEM_EXTERNALSWITCH),y)
subdir-y                         += ext_switch_manager/src
endif
endif
# ARRIS MOD END

#UNIHAN ADD - START
subdir-y                         += client_list/file_utils
#UNIHAN ADD - END
subdir-$(CONFIG_SYSTEM_MOCA)     += moca/src
subdir-$(CONFIG_SYSTEM_MOCA)     += bootload/src
subdir-$(CONFIG_TI_LSD)          += lsd/src
subdir-$(CONFIG_TI_SWT)          += swt/src
subdir-y                         += wlan/src
subdir-$(CONFIG_TI_INIPARSER)    += iniparser/src
subdir-$(CONFIG_SOAP_RPC_WIFI_CFG_CLIENT) += wifi_rpc_client/src
subdir-$(CONFIG_SYSTEM_RPC_MANAGEMENT) += rpc_management/src
#ARRIS ADD - need this rule to build dummy executable for TM1602
subdir-$(CONFIG_VENDOR_ARRIS_NO_ATOM) += rpc_management/src/rpc_if_config
subdir-$(CONFIG_VENDOR_ARRIS_NO_ATOM) += ext_switch_manager/src/ext_switch_init/src
# END ARRIS
subdir-$(CONFIG_RPC_WLAN_CFG_CLIENT)   += rpc_wlan_config/src
# Due to PCD limitations, the manager always exists
subdir-$(CONFIG_INTEL_ETH_PHY)   += ext_phy_manager/src
# ARRIS remove to save flash memory
# subdir-y                         += sw_upgrade_box/src
subdir-y                         += production_db/src
subdir-y                         += semops/src

#ARRIS ADD - Start
subdir-$(CONFIG_RPC_WLAN_CFG_CLIENT)   += arris_rpc/wlan
subdir-$(CONFIG_RPC_WLAN_CFG_CLIENT)   += arris_rpc/wlan_server
ifeq ($(CONFIG_MACH_PUMA5),y)
ifeq ($(INCLUDE_ARRIS_GW),y)
subdir-y                               += sl2cp_lite/src/common
ifeq ($(INCLUDE_ARRIS_TG852),y)
subdir-y                               += sl2cp_lite/src/app
endif
endif
endif

#ARRIS ADD - End
# Anything included in ticc most probably must appear before this line...
subdir-$(CONFIG_TI_COMMON_COMPONENTS_TICC) += ticc/src
subdir-$(CONFIG_TI_STATEMACHINE) += ti_sme/src
ifeq ($(DOCSIS_SOC), PUMA6)
subdir-$(CONFIG_TI_BBU)          += bbu6/src
else
subdir-$(CONFIG_TI_BBU)          += bbu/src
endif
subdir-$(CONFIG_TI_SCHED_UTIL)   += sched/src
subdir-$(CONFIG_TI_WATCHDOG_RT)  += watchdog/src
subdir-$(CONFIG_TI_CLI)          += cli/src
ifeq ($(CONFIG_TI_DOCSIS),y)
subdir-$(CONFIG_SYSTEM_APPPSM)   += psm/src
subdir-$(CONFIG_SYSTEM_LPCM)     += lpcm/src
endif
ifeq ($(BUILD_POST),y)
subdir-$(CONFIG_TI_MMC_UTIL)     += mmc/src
subdir-$(CONFIG_TI_ENVUTILS)     += env/src
subdir-y                         += cc_init_once/src
#ARRIS MOD START
#need to build lighttpd for cm  only builds
ifneq ($(CONFIG_VENDOR_INTEL_GATEWAY),y)
subdir-y += libz/src
subdir-y += lighttpd/src
endif
#ARRIS MOD END  
ifeq ($(DOCSIS_SOC), PUMA6)
subdir-$(CONFIG_TI_BBU)          += bbu6/src/bbu_driver/src
else
subdir-$(CONFIG_TI_BBU)          += bbu/src/bbu_driver/src
endif
#subdir-$(CONFIG_TI_DMALLOC_UTIL) += dmalloc/src // ARRIS remove
subdir-$(CONFIG_TI_EXMAP_UTIL)   += exmap/src
subdir-$(CONFIG_TI_IOSTAT_UTIL)  += iostat/src
subdir-$(CONFIG_TI_PORTMAP_UTIL) += portmap/src
subdir-$(CONFIG_TI_ETHTOOL_UTIL) += ethtool/src
subdir-$(CONFIG_TI_LIBPCAP)      += libpcap/src
subdir-$(CONFIG_TI_TCPDUMP_UTIL) += tcpdump/src
subdir-$(CONFIG_INTEL_TRACE_CMD) += trace-cmd/src
else
endif
subdir-y                         += lock_mem/src
# ARRIS MERGE ADD ifdef
ifdef CONFIG_SYSTEM_MOCA
subdir-y                         += moca/src
endif
ifneq ($(CC_BUILDONLY),)
    # Build only targets on the list
    subdir-y = $(foreach s,$(CC_BUILDONLY),$(s)/src )
endif

# ARRIS ADD for OG1600
subdir-$(CONFIG_VENDOR_GPS)      += nmealib-0.5.3/src
subdir-$(CONFIG_VENDOR_GPS)      += gps/src

subdir-$(CONFIG_TI_DMALLOC_UTIL) += dmalloc/src # ARRIS MOVE 
#
# Targets that are supported according to UA spec,
# Please do not change anything below.
#

all: build install

build: first_rule

include $(COMCOMP_PREFIX)/build/rules.make

config: config-sub_dirs

install: install-sub_dirs

clean: uninstall clean-sub_dirs
	$(foreach subdir, $(subdir-y), $(shell find ./$(subdir) \( -name core -o -name '*~' -o -name '.*~' -o -name '.#*' \) -follow -type f -print | xargs rm -f ))

distclean: clean
	$(foreach subdir, $(subdir-y), $(shell find ./$(subdir) \( -name '*.d' \) -type f -print -follow | xargs rm -f ))

uninstall: uninstall-sub_dirs

build_pre:

build_post:


