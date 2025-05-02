#ifndef PATCHLEVEL
#include ../../Rules.mak
#endif 

CROSS_COMPILE = $(CROSS)

include $(TARGET_HOME)/.config

# Kernel directory
ifndef KERNEL_DIR
KERNELDIR := $(TARGET_HOME)/kernel/ti/linux-2.6.39.3/src
endif

# File system path, where to install the module
ifndef TI_filesystem_path
$(error TI_filesystem_path is not defined in gw/src/gpl_modules/src/gpl_modules-0.1/Makefile)
endif

MODULES = ip6_gre.ko 

obj-m:= ip6_gre.o 

all:modules

modules:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules

clean:
	rm -rf $(MODULES) *.o *.ko *.cmd .*.cmd

install:
	@if [ ! -d $(TI_filesystem_path)/lib/modules ]; then mkdir $(TI_filesystem_path)/lib/modules; fi
	@if [ ! -d $(TI_filesystem_path)/lib/modules/gpl_modules ]; then mkdir $(TI_filesystem_path)/lib/modules/gpl_modules; fi
	@for module in $(MODULES); do \
	  cp -f $$module $(TI_filesystem_path)/lib/modules/gpl_modules; \
	done

uninstall:
	@for module in $(MODULES); do\
	  find $(TI_filesystem_path)/lib/modules/gpl_modules -name $$module | xargs rm -f; \
	done


