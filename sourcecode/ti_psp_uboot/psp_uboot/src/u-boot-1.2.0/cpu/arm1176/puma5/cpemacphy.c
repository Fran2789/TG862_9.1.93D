/*
 * cpemacphy.c
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
//  DESCRIPTION:
//   MDIO Polling State Machine API. Functions will enable mii-Phy
//   negotiation.
//
//  HISTORY:
//    01Jan01 Denis, Bill      Original
//      27Mar02 Michael Hanrahan (modified from emacmdio.c)
//      07May02 Michael Hanrahan replaced clockwait for code delay
//***************************************************************************
#include <common.h> 

#if (CONFIG_COMMANDS & CFG_CMD_NET) 

#define UINT32 unsigned int
#define UINT16 unsigned short 
#define REG32( addr )         (*(volatile UINT32 *)(addr))

#define _HAL_MDIO
#include "mdio_reg.h"

#ifndef PHY_DEVICE_DEF
typedef struct
{
   UINT32 miibase;
   UINT32 inst;
   UINT32 PhyState;
   UINT32 PhyMask;
   UINT32 MLinkMask;
} PHY_DEVICE;
#endif

#ifdef CONFIG_PUMA5_VOLCANO_EMU
static void   _mdioDelayEmulate(PHY_DEVICE *PhyDev, int ClockWait);
#endif
static void   _mdioWaitForAccessComplete(PHY_DEVICE *PhyDev);
static void   _mdioUserAccess(PHY_DEVICE *PhyDev, UINT32 method, UINT32 regadr, UINT32 phyadr, UINT32 data);
static UINT32 _mdioUserAccessRead(PHY_DEVICE *PhyDev, UINT32 regadr, UINT32 phyadr);
static void   _mdioUserAccessWrite(PHY_DEVICE *PhyDev, UINT32 regadr, UINT32 phyadr, UINT32 data);

static void _mdioDisablePhy(PHY_DEVICE *PhyDev,UINT32 PhyNum);
static void _mdioPhyTimeOut(PHY_DEVICE *PhyDev);
static void _mdioResetPhy(PHY_DEVICE *PhyDev,UINT32 PhyNum);

#ifdef MDIO_DEBUG
static void _mdioDumpPhy(PHY_DEVICE *PhyDev, UINT32 p);
static void _mdioDumpState(PHY_DEVICE *PhyDev);
#endif



static void _MdioDefaultState  (PHY_DEVICE *PhyDev);
static void _MdioFindingState  (PHY_DEVICE *PhyDev);
static void _MdioFoundState    (PHY_DEVICE *PhyDev);
static void _MdioInitState     (PHY_DEVICE *PhyDev);
static void _MdioLinkedState   (PHY_DEVICE *PhyDev);
static void _MdioLinkWaitState (PHY_DEVICE *PhyDev);
static void _MdioNwayStartState(PHY_DEVICE *PhyDev);
static void _MdioNwayWaitState (PHY_DEVICE *PhyDev);

int cpMacMdioGetSpeed(PHY_DEVICE *PhyDev);

#ifndef TRUE
#define TRUE (1==1)
#endif

#ifndef FALSE
#define FALSE (1==2)
#endif

#define PHY_NOT_FOUND  0xFFFF    /*  Used in Phy Detection */

/*PhyState breakout                                                          */

#define PHY_DEVICE_OFFSET      (0)
#define PHY_DEVICE_SIZE        (5)
#define PHY_DEVICE_MASK        (0x1f<<PHY_DEVICE_OFFSET)

#define PHY_STATE_OFFSET    (PHY_DEVICE_SIZE+PHY_DEVICE_OFFSET)
#define PHY_STATE_SIZE      (5)
#define PHY_STATE_MASK      (0x1f<<PHY_STATE_OFFSET)
  #define INIT       (1<<PHY_STATE_OFFSET)
  #define FINDING    (2<<PHY_STATE_OFFSET)
  #define FOUND      (3<<PHY_STATE_OFFSET)
  #define NWAY_START (4<<PHY_STATE_OFFSET)
  #define NWAY_WAIT  (5<<PHY_STATE_OFFSET)
  #define LINK_WAIT  (6<<PHY_STATE_OFFSET)
  #define LINKED     (7<<PHY_STATE_OFFSET)

#define PHY_SPEED_OFFSET    (PHY_STATE_OFFSET+PHY_STATE_SIZE)
#define PHY_SPEED_SIZE      (2)
#define PHY_SPEED_MASK      (3<<PHY_SPEED_OFFSET)

#define PHY_DUPLEX_OFFSET   (PHY_SPEED_OFFSET+PHY_SPEED_SIZE)
#define PHY_DUPLEX_SIZE     (1)
#define PHY_DUPLEX_MASK     (1<<PHY_DUPLEX_OFFSET)

#define PHY_TIM_OFFSET      (PHY_DUPLEX_OFFSET+PHY_DUPLEX_SIZE)
#define PHY_TIM_SIZE        (10)
#define PHY_TIM_MASK        (0x3ff<<PHY_TIM_OFFSET)
  #define PHY_FIND_TO (  2<<PHY_TIM_OFFSET)
  #define PHY_RECK_TO (200<<PHY_TIM_OFFSET)
  #define PHY_LINK_TO (500<<PHY_TIM_OFFSET)
  #define PHY_NWST_TO (500<<PHY_TIM_OFFSET)
  #define PHY_NWDN_TO (800<<PHY_TIM_OFFSET)

#define PHY_SMODE_OFFSET    (PHY_TIM_OFFSET+PHY_TIM_SIZE)
#define PHY_SMODE_SIZE      (8)
#define PHY_SMODE_MASK      (0xff<<PHY_SMODE_OFFSET)
  #define SMODE_AUTO   (0x80<<PHY_SMODE_OFFSET)
  #define SMODE_FD1000 (0x10<<PHY_SMODE_OFFSET)
  #define SMODE_FD100  (0x08<<PHY_SMODE_OFFSET)
  #define SMODE_HD100  (0x04<<PHY_SMODE_OFFSET)
  #define SMODE_FD10   (0x02<<PHY_SMODE_OFFSET)
  #define SMODE_HD10   (0x01<<PHY_SMODE_OFFSET)
  #define SMODE_ALL    (0x1f<<PHY_SMODE_OFFSET)

#define PHY_CHNG_OFFSET    (PHY_SMODE_OFFSET+PHY_SMODE_SIZE)
#define PHY_CHNG_SIZE      (1)
#define PHY_CHNG_MASK      (1<<PHY_CHNG_OFFSET)
  #define PHY_CHANGE (1<<PHY_CHNG_OFFSET)

/*  Are these two 'globals' allowable??? */

#ifdef MDIO_DEBUG
static char *lstate[]={"NULL","INIT","FINDING","FOUND","NWAY_START","NWAY_WAIT","LINK_WAIT","LINKED"};
#endif

static int cpMacDebug;

/*  Local MDIO Register Macros    */

#define myMDIO_ALIVE           MDIO_ALIVE     (PhyDev->miibase)
#define myMDIO_CONTROL         MDIO_CONTROL   (PhyDev->miibase)
#define myMDIO_LINK            MDIO_LINK      (PhyDev->miibase)
#define myMDIO_LINKINT         MDIO_LINKINT   (PhyDev->miibase)
#define myMDIO_USERACCESS      MDIO_USERACCESS(PhyDev->miibase, PhyDev->inst)
#define myMDIO_USERPHYSEL      MDIO_USERPHYSEL(PhyDev->miibase, PhyDev->inst)
#define myMDIO_VER             MDIO_VER       (PhyDev->miibase)

#ifdef CONFIG_PUMA5_VOLCANO_EMU
//************************************
//*
//* Delays at least ClockWait cylces
//* before returning
//*
//************************************
void _mdioDelayEmulate(PHY_DEVICE *PhyDev, int ClockWait)
  {
  volatile UINT32 i=0;
  while(ClockWait--)
    {
    i |= myMDIO_LINK; /*  MDIO register access to burn cycles */
    }
  }
#endif

void _mdioWaitForAccessComplete(PHY_DEVICE *PhyDev)
  {
  while((myMDIO_USERACCESS & MDIO_USERACCESS_GO)!=0)
    {
    }
  }

void _mdioUserAccess(PHY_DEVICE *PhyDev, UINT32 method, UINT32 regadr, UINT32 phyadr, UINT32 data)
  {
  UINT32  control;

  control =  MDIO_USERACCESS_GO |
             (method) |
             (((regadr) << 21) & MDIO_USERACCESS_REGADR) |
             (((phyadr) << 16) & MDIO_USERACCESS_PHYADR) |
             ((data) & MDIO_USERACCESS_DATA);

  myMDIO_USERACCESS = control;
  }



//************************************
//*
//* Waits for MDIO_USERACCESS to be ready and reads data
//* If 'WaitForData' set, waits for read to complete and returns Data,
//* otherwise returns 0
//* Note: 'data' is 16 bits but we use 32 bits
//*        to be consistent with rest of the code.
//*
//************************************
UINT32 _mdioUserAccessRead(PHY_DEVICE *PhyDev, UINT32 regadr, UINT32 phyadr)
  {

  _mdioWaitForAccessComplete(PhyDev);  /* Wait until UserAccess ready */
  _mdioUserAccess(PhyDev, MDIO_USERACCESS_READ, regadr, phyadr, 0);
  _mdioWaitForAccessComplete(PhyDev);  /* Wait for Read to complete */

  return(myMDIO_USERACCESS & MDIO_USERACCESS_DATA);
  }


//************************************
//*
//* Waits for MDIO_USERACCESS to be ready and writes data
//*
//************************************
void _mdioUserAccessWrite(PHY_DEVICE *PhyDev, UINT32 regadr, UINT32 phyadr, UINT32 data)
  {
  _mdioWaitForAccessComplete(PhyDev);  /* Wait until UserAccess ready */
  _mdioUserAccess(PhyDev, MDIO_USERACCESS_WRITE, regadr, phyadr, data);
  }

#ifdef MDIO_DEBUG
void _mdioDumpPhy(PHY_DEVICE *PhyDev, UINT32 p)
  {
  UINT32 j,n,PhyAcks;
  UINT32 PhyRegAddr;
  UINT32 phy_num;
  UINT32 PhyMask  = PhyDev->PhyMask;

  PhyAcks=myMDIO_ALIVE;
  PhyAcks&=PhyMask;   /* Only interested in 'our' Phys */

  for(phy_num=0,j=1;phy_num<32;phy_num++,j<<=1)
    {
    if (PhyAcks&j)
      {
      printf("%2d%s:",phy_num,(phy_num==p)?">":" ");
      for(PhyRegAddr=0;PhyRegAddr<6;PhyRegAddr++)
        {
        n = _mdioUserAccessRead(PhyDev, PhyRegAddr, phy_num);
        printf(" %04x",n&0x0ffff);
        }
      printf("\n");
      }
    }
  }
#endif

/* 
 * Performs output voltage amplitude adjustments as per ET1011C errata.
 * Note : Expects PHY to be in FOUND state
 */
void _fixup_et1011c_phy (PHY_DEVICE *PhyDev, UINT32 PhyNum)
{
#ifdef MDIO_DEBUG
    if (cpMacDebug)
        printf ("Setting PHY registers as per ET1011C errata...\n");
#endif

    MdioSetBits (PhyDev->miibase, PhyDev->inst, PhyNum, 0, 11);
    MdioSetBits (PhyDev->miibase, PhyDev->inst, PhyNum, 18, (1<<2));
    MdioWr (PhyDev->miibase, PhyDev->inst, PhyNum, 16, 0x8805);
    MdioWr (PhyDev->miibase, PhyDev->inst, PhyNum, 17, 0xF03E);
    MdioWr (PhyDev->miibase, PhyDev->inst, PhyNum, 16, 0x8806);
    MdioWr (PhyDev->miibase, PhyDev->inst, PhyNum, 17, 0x003E);
    MdioWr (PhyDev->miibase, PhyDev->inst, PhyNum, 16, 0x8807);
    MdioWr (PhyDev->miibase, PhyDev->inst, PhyNum, 17, 0x1F00);
    MdioClearBits (PhyDev->miibase, PhyDev->inst, PhyNum, 18, (1<<2));
    MdioClearBits (PhyDev->miibase, PhyDev->inst, PhyNum, 0, 11);
    return;

}

