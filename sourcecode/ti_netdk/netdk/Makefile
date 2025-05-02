# *************************************************************************
# * Makefile for building NET-DK components.
# *************************************************************************
# *	(C) Copyright 2006, Texas Instruments, Inc.
# *************************************************************************

# Read the configuration file. 
-include $(TARGET_HOME)/.config
-include $(TARGET_HOME)/TI_Build.make

#--------------------------------------------------------------------------
# NET-DK Standard Definitions
#--------------------------------------------------------------------------

# Source directory where all the NET-DK source files are located.
NETDK_SRC_DIR = $(TARGET_HOME)/ti/netdk/src

#--------------------------------------------------------------------------
# Select the NET-DK Components on the basis of the configuration
#--------------------------------------------------------------------------

TI_NETDK_COMPONENTS-$(CONFIG_TI_SSL) 	  	            += openssl
# ARRIS REMOVE - Move this to Arris_libs Makefile - TI_NETDK_COMPONENTS-$(CONFIG_TI_UDHCP) 		            += udhcp
# ARRIS REMOVE - Move this to Arris_libs Makefile - TI_NETDK_COMPONENTS-$(CONFIG_TI_UTELNETD) 	            += utelnetd
TI_NETDK_COMPONENTS-$(CONFIG_TI_BRIDGE_UTILS)           += bridge_utils
TI_NETDK_COMPONENTS-$(CONFIG_TI_MINIHTTPD) 	            += mini_httpd
# dproxy move to il_gwdk vob
# TI_NETDK_COMPONENTS-$(CONFIG_TI_DPROXY) 	            += dproxy 
# ARRIS REMOVE - Move this to Arris_libs Makefile - TI_NETDK_COMPONENTS-$(CONFIG_TI_TIUDHCP) 	            += ti_udhcp   
# ARRIS REMOVE - Move this to Arris_libs Makefile - TI_NETDK_COMPONENTS-$(CONFIG_TI_TODC)                          += ti_todc
# ARRIS REMOVE - Move this to Arris_libs Makefile - TI_NETDK_COMPONENTS-$(CONFIG_TI_WEBSERVER)              += ti_webserver
TI_NETDK_COMPONENTS-$(CONFIG_TI_SYSLOG) 	            += sysklogd   
# ARRIS REMOVE - Move this to Arris_libs Makefile - TI_NETDK_COMPONENTS-$(CONFIG_TI_TFTP)                          += tftp
# ARRIS REMOVE - Move this to Arris_libs Makefile - TI_NETDK_COMPONENTS-$(CONFIG_TI_DHCPv6) 	            += ti_dhcpv6   
#TI_NETDK_COMPONENTS-$(CONFIG_TI_IPSEC_TOOLS)            += ipsec_tools  
#TI_NETDK_COMPONENTS-$(CONFIG_TI_BER)                    += ber  
TI_NETDK_COMPONENTS-$(CONFIG_TI_NETDK_TEST_APPS)   		+= ti_netdk_test_app  
TI_NETDK_COMPONENTS-$(CONFIG_TI_DNSC_RULI) 	  	        += liboop
# ARRIS REMOVE - Move this to Arris_libs Makefile - TI_NETDK_COMPONENTS-$(CONFIG_TI_DNSC_RULI) 	  	        += ruli
# ARRIS REMOVE - Move this to Arris_libs Makefile - TI_NETDK_COMPONENTS-$(CONFIG_TI_DROPBEAR) 	  	        += dropbear-0.50

TI_CLEAN_ALL_NETDK_COMPONENTS = udhcp utelnetd openssl ti_arp bridge_utils ti_udhcp ti_todc flex ti_webserver sysklogd ti_tftp ti_dhcpv6 ti_netdk_libs ipsec_tools ber ti_netdk_test_app liboop ruli ti_kerberos dropbear-0.50
# mini_httpd dproxy

TI_NETDK_COMPONENTS_CLEAN      = $(foreach T,$(TI_CLEAN_ALL_NETDK_COMPONENTS),$(T)_clean)
TI_NETDK_COMPONENTS_DISTCLEAN = $(foreach T,$(TI_CLEAN_ALL_NETDK_COMPONENTS),$(T)_distclean)

