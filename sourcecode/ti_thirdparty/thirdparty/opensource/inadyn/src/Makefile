# *************************************************************************
# * Makefile Wrapper file for building inadyn package
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

PACKAGE_VERSION := 1.99.13
PACKAGE_DIR := inadyn-$(PACKAGE_VERSION)
TARGET := inadyn
PACKAGE_CONF := \
    --host=$(patsubst %-gcc,%,$(CROSS)gcc) --disable-ssl --prefix=$(TI_filesystem_path) \
    --exec-prefix=$(TI_filesystem_path)

# ARRIS ADD - Optimizations
export CFLAGS += -Os
# END ARRIS ADD

first_rule: all

all: build

$(CURDIR)/$(PACKAGE_DIR)/Makefile:
	@cd $(CURDIR)/$(PACKAGE_DIR) && autoreconf -fi
	cd $(CURDIR)/$(PACKAGE_DIR) && CC=$(CROSS)gcc ./configure $(PACKAGE_CONF)

build: $(CURDIR)/$(PACKAGE_DIR)/Makefile
	@$(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) all

config_rule: $(CURDIR)/$(PACKAGE_DIR)/Makefile

install_rule install: build
	@install -d $(TI_filesystem_path)/usr/sbin
	@install -m 0755 $(CURDIR)/$(PACKAGE_DIR)/src/$(TARGET) $(TI_filesystem_path)/usr/sbin/
#	@cp $(CURDIR)/$(PACKAGE_DIR)/src/$(TARGET) $(TI_filesystem_path)/usr/sbin/
#	@chmod 0755 $(TI_filesystem_path)/usr/sbin/$(TARGET)

clean_rule distclean clean:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) distclean -i; fi
	@rm -rf $(CURDIR)/man

uninstall_rule:
	@if [ -f $(CURDIR)/$(PACKAGE_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(PACKAGE_DIR) uninstall -i; fi
	