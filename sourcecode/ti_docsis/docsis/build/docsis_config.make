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

-include $(TARGET_HOME)/.config


# Debugging
ifeq ($(CONFIG_TI_DOCSIS_DEBUG),y)

# Enable debug mode
CFLAGS-private += -g -ggdb -O1 -fno-omit-frame-pointer
COMPONENT_DEBUG := y
else
# Default optimization for speed
CFLAGS-private += -O2
endif

# DSG Compile Flag Definition
ifeq ($(CONFIG_DSG),y)
export CFLAGS += -DDSG
ifeq ($(CONFIG_HTX_DSG_RPC),y)
export CFLAGS += -DHTX_DSG_RPC
endif
ifeq ($(CONFIG_DSG_CCIF_PROXY),y)
export CFLAGS += -DDSG_CCIF_PROXY
endif
ifeq ($(CONFIG_DSG_INTEL_PARKER),y)
export CFLAGS += -DDSG_INTEL_PARKER
endif
endif
AUTO_GENERATE_H_FILES := $(shell ls ../include/*.h 2> /dev/null)
AUTO_GENERATE_C_FILES := $(patsubst %.c,%.o,$(shell ls *.c 2> /dev/null))

# The following 3 defines are used for debug purposes only, in order to print events as strings
# instead of numbers on SME messages.
# The first 2 defines are used for log messages in SME_CreateSM_ex() and the third is the name of
# the components array of structs that contain for each module (events enum) upper bound\lowwer bound
# and module events names array.

CFLAGS-private += -DSME_DEFAULT_LOG_COMPONENT=TI_COMPONENT_DOCSIS
CFLAGS-private += -DSME_DEFAULT_LOG_MODULE=DOCSIS_MOD_SME
CFLAGS-private += -DSME_DEFAULT_EVENT_NAMES_TABLE=DocsisEventIdNamesModulePtr___

# The following creates unique CLI command and sub-menu names per component
CFLAGS-private += -DCLI_COMPONENT_NAME=DOCSIS

# ARRIS CHANGE L_ALL_DOCSIS
L_ALL_DOCSIS := -lcrypto -lti_sme -lsme -lticc -lall_docsis -lm
ifeq ($(CONFIG_DSG),y)
L_ALL_DOCSIS += -lpthread
endif
ifeq ($(CONFIG_SYSTEM_MOCA2_0),y)
L_ALL_DOCSIS += -lmoca_api
endif
USO_POSTFIX := _all_subso
