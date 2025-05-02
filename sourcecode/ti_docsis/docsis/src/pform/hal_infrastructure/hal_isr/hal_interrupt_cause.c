/* 
  GPL LICENSE SUMMARY

  Copyright(c) 2008-2013 Intel Corporation.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.

  Contact Information:
    Intel Corporation
    2200 Mission College Blvd.
    Santa Clara, CA  97052
*/

/*
 * hal_interrupt_cause.c
 * Description:
 * Implementation functions that retrieve the interrupt cause for DOCSIS Interrupts
*/

#define _HAL_INTERRUPT_CAUSE_C_

/*! \file hal_interrupt_handlers_u.c
    \brief Implementation functions that retrieve the interrupt cause for DOCSIS
    \Interrupts.
    \IMPORTANT:
    \   A. This code will run as part of a loadable kernel module, and should
    \      be used in kernel-space only.
    \   B. These functions assume that they are protected externally, meaning
    \      that their code is executed in "atomic" form.
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/module.h>   /* module */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <hardware.h>

#include "docint.h"
#include "hal_interrupt_cause.h"
#include "hal_mac_phy_regs_kernel.h"
#include "status.h"

#if PUMA6_OR_NEWER_SOC_TYPE
#include "hal_mac_interrupt_regs_puma6.h"
#else
#include "hal_mac_interrupt_regs_puma5.h"
#endif


#define  BIT_0    0x00000001
#define  BIT_2    0x00000004

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
static Uint32   gIsrStatsEnabled = 0;
static  Uint32   gIsrPuma6D0 = 0;

#define PUMA5_HW_REV_1_X   0
#define PUMA5_HW_REV_2_0   1

#define FEC_LOCK_VAL                                1
#define FEC_LOST_VAL                                0

typedef struct
{
    Bool    isFecLockMasked; /* Indicates if FecLock interrupt is masked or enabled */
    Bool    isFecLostMasked; /* Indicates if FecLost interrupt is masked or enabled */
    Uint16  isrEnRegAddress; /* Interrupt enable register address (0xc2/0xc4/0x4c2/0x4c4) */
    Uint8   currState;       /* Last known FEC lost/lock status */
    Uint8   prevState;       /* Previous FEC lost/lock status */
    Uint8   fecLockBit;      /* FecLock bit in Interrupt enable register */
    Uint8   fecLostBit;      /* FecLost bit in Interrupt enable register */
    Uint8   fecLockCause;    /* Offset of FecLock interrupt cause in the interrupt group */
    Uint8   fecLostCause;    /* Offset of FecLost interrupt cause in the interrupt group */

} PhyFecLockLostDb_t;

static PhyFecLockLostDb_t   gPhyFecLockLostDb[PHY_INTERNAL_COUNT];

typedef struct
{
    Uint32  countIsr_SERDES_PHY_SYNC_LOCKED     ;
    Uint32  countIsr_SERDES_PHY_SYNC_UNLOCKED   ;
    Uint32  countIsr_SYNC_LOCKED                ;
    Uint32  countIsr_SYNC_UNLOCKED              ;
    Uint32  countIsr_SYNC_WATCHDOG              ;
    Uint32  countIsr_SYMBOL_ERROR               ;
    Uint32  countIsr_FRAME_ERROR                ;
    Uint32  countIsr_PRBS_PATTERN_FOUND         ;
    Uint32  countIsr_PRBS_PATTERN_LOST          ;
    Uint32  countIsr_INPUT_FIFO_OVERFLOW        ;
    Uint32  countIsr_OUTPUT_FIFO_UNDERFLOW      ;
    Uint32  countIsr_INPUT_BUFFER_OVERFLOW      ;
    Uint32  countIsr_INPUT_BUFFER_UNDERFLOW     ;
}
HsifErrCounters_t;


typedef struct
{
    Uint32  countIsr_FEC_LOCK   [ PHY_INTERNAL_COUNT ];
    Uint32  countIsr_FEC_LOSS   [ PHY_INTERNAL_COUNT ];
    Uint32  countIsr_FEC_BOTH   [ PHY_INTERNAL_COUNT ];
    Uint32  countIsr_FEC_GHOST  [ PHY_INTERNAL_COUNT ];

#if PUMA6_OR_NEWER_SOC_TYPE
    HsifErrCounters_t  HSIF_LANE_err[ HSIF_MAX_LANES ];
#endif

    Uint32  countIsr_ghost;
    Uint32  countIsr_root;
}
FecIrqStatsDb_t;

static FecIrqStatsDb_t      gPhyStatsDb;
/**************************************************************************/

#define MAX_US_CHANNELS     8

typedef struct
{
    Uint32  countIsr_NewUcd_Ch[MAX_US_CHANNELS];
    Uint32  countIsr_NewMap_Ch[MAX_US_CHANNELS];
    Uint32  countIsr_ghost;
    Uint32  countIsr_root;
}
UcdIrqStatsDb_t;

static UcdIrqStatsDb_t      gUcdStatsDb;

/**************************************************************************/

typedef struct
{
    Uint32  countIsr_MDD[PHY_RECEIVER_COUNT];
    Uint32  countIsr_KeySeqError;
    Uint32  countIsr_OutOfRangeError;

    Uint32  countIsr_error;
    Uint32  countIsr_ghost;
    Uint32  countIsr_root;
}
DsFwIrqStatsDb_t;

static DsFwIrqStatsDb_t     gDsFwStatsDb;

/**************************************************************************/

typedef struct
{
    Uint32  countIsr_mbxReady;
    Uint32  countIsr_cmdComplete;
    Uint32  countIsr_eventReady;

    Uint32  countIsr_RNG_Sent_Ch[MAX_US_CHANNELS];
    Uint32  countIsr_RNG_Opp_Ch [MAX_US_CHANNELS];

    Uint32  countIsr_error;
    Uint32  countIsr_ghost;
    Uint32  countIsr_root;
}
UsFwIrqStatsDb_t;

static UsFwIrqStatsDb_t     gUsFwStatsDb;

/**************************************************************************/

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static Int32 HAL_IsrMaskPhyInterrupt(Uint32 cause, Bool maskOp);
#if PUMA6_OR_NEWER_SOC_TYPE
static Int32 HAL_IsrMaskHsifInterrupt(Uint32 cause, Bool maskOp);
#endif

static STATUS HAL_GetPhyReg16K(Uint32 regNum, Uint16 *pValue);
static STATUS HAL_SetPhyReg16K(Uint32 regNum, Uint16 value);

/* Macro for retrieving the index number of an interrupt inside database*/
/* of type PhyFecLockLostStatus_t. The macro receives phy-interrupt     */
/* number and returns the local db-index.                               */
#define HAL_PHY_GET_FEC_STATUS_DB_INDX(intrBit)     ((intrBit)/6)


/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/


/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

void HAL_IsrStatsEnableSet(Uint32   enable)
{
    gIsrStatsEnabled = enable;
    return ;
}

void HAL_IsrStatsEnableGet(Uint32 * enable)
{
    if (enable)
    {
        *enable = gIsrStatsEnabled;
    }
    return ;
}


/**********************************/
/*  MAC Errors Interrupt Group    */
/**********************************/

