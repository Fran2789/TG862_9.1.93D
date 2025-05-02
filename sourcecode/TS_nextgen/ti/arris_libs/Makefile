##  Copyright 2014, ARRIS Enterprises, Inc., All rights reserved                              ##

.PHONY: dummy all config build install uninstall clean distclean package

#
# Include the configuration and definition files
#
-include $(TARGET_HOME)/.config
include $(TARGET_HOME)/TI_Build.make

export DOCSIS_PREFIX  := $(TARGET_HOME)/ti/docsis
export CC_PREFIX := $(TARGET_HOME)/ti/common_components
export NETDK_PREFIX := $(TARGET_HOME)/ti/netdk
export DOCSIS_SHARED_LIBS_PREFIX := $(DOCSIS_PREFIX)/build
export TI_INC_PREFIX := $(TARGET_HOME)/ti/include
export ARRIS_DOCSIS_PREFIX := $(TARGET_HOME)/ti/arris_docsis
export ARRIS_PCABLE_PREFIX := $(TARGET_HOME)/ti/arris_pcable
export ARRIS_LIBS_PREFIX := $(TARGET_HOME)/ti/arris_libs
export PACM_PREFIX := $(TARGET_HOME)/ti/pacm
## ARRIS ADD GW_PREFIX ##
export GW_PREFIX := $(TARGET_HOME)/ti/gw

#
# Build directories
#                                       $(TARGET_HOME)/ti/
build-y += src

udhcp-$(CONFIG_TI_UDHCP) := $(NETDK_PREFIX)/src/udhcp/udhcpd
telnet-$(CONFIG_TI_UTELNETD) := $(NETDK_PREFIX)/src/utelnetd/utelnetd
todc-$(CONFIG_TI_TODC) := $(NETDK_PREFIX)/src/ti_todc
tftp-$(CONFIG_TI_TFTP) := $(NETDK_PREFIX)/src/ti_tftp
dhcp-$(CONFIG_TI_TIUDHCP) := $(NETDK_PREFIX)/src/ti_udhcp
dhcpv6-$(CONFIG_TI_DHCPv6) := $(NETDK_PREFIX)/src/ti_dhcpv6
ti_webserver-$(CONFIG_TI_WEBSERVER) := $(NETDK_PREFIX)/src/ti_webserver_ti_webserver
dropbear-$(CONFIG_TI_DROPBEAR) := $(NETDK_PREFIX)/src/dropbear-0.50/dropbear
ruli-$(CONFIG_TI_DNSC_RULI)    := $(NETDK_PREFIX)/src/src/ruli
#
# General definitions
#
BUILD_DIRS-$(CONFIG_TI_DOCSIS) := $(build-y)
CLEAN_DIRS := $(shell set t=""; for i in "$(BUILD_DIRS-y)"; do t="$$i $$t"; done; echo "$$t")
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
# Handle build dependencies 
#
extra-install-list := $(telnet-y) $(todc-y) $(tftp-y) $(dhcp-y) $(dhcpv6-y) $(ti_webserver-y) $(dropbear-y) $(udhcp-y) $(ruli-y) 
extra-clean-list := $(telnet-y)_clean $(todc-y)_clean $(tftp-y)_clean $(dhcp-y)_clean $(dhcpv6-y)_clean $(ti_webserver-y)_clean $(dropbear-y)_clean $(udhcp-y)_clean \
					$(ruli-y)_clean
extra-distclean-list := $(telnet-y)_distclean $(todc-y)_distclean $(tftp-y)_distclean $(dhcp-y)_distclean $(dhcpv6-y)_distclean $(ti_webserver-y)_distclean $(dropbear-y)_distclean $(udhcp-y)_distclean $(ruli-y)_distclean

#
# DOCSIS Targets that are supported (According to UA spec)
#
all: build install

dummy:

build: dummy $(build-list)


install: dummy $(install-list) $(extra-install-list)

uninstall: dummy $(uninstall-list)

clean: dummy $(clean-list) $(extra-clean-list)
	@find ./build \( -name '*.[oas]' -o -name core -o -name '*~' -o -name '.*~' -o -name '.#*' -o -name '*.so' \) -follow -type f -print | xargs rm -f

distclean: dummy clean $(distclean-list) $(extra-distclean-list)
      
package:

config: $(config-list)


## UDHCP hack - Need to build TI UDHCP server AFTER Arris libs are installed
$(udhcp-y): dummy
	$(MAKE) -f $(NETDK_PREFIX)/Makefile udhcp

$(udhcp-y)_clean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile udhcp_clean

$(udhcp-y)_distclean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile udhcp_distclean

## Telnet hack - Need to build TI telnet server AFTER Arris libs are installed
$(telnet-y): dummy
	$(MAKE) -f $(NETDK_PREFIX)/Makefile utelnetd

$(telnet-y)_clean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile utelnetd_clean

$(telnet-y)_distclean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile utelnetd_distclean


## Todc hack - Need to build ti_todc AFTER Arris libs are installed
$(todc-y): dummy
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_todc

$(todc-y)_clean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_todc_clean

$(todc-y)_distclean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_todc_distclean

## Tftp hack - Need to build ti_tftp AFTER Arris libs are installed
$(tftp-y): dummy
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_tftp

$(tftp-y)_clean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_tftp_clean

$(tftp-y)_distclean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_tftp_distclean

## Dhcp hack - Need to build ti_udhcp AFTER Arris libs are installed
$(dhcp-y): dummy
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_udhcp

$(dhcp-y)_clean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_udhcp_clean

$(dhcp-y)_distclean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_udhcp_distclean

## Dhcpv6 hack - Need to build ti_dhcpv6 AFTER Arris libs are installed
$(dhcpv6-y): dummy
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_dhcpv6

$(dhcpv6-y)_clean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_dhcpv6_clean

$(dhcpv6-y)_distclean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_dhcpv6_distclean

## ti_webserver hack - Need to build ti_webserver AFTER Arris libs are installed
$(ti_webserver-y): dummy
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_webserver

$(ti_webserver-y)_clean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_webserver_clean

$(ti_webserver-y)_distclean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ti_webserver_distclean

## Dropbear hack - Need to build dropbear AFTER Arris libs are installed
$(dropbear-y): dummy
	$(MAKE) -f $(NETDK_PREFIX)/Makefile dropbear-0.50

$(dropbear-y)_clean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile dropbear-0.50_clean

$(dropbear-y)_distclean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile dropbear-0.50_distclean

## ruli - Need to build ruli AFTER Arris libs are installed
$(ruli-y): dummy
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ruli

$(ruli-y)_clean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ruli_clean

$(ruli-y)_distclean:
	$(MAKE) -f $(NETDK_PREFIX)/Makefile ruli_distclean

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

