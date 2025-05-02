# 
#  GPL LICENSE SUMMARY
#
#  Copyright(c) 2005-2011 Intel Corporation.
#
#  This program is free software; you can redistribute it and/or modify 
#  it under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful, but 
#  WITHOUT ANY WARRANTY; without even the implied warranty of 
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License 
#  along with this program; if not, write to the Free Software 
#  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#  The full GNU General Public License is included in this distribution 
#  in the file called LICENSE.GPL.
#
#  Contact Information:
#    Intel Corporation
#    2200 Mission College Blvd.
#    Santa Clara, CA  97052
#

# *************************************************************************
# * Makefile Wrapper file for building avahi  package
# *************************************************************************

#include $(TARGET_HOME)/TI_Build.make
include $(TARGET_HOME)/.config

PACKAGE_VERSION := 0.6.28
PACKAGE_DIR := avahi-$(PACKAGE_VERSION)
BUILD_DIR := build
#disable internationalisation -USE_NLS=no, --disable-nls
export USE_NLS='no'
PACKAGE_CONF := \
	--host=$(patsubst %-gcc,%,$(CROSS)gcc) \
	--exec-prefix="" \
	--prefix=""  \
	--datadir=/usr/share/avahi \
	--mandir=/usr/share/avahi \
	--sysconfdir=/var/tmp  \
	--localedir=/usr/share/avahi \
	--localstatedir=/var \
	--disable-nls  \
	--disable-glib \
	--disable-gobject \
	--disable-qt3 \
	--disable-qt4  \
	--disable-gtk \
	--disable-gtk3 \
	--disable-dbus \
	--disable-gdbm \
	--disable-python \
	--disable-pygtk \
	--disable-python-dbus  \
	--disable-mono        \
	--disable-monodoc     \
	--disable-doxygen-doc  \
	--disable-doxygen-dot   \
	--disable-doxygen-xml \
	--disable-doxygen-html \
	--disable-manpages     \
	--disable-xmltoman     \
	--with-gnu-ld \
	--with-xml=expat \
	--with-avahi-user=root \
	--with-avahi-group=root \
	--with-avahi-priv-access-group=root \
	--with-autoipd-user=root \
	--with-autoipd-group=root \
	--with-distro=none \
	CPPFLAGS=" -DDISABLE_SYSTEMD" \
	HAVE_GLIB=no \
	USE_NLS='no' \
	USER_LINGUAS='en_GB' \
	LIBDAEMON_CFLAGS=" -D_REENTRANT -I$(TARGET_HOME)/ti/include -I$(TARGET_HOME)/ti/include/libdaemon" \
	LIBDAEMON_LIBS=" -L$(TARGET_HOME)/ti/lib -ldaemon"


export CFLAGS += $(CFLAGS_optimize) $(CFLAGS_so)
export LDFLAGS += $(LDFLAGS_optimize)

first_rule: all

all: build 

$(CURDIR)/$(BUILD_DIR)/Makefile:
	mkdir -p $(CURDIR)/$(BUILD_DIR)	
	rm -f  $(CURDIR)/$(PACKAGE_DIR)/config.h.in
	
	cp -f $(CURDIR)/$(PACKAGE_DIR)/config.h.in_orig $(CURDIR)/$(PACKAGE_DIR)/config.h.in
	chmod +w $(CURDIR)/$(PACKAGE_DIR)/config.h.in
	@# ARRIS CHANGE - MTC - added  autoreconf ivf instead of iv
	cd $(CURDIR)/$(PACKAGE_DIR) && autoreconf -ivf
	cd $(CURDIR)/$(BUILD_DIR) && ../$(PACKAGE_DIR)/configure INSTALL="/usr/bin/install -p -c" $(PACKAGE_CONF)
	@# END ARRIS CHANGE

build: $(CURDIR)/$(BUILD_DIR)/Makefile
	$(MAKE) -C $(CURDIR)/$(BUILD_DIR) all USE_NLS='no' USER_LINGUAS='en_GB'

config_rule:

# Perhaps is is a bug , but it is impossible to disable all languages, so en_GB stands 
export USER_LINGUAS='en_GB'
install_rule:
	@echo "***"
	@echo "***"
	@echo "*** Avahi DESTDIR : $(TI_filesystem_path)"
	@echo "***"
	@echo "***"
	$(MAKE) -C $(CURDIR)/$(BUILD_DIR) DESTDIR=$(TI_filesystem_path) install USE_NLS='no' USER_LINGUAS='en_GB'
	@if [ -f $(TI_filesystem_path)/lib/libavahi-common.a ]; then mv -f $(TI_filesystem_path)/lib/libavahi-common.a $(TARGET_HOME)/ti/lib/libavahi-common.a; fi; 
	@if [ -f $(TI_filesystem_path)/lib/libavahi-core.a ]; then mv -f $(TI_filesystem_path)/lib/libavahi-core.a $(TARGET_HOME)/ti/lib/libavahi-core.a; fi;
	rm -f $(TI_filesystem_path)/var/tmp/avahi/avahi-daemon.conf
# remove internationaliozation messages after install
	rm -rf $(TI_filesystem_path)/usr/share/avahi
	rm -rf $(TI_filesystem_path)/lib/avahi	


clean_rule:
	@if [ -f $(CURDIR)/$(BUILD_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(BUILD_DIR) distclean -i; fi


uninstall_rule:
	@if [ -f $(CURDIR)/$(BUILD_DIR)/Makefile ]; then $(MAKE) -C $(CURDIR)/$(BUILD_DIR) uninstall -i; fi