/**************************************************************************/
/*! \fn Int32 HAL_IsrGetCauseMacErrorGroup(IntCauseErrorGroup_t *pCause)
 **************************************************************************
 *  \brief retreives the specific causes that asserted an interrupt on the
 *  \MAC-error group interrupt-line.
 *  \param[out] pCause pointer to the structure that will hold the interrupt cause(s).
 *  \return OK if successful, reject or fail otherwise
 **************************************************************************/
Int32 HAL_IsrGetCauseMacErrorGroup(unsigned int *pCause)
{
    *(volatile Uint32*)(pCause++) = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_1_STATUS_REG);

#if PUMA6_OR_NEWER_SOC_TYPE
    *(volatile Uint32*)(pCause++) = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_1A_STATUS_REG);
    *(volatile Uint32*)(pCause++) = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_1B_STATUS_REG);
    *(volatile Uint32*)(pCause++) = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_1C_STATUS_REG);
#endif

    *(volatile Uint32*)(pCause++) = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_2_STATUS_REG);

#if PUMA6_OR_NEWER_SOC_TYPE
    *(volatile Uint32*)(pCause++) = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_2A_STATUS_REG);
#endif

    *(volatile Uint32*)(pCause++) = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_3_STATUS_REG);

#if PUMA6_OR_NEWER_SOC_TYPE
    *(volatile Uint32*)(pCause++) = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_3A_STATUS_REG);
#endif

    *(volatile Uint32*)(pCause++) = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_4_STATUS_REG);

    return 0;
}


/****************************************/
/*    MAC UCD-Change Interrupt group    */
/****************************************/
int     HAL_IsrUcdChange_ResetStats( char * buf, int count )
{
    memset(&gUcdStatsDb, 0, sizeof(gUcdStatsDb));
    return 0;
}

int     HAL_IsrUcdChange_PrintStats( char * buf, int count )
{
    int i;
    int len = 0;
    int total = 0;

    len += sprintf(buf+len," ch |   NEW UCD  |   NEW MAP  |\n");
    len += sprintf(buf+len,"----+------------+------------+\n");

    for (i = 0; i < MAX_US_CHANNELS; i++)
    {
        len += sprintf(buf+len," %2d | %10d | %10d |\n", i,
                       gUcdStatsDb.countIsr_NewUcd_Ch[i],
                       gUcdStatsDb.countIsr_NewMap_Ch[i]
                       );
        len += sprintf(buf+len,"----+------------+------------+\n");

        total +=       gUcdStatsDb.countIsr_NewUcd_Ch[i] +
                       gUcdStatsDb.countIsr_NewMap_Ch[i];
    }
    len += sprintf(buf+len,"Total statuses: %d Total IRQs : %d Ghost IRQs %d\n", total, gUcdStatsDb.countIsr_root, gUcdStatsDb.countIsr_ghost);

    return len;
}

/**************************************************************************/
/*! \fn Uint32 HAL_IsrUcdChange_GetCause()
 **************************************************************************
 *  \brief retreives the specific causes that asserted an interrupt on the
 *  \MAC UCD Change interrupt-line.
 *  \return The interrupt cause(s) as a bit-map.
 **************************************************************************/
Uint32 HAL_IsrUcdChange_GetCause( void )
{
    int i;
    Uint32 regVal = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS +
                                  MAC_UCD_CHANGE_INTERRUPT_STATUS_REG);

    if (gIsrStatsEnabled)
    {
        gUcdStatsDb.countIsr_root++;

        for (i = 0; i < 16; i++)
        {
            if (regVal & (1 << i))
            {
                switch (i)
                {
                case 0:
                case 1:
                case 2:
                case 3:     gUcdStatsDb.countIsr_NewUcd_Ch[i]++; break;

                case 4:
                case 5:
                case 6:
                case 7:     gUcdStatsDb.countIsr_NewMap_Ch[i-4]++; break;

                case 8:
                case 9:
                case 10:
                case 11:    gUcdStatsDb.countIsr_NewUcd_Ch[i-4]++; break;

                case 12:
                case 13:
                case 14:
                case 15:     gUcdStatsDb.countIsr_NewMap_Ch[i-8]++; break;

                default:    break;
                }
            }
        }

        if (0 == regVal)
        {
            gUcdStatsDb.countIsr_ghost++;
        }
    }

    return regVal;
}

/***********************************/
/*    MAC US FW Interrupt group    */
/***********************************/

int     HAL_IsrUsFw_ResetStats( char * buf, int count )
{
    memset(&gUsFwStatsDb, 0, sizeof(gUsFwStatsDb));
    return 0;
}

int     HAL_IsrUsFw_PrintStats( char * buf, int count )
{
    int i;
    int len = 0;
    int total = 0;

    len += sprintf(buf+len,"  ch |  RNG Sent  |   RNG Opp. |\n");
    len += sprintf(buf+len,"-----+------------+------------+\n");

    for (i = 0; i < MAX_US_CHANNELS; i++)
    {
        len += sprintf(buf+len,"  %2d | %10d | %10d |\n", i, gUsFwStatsDb.countIsr_RNG_Sent_Ch[i], gUsFwStatsDb.countIsr_RNG_Opp_Ch[i]);
        len += sprintf(buf+len,"-----+------------+------------+\n");

        total +=       gUsFwStatsDb.countIsr_RNG_Sent_Ch[i] + gUsFwStatsDb.countIsr_RNG_Opp_Ch[i];
    }

    len += sprintf(buf+len," CMD complete     | %10d |\n", gUsFwStatsDb.countIsr_cmdComplete);
    len += sprintf(buf+len,"------------------+------------+\n");
    total +=    gUsFwStatsDb.countIsr_cmdComplete;

    len += sprintf(buf+len," Event Ready      | %10d |\n", gUsFwStatsDb.countIsr_eventReady);
    len += sprintf(buf+len,"-----+------------+------------+\n");
    total +=    gUsFwStatsDb.countIsr_eventReady;

    len += sprintf(buf+len," MBX Ready        | %10d |\n", gUsFwStatsDb.countIsr_mbxReady);
    len += sprintf(buf+len,"-----+------------+------------+\n");
    total +=    gUsFwStatsDb.countIsr_mbxReady;

    len += sprintf(buf+len,"Total statuses: %d\nTotal     IRQs: %d\nGhost     IRQs: %d\nError     IRQs: %d\n",
                   total,
                   gUsFwStatsDb.countIsr_root,
                   gUsFwStatsDb.countIsr_ghost,
                   gUsFwStatsDb.countIsr_error);

    return len;
}

/**************************************************************************/
/*! \fn Uint32 HAL_IsrUsFw_GetCause()
 **************************************************************************
 *  \brief retreives the specific causes that asserted an interrupt on the
 *  \MAC US FW interrupt-line.
 *  \return The interrupt cause(s) as a bit-map.
 **************************************************************************/
Uint32 HAL_IsrUsFw_GetCause( void )
{
    int     i;
    Uint32  regVal = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS +
                                  MAC_US_FW_INTERRUPT_STATUS_REG);

    if (gIsrStatsEnabled)
    {
        gUsFwStatsDb.countIsr_root++;

        for (i=0; i<32; i++)
        {
            if (regVal & (1 << i))
            {
                if (0==i)
                {
                    gUsFwStatsDb.countIsr_cmdComplete++;
                }
                else if (2==i)
                {
                    gUsFwStatsDb.countIsr_eventReady++;
                }
                else if (31==i)
                {
                    gUsFwStatsDb.countIsr_mbxReady++;
                }
                else
                {
            #if PUMA6_OR_NEWER_SOC_TYPE
                    if ((7<i) && (i<16))
                    {
                        gUsFwStatsDb.countIsr_RNG_Sent_Ch[i-8]++;
                    }
                    else if ((15<i) &&(i<24))
                    {
                        gUsFwStatsDb.countIsr_RNG_Opp_Ch[i-16]++;
                    }
            #else
                    if ((2<i) && (i<7))
                    {
                        gUsFwStatsDb.countIsr_RNG_Sent_Ch[i-3]++;
                    }
                    else if ((6<i) &&(i<11))
                    {
                        gUsFwStatsDb.countIsr_RNG_Opp_Ch[i-7]++;
                    }
            #endif
                    else
                    {
                        gUsFwStatsDb.countIsr_error++;
                    }
                }
            }
        }

        if (0 == regVal)
        {
            gUsFwStatsDb.countIsr_ghost++;
        }
    }

    return regVal;
}

/***********************************/
/*    MAC DS FW Interrupt group    */
/***********************************/

int HAL_IsrDsFw_Init(int isPuma6D0)
{
    gIsrPuma6D0 = isPuma6D0;
    return 0;
}

int     HAL_IsrDsFw_ResetStats( char * buf, int count )
{
    memset(&gDsFwStatsDb, 0, sizeof(gDsFwStatsDb));
    return 0;
}

int     HAL_IsrDsFw_PrintStats( char * buf, int count )
{
    int i;
    int len = 0;
    int total = 0;

    len += sprintf(buf+len,"  ch |     MDD    |\n");
    len += sprintf(buf+len,"-----+------------+\n");

    for (i = 0; i < PHY_RECEIVER_COUNT; i++)
    {
        len += sprintf(buf+len,"  %2d | %10d |\n", i, gDsFwStatsDb.countIsr_MDD[i]);
        len += sprintf(buf+len,"-----+------------+\n");

        total +=       gDsFwStatsDb.countIsr_MDD[i];
    }

    len += sprintf(buf+len," OOR | %10d |\n", gDsFwStatsDb.countIsr_OutOfRangeError);
    len += sprintf(buf+len,"-----+------------+\n");
    total +=    gDsFwStatsDb.countIsr_OutOfRangeError;

    len += sprintf(buf+len," SEQ | %10d |\n", gDsFwStatsDb.countIsr_KeySeqError);
    len += sprintf(buf+len,"-----+------------+\n");
    total +=    gDsFwStatsDb.countIsr_KeySeqError;

    len += sprintf(buf+len,"Total statuses: %d\nTotal     IRQs: %d\nGhost     IRQs: %d\nError     IRQs: %d\n",
                   total,
                   gDsFwStatsDb.countIsr_root,
                   gDsFwStatsDb.countIsr_ghost,
                   gDsFwStatsDb.countIsr_error);

    return len;
}

/**************************************************************************/
/*! \fn Uint32 HAL_IsrDsFw_GetCause()
 **************************************************************************
 *  \brief retreives the specific causes that asserted an interrupt on the
 *  \MAC DS FW interrupt-line.
 *  \return The interrupt cause(s) as a bit-map.
 **************************************************************************/
Uint32 HAL_IsrDsFw_GetCause( unsigned int *pCause )
{
    Uint32 i;
    Uint32 regVal;
    Uint32 isGhost = 0;
    Uint32 num0fBits = 0;

    *pCause = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_DS_FW_INTERRUPT_STATUS_REG);
    num0fBits = 32;

#if PUMA6_OR_NEWER_SOC_TYPE
    if (gIsrPuma6D0)
    {
        /* Read Second gourp interrupt group, only on D0 and more (otherwise register does not exist) */
        *(pCause + 1)  = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_DS_FW_INTERRUPT2_STATUS_REG); 
        num0fBits+=32;
    }
#endif

    if (gIsrStatsEnabled)
    {
        regVal = *pCause;

        gDsFwStatsDb.countIsr_root++;
        if (0 == regVal)
        {
            isGhost++;
        }
        else
        {
        for (i=0; i<32; i++)
        {
            if (regVal & (1 << i))
            {
        #if PUMA6_OR_NEWER_SOC_TYPE
                if (i<24)
                {
                    gDsFwStatsDb.countIsr_MDD[i]++;
                }
                else if (24==i)
                {
                    gDsFwStatsDb.countIsr_KeySeqError++;
                }
                else if (25==i)
                {
                    gDsFwStatsDb.countIsr_OutOfRangeError++;
                }
        #else
                if ((7<i) && (i<16))
                {
                    gDsFwStatsDb.countIsr_MDD[i-8]++;
                }
                else if (16==i)
                {
                    gDsFwStatsDb.countIsr_OutOfRangeError++;
                }
                else if (0==i)
                {
                    gDsFwStatsDb.countIsr_KeySeqError++;
                }
        #endif
                else
                {
                    gDsFwStatsDb.countIsr_error++;
                }
            }
        }
        }

        if (gIsrPuma6D0)
        {
            regVal  = *(pCause+1);
            if (0 == regVal)
            {
                if (isGhost != 0)
                {
                    gDsFwStatsDb.countIsr_ghost++;
                }
            }
            else
            {
                for (i=0;i<32;i++)
                {
                    if ((regVal&0x00000001) != 0)
                    {
                         gDsFwStatsDb.countIsr_MDD[i]++;
                    }
                    regVal>>=1;
                }
            }
        }
        else
        {
            if (isGhost != 0)
            {
                gDsFwStatsDb.countIsr_ghost++;
            }
        }
    }

    return num0fBits;
}

