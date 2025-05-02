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

.PHONY:  build clean distclean install uninstall

.NOTPARALLEL:

export UMF_PRINT_DIRECTORY:=y
export CONFIG_VENDOR_ARRIS_P6MG #  ARRIS ADD
export CONFIG_VENDOR_ARRIS_TG1682 #  ARRIS ADD

GW_PCD_H = $(TI_include)/gwsdk_gw_pcd.h
GW_PCD_SRC = $(TARGET_HOME)/ti/gw/src/gw_control/src/etc/gwsdk_gw.pcd
GW_PCD_DEST = $(TI_filesystem_path)$(TI_scriptsdir)/gwsdk_gw.pcd
GW_PCD_DEST2 = $(TI_filesystem_basefs_path)$(TI_scriptsdir)/gwsdk_gw.pcd

#
# Include the configuration file and build file
#
-include $(TARGET_HOME)/.config
include $(COMPONENT_PREFIX)/build/gw_config.make

#
# Modules directories to be built
#

# UNIHAN ADD START
ifneq ($(GW_SKIP_3RD),y)
subdir-$(CONFIG_TI_LIBXML2)             += libxml2/src
endif
# UNIHAN ADD END

#SERCOMM ADD
subdir-y                            += arp_daemon/src
ifneq ($(CONFIG_VENDOR_ARRIS_CM_TR69),y)
subdir-y                            += adminactive/src
subdir-y                            += bssactived/src
subdir-y                            += daylightsaving/src
subdir-y                            += udns/src
subdir-y                            += ipp2p/src
subdir-y                            += fw_modules/src
subdir-y                            += ndisc6/src
subdir-y                            += gpl_modules/src
endif

#SERCOMM ADD END

#
# Init
#
#ARRIS chagne to build wirless tool for PUMA5 WIFI
ifeq (${CONFIG_MACH_PUMA5},y)
subdir-y         += wireless_tools/src
subdir-y         += ../../../ti_thirdparty/thirdparty/mediatek/RT3352
else
subdir-$(CONFIG_TI_WLAN_ENABLE)         += wireless_tools/src
endif
#ARRIS change end
ifneq ($(CONFIG_VENDOR_ARRIS_CM_TR69),y)
subdir-y                            += gw_control/src
subdir-y                            += init
subdir-y                            += parental_control/src
endif
#ARRIS change to not build GW_SNMP
subdir-$(CONFIG_TI_GW_SNMP_OFF)     += snmp/src
#ARRIS change end

ifneq ($(GW_SKIP_3RD),y)
    #
    # Open source software libraries
    #move libz to arris libs
    #subdir-$(CONFIG_TI_LIBZ)                += libz/src
# UNIHAN DEL START
#subdir-$(CONFIG_TI_LIBXML2)             += libxml2/src
# UNIHAN DEL END
    subdir-$(CONFIG_TI_LIBDAEMON)           += libdaemon/src
    
    #
    # Third-party (Open source) software modules
    #
    subdir-$(CONFIG_TI_AVAHI)               += avahi/src
    subdir-$(CONFIG_TI_ARNO_IPTABLES)       += arno-iptables-firewall/src
    subdir-$(CONFIG_TI_IPTABLES)            += iptables/src

ifneq ($(BUILD_POST),y)
    subdir-$(CONFIG_TI_IPTABLES)            += netfilter_netlink/src
    subdir-$(CONFIG_TI_IPTABLES)            += netfilter_conntrack/src

ifeq ($(CONFIG_TI_HUNTER_CREEK),y)
    subdir-$(CONFIG_TI_IPTABLES)            += libmnl/src
    subdir-$(CONFIG_TI_IPTABLES)            += netfilter_queue/src    
endif

endif
    subdir-$(CONFIG_TI_IPROUTE2)            += iproute2/src
    subdir-$(CONFIG_TI_EBTABLES)            += ebtables/src
    
# ARRIS ADD BEGIN vendor specific targets that must be built after iproute and netfilter
#
# Vendor specific software modules
#
ifneq ($(CONFIG_VENDOR_ARRIS_CM_TR69),y)
subdir-y                            += vendor/src
subdir-y                            += libmnl/src
# ARRIS ADD END

# SERCOMM ADD: Plugin so-s that need to link with other so-s
subdir-y                            += gw_control/src/dhcpc_plugin_v4/src

# UNIHAN ADD : link after gw_control, because of dhcpc_plugin_v6 need gwdb.so
subdir-y                            += gw_control/src/dhcpc_plugin_v6/src
# # UNIHAN ADD END
endif
    subdir-$(CONFIG_TI_QUAGGA)              += quagga/src
    subdir-$(CONFIG_TI_DNS_MASQ)            += dnsmasq/src
#ignore php module for TW/NA 
ifeq ($(CONFIG_VENDOR_COMCAST),y)
    subdir-$(CONFIG_TI_PHP)                 += php/src
