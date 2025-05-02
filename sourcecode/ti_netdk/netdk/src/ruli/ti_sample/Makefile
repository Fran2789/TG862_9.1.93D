#-----------------------------------------------------------------
# Copyright 2007, Texas Instruments Incorporated
#
# Makefile to build TI Sample code to test RULI Library
#-----------------------------------------------------------------

-include $(TARGET_HOME)/TI_Build.make

OOP_INCLUDE_DIR = $(TARGET_HOME)/ti/include
OOP_LIB_DIR = $(TARGET_HOME)/ti/lib
INSTALL_BIN_DIR = $(TI_filesystem_path)$(TI_sbindir)

PLATFORM=$(shell uname)

ifeq ($(PLATFORM),SunOS)
SOLARIS_LIBS = -lresolv -lnsl -lsocket
endif

CC      = gcc
CFLAGS  = -Os -pipe -Wall -g -I../src -I$(OOP_INCLUDE_DIR)
LDFLAGS = -L../src -L$(OOP_LIB_DIR) $(SOLARIS_LDFLAGS)
UNRULI_LINK = $(CC) $(ARCH) $(LDFLAGS) -o $@ $^ $(SOLARIS_LIBS)
LINK = $(UNRULI_LINK) $(EXTRA_LINK) -lruli -loop -lpthread



TARGETS = ti_api_demo 
SRV_SHARED_OBJS = stdout_srv_list.o  

.PHONY: default
default: $(TARGETS)

.PHONY: clean
clean:
	rm -f *~ .*~ *.o $(TARGETS)
	rm -f $(TI_filesystem_path)$(TI_sbindir)/ti_api_demo

.PHONY: build
build: clean default

.PHONY: strip
strip: $(TARGETS)
	strip $(TARGETS)

.PHONY: install
install: $(TARGETS)
	mkdir -p $(INSTALL_BIN_DIR) 
	cp $(TARGETS) $(INSTALL_BIN_DIR)

%.o: %.c
	$(CC) $(ARCH) $(CPPFLAGS) $(CFLAGS) -c $<

ti_api_demo: ti_api_demo.o $(SRV_SHARED_OBJS)
	$(LINK)
