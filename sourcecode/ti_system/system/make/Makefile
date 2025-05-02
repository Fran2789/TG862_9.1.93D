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
#**|         Copyright (c) 1998-2007 Texas Instruments Incorporated           |**
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
# Top level system Makefile
#

-include $(TARGET_HOME)/.config
-include $(TARGET_HOME)/release.mak

# This Makefile requires bash shell, others may be incompatible
export SHELL:=/bin/bash

#CROSS set in TI_Build.make
ifdef CONFIG_TI_SYSTEM_BOARD_NAME
export BOARD:=$(CONFIG_TI_SYSTEM_BOARD_NAME)
endif
export DEBUG
export PRODUCT PRODUCT_NAME DOCSIS_SOC DOCSIS_SOC_NAME
export TEMP_BUILD_DIR:=$(TARGET_HOME)/`whoami`_`date +%N`
export BUILD_SUFFIX

include $(TARGET_HOME)/ti/system/make/TI_Build.make

export CROSS_TOOLS := $(CROSS)
package_files := package.bld package.xdc Settings.xdc

default: help

.NOTPARALLEL:

# The complete list of components:
SYS_$(SKIP_SYSTEM)COMPONENTS:= tools psp_uboot linux system netdk psp_boot_ram

# ----------------------------------------------------------------------------- #
# ARRIS ADD: BEGIN  Need to add the gw to the SPLIT_APP_COMPONENTS
#             and gw_lib/gw_bin to the APP_COMPONENTS VARIABLE
ifeq ($(CONFIG_VENDOR_ARRIS_CM_TR69),y)
ifeq (,$(findstring gw_lib , ${APP_COMPONENTS}))
APP_COMPONENTS := $(patsubst common_post, gw_bin gw_lib common_post, ${APP_COMPONENTS})
endif
ifeq (,$(findstring gw , ${SPLIT_APP_COMPONENTS}))
SPLIT_APP_COMPONENTS += gw
endif
endif
# ARRIS ADD: END
COMPONENTS := $(SYS_COMPONENTS) $(APP_COMPONENTS)

# List of standard targets
targets := config build install uninstall clean distclean package

# Exceptions to components locations (default is ti/comp_name)
tools_home := tools
linux_home := kernel/ti/linux-2.6.39.3
thirdparty_home := $(TI_thirdparty_root)

# Exceptions to generic config target
config_EXCEPTIONS += system linux netdk

# Exceptions to generic build target
build_EXCEPTIONS +=	psp_uboot

# Exceptions to generic install target
install_EXCEPTIONS += linux

# Exceptions to generic distclean target
distclean_EXCEPTIONS +=

# Exceptions to generic package target
package_EXCEPTIONS +=

-include $(TI_thirdparty_root)/thirdparty.mak

# Exception for system config
linux_headers  := $(TI_include)/linux/.install
uclibc_headers := $(TI_include)/stdio.h
$(linux_headers) :
	@echo === $@ ===
	@mkdir -p $(TI_include)
	make -C  $(TARGET_HOME)/$(linux_home)/src/ headers_install ARCH=arm  INSTALL_HDR_PATH=$(TARGET_HOME)/ti

$(uclibc_headers) : $(linux_headers)
	@echo === $@ ===
	@mkdir -p $(TI_include)
	make -C  ti/system/src/uClibc -f Makefile.flat headers
system_config:$(linux_headers) $(uclibc_headers)
	@echo === $@ ===
	@mkdir -p $(TI_include)
	@#ARRIS CHANGE - preserve timestamps
	@if [ ! -e "./ti/include/autoconf.h" ]; then \
	   ln -sf ../../build/$(PRODUCT_NAME)/configs/autoconf.h ./ti/include/. ;\
	fi;
	@if [ ! -e "./ti/include/puma_autoconf.h" ]; then \
	   ln -sf ../../build/$(PRODUCT_NAME)/configs/puma_autoconf.h ./ti/include/. ;\
	fi;
	@#END ARRIS CHANGE
	@ln -sf ../../build/$(PRODUCT_NAME)/configs/release.h ./ti/include/.
	@mkdir -p $(TARGET_HOME)/ti/include/linux/
	@if [ -f build/$(PRODUCT_NAME)/configs/vendor_release.h ]; then \
		ln -sf ../../build/$(PRODUCT_NAME)/configs/vendor_release.h $(TARGET_HOME)/ti/include/. ;\
	fi;

	@echo APP_COMPONENTS: ${APP_COMPONENTS} SPLIT_APP_COMPONENTS: ${SPLIT_APP_COMPONENTS} "######### in system Makefile ###########"

	@#ARRIS add to avoid a common compile error
	@if [ ! -e "$(TARGET_HOME)/ti/include/_tistdtypes.h" ]; then \
	   ln -sf $(TARGET_HOME)/kernel/ti/linux-2.6.39.3/src/include/asm-arm/arch-avalanche/generic/_tistdtypes.h $(TARGET_HOME)/ti/include/. ;\
	fi;

