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
#**|         Copyright (c) 1998-2006 Texas Instruments Incorporated           |**
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

#
# Common Components CLI menu configuration file
#

#
# List of top-level directories of each component
#
# Add new directories here.
# All components are usually under $(TARGET_HOME)/ti directory.
#
# The format is as follows:
# TARGET_COMPONENT = $(TARGET_HOME)/ti/component_directory
#
TARGET_COMMON_COMPONENTS = $(TARGET_HOME)/ti/common_components/src
ifndef KERNEL_DIR
KERNEL_DIR := $(TARGET_HOME)/kernel/ti/linux-2.6.39.3/src
endif

#
# List of include directories for the component, to be added to the CLI compilation.
#
# Add directory list here.
# Component's include directory should be $(TARGET_COMPONENT)/src/vendor/cli_menus
# Other component APIs should be exported by the component to system's ti/include directory.
# 
# The format is as follows:
# CLI_MENU_CFLAGS-$(CONFIG_TI_COMPONENT) += $(TARGET_COMPONENT)/src/vendor/cli_menus
#
CLI_MENU_CFLAGS-$(CONFIG_TI_LOGGER) += -I$(TARGET_COMMON_COMPONENTS)/logger/include
CLI_MENU_CFLAGS-$(CONFIG_TI_EVENT_MGR) += -I$(TARGET_COMMON_COMPONENTS)/event_mngr/include
CLI_MENU_CFLAGS-$(CONFIG_TI_STATEMACHINE) += -I$(TARGET_COMMON_COMPONENTS)/ti_sme/src/ti_sme_menu/include
CLI_MENU_CFLAGS-y += -I$(TARGET_COMMON_COMPONENTS)/../system/cli_menus/include
ifeq ($(DOCSIS_SOC), PUMA6)
CLI_MENU_CFLAGS-y += -I$(TARGET_COMMON_COMPONENTS)/bbu6/src/bbu_menu/include
else
CLI_MENU_CFLAGS-y += -I$(TARGET_COMMON_COMPONENTS)/bbu/src/bbu_menu/include
endif
CLI_MENU_CFLAGS-$(CONFIG_SYSTEM_L2SWITCH) += -I$(TARGET_COMMON_COMPONENTS)/l2switch_v3/include
CLI_MENU_CFLAGS-$(CONFIG_SYSTEM_MOCA) += -I$(TARGET_COMMON_COMPONENTS)/moca/include
# ARRIS ADD START
ifeq ($(CONFIG_TI_CABLE_VFE),y)
CLI_MENU_CFLAGS-y += -I$(TARGET_HOME)/ti/cable_vfe/src/include
endif
# ARRIS ADD END 

ifeq ($(DOCSIS_SOC), PUMA6)
CLI_MENU_CFLAGS-y += -I$(KERNEL_DIR)/include/asm-arm/arch-avalanche/puma6
endif
CLI_MENU_CFLAGS-$(CONFIG_SYSTEM_APPPSM) += -I$(TARGET_COMMON_COMPONENTS)/src/psm/src/psm_menu/include
CLI_MENU_CFLAGS-$(CONFIG_SYSTEM_LPCM) += -I$(TARGET_COMMON_COMPONENTS)/src/lpcm/src/lpcm_menu/include
CLI_MENU_CFLAGS-$(CONFIG_SYSTEM_LPCM) += -I$(TARGET_COMMON_COMPONENTS)/src/lpcm/src/lpcm_utils/include

