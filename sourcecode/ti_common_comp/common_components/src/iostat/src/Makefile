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

APPDIR := iostat-2.2
APPNAME := iostat
export CFLAGS += -Werror

first_rule: all

all:
	@$(MAKE) -C $(APPDIR) all 

install_rule:
	@install --preserve-timestamps $(APPDIR)/$(APPNAME) $(TI_filesystem_path)$(TI_sbindir)
    
clean_rule:
	@$(MAKE) -C $(APPDIR) clean

config_rule:

uninstall_rule:
	@rm -rf $(TI_filesystem_path)$(TI_sbindir)/$(APPNAME)