# Exception for linux config
linux_config: $(TARGET_HOME)/.config
	@echo === $@ ===
	@if [ ! -d $(TI_filesystem_basepath) ]; then \
		echo "*** $(TI_filesystem_basepath) not exist";  \
		if [ -f $(TI_filesystem_basepath).tgz ]; then \
			echo "*** Restore $(TI_filesystem_basepath)";  \
			cd $(TI_base)/build/$(PRODUCT_NAME); \
			tar xf fs.tgz;	\
		fi; \
	fi;
	@$(MAKE) -C $(linux_home) config 2> /dev/null


linux_install:
	@echo === $@ ===
	@$(MAKE) -C $(linux_home) install


# ---------------------------
# DO NOT EDIT BELOW THIS LINE
# ---------------------------
gethome = $(patsubst file,$($(1)_home),$(patsubst undefined,ti/$(1),$(origin $(1)_home)))
getmake = $(patsubst file,$($(1)_makefile),$(patsubst undefined,Makefile,$(origin $(1)_makefile)))
getopts = $(patsubst file,$($(1)_option),$(patsubst undefined,y,$(origin $(1)_option)))

define mktarget
$(1)$(2): $(1)_lib$(2) $(1)_bin$(2)
endef
targets = _config _clean _distclean _install _uninstall

# Backward compatibilty with non-splitted targets
define SPLIT_APP_COMPONENTS_template
$(1): $(1)_lib $(1)_bin
$(foreach target,$(targets),$(eval $(call mktarget,$(1),$(target))))
$(1)_package: $(1)_lib_package
package_EXCEPTIONS += $(1)_bin
$(1)_lib_home := ti/$(1)
$(1)_bin_home := ti/$(1)
$(1)_lib_params := BUILD_POST=n
$(1)_bin_params := BUILD_POST=y
endef

$(foreach component,$(SPLIT_APP_COMPONENTS),$(eval $(call SPLIT_APP_COMPONENTS_template,$(component))))

AVAIL_COMPS := $(foreach comp,$(COMPONENTS),$(shell if [ -e $(call gethome,$(comp))/$(call getmake,$(comp)) ]; then echo $(comp); fi))
BUILD_COMPS := $(foreach comp,$(AVAIL_COMPS),$(shell if [ "$(call getopts,$(comp))" == "y" ]; then echo $(comp); fi))
CLEAN_COMPS := $(shell set t=""; for i in $(AVAIL_COMPS); do t="$$i $$t"; done; echo $$t; unset $$t)