#
# List of menu object(s) for the component, to be compiled by the CLI.
#
# Add object list here.
# All component's menu objects should be under $(TARGET_COMPONENT)/src/vendor/cli_menus
# 
# The format is as follows:
# CLI_MENU_FILES-$(CONFIG_TI_COMPONENT) += $(TARGET_COMPONENT)/src/vendor/cli_menus/component_menu.o
#
CLI_MENU_FILES-$(CONFIG_TI_LOGGER) += $(TARGET_COMMON_COMPONENTS)/logger/src/logger_menu/src/logger_menu.o
CLI_MENU_FILES-$(CONFIG_TI_EVENT_MGR) += $(TARGET_COMMON_COMPONENTS)/event_mngr/src/event_mgr_menu/src/event_mgr_menu.o
CLI_MENU_FILES-$(CONFIG_TI_STATEMACHINE) += $(TARGET_COMMON_COMPONENTS)/ti_sme/src/ti_sme_menu/src/sme_menu.o
CLI_MENU_SYSTEM_FILES := $(shell ls $(TARGET_COMMON_COMPONENTS)/../system/cli_menus/src/*.c 2> /dev/null)
ifeq ($(DOCSIS_SOC), PUMA6)
CLI_MENU_FILES-$(CONFIG_TI_BBU) += $(TARGET_COMMON_COMPONENTS)/bbu6/src/bbu_menu/src/bbu_menu.o
CLI_MENU_FILES-$(CONFIG_TI_BBU) += $(TARGET_COMMON_COMPONENTS)/bbu6/src/bbu_menu/src/bbu_dbg_menu.o
else
CLI_MENU_FILES-$(CONFIG_TI_BBU) += $(TARGET_COMMON_COMPONENTS)/bbu/src/bbu_menu/src/bbu_menu.o
CLI_MENU_FILES-$(CONFIG_TI_BBU) += $(TARGET_COMMON_COMPONENTS)/bbu/src/bbu_menu/src/bbu_dbg_menu.o
endif 
CLI_MENU_FILES-$(CONFIG_SYSTEM_L2SWITCH) += $(TARGET_COMMON_COMPONENTS)/l2switch_v3/src/l2switch_menu/src/l2switch_menu.o
CLI_MENU_FILES-$(CONFIG_SYSTEM_MOCA) += $(TARGET_COMMON_COMPONENTS)/moca/src/moca_menu/src/moca_menu.o
CLI_MENU_FILES-$(CONFIG_SYSTEM_MOCA1_1) += $(TARGET_COMMON_COMPONENTS)/moca/src/moca_menu/src/structureprint.o
CLI_MENU_FILES-$(CONFIG_SYSTEM_EXTERNALSWITCH) += $(TARGET_COMMON_COMPONENTS)/l2switch_v3/src/l2switch_menu/src/ext_switch_menu.o
CLI_MENU_FILES-y += $(CLI_MENU_SYSTEM_FILES:.c=.o)
CLI_MENU_FILES-$(CONFIG_SYSTEM_APPPSM) += $(TARGET_COMMON_COMPONENTS)/psm/src/psm_menu/src/psm_submenu.o 
CLI_MENU_FILES-$(CONFIG_SYSTEM_LPCM) += $(TARGET_COMMON_COMPONENTS)/lpcm/src/lpcm_utils/src/lpcm_db_utils.o 
CLI_MENU_FILES-$(CONFIG_SYSTEM_LPCM) += $(TARGET_COMMON_COMPONENTS)/lpcm/src/lpcm_menu/src/lpcm_menu.o 
#
# List of libraries required for the component menu object.
#
# Add libraries list here, if required (lines can be empty).
# All component's libraries should be exported by the component to system's ti/lib directory.
# 
# The format is as follows:
# CLI_MENU_LDFLAGS-$(CONFIG_TI_COMPONENT) += -llibrary
#
ifdef CONFIG_TI_COMMON_COMPONENTS_TICC
CLI_MENU_LDFLAGS-y += -lticc
else
CLI_MENU_LDFLAGS-$(CONFIG_TI_LOGGER) += -llogger -lshmdb -licc
CLI_MENU_LDFLAGS-$(CONFIG_TI_EVENT_MGR) += -leventmgrif
CLI_MENU_LDFLAGS-$(CONFIG_TI_STATEMACHINE) += -lsme -lti_sme
endif
CLI_MENU_LDFLAGS-$(CONFIG_TI_LOGGER) += -llogger_ldplugin 
CLI_MENU_LDFLAGS-$(CONFIG_SYSTEM_MOCA2_0) += -lmoca_api
CLI_MENU_LDFLAGS-$(CONFIG_TI_BBU) += -lbbu
CLI_MENU_LDFLAGS-$(CONFIG_TI_STATEMACHINE) += -lti_sme -lsme
CLI_MENU_LDFLAGS-y += -lm
ifdef CONFIG_VENDOR_TIME_WARNER
CLI_MENU_LDFLAGS-y += -larris_password -lcertlib
endif
