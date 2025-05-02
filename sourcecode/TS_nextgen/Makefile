#********************************************************************************
#**+--------------------------------------------------------------------------+**
#**|                            ****                                          |**
#**|                            ****                                          |**
#**|                            ******o***                                    |**
#**|                      ********_///_****                                   |**
#**|                      ***** /_//_/ ****                                   |**
#**|                       ** ** (__/ ****                                    |**
#**|                           *********                                      |**
#**|                            ****                                          |**
#**|                            ***                                           |**
#**|                                                                          |**
#**|         Copyright (c) 1998-2011 Texas Instruments Incorporated           |**
#**|                        ALL RIGHTS RESERVED                               |**
#**|                                                                          |**
#**| Permission is hereby granted to licensees of Texas Instruments           |**
#**| Incorporated (TI) products to use this computer program for the sole     |**
#**| purpose of implementing a licensee product based on TI products.         |**
#**| No other rights to reproduce, use, or disseminate this computer          |**
#**| program, whether in part or in whole, are granted.                       |**
#**|                                                                          |**
#**| TI makes no representation or warranties with respect to the             |**
#**| performance of this computer program, and specifically disclaims         |**
#**| any responsibility for any damages, special or consequential,            |**
#**| connected with the use of this program.                                  |**
#**|                                                                          |**
#**+--------------------------------------------------------------------------+**
#********************************************************************************

# Define the home directory to be the current
export TARGET_HOME := $(shell pwd)

-include $(TARGET_HOME)/.prod

ifndef PRODUCT
help:
	$(error Error: Please specify PRODUCT)

all: help

else
first_rule: help

ifndef PRODUCT_NAME
# Generate product name, as lower case of product if not defined.
PRODUCT_NAME = $(shell echo $(PRODUCT)| tr [:upper:] [:lower:])
endif

ifndef DOCSIS_SOC
# ARRIS CHANGE - Default dev1 to Puma6
$(warning Warning: Please specify DOCSIS_SOC <PUMA5 or PUMA6> The default is Puma6)
export DOCSIS_SOC:=PUMA6
# END ARRIS CHANGE
endif

ifndef DOCSIS_SOC_NAME
# Generate SoC name, as lower case of docsis_soc if not defined.
DOCSIS_SOC_NAME = $(shell echo $(DOCSIS_SOC)| tr [:upper:] [:lower:])
endif

# List of application components, and build exceptions (if any)
include $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs/.appcomps
# ARRIS ADD: BEGIN  Need to add the gw to the SPLIT_APP_COMPONENTS
#             and gw_lib/gw_bin to the APP_COMPONENTS VARIABLE
ifeq ($(CONFIG_VENDOR_ARRIS_GATEWAY_TR69),y)
ifeq (,$(findstring gw_lib , ${APP_COMPONENTS}))
APP_COMPONENTS := $(patsubst common_post, gw_bin gw_lib common_post, ${APP_COMPONENTS})
endif
ifeq (,$(findstring gw , ${SPLIT_APP_COMPONENTS}))
SPLIT_APP_COMPONENTS += gw
endif
endif
# ARRIS ADD: END


# ----------------------------------------------------------------------------- #
# Include the Unified system top makefile
include ti/system/make/Makefile
# ----------------------------------------------------------------------------- #
endif

# ARRIS ADD - Start (Preprocessor Macro)
ifeq ($(CONFIG_VENDOR_ARRIS_GATEWAY),y)
export INCLUDE_ARRIS_GW=y
CFLAGS += -DINCLUDE_ARRIS_GW
endif
ifeq ($(CONFIG_VENDOR_INTEL_GATEWAY),y)
export INCLUDE_INTEL_GW=y
CFLAGS += -DINCLUDE_INTEL_GW
endif
ifeq ($(CONFIG_VENDOR_INCLUDE_SERCOMM),y)
export INCLUDE_SERCOMM_GW=y
CFLAGS += -DINCLUDE_SERCOMM_GW
endif
ifeq ($(CONFIG_VENDOR_TIME_WARNER),y)
export INCLUDE_TWC=y
endif
ifeq ($(CONFIG_VENDOR_COMCAST),y)
export INCLUDE_CT=y
endif
ifeq ($(CONFIG_SYSTEM_EXTERNALSWITCH),y)
export CONFIG_SYSTEM_EXTERNALSWITCH=y
endif
ifeq ($(CONFIG_SYSTEM_REALTEKSWITCH),y)
export CONFIG_SYSTEM_REALTEKSWITCH=y
endif
ifeq ($(CONFIG_SYSTEM_BROADCOMSWITCH),y)
export CONFIG_SYSTEM_BROADCOMSWITCH=y
endif
ifeq ($(CONFIG_VENDOR_ARRIS_RDK),y)
export CONFIG_VENDOR_ARRIS_RDK=y
endif
ifeq ($(CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA),y)
export CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA=y
endif

ifeq ($(CONFIG_VENDOR_ARRIS_DOWNLOAD_TYPE),0x103)
export INCLUDE_ARRIS_TG852=y
CFLAGS += -DINCLUDE_ARRIS_TG852
endif
ifeq ($(CONFIG_VENDOR_ARRIS_DOWNLOAD_TYPE),0x107)
export INCLUDE_ARRIS_TG862=y
CFLAGS += -DINCLUDE_ARRIS_TG862
endif
ifeq ($(CONFIG_VENDOR_ARRIS_DOWNLOAD_TYPE),0x109)
export INCLUDE_ARRIS_TG862=y
CFLAGS += -DINCLUDE_ARRIS_TG862
endif
ifeq ($(CONFIG_VENDOR_ARRIS_TG1682),y)
export INCLUDE_ARRIS_TG1682=y
CFLAGS += -DINCLUDE_ARRIS_TG1682
endif
ifdef CONFIG_VENDOR_ARRIS_P6MG
export INCLUDE_ARRIS_P6MG=y
CFLAGS += -DINCLUDE_ARRIS_P6MG
endif

# UNIHAN ADD START
ifeq ($(CONFIG_SYSTEM_MOCA),y)
export INCLUDE_SYSTEM_MOCA=y
CFLAGS += -DINCLUDE_SYSTEM_MOCA
endif
# UNIHAN ADD END

ifdef CONFIG_ARRIS_PCABLE
CFLAGS += -DINCLUDE_PCABLE
endif

ifdef CONFIG_VENDOR_ARRIS_USG
CFLAGS += -DINCLUDE_USG
endif

ifdef CONFIG_ARRIS_DECT
CFLAGS += -DINCLUDE_DECT
endif

ifdef CONFIG_VENDOR_ARRIS_GATEWAY_TR69
CFLAGS += -DINCLUDE_ARRIS_GW_TR69
endif

ifdef CONFIG_ARRIS_PCABLE_20
ifdef CONFIG_SIP
CFLAGS += -DINC_STUN
endif
endif

ifdef CONFIG_SIP
CFLAGS += -DCONFIG_SIP
endif

ifdef CONFIG_ARRIS_PCABLE_20
CFLAGS += -DCONFIG_ARRIS_PCABLE_20 -DCONFIG_SIP
endif
# END ARRIS


# ARRIS ADD - End 
