##*******************************************************************************
#*+--------------------------------------------------------------------------+**
#*|                            ****                                          |**
#*|                            ****                                          |**
#*|                            ******o***                                    |**
#*|                      ********_///_****                                   |**
#*|                      ***** /_//_/ ****                                   |**
#*|                       ** ** (__/ ****                                    |**
#*|                           *********                                      |**
#*|                            ****                                          |**
#*|                            ***                                           |**
#*|                                                                          |**
#*|         Copyright (c) 1998-2009 Texas Instruments Incorporated           |**
#*|                        ALL RIGHTS RESERVED                               |**
#*|                                                                          |**
#*| Permission is hereby granted to licensees of Texas Instruments           |**
#*| Incorporated (TI) products to use this computer program for the sole     |**
#*| purpose of implementing a licensee product based on TI products.         |**
#*| No other rights to reproduce, use, or disseminate this computer          |**
#*| program, whether in part or in whole, are granted.                       |**
#*|                                                                          |**
#*| TI makes no representation or warranties with respect to the             |**
#*| performance of this computer program, and specifically disclaims         |**
#*| any responsibility for any damages, special or consequential,            |**
#*| connected with the use of this program.                                  |**
#*|                                                                          |**
#*+--------------------------------------------------------------------------+**
#*******************************************************************************

# ******************************************************************************
# * FILE PURPOSE:    Puma5 System main Makefile
# ******************************************************************************
# * FILE NAME:       Makefile
# *
# * DESCRIPTION:     Puma5 System main Makefile
# *
# *****************************************************************************/

.PHONY: dummy

# subdirectories must be built and installed in exactly given order. so notparallel.
.NOTPARALLEL:

all: build install

-include $(TARGET_HOME)/TI_Build.make
-include $(TARGET_HOME)/.config

ifeq ($(DOCSIS_SOC), PUMA6)
subdir-y += src/uClibc src/busybox src/udev src/e2fsprogs/src
else
subdir-y += src/uClibc src/busybox src/udev
endif

SUB_DIRS = $(subdir-y)

config-list = $(patsubst %, _config_%, $(SUB_DIRS))
build-list = $(patsubst %, _build_%, $(SUB_DIRS))
install-list = $(patsubst %, _install_%, $(SUB_DIRS))
uninstall-list = $(patsubst %, _uninstall_%, $(SUB_DIRS))
clean-list = $(patsubst %, _clean_%, $(SUB_DIRS))
distclean-list = $(patsubst %, _distclean_%, $(SUB_DIRS))

config: dummy $(config-list)
build: dummy $(build-list)
# ARRIS MOD - add c++
install: install_system_headers $(install-list) install_libgcc install_libcpp install_libstdc++
uninstall: dummy $(uninstall-list)
clean: dummy $(clean-list)
distclean: dummy $(distclean-list)

ifdef SUB_DIRS
$(config-list) : dummy
	@$(MAKE) -C $(patsubst _config_%,%,$@) -f Makefile.flat config
endif

ifdef SUB_DIRS
$(build-list) : dummy
	@echo "*--------------------------------------------------"
	@echo "*  Building: $(patsubst _build_%,%,$@)"
	@echo "*--------------------------------------------------"
	@$(MAKE) -C $(patsubst _build_%,%,$@) -f Makefile.flat build
endif

ifdef SUB_DIRS
$(install-list) : dummy
	@$(MAKE) -C $(patsubst _install_%,%,$@) -f Makefile.flat install
endif

ifdef SUB_DIRS
$(uninstall-list) : dummy
	@$(MAKE) -C $(patsubst _uninstall_%,%,$@) -f Makefile.flat uninstall
endif

ifdef SUB_DIRS
$(clean-list) : dummy
	@$(MAKE) -C $(patsubst _clean_%,%,$@) -f Makefile.flat clean
endif

ifdef SUB_DIRS
$(distclean-list) : dummy
	@$(MAKE) -C $(patsubst _distclean_%,%,$@) -f Makefile.flat distclean
endif

install_system_headers:
	@install -p -m 0644 src/include/*.h $(TI_include)

# Find the toolchain's libgcc_s.so.1 file automatically
libgcc_s_path := $(shell find `$(CROSS)gcc -print-search-dirs | grep libraries | awk -F"libraries: =" {'print $$2'} | sed "s/:/ /g"` -type f -name libgcc_s.so.1 -print 2> /dev/null)
libcpp := $(shell ${CROSS}gcc --print-file-name=libstdc++.so.6)
install_libgcc:
ifdef libgcc_s_path
	@install -p $(firstword $(libgcc_s_path)) $(TI_filesystem_path)/lib
	@ln -sf libgcc_s.so.1 $(TI_filesystem_path)/lib/libgcc_s.so
else
	$(error Unable to find libgcc_s.so.1 in the compiler search path)
endif

# if configured we need to install toolchain's libstdc++.so.6 library
install_libcpp:
ifeq ($(CONFIG_TI_SYSTEM_SUPPORT_CPP),y)
ifdef libcpp
	@install -p $(firstword $(libcpp)) $(TI_filesystem_path)/lib
else
	$(error Unable to find libstdc++.so.6 in the compiler search path)
endif	 
endif

# ARRIS ADD - Find the toolchain's libstdc++.so.6.0.17 file automatically
# this is needed for MoCA
ifeq ($(CONFIG_VENDOR_ARRIS_ENTROPIC_MOCA),y)
libstdc++_path := $(shell find `$(CROSS)gcc -print-search-dirs | grep libraries | awk -F"libraries: =" {'print $$2'} | sed "s/:/ /g"` -type f -name libstdc++.so.6.0.17 -print 2> /dev/null)
install_libstdc++:
ifdef libstdc++_path
	@install -p $(firstword $(libstdc++_path)) $(TI_filesystem_path)/lib
	@ln -sf libstdc++.so.6.0.17 $(TI_filesystem_path)/lib/libstdc++.so
	@ln -sf libstdc++.so.6.0.17 $(TI_filesystem_path)/lib/libstdc++.so.6
else
	$(error Unable to find libstdc++.so.6.0.17 in the compiler search path)
endif
else
install_libstdc++:
endif
#END ARRIS ADD

package:
	@$(TARGET_HOME)/tools/scripts/mkpkg

dummy:

.PHONY: all config build install uninstall clean distclean
