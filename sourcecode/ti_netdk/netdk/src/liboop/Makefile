#-----------------------------------------------------------------
#
# LIBOOP Library.
# version 1.0
#-----------------------------------------------------------------
-include $(TARGET_HOME)/TI_Build.make

# Define the tools that are to be used.
CC=$(CROSS)gcc
RANLIB = $(CROSS)ranlib

# List of all object Files that create the Web Server.
OBJECTS= sys.o select.o signal.o alloc.o read.o \
	read-fd.o read-mem.o
LIBOBJS= sys.o select.o signal.o alloc.o read.o \
	read-fd.o read-mem.o

# Linker Flags.
LDFLAGS = -shared
#LDFLAGS = 

LIBOOP = liboop 
LIBOOPLIB = liboop.a
#LIBOBJS =
LIBS = 

# CFLAGS Definitons.
CFLAGS += -Os -Wall -rdynamic -fPIC

DEFS = -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE=\"liboop\" -DVERSION=\"1.0\" -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DHAVE_DLFCN_H=1 -DHAVE_POLL_H=1 -DHAVE_SYS_SELECT_H=1 -DHAVE_SYS_SOCKET_H=1 -DHAVE_STRING_H=1 -DHAVE_STRINGS_H=1 

# Build the liboop
all: $(LIBOOP)

$(LIBOOP): $(OBJECTS) $(LIBOOPLIB)
#all: $(OBJECTS)
#	 $(CC) $(DEFS) $(LDFLAGS) -o liboop.so $^
	$(CC) $(CFLAGS) $(DEFS) -o $(LIBOOP) $(LDFLAGS) $(OBJECTS) $(LIBOOPLIB) $(LIBS)

$(LIBOOPLIB): $(LIBOBJS)
	$(AR) cru $(LIBOOPLIB) $(LIBOBJS)
	$(RANLIB) $(LIBOOPLIB) 

# Clean
clean:
	-rm -rf $(OBJECTS)
	-rm -f liboop.a
	-rm -f liboop

# Rule for creating the LIBOOP Library
%.so: %.c
	$(CC) -o $@ $(LDFLAGS) $(CFLAGS) $^ 

# Rule for building the C Files.
%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $^