#if PUMA6_OR_NEWER_SOC_TYPE
int  HAL_IsrHsif_PrintStats( char * buf, int count )
{
    int i;
    int len = 0;
    int total = 0;
    int total_phy = 0;

    for (i = 0; i < PHY_INTERNAL_COUNT; i++)
    {
        total_phy +=   gPhyStatsDb.countIsr_FEC_LOCK[i] +
                       gPhyStatsDb.countIsr_FEC_LOSS[i] +
                       gPhyStatsDb.countIsr_FEC_BOTH[i] +
                       gPhyStatsDb.countIsr_FEC_GHOST[i] ;
    }

    len += sprintf(buf+len,"----+-------------------------+--------------------------------------+\n");
    len += sprintf(buf+len,"    |     SERDES PHY SYNC     |                 SYNC                 |\n");
    len += sprintf(buf+len," ch |    LOCK    |   UNLOCK   |    LOCK    |   UNLOCK   |  WATCHDOG  |\n");
    len += sprintf(buf+len,"----+------------+------------+------------+------------+------------+\n");
    for (i = 0; i < HSIF_MAX_LANES; i++)
    {
        len += sprintf(buf+len," %2d | %10d | %10d | %10d | %10d | %10d |\n", i,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_SERDES_PHY_SYNC_LOCKED  ,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_SERDES_PHY_SYNC_UNLOCKED,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYNC_LOCKED             ,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYNC_UNLOCKED           ,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYNC_WATCHDOG           );

        total +=
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SERDES_PHY_SYNC_LOCKED   +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SERDES_PHY_SYNC_UNLOCKED +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYNC_LOCKED              +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYNC_UNLOCKED            +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYNC_WATCHDOG           ;
    }
    len += sprintf(buf+len,"----+------------+------------+------------+------------+------------+\n");

    len += sprintf(buf+len,"    |         RX FIFO         |         TX BUFFER       |\n");
    len += sprintf(buf+len," ch |  OVERFLOW  |  UNDERFLOW |  OVERFLOW  |  UNDERFLOW |\n");
    len += sprintf(buf+len,"----+------------+------------+------------+------------+\n");
    for (i = 0; i < HSIF_MAX_LANES; i++)
    {
        len += sprintf(buf+len," %2d | %10d | %10d | %10d | %10d |\n", i,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_INPUT_FIFO_OVERFLOW   ,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_OUTPUT_FIFO_UNDERFLOW ,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_INPUT_BUFFER_OVERFLOW ,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_INPUT_BUFFER_UNDERFLOW);

        total +=
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_INPUT_FIFO_OVERFLOW   +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_OUTPUT_FIFO_UNDERFLOW +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_INPUT_BUFFER_OVERFLOW +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_INPUT_BUFFER_UNDERFLOW;
    }
    len += sprintf(buf+len,"----+------------+------------+------------+------------+\n");

    len += sprintf(buf+len,"    |       PRBS PATTERN      |          ERROR          |\n");
    len += sprintf(buf+len," ch |    LOCK    |   UNLOCK   |   SYMBOL   |    FRAME   |\n");
    len += sprintf(buf+len,"----+------------+------------+------------+------------+\n");
    for (i = 0; i < HSIF_MAX_LANES; i++)
    {
        len += sprintf(buf+len," %2d | %10d | %10d | %10d | %10d |\n", i,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_PRBS_PATTERN_FOUND,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_PRBS_PATTERN_LOST ,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYMBOL_ERROR      ,
        gPhyStatsDb.HSIF_LANE_err[i].countIsr_FRAME_ERROR);

        total +=
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_PRBS_PATTERN_FOUND +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_PRBS_PATTERN_LOST  +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYMBOL_ERROR       +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_FRAME_ERROR;


    }
    len += sprintf(buf+len,"----+------------+------------+------------+------------+\n");

    len += sprintf(buf+len,"PHY  Interrupts: %d\n",total_phy);
    len += sprintf(buf+len,"HSIF Interrupts: %d\n",total);
    len += sprintf(buf+len,"Total IRQs : %d\nGhost IRQs : %d\n", gPhyStatsDb.countIsr_root, gPhyStatsDb.countIsr_ghost);

    return len;
}
#endif


/***********************************/
/*         PHY  Interrupts         */
/***********************************/

int  HAL_IsrPhyFec_PrintStats( char * buf, int count )
{
    int i;
    int len = 0;
    int total = 0;

    len += sprintf(buf+len," ch |  FEC LOCK  |  FEC LOST  |    BOTH    |    GHOST   |\n");
    len += sprintf(buf+len,"----+------------+------------+------------+------------+\n");

    for (i = 0; i < PHY_INTERNAL_COUNT; i++)
    {
        len += sprintf(buf+len," %2d | %10d | %10d | %10d | %10d |\n", i,
                       gPhyStatsDb.countIsr_FEC_LOCK[i],
                       gPhyStatsDb.countIsr_FEC_LOSS[i],
                       gPhyStatsDb.countIsr_FEC_BOTH[i],
                       gPhyStatsDb.countIsr_FEC_GHOST[i]
                       );
        len += sprintf(buf+len,"----+------------+------------+------------+------------+\n");

        total +=       gPhyStatsDb.countIsr_FEC_LOCK[i] +
                       gPhyStatsDb.countIsr_FEC_LOSS[i] +
                       gPhyStatsDb.countIsr_FEC_BOTH[i] +
                       gPhyStatsDb.countIsr_FEC_GHOST[i] ;
    }

    len += sprintf(buf+len,"PHY  Interrupts: %d\n",total);

#if PUMA6_OR_NEWER_SOC_TYPE
    total = 0;

    for (i = 0; i < HSIF_MAX_LANES; i++)
    {
        total +=
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_INPUT_FIFO_OVERFLOW       +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_OUTPUT_FIFO_UNDERFLOW     +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_INPUT_BUFFER_OVERFLOW     +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_INPUT_BUFFER_UNDERFLOW    +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_PRBS_PATTERN_FOUND        +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_PRBS_PATTERN_LOST         +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYMBOL_ERROR              +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_FRAME_ERROR               +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SERDES_PHY_SYNC_LOCKED    +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SERDES_PHY_SYNC_UNLOCKED  +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYNC_LOCKED               +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYNC_UNLOCKED             +
            gPhyStatsDb.HSIF_LANE_err[i].countIsr_SYNC_WATCHDOG;
    }

    len += sprintf(buf+len,"HSIF Interrupts: %d\n",total);
#endif

    len += sprintf(buf+len,"Total IRQs : %d\nGhost IRQs : %d\n", gPhyStatsDb.countIsr_root, gPhyStatsDb.countIsr_ghost);

    return len;
}

int  HAL_IsrPhyFec_PrintConfig( char * buf, int count )
{
    int i;
    int len = 0;

    len += sprintf(buf+len," ch | EnADDR | StADDR | LOCK | LOST | lock | lost |\n");
    len += sprintf(buf+len,"----+--------+--------+------+------+------+------+\n");

    for (i = 0; i < PHY_INTERNAL_COUNT; i++)
    {
        len += sprintf(buf+len," %2d | 0x%04X | 0x%04X | %4d | %4d | %4d | %4d |\n", i,
                       gPhyFecLockLostDb[i].isrEnRegAddress,
                       PHY_RX_INTERRUPT_STATUS_REG(i),
                       gPhyFecLockLostDb[i].fecLockBit,
                       gPhyFecLockLostDb[i].fecLostBit,
                       gPhyFecLockLostDb[i].fecLockCause,
                       gPhyFecLockLostDb[i].fecLostCause
                       );
        len += sprintf(buf+len,"----+--------+--------+------+------+------+------+\n");
    }

    return len;
}

int  HAL_IsrPhyFec_ResetStats( char * buf, int count )
{
    memset (&gPhyStatsDb, 0, sizeof(gPhyStatsDb));
    return 0;
}

/**************************************************************************/
/*! \fn Int32 HAL_IsrResetPhyFecDB()
 **************************************************************************
 *  \brief Reset Phy-Rx database.
 *  \param[in] none.
 *  \return OK if successful, reject or fail otherwise.
 **************************************************************************/
Int32 HAL_IsrResetPhyFecDB()
{
    Uint32 i;

    memset(gPhyFecLockLostDb, 0, sizeof(gPhyFecLockLostDb));

    for (i = 0; i < PHY_INTERNAL_COUNT; i++)
    {
        gPhyFecLockLostDb[i].isFecLockMasked = True;                            /* Indicates if FecLock interrupt is masked or enabled */
        gPhyFecLockLostDb[i].isFecLostMasked = True;                            /* Indicates if FecLost interrupt is masked or enabled */
        gPhyFecLockLostDb[i].currState = 0;                                     /* Last known FEC lost/lock status */
        gPhyFecLockLostDb[i].prevState = 0;                                     /* Previous FEC lost/lock status */
        gPhyFecLockLostDb[i].fecLockCause = PHY_INT_FEC_LOCK_RX(i);             /* Offset of FecLock interrupt cause in the interrupt group */
        gPhyFecLockLostDb[i].fecLostCause = PHY_INT_FEC_LOST_RX(i);             /* Offset of FecLost interrupt cause in the interrupt group */
        gPhyFecLockLostDb[i].isrEnRegAddress = PHY_RX_INTERRUPT_ENABLE_REG(i);  /* Interrupt enable register address (0xc2/0xc4/0x4c2/0x4c4) */

#if PUMA6_OR_NEWER_SOC_TYPE
        {
            gPhyFecLockLostDb[i].fecLockBit = PHY_FEC_LOCKED_RX(i);             /* FecLock bit in Interrupt enable register */
            gPhyFecLockLostDb[i].fecLostBit = PHY_FEC_LOST_RX(i);               /* FecLost bit in Interrupt enable register */
        }
#else
        if (system_rev == PUMA5_HW_REV_2_0)
        {
            gPhyFecLockLostDb[i].fecLockBit = PHY_HW_REV_2_0_FEC_LOCKED_RX(i);  /* FecLock bit in Interrupt enable register */
            gPhyFecLockLostDb[i].fecLostBit = PHY_HW_REV_2_0_FEC_LOST_RX(i);    /* FecLost bit in Interrupt enable register */
        }
        else
        {
            gPhyFecLockLostDb[i].fecLockBit = PHY_HW_REV_1_X_FEC_LOCKED_RX(i);  /* FecLock bit in Interrupt enable register */
            gPhyFecLockLostDb[i].fecLostBit = PHY_HW_REV_1_X_FEC_LOST_RX(i);    /* FecLost bit in Interrupt enable register */
        }
#endif
    }

    return 0;
}

/**************************************************************************/
/*! \fn Uint32 HAL_IsrGetFecStatusBitVal(Uint32 ch)
 **************************************************************************
 *  \brief retrieves current FEC status of the channel (FEC_LOCK_VAL/FEC_LOST_VAL)
 *  \param[in] Uint32 ch - channel to check for FEC status
 *  \return The ch FEC status.
 **************************************************************************/
Uint32 HAL_IsrGetFecStatusBitVal(Uint32 ch)
{
    Uint16 valFec = 0;
    Uint16 annexVal = 0;
    Uint32 bitVal = 0;

    /*check FEC status register for FEC lost/lock status */
    HAL_GetPhyReg16K( PHY_RX_FEC_STATUS(ch), &valFec );

    bitVal = (valFec >> PHY_RX_FEC_STATUS_TR_SYNC) & 1;

    /* Euro patch: FEC A Status Register is inverted to FEC B Status Register */
    HAL_GetPhyReg16K( PHY_RX_CH_REG_OFFSET(ch, PHY_QAM_MODE_0_REGISTER), &annexVal);
    annexVal &= (1 << PHY_QAM_ANNEX_TYPE);
    if(annexVal) /*0 - Annex B, 1 - Annex A*/
    {
        bitVal = 1 - bitVal;
    }

    return bitVal;
}

/**************************************************************************/
/*! \fn Uint32 HAL_IsrPhyFec_GetCause(unsigned int *pCause)
 **************************************************************************
 *  \brief retrieves the specific causes that asserted an interrupt on the
 *  \PHY interrupt-line.
 *  \return The interrupt cause(s) as a bit-map.
 **************************************************************************/
