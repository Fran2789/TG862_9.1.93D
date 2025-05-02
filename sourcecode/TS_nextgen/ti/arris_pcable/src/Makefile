###########################################################################
##                                                                       ##
##                AAA   RRRRRR  RRRRRR  IIIII  SSSSS                     ##
##               AAAAA  RR   RR RR   RR  III  SS                         ##
##              AA   AA RRRRRR  RRRRRR   III   SSSSS                     ##
##              AAAAAAA RR  RR  RR  RR   III       SS                    ##
##              AA   AA RR   RR RR   RR IIIII  SSSSS                     ##
##                                                                       ##
###########################################################################

###########################################################################
# FILE NAME: Makefile                                                     #
#                                                                         #
# DESCRIPTION:                                                            #
#                                                                         #
###########################################################################

.PHONY:  build clean distclean install uninstall

export COMPONENT_PREFIX := $(ARRIS_PCABLE_DIR)
export TI_include_callp := $(TI_include)

include $(ARRIS_PCABLE_DIR)/build/arris_pcable_config.make
include $(TARGET_HOME)/TI_Build.make

#
# Component directories to be built

ifdef CONFIG_SIP
subdir-y             += sip
ifdef CONFIG_ARRIS_PCABLE_20
subdir-y             += cert_bootstrapping
subdir-y             += uuid
endif
endif
subdir-y             += callp_dqos
subdir-y             += database
subdir-y             += dp
subdir-y             += dsp
subdir-y             += sys
subdir-y             += call_auto
subdir-y             += tlm/atmel_ko
# do not include TI charger on Puma5 hardware
ifneq ($(CONFIG_MACH_PUMA5), y)
subdir-y             += tlm/tibat_ko
endif
subdir-y             += tlm/tlm_ko
subdir-y             += tlm/tlm_main
subdir-y             += linecard

#
# Targets that are supported according to UA spec,
# Please do not change anything below.
#
subdirutil := $(patsubst %/src,/%_util/src,$(subdir-y))
subdirutil-y := $(join $(subdir-y),$(subdirutil))

#        
#Create a list of Arris pcable header files
#
ARRIS_PCABLE_HEADER_LIST := $(shell find $(TARGET_HOME)/ti/arris_pcable -type f -name "*.h" 2> /dev/null)
ARRIS_PCABLE_C_LIST = $(shell find $(TARGET_HOME)/ti/arris_pcable -type f -name "*.c" 2> /dev/null)
ARRIS_PCABLE_OBJ_LIST = $(subst .c,.o,$(ARRIS_PCABLE_C_LIST))
ARRIS_PCABLE_DEP_LIST = $(subst .c,.d,$(ARRIS_PCABLE_C_LIST))

all: prnt build install

prnt:
	echo $(subdir-y)

build: first_rule 

include $(TARGET_HOME)/ti/arris_pcable/build/rules.make

#Copies header files from the header list to TI's include folder
config: config-sub_dirs
	@install --preserve-timestamps -m 0644 $(ARRIS_PCABLE_HEADER_LIST) $(TI_include)

install: install-sub_dirs

clean: uninstall clean-sub_dirs
	@find . \( -name '*.[oas]' -o -name core -o -name '*~' -o -name '.*~' -o -name '.#*' -o -name '*.so' \) -follow -type f -print | xargs rm -f
	@echo 'cleaning arris_pcable compiled objects'
	@for file in $(ARRIS_PCABLE_OBJ_LIST); do ( \
            rm -f $${file}; \
        ); done
	@for file in $(ARRIS_PCABLE_DEP_LIST); do ( \
            rm -f $${file}; \
        ); done
ifndef CONFIG_SIP    
	@echo 'cleaning NCS parser code '
	@rm -f $(ARRIS_PCABLE_DIR)/src/callp_dqos/callp_sgcp.tab.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/callp_dqos/callp_sdp.tab.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/callp_dqos/callp_sgcp.lex.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/callp_dqos/callp_sdp.lex.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/callp_dqos/callp_sgcp.tab.h
	@rm -f $(ARRIS_PCABLE_DIR)/src/callp_dqos/callp_sdp.tab.h
	@rm -f $(ARRIS_PCABLE_DIR)/src/callp_dqos/callp_sgcp.output
	@rm -f $(ARRIS_PCABLE_DIR)/src/callp_dqos/callp_sdp.output
else
	@echo 'cleaning SIP parser code '
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_broadsoft_info.tab.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_mwi.tab.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_n2p_call_forward.tab.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_broadsoft_info.lex.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_mwi.lex.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_n2p_call_forward.lex.c
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_broadsoft_info.tab.h
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_mwi.tab.h
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_n2p_call_forward.tab.h 
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_broadsoft_info.output
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_mwi.tab.h
	@rm -f $(ARRIS_PCABLE_DIR)/src/sip/sip_n2p_call_forward.output
endif
    
distclean: clean
	@find . \( -name '.depend' \) -type f -print -follow | xargs rm -f

uninstall: uninstall-sub_dirs
	@rm -f $(patsubst %.h,$(TI_include)/%.h,$(filter %.h,$(subst /, ,$(ARRIS_PCABLE_HEADER_LIST))))

build_pre:

build_post:
