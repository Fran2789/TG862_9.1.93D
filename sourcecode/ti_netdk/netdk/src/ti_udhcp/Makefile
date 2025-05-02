# -------------------------------------------------------------------------------------
# Copyright 2006, Texas Instruments Incorporated
#
# This program has been modified from its original operation by Texas Instruments
# to do the following:
#	Customisation to build with rgdk framework
#
#  THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
#  "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
#  OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
#  TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
#  PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
#  DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
#  COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
#  
#  These changes are covered as per original license.
# -------------------------------------------------------------------------------------
# ti udhcp makefile
-include $(TARGET_HOME)/TI_Build.make
-include $(TARGET_HOME)/.config

ifeq ($(CONFIG_TI_TIUDHCPC_SAMPLE_PLUGIN),y) 
SAMPLE_PLUGIN=1
endif

prefix=$(TI_filesystem_path)
SBINDIR=${prefix}/sbin
USRSBINDIR=${prefix}/sbin
USRBINDIR=${prefix}/bin
USRSHAREDIR=${prefix}/usr/share

# Uncomment this to output messages to syslog, otherwise, messages go to stdout
CFLAGS += -Os

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc
INSTALL = install

VER := 1.0


OBJS_SHARED = options.o socket.o packet.o udhcp_alloc.o arpping.o
DHCPC_OBJS = dhcpc.o clientpacket.o script.o udhcpc_plugin.o udhcp_default_plugin.o

EXEC = ti_udhcpc
OBJS = $(DHCPC_OBJS) $(OBJS_SHARED)

BOOT_PROGRAMS = ti_udhcpc
ifdef SYSLOG
CFLAGS += -DSYSLOG
endif

CFLAGS += -rdynamic -W -Wall -Wstrict-prototypes -DVERSION='"$(VER)"' 

#DEBUG=1
ifdef DEBUG
CFLAGS += -g -DDEBUG
else
CFLAGS += -Os -fomit-frame-pointer
endif
STRIP=


LDFLAGS = -rdynamic -lresolv -ldl -lm
LDFLAGS += -L$(TARGET_HOME)/ti/lib -lpthread

ifeq ($(CONFIG_TI_DMALLOC_UTIL),y)
#un commentthe below lines to enable dmalloc for this process
#CFLAGS += -DCONFIG_DMALLOC_ARRIS
#LDFLAGS += -ldmallocth
endif
ifdef SAMPLE_PLUGIN
all: $(EXEC) plugin
else
all: $(EXEC)
endif

$(OBJS): *.h Makefile
$(EXEC): Makefile

.c.o:
	$(CC) -c $(CFLAGS) $<

$(EXEC): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(EXEC)


install:

	$(INSTALL) $(BOOT_PROGRAMS) $(SBINDIR)
	@# ARRIS CHANGE
	$(INSTALL) -p -m 0644 udhcpc_plugin.h $(TI_include)
	$(INSTALL) -p -m 0644 udhcp_packet.h  $(TI_include)
	$(INSTALL) -p -m 0644 udhcp_cfg.h  $(TI_include)
	$(INSTALL) -p -m 0644 udhcp_debug.h $(TI_include)
	@# END ARRIS CHANGE

ifdef SAMPLE_PLUGIN
	$(INSTALL) -m 0644 plugins/sample/udhcpc_sample_plugin.so $(TI_filesystem_path)$(TI_rlibdir)
endif

plugin:
	$(MAKE) CC=$(CC) -C plugins all

plugin_clean:
ifdef SAMPLE_PLUGIN
	$(MAKE) CC=$(CC) -C plugins clean
endif


clean:plugin_clean
	-rm -f ti_udhcpc *.o core 
	-rm -f $(SBINDIR)/$(BOOT_PROGRAMS)
	-rm -f $(TI_include)/udhcp_packet.h
	-rm -f $(TI_include)/udhcpc_plugin.h
	-rm -f $(TI_include)/udhcp_debug.h
ifdef SAMPLE_PLUGIN
	-rm -f $(TI_filesystem_path)$(TI_rlibdir)/udhcpc_sample_plugin.so
endif