Uint32 HAL_IsrPhyFec_GetCause( Uint32 *pCause )
{
    Uint32 bitVal;
    Uint32 i;
    Uint32 * groupOutput = NULL;
    Uint32  anyPhyCauseDetected = 0;

#ifdef DEBUG_CAUSE_PRINTS
    printk( "--> %s\n", __FUNCTION__);
#endif

    gPhyStatsDb.countIsr_root++;

    for(i = 0; i < PHY_INTERNAL_COUNT; i++)
    {
        Uint16 LOSS;
        Uint16 LOCK;

#if PUMA5_SOC_TYPE
        /* Clear all interrupts */
        if (system_rev == PUMA5_HW_REV_2_0)
#endif
        {
            Uint16 irqStatus = 0;

            HAL_GetPhyReg16K( PHY_RX_INTERRUPT_STATUS_REG(i), &irqStatus );

            LOSS = (1 << gPhyFecLockLostDb[i].fecLostBit) & irqStatus;
            LOCK = (1 << gPhyFecLockLostDb[i].fecLockBit) & irqStatus;

#ifdef DEBUG_CAUSE_PRINTS
            printk("    : idx%02d : irqStatus[0x%04X] = 0x%04X ...", i, PHY_RX_INTERRUPT_STATUS_REG(i), irqStatus );
#endif

            if (LOSS | LOCK)
            {
                HAL_SetPhyReg16K( PHY_RX_INTERRUPT_STATUS_REG(i),  LOSS | LOCK ); /* Clear appropriate Int ! */
                HAL_GetPhyReg16K( PHY_RX_INTERRUPT_STATUS_REG(i), &irqStatus );

                anyPhyCauseDetected = 1;

#ifdef DEBUG_CAUSE_PRINTS
                printk(" 0x%04X mask 0x%04X\n", irqStatus, LOSS | LOCK );
#endif
                if (gIsrStatsEnabled)
                {
                    if (LOSS && LOCK)
                    {
                        gPhyStatsDb.countIsr_FEC_BOTH[i]++;
                    }
                    else
                    {
                        if (LOSS)
                        {
                            gPhyStatsDb.countIsr_FEC_LOSS[i]++;
                        }
                        if (LOCK)
                        {
                            gPhyStatsDb.countIsr_FEC_LOCK[i]++;
                        }
                    }
                }
            }
            else
            {
#ifdef DEBUG_CAUSE_PRINTS
                printk("\n");
#endif
            }
        }

        if (0 == i%4)
        {
             groupOutput = &pCause[i/4];
            *groupOutput = 0;
        }

        {
            bitVal = HAL_IsrGetFecStatusBitVal(i);

    #ifdef DEBUG_CAUSE_PRINTS
            printk( "-->%s: port=%d: bitVal = %d, currState  = %d \n",__FUNCTION__, i+1, bitVal, gPhyFecLockLostDb[i].currState);
    #endif

            if (bitVal != gPhyFecLockLostDb[i].currState)
            {
                /* updating database */
                gPhyFecLockLostDb[i].prevState = gPhyFecLockLostDb[i].currState;
                gPhyFecLockLostDb[i].currState = bitVal;

                if (FEC_LOST_VAL == bitVal)
                {
    #ifdef DEBUG_CAUSE_PRINTS
                    printk( "-->%s: bitVal = FEC_LOST_VAL\n", __FUNCTION__);
    #endif

                    if(!(gPhyFecLockLostDb[i].isFecLostMasked))
                    {
                        *groupOutput |= (1 << gPhyFecLockLostDb[i].fecLostCause);
                    }
                }
                else
                {
    #ifdef DEBUG_CAUSE_PRINTS
                    printk( "-->%s: bitVal = FEC_LOCK_VAL\n",__FUNCTION__);
    #endif

                    if(!(gPhyFecLockLostDb[i].isFecLockMasked))
                    {
                        *groupOutput |= (1 << gPhyFecLockLostDb[i].fecLockCause);
                    }
                }
            }
            else
            {
                if (gIsrStatsEnabled)
                {
                    if (LOSS | LOCK) /* There was no actual change in a status while channel raised interrupt */
                    {
                        gPhyStatsDb.countIsr_FEC_GHOST[i]++;
                    }
                }
            }
        }
    }

#if PUMA6_OR_NEWER_SOC_TYPE
    {
        Uint16 masterHsifStatus = 0;
        Uint32 report0status = 0;
        Uint32 report1status = 0;

        HAL_GetPhyReg16K( PHY_GENERAL_INTERRUPT_STATUS, &masterHsifStatus );

        
        if (masterHsifStatus & BIT_2)
        {
            Uint16 lane0status = 0;
            Uint16 lane1status = 0;
            Uint16 lane0mask = 0;
            Uint16 lane1mask = 0;

            HAL_GetPhyReg16K( PHY_HSIF_INT_STATUS1_REG, &lane0status );
            HAL_GetPhyReg16K( PHY_HSIF_INT_STATUS2_REG, &lane1status );
            HAL_GetPhyReg16K( PHY_HSIF_RX_INT_MASK1_REG, &lane0mask );
            HAL_GetPhyReg16K( PHY_HSIF_RX_INT_MASK2_REG, &lane1mask );

            lane0status &= ~lane0mask;
            lane1status &= ~lane1mask;


            if ( lane0status )
            {
                /* decide which error to report based on hierarhy */
                if      (lane0status & PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED )
                {        report0status = PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED;  }
                else if (lane0status & PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED)
                {        report0status = PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED;             }
                else if (lane0status & PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW)
                {        report0status = PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW;       }
                else if (lane0status & PHY_HSIF_INT_RX_LANE_FRAME_ERROR)
                {        report0status = PHY_HSIF_INT_RX_LANE_FRAME_ERROR;               }
                else if (lane0status & PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR)
                {        report0status = PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR;              }
                else if (lane0status & PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW )
                {        report0status = PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW; }
                
                /* mask ALL RX interupts till it will be handled in user space */
                if (report0status)
                {
                   HAL_SetPhyReg16K( PHY_HSIF_RX_INT_MASK1_REG, (lane0mask | PHY_HSIF_INT_RX_LANE_ALL_INT) );
                }

                report0status |= lane0status & (PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_OVERFLOW | PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_UNDERFLOW); 

                HAL_SetPhyReg16K( PHY_HSIF_INT_STATUS1_REG, lane0status );

            }

            if (lane1status )
            {
		        /* decide which error to report based on hierarhy */
                if      (lane1status & PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED )
                {        report1status = PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED;  }
                else if (lane1status & PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED)
                {        report1status = PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED;             }
                else if (lane1status & PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW)
                {        report1status = PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW;       }
                else if (lane1status & PHY_HSIF_INT_RX_LANE_FRAME_ERROR)
                {        report1status = PHY_HSIF_INT_RX_LANE_FRAME_ERROR;               }
                else if (lane1status & PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR)
                {        report1status = PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR;              }
                else if (lane1status & PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW )
                {       report1status = PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW;  }

                /* mask ALL RX interupts till it will be handled in user space */
                if (report1status)
                {
                   HAL_SetPhyReg16K( PHY_HSIF_RX_INT_MASK2_REG, (lane1mask | PHY_HSIF_INT_RX_LANE_ALL_INT) );
                }

                report1status |= lane1status & (PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_OVERFLOW | PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_UNDERFLOW); 

                HAL_SetPhyReg16K( PHY_HSIF_INT_STATUS2_REG, lane1status );
            }



            *(++groupOutput) = ((report1status) << 16) | report0status;

            if (gIsrStatsEnabled)
            {
                int i;
                if (report0status)
                {
                    for (i=0; i<16; i++)
                    {
                        switch (report0status & (1 << i))
                        {
                            case PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_LOCKED  :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_SERDES_PHY_SYNC_LOCKED   ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED:    gPhyStatsDb.HSIF_LANE_err[0].countIsr_SERDES_PHY_SYNC_UNLOCKED ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SYNC_LOCKED             :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_SYNC_LOCKED              ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED           :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_SYNC_UNLOCKED            ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SYNC_WATCHDOG           :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_SYNC_WATCHDOG            ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR            :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_SYMBOL_ERROR             ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_FRAME_ERROR             :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_FRAME_ERROR              ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_FOUND      :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_PRBS_PATTERN_FOUND       ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_LOST       :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_PRBS_PATTERN_LOST        ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW     :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_INPUT_FIFO_OVERFLOW      ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW   :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_OUTPUT_FIFO_UNDERFLOW    ++ ; break;
                            case PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_OVERFLOW   :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_INPUT_BUFFER_OVERFLOW    ++ ; break;
                            case PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_UNDERFLOW  :    gPhyStatsDb.HSIF_LANE_err[0].countIsr_INPUT_BUFFER_UNDERFLOW   ++ ; break;

                        default:
                            break;
                        }
                    }
                }
                if (report1status)
                {
                    for (i=0; i<16; i++)
                    {
                        switch (report1status & (1 << i))
                        {
                            case PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_LOCKED  :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_SERDES_PHY_SYNC_LOCKED   ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED:    gPhyStatsDb.HSIF_LANE_err[1].countIsr_SERDES_PHY_SYNC_UNLOCKED ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SYNC_LOCKED             :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_SYNC_LOCKED              ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED           :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_SYNC_UNLOCKED            ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SYNC_WATCHDOG           :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_SYNC_WATCHDOG            ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR            :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_SYMBOL_ERROR             ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_FRAME_ERROR             :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_FRAME_ERROR              ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_FOUND      :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_PRBS_PATTERN_FOUND       ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_LOST       :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_PRBS_PATTERN_LOST        ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW     :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_INPUT_FIFO_OVERFLOW      ++ ; break;
                            case PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW   :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_OUTPUT_FIFO_UNDERFLOW    ++ ; break;
                            case PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_OVERFLOW   :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_INPUT_BUFFER_OVERFLOW    ++ ; break;
                            case PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_UNDERFLOW  :    gPhyStatsDb.HSIF_LANE_err[1].countIsr_INPUT_BUFFER_UNDERFLOW   ++ ; break;

                        default:
                            break;
                        }
                    }
                }
            }

            HAL_SetPhyReg16K( PHY_GENERAL_INTERRUPT_STATUS, BIT_2 );
        }

        if (gIsrStatsEnabled && (0 == (anyPhyCauseDetected | report0status | report1status)))
        {
            gPhyStatsDb.countIsr_ghost++;
        }
    }
#else
    if (gIsrStatsEnabled && (0 == anyPhyCauseDetected))
    {
        gPhyStatsDb.countIsr_ghost++;
    }
#endif

#ifdef DEBUG_CAUSE_PRINTS
    printk( "-->%s: pCause = 0x%x \n\n",__FUNCTION__, *pCause);
#endif

    return 0;
}


/**************************************************************************/
/*! \fn Int32 HAL_IsrMaskInterrupt(Uint32 cause, Bool maskOp)
 **************************************************************************
 *  \brief Masks or unmasks a speific interrupt.
 *  \param[in] cause defines the specific interrupt that needs to be
 *  \                masked/unmasked.
 *  \param[in] maskOp defines whether to mask or unmask.
 *  \                 True = mask, False= unmask.
 *  \return OK if successful, reject or fail otherwise
 **************************************************************************/
