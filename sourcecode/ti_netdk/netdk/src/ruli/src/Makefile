#-----------------------------------------------------------------
# Copyright 2007, Texas Instruments Incorporated
#
# RULI Source Compilation Makefile
#-----------------------------------------------------------------
-include $(TARGET_HOME)/TI_Build.make
-include $(TARGET_HOME)/.config

OOP_LIB_DIR = $(TARGET_HOME)/ti/lib

#DEBUG += -DRULI_SRV_DEBUG 
#DEBUG += -DRULI_HOST_DEBUG
#DEBUG += -DRULI_RES_DEBUG
#DEBUG += -DRULI_TXT_COMPARE_DEBUG
#DEBUG += -DRULI_SYNC_DEBUG
#DEBUG += -DRULI_SEARCH_DEBUG
#DEBUG += -DRULI_MSG_DUMP_DEBUG -DRULI_SOCK_DUMP_DEBUG
#DEBUG += -DRULI_SMTP_DEBUG
#DEBUG += -DRULI_RFC3484_DEBUG
#DEBUG += -DTI_API_DEBUG

# List of all object Files that create the Test Plugin.
#LIBOBJ = ruli_isaac.o ruli_mem.o ruli_rand.o ruli_util.o ruli_list.o \
#	ruli_addr.o ruli_sock.o ruli_txt.o ruli_msg.o ruli_fsm.o \
#	ruli_res.o ruli_parse.o ruli_host.o ruli_srv.o ruli_conf.o \
#	ruli_search.o ruli_http.o ruli_smtp.o ruli_sync.o \
#	ruli_getaddrinfo.o
LIBOBJ = ruli_isaac.o ruli_mem.o ruli_rand.o ruli_util.o ruli_list.o \
	ruli_addr.o ruli_sock.o ruli_txt.o ruli_msg.o ruli_fsm.o \
	ruli_res.o ruli_parse.o ruli_host.o ruli_srv.o ruli_conf.o \
	ruli_search.o ti_api.o trivial_conf_handler.o
SHAREDOBJ = $(LIBOBJ:%.o=%.os)

#WARN = -ansi -Wshadow -Wpointer-arith -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wredundant-decls
WARN =

# gcc-specific options (not valid for g++)
ifeq ($(CC),gcc)
#WARN += -Wnested-externs
#WARN += -Wmissing-prototypes
#WARN += -Wstrict-prototypes
endif

FLAGS = $(DEBUG) -Os -pipe -Wall -g -shared -D_REENTRANT -I.   

#CFLAGS += $(WARN) -Werror $(FLAGS)
CFLAGS += $(WARN) $(FLAGS)
#LDFLAGS = -shared

SONAME = libruli.so.4
LDFLAGS = -L$(OOP_LIB_DIR)

SHARED = -fPIC
COMPILE = $(CC) $(ARCH) $(CPPFLAGS) $(CFLAGS) -c
SHARED_COMPILE = $(CC) $(ARCH) $(CPPFLAGS) $(SHARED) $(CFLAGS) -o $@ -c

.PHONY: default
default: ruli

.PHONY: clean
clean:
	rm -f *~ .*~ *.o *.os *.so *.a *.so.*

.PHONY: build
build: clean default

.PHONY: strip
strip: ruli
	strip $(SONAME) libruli.a

.PHONY: install
install: ruli
	mkdir -p $(INSTALL_LIB_DIR) $(INSTALL_INCLUDE_DIR) $(INSTALL_MAN3_DIR)
	cp $(SONAME) libruli.a $(INSTALL_LIB_DIR)
	cp *.h $(INSTALL_INCLUDE_DIR)
	cp ../doc/man/*.3 $(INSTALL_MAN3_DIR)
	cd $(INSTALL_LIB_DIR) && ln -s $(SONAME) libruli.so
	@echo
	@echo "REMINDER:"
	@echo "Add $(INSTALL_LIB_DIR) to your dynamic loader path:"
	@echo "# echo $(INSTALL_LIB_DIR) >> /etc/ld.so.conf"
	@echo "# ldconfig"
	@echo

.PHONY: ruli
ruli: $(SONAME) 
#libruli.a

%.o: %.c %.h
	$(COMPILE) $<

%.os: %.c %.h
	$(SHARED_COMPILE) $<

$(SONAME): libruli.so
	cp libruli.so $(SONAME)

libruli.so: $(SHAREDOBJ)
	$(CC) $(ARCH) $(CFLAGS) $(LDFLAGS) -Wl,-soname,$(SONAME) \
		-Wl,--version-script,libruli.ver -o $@ $(SHAREDOBJ) -loop -ldl

libruli.a: $(LIBOBJ)
	ar rcv $@ $(LIBOBJ)
	ar sv $@
