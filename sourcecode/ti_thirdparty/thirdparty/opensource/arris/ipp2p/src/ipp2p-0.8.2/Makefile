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
$(error TI_filesystem_path is not defined in gw/src/ipp2p/src/ipp2p-0.8.2/Makefile)
endif

#KERNELDIR =$(shell pwd)/../../../TI/kernel/ti/linux-2.6.39.3/src/
KERNEL_NETFILTER_DIR = $(KERNELDIR)/net/ipv4/netfilter
KERNEL_SIPDIR = $(KERNELDIR)/net/ipv4/netfilter
SUBDIR = $(GW_PREFIX)/src/ipp2p/src/ipp2p-0.8.2

OBJS = ipt_ipp2p.ko

obj-m:= ipt_ipp2p.o

#CFLAGS+= -DeGW

IPTABLES_SRC = $(GW_PREFIX)/src/iptables/src
IPTABLES_VERSION = $(shell cat $(IPTABLES_SRC)/Makefile | grep -e '^PACKAGE_VERSION := ' | cut -d"=" -f2)
IPTABLES_OPTION = -DIPTABLES_VERSION=\"$(IPTABLES_VERSION)\"
IPTABLES_INCLUDE = -I$(IPTABLES_SRC)/include

all:modules

modules:
	@# ARRIS ADD
	@echo "  *** CP ipt_ipp2p.h ***"
	@install -p -m 0644 $(GW_PREFIX)/src/ipp2p/src/ipp2p-0.8.2/ipt_ipp2p.h $(TARGET_HOME)/ti/include/ipt_ipp2p.h
	@# END ARRIS CHANGE

	@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules
	#$(MAKE) -C $(KERNELDIR) SUBDIRS=$(SUBDIR) modules V=1 CROSS_COMPILE=$(CROSS_COMPILE)

libipt_ipp2p.so: libipt_ipp2p.c ipt_ipp2p.h
	$(CC) $(CFLAGS) $(IPTABLES_OPTION) $(IPTABLES_INCLUDE) -fPIC -c libipt_ipp2p.c
	ld -shared -o libipt_ipp2p.so libipt_ipp2p.o

clean:
	rm -rf $(OBJS) *.o *.ko *.cmd .*.cmd

install:
	@if [ ! -d $(TI_filesystem_path)/lib/xtables ]; then mkdir $(TI_filesystem_path)/lib/xtables; fi
	cp -f $(OBJS) $(TI_filesystem_path)/lib/xtables/
	#@$(MAKE) -C $(KERNEL_DIR) M=`pwd` ARCH=arm modules_install


uninstall:
	@find $(TI_filesystem_path)/lib/xtables -name $(OBJS) | xargs rm -f