Int32 HAL_IsrMaskInterrupt(Uint32 cause, Bool maskOp)
{
    Uint32 address = MAC_KERNEL_BASE_ADDRESS;
    Uint32 group = HAL_GET_INTERRUPT_GROUP(cause);
    Uint8  bit = HAL_GET_INTERRUPT_BIT(cause);

    switch (group)
    {
    case MAC_ERR1_INT_GROUP:
        if (bit > MAC_ERR1_INT_FLTR_UCD_US4)
            return -1;
        address += MAC_ERROR_INTERRUPT_1_MASK_REG;
        break;

#if PUMA6_OR_NEWER_SOC_TYPE
    case MAC_ERR1A_INT_GROUP: address += MAC_ERROR_INTERRUPT_1A_MASK_REG; break;
    case MAC_ERR1B_INT_GROUP: address += MAC_ERROR_INTERRUPT_1B_MASK_REG; break;
    case MAC_ERR1C_INT_GROUP: address += MAC_ERROR_INTERRUPT_1C_MASK_REG; break;

    case MAC_ERR2A_INT_GROUP: address += MAC_ERROR_INTERRUPT_2A_MASK_REG; break;

    case MAC_ERR3A_INT_GROUP: address += MAC_ERROR_INTERRUPT_3A_MASK_REG; break;
#endif

    case MAC_ERR2_INT_GROUP:
        if (False == maskOp)
        {
            /* When unmasking this interrupt need to read the status register in order to clear the ISR - in order not getting dummy ISR */
            Uint32 __attribute__ ((unused)) statusClear = *(volatile Uint32*)(MAC_KERNEL_BASE_ADDRESS + MAC_ERROR_INTERRUPT_2_STATUS_REG);
        }
        address += MAC_ERROR_INTERRUPT_2_MASK_REG;
        break;

    case MAC_ERR3_INT_GROUP:
        if (bit > MAC_ERR3_INT_MP_ORD_FIFO_FULL)
            return -1;
        address += MAC_ERROR_INTERRUPT_3_MASK_REG;
        break;

    case MAC_ERR4_INT_GROUP:
        address += MAC_ERROR_INTERRUPT_4_MASK_REG;
        break;

    case MAC_UCD_INT_GROUP:
#if PUMA6_OR_NEWER_SOC_TYPE
        if (bit > MAC_UCD_INT_NEW_UCD_US8)
#else
        if (bit > MAC_UCD_INT_NEW_UCD_US4)
#endif
        {
            return -1;
        }
        address += MAC_UCD_CHANGE_INTERRUPT_MASK_REG;
        break;

    case MAC_US_FW_INT_GROUP:
        address += MAC_US_FW_INTERRUPT_MASK_REG;
        break;

    case MAC_DS_FW_INT_GROUP:
        address += MAC_DS_FW_INTERRUPT_MASK_REG;
        break;
#if PUMA6_OR_NEWER_SOC_TYPE
    case MAC_DS_FW_INT_GROUP2:
        address += MAC_DS_FW_INTERRUPT2_MASK_REG; 
        break;
#endif

    case PHY_INT_GROUP:
#if PUMA6_OR_NEWER_SOC_TYPE
    case PHY_INT_GROUP1:
    case PHY_INT_GROUP2:
    case PHY_INT_GROUP3:
    case PHY_INT_GROUP4:
    case PHY_INT_GROUP5:
#endif
        return HAL_IsrMaskPhyInterrupt(cause, maskOp);
        break;

#if PUMA6_OR_NEWER_SOC_TYPE
    case PHY_INT_HSIF_GROUP:
        return HAL_IsrMaskHsifInterrupt(cause, maskOp);
        break;
#endif

    case PHY_PGA_GRT_INT_GROUP:
        if(maskOp == True)
        {
            printk(KERN_INFO "HAL_IsrMaskInterrupt: calling disable_irq(PHY_PGA_GRT_IRQ_NUM)\n");
            //disable_irq(PHY_PGA_GRT_IRQ_NUM);
            /* Use noSync since we are already in an interrupt context, and disable_irq would case a deadlock */
            disable_irq_nosync(PHY_PGA_GRT_IRQ_NUM);
        }
        else
        {
            printk(KERN_INFO "HAL_IsrMaskInterrupt: calling enable_irq(PHY_PGA_GRT_IRQ_NUM)\n");
#if PUMA6_OR_NEWER_SOC_TYPE
            if (siliconStepping < SILICON_STEPPING_ID_C_0)
            {
            ack_irq(PHY_PGA_GRT_IRQ_NUM);
            }
#endif
            enable_irq(PHY_PGA_GRT_IRQ_NUM);
        }
        return 0;
        break;


    case PHY_MPT_INT_GROUP:

        if ((system_rev == PUMA5_HW_REV_2_0)  || (gIsrPuma6D0==1))
        {
            if(maskOp == True)
            {
                printk(KERN_INFO "HAL_IsrMaskInterrupt: calling disable_irq_nosync(PHY_MPT_INT_NUM)\n");
                //disable_irq(PHY_MPT_INT_NUM);
                /* Use noSync since we are already in an interrupt context, and disable_irq would case a deadlock */
                disable_irq_nosync(PHY_MPT_INT_NUM);
            }
            else
            {
                printk(KERN_INFO "HAL_IsrMaskInterrupt: calling enable_irq(PHY_MPT_INT_NUM)\n");
                enable_irq(PHY_MPT_INT_NUM);
            }
        }
        return 0;

    default:
        return -1;
    }

    if(maskOp)
        /* mask the interrupt*/
        *((volatile Uint32 *)address) |= (1<<bit);
    else
        /* unmask the interrupt*/
        *((volatile Uint32 *)address) &= ~(1<<bit);
    return 0;
}


/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

#if PUMA6_OR_NEWER_SOC_TYPE
/**************************************************************************/
/*! \fn static Int32 HAL_IsrMaskHsifInterrupt(Uint32 cause, Bool maskOp)
 **************************************************************************
 *  \brief Perform mask or unmask operation.
 *  \param[in] Uint32 cause - interrupts bitmask.
 *  \param[in] Bool maskOp - Mask operation (maskOp = 1) or Unmask operation
 *   (maskOp = 0).
 *  \return OK if successful, reject or fail otherwise.
 **************************************************************************/
static Int32 HAL_IsrMaskHsifInterrupt(Uint32 cause, Bool maskOp)
{
    Uint8  intrBit = HAL_GET_INTERRUPT_BIT(cause);
    Uint16 regVal = 0;
    Uint32 lane = intrBit/16;

#ifdef DEBUG_CAUSE_PRINTS
    printk( "-->%s:\n", __FUNCTION__);
#endif

    if (intrBit >= 16)
    {
        intrBit -= 16;
    }


    HAL_GetPhyReg16K( PHY_HSIF_RX_INT_MASK1_REG + lane, &regVal );

    if (maskOp)
    {
        regVal |= BIT_0 << intrBit;
    }
    else
    {
        HAL_SetPhyReg16K( PHY_HSIF_INT_STATUS1_REG + lane, (BIT_0 << intrBit) );  // Clear the appropriate bit in the status register
        regVal &= ~(BIT_0 << intrBit);
    }

    HAL_SetPhyReg16K( PHY_HSIF_RX_INT_MASK1_REG + lane, regVal );

    return 0;
}
#endif



