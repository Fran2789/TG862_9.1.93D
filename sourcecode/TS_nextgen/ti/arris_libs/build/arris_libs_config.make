#  Copyright 2014, ARRIS Group, Inc., All rights reserved                                        

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

AUTO_GENERATE_H_FILES := $(shell ls ../include/*.h 2> /dev/null)
AUTO_GENERATE_C_FILES := $(patsubst %.c,%.o,$(shell ls *.c 2> /dev/null))

USO_POSTFIX := _all_subso

# Makefile debug stuff
#UMF_DEBUG_LEVEL := 3
#UMF_LINK_ANALYSIS := y

