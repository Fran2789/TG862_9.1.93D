.PHONY: dummy

all: build install

-include $(TARGET_HOME)/TI_Build.make
-include $(TARGET_HOME)/.config

# ARRIS MOD BEGIN : remove squashfs - I can't get it to build in clearcase
# subdir-y = src/kconfig src/ver src/mkimage src/7zip src/squashfs src/srec
# subdir-y = src/kconfig src/ver src/mkimage src/7zip src/srec
subdir-y = src/kconfig src/7zip
# ARRIS MOD END 

SUB_DIRS = $(subdir-y)

build-list = $(patsubst %, _build_%, $(SUB_DIRS))
install-list = $(patsubst %, _install_%, $(SUB_DIRS))
clean-list = $(patsubst %, _clean_%, $(SUB_DIRS))
distclean-list = $(patsubst %, _distclean_%, $(SUB_DIRS))

config: dummy
build: dummy $(build-list)
install: dummy $(install-list)
uninstall: dummy $(uninstall-list)
clean: dummy $(clean-list)
distclean: dummy $(distclean-list)

ifdef SUB_DIRS
$(build-list) : dummy
	$(MAKE) -C $(patsubst _build_%,%,$@) -f Makefile.flat build
endif

ifdef SUB_DIRS
$(install-list) : dummy
	$(MAKE) -C $(patsubst _install_%,%,$@) -f Makefile.flat install
endif

ifdef SUB_DIRS
$(uninstall-list) : dummy
	$(MAKE) -C $(patsubst _uninstall_%,%,$@) -f Makefile.flat uninstall
endif

ifdef SUB_DIRS
$(clean-list) : dummy
	@ $(MAKE) -C $(patsubst _clean_%,%,$@) -f Makefile.flat clean
endif

ifdef SUB_DIRS
$(distclean-list) : dummy
	@ $(MAKE) -C $(patsubst _distclean_%,%,$@) -f Makefile.flat distclean
endif

package:
	$(TARGET_HOME)/tools/scripts/mkpkg

dummy:
