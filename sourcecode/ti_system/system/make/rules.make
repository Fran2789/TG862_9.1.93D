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
##|         Copyright (c) 1998-2009 Texas Instruments Incorporated           |##
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

# Unified Makefile Framework - Rules Makefile
# -------------------------------------------
# Note: This file should not be changed!
#
# Maintenance and support: Hai Shalom, hai@ti.com
#
# General purpose variables:
# --------------------------
# COMPONENT_INCLUDES - General component include directives
# COMPONENT_PREFIX - Component root directory
# SKIP_BUILD - If equals 'y', the Makefile skips compilation and doesn't erease targets
#
# Debug options:
# --------------
# UMF_DEBUG_LEVEL=0 - Display working directory
# UMF_DEBUG_LEVEL=1 - Display also target creation command line
# UMF_DEBUG_LEVEL=2 - Display also object compilation command line
# UMF_DEBUG_LEVEL=3 - Display also all targets spanning tree
#
# UMF_LINK_ANALYSIS - If equals 'y', run link analysis per each link to find unneeded libraries
# ------------------------------------------------------------------------------

PHONY:=dummy first_rule sub_dirs all_targets install_extra config_rule config-sub_dirs \
install_headers install_alternate_fs clean_rule clean-sub_dirs build_pre build_post dep $(SO_TARGET) $(A_TARGET) $(NO_TARGET)

.PHONY: $(PHONY)

.NOTPARALLEL:


# Default rule which all makefiles fall into
first_rule: sub_dirs all_targets

# Make sure we have all system definitions
ifndef TI_filesystem_path
include $(TARGET_HOME)/TI_Build.make
endif

# Sub directory definitions. The Make will search for _util directoris to build them first
subdirutil-$(BUILD_POST) := $(join $(subdir-y),$(patsubst %/src,/%_util/src,$(subdir-y)))
SUB_DIRS = $(subdirutil-$(BUILD_POST)) $(subdir-y)

ifeq ($(BUILD_POST),n)
override TARGET :=
ifneq ($(strip $(SO_TARGET) $(O_TARGET) $(A_TARGET) $(NO_TARGET) $(UO_TARGET) $(USO_TARGET)),)
INSTALL_EXTRA := install_extra
endif
else 
ifeq ($(BUILD_POST),y)
override SO_TARGET:=
override A_TARGET:=
override O_TARGET:=
override NO_TARGET:=
override UO_TARGET:=
override USO_TARGET:=
ifdef TARGET
INSTALL_EXTRA := install_extra
endif
else
INSTALL_EXTRA := install_extra
endif
endif


# Default linker flags and module's specific linker flags
# Also tell MAKE where to find libraries that are prerequisites
LDFLAGS-private := -L$(COMPONENT_PREFIX)/build -L$(TI_filesystem_basefs_path)$(TI_rlibdir) -L$(TI_lib_path) -L. $(filter-out -l%,$(ldflags-y))
# Lib-s are installed here
VPATH=$(TI_filesystem_basefs_path)$(TI_rlibdir) $(TI_lib_path)  # ARRIS MOD - add ti/lib
#vpath %.so $(TI_filesystem_basefs_path)$(TI_rlibdir)
# Some are also here, specifically, the so name without the version extension
vpath %.so $(TI_lib_path)
# If building to a different file system, link to there too
ifneq ($(TI_filesystem_basefs_path),$(TI_filesystem_path))
    # ARRIS MOD BEGIN : Don't use the vpath %.so filter - some libs don't end in .so.... : Set VPATH variable instead 
    VPATH += $(TI_filesystem_path)$(TI_rlibdir)
    LDFLAGS-private += -L$(TI_filesystem_path)$(TI_rlibdir) 
    # vpath %.so $(TI_filesystem_path)$(TI_rlibdir)
    # ARRIS MOD END */
endif

# Default C Flags:
# Create dependencies file during compilation, display all warnings, generate symbols, and module's flags
CFLAGS-private += -I$(CURDIR)/../include -I$(CURDIR)/. -MMD -Wall -g $(cflags-y) $(COMPONENT_INCLUDES)