generic-all-list := $(filter-out $(all_EXCEPTIONS),$(BUILD_COMPS))
exceptions-all-list := $(filter $(all_EXCEPTIONS),$(BUILD_COMPS))
generic-config-list := $(patsubst %,%_config,$(filter-out $(config_EXCEPTIONS),$(BUILD_COMPS)))
exceptions-config-list := $(patsubst %,%_config,$(filter $(config_EXCEPTIONS),$(BUILD_COMPS)))
generic-build-list := $(patsubst %,%_build,$(filter-out $(build_EXCEPTIONS),$(BUILD_COMPS)))
exceptions-build-list := $(patsubst %,%_build,$(filter $(build_EXCEPTIONS),$(BUILD_COMPS)))
generic-install-list := $(patsubst %,%_install,$(filter-out $(install_EXCEPTIONS),$(BUILD_COMPS)))
exceptions-install-list := $(patsubst %,%_install,$(filter $(install_EXCEPTIONS),$(BUILD_COMPS)))
generic-uninstall-list := $(patsubst %,%_uninstall,$(filter-out $(uninstall_EXCEPTIONS),$(CLEAN_COMPS)))
exceptions-uninstall-list := $(patsubst %,%_uninstall,$(filter $(uninstall_EXCEPTIONS),$(CLEAN_COMPS)))
generic-clean-list := $(patsubst %,%_clean,$(filter-out $(clean_EXCEPTIONS),$(CLEAN_COMPS)))
exceptions-clean-list := $(patsubst %,%_clean,$(filter $(clean_EXCEPTIONS),$(CLEAN_COMPS)))
generic-distclean-list := $(patsubst %,%_distclean,$(filter-out $(distclean_EXCEPTIONS),$(CLEAN_COMPS)))
exceptions-distclean-list := $(patsubst %,%_distclean,$(filter $(distclean_EXCEPTIONS),$(CLEAN_COMPS)))
generic-package-list := $(patsubst %,%_package,$(filter-out $(package_EXCEPTIONS),$(CLEAN_COMPS)))
exceptions-package-list := $(patsubst %,%_package,$(filter $(package_EXCEPTIONS),$(CLEAN_COMPS)))

kernel: linux
$(PRODUCT_NAME): dummy $(exceptions-config-list) $(generic-config-list) $(generic-all-list) $(exceptions-all-list) filesystem
config: dummy $(exceptions-config-list) $(generic-config-list)
build: dummy $(generic-build-list) $(exceptions-build-list)
install: dummy $(generic-install-list) $(exceptions-install-list)
uninstall: dummy $(generic-uninstall-list) $(exceptions-uninstall-list)
clean: dummy $(generic-clean-list) $(exceptions-clean-list)
distclean: dummy product_setup $(generic-distclean-list) $(exceptions-distclean-list) final_distclean
package: $(linux_headers) $(uclibc_headers) dummy product_setup $(exceptions-package-list) $(generic-package-list) $(PRODUCT_NAME)_package

parallel-ignore := docsis*
#[igor]parallel-ignore := docsis*|pacm*|common_voice_utils*|masdk*

ifdef generic-all-list
$(generic-all-list) : dummy
	@$(MAKE) $@_config
	@$(MAKE) $@_build
	@$(MAKE) $@_install
endif

ifdef generic-config-list
$(generic-config-list) : dummy
	@echo === $@ ===
	@if [ ! -d $(TI_filesystem_basepath) ]; then \
		echo "*** $(TI_filesystem_basepath) not exist";  \
		if [ -f $(TI_filesystem_basepath).tgz ]; then \
			echo "*** Restore $(TI_filesystem_basepath)";  \
			cd $(TI_base)/build/$(PRODUCT_NAME); \
			tar xf fs.tgz;	\
		fi; \
	fi;
	$(MAKE) `case $@ in $(parallel-ignore)) echo -j1 ;; esac` -C $(call gethome,$(patsubst %_config,%,$@)) -f $(call getmake,$(patsubst %_config,%,$@)) config $($(patsubst %_config,%,$@)_params)
endif

ifdef generic-build-list
$(generic-build-list) : dummy
	@echo "*--------------------------------------------------"
	@echo "*  TI $(PRODUCT) Distribution                           "
	@echo "*  Build date: $(shell date -R) :                  "
	@echo "*  Building: $@                                    "
	@echo "*--------------------------------------------------"
	@$(MAKE) `case $@ in $(parallel-ignore)) echo -j1 ;; esac` -C $(call gethome,$(patsubst %_build,%,$@)) -f $(call getmake,$(patsubst %_build,%,$@)) build $($(patsubst %_build,%,$@)_params)
endif

ifdef generic-install-list
$(generic-install-list) : dummy
	@echo === $@ ===
	@$(MAKE) `case $@ in $(parallel-ignore)) echo -j1 ;; esac` -C $(call gethome,$(patsubst %_install,%,$@)) -f $(call getmake,$(patsubst %_install,%,$@)) install $($(patsubst %_install,%,$@)_params)
endif

ifdef generic-uninstall-list
$(generic-uninstall-list) : dummy
	@echo === $@ ===
	@$(MAKE) -C $(call gethome,$(patsubst %_uninstall,%,$@)) -f $(call getmake,$(patsubst %_uninstall,%,$@)) uninstall $($(patsubst %_uninstall,%,$@)_params)
endif

ifdef generic-clean-list
$(generic-clean-list) : dummy
	@echo === $@ ===
	@$(MAKE) -C $(call gethome,$(patsubst %_clean,%,$@)) -f $(call getmake,$(patsubst %_clean,%,$@)) clean $($(patsubst %_clean,%,$@)_params)
endif

ifdef generic-distclean-list
$(generic-distclean-list) : dummy
	@echo === $@ ===
	@$(MAKE) -C $(call gethome,$(patsubst %_distclean,%,$@)) -f $(call getmake,$(patsubst %_distclean,%,$@)) distclean $($(patsubst %_distclean,%,$@)_params)
endif

ifdef generic-package-list
$(generic-package-list) : dummy	product_setup
	@echo === $@ ===
	@$(MAKE) -C $(call gethome,$(patsubst %_package,%,$@)) -f $(call getmake,$(patsubst %_package,%,$@)) package
	@mv $(call gethome,$(patsubst %_package,%,$@))/$(patsubst %_lib,%,$(patsubst %_package,%,$@))[\.\-]*.tgz $(TARGET_HOME)/build/$(PRODUCT_NAME)/
endif

dummy:

help:
ifdef PRODUCT
ifdef PRODUCT_NAME
	@echo "-----------------------------------------------------------------------------"
	@echo "Available targets are:"
	@echo "-----------------------------------------------------------------------------"
	@echo "	xconfig/menuconfig	- to configure"
	@echo "	$(PRODUCT_NAME)			- to build $(PRODUCT)"
	@echo "	xbuild			- to configure and build"
	@echo
	@echo "You can also use following targets:"
	@echo "Components:"
	@echo "	" $(AVAIL_COMPS)
	@echo
	@echo "Targets:"
	@echo "	" $(targets)
	@echo
	@echo "-----------------------------------------------------------------------------"
	@echo "Components+targets:"
	@echo "-----------------------------------------------------------------------------"
	@echo -e "Configuration targets:\n"$(patsubst %,%_config,$(AVAIL_COMPS)) $(patsubst %,%_config,$(SPLIT_APP_COMPONENTS))
	@echo -e "\nBuild targets:\n"$(patsubst %,%_build,$(AVAIL_COMPS)) $(patsubst %,%_build,$(SPLIT_APP_COMPONENTS))
	@echo -e "\nInstall targets:\n"$(patsubst %,%_install,$(AVAIL_COMPS)) $(patsubst %,%_install,$(SPLIT_APP_COMPONENTS))
	@echo -e "\nUninstall targets:\n"$(patsubst %,%_uninstall,$(AVAIL_COMPS)) $(patsubst %,%_uninstall,$(SPLIT_APP_COMPONENTS))
	@echo -e "\nClean targets:\n"$(patsubst %,%_clean,$(AVAIL_COMPS)) $(patsubst %,%_clean,$(SPLIT_APP_COMPONENTS))
	@echo -e "\nDistclean targets:\n"$(patsubst %,%_distclean,$(AVAIL_COMPS)) $(patsubst %,%_distclean,$(SPLIT_APP_COMPONENTS))
	@echo -e "\nPackaging targets:\n"$(patsubst %,%_package,$(AVAIL_COMPS))
	@echo " "
else
	@echo Error: PRODUCT_NAME was not defined neither in Makefile nor in .prod file, aborting.
	@exit 1
endif
else
	@echo Error: PRODUCT was not defined neither in Makefile nor in .prod file, aborting.
	@exit 1
endif

.PHONY: dummy product_setup all kernel $(PRODUCT_NAME) config build install clean distclean package
.PHONY: $(generic-all-list) $(exceptions-all-list) $(generic-config-list) $(exceptions-config-list)
.PHONY: $(generic-build-list) $(exceptions-build-list) $(generic-install-list) $(exceptions-install-list)
.PHONY: $(generic-uninstall-list) $(exceptions-uninstall-list) $(generic-clean-list) $(exceptions-clean-list)
.PHONY: $(generic-distclean-list) $(exceptions-distclean-list) $(generic-package-list) $(exceptions-package-list)

KCONFIG_BIN:=$(TARGET_HOME)/tools/bin
KCONFIG_CFG=$(TARGET_HOME)/build/$(PRODUCT_NAME)/configs/
KCONFIG_OPT=-p $(KERNEL_DIR) -p $(TARGET_HOME)/ti/system/src/uClibc -p $(TARGET_HOME)/ti/system/src/busybox -u $(KCONFIG_CFG)/TI_unselect
KCONFIG_FILE=$(KCONFIG_CFG)/TI_$(PRODUCT).Kconfig

# Set the default system configuration according to the SoC type
ifeq ($(DOCSIS_SOC), PUMA5)
DEFCONFIG_FILE=$(KCONFIG_CFG)/defconfig
else
DEFCONFIG_FILE=$(KCONFIG_CFG)/defconfig_$(DOCSIS_SOC_NAME)
endif

#QUIET_CONF := > /dev/null 2> /dev/null

all: $(PRODUCT_NAME)

product_setup:
	@ln -sf ./ti/system/make/TI_Build.make ./
	@if [ ! -d $(TARGET_HOME)/build/$(PRODUCT_NAME) ]; then \
		echo Invalid product selected \($(PRODUCT)\), aborting.; \
		exit 1; \
	fi
	@echo PRODUCT=$(PRODUCT) > $(TARGET_HOME)/.prod
	@echo PRODUCT_NAME=$(PRODUCT_NAME) >> $(TARGET_HOME)/.prod
	@echo DOCSIS_SOC=$(DOCSIS_SOC) >> $(TARGET_HOME)/.prod

conf_title:	product_setup
	@echo "*--------------------------------------------------"
	@echo "*  Intel $(PRODUCT) Distribution"
	@echo "*  SoC: $(DOCSIS_SOC)"
	@echo "*  Build date: $(shell date -R) :"
	@echo "*  Creating product configuration, please wait..."
	@echo "*--------------------------------------------------"

defconfig: conf_title
	@echo Compiling system tools...
	@$(MAKE) tools $(QUIET_CONF)
	@rm -f $(TARGET_HOME)/.config
	@cp $(DEFCONFIG_FILE) $(TARGET_HOME)/.config ;\
	$(buildrun) $(QUIET_CONF)
	@$(MAKE) .config.endian	-s
	@echo Configuring system...
	@$(TARGET_HOME)/tools/scripts/fix_arm_offsets.sh tmp $(TARGET_HOME)/.config
	@$(KCONFIG_BIN)/conf -s $(KCONFIG_OPT) $(KCONFIG_FILE)
	@if [ -f $(TARGET_HOME)/autoconf.h ]; then \
    	mv autoconf.h auto.conf $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs ;\
	fi
	@echo Done.

# ARRIS ADD
buildconfig: conf_title
	@echo Compiling system tools...
	@$(MAKE) tools $(QUIET_CONF)
	@rm -f $(TARGET_HOME)/.config
	@cp $(KCONFIG_CFG)/buildconfig $(TARGET_HOME)/.config ;\
	$(buildrun) $(QUIET_CONF)
	@$(MAKE) .config.endian -s
	@echo Configuring system...
	@$(TARGET_HOME)/tools/scripts/fix_arm_offsets.sh tmp $(TARGET_HOME)/.config
	@echo Done.
# END ARRIS ADD

oldconfig: conf_title
	@echo Compiling system tools...
	@$(MAKE) tools $(QUIET_CONF)
	@if [ ! -f $(TARGET_HOME)/.config ]; then \
	  cp $(DEFCONFIG_FILE) $(TARGET_HOME)/.config ;\
	fi
	@$(MAKE) .config.endian	-s
	@echo Configuring system...
	@$(TARGET_HOME)/tools/scripts/fix_arm_offsets.sh tmp $(TARGET_HOME)/.config
	$(KCONFIG_BIN)/conf -s $(KCONFIG_OPT) $(KCONFIG_FILE)
	@if [ -f $(TARGET_HOME)/autoconf.h ]; then \
    	mv autoconf.h auto.conf $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs ;\
	fi
	@echo Done.

