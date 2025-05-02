.NOTPARALLEL:

TI_PRODUCT := $(PRODUCT_NAME)
export TI_BUILD_ROOT := $(TARGET_HOME)
TI_build_root := $(TARGET_HOME)

#-----------------------------------------
# Release specific Information
#-----------------------------------------
export TI_release_version := 1.0
export TI_kernel_version := linux-2.6.39.3
export RTOS := linux

# ARRIS ADD BEGIN : Use the new toolchain for the new kernel
export PATH := /usr/local/buildtools/ti-puma6_2014_ver3/usr/bin:$(PATH)
# ARRIS ADD END

#-----------------------------------------
# build specific defines (global)
#-----------------------------------------
export TI_tools_path := $(TARGET_HOME)/tools
export TI_kernel_path := $(TARGET_HOME)/kernel/ti
export TI_lib_path := $(TI_build_root)/ti/lib
export TI_thirdparty_root := $(TARGET_HOME)/ti/thirdparty
export KERNEL_DIR := $(TI_kernel_path)/$(TI_kernel_version)/src
export OPENSSL_LIB_DIR := $(TI_build_root)/ti/lib
export OPENSSL_SRC_DIR := $(TI_build_root)/ti/netdk/src/openssl
export UPNP_LIB_DIR := $(TI_build_root)/ti/lib
export UPNP_HDR_DIR := $(TI_build_root)/ti/netdk/src/upnp-ipc
export UPNP_SRC_DIR := $(TI_build_root)/ti/netdk/src/upnpd
export TI_make_path := $(TARGET_HOME)/ti/system/make
export TI_MASDK_CONFIG_PATH := ti/ncsdk/build/$(RTOS)

# ARRIS ADD BEGIN : define the envoy source directory
export ENVOY_DIR := envoy_linux_src
# ARRIS ADD END

#-----------------------------------------
# filesystem specific defines
#-----------------------------------------
export TI_bindir := /usr/bin
export TI_sbindir := /usr/sbin
export TI_etcdir := /etc
export TI_scriptsdir := /etc/scripts
export TI_datadir := /share
export TI_libdir := /usr/lib
export TI_mandir := /share/man
export TI_infodir := /share/info
export TI_includedir := /usr/include
export TI_docdir := /share/doc
export TI_rsbindir := /sbin
export TI_rusrdir := /usr
export TI_rlibdir := /lib
export TI_wwwdir := /usr/www
export TI_wwwsafedir := /usr/www_safe
export TI_cgidir := /usr/www/cgi-bin
export TI_htmldir := /usr/www/html
export TI_htmlsafedir := /usr/www_safe/html
export TI_imagedir := /usr/www/html/defs/style5
export TI_vardir := /var
export TI_vartmpdir := /var/tmp
export TI_varflashdir := /var/flash
export TI_vardevdir := /var/dev
export TI_varprocdir := /var/proc
export TI_vopdir := /vop

export TI_base := $(TARGET_HOME)
# This is where all file systems are created
export TI_filesystem_basepath := $(TI_base)/build/$(PRODUCT_NAME)/fs
# Name of System FS (includes at least busybox+uclibc+stuff like that)
export TI_filesystem_basefs_name := base_fs
export TI_filesystem_basefs_path := $(TI_filesystem_basepath)/$(TI_filesystem_basefs_name)
# Default value of filesystem path. 
# Components may change this by redefining, e.g.
#   export TI_filesystem_path := $(TI_filesystem_basepath)/my_component_fs
#   Everything under my_component_fs will be created as a separate file system
export TI_filesystem_path := $(TI_filesystem_basefs_path)
export TI_FS_PATH := $(TI_filesystem_path)
export TI_FS_LIB_PATH := $(TI_rlibdir)

# Allow override by environment, assign only if not defined
ifeq ($(TI_images_path),)
export TI_images_path := $(TI_base)/build/$(PRODUCT_NAME)/images
endif

#-----------------------------------------
# Tools specific defines
#----------------------------------------
export TI_include := $(TARGET_HOME)/ti/include
export TI_uclibc_include := $(TI_include)

#-----------------------------------------
# OpenSSL specific defines
#----------------------------------------
export CONFIG_TI_SSLOPTS := no-bf no-cast no-idea no-rc2 no-rc4 no-rc5 no-dsa no-md2 no-md4 no-mdc2 no-ripemd no-engines

#------------------------------------------
# Environment specific sets
#------------------------------------------

#TC_BUILDROOT_NAME := unknown
TC_BUILDROOT_NAME := buildroot

export CROSS_PREFIX := armeb-$(TC_BUILDROOT_NAME)-linux-uclibcgnueabi
ifndef CROSS
# Find out which BE compilers are available (MontaVista / Free Software Foundation)
BEGCC_MV := $(shell which arm_v6_be_uclibc-gcc 2>/dev/null)
BEGCC_FSF := $(shell which armeb-$(TC_BUILDROOT_NAME)-linux-uclibcgnueabi-gcc 2>/dev/null)



# Prefer MV compiler. If not availble, use FSF
ifneq ($(BEGCC_MV),)
BEGCC := $(BEGCC_MV)
export CROSS := arm_v6_be_uclibc-
else
ifneq ($(BEGCC_FSF),)
BEGCC := $(BEGCC_FSF)
export CROSS := $(CROSS_PREFIX)-
endif
endif

ifneq ($(CROSS),)
# Find out which LE compilers are available (MontaVista / Free Software Foundation)
LEGCC_MV := $(shell which arm_v6_le_uclibc-gcc 2>/dev/null)
LEGCC_FSF := $(shell which armeb-$(TC_BUILDROOT_NAME)-linux-uclibcgnueabi-gcc 2>/dev/null)

# Prefer MV compiler. If not availble, use FSF
ifneq ($(LEGCC_MV),)
LEGCC := $(LEGCC_MV)
export CROSS := arm_v6_le_uclibc-
else
ifneq ($(LEGCC_FSF),)
LEGCC := $(LEGCC_FSF)
export CROSS := $(CROSS_PREFIX)-
endif
endif
endif

else # defined $(CROSS)
UNKNOWN_GCC := $(shell which $(CROSS)gcc 2>/dev/null)

ifneq ($(UNKNOWN_GCC),)
# Try to guess the endian
TEST_BEGCC1 := $(findstring be,$(CROSS))
TEST_BEGCC2 := $(findstring armeb-,$(CROSS))

ifneq ($(TEST_BEGCC1),)
BEGCC := $(UNKNOWN_GCC)
else
ifneq ($(TEST_BEGCC2),)
BEGCC := $(UNKNOWN_GCC)
endif
endif

TEST_LEGCC1 := $(findstring le,$(CROSS))
TEST_LEGCC2 := $(findstring arm-,$(CROSS))

ifneq ($(TEST_LEGCC1),)
LEGCC := $(UNKNOWN_GCC)
else
ifneq ($(TEST_LEGCC2),)
LEGCC := $(UNKNOWN_GCC)
endif
endif

endif # $(UNKNOWN_GCC)
endif # $(CROSS)

export CC := $(CROSS)gcc
export LD := $(CROSS)ld
export AR := $(CROSS)ar
export SEVENZIP := $(TARGET_HOME)/tools/bin/7zip
export CROSS_COMPILE := $(CROSS)
export CROSS_TOOLS := $(CROSS)

#------------------------------------------
# Compilation and Linking flags
#------------------------------------------
# ARRIS ADD : Just set these flags once
ifndef TI_BUILD_FLAGS
export TI_BUILD_FLAGS = 1

# ARRIS CHANGE - Add ncsdk path
export CFLAGS += -I$(TI_include) \
                 -I$(TI_include)/asm-arm/arch-avalanche/generic \
                 -I$(TI_include)/masdk \
                 -I$(TI_include)/ncsdk
# END ARRIS CHANGE

ifneq ($(CONFIG_INTEL_SYSTEM_STACK_BACKTRACE_MINIMAL),)
    # Minimal support - juts leave everything the way it is 
else ifneq ($(CONFIG_INTEL_SYSTEM_STACK_BACKTRACE_BASIC),)
    # Provide basic symbols for backtrace
    export CFLAGS += -funwind-tables -rdynamic 
else ifneq ($(CONFIG_INTEL_SYSTEM_STACK_BACKTRACE_FULL),)
    # Full symbolic backtrace 
    # TBD
endif


export LDFLAGS += -L$(TI_lib_path) \
                  -L$(TI_filesystem_path)$(TI_rlibdir)
#------------------------------------------
# Thumb mode
#------------------------------------------
ifdef CONFIG_ARM_THUMB
export TI_cflags_ARMThumb = -mthumb -mthumb-interwork
export CFLAGS += $(TI_cflags_ARMThumb)
endif

endif
# END ARRIS ADD - Just define once

#------------------------------------------
# Default compilation rule
#------------------------------------------
%.o: %.c
	@echo "  CC [C] 	$(notdir $@)"
	@$(CC) $(CFLAGS) -o $@ -c $<
