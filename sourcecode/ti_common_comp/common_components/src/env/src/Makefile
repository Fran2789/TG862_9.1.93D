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

include $(TARGET_HOME)/TI_Build.make

TARGET_DIR := fw_env
TARGET := $(TARGET_DIR)/printenv

all: build install

install_rule: install

clean_rule: clean

config_rule: config

uninstall_rule: uninstall

build: $(TARGET)


obj-y := $(TARGET_DIR)/crc32.o $(TARGET_DIR)/fw_env.o $(TARGET_DIR)/fw_env_main.o


$(TARGET): $(obj-y)
	@rm -f $@
	@echo "  LD	 	$@"
	@$(CC) $(LDFLAGS) -o $@ $(filter $(obj-y), $^)

install:
	@install -m 0755 $(TARGET) $(TI_filesystem_path)$(TI_sbindir)/$(notdir $(TARGET))
	@cd $(TI_filesystem_path)$(TI_sbindir) && ln -sf $(notdir $(TARGET)) ./setenv && ln -sf $(notdir $(TARGET)) ./getenv
ifdef TI_ENVUTILS_USE_CONFIG_FILE
	@install $(TARGET_DIR)/fw_env.config $(TI_filesystem_path)$(TI_etcdir)
endif

clean:
	@rm -f $(TARGET) $(obj-y)

config:

uninstall:
	@rm -f $(TI_filesystem_path)$(TI_sbindir)/$(notdir $(TARGET))
	@rm -f $(TI_filesystem_path)$(TI_etcdir)/fw_env.config
	@rm -f $(TI_filesystem_path)$(TI_sbindir)/setenv
	@rm -f $(TI_filesystem_path)$(TI_sbindir)/getenv

distclean: clean uninstall

%.o : %.c
	@echo "  CC [C] 	$@"
	@$(CC) $(CFLAGS) -c $< -o $@


