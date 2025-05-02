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

# Note: This file should not be changed!
#
# Maintenance and support: Hai Shalom, hai@ti.com
#
# General purpose variables:
#
# COMPONENT_INCLUDES - General component include directives
# COMPONENT_PREFIX - Component root directory
# SKIP_BUILD - If equals 'y', the Makefile skips compilation and doesn't erease targets

.PHONY: dummy

ifdef NO_TARGET
.PHONY: $(NO_TARGET)
endif

# Default rule which all makefiles fall into
first_rule: sub_dirs all_targets

include $(TARGET_HOME)/TI_Build.make

# Sub directory definitions. The Make will search for _util directoris to build them first
subdirutil-y := $(join $(subdir-y),$(patsubst %/src,/%_util/src,$(subdir-y)))
SUB_DIRS = $(subdirutil-y) $(subdir-y)

# Default linker flags
LDFLAGS := -L$(COMPONENT_PREFIX)/build -L$(TI_filesystem_path)$(TI_rlibdir) -L$(TI_lib_path) -L.

RANLIB=$(CROSS)ranlib

ifdef COMPONENT_INCLUDES
CFLAGS += $(COMPONENT_INCLUDES)
endif

# Create dependencies file during compilation
CFLAGS += -MMD

# Display all warnings
CFLAGS += -Wall

# Module specific compiler flags
CFLAGS += $(cflags-y)

# Module specific linker flags
LDFLAGS += $(ldflags-y) 

# Shared library must by compiled with position independed code
ifdef SO_TARGET
CFLAGS += -fPIC 
endif

# Library might be used by a shared library. Compile with position independed code
ifdef A_TARGET
CFLAGS += -fPIC -ffunction-sections -fdata-sections 
endif

# Objects might be used by a shared library. Compile with position independed code
ifdef O_TARGET
CFLAGS += -fPIC -ffunction-sections -fdata-sections 
endif

# Objects might be used by a shared library. Compile with position independed code
ifdef NO_TARGET
CFLAGS += -fPIC -ffunction-sections -fdata-sections 
endif

ifdef TARGET
# Remove unused sections and reduce padding
LDFLAGS += -Wl,--gc-sections -fwhole-program -Wl,--sort-common -Wl,--sort-section,alignment
endif

# allow userspace stack unwinding
CFLAGS += -fasynchronous-unwind-tables

ifdef CONFIG_ARRIS_BACKTRACE_SYMBOLS
CFLAGS +=  -rdynamic
endif

# Library names format lib[name].so and lib[name].a
ifdef SO_TARGET
SO_TARGET_NAME := lib$(SO_TARGET).so
endif

ifdef A_TARGET
A_TARGET_NAME := lib$(A_TARGET).a
endif

ifdef O_TARGET
O_TARGET_NAME := $(O_TARGET).o
endif

# Main directory list
subdir-list = $(patsubst %, _subdir_%, $(SUB_DIRS))
sub_dirs: dummy $(subdir-list)