# All targets except shared libraries are compiled with section per each data/function
ifndef SO_TARGET
ifneq ($(CONFIG_INTEL_SYSTEM_STACK_BACKTRACE_MINIMAL),)
    # On all except Minimal support, these will interfere with Stack Backtrace
    CFLAGS-private += -ffunction-sections -fdata-sections
endif
endif

# Compile all libraries with position independed code
ifndef TARGET
CFLAGS-private += -fPIC
else
# Remove unused sections and reduce padding
LDFLAGS-private += -Wl,--gc-sections -fwhole-program -Wl,--sort-common -Wl,--sort-section,alignment
endif

# Library names format lib[name].so and lib[name].a
ifdef USO_TARGET
# Unified shared object target is simple shared object target except list of objects predefined
SO_TARGET := $(USO_TARGET)
obj-y := $(shell ls $(COMPONENT_PREFIX)/build/*.o$(USO_POSTFIX) 2>/dev/null)
endif

ifdef SO_TARGET
SO_TARGET_NAME := lib$(SO_TARGET).so
endif

ifdef A_TARGET
A_TARGET_NAME := lib$(A_TARGET).a
endif

ifdef O_TARGET
O_TARGET_NAME := $(O_TARGET).o
endif

ifdef UO_TARGET
# Unified object target is simple object with special formatted name
O_TARGET_NAME := $(UO_TARGET).o$(USO_POSTFIX)
O_TARGET := $(UO_TARGET)
endif

#########################################################################
# Debugging the UMF
#########################################################################

TARGET_CMD_DEBUG := @
COMPILE_CMD_DEBUG := @
OUTPUT_FILE_DEBUG := /dev/null

ifdef UMF_DEBUG_LEVEL
ifeq ($(UMF_DEBUG_LEVEL),1)
TARGET_CMD_DEBUG :=
else
ifeq ($(UMF_DEBUG_LEVEL),2)
TARGET_CMD_DEBUG :=
COMPILE_CMD_DEBUG :=
else
ifeq ($(UMF_DEBUG_LEVEL),3)
TARGET_CMD_DEBUG :=
COMPILE_CMD_DEBUG :=
OUTPUT_FILE_DEBUG := /dev/stdout
endif
endif
endif
else
ifndef UMF_PRINT_DIRECTORY
MAKEFLAGS += --no-print-directory
endif
endif

#########################################################################
# Directory lists generation
#########################################################################

# Main directory list
subdir-list = $(patsubst %, _subdir_%, $(SUB_DIRS))
sub_dirs: $(subdir-list)

ifdef SUB_DIRS
$(subdir-list) : dummy
	@ if [ -f $(patsubst _subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst _subdir_%,%,$@) UMF_ACTIVE_RULE=UMF_BUILD ;\
	fi

# Configuration directory list
config-subdir-list = $(patsubst %, config_subdir_%, $(SUB_DIRS))
config-sub_dirs: $(config-subdir-list)

# Clean directory list
clean-subdir-list = $(patsubst %, clean_subdir_%, $(SUB_DIRS))
clean-sub_dirs: $(clean-subdir-list)

# Install directory list
install-subdir-list = $(patsubst %, install_subdir_%, $(SUB_DIRS))
install-sub_dirs: $(install-subdir-list)

# Uninstall directory list
uninstall-subdir-list = $(patsubst %, uninstall_subdir_%, $(SUB_DIRS))
uninstall-sub_dirs: $(uninstall-subdir-list)
endif

headers-list := $(patsubst %,../include/%,$(filter-out ../include,$(install_headers-y)))
uninstall-header-list := $(patsubst ../include/%,$(TI_include)/%,$(install_headers-y))

# Header installations during config
install_headers:
ifdef install_headers-y
	@install --preserve-timestamps -m 0644 $(headers-list) $(TI_include)
endif

# Create basic file system structure when different than base fs
$(TI_filesystem_path):
ifneq ($(TI_filesystem_path),$(TI_filesystem_basefs_path))
	@echo "***"
	@echo "***"
	@echo "*** Making skeleton for fs $(TI_filesystem_path)"
	@echo "***"
	@echo "***"
	@install -m 0755 -d $(TI_filesystem_path)/var
	@install -m 0755 -d $(TI_filesystem_path)/var/tmp
	@install -m 0755 -d $(TI_filesystem_path)/etc
	@install -m 0755 -d $(TI_filesystem_path)/etc/init.d
	@install -m 0755 -d $(TI_filesystem_path)/etc/scripts
	@install -m 0755 -d $(TI_filesystem_path)/bin
	@install -m 0755 -d $(TI_filesystem_path)/lib
	@install -m 0755 -d $(TI_filesystem_path)/lib/modules
	@install -m 0755 -d $(TI_filesystem_path)/sbin
	@install -m 0755 -d $(TI_filesystem_path)/usr
	@install -m 0755 -d $(TI_filesystem_path)/usr/sbin
	@install -m 0755 -d $(TI_filesystem_path)/usr/bin
	@install -m 0755 -d $(TI_filesystem_path)/usr/lib
	@install -m 0755 -d $(TI_filesystem_path)/usr/www
	touch $(TI_filesystem_path)/$(notdir $(TI_filesystem_path)).fsname
endif

install_alternate_fs: $(TI_filesystem_path)

#########################################################################
# Targets for config, clean and install
#########################################################################
ifdef SUB_DIRS
$(config-subdir-list) : dummy
	@ if [ -f $(patsubst config_subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst config_subdir_%,%,$@) config_rule UMF_ACTIVE_RULE=UMF_CONFIG > $(OUTPUT_FILE_DEBUG) ;\
	fi

$(clean-subdir-list) : dummy
	@ if [ -f $(patsubst clean_subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst clean_subdir_%,%,$@) clean_rule UMF_ACTIVE_RULE=UMF_CLEAN > $(OUTPUT_FILE_DEBUG) ;\
	fi

$(install-subdir-list) : dummy
	@ if [ -f $(patsubst install_subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst install_subdir_%,%,$@) install_rule UMF_ACTIVE_RULE=UMF_INSTALL > $(OUTPUT_FILE_DEBUG) ;\
	fi

$(uninstall-subdir-list) : dummy
	@ if [ -f $(patsubst uninstall_subdir_%,%,$@)/Makefile ]; then \
		$(MAKE) -C $(patsubst uninstall_subdir_%,%,$@) uninstall_rule UMF_ACTIVE_RULE=UMF_UNINSTALL > $(OUTPUT_FILE_DEBUG) ;\
	fi
endif

config_rule: install_headers install_alternate_fs

ifdef SUB_DIRS
config_rule: config-sub_dirs
endif

#########################################################################
# Clean targets
#########################################################################

clean_rule:
	@echo erasing in $(CURDIR)
ifneq ($(SKIP_BUILD),y)
	$(COMPILE_CMD_DEBUG)rm -rf $(obj-y) $(obj-y:.o=.d) $(clean-y)
ifdef TARGET
	#$(TARGET_CMD_DEBUG)rm -rf $(TARGET) $(TARGET).map $(TARGET).d $(TARGET).o $(TARGET).dep
	$(TARGET_CMD_DEBUG)rm -rf $(TARGET) $(TARGET).map $(TARGET).dep
else
ifdef INSTALL_EXTRA
	$(TARGET_CMD_DEBUG)rm -rf $(O_TARGET_NAME) $(A_TARGET_NAME) $(SO_TARGET_NAME) $(SO_TARGET_NAME).map $(SO_TARGET_NAME).dep $(A_TARGET_NAME).dep
endif # Library targets
endif # !TARGET
else #SKIP_BUILD
	$(COMPILE_CMD_DEBUG)rm -rf $(obj-y:.o=.d) $(clean-y)
endif

ifdef SUB_DIRS
clean_rule: clean-sub_dirs
endif

#########################################################################
# Install targets and all other extra outputs
#########################################################################

# ARRIS CHANGE - install sub-dirs before running install_extra on top level makefile.  That way
# you can take an action after they install.
ifdef SUB_DIRS
install_rule: install-sub_dirs
endif
# END ARRIS CHANGE

install_rule: $(INSTALL_EXTRA)
ifdef TARGET
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0755 $(TARGET) $(TI_filesystem_path)$(TI_sbindir)
endif
ifdef SO_TARGET
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0755 $(SO_TARGET_NAME) $(TI_filesystem_path)$(TI_rlibdir)
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0755 $(SO_TARGET_NAME) $(TI_lib_path)
endif
ifdef A_TARGET
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0755 $(A_TARGET_NAME) $(TI_lib_path)
endif


#########################################################################
# Uninstall targets and all other extra outputs
#########################################################################

uninstall_rule:
ifdef TARGET
	$(TARGET_CMD_DEBUG)rm -rf $(TI_filesystem_path)$(TI_sbindir)/$(TARGET)
endif
ifdef SO_TARGET
	$(TARGET_CMD_DEBUG)rm -rf $(TI_filesystem_path)$(TI_rlibdir)/$(SO_TARGET_NAME)
	$(TARGET_CMD_DEBUG)rm -rf $(TI_lib_path)/$(SO_TARGET_NAME)
endif
ifdef A_TARGET
	$(TARGET_CMD_DEBUG)rm -rf $(TI_lib_path)/$(A_TARGET_NAME)
endif
ifdef install_headers-y
ifneq ($(BUILD_POST),y)
	$(TARGET_CMD_DEBUG)rm -rf $(uninstall-header-list)
endif
endif

ifdef SUB_DIRS
uninstall_rule: uninstall-sub_dirs
endif

#########################################################################
# Target builds
#########################################################################

# All binary targets
all_targets: dummy build_pre $(O_TARGET_NAME) $(SO_TARGET_NAME) $(A_TARGET_NAME) $(TARGET) $(NO_TARGET) build_post

# Create Object files from C files
%.o : %.c
	@echo "  CC [C] 	$@"
ifeq ($(UMF_ABSOLUTE_OBJECT_PATH),y)
	$(COMPILE_CMD_DEBUG)$(CC) $(CFLAGS) $(CFLAGS-private) -c $< -o $@
else
	$(COMPILE_CMD_DEBUG)$(CC) $(CFLAGS) $(CFLAGS-private) -c $(CURDIR)/$< -o $@
endif

ifneq ($(SKIP_BUILD),y)

# Create an object from number of objects
ifdef O_TARGET_NAME
$(O_TARGET_NAME): $(obj-y)
	@echo "  LD	 	$@"
	$(TARGET_CMD_DEBUG)$(LD) -r -o $@ $(filter $(obj-y), $^) $(LDFLAGS) $(LDFLAGS-private)
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0644 $@ $(COMPONENT_PREFIX)/build
endif

# Create an executable
ifdef TARGET
ldlibs := $(filter -l%,$(ldflags-y))
ldarchives := $(patsubst %,$(COMPONENT_PREFIX)/build/lib%.a,$(ldarchives-y))


$(TARGET): $(obj-y)
	@echo "  LD	 	$@"
ifdef UMF_LINK_ANALYSIS
	# ARRIS Fix Link Analysis Tool
	$(TARGET_CMD_DEBUG)$(TARGET_HOME)/tools/scripts/link-analysis.sh $(CC) $@ "$(filter-out %Makefile ,$(obj-y))" "$(LDFLAGS) $(filter-out -l%,$(LDFLAGS-private)) -Wl,-Map,$@.map" "$(ldlibs)" "$(ldarchives)"
	#$(TARGET_CMD_DEBUG)$(TARGET_HOME)/tools/scripts/link-analysis.sh $(CC) $@ "$(filter-out %Makefile ,$(obj-y))" "$(LDFLAGS) $(filter-out -l%,$(LDFLAGS-private)) -Wl,-Map,$@.map" "$(ldlibs)" "$(ldarchives)" >> $(TARGET_HOME)/link_analysis.txt
else
	$(TARGET_CMD_DEBUG)$(CC) -o $@ $(filter-out %Makefile ,$(obj-y)) $(LDFLAGS) $(LDFLAGS-private) -Wl,--start-group $(ldarchives) $(ldlibs) -Wl,--end-group -Wl,-Map,$@.map
endif
	# Prepare dependency file list  TARGET.dep
	# this list will be appended to dependencies line, (see include directive) TARGET: $(obj-y)  + TARGET.dep   
	@echo "$(TARGET): \\" > $(TARGET).dep
	@$(CROSS)objdump --private-headers $(TARGET) | grep NEEDED | \
                awk -F" " '{print $$2" \\"}' | sed -e 's=.*/==' >> $(TARGET).dep

ifdef ldarchives
	@echo "$(ldarchives) \\" >> $(TARGET).dep
endif
	@echo "$(CURDIR)/Makefile" >> $(TARGET).dep
endif

# Create an archive
ifdef A_TARGET
$(A_TARGET_NAME): $(obj-y)
	@echo "  AR	 	$@"
	$(TARGET_CMD_DEBUG)$(AR) rc $@ $(filter $(obj-y), $^)
	$(TARGET_CMD_DEBUG)$(CROSS)ranlib $@
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0644 $@ $(COMPONENT_PREFIX)/build
	@echo "$(A_TARGET_NAME): \\" > $(A_TARGET_NAME).dep
	@echo "$(CURDIR)/Makefile" >> $(A_TARGET_NAME).dep
endif

# Create a shared library
ifdef SO_TARGET
ldlibs := $(filter -l%,$(ldflags-y))
ldarchives := $(patsubst %,$(COMPONENT_PREFIX)/build/lib%.a,$(ldarchives-y))

#	@$(CROSS)objdump --private-headers $(SO_TARGET_NAME) | grep NEEDED | awk -F" " '{print "$(TI_filesystem_path)/lib/"$$2" \\"}' >> $(SO_TARGET_NAME).dep
#	@$(CROSS)objdump --private-headers $(SO_TARGET_NAME) | grep NEEDED | awk -F" " '{print "$(TI_lib_path)/"$$2" \\"}' >> $(SO_TARGET_NAME).dep

$(SO_TARGET_NAME): $(obj-y)
	@echo "  CC [SO] 	$@"
	$(TARGET_CMD_DEBUG)$(CC) -shared -o $@ $(filter $(obj-y), $^) $(LDFLAGS) $(LDFLAGS-private) -Wl,--start-group $(ldarchives) $(ldlibs) -Wl,--end-group -Wl,-Map,$@.map
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0755 $@ $(COMPONENT_PREFIX)/build
	$(TARGET_CMD_DEBUG)if [ -d $(TI_filesystem_path)$(TI_rlibdir) ]; then install --preserve-timestamps -m 0755 $(SO_TARGET_NAME) $(TI_filesystem_path)$(TI_rlibdir); fi
	$(TARGET_CMD_DEBUG)if [ -d $(TI_lib_path) ]; then install --preserve-timestamps -m 0755 $(SO_TARGET_NAME) $(TI_lib_path); fi
	@echo "$(SO_TARGET_NAME): \\" > $(SO_TARGET_NAME).dep
	@$(CROSS)objdump --private-headers $(SO_TARGET_NAME) | grep NEEDED | awk -F" " '{print $$2" \\"}' | sed -e 's=.*/==' >> $(SO_TARGET_NAME).dep
ifdef ldarchives
	@echo "$(ldarchives) \\" >> $(SO_TARGET_NAME).dep
endif
	@echo "$(CURDIR)/Makefile" >> $(SO_TARGET_NAME).dep
endif

# Create a list of objects
ifdef NO_TARGET
$(NO_TARGET): $(obj-y)
endif

else

# Empty rules, which skip the building

ifdef O_TARGET
$(O_TARGET):
	@echo "  LD	 	$@"
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0644 $@ $(COMPONENT_PREFIX)/build
endif

ifdef TARGET
$(TARGET):
	@echo "  LD	 	$@"
endif

ifdef A_TARGET
$(A_TARGET_NAME):
	@echo "  AR	 	$@"
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0644 $@ $(COMPONENT_PREFIX)/build
endif

ifdef SO_TARGET
$(SO_TARGET_NAME):
	@echo "  CC [SO] 	$@"
	$(TARGET_CMD_DEBUG)install --preserve-timestamps -m 0755 $@ $(COMPONENT_PREFIX)/build
endif

ifdef NO_TARGET
$(NO_TARGET):
endif

endif


# Obsolete
dep:

dummy:
	@echo > /dev/null

#########################################################################
# Include dependency files
#########################################################################

ifneq ($(obj-y),)
-include $(patsubst %.o,%.d,$(patsubst %.o$(USO_POSTFIX),%.d,$(obj-y)))
ifdef TARGET
-include $(TARGET).dep
endif
ifdef SO_TARGET
-include $(SO_TARGET_NAME).dep
endif
ifdef A_TARGET
-include $(A_TARGET_NAME).dep
endif
endif