#--------------------------------------------------------------------------
# NET-DK Targets that are supported.
#--------------------------------------------------------------------------
.NOTPARALLEL:

all: build install

config:

build: $(TI_NETDK_COMPONENTS-y)

install: dummy

clean: $(TI_NETDK_COMPONENTS_CLEAN)

distclean: $(TI_NETDK_COMPONENTS_DISTCLEAN)

package:
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos libasn1.a
	$(TARGET_HOME)/tools/scripts/mkpkg
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos clean

dummy: 
#--------------------------------------------------------------------------
# Individual Rule for building each of the NET-DK Components.
#--------------------------------------------------------------------------
# ****************
# NETDK TEST APPS
# **************** 

ti_netdk_test_app:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_netdk_test_applications -f Makefile.flat all

ti_netdk_test_app_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_netdk_test_applications -f Makefile.flat clean

ti_netdk_test_app_distclean: ti_netdk_test_app_clean 
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
ifdef CONFIG_TI_NETDK_TEST_APP_IPSEC
	rm -f $(TI_filesystem_path)/usr/sbin/test_setkey
endif

# ********
# KERBEROS
# ********

ti_kerberos:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos 
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos install

ti_kerberos_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos clean

ti_kerberos_distclean:ti_kerberos_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos distclean

# ********
# CERTLIB
# ********

ti_certlib:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos/ossl 
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos/ossl install

ti_certlib_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos/ossl clean

ti_certlib_distclean:ti_certlib_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_kerberos/ossl distclean


# ****************
# BER Library
# ****************

ber:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/libber -f Makefile.flat

ber_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/libber -f Makefile.flat clean

ber_distclean:ber_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/libber/src libdir=$(TI_filesystem_path)$(TI_rlibdir) builddir=$(TI_build_root)/ti/lib uninstall
	rm -f  $(TI_filesystem_path)$(TI_rlibdir)/libber.so
	rm -f  $(TI_filesystem_path)$(TI_rlibdir)/libber.so.0
	rm -f  $(TI_filesystem_path)$(TI_rlibdir)/libber.so.0.4.2
ifdef CONFIG_TI_INSTALL_BER_TEST_APPS
	rm -f $(TI_filesystem_path)/usr/sbin/test_utils 
	rm -f $(TI_filesystem_path)/usr/sbin/test_encode 
	rm -f $(TI_filesystem_path)/usr/sbin/test_decode 
endif


# **************
# IPSec Tools.
# ************** 

ipsec_tools:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ipsec_tools -f Makefile.flat

ipsec_tools_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ipsec_tools -f Makefile.flat clean

ipsec_tools_distclean: ipsec_tools_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f $(TI_filesystem_path)$(TI_sbindir)/setkey
	rm -f $(TI_filesystem_path)$(TI_rlibdir)/libipsec.so


# **************
# Telnetd.
# **************

utelnetd:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"

utelnetd_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/utelnetd -f Makefile.flat clean

utelnetd_distclean: utelnetd_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f $(TI_filesystem_path)$(TI_rsbindir)/utelnetd

# **************
# Mini-HTTP
# **************

mini_httpd:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/mini_httpd -f Makefile.flat

mini_httpd_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/mini_httpd -f Makefile.flat clean

mini_httpd_distclean: mini_httpd_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f $(TI_filesystem_path)$(TI_sbindir)/mini_httpd 

# **************
# Bridge-Utils
# **************

bridge_utils:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/bridge_utils -f Makefile.flat 

bridge_utils_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/bridge_utils -f Makefile.flat clean

bridge_utils_distclean: bridge_utils_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f $(TI_filesystem_path)$(TI_sbindir)/brctl 

# **************
# DHCP Server
# **************

udhcp:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/udhcp -f Makefile.flat 

udhcp_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/udhcp -f Makefile.flat clean

udhcp_distclean: udhcp_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f $(TI_filesystem_path)$(TI_rsbindir)/udhcpd  
	rm -f $(TI_filesystem_path)$(TI_rsbindir)/udhcpc

# **************
# DNS Proxy
# **************

dproxy:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/dproxy -f Makefile.flat 

dproxy_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/dproxy -f Makefile.flat clean

dproxy_distclean: dproxy_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f $(TI_filesystem_path)$(TI_rsbindir)/dproxy 
	rm -f $(TI_filesystem_path)$(TI_etcdir)/dproxy.conf

