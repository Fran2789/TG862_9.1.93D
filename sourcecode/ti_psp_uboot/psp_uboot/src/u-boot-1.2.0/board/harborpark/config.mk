#
# GPL LICENSE SUMMARY
#
#  Copyright(c) 2011 Intel Corporation.
#
#  This program is free software; you can redistribute it and/or modify 
#  it under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful, but 
#  WITHOUT ANY WARRANTY; without even the implied warranty of 
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License 
#  along with this program; if not, write to the Free Software 
#  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#  The full GNU General Public License is included in this distribution 
#  in the file called LICENSE.GPL.
#
#  Contact Information:
#    Intel Corporation
#    2200 Mission College Blvd.
#    Santa Clara, CA  97052
#


# RAM BASE   => 0x40000000
# RAM SIZE   => 0x02000000 (32MB) : Minimum. It can be more (64MB) but we don't care
# UBOOT SIZE => 0x50000 (320KB)   : Expected size of uboot uzsage (code from flash + data )
# In Puma6 CONFIG_UBOOT_SDRAM_ADDRESS = 0x41FB0000 = 0x40000000 + 0x02000000 - 0x50000
TEXT_BASE = $(CONFIG_UBOOT_SDRAM_ADDRESS)

PLATFORM_CPPFLAGS += -I$(TOPDIR)/cpu/$(CPU)/$(SOC) \
	-UDEBUG -ffunction-sections
PLATFORM_LDFLAGS += -pie

