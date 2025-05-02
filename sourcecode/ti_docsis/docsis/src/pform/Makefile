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

include $(DOCSIS_PREFIX)/build/docsis_config.make

#
# Final output target
#
# Select one (or more) targets to be generated from this Makefile:
#
# TARGET	- Binary executable file
# A_TARGET	- Static library file (.a file)
# SO_TARGET	- Shared library file (.so file)
# O_TARGET	- Object file (.o file)
#
A_TARGET :=

#
# Compilation options to add to the compiler (optional)
#
cflags-y :=

# Export .h files in these dirs to all sub dirs

export CFLAGS += \
	-I$(CURDIR)/hal_infrastructure/hal_mac_regs \
	-I$(CURDIR)/hal_infrastructure/hal_upstream_mbx/src \
	-I$(CURDIR)/hal_infrastructure/hal_isr/mlx/mask_lib \
	-I$(CURDIR)/hal_infrastructure/hal_soc_interface_driver \
	-I$(CURDIR)/hal_infrastructure/hal_soc_interface_driver/mpeg_out \
	-I$(CURDIR)/hal_infrastructure/hal_soc_interface_driver/mpeg_encap \
	-I$(CURDIR)/hal_infrastructure/hal_soc_interface_driver/docsis_global \

#
# Link options to add to the linker (optional)
# e.g. ldflags-y := -lfoo
#
ldflags-y :=

#
# List of sub-directories to compile (if required)
# e.g. subdir-y := mymodule1/src mymodule2/src
#
# HAL INFRASTRUCTURE
subdir-y := \
	hal_infrastructure/hal_reg_access/src/reg_access_lib \
	hal_infrastructure/hal_isr/mlx/mask_lib \
	hal_infrastructure/hal_phy/src \
	hal_vendor/hal_calibration/hal_ffs_calibration/src \
	hal_vendor/hal_calibration/hal_ds_calibration/src \
	hal_vendor/hal_calibration/hal_us_calibration/src \
	hal_infrastructure/hal_upstream_mbx/src/cmd_mbox_lib \
	hal_infrastructure/hal_upstream_mbx/src/event_mbox_lib \
	hal_infrastructure/hal_ds_fw_if/src \
	hal_infrastructure/hal_fw_download \
	hal_infrastructure/hal_i2c_if/src \
	hal_vendor/hal_tuners/hal_tuner_manager_api/src \
	hal_infrastructure/hal_db/src \


subdir-$(BUILD_POST) +=	hal_infrastructure/hal_isr \
	hal_services/hal_mng_q \
	hal_infrastructure/hal_soc_interface_driver


# HAL SERVICES
subdir-y +=		hal_services/hal_sync/src \
			hal_services/hal_mdd/src \
			hal_services/hal_global/src \
			hal_services/hal_rng/src \
			hal_services/hal_ucd/src \
			hal_services/hal_ucd_forwarding/src \
			hal_services/hal_filter/src \
			hal_services/hal_ds_channel_control/src \
			hal_services/hal_us_channel_control/src \
			hal_services/hal_qos/src \
			hal_services/hal_bpi/src \
			hal_services/hal_counter/src \
			hal_services/hal_phs/src \
			hal_services/hal_mpeg/src \
			hal_services/hal_dsid/src \
			hal_services/hal_oob/src \

# Executables
subdir-y += \
	hal_infrastructure/hal_reg_access/src/reg_access_app \
	hal_infrastructure/hal_upstream_mbx/src/cmd_mbox_app \
	hal_infrastructure/hal_upstream_mbx/src/event_mbox_app \
	hal_vendor/hal_tuners/hal_tuner_manager_app/src \
	hal_infrastructure/hal_isr/mlx \
	hal_infrastructure/hal_isr/mlx/mpt_int \
#	hal_vendor/hal_tuners/test/src \
#	hal_services/hal_rng/test \
#	hal_services/hal_ucd/test \
#	hal_services/hal_qos/test \
#	hal_services/hal_bpi/test \
#	hal_services/hal_global/test \
#	hal_services/hal_phs/test \



# List of objects to compile
#
# The list could be either automatically generated from the C files in the
# current directory by the following syntax:
#
# obj-y := $(AUTO_GENERATE_C_FILES)
#
# or manually generated by explicitly specifing each C file:
#
# obj-y := obj1.o obj2.o ....
#
obj-y := $(AUTO_GENERATE_C_FILES)

#
# List of header files to install in the system include directory
#
# The list could be either automatically generated from the H files in the
# include directory by the following syntax:
#
# install_headers-y := $(AUTO_GENERATE_H_FILES)
#
# or manually generated by explicitly specifing each H file:
#
# install_headers-y := header1.h header2.h ....
#
install_headers-y :=

#
# List of additional files to clean (excluding object files and targets)
#
clean-y :=

#
# Make rules include file (Do not remove!)
#
include $(DOCSIS_PREFIX)/build/rules.make

#
# Additional module targets:
#

#
# Actions which require control here, before starting to build this module.
# If there is nothing to be done, this target should be left empty.
#
build_pre:

#
# Actions which require the control here, after this module has been built.
# If there is nothing to be done, this target should be left empty.
#
build_post:

#
# Target to install additional files
#
# Use the install command to copy additional files by the following syntax:
#
# @install -m 0755 <list of files> <destination directory>
#
install_extra:


