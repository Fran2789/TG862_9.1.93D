/*
 * lava_nogig.c
 * Description:
 * See below.
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

//===========================================================================
//  lava_nogig - Disable gigabit capabilities on Lava board PHYs
//
//  History:
//  1.01 20Apr06 BEGR - Perform operation for Marvell PHYs only
//                    - Added MdioRd, MdioWr, and MdioSetBits functions
//  1.00 18Mar05 BEGR - Original version written
//===========================================================================
#include <common.h> 

#if (CONFIG_COMMANDS & CFG_CMD_NET) 
#ifdef CONFIG_PUMA5_VOLCANO_EMU

#include "../../cpu/arm1176/puma5/mdio_reg.h"

#define VERSION       "1.01"

#define UINT32  unsigned int
#define UINT16  unsigned short

UINT32 MdioBase=0, MdioResetBit=0, MdioResetMask=0;
UINT32 RCR_BASE;
int verbose = 0;

int IsMarvellGigPhy(int PhyAddr)
{
  if( MdioRd(MdioBase, 0, PhyAddr,2) == 0x0141 )       // Read OUI MSbs
  {
    UINT16 PhyID;

    PhyID = MdioRd(MdioBase, 0, PhyAddr,3) & ~0xF;     // Read OUI LSb and model number (ignore rev number)
    if( PhyID==0x0CC0 || PhyID==0x0CD0 )
      return 1;
  }
  return 0;
}

int lava_nogig(void)
{
  int i;

  int FirstTime=1;
  UINT32 ActivePhys;

  verbose = 1;
  //-------------------------------------------------------------------------
  //  Reset controller base.
  //-------------------------------------------------------------------------
  RCR_BASE = 0x08611600;
  MdioBase = 0x03048000;
  MdioResetBit = 22;
  MdioResetMask = 1<<MdioResetBit;
  
  if(verbose)
  {
    printf("     MdioBase = 0x%08x\n",MdioBase);
    printf("MdioResetMask = 0x%08x\n",MdioResetMask);
    printf(" MdioResetBit = 0x%08x\n",MdioResetBit);
  }
  //-------------------------------------------------------------------------
  // Unreset MDIO module (and any other required modules) and enable SM
  //-------------------------------------------------------------------------
#define pRCR_PRCR_REG   ((volatile UINT32 *)(RCR_BASE+0x0000))
#define RCR_PRCR_REG    (*pRCR_PRCR_REG)
  RCR_PRCR_REG |= MdioResetMask;
  udelay(10000);               // MDIO module time to unreset
  {
    int delay = 0xfffff;
    while (delay--);
  }
  MDIO_CONTROL(MdioBase) |= (1<<30);  // Enable MDIO SM
  {
    int delay = 0xfffff;
    while (delay--);
  }
  udelay(500000);              // Give SM time to poll PHYs
  //-------------------------------------------------------------------------
  // For every PHY that ack'd MDIO query, check to see if it is a
  // Marvell 88E1141, 88E1145, or 88E1111 phy, and if so...
  // 1) Clear register 9 (do not advertise gig capabilities). This corrects
  //    using loop-back connector.
  // 2) Clear bit 11 in register 16 (assert CRS on transmit). This prevents
  //    carrier sense errors when operating in half-duplex mode.
  //-------------------------------------------------------------------------
  {
    int delay = 0xfffff;
    while (delay--);
  }
  
  ActivePhys = MDIO_ALIVE(MdioBase);
  if(verbose) printf("   ActivePhys = 0x%08x\n",ActivePhys);
  for(i=0;i<32;i++)
  {
    if( ActivePhys & (1<<i) )
    {
      if( IsMarvellGigPhy(i) )
      {
        if(FirstTime)
        {
          printf("Disabling gigabit capabilities for LAVA Marvell phy(s): %d",i);
          FirstTime=0;
        }
        else
          printf(",%d",i);
        MdioClearBits(MdioBase, 0, i,9,0xffff);              // Set PHY register 9 to 0 (disables gigbit capability)
        MdioSetBits(MdioBase, 0, i,16,(1<<11));  // Set PHY register 16, bit 11 for CRS
      }
    }
  }
  printf("\n");
  return 0;
}

#endif /* CONFIG_PUMA5_VOLCANO_EMU */
#endif /* CONFIG_COMMANDS & CFG_CMD_NET */
