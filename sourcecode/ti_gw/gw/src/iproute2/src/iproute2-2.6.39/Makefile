# ------------------------------------------------------------------------------
#
# Includes Intel Corporation's changes/modifications dated:
#   26-Oct-2011: Compiling with Size Optimizations.
#   26-Oct-2011: Cross compilation
#   26-Oct-2011: Installation to target filesystem
#
# Changed/modified portions - Copyright 251 2011, Intel Corporation.
#
#
#
# THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
# "AS IS," AND INTEL CORPORATION MAKES NO REPRESENTATIONS 
# OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
# TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY 
# PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR 
# DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, 
# COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. 

#-------------------------------------------------------------------------------------

# Include this file that contains definitions.
-include $(TARGET_HOME)/TI_Build.make
DESTDIR=$(TI_filesystem_path)
ROOTDIR=$(DESTDIR)
LIBDIR=$(TI_filesystem_path)/lib/
SBINDIR=$(TI_filesystem_path)/sbin
CONFDIR=$(TI_filesystem_path)/etc/iproute2
DOCDIR=/usr/share/doc/iproute2
MANDIR=/usr/share/man

KERNEL_INCLUDE=$(KERNEL_DIR)/src/include

ARPDDIR=/var/lib/arpd

# Path to db_185.h include
DBM_INCLUDE:=$(ROOTDIR)/usr/include

SHARED_LIBS = y

DEFINES= -DRESOLVE_HOSTNAMES -DLIBDIR=\"$(LIBDIR)\"
ifneq ($(SHARED_LIBS),y)
DEFINES+= -DNO_SHARED_LIBS
endif

#options if you have a bind>=4.9.4 libresolv (or, maybe, glibc)
LDLIBS=
ADDLIB=

CCC = $(CROSS)gcc
HOSTCC = $(CROSS)gcc
CCOPTS = -D_GNU_SOURCE -Wstrict-prototypes $(CFLAGS_Warn) $(CFLAGS_optimize) $(TI_cflags_ARMThumb)
CFLAGS = $(CCOPTS) -I../include $(TI_include) -fpic $(DEFINES)
YACCFLAGS = -d -t -v


LIBNETLINK=../lib/libnetlink.a ../lib/libutil.a
LDLIBS += $(LIBNETLINK)

# The package is a generic package that can build both the "ip" and "tc" 
# applications. We need to select which application is needed to be built 
# depending on the configuration. The "lib" is a common library that 
# always needs to be built.
SUBDIRS=lib
SUBDIRS += ip

ifndef UMF_DEBUG_LEVEL
STDOUT :=2>/dev/null 1>/dev/null
endif
all: Config build
build:
	@set -e; \
	for i in $(SUBDIRS); \
	do $(MAKE) $(MFLAGS) -C $$i $(STDOUT); done

Config:
	sh configure $(TI_include)

install:
	mkdir -p $(CONFDIR)
	install -m 0644 $(shell find etc/iproute2 -maxdepth 1 -type f) $(CONFDIR)

snapshot:
	echo "static const char SNAPSHOT[] = \""`date +%y%m%d`"\";" \
		> include/SNAPSHOT.h

clean:
	@rm -f cscope.*
	@for i in $(SUBDIRS) doc; \
	do $(MAKE) $(MFLAGS) -C $$i $(STDOUT); done
	@rm -rf $(CONFDIR)

clobber:
	touch Config
	@$(MAKE) $(MFLAGS) clean $(STDOUT)
	@rm -f Config

distclean: clobber

cscope:
	cscope -b -q -R -Iinclude -sip -slib -smisc -snetem -stc

.EXPORT_ALL_VARIABLES:
