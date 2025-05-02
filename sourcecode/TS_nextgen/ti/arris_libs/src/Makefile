##  Copyright 2014, ARRIS Group, Inc., All rights reserved                              ##

.PHONY:  build clean distclean install uninstall

#
# Include the configuration file and build file
#
include $(ARRIS_LIBS_PREFIX)/build/arris_libs_config.make 
include $(TARGET_HOME)/TI_Build.make


#
# Modules directories to be built
#

#
# First we build all libraries
#
ifeq ($(CONFIG_MACH_PUMA6),y)
subdir-y += mfh
endif
subdir-y += boot
ifeq ($(CONFIG_SYSTEM_EXTERNALSWITCH),y)
subdir-y += extswt
else
subdir-y += eth
endif

subdir-y += nvm
subdir-y += cli
subdir-y += timers
subdir-y += database
# UNIHAN ADD START
ifndef CONFIG_VENDOR_COMCAST
subdir-y += tacacs/src
endif
# UNIHAN ADD END
subdir-y += password
subdir-y += rf
subdir-y += ncsLog_db
subdir-y += libcurl
subdir-y += web

ifeq ($(CONFIG_ARRIS_PCABLE),y) 
subdir-y += callptimers
subdir-y += tlm
subdir-y += vqm
subdir-y += mta_db
subdir-y += mta_table_db
else
ifdef INCLUDE_ARRIS_GW
subdir-y += tlm
endif
endif

ifdef CONFIG_SIP
subdir-y += sip_db
endif
subdir-y += llist
ifdef INCLUDE_SERCOMM_GW
subdir-y += gw_api
endif
ifdef CONFIG_VENDOR_ARRIS_SPEEDTEST
subdir-y += speedtest_db
endif

ifdef CONFIG_VENDOR_ARRIS_HNE_AUTOCFG
subdir-y += hne_auto_config
endif

subdir-$(CONFIG_TI_LIBZ) += libz/src

ifdef CONFIG_VENDOR_ARRIS_NPM
subdir-y += tidy
subdir-y += npm_db
endif
ifdef CONFIG_VENDOR_ARRIS_GATEWAY_TR69
subdir-y += tr69/src
endif

# pcre needs to build before agentx 
subdir-y += pcre/src

subdir-y += arris_kernel
subdir-y += agentx/src

ifdef INCLUDE_ARRIS_GW
subdir-y += clientdb
endif

ifeq ($(CONFIG_ARRIS_HPAV_PROXY),y)
subdir-y += hpavdb
subdir-y += hpavproxy
endif

# OG1600 GPS code
subdir-$(CONFIG_VENDOR_GPS) += gps

# Make sure main is LAST on this list as it pulls in some previous libs
subdir-y += main

# UNIHAN ADD START
subdir-y += ntp/src
# UNIHAN ADD END

ifeq ($(CONFIG_PROFILING),y)
subdir-y += popt/src
subdir-y += binutils/src
endif

#SERCOMM ADD
ifeq ($(CONFIG_VENDOR_ARRIS_BSS_STEERING),y)
subdir-y += BssSteering_lib 
endif
#SERCOMM END 
#
# Second, we build all applications
#



#
# Targets that are supported according to UA spec,
# Please do not change anything below.
#
subdirutil := $(patsubst %/src,/%_util/src,$(subdir-y))
subdirutil-y := $(join $(subdir-y),$(subdirutil))

arris-docsis-headers-list := $(shell ls $(ARRIS_DOCSIS_PREFIX)/include/*.h 2> /dev/null)
arris-libs-headers-list += $(shell ls $(ARRIS_LIBS_PREFIX)/include/*.h 2> /dev/null)
curl-headers-list := $(shell ls $(ARRIS_LIBS_PREFIX)/src/include/curl/*.h 2>  /dev/null)

# Build up a list of everything to install and clean.  Do not clean the arris_docsis stuff as arris_docsis will handle that!
installed-headers-list := $(arris-docsis-headers-list) $(arris-libs-headers-list)

ifdef INCLUDE_ARRIS_GW
gw_header_list := $(shell ls gw_api/*.h 2>  /dev/null)
installed-headers-list += $(gw_header_list)
endif

tidy_header_list := $(shell ls $(ARRIS_LIBS_PREFIX)/src/include/tidy/*.h 2>  /dev/null)

all: prnt build install

prnt:
	@echo Building $(subdir-y)

#build: dep first_rule
build: first_rule

  export COMPONENT_INCLUDES := $(patsubst %,-I%, $(shell find $(DOCSIS_PREFIX) -name include -type d -follow)) $(patsubst %,-I%, $(shell find $(CC_PREFIX) -name include -type d -follow)) $(patsubst %,-I%, $(shell find $(ARRIS_DOCSIS_PREFIX) -name include -type d -follow)) 
ifeq ($(CONFIG_ARRIS_PCABLE),y) 
  export  COMPONENT_INCLUDES += $(patsubst %,-I%, $(shell find $(ARRIS_PCABLE_PREFIX) -name include -type d -follow)) -I$(TARGET_HOME)/ti/arris_libs/src/llist
ifeq ($(CONFIG_SIP),y)
  export  COMPONENT_INCLUDES += $(patsubst %,-I%, $(shell find $(TARGET_HOME)/../SIP/include -type d -follow)) $(patsubst %,-I%, $(shell find $(TARGET_HOME)/../SIP/sip -type d -follow)) $(patsubst %,-I%, $(shell find $(TARGET_HOME)/../SIP/common -type d -follow)) -I$(TARGET_HOME)/../SIP/sdp -I$(TARGET_HOME)/../SIP/ads -DRV_OS_TYPE=RV_OS_TYPE_LINUX -DCONFIG_SIP 
endif
endif

export COMPONENT_PREFIX := $(ARRIS_LIBS_PREFIX)

include $(ARRIS_LIBS_PREFIX)/build/rules.make

config: config-sub_dirs
	@echo Arris_libs installing: $(installed-headers-list)
	@echo ARRIS_LIBS_PREFIX is $(ARRIS_LIBS_PREFIX)

install: install-sub_dirs install_extra

install_extra:

clean: uninstall clean-sub_dirs
	find . \( -name '*.[oas]' -o -name core -o -name '*~' -o -name '.*~' -o -name '.#*' -o -name '*.so' \) -follow -type f -print

distclean: clean
	@find . \( -name '.depend' \) -type f -print -follow | xargs rm -f

uninstall: uninstall-sub_dirs
	#@rm -f $(patsubst $(ARRIS_DOCSIS_PREFIX)/include/%,$(TI_include)/%,$(arris-docsis-headers-list))
	@rm -f $(patsubst $(ARRIS_LIBS_PREFIX)/include/%,$(TI_include)/%,$(arris-libs-headers-list))
	@rm -f $(patsubst $(ARRIS_LIBS_PREFIX)/src/include/curl/%,$(TI_include)/curl/%,$(curl-headers-list))
	@rm -f $(patsubst $(ARRIS_LIBS_PREFIX)/src/include/tidy/%,$(TI_include)/tidy/%,$(tidy-headers-list))
	@rm -f $(patsubst gw_api/%,$(TI_include)/%,$(gw_header_list))

build_pre:

build_post:

