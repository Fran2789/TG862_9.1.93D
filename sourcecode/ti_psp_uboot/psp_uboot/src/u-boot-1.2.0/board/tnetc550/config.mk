# config.mk
# Description:
# tnetc550 build config.
#
#
# Copyright (C) 2008, Texas Instruments, Incorporated
#
#  This program is free software; you can distribute it and/or modify it
#  under the terms of the GNU General Public License (Version 2) as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
#  for more details.
#
#  You should have received a copy of the GNU General Public License along
#  with this program; if not, write to the Free Software Foundation, Inc.,
#  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
#

# !@T OLDMM TEXT_BASE = 0x11FF0000	# 0x10000000 + 0x02000000 - 0x40000

# In Puma5 it is 0x81FB0000 = 0x80000000 + 0x02000000 - 0x50000
TEXT_BASE = $(CONFIG_UBOOT_SDRAM_ADDRESS)
 
PLATFORM_CPPFLAGS += -I$(TOPDIR)/cpu/$(CPU)/$(SOC) \
	-UDEBUG -ffunction-sections

