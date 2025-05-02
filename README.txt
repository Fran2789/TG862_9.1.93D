Copyright (c) 2017 ARRIS Enterprises, LLC.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

Alternatively, this software may be distributed under the terms of the
GNU General Public License ("GPL") version 2 as published by the Free
Software Foundation.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

GPLv2 license:

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.



##############################################################

OSS components of the Touchstone TG862 Cable Voice Gateway
FW Version 9.1.93D

##############################################################

README.txt - This file explains the OSS package contents and how to build.

== CONTENTS==
   
     Overview
     Package Info
     Open Source Toolchain
     Installation
     Build Instructions
     End


== Overview ==

The TG862 (Touchstone(R) TG862 Cable Voice Gateway) is a 16x4 DOCSIS(R) 3.0 compliant 
Gateway with 802.11n WiFi and MoCA(R) 1.1. 
This package contains the files used in the 9.1.93D code on TG862 that are open 
source and meant for re-distribution to the community.


== Package Info ==

A brief description of the files included in the TG862_9.1.93D.tar.bz2 package.

OSS Package contents
--------------------

Here is a list of the OSS packages that 9.1.93D code on the TG862 uses. 
For details of each package, refer to the README in respective directories.

NOTE: "modified" string in the package name implies that the packages are modified by Intel/ARRIS.


7zip-4.57-modified	        : File archiver tool with a high compression ratio

arno-iptables-firewall-modified : Arno's iptables firewall

arris_mod		        : ARRIS utilities kernel module
 
atmel_char		        : Atmel device driver kernel module

avahi-0.6.28-modified           : A zero-configuration networking implementation

bridge_utils-1.0.4-modified     : This package is for Linux ethernet bridge code

busybox-1.19.2-modified	        : Contains free unix utilities

cable_ni-sdk4.5	                : DOCSIS data path kernel module  

core-sdk4.5		        : DOCSIS Filters/QoS Classifiers Control module

dbridge_dlm-sdk4.5	        : DOCSIS Bridge kernel module

dibbler-1.0.0RC1-modified       : A portable DHVPv6 implementation

dlm-sdk4.5		        : Gateway Parental Control Kernel Module

dnsmasq-2.57-modified           : Network services for small networks

dpp_dlm-sdk4.5		        : DOCSIS Packet Processor Kernel Module

ebtables-2.0.10-4               : Linux Ethernet bridge firewalling / filtering

engine_pro_C-07.05.26-modified  : State Machine Engine program in the form of portable standard C

erouter_ni-sdk4.5	        : eRouter test network device Kernel Module

ethtool-6-modified              : Linux utility for controlling network drivers and hardware

expat-2.0.1                     : A stream-oriented XML parser library written in C

extswt                          : ARRIS external Ethernet switch kernel module

e2fsprogs-1.42.6-modified       : Ext2/3/4 Filesystem utilities

fw_env-1.2.0-modified	        : Command line user interface to firmware (=U-Boot) environment

fw-modules-0.1-modified         : Port Scan Detection Kernel Module

hal_isr-sdk4.5		        : DOCSIS HAL Infrastructure Kernel Module

hal_soc_interface_driver-sdk4.5 : DOCSIS SoC Interface driver Kernel Module

hal_mng_q-sdk4.5                : DOCSIS HAL Management Queue Kernel Module

igmpproxy-1.0-modified          : Simple dynamic multicast routing daemon

inadyn-1.99.13                  : Small and simple DDNS client

Intel-Puma-Toolchain_03         : This contains the tool chain installation script and patches

iostat-2.2-modified	        : This contains Linux I/O performance monitoring utility

ipp2p-0.8.2                     : Peer-to-peer (P2P) traffic detection Kernel Module

iproute2-2.6.39-modified        : Utilities for controlling TCP/IP networking and traffic control

iptables-1.4.12.1-modified      : Application to configure Linux kernel firewall tables

ip6_gre.c                       : GRE over IPv6 decoder Linux Kernel Module

kconfig-1.4-modified	        : Utility to save, copy, delete and restore kernel configuration

libdaemon-0.14-modified	        : A lightweight daemon framework in C

libmnl-1.0.3           	        : A minimalistic user-space library oriented to Netlink developers

libnetfilter_conntrack-1.0.1    : Userspace library API to the in-kernel connection tracking state table

libnetfilter_netlink-1.0.0      : Low-level library for netfilter related kernel/userspace communication

liboop-1.0-modified    	        : Low-level event loop management library for POSIX-based operating systems

libsoap-1.1.0-modified          : A client/server SOAP library implemented in pure C

linux-2.6.39.3-modified	        : The linux kernel

l2switch_proxy_driver  	        : Implementation of l2switch proxy Driver Kernel Module

masdk gpl-sdk4.5       	        : Multimedia Application Services Development Kit Kernel Modules

mldproxy-0.1-modified           : Multicast Listener Discovery proxy

mtani-sdk4.5           	        : MTA Network Device Kernel Kernel Module

nbtscan-1.5.1a-modified         : NETBIOS nameserver scanner

ndisc6-1.0.2-modified  	        : IPv6 diagnostic tools

openssl-0.9.8l-modified	        : TLS/SSL and crypto library

quagga-0.99.16-modified         : Routing software suite for OSPFv2/v3, RIPv1/v2/ng, and BP-4

ruli-0.36-modified     	        : Resolver User Layer Interface library for querying DNS SRV resource records

sc_hooks-0.1                    : Gateway utilities Kernel Modules

squashfs-4.2-modified	        : This is a read only file system for Linux

ssmtp-2.64-modified    	        : A program to send mail via a mailhub

ti_sysklogd-1.4.1-modified      : Kernel and system logging daemons

ti_udhcp-0.9.9-modified         : DHCP client application

tibat_char		        : TI device driver kernel module

u-boot-1.2.0-modified           : Bootloader for TG862 gateway

uClibc-0.9.33.2-modified        : This contains the C library functions for embedded Linux systems

udev-0.87-modified	        : A user space implementation of dev file system

udhcp-0.9.7-modified            : DHCP server application

udns-0.0.9-modified             : A stub DNS resolver library

ups_manager		        : ARRIS telemetry kernel module


In addition, the TG862_9.1.93D.tar.bz2 package contains this below file:

README.txt                      : This README file.


== Open Source Toolchain ==

armeb-linux-uclibceabi-gcc -v
gcc version 4.7.3 (Buildroot 2013.08.1)


== Installation ==

The following install and build instructions have been verified on an Ubuntu 12.04.5 LTS 64-bit host.
Make sure you have an Internet connection to create the host environment setup.


HOST ENVIRONMENT SETUP
----------------------

1) Download and install Ubuntu 12.04.5 LTS 64-bit
2) Download the necessary packages using these three shell commands:


sudo apt-get --yes update

sudo apt-get --yes install gcc-4.6 g++-4.6 libc6-i386 lib32ncursesw5-dev \
lib32stdc++6 lib32gcc1 xutils-dev \
apt-file libncurses-dev automake autoconf rpm patch lib32ncurses5 lib32ncurses5-dev \
libncurses5-dev doxygen g++ bison flex gettext texinfo minicom lrzsz gawk fakeroot \
git-core gnupg gperf libsdl-dev libesd0-dev libwxgtk2.6-dev \
build-essential zip zlib1g-dev valgrind gcc-multilib \
g++-multilib ia32-libs x11proto-core-dev libx11-dev lib32z1-dev \
lib32bz2-dev libc6-dev-i386 intltool unifdef \
lib32readline6-dev openssl libssl-dev libstdc++6-4.6-dev curl autopoint xsltproc docbook-xsl

sudo apt-file update


3) Restart to complete the updates

4) Download the "TG1862_9.1.93D.tar.bz2" package
5) cd to the directory where "TG862_9.1.93D.tar.bz2" was downloaded
6) Untar the contents using the command:  tar -xvf TG862_9.1.93D.tar.bz2
   This will create a directory named 'TG862_9.1.93D'
7) cd TG862_9.1.93D
   This directory contains the toolchain 'Intel-Puma-Toolchain_03.tgz' file, 
   the top-level source code directory 'sourcecode', and this README file.


== Build Instructions ==

INSTRUCTIONS TO BUILD TOOL CHAIN
--------------------------------

1) Make a directory for the toolchain build:
   mkdir -p $HOME/toolchains

2) Copy the toolchain file 'Intel-Puma-Toolchain_03.tgz' to that directory:
   cp Intel-Puma-Toolchain_03.tgz $HOME/toolchains/

3) Change direcory to this new location:
   cd $HOME/toolchains

4) Extract the toolchain:
   tar -xvf Intel-Puma-Toolchain_03.tgz

5) Prepare the toochain build install directory environment variable:
   export INTEL_PUMA_TOOLCHAIN_INSTALL_DIR=$HOME/toolchains/Intel-Puma-Toolchain_03

6) Change directory into the extracted toolchain source:
   cd Intel-Puma-Toolchain_03

7) Start the toolchain build with this command:
   ./build-toolchain-13.08.1.sh

   Most toolchains take quite a while to build. When done, the toolchain executables are in the folder:
   $INTEL_PUMA_TOOLCHAIN_INSTALL_DIR/usr/bin

8) Add the toolchain executables folder to the PATH environment variable so the build process can access it.
   export PATH=$HOME/toolchains/Intel-Puma-Toolchain_03/usr/bin:$PATH

   To make this path permanent, add it to your system's $HOME/.bashrc file.


INSTRUCTIONS TO BUILD TG862 SOURCE CODE
----------------------------------------

1) Change directory back to 'TG862_9.1.93D'

2) cd sourcecode

3) Copy the contents of the m4 directory to the toolchain's usr/share/aclocal directory:
   cp m4/* $INTEL_PUMA_TOOLCHAIN_INSTALL_DIR/usr/share/aclocal 

4) cd TS_nextgen

5) In the 'TS_nextgen' directory, issue this command for initial configuration:
   make buildconfig PRODUCT=VGWSDK DOCSIS_SOC=PUMA6

6) To build the entire TG862 product, issue this last command:
   make vgwsdk

7) Resulting target filesystems can be found in /TG862_9.1.93D/sourcecode/TS_nextgen/build/vgwsdk/fs/
   The fs directory contains two filesystem structures:
   a) the "base_fs" directory, for CM/MTA applications & libraries (i.e. busybox, DOCSIS networking)
   b) the "gw" directory, for Gateway/router applications & libraries.
    
= END =