xconfig: conf_title
	@echo Compiling system tools...
	@$(MAKE) tools $(KCONFIG_BIN)/qconf $(QUIET_CONF)
	@if [ ! -f $(TARGET_HOME)/.config ]; then \
	  cp $(DEFCONFIG_FILE) $(TARGET_HOME)/.config ;\
	fi
	@$(MAKE) .config.endian -s
	@echo Configuring system...
	$(KCONFIG_BIN)/qconf $(KCONFIG_OPT) $(KCONFIG_FILE)
	@$(TARGET_HOME)/tools/scripts/fix_arm_offsets.sh tmp $(TARGET_HOME)/.config
	$(KCONFIG_BIN)/conf -s $(KCONFIG_OPT) $(KCONFIG_FILE)
	@if [ -f $(TARGET_HOME)/autoconf.h ]; then \
    	mv autoconf.h auto.conf $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs ;\
	fi
	@echo Done.

menuconfig: conf_title
	@echo Compiling system tools...
	@$(MAKE) tools $(QUIET_CONF)
	@if [ ! -f $(TARGET_HOME)/.config ]; then \
	  cp $(DEFCONFIG_FILE) $(TARGET_HOME)/.config ;\
	fi
	@$(MAKE) .config.endian	-s
	@echo Configuring system...
	$(KCONFIG_BIN)/mconf $(KCONFIG_OPT) $(KCONFIG_FILE) $(TARGET_HOME)/.config
	$(TARGET_HOME)/tools/scripts/fix_arm_offsets.sh tmp $(TARGET_HOME)/.config
	$(KCONFIG_BIN)/conf -s $(KCONFIG_OPT) $(KCONFIG_FILE)
	@if [ -f $(TARGET_HOME)/autoconf.h ]; then \
    	mv autoconf.h auto.conf $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs ;\
	fi
	@echo Done.

xbuild: conf_title
	@echo Compiling system tools...
	@$(MAKE) tools $(KCONFIG_BIN)/qconf $(QUIET_CONF)
	@echo $@:
	@$(RM) -f .qbuild
	@if [ ! -f $(TARGET_HOME)/.config ]; then \
	  cp $(DEFCONFIG_FILE) $(TARGET_HOME)/.config ;\
	fi
	@$(MAKE) .config.endian -s
	@echo Configuring system...
	$(KCONFIG_BIN)/qbuild $(KCONFIG_OPT) $(KCONFIG_FILE)
	@$(TARGET_HOME)/tools/scripts/fix_arm_offsets.sh tmp $(TARGET_HOME)/.config
	$(KCONFIG_BIN)/conf -s $(KCONFIG_OPT) $(KCONFIG_FILE)
	@if [ -f $(TARGET_HOME)/autoconf.h ]; then \
    	mv autoconf.h auto.conf $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs ;\
	fi

# ARRIS ADD - Start
sipconfigB: conf_title
	@echo Compiling system tools...
	@$(MAKE) tools $(QUIET_CONF)
	@rm -f $(TARGET_HOME)/.config
	@cp $(KCONFIG_CFG)/sipconfig $(TARGET_HOME)/.config ;\
	$(buildrun) $(QUIET_CONF)
	@$(MAKE) .config.endian	-s
	@echo Configuring system...
	@$(TARGET_HOME)/tools/scripts/fix_arm_offsets.sh tmp $(TARGET_HOME)/.config
	$(KCONFIG_BIN)/conf -s $(KCONFIG_OPT) $(KCONFIG_FILE)
	@if [ -f $(TARGET_HOME)/autoconf.h ]; then \
    	mv autoconf.h auto.conf $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs ;\
	fi
	@echo Done.