endif
    subdir-$(CONFIG_TI_LIGHT_HTTP_SERVER)   += lighttpd/src
    subdir-$(CONFIG_TI_IGMP_PROXY)          += igmpproxy/src
    subdir-$(CONFIG_TI_MLD_PROXY)           += mldproxy/src
    subdir-$(CONFIG_TI_SSMTP)               += ssmtp/src
    subdir-$(CONFIG_TI_MINIUPNPD)           += miniupnpd/src
    subdir-$(CONFIG_VENDOR_ARRIS_HNE_AUTOCFG) += minissdpd/src
    subdir-$(CONFIG_TI_DHCPV6S_DIBBLER)     += dibbler/src
    subdir-$(CONFIG_TI_TR69)                += tr69/src
#SERCOMM ADD
ifneq ($(CONFIG_VENDOR_ARRIS_CM_TR69),y)
    subdir-y                            += sc_compress/src
    subdir-$(CONFIG_ARRIS_ICTRL)        += HNAPSetBridgeConn/src
endif
#SERCOMM ADD END
else
# ARRIS ADD 
ifneq ($(BUILD_POST),y)
    subdir-$(CONFIG_TI_IPTABLES)            += netfilter_netlink/src
    subdir-$(CONFIG_TI_IPTABLES)            += netfilter_conntrack/src

ifeq ($(CONFIG_TI_HUNTER_CREEK),y)
    subdir-$(CONFIG_TI_IPTABLES)            += libmnl/src
    subdir-$(CONFIG_TI_IPTABLES)            += netfilter_queue/src
endif

endif
    subdir-$(CONFIG_TI_IPROUTE2)            += iproute2/src
subdir-y                            += vendor/src
# ARRIS ADD END
endif

# ARRIS ADD
ifneq (${CONFIG_MACH_PUMA5},y)
subdir-y                                    += inadyn/src
endif
# ARRIS ADD END

#
# Test software modules
#
#subdir-y                            += test/src

# Plugin so-s that may need to link with other so-s
ifneq ($(CONFIG_VENDOR_ARRIS_CM_TR69),y)
ifeq ($(CONFIG_TI_GW_SEPARATE_FS_ENABLE),y)
    subdir-y += \
        gw_control/src/cli_menus/src \
        gw_control/src/logger/src \

endif
endif

ifneq ($(GW_BUILDONLY),)
    # Build only targets on the list
ifneq ($(CONFIG_VENDOR_ARRIS_CM_TR69),y)
    subdir-y = $(foreach s,$(GW_BUILDONLY),$(s)/src )
endif
endif

# UNIHAN ADD
ifeq ($(CONFIG_ARRIS_ICTRL), y)
	subdir-$(CONFIG_ARRIS_ICTRL) += libsoap/src
endif
ifneq ($(CONFIG_VENDOR_ARRIS_CM_TR69),y)
subdir-y                            += nbtscan/src
endif
# UNIHAN END

ifeq (${CONFIG_ARRIS_HOTSPOT},y)
subdir-y                            += gre_dhcp_relay/src
subdir-y                            += gre_dhcp6_relay/src
subdir-y                            += sc_hooks/src
subdir-y                            += gre_monitor/src
endif

ifeq ($(CONFIG_VENDOR_ARRIS_BSS_STEERING),y)
subdir-y                            += BssSteering/src
endif

# ARRIS ADD BEGIN : customer and model specific files
WIFI_PREFIX = 
ifdef CONFIG_VENDOR_ARRIS_P6MG
	MODEL = P6MG
else
	MODEL = P6
endif
ifeq ($(DOCSIS_SOC), PUMA5)
 MODEL = P5
 WIFI_PREFIX = mtk_
endif

CUST = NA
ifeq ($(CONFIG_VENDOR_COMCAST),y)
	CUST = CT
endif
ifeq ($(CONFIG_VENDOR_TIME_WARNER),y)
	CUST = TW
endif

# ARRIS ADD END

#
# Targets that are supported according to UA spec,
# Please do not change anything below.
#

#gw-headers-list := $(shell ls ./include/*.h 2> /dev/null)

all: build install

build: first_rule

include $(COMPONENT_PREFIX)/build/rules.make

config: config-sub_dirs config_pcd
#	@install --preserve-timestamps -m 0644 $(gw-headers-list) $(TI_include)
	@#ARRIS CHANGE - preserve timestamps
#	@install -p -m 0644 $(TARGET_HOME)/ti/docsis/src/common/docsis_mac/ip_db/include/ipprov_db.h $(TI_include)/ipprov_db.h
#	@install -p -m 0644 $(TARGET_HOME)/ti/docsis/src/common/docsis_mac/ip_db/include/ip_defs.h $(TI_include)/ip_defs.h
	@#END ARRIS CHANGE
