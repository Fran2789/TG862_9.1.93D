/*
 * cpemacphy.h
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

//***************************************************************************
// 	DESCRIPTION:
//		This include file contains definitions for the the MDIO API
//
// 	HISTORY:
//		27Mar02 Michael Hanrahan Original (modified from emacmdio.h)
//		04Apr02 Michael Hanrahan Added Interrupt Support
//***************************************************************************
#ifndef INC_CPMDIO
#define INC_CPMDIO


#ifndef _HAL_MDIO
//typedef void PHY_DEVICE;
typedef struct
{
   UINT32 miibase;
   UINT32 inst;
   UINT32 PhyState;
   UINT32 PhyMask;
   UINT32 MLinkMask;
} PHY_DEVICE;
#define PHY_DEVICE_DEF
#endif


/*Version Information */

void cpMacMdioGetVer(UINT32 miiBase, UINT32 *ModID,  UINT32 *RevMaj,  UINT32 *RevMin);

/*Called once at the begining of time                                        */

int  cpMacMdioInit(PHY_DEVICE *PhyDev, UINT32 miibase, UINT32 inst, UINT32 PhyMask, UINT32 MLinkMask, UINT32 ResetBase, UINT32 ResetBit, UINT32 cpufreq,int verbose);
int  cpMacMdioGetPhyDevSize(void);


/*Called every 10 mili Seconds, returns TRUE if there has been a mode change */

int cpMacMdioTic(PHY_DEVICE *PhyDev);

/*Called to set Phy mode                                                     */

void cpMacMdioSetPhyMode(PHY_DEVICE *PhyDev,UINT32 PhyMode);

/*Calls to retreive info after a mode change!                                */

int  cpMacMdioGetDuplex(PHY_DEVICE *PhyDev);
int  cpMacMdioGetSpeed(PHY_DEVICE *PhyDev);
int  cpMacMdioGetPhyNum(PHY_DEVICE *PhyDev);
int  cpMacMdioGetLinked(PHY_DEVICE *PhyDev);
void cpMacMdioLinkChange(PHY_DEVICE *PhyDev);

/*  Shot Down  */

void cpMacMdioClose(PHY_DEVICE *PhyDev, int Full);


/* Phy Mode Values  */

#define NWAY_FD100          (1<<8)
#define NWAY_HD100          (1<<7)
#define NWAY_FD10           (1<<6)
#define NWAY_HD10           (1<<5)
#define NWAY_AUTO           (1<<0)

#define NWAY_FD1000         (3<<8)

#endif  /*  INC_CPMDIO */