pc15config: conf_title
	@echo Compiling system tools...
	@$(MAKE) tools $(QUIET_CONF)
	@rm -f $(TARGET_HOME)/.config
	@cp $(KCONFIG_CFG)/defconfig $(TARGET_HOME)/.config ;\
	@sed -i 's/CONFIG_TI_PACM_PC_1_0_EVENTS=y/CONFIG_TI_PACM_PC_1_0_EVENTS=n/' $(TARGET_HOME)/.config; \
	$(buildrun) $(QUIET_CONF)
	@$(MAKE) .config.endian -s
	@echo Configuring system...
	@$(TARGET_HOME)/tools/scripts/fix_arm_offsets.sh tmp $(TARGET_HOME)/.config
	$(KCONFIG_BIN)/conf -s $(KCONFIG_OPT) $(KCONFIG_FILE)
	@if [ -f $(TARGET_HOME)/autoconf.h ]; then \
	mv autoconf.h auto.conf $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs ;\
	fi
	@echo Done.

wbm750config: conf_title
	@echo Compiling system tools...
	@$(MAKE) tools $(QUIET_CONF)
	@rm -f $(TARGET_HOME)/.config
	@cp $(KCONFIG_CFG)/defconfig $(TARGET_HOME)/.config ;\
	sed -i 's/CONFIG_VENDOR_ARRIS_DOWNLOAD_TYPE=0x106/CONFIG_VENDOR_ARRIS_DOWNLOAD_TYPE=0x105/' $(TARGET_HOME)/.config; \
	sed -i 's/CONFIG_USB_MUSB_PERIPHERAL=y/CONFIG_USB_MUSB_PERIPHERAL=n/' $(TARGET_HOME)/.config; \
	sed -i 's/CONFIG_USB_GADGET_MUSB_HDRC=y/CONFIG_USB_GADGET_MUSB_HDRC=n/' $(TARGET_HOME)/.config; \
	sed -i 's/CONFIG_USB_TI_CPPI_DMA=y/CONFIG_USB_TI_CPPI_DMA=n/' $(TARGET_HOME)/.config; \
	sed -i 's/CONFIG_USB_GADGET=y/CONFIG_USB_GADGET=n/' $(TARGET_HOME)/.config; \
	sed -i 's/CONFIG_USB_GADGET_SELECTED=y/CONFIG_USB_GADGET_SELECTED=n/' $(TARGET_HOME)/.config; \
	sed -i 's/CONFIG_USB_GADGET_DUALSPEED=y/CONFIG_USB_GADGET_DUALSPEED=n/' $(TARGET_HOME)/.config; \
	sed -i 's/CONFIG_USB_ETH=y/CONFIG_USB_ETH=n/' $(TARGET_HOME)/.config; \
	sed -i 's/CONFIG_USB_ETH_RNDIS=y/CONFIG_USB_ETH_RNDIS=n/' $(TARGET_HOME)/.config; \
	sed -i 's/CONFIG_USB_MUSB_HDRC=y/CONFIG_USB_MUSB_HDRC=n/' $(TARGET_HOME)/.config; \
	$(buildrun) $(QUIET_CONF)
	@$(MAKE) .config.endian -s
	@echo Configuring system...
	@$(TARGET_HOME)/tools/scripts/fix_arm_offsets.sh tmp $(TARGET_HOME)/.config
	$(KCONFIG_BIN)/conf -s $(KCONFIG_OPT) $(KCONFIG_FILE)
	@if [ -f $(TARGET_HOME)/autoconf.h ]; then \
	mv autoconf.h auto.conf $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs ;\
	fi
	@echo Done.


sipconfig:
	@$(MAKE) sipconfigB PRODUCT=VSDK

sip: sipconfig 
	@$(MAKE) vsdk

pc15: 
	@$(MAKE) pc15config PRODUCT=VSDK
	@$(MAKE) vsdk

ncs: 
	@$(MAKE) defconfig PRODUCT=VSDK
	@$(MAKE) vsdk   
    
wbm750: 
	@$(MAKE) wbm750config PRODUCT=DSDK
	@$(MAKE) dsdk   
    
# ARRIS ADD - End 

$(KCONFIG_BIN)/qconf:
	$(error tools/bin/qconf was not built, Qt library is probably missing. Please use defconfig/menuconfig targets instead)

awksety = $(foreach x,$(AWKY),/$(x)/ {print "$(x)=y"; next})
awkunset = $(foreach x,$(AWKUN),/$(x)/ {print "\# $(x) is not set"; next})
awkrun = awk '$(awksety) $(awkunset) {print $$0}'

