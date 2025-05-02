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

# 
# Includes Intel Corporation's changes/modifications dated: 2011. 
# Changed/modified portions - Copyright © 2011 , Intel Corporation.   
# 

include $(TOPDIR)/config.mk

LIB	= $(obj)lib$(SOC).a

COBJS	:= timer.o spi_sfi.o puma6_hw_mutex.o puma6_gpio_ctrl.o signature.o mmc_utils.o puma6_aep.o
SOBJS	= reset.o

SRCS	:= $(SOBJS:.o=.S) $(COBJS:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS))
SOBJS	:= $(addprefix $(obj),$(SOBJS))

$(LIB):	$(obj).depend $(OBJS) $(SOBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS) $(SOBJS)

clean:
	rm -f $(SOBJS) $(OBJS)

distclean:	clean
	rm -f $(LIB) core *.bak .depend

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
