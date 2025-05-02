# *************************************************************************
# * Makefile Wrapper file for building ndisc6 package
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

PACKAGE_VERSION := 1.0.2
PACKAGE_DIR := ndisc6-$(PACKAGE_VERSION)
PACKAGE_CONF := --host=armeb-unknown-linux-uclibcgnueabi --prefix=$(TI_filesystem_path) \
        --exec-prefix=$(TI_filesystem_path) --datarootdir=$(CURDIR)

# ARRIS ADD - Optimizations
export CFLAGS += -Os
# END ARRIS ADD

first_rule: all

all: build

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	@cd $(CURDIR)/$(PACKAGE_DIR) && autoreconf -fi
	cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc ./configure INSTALL="/usr/bin/install -p -c" $(PACKAGE_CONF)

build: $(CURDIR)/$(PACKAGE_DIR)/Makefile
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) all

config_rule: $(CURDIR)/$(PACKAGE_DIR)/Makefile

install_rule install:
	$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) install
	@echo "***"
	@echo "***"
	@echo "*** ndisc6 prefix : $(TI_filesystem_path)"
	@echo "***"
	@echo "***"

clean_rule distclean clean:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi
	@rm -rf $(CURDIR)/man

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) uninstall -i; fi

