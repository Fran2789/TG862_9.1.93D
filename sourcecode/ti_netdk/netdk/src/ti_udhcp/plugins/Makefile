#-----------------------------------------------------------------
# Copyright 2006, Texas Instruments Incorporated
#
# Plugin Makefile that is invoked for all TI UDHCPC Plugins 
#-----------------------------------------------------------------

# Use the configuration file to determine the PLUGINs that need to be built.
#-include ../../.config

# Use this MACRO to pass any common C Flags that are shared by all PLUGINS
PLUGIN_CFLAGS = "-I../.. -Os"

# Use this MACRO to pass any common LD flags. All plugins are shared libraries.
LDFLAGS=-shared

# The TI_UDHCPC Plugin is always built.
PLUGINS-y += sample/udhcp_sample_plugin.so

# Build all the Configured Plugins.
all: $(PLUGINS-y)

#-----------------------------------------------------------------
# Individual Rule for building each Plugin.
#-----------------------------------------------------------------
sample/udhcp_sample_plugin.so:
	$(MAKE) -C sample CC=$(CC) PLUGIN_CFLAGS=$(PLUGIN_CFLAGS) LDFLAGS=$(LDFLAGS) all

#-----------------------------------------------------------------
# Clean all the plugins
#-----------------------------------------------------------------
clean:
	$(MAKE) -C sample clean

