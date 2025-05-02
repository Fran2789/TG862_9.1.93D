#-----------------------------------------------------------------
# Copyright 2006, Texas Instruments Incorporated
#
# HTTP Plugin Makefile 
#-----------------------------------------------------------------

# List of all object Files that create the Test Plugin.
OBJECTS= udhcpc_sample_plugin.o

# Inherit the common options that have been passed.
# Add any other PLUGIN Specific Options here.
CFLAGS += $(PLUGIN_CFLAGS)

# Build the Plugin.
all:$(OBJECTS)
	$(CC) $(LDFLAGS) -o udhcpc_sample_plugin.so $^

# Clean the Plugin and all related object files.
clean: 
	-rm -rf $(OBJECTS)
	-rm -rf udhcpc_sample_plugin.so

# Rule for creating the LIBRARY plugin
%.so: %.c
	$(CC) -o $@ $(LDFLAGS) $(CFLAGS) $^ 

# Rule for creating the OBJECT Files.
%.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $^