ifdef BEGCC
AWKY := ARCH_BIG_ENDIAN
AWKUN := CONFIG_CPU_LITTLE_ENDIAN ARCH_LITTLE_ENDIAN
else
ifdef LEGCC
AWKY := CONFIG_CPU_LITTLE_ENDIAN ARCH_LITTLE_ENDIAN
AWKUN := ARCH_BIG_ENDIAN
endif
endif

.config.endian:
# modify the following conditional to toggle automatic endian processing
ifeq (1,1)
	@echo Cross compiler configuration:
	@if [[ -n "$(BEGCC)" && -n "$(LEGCC)" ]]; then \
	  echo Two different endian compilers found. ;\
	  echo $(BEGCC) ;\
	  echo $(LEGCC) ;\
	  echo Defaulting to big endian cross compiler \($(CROSS)\). ;\
	  $(awkrun) .config > $@ ;\
	  mv -f $@ .config ;\
	elif [[ -n "$(CROSS)" && -z "$(BEGCC)" && -z "$(LEGCC)" ]]; then \
	  echo CROSS defined but no compiler has been found. ;\
	  echo Please set BEGCC or LEGCC to indicate endian of your toolchain;\
	  echo or add toolchain to PATH. ;\
	  exit 1 ;\
	elif [[ -z "$(CROSS)" && -z "$(BEGCC)" && -z "$(LEGCC)" ]]; then \
	  echo No ARM compilers found. Add one to PATH and reconfigure before building. ;\
	  exit 1 ;\
	else \
	  if [[ -n "$(BEGCC)" ]]; then \
	    echo Using Big-Endian cross compiler \($(CROSS)gcc\).;\
	  else \
	    echo Using Little-Endian cross compiler \($(CROSS)gcc\). ;\
	  fi ;\
	  $(awkrun) .config > $@ ;\
	  mv -f $@ .config ;\
	fi
endif

.PHONY: .config.endian
.DELETE_ON_ERROR: .config.endian

#-----------------------------------------------
# filesystem build
#-----------------------------------------------
filesystem:
# ARRIS ADD
ifeq ($(CONFIG_ARRIS_SYSTEM_GDBSERVER),y)
	@install -m 0755 $(TARGET_HOME)/tools/src/gdbserver/gdbserver $(TI_filesystem_path)/usr/sbin
endif
# END ARRIS ADD
	@$(MAKE) -C $(TI_make_path) -f Makefile.image img_complete TEMP_BUILD_DIR=$(TEMP_BUILD_DIR)

final_distclean:
# ARRIS ADD
	@$(RM) $(TI_filesystem_path)/usr/sbin/gdbserver
# END ARRIS ADD
ifneq ($(SKIP_SYSTEM),y)
	@$(RM) $(TARGET_HOME)/.config*
	@$(RM) $(TARGET_HOME)/.qbuild
	@$(RM) $(TI_images_path)/*.img
	@$(RM) $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs/autoconf.h
	@$(RM) $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs/BB_autoconf.h
	@$(RM) $(TARGET_HOME)/build/$(PRODUCT_NAME)/configs/uClibc_autoconf.h
	@$(RM) $(TARGET_HOME)/.prod
	@$(RM) $(TARGET_HOME)/release.mak
	@$(RM) $(TARGET_HOME)/TI_Build.make
	$(foreach package_file,$(package_files),$(shell rm -rf $(package_file)))
# This needs to be fixed later on, when all components do proper uninstall
	@$(RM) -r $(TARGET_HOME)/build/$(PRODUCT_NAME)/fs/*
endif

$(PRODUCT_NAME)_package:
	$(foreach package_file,$(package_files),$(shell cp $(TARGET_HOME)/build/$(PRODUCT_NAME)/package/.$(package_file).$(PRODUCT_NAME) $(package_file)))
	@echo PRODUCT=$(PRODUCT) > $(TARGET_HOME)/.prod
	@echo PRODUCT_NAME=$(PRODUCT_NAME) >> $(TARGET_HOME)/.prod
	$(TARGET_HOME)/tools/scripts/mkpkg -s -d . -o $(TARGET_HOME)/build/$(PRODUCT_NAME)/
