AUTO_GENERATE_C_FILES := $(shell ls *.c 2> /dev/null)
AUTO_GENERATE_O_FILES := $(patsubst %.c,%.o,$(AUTO_GENERATE_C_FILES))
CC=gcc
CFLAGS += -DHAVE_MLDV2 -DHAVE_RFC3542 -Werror

all:mldproxy
%.o : %.c 
	$(CROSS)$(CC) $(CFLAGS) -I. -I$(TARGET_HOME)/ti/include -g -c -Wstrict-prototypes -o $@  $<

mldproxy:$(AUTO_GENERATE_O_FILES) Makefile
	$(CROSS)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(AUTO_GENERATE_O_FILES)

config_rule:

distclean : clean  uninstall
clean clean_rule:
	rm -f mldproxy *.o

install install_rule:mldproxy
	install -m 755 $^ $(TI_filesystem_path)/bin
	$(CROSS)strip $(TI_filesystem_path)/bin/$^
	install -m 644 mld.conf $(TI_filesystem_path)/etc

uninstall uninstall_rule:
	rm -f $(TI_filesystem_path)/etc/mld.conf $(TI_filesystem_path)/bin/mldproxy
