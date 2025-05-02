###########################################################################
## 							 					 ##
##                AAA   RRRRRR  RRRRRR  IIIII  SSSSS   			 ##
##               AAAAA  RR   RR RR   RR  III  SS       			 ##
##              AA   AA RRRRRR  RRRRRR   III   SSSSS   			 ##
##              AAAAAAA RR  RR  RR  RR   III       SS  			 ##
##              AA   AA RR   RR RR   RR IIIII  SSSSS   			 ##
##							 					 ##
###########################################################################

###########################################################################
# FILE NAME: arris_pcable_config.make 		     			        #
#												  #
# DESCRIPTION: Enable debugging option    					  #
#												  #
###########################################################################

-include $(TARGET_HOME)/.config

# Debugging
ifeq ($(CONFIG_ARRIS_PCABLE_DEBUG),y)
CFLAGS += -g -O1
LDFLAGS += -g
else
CFLAGS += -O2
endif

ifdef CONFIG_ARRIS_DECT
CFLAGS += -DGG_LINUX
endif
AUTO_GENERATE_H_FILES := $(shell ls ../include/*.h 2> /dev/null)
AUTO_GENERATE_C_FILES := $(patsubst %.c,%.o,$(shell ls *.c 2> /dev/null))

USO_POSTFIX := _all_subso

# Makefile debug stuff
#UMF_DEBUG_LEVEL := 3
#UMF_LINK_ANALYSIS := y
