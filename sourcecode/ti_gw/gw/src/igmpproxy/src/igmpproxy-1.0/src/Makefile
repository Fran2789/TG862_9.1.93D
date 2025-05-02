# ARRIS MOD START
# we need the .config at top, because we need compile flag
include $(TARGET_HOME)/.config
#ARRIS MOD END

AUTO_GENERATE_C_FILES := $(shell ls *.c 2> /dev/null)
AUTO_GENERATE_O_FILES := $(patsubst %.c,%.o,$(AUTO_GENERATE_C_FILES))
	#$(CROSS)$(CC) -I. -I/vobs/ua_sdk/ti/include -g -c -Wstrict-prototypes -o $@  $<
CC=gcc
CFLAGS= -Wstrict-prototypes -g -Wno-system-headers -Wformat=2 -Wshadow -Wcast-qual -Wwrite-strings -W

# ARRIS ADD
ifeq ($(CONFIG_TI_DMALLOC_UTIL),y)
#uncomment the below lines to enable dmalloc for this process
#CFLAGS += -DCONFIG_DMALLOC_ARRIS -I$(TI_include)
#LDFLAGS += -ldmallocth
endif

# END ARRIS ADD

OBJS:=$(AUTO_GENERATE_O_FILES)
# pull in dependency info for *existing* .o files

all: $(OBJS) igmpproxy

%.o : %.c
	$(CROSS)$(CC) $(CFLAGS) -I. -c -o $*.o  $*.c
	$(CROSS)$(CC) -MM  $*.c > $*.d

-include $(OBJS:.o=.d)

igmpproxy: $(AUTO_GENERATE_O_FILES) Makefile
	$(CROSS)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(AUTO_GENERATE_O_FILES)


config_rule:

distclean : clean  uninstall
clean:
	rm -f igmpproxy *.o *.d

install install_rule: igmpproxy
	install -m 755 $^ $(TI_filesystem_path)/bin
	$(CROSS)strip $(TI_filesystem_path)/bin/$^

uninstall uninstall_rule:
	rm -f  $(TI_filesystem_path)/bin/igmpproxy
