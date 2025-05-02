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
ifeq ($(CONFIG_TI_COMMON_COMPONENTS_DEBUG),y)
# ARRIS CHANGE - Change O0 to O1
CFLAGS-private := -g -O1
# END ARRIS CHANGE
COMPONENT_DEBUG := y
else
ifeq ($(CONFIG_TI_COMMON_COMPONENTS_SIZE_OPTIMIZATION),y)
CFLAGS-private := -Os
else
CFLAGS-private := -O2
endif
endif

# DSG Compile Flag Definition
ifeq ($(CONFIG_DSG),y)
export CFLAGS += -DDSG
endif
AUTO_GENERATE_H_FILES := $(shell ls ../include/*.h 2> /dev/null)
AUTO_GENERATE_C_FILES := $(patsubst %.c,%.o,$(shell ls *.c 2> /dev/null))

# The following creates unique CLI command and sub-menu names per component
CFLAGS-private += -DCLI_COMPONENT_NAME=CC


# ARRIS ADD - Makefile debug stuff
#UMF_DEBUG_LEVEL := 3
#UMF_LINK_ANALYSIS := y