ifdef SUB_DIRS
$(subdir-list) : dummy
	@ if [ -f $(patsubst _subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst _subdir_%,%,$@) ;\
	fi
endif

# Configuration directory list
config-subdir-list = $(patsubst %, config_subdir_%, $(SUB_DIRS))
config-sub_dirs: dummy $(config-subdir-list)

# Clean directory list
clean-subdir-list = $(patsubst %, clean_subdir_%, $(SUB_DIRS))
clean-sub_dirs: dummy $(clean-subdir-list)

# Install directory list
install-subdir-list = $(patsubst %, install_subdir_%, $(SUB_DIRS))
install-sub_dirs: dummy $(install-subdir-list)

# Uninstall directory list
uninstall-subdir-list = $(patsubst %, uninstall_subdir_%, $(SUB_DIRS))
uninstall-sub_dirs: dummy $(uninstall-subdir-list)

headers-list := $(patsubst %,../include/%,$(filter-out ../include,$(install_headers-y)))
uninstall-header-list := $(patsubst ../include/%,$(TI_include)/%,$(install_headers-y))

# Header installations during config
install_headers:
ifdef install_headers-y
	@install --preserve-timestamps -m 0644 $(headers-list) $(TI_include)
endif

# Targets for config, clean and install
ifdef SUB_DIRS
$(config-subdir-list) : dummy
	@ if [ -f $(patsubst config_subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst config_subdir_%,%,$@) config_rule > /dev/null ;\
	fi

$(clean-subdir-list) : dummy
	@ if [ -f $(patsubst clean_subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst clean_subdir_%,%,$@) clean_rule > /dev/null ;\
	fi

$(install-subdir-list) : dummy
	@ if [ -f $(patsubst install_subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst install_subdir_%,%,$@) install_rule > /dev/null ;\
	fi

$(uninstall-subdir-list) : dummy
	@ if [ -f $(patsubst uninstall_subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst uninstall_subdir_%,%,$@) uninstall_rule > /dev/null ;\
	fi
endif

config_rule: config-sub_dirs install_headers

dep:

clean_rule:	clean-sub_dirs
	@rm -rf $(obj-y) $(obj-y:.o=.d) $(clean-y)
ifneq ($(SKIP_BUILD),y)
	@rm -rf $(O_TARGET_NAME) $(TARGET) $(A_TARGET_NAME) $(SO_TARGET_NAME)
endif    

# Install targets and all other extra outputs
install_rule: install-sub_dirs install_extra
ifdef TI_filesystem_path
ifdef TARGET
	@install -m 0755 $(STRIP) $(TARGET) $(TI_filesystem_path)$(TI_sbindir)
endif
ifdef SO_TARGET
	@install -m 0755 $(STRIP) $(SO_TARGET_NAME) $(TI_filesystem_path)$(TI_rlibdir)
	@install -m 0755 $(STRIP) $(SO_TARGET_NAME) $(TI_lib_path)
endif
ifdef A_TARGET
	@install -m 0755 $(STRIP) $(A_TARGET_NAME) $(TI_lib_path)
endif
else
	@echo Cannot install, TI_filesystem_path not defined.
endif

# Uninstall targets and all other extra outputs
uninstall_rule: uninstall-sub_dirs
ifdef TI_filesystem_path
ifdef TARGET
	@rm -rf $(TI_filesystem_path)$(TI_sbindir)/$(TARGET)
endif
ifdef SO_TARGET
	@rm -rf $(TI_filesystem_path)$(TI_rlibdir)/$(SO_TARGET_NAME)
	@rm -rf $(TI_lib_path)/$(SO_TARGET_NAME)
endif
ifdef A_TARGET
	@rm -rf $(TI_lib_path)/$(A_TARGET_NAME)
endif
ifdef install_headers-y
	@rm -rf $(uninstall-header-list)
endif
else
	@echo Cannot uninstall, TI_filesystem_path not defined.
endif


# All binary targets
all_targets: dummy build_pre $(O_TARGET_NAME) $(SO_TARGET_NAME) $(A_TARGET_NAME) $(TARGET) $(NO_TARGET) build_post

%.o : %.c
	@echo "  CC [C] 	$@"
	@if [ -f $(CURDIR)/$< ]; then \
		$(CC) $(CFLAGS) -c $(CURDIR)/$< -o $@ ;\
	else \
		$(CC) $(CFLAGS) -c $< -o $@ ;\
	fi

ifneq ($(SKIP_BUILD),y)
ifdef O_TARGET_NAME
ifdef obj-y
$(O_TARGET_NAME) : $(obj-y) dummy
	@rm -f $@ $(COMPONENT_PREFIX)/build/$@
	@echo "  LD [TGT] 	$@"
	@$(LD) -r -o $@ $(filter $(obj-y), $^) $(LDFLAGS)
	@install -m 0755 $(STRIP) $@ $(COMPONENT_PREFIX)/build
endif
endif

ifdef TARGET
ifdef obj-y
$(TARGET) : $(obj-y) dummy
	@rm -f $@
	@echo "  CC [TGT] 	$@"
	@$(CC) -o $@ $(filter $(obj-y), $^) $(LDFLAGS) $(LDEXTRA) -Wl,-Map,$@.map
endif
endif

ifdef A_TARGET
ifdef obj-y
$(A_TARGET_NAME): $(obj-y)
	@rm -f $@ $(COMPONENT_PREFIX)/build/$@
	@echo "  CC [AR] 	$@"
	@$(AR) rc $@ $(filter $(obj-y), $^)
	@$(RANLIB) $@
	@install -m 0755 $(STRIP) $@ $(COMPONENT_PREFIX)/build
endif
endif

ifdef SO_TARGET
ifdef obj-y
$(SO_TARGET_NAME): $(obj-y)
	@rm -f $@ $(COMPONENT_PREFIX)/build/$@
	@echo "  CC [SO] 	$@"
	@$(CC) -shared -o $@ $(filter $(obj-y), $^) $(LDFLAGS) $(LDEXTRA)
	@install -m 0755 $(STRIP) $@ $(COMPONENT_PREFIX)/build
endif
endif

ifdef NO_TARGET
ifdef obj-y
$(NO_TARGET): $(obj-y) dummy
endif
endif

else

# Empty rules, which skip the building

ifdef O_TARGET
ifdef obj-y
$(O_TARGET) : dummy
	@echo "  LD [TGT] 	$@"
	@install -m 0755 $(STRIP) $@ $(COMPONENT_PREFIX)/build
endif
endif

ifdef TARGET
ifdef obj-y
$(TARGET) : dummy
	@echo "  CC [TGT] 	$@"
endif
endif

ifdef A_TARGET
ifdef obj-y
$(A_TARGET_NAME): dummy
	@echo "  CC [AR] 	$@"
	@install -m 0755 $(STRIP) $@ $(COMPONENT_PREFIX)/build
endif
endif

ifdef SO_TARGET
ifdef obj-y
$(SO_TARGET_NAME): dummy
	@echo "  CC [SO] 	$@"
	@install -m 0755 $(STRIP) $@ $(COMPONENT_PREFIX)/build
endif
endif
endif

ifdef NO_TARGET
ifdef obj-y
$(NO_TARGET): dummy
endif
endif

dummy:

# Include dependency files
ifneq ($(obj-y),)
-include $(obj-y:.o=.d)
endif