#ifdef MDIO_DEBUG
void _mdioDumpState(PHY_DEVICE *PhyDev)
  {
  UINT32 state    = PhyDev->PhyState;

  if (!cpMacDebug) return;

  printf("Instance %d:\n",PhyDev->inst);
  printf("Phy: %d, ",(state&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET);
  printf("State: %d/%s\n",(state&PHY_STATE_MASK)>>PHY_STATE_OFFSET,lstate[(state&PHY_STATE_MASK)>>PHY_STATE_OFFSET]);
  printf("Speed: %d, ",(state&PHY_SPEED_MASK)>>PHY_SPEED_OFFSET);
  printf("Dup: %d\n",(state&PHY_DUPLEX_MASK)>>PHY_DUPLEX_OFFSET);
  printf("Tim: %d, ",(state&PHY_TIM_MASK)>>PHY_TIM_OFFSET);
  printf("SMode: %d, ",(state&PHY_SMODE_MASK)>>PHY_SMODE_OFFSET);
  printf("Chng: %d",(state&PHY_CHNG_MASK)>>PHY_CHNG_OFFSET);
  printf("\n\n");

  if (((state&PHY_STATE_MASK)!=FINDING)&&((state&PHY_STATE_MASK)!=INIT))
    _mdioDumpPhy(PhyDev, (state&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET);
  }
#endif

void _mdioResetPhy(PHY_DEVICE *PhyDev,UINT32 PhyNum)
  {
  UINT16 PhyControlReg;

  _mdioUserAccessWrite(PhyDev, PHY_CONTROL_REG, PhyNum, PHY_RESET);
#ifdef MDIO_DEBUG
  if (cpMacDebug)
    printf("cpMacMdioPhYReset(%d)\n",PhyNum);
#endif

  /* Read control register until Phy Reset is complete */
  do
   {
    PhyControlReg = _mdioUserAccessRead(PhyDev, PHY_CONTROL_REG, PhyNum);
   }
   while (PhyControlReg & PHY_RESET); /* Wait for Reset to clear */
  
#ifndef CONFIG_PUMA5_VOLCANO_EMU
    /* Output voltage amplitude adjustments as per Errata ET1011C */
    _fixup_et1011c_phy (PhyDev, PhyNum);
#endif
  }

void _mdioDisablePhy(PHY_DEVICE *PhyDev,UINT32 PhyNum)
  {
  _mdioUserAccessWrite(PhyDev, PHY_CONTROL_REG, PhyNum, PHY_ISOLATE|PHY_PDOWN);

#ifdef MDIO_DEBUG
  if (cpMacDebug)
    printf("cpMacMdioDisablePhy(%d)\n",PhyNum);
#endif
  }

void _MdioInitState(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  UINT32 CurrentState;

  CurrentState=*PhyState;
  CurrentState=(CurrentState&~PHY_TIM_MASK)|(PHY_FIND_TO);
  CurrentState=(CurrentState&~PHY_STATE_MASK)|(FINDING);
  CurrentState=(CurrentState&~PHY_SPEED_MASK);
  CurrentState=(CurrentState&~PHY_DUPLEX_MASK);
  CurrentState|=PHY_CHANGE;

  *PhyState=CurrentState;

  }

void _MdioFindingState(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  UINT32  PhyMask  = PhyDev->PhyMask;
  UINT32  PhyNum,i,j,PhyAcks;


  PhyNum=PHY_NOT_FOUND;

  if (*PhyState&PHY_TIM_MASK)
    {
    *PhyState=(*PhyState&~PHY_TIM_MASK)|((*PhyState&PHY_TIM_MASK)-(1<<PHY_TIM_OFFSET));
    }
   else
    {
    PhyAcks=myMDIO_ALIVE;
    PhyAcks&=PhyMask;   /* Only interested in 'our' Phys */

    for(i=0,j=1;(i<32)&&((j&PhyAcks)==0);i++,j<<=1);

    if ((PhyAcks)&&(i<32)) PhyNum=i;
    if (PhyNum!=PHY_NOT_FOUND)
      {
      /*  Phy Found! */
      *PhyState=(*PhyState&~PHY_DEVICE_MASK)|((PhyNum&PHY_DEVICE_MASK)<<PHY_DEVICE_OFFSET);
      *PhyState=(*PhyState&~PHY_STATE_MASK)|(FOUND);
      *PhyState|=PHY_CHANGE;
#ifdef MDIO_DEBUG
      if (cpMacDebug)
        printf("cpMacMdioFindingState: PhyNum: %d\n",PhyNum);
#endif
      }
     else
      {
#ifdef MDIO_DEBUG
      if (cpMacDebug)
        printf("cpMacMdioFindingState: Timed Out looking for a Phy!\n");
#endif
      *PhyState|=PHY_RECK_TO;  /* This state currently has no support?*/
      }
    }
  }

void _MdioFoundState(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState  = &PhyDev->PhyState;
  UINT32  PhyMask   = PhyDev->PhyMask;
  UINT32  MLinkMask = PhyDev->MLinkMask;
  UINT32  PhyNum,PhyStatus,NWAYadvertise,m,phynum,i,j,PhyAcks;
  UINT32  PhySel;

  if ((*PhyState&PHY_SMODE_MASK)==0) return;

  PhyNum=(*PhyState&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET;

  PhyAcks=myMDIO_ALIVE;
  PhyAcks&=PhyMask;   /* Only interested in 'our' Phys */

  /* Will now isolate all our Phys, except the one we have decided to use */
  for(phynum=0,j=1;phynum<32;phynum++,j<<=1)
    {
    if (PhyAcks&j)
      {
        if (phynum!=PhyNum)  /* Do not disabled Found Phy */
          _mdioDisablePhy(PhyDev,phynum);
      }
    }

  /*  Reset the Phy and proceed with auto-negotiation */
  _mdioResetPhy(PhyDev,PhyNum);

  /* Now setup the MDIOUserPhySel register */

  PhySel=PhyNum;  /* Set the phy address */

  /*  Set the way Link will be Monitored */
  /* Check the Link Selection Method */
  if ((1 << PhyNum) & MLinkMask)
    PhySel |= MDIO_USERPHYSEL_LINKSEL;

  myMDIO_USERPHYSEL = PhySel;  /* update PHYSEL */

  /* Get the Phy Status */
  PhyStatus = _mdioUserAccessRead(PhyDev, PHY_STATUS_REG, PhyNum);

#ifdef MDIO_DEBUG
  if (cpMacDebug)
    printf("Enable Phy to negotiate external connection\n");
#endif

  NWAYadvertise=NWAY_SEL;
  if (*PhyState&SMODE_FD100) NWAYadvertise|=NWAY_FD100;
  if (*PhyState&SMODE_HD100) NWAYadvertise|=NWAY_HD100;
  if (*PhyState&SMODE_FD10)  NWAYadvertise|=NWAY_FD10;
  if (*PhyState&SMODE_HD10)  NWAYadvertise|=NWAY_HD10;

  *PhyState&=~(PHY_TIM_MASK|PHY_STATE_MASK);
  if ((PhyStatus&NWAY_CAPABLE)&&(*PhyState&SMODE_AUTO))   /*NWAY Phy Detected*/
    {
    /*For NWAY compliant Phys                                                */

    _mdioUserAccessWrite(PhyDev, NWAY_ADVERTIZE_REG, PhyNum, NWAYadvertise);

    if (*PhyState&SMODE_FD1000) 
        _mdioUserAccessWrite(PhyDev, NWAY_GIG_ADVERTIZE_REG, PhyNum, NWAY_FD1000);
  
#ifdef MDIO_DEBUG
    if (cpMacDebug)
      {
      printf("NWAY Advertising: ");
      if ((NWAYadvertise&NWAY_FD1000) == NWAY_FD1000) printf("FullDuplex-1000");
      if (NWAYadvertise&NWAY_FD100) printf("FullDuplex-100 ");
      if (NWAYadvertise&NWAY_HD100) printf("HalfDuplex-100 ");
      if (NWAYadvertise&NWAY_FD10)  printf("FullDuplex-10 ");
      if (NWAYadvertise&NWAY_HD10)  printf("HalfDuplex-10 ");
      printf("\n");
      }
#endif

    _mdioUserAccessWrite(PhyDev, PHY_CONTROL_REG, PhyNum, AUTO_NEGOTIATE_EN);

    _mdioUserAccessWrite(PhyDev, PHY_CONTROL_REG, PhyNum, AUTO_NEGOTIATE_EN|RENEGOTIATE);

    *PhyState|=PHY_CHANGE|PHY_NWST_TO|NWAY_START;
    }
   else
    {
    *PhyState&=~SMODE_AUTO;   /*The Phy is not capable of auto negotiation!  */
    m=NWAYadvertise;
    for(j=0x8000,i=0;(i<16)&&((j&m)==0);i++,j>>=1);
    m=j;
    j=0;
    if (m&(NWAY_FD100|NWAY_HD100))
      {
      j=PHY_100;
      m&=(NWAY_FD100|NWAY_HD100);
      }
    if (m&(NWAY_FD100|NWAY_FD10))
      j |= PHY_FD;
#ifdef MDIO_DEBUG
    if (cpMacDebug)
      printf("Requested PHY mode %s Duplex %s Mbps\n",(j&PHY_FD)?"Full":"Half",(j&PHY_100)?"100":"10");
#endif
    _mdioUserAccessWrite(PhyDev, PHY_CONTROL_REG, PhyNum, j);
    *PhyState&=~PHY_SPEED_MASK;
    if (j&PHY_100)
      *PhyState|=(1<<PHY_SPEED_OFFSET);
    *PhyState&=~PHY_DUPLEX_MASK;
    if (j&PHY_FD)
      *PhyState|=(1<<PHY_DUPLEX_OFFSET);
    *PhyState|=PHY_CHANGE|PHY_LINK_TO|LINK_WAIT;
    }
  }

void _MdioNwayStartState(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  UINT32 PhyNum,PhyMode;

  PhyNum=(*PhyState&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET;

  /*Wait for Negotiation to start                                            */

  PhyMode=_mdioUserAccessRead(PhyDev, PHY_CONTROL_REG, PhyNum);

  if((PhyMode&RENEGOTIATE)==0)
    {
    _mdioUserAccessRead(PhyDev, PHY_STATUS_REG, PhyNum); /*Flush pending latch bits*/
    *PhyState&=~(PHY_STATE_MASK|PHY_TIM_MASK);
    *PhyState|=PHY_CHANGE|NWAY_WAIT|PHY_NWDN_TO;
    }
   else
    {
    if (*PhyState&PHY_TIM_MASK)
      *PhyState=(*PhyState&~PHY_TIM_MASK)|((*PhyState&PHY_TIM_MASK)-(1<<PHY_TIM_OFFSET));
     else
      _mdioPhyTimeOut(PhyDev);
    }
  }

void _MdioNwayWaitState(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  UINT32  PhyNum,PhyStatus,NWAYadvertise,NWAYREMadvertise,NegMode,i,j;

  PhyNum=(*PhyState&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET;

  PhyStatus=_mdioUserAccessRead(PhyDev, PHY_STATUS_REG, PhyNum);

  if (PhyStatus&NWAY_COMPLETE)
    {
    *PhyState|=PHY_CHANGE;
    *PhyState&=~PHY_SPEED_MASK;
    *PhyState&=~PHY_DUPLEX_MASK;

#ifndef CONFIG_PUMA5_VOLCANO_EMU
    UINT32 rem_gig = _mdioUserAccessRead(PhyDev, 10, PhyNum);
    if (rem_gig & (3<<10))
    {
#ifdef MDIO_DEBUG
        if (cpMacDebug)
            printf ("Remote PHY is GIG capable(cpMacDebug=%d)...\n", cpMacDebug);
#endif
        
        NegMode = NWAY_FD1000;
    }
    else
    {
#endif
            
    NWAYadvertise   =_mdioUserAccessRead(PhyDev, NWAY_ADVERTIZE_REG, PhyNum);
    NWAYREMadvertise=_mdioUserAccessRead(PhyDev, NWAY_REMADVERTISE_REG, PhyNum);

    /* Negotiated mode is we and the remote have in common */
    NegMode = NWAYadvertise & NWAYREMadvertise;

    /* Limit negotiation to fields below */
    NegMode &= (NWAY_FD100|NWAY_HD100|NWAY_FD10|NWAY_HD10);

    if (NegMode==0)
      {
      NegMode=(NWAY_HD100|NWAY_HD10)&NWAYadvertise; /*or 10 ?? who knows, Phy is not MII compliant*/
      }
    for(j=0x8000,i=0;(i<16)&&((j&NegMode)==0);i++,j>>=1);
    NegMode=j;

#ifndef CONFIG_PUMA5_VOLCANO_EMU
    }
#endif
#ifdef MDIO_DEBUG
    if (cpMacDebug)
      {
      printf("Negotiated connection: ");
      if ((NegMode&NWAY_FD1000) == NWAY_FD1000) printf("FullDuplex 1000 Mbs\n");
      else if (NegMode&NWAY_FD100) printf("FullDuplex 100 Mbs\n");
      else if (NegMode&NWAY_HD100) printf("HalfDuplex 100 Mbs\n");
      else if (NegMode&NWAY_FD10) printf("FullDuplex 10 Mbs\n");
      else if (NegMode&NWAY_HD10) printf("HalfDuplex 10 Mbs\n");
      }
#endif
    if (NegMode!=0)
      {
      if (PhyStatus&PHY_LINKED)
        *PhyState=(*PhyState&~PHY_STATE_MASK)|LINKED;
       else
        *PhyState=(*PhyState&~PHY_STATE_MASK)|LINK_WAIT;

      if ((NegMode&NWAY_FD1000) == NWAY_FD1000)
      {
            *PhyState=(*PhyState&~PHY_SPEED_MASK)|(3<<PHY_SPEED_OFFSET);
      }
      else
          if (NegMode&(NWAY_FD100|NWAY_HD100))
          {
            *PhyState=(*PhyState&~PHY_SPEED_MASK)|(1<<PHY_SPEED_OFFSET);
          }
      
      if (NegMode&(NWAY_FD100|NWAY_FD10))
        *PhyState=(*PhyState&~PHY_DUPLEX_MASK)|(1<<PHY_DUPLEX_OFFSET);
      }
    }
   else
    {
    if (*PhyState&PHY_TIM_MASK)
      *PhyState=(*PhyState&~PHY_TIM_MASK)|((*PhyState&PHY_TIM_MASK)-(1<<PHY_TIM_OFFSET));
     else
      _mdioPhyTimeOut(PhyDev);
    }
  }

void _MdioLinkWaitState(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  UINT32  PhyStatus;
  UINT32  PhyNum;

  PhyNum=(*PhyState&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET;

  PhyStatus=_mdioUserAccessRead(PhyDev, PHY_STATUS_REG, PhyNum);

  if (PhyStatus&PHY_LINKED)
    {
    *PhyState=(*PhyState&~PHY_STATE_MASK)|LINKED;
    *PhyState|=PHY_CHANGE;
    }
   else
    {
    if (*PhyState&PHY_TIM_MASK)
      *PhyState=(*PhyState&~PHY_TIM_MASK)|((*PhyState&PHY_TIM_MASK)-(1<<PHY_TIM_OFFSET));
     else
      _mdioPhyTimeOut(PhyDev);
    }
  }

void _mdioPhyTimeOut(PHY_DEVICE *PhyDev)
  {
  /*Things you may want to do if we cannot establish link, like look for another Phy*/
  /* and try it.                                                             */
  }

void _MdioLinkedState(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  UINT32  PhyNum   = (*PhyState&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET;

#ifndef CONFIG_PUMA5_VOLCANO_EMU
  int speed  = cpMacMdioGetSpeed (PhyDev);
    UINT32 rem_gig = _mdioUserAccessRead(PhyDev, 10, PhyNum);

    if (myMDIO_LINK&(1<<PhyNum)) {
        if (((speed == 3) && (rem_gig & (3<<10))) 
                || ((speed != 3) && !(rem_gig & (3<<10))))
        return;  /* if still Linked, exit*/
    }
#else
  if (myMDIO_LINK&(1<<PhyNum)) return;  /* if still Linked, exit*/
#endif

  /* Not Linked */
  *PhyState&=~(PHY_STATE_MASK|PHY_TIM_MASK);
  *PhyState|=PHY_CHANGE|NWAY_WAIT|PHY_NWDN_TO;
  }

void _MdioDefaultState(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  /*Awaiting a cpMacMdioInit call                                             */
  *PhyState|=PHY_CHANGE;
  }


/*User Calls*********************************************************       */

void cpMacMdioClose(PHY_DEVICE *PhyDev, int Full)
  {
  }


int cpMacMdioInit(PHY_DEVICE *PhyDev, UINT32 miibase, UINT32 inst, UINT32 PhyMask, UINT32 MLinkMask, UINT32 ResetReg, UINT32 ResetBit, UINT32 cpufreq,int verbose)
  {
  UINT32 ControlState;
  UINT32 *PhyState = &PhyDev->PhyState;

  cpMacDebug=verbose;

  PhyDev->miibase   = miibase;
  PhyDev->inst      = inst;
  PhyDev->PhyMask   = PhyMask;
  PhyDev->MLinkMask = MLinkMask;

  /*Setup MII MDIO access regs                                              */

  ControlState  = MDIO_CONTROL_ENABLE;
  ControlState |= ((cpufreq/1000000) & MDIO_CONTROL_CLKDIV);

#ifdef CONFIG_PUMA5_VOLCANO_EMU
  /*
      If mii is not out of reset or if the Control Register is not set correctly
      then initalize
  */
  if( !(REG32(ResetReg) & (1 << ResetBit)) || (myMDIO_CONTROL != ControlState) )
    {
       /*  MII not setup, Setup initial condition  */
      REG32(ResetReg) &= ~(1 << ResetBit);  /* put mii in reset  */
      _mdioDelayEmulate(PhyDev, 64);
      REG32(ResetReg) |= (1 << ResetBit);  /* take mii out of reset  */
      _mdioDelayEmulate(PhyDev, 64);
      myMDIO_CONTROL = ControlState;  /* Enable MDIO   */
    }
#else
      myMDIO_CONTROL = ControlState;  /* Enable MDIO   */
#endif /* !CONFIG_PUMA5_VOLCANO_EMU */

  *PhyState=INIT;

#ifdef MDIO_DEBUG
    if (cpMacDebug) {
        printf("cpMacMdioInit\n");
        _mdioDumpState(PhyDev);
    }
#endif    
    return(0);
}

void cpMacMdioSetPhyMode(PHY_DEVICE *PhyDev,UINT32 PhyMode)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  UINT32 CurrentState;

  *PhyState&=~PHY_SMODE_MASK;

  if (PhyMode&NWAY_AUTO)  *PhyState|=SMODE_AUTO;
  if ((PhyMode&NWAY_FD1000) == NWAY_FD1000)*PhyState|=SMODE_FD1000;
  if (PhyMode&NWAY_FD100) *PhyState|=SMODE_FD100;
  if (PhyMode&NWAY_HD100) *PhyState|=SMODE_HD100;
  if (PhyMode&NWAY_FD10)  *PhyState|=SMODE_FD10;
  if (PhyMode&NWAY_HD10)  *PhyState|=SMODE_HD10;

  CurrentState=*PhyState&PHY_STATE_MASK;
  if ((CurrentState==NWAY_START)||
      (CurrentState==NWAY_WAIT) ||
      (CurrentState==LINK_WAIT) ||
      (CurrentState==LINKED)      )
    *PhyState=(*PhyState&~PHY_STATE_MASK)|FOUND|PHY_CHANGE;
#ifdef MDIO_DEBUG
  if (cpMacDebug)
    printf("cpMacMdioSetPhyMode\n");
  _mdioDumpState(PhyDev);
#endif
  }

/* cpMacMdioTic is called every 10 mili seconds to process Phy states         */

int cpMacMdioTic(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  UINT32  CurrentState;

  CurrentState=*PhyState;
  switch(CurrentState&PHY_STATE_MASK)
    {
    case INIT:       _MdioInitState(PhyDev);      break;
    case FINDING:    _MdioFindingState(PhyDev);   break;
    case FOUND:      _MdioFoundState(PhyDev);     break;
    case NWAY_START: _MdioNwayStartState(PhyDev); break;
    case NWAY_WAIT:  _MdioNwayWaitState(PhyDev);  break;
    case LINK_WAIT:  _MdioLinkWaitState(PhyDev);  break;
    case LINKED:     _MdioLinkedState(PhyDev);    break;
    default:         _MdioDefaultState(PhyDev);   break;
    }

#if 0
    if (cpMacDebug) {
      volatile unsigned short reg;
      UINT32 PhyNum=(*PhyState&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET;
      reg = (volatile unsigned short) MdioRd (PhyDev->miibase, PhyDev->inst, PhyNum, 9);
      printf ("1000Base-T Control(%d:9) = %#x\n", PhyNum, reg);
      reg = (volatile unsigned short) MdioRd (PhyDev->miibase, PhyDev->inst, PhyNum, 4);
      printf ("Auto-neg advertisement(4) = %#x\n", reg);
      reg = (volatile unsigned short) MdioRd (PhyDev->miibase, PhyDev->inst, PhyNum, 5);
      printf ("Link partner ability(5) = %#x\n", reg);
      reg = (volatile unsigned short) MdioRd (PhyDev->miibase, PhyDev->inst, PhyNum, 10);
      printf ("1000Base-T Status(10) = %#x\n", reg);
      reg = (volatile unsigned short) MdioRd (PhyDev->miibase, PhyDev->inst, PhyNum, 15);
      printf ("Extended Status(15) = %#x\n", reg);
  }
#endif

  /*Dump state info if a change has been detected                            */
#ifdef MDIO_DEBUG
  if ((CurrentState&~PHY_TIM_MASK)!=(*PhyState&~PHY_TIM_MASK))
    _mdioDumpState(PhyDev);
#endif

  /*Return state change to user                                              */

  if (*PhyState&PHY_CHNG_MASK)
    {
    *PhyState&=~PHY_CHNG_MASK;
    return(TRUE);
    }
   else
    return(FALSE);
  }

/* cpMacMdioGetDuplex is called to retrieve the Duplex info                   */

int cpMacMdioGetDuplex(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  return(*PhyState&PHY_DUPLEX_MASK);
  }

/* cpMacMdioGetSpeed is called to retreive the Speed info                     */

int cpMacMdioGetSpeed(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  return((*PhyState&PHY_SPEED_MASK) >> PHY_SPEED_OFFSET);
  }

/* cpMacMdioGetPhyNum is called to retreive the Phy Device Adr info           */

int cpMacMdioGetPhyNum(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  return((*PhyState&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET);
  }

/* cpMacMdioGetLinked is called to Determine if the LINKED state has been reached*/

int cpMacMdioGetLinked(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;

  return((*PhyState&PHY_STATE_MASK)==LINKED);
  }

void cpMacMdioLinkChange(PHY_DEVICE *PhyDev)
  {
  UINT32 *PhyState = &PhyDev->PhyState;
  UINT32  PhyNum,PhyStatus;

  PhyNum=(*PhyState&PHY_DEVICE_MASK)>>PHY_DEVICE_OFFSET;

  if (cpMacMdioGetLinked(PhyDev))
    {
    PhyStatus=_mdioUserAccessRead(PhyDev, PHY_STATUS_REG, PhyNum);

    if ((PhyStatus&PHY_LINKED)==0)
      {
      *PhyState&=~(PHY_TIM_MASK|PHY_STATE_MASK);
      if (*PhyState&SMODE_AUTO)
        {
        _mdioUserAccessWrite(PhyDev, PHY_CONTROL_REG, PhyNum, AUTO_NEGOTIATE_EN|RENEGOTIATE);
        *PhyState|=PHY_CHANGE|PHY_NWST_TO|NWAY_START;
        }
       else
        {
        *PhyState|=PHY_CHANGE|PHY_LINK_TO|LINK_WAIT;
        }
      }
    }
  }

void cpMacMdioGetVer(UINT32 miibase, UINT32 *ModID,  UINT32 *RevMaj,  UINT32 *RevMin)
  {
  UINT32  Ver;

  Ver = MDIO_VER(miibase);

  *ModID  = (Ver & MDIO_VER_MODID) >> 16;
  *RevMaj = (Ver & MDIO_VER_REVMAJ) >> 8;
  *RevMin = (Ver & MDIO_VER_REVMIN);
  }

int cpMacMdioGetPhyDevSize(void)
  {
  return(sizeof(PHY_DEVICE));
  }

#endif /* CONFIG_COMMANDS & CFG_CMD_NET */