# **************
# Open SSL
# **************

openssl:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/openssl -f Makefile.flat

openssl_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/openssl -f Makefile.flat clean

openssl_distclean:
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/openssl -f Makefile.flat distclean

# **************
# TI ARP Utility
# **************

ti_arp:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_arp -f Makefile.flat

ti_arp_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_arp -f Makefile.flat clean

ti_arp_distclean:ti_arp_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f  $(TI_filesystem_path)$(TI_rsbindir)/arp

# ********************
# TI UDHCPC V4 Utility
# ********************

ti_udhcp:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_udhcp -f Makefile.flat

ti_udhcp_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_udhcp -f Makefile.flat clean

ti_udhcp_distclean:ti_udhcp_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f  $(TI_filesystem_path)$(TI_rsbindir)/ti_udhcpc


# ****************************
# TI TODC v4/v6 Utility
# ****************************

ti_todc:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"

ti_todc_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_todc -f Makefile.flat clean

ti_todc_distclean:ti_todc_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f  $(TI_filesystem_path)$(TI_rsbindir)/ti_todc


# ****************
# FLEX Library
# ****************

flex:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/flex -f Makefile.flat

flex_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/flex -f Makefile.flat clean

flex_distclean:flex_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f  $(TARGET_HOME)/ti$(TI_rlibdir)/libfl.a


# **************
# TI WEBSERVER
# **************

ti_webserver:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_webserver -f Makefile.flat

ti_webserver_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_webserver -f Makefile.flat clean

ti_webserver_distclean: ti_webserver_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f $(TI_filesystem_path)$(TI_sbindir)/ti_webserver
	rm -f $(TI_filesystem_path)$(TI_rlibdir)/http_plugin.so
	@if [ -e $(TI_filesystem_path)$(TI_rlibdir)/https_plugin.so ]; then \
	rm -f $(TI_filesystem_path)$(TI_rlibdir)/https_plugin.so ; \
	fi


# ****************
# syslogd
# ****************

sysklogd:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_sysklogd -f Makefile.flat

sysklogd_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_sysklogd -f Makefile.flat clean

sysklogd_distclean: sysklogd_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"

# ****************
# tftp
# ****************

ti_tftp:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"

ti_tftp_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_tftp -f Makefile.flat clean

ti_tftp_distclean: ti_tftp_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"

# ********************
# TI DHCPv6 Utility
# ********************

ti_dhcpv6:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"

ti_dhcpv6_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_dhcpv6 -f Makefile.flat clean

ti_dhcpv6_distclean:ti_dhcpv6_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f  $(TI_filesystem_path)$(TI_rsbindir)/ti_dhcp6c

# *****************
# NETDK Libs 
# *****************

ti_netdk_libs:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_netdk_libs -f Makefile.flat

ti_netdk_libs_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ti_netdk_libs -f Makefile.flat clean

ti_netdk_libs_distclean:ti_netdk_libs_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
	rm -f $(TI_filesystem_path)$(TI_rlibdir)/libtinetdk.so

# ****************
# LIBOOP Library
# ****************

liboop:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/liboop -f Makefile.flat

liboop_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/liboop -f Makefile.flat clean

liboop_distclean:liboop_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"

# ****************
# RULI Library
# ****************

ruli:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ruli -f Makefile.flat

ruli_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/ruli -f Makefile.flat clean

ruli_distclean:ruli_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
ifdef CONFIG_TI_INSTALL_DNSC_TEST_APPS
	rm -f $(TI_filesystem_path)/usr/sbin/ti_api_demo
endif

# *****************************
# DROPBEAR - SSH Client/Server
# *****************************

dropbear-0.50:
	@echo "*--------------------------------------------------"
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"

dropbear-0.50_clean:
	@echo "*--------------------------------------------------"
	@echo "*  Cleaning: $@                                    "
	@echo "*--------------------------------------------------"
	$(MAKE) -C $(NETDK_SRC_DIR)/dropbear-0.50 -f Makefile.flat clean

dropbear-0.50_distclean:dropbear-0.50_clean
	@echo "*--------------------------------------------------"
	@echo "*  Dist Cleaning: $@                               "
	@echo "*--------------------------------------------------"