/**************************************************************************/
/*! \fn static Int32 HAL_IsrMaskPhyInterrupt(Uint32 cause, Bool maskOp)
 **************************************************************************
 *  \brief Perform mask or unmask operation.
 *  \param[in] Uint32 cause - interrupts bitmask.
 *  \param[in] Bool maskOp - Mask operation (maskOp = 1) or Unmask operation
 *   (maskOp = 0).
 *  \return OK if successful, reject or fail otherwise.
 **************************************************************************/
static Int32 HAL_IsrMaskPhyInterrupt(Uint32 cause, Bool maskOp)
{
    Uint32 address;
    Uint8  intrBit;
    Uint32 intrGroup;
    Uint16 regVal = 0;
    Uint32 bitVal;
    Uint32 arrIndex;

#ifdef DEBUG_CAUSE_PRINTS
    Bool   isFecLock;
    printk( "-->%s:\n", __FUNCTION__);
#endif

    intrBit   = HAL_GET_INTERRUPT_BIT(cause);
    intrGroup = HAL_GET_INTERRUPT_GROUP(cause) - PHY_INT_GROUP;
    arrIndex  = HAL_PHY_GET_FEC_STATUS_DB_INDX(intrBit) + intrGroup*4;

    address = gPhyFecLockLostDb[arrIndex].isrEnRegAddress;

    switch (intrBit)
    {
        case PHY_INT_FEC_LOST_RX(0):
        case PHY_INT_FEC_LOST_RX(1):
        case PHY_INT_FEC_LOST_RX(2):
        case PHY_INT_FEC_LOST_RX(3):
            {
                intrBit = gPhyFecLockLostDb[arrIndex].fecLostBit;
                gPhyFecLockLostDb[arrIndex].isFecLostMasked = maskOp;
#ifdef DEBUG_CAUSE_PRINTS
                isFecLock = False;
#endif
                break;
            }
        case PHY_INT_FEC_LOCK_RX(0):
        case PHY_INT_FEC_LOCK_RX(1):
        case PHY_INT_FEC_LOCK_RX(2):
        case PHY_INT_FEC_LOCK_RX(3):
            {
                intrBit = gPhyFecLockLostDb[arrIndex].fecLockBit;
                gPhyFecLockLostDb[arrIndex].isFecLockMasked = maskOp;
#ifdef DEBUG_CAUSE_PRINTS
                isFecLock = True;
#endif
                break;
            }
        default:
            return -1;
            break;
    }

#ifdef DEBUG_CAUSE_PRINTS
    printk( "-->%s: RX=%d, lock=%d, intrBit=%d, maskOp=%d\n", __FUNCTION__, arrIndex+1, isFecLock, intrBit, maskOp);
#endif

#if PUMA6_OR_NEWER_SOC_TYPE
    {
        /* intrBit=1 ==> Interrupt Unmasked (enabled)           */
        /* intrBit=0 ==> Interrupt Masked (disabled)            */
        HAL_GetPhyReg16K(address, &regVal);
        if (maskOp)
        {
            /* mask (disable) the interrupt */
            regVal &= (~(1 << intrBit));
        }
        else
        {
            /* unmask (enable) the interrupt - clear the status first */
            HAL_SetPhyReg16K(address+1, (1 << intrBit));
            regVal |= (1 << intrBit);
        }
    }
#else
    if (system_rev == PUMA5_HW_REV_2_0)
    {
        /* in PHY core rev2.0, the logic of Mask/Unmask is:          */
        /* intrBit=1 & bit12=1 ==> Interrupt Unmasked (enabled)      */
        /* intrBit=1 & bit12=0 ==> Interrupt Masked (disabled)       */
        /* intrBit=0 ==> No change                                   */
        regVal = 1 << intrBit;
        if (!maskOp)
        {
            regVal |= (1 << PHY_BITWISE_ENABLE_ISR_HW_REV_2_0);
        }
    }
    else
    {
        /* in PHY core rev1.x, the logic of Mask/Unmask is:     */
        /* intrBit=1 ==> Interrupt Unmasked (enabled)           */
        /* intrBit=0 ==> Interrupt Masked (disabled)            */
        HAL_GetPhyReg16K(address, &regVal);
        if (maskOp)
        {
            /* mask (disable) the interrupt */
            regVal &= (~(1 << intrBit));
        }
        else
        {
            /* unmask (enable) the interrupt*/
            regVal |= (1 << intrBit);
        }
    }
#endif
    HAL_SetPhyReg16K(address, regVal);

#ifdef DEBUG_CAUSE_PRINTS
    printk( "-->%s: address = 0x%x, regVal = 0x%x\n", __FUNCTION__, address, regVal);
    printk(KERN_INFO "-->%s: updating database...\n", __FUNCTION__);
#endif

    /* Check the relevant registers for FEC loss/lock */
    bitVal = HAL_IsrGetFecStatusBitVal(arrIndex);

    if (bitVal != gPhyFecLockLostDb[arrIndex].currState)
    {
        /*updating database*/
        gPhyFecLockLostDb[arrIndex].prevState = gPhyFecLockLostDb[arrIndex].currState;
        gPhyFecLockLostDb[arrIndex].currState = bitVal;
    }

    return 0;
}


/* ************************************************************************ */
/*                                                                          */
/*                                                                          */
/*                                                                          */
/*                                                                          */
/*                                                                          */
/* ************************************************************************ */
static STATUS HAL_GetPhyReg16K(Uint32 phyRegAddr, Uint16 *pValue)
{
    Uint32 regOffset;

    regOffset = phyRegAddr << HAL_PHY_REGISTER_SHIFT;
    if(regOffset >= HAL_PHY_ADDRESS_SPACE_SIZE)
       return STATUS_NOK;

    *pValue = *(volatile Uint16*)(HAL_PHY_KERNEL_BASE_ADDRESS + regOffset);


#ifdef DEBUG_CAUSE_PRINTS
    printk(KERN_INFO "%s: phyRegAddr = 0x%x, regOffset = %d, value = (0x%x)\n", __FUNCTION__, phyRegAddr, regOffset, *pValue);
#endif

    return STATUS_OK;
}

static STATUS HAL_SetPhyReg16K(Uint32 phyRegAddr, Uint16 value)
{
    Uint32 regOffset;

    regOffset = phyRegAddr << HAL_PHY_REGISTER_SHIFT;
    if(regOffset >= HAL_PHY_ADDRESS_SPACE_SIZE)
       return STATUS_NOK;
#if PUMA6_OR_NEWER_SOC_TYPE
     /* In Puma6 the write to the PHY must be in 32 bit (in spite the fact that the PHY regs are 16 bits) */
     (*(volatile Uint32*)(HAL_PHY_KERNEL_BASE_ADDRESS + regOffset))= (Uint32)value;
#else
     *(volatile Uint16*)(HAL_PHY_KERNEL_BASE_ADDRESS + regOffset) = value;
#endif
#ifdef DEBUG_CAUSE_PRINTS
    printk(KERN_INFO "%s: phyRegAddr =  0x%x, regOffset = %d, value = (0x%x)\n", __FUNCTION__, phyRegAddr, regOffset, value);
#endif

    return STATUS_OK;
}

/* ************************************************************************ */

