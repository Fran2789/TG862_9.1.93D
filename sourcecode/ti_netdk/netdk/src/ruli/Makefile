#
# $Id: Makefile,v 1.9 2004/06/24 05:02:24 evertonm Exp $
#
# This is a Debian-friendly top-level Makefile.
#-----------------------------------------------------------------
# Copyright 2007, Texas Instruments Incorporated
#
# Makefile has been modified to:
#   1. Compile in the ti_sample programs
#   2. Compile out the sample, tools 
#   3. Suit the netdk build process
#-----------------------------------------------------------------

#Intel
#Compile in ti_dns_app

-include $(TARGET_HOME)/TI_Build.make
-include $(TARGET_HOME)/.config

CFLAGS += -Os -Wall -rdynamic

#.PHONY: default
.PHONY: all
all:
	$(MAKE) CC=$(CC) -C src   
	$(MAKE) CC=$(CC) -C ti_sample 
	$(MAKE) CC=$(CC) -C intel_dns_app

.PHONY: clean
clean:
	$(MAKE) CC=$(CC) -C src    clean
	$(MAKE) CC=$(CC) -C ti_sample clean
	$(MAKE) CC=$(CC) -C intel_dns_app clean

.PHONY: install
install:
	$(MAKE) CC=$(CC) -C src    install
	$(MAKE) CC=$(CC) -C ti_sample install
	$(MAKE) CC=$(CC) -C intel_dns_app install

.PHONY: dpkg
dpkg:
	dpkg-buildpackage -rfakeroot

.PHONY: dpkg-clean
dpkg-clean:
	fakeroot debian/rules clean

.PHONY: release-clean
release-clean:
	rm -rf debian install doc/rfc ruli-mta test tube \
		`find . -name CVS -o -name "*~"`