install: install-sub_dirs
#	@install -m 0755 $(GW_PCD_SRC) $(GW_PCD_DEST)
#ARRIS ADD BEGIN
ifeq ($(CONFIG_MACH_PUMA5),y)
	@echo Not installing oui.txt for PUMA5 products
ifeq ($(CONFIG_VENDOR_COMCAST),y)
	@install -m 0644 $(TARGET_HOME)/../ti_thirdparty/thirdparty/sercomm/MSO/puma5_nvram_oid.map.ct $(TI_FS_PATH)/var/tmp/puma5_nvram_oid.map.ct
else
	@install -m 0644 $(TARGET_HOME)/../ti_thirdparty/thirdparty/sercomm/MSO/puma5_nvram_oid.map $(TI_FS_PATH)/var/tmp/puma5_nvram_oid.map
endif
ifeq ($(INCLUDE_ARRIS_TG852),y)	
	@install -m 0644 $(TARGET_HOME)/build/mtk/ralink_uImage $(TI_filesystem_path)$(TI_etcdir)/ralink_uImage
endif	
ifeq ($(CONFIG_VENDOR_ARRIS_DG860),y)
	@install -m 0755 $(GW_PCD_SRC) $(GW_PCD_DEST2)
endif
else #PUMA6
#	@install -m 0644 vendor/src/oui.txt $(TI_filesystem_path)$(TI_etcdir)/oui.txt

ifeq ($(CUST),NA)
#	@find vendor/src/mso_defaults/ -wholename '*qca_wifi_gw_mso_defaults*' -not -wholename '*CT*' -not -wholename '*TW' | xargs install -m 0644 -t $(TI_filesystem_path)$(TI_etcdir)/
else
	@install -m 0644 vendor/src/mso_defaults/qca_wifi_gw_mso_defaults.$(CUST) $(TI_filesystem_path)$(TI_etcdir)/qca_wifi_gw_mso_defaults
endif

endif

ifeq ($(CUST),NA)

#	@find vendor/src/mso_defaults/ -wholename '*gwdb_gw_mso_defaults*' -not -wholename '*CT' -not -wholename '*TW' | xargs install -m 0644 -t $(TI_filesystem_path)$(TI_etcdir)/
#	@find vendor/src/mso_defaults/ -wholename '*$(WIFI_PREFIX)wifi_gw_mso_defaults*' -not -wholename '*CT' -not -wholename '*TW' | xargs install -m 0644 -t $(TI_filesystem_path)$(TI_etcdir)/
ifeq ($(DOCSIS_SOC), PUMA5)
	@rename "mtk_" "" $(TI_filesystem_path)$(TI_etcdir)/*wifi_gw_mso_defaults*
endif
#	@find vendor/src/mso_defaults/ -wholename '*rnddb_gw_mso_defaults*$(MODEL)' -not -wholename '*CT*$(MODEL)' -not -wholename '*TW*$(MODEL)' | xargs install -m 0644 -t $(TI_filesystem_path)$(TI_etcdir)/
	
else
	@install -m 0644 vendor/src/mso_defaults/gwdb_gw_mso_defaults.$(CUST) $(TI_filesystem_path)$(TI_etcdir)/gwdb_gw_mso_defaults
	@install -m 0644 vendor/src/mso_defaults/$(WIFI_PREFIX)wifi_gw_mso_defaults.$(CUST) $(TI_filesystem_path)$(TI_etcdir)/wifi_gw_mso_defaults
	@install -m 0644 vendor/src/mso_defaults/rnddb_gw_mso_defaults.$(CUST)_$(MODEL) $(TI_filesystem_path)$(TI_etcdir)/rnddb_gw_mso_defaults
endif
	mkdir -p $(TI_FS_PATH)/var/tmp/gwconfig/8/
#ARRIS ADD END


clean: uninstall clean-sub_dirs
	$(foreach subdir, $(subdir-y), $(shell find ./$(subdir) \( -name core -o -name '*~' -o -name '.*~' -o -name '.#*' \) -follow -type f -print | xargs rm -f ))

distclean: clean
	$(foreach subdir, $(subdir-y), $(shell find ./$(subdir) \( -name '*.d' \) -type f -print -follow | xargs rm -f ))

uninstall: uninstall-sub_dirs
	@rm -f $(patsubst ./include/%,$(TI_include)/%,$(gw-headers-list))
	@rm -f $(GW_PCD_DEST)

build_pre:

build_post:

#$(GW_PCD_H): $(GW_PCD_SRC)
#	@if [ -f $(TARGET_HOME)/ti/common_components/bin/pcdparser ]; then $(TARGET_HOME)/ti/common_components/bin/pcdparser -f $(GW_PCD_SRC) -o $(GW_PCD_H); fi


config_pcd: #$(GW_PCD_H)

