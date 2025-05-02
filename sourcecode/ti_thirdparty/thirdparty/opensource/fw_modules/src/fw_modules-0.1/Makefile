#ifndef PATCHLEVEL
#include ../../Rules.mak
#endif 

CROSS_COMPILE = $(CROSS)

# Kernel directory
ifndef KERNEL_DIR
KERNELDIR := $(TARGET_HOME)/kernel/ti/linux-2.6.39.3/src
endif

# File system path, where to install the module
ifndef TI_filesystem_path
$(error TI_filesystem_path is not defined in gw/src/fw_modules/src/fw_modules-0.1)
endif

MODULES = ipt_psd.ko 

obj-m:= ipt_psd.o 

all:modules

modules:
	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules

clean:
	rm -rf $(MODULES) *.o *.ko *.cmd .*.cmd

install:
	@if [ ! -d $(TI_filesystem_path)/lib/xtables ]; then mkdir $(TI_filesystem_path)/lib/xtables; fi
	@for module in $(MODULES); do \
	  cp -f $$module $(TI_filesystem_path)/lib/xtables; \
	done

uninstall:
	@for module in $(MODULES); do\
	  find $(TI_filesystem_path)/lib/xtables -name $$module | xargs rm -f; \
	done


