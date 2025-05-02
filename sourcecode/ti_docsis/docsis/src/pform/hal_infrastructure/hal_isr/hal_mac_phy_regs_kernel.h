/* 
 
  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.
 
  GPL LICENSE SUMMARY
 
  Copyright(c) 2013 Intel Corporation.
 
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
 
  BSD LICENSE 
 
  Copyright(c) 2013 Intel Corporation. All rights reserved.
 
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:
 
    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived 
      from this software without specific prior written permission.
 
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/



#ifndef _HAL_MAC_PHY_REGS_KERNEL_H_
#define _HAL_MAC_PHY_REGS_KERNEL_H_
#include <puma_autoconf.h>



/* --------------------------------------------------------------- */
/* ------------------- COMMON (PUMA5 & PUMA6) -------------------- */
/* --------------------------------------------------------------- */



#define PHY_RX_CHANNEL_ADDRESS_SPACING              0x100
#define PHY_RX_CH_REG_OFFSET(ch,reg_num)            (((ch)*PHY_RX_CHANNEL_ADDRESS_SPACING) + (reg_num))

/**********************************/
/*      QAM register mapping      */
/**********************************/
#define PHY_QAM_MODE_0_REGISTER (0x01) /*(R/W, 0x0001)*/

#define PHY_QAM_ANNEX_TYPE          10
#define PHY_QAM_ANNEX_ANA           (1<<PHY_QAM_ANNEX_TYPE)

/*******************************************************/
/* FEC Status registers (duplicated for each receiver) */
/*******************************************************/
#define PHY_RX_FEC_STATUS(ch)       PHY_RX_CH_REG_OFFSET((ch) ,0xA3)

/* Bit Mapping */
#define PHY_RX_FEC_STATUS_INT_PREMASK               2
#define PHY_RX_FEC_STATUS_TR_SYNC                   1 /* This is the bit that indicates FEC lock.  */
#define PHY_RX_FEC_STATUS_MPEG_SYNC                 0



/* --------------- moved from: hal_phy_interrupt_regs.h --------------------------- */



/* NOTE:                                                                  */
/* Unlike the MAC interrupts, the PHY interrupt causes are not directly   */
/* linked to the bits in the status-register. This is the definition of   */
/* PHY-Interrupt causes that will be used by all the SW mechanisms.       */

/*****************/
/* Rx            */
/*****************/
#define PHY_INT_FEC_LOST_RX(ch)                  (((ch) & 0x3) * 6 + 0)
#define PHY_INT_FEC_LOCK_RX(ch)                  (((ch) & 0x3) * 6 + 1)
#define PHY_INT_MPEG_LOST_RX(ch)                 (((ch) & 0x3) * 6 + 2)
#define PHY_INT_MPEG_LOCK_RX(ch)                 (((ch) & 0x3) * 6 + 3)
#define PHY_INT_QAM_LOCK_RX(ch)                  (((ch) & 0x3) * 6 + 4)
#define PHY_INT_QAM_LOST_RX(ch)                  (((ch) & 0x3) * 6 + 5)





/* --------------------------------------------------------------- */
/* --------------------------- PUMA6 ----------------------------- */
/* --------------------------------------------------------------- */
#if PUMA6_OR_NEWER_SOC_TYPE


/* ------------------- moved from: hal_phy_hsif_puma6.h ------------------------ */

typedef enum HsifLanesIndex
{
    HSIF_LANE0 = 0, /* 0 */
    HSIF_LANE1,     /* 1 */
    HSIF_MAX_LANES, /* 2 */
    HSIF_INVALID_LANE = HSIF_MAX_LANES
}HsifLanesIndex_e;

#define PHY_HSIF_INT_STATUS1_REG    (0x1E48)    /*R/W   0x0000  */
/* ------------------------------------------------------------------------------
Read :
    '0' : No interrupt received
    '1' : Interrupt received

Write :-
    '0' : NOP
    '1' : Clear respective status bit (write bit is self clearing apart from the Rx OP  FIFO underflow bit).

Bit[0] : Rx Lane 0 SERDES PHY sync locked.
Bit[1] : Rx Lane 0 SERDES PHY sync unlocked.
Bit[2] : Rx Lane 0 sync locked.
Bit[3] : Rx Lane 0 sync unlocked.
Bit[4] : RxLane 0 sync watchdog.
Bit[5] : Rx Lane 0 symbol error.
Bit[6] : Rx Lane 0 frame error.
Bit[7] : Rx Lane 0 PRBS pattern found.
Bit[8] : Rx Lane 0 PRBS pattern lost.
Bit[9] : Rx Lane 0 I/P FIFO overflow.
Bit[10] Rx OP  FIFO underflow
Bit[11] : Tx Lane 0 I/P Buffer overflow.
Bit[12] : Tx Lane 0 I/P Buffer underflow.
Bits[15:13] : Reserved.

Add interrupts for FIFO over/underflows...?

 **------------------------------------------------------------------------------*/


#define PHY_HSIF_INT_STATUS2_REG    (0x1E49)    /*  R/W 0x0000  */
/* ------------------------------------------------------------------------------
Read :-
    '0' : No interrupt received
    '1' : Interrupt received

Write :-
    '0' : NOP
    '1' : Clear respective status bit (bit write is self clearing apart from the Rx OP  FIFO overflow bit).

Bit[0] : Rx Lane 1 SERDES PHY sync locked.
Bit[1] : Rx Lane 1 SERDES PHY sync unlocked.
Bit[2] : Rx Lane 1 sync locked.
Bit[3] : Rx Lane 1 sync unlocked.
Bit[4] : Rx Lane 1 sync watchdog.
Bit[5] : Rx Lane 1 symbol error.
Bit[6] : Rx Lane 1 frame error.
Bit[7] : Rx Lane 1 PRBS pattern found.
Bit[8] : Rx Lane 1 PRBS pattern lost.
Bit[9] : Rx Lane 1 I/P FIFO overflow.
Bit[10] Rx OP  FIFO overflow
Bit[11] : Tx Lane 1 I/P Buffer overflow.
Bit[12] : Tx Lane 1 I/P Buffer underflow.

Bits[15:1] : Reserved.

Add interrupts for FIFO over/underflows...?

 **------------------------------------------------------------------------------*/




#define PHY_HSIF_RX_INT_MASK1_REG   (0x1E46)    /*R/W   0x00FF  D/S RX Interrupt Masks :-*/
/* ------------------------------------------------------------------------------
'0' = Allow interrupt
'1' = Mask interrupt

Bit[0] : Rx Lane 0 SERDES PHY sync unlocked -> locked.
Bit[1] : Rx Lane 0 SERDES PHY sync locked -> unlocked.

Bit[2] : Rx Lane 0 sync locked.
Bit[3] : Rx Lane 0 sync unlocked.
Bit[4] : Rx Lane 0 sync watchdog.
Bit[5] : Rx Lane 0 symbol error.
Bit[6] : Rx Lane 0 frame error.
Bit[7] : Rx Lane 0 PRBS pattern found.
Bit[8] : Rx Lane 0 PRBS pattern lost
Bit[9] : Rx Lane 0 I/P FIFO overflow
Bit[10] Rx OP  FIFO underflow
Bit[11] : Tx Lane 0 I/P Buffer overflow
Bit[12] : Tx Lane 0 I/P Buffer underflow.
Bits[15:13] : Reserved
 **------------------------------------------------------------------------------*/




#define PHY_HSIF_RX_INT_MASK2_REG   (0x1E47)    /*  R/W 0x01FF  */
/* ------------------------------------------------------------------------------
Bit[0] : Rx Lane 1 SERDES PHY sync locked.
Bit[1] : Rx Lane 1 SERDES PHY sync unlocked.
Bit[2] : Rx Lane 1 sync locked.
Bit[3] : Rx Lane 1 sync unlocked.
Bit[4] : Rx Lane 1 sync watchdog.
Bit[5] : Rx Lane 1 symbol error.
Bit[6] : Rx Lane 1 frame error.
Bit[7] : Rx Lane 1 PRBS pattern found.
Bit[8] : Rx Lane 1 PRBS pattern lost
Bit[9] : Rx Lane 1 I/P FIFO overflow.
Bit[10] Rx OP  FIFO overflow
Bit[11] : Tx Lane 1 I/P Buffer overflow
Bit[12] : Tx Lane 1 I/P Buffer underflow.
Bits[15:13] : Reserved
 **------------------------------------------------------------------------------*/


#define PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_LOCKED_BIT         (0)
#define PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED_BIT       (1)
#define PHY_HSIF_INT_RX_LANE_SYNC_LOCKED_BIT                    (2)
#define PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED_BIT                  (3)
#define PHY_HSIF_INT_RX_LANE_SYNC_WATCHDOG_BIT                  (4)
#define PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR_BIT                   (5)
#define PHY_HSIF_INT_RX_LANE_FRAME_ERROR_BIT                    (6)
#define PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_FOUND_BIT             (7)
#define PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_LOST_BIT              (8)
#define PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW_BIT            (9)
#define PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW_BIT     (10) /* bit 10 indicates undeflow in PHY_HSIF_INT_STATUS1_REG and overflow in PHY_HSIF_INT_STATUS2_REG*/
#define PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_OVERFLOW_BIT          (11)
#define PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_UNDERFLOW_BIT         (12)

#define PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_LOCKED         (BIT_0 << PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_LOCKED_BIT  )
#define PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED       (BIT_0 << PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED_BIT)
#define PHY_HSIF_INT_RX_LANE_SYNC_LOCKED                    (BIT_0 << PHY_HSIF_INT_RX_LANE_SYNC_LOCKED_BIT             )
#define PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED                  (BIT_0 << PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED_BIT           )
#define PHY_HSIF_INT_RX_LANE_SYNC_WATCHDOG                  (BIT_0 << PHY_HSIF_INT_RX_LANE_SYNC_WATCHDOG_BIT           )
#define PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR                   (BIT_0 << PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR_BIT            )
#define PHY_HSIF_INT_RX_LANE_FRAME_ERROR                    (BIT_0 << PHY_HSIF_INT_RX_LANE_FRAME_ERROR_BIT             )
#define PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_FOUND             (BIT_0 << PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_FOUND_BIT      )
#define PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_LOST              (BIT_0 << PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_LOST_BIT       )
#define PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW            (BIT_0 << PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW_BIT     )
#define PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW    (BIT_0 << PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW_BIT)
#define PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_OVERFLOW          (BIT_0 << PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_OVERFLOW_BIT   )
#define PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_UNDERFLOW         (BIT_0 << PHY_HSIF_INT_TX_LANE_INPUT_BUFFER_UNDERFLOW_BIT  )

#define PHY_HSIF_INT_RX_LANE_ALL_INT   \
          ( PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_LOCKED   | \
            PHY_HSIF_INT_RX_LANE_SERDES_PHY_SYNC_UNLOCKED | \
            PHY_HSIF_INT_RX_LANE_SYNC_LOCKED              | \
            PHY_HSIF_INT_RX_LANE_SYNC_UNLOCKED            | \
            PHY_HSIF_INT_RX_LANE_SYNC_WATCHDOG            | \
            PHY_HSIF_INT_RX_LANE_SYMBOL_ERROR             | \
            PHY_HSIF_INT_RX_LANE_FRAME_ERROR              | \
            PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_FOUND       | \
            PHY_HSIF_INT_RX_LANE_PRBS_PATTERN_LOST        | \
            PHY_HSIF_INT_RX_LANE_INPUT_FIFO_OVERFLOW      | \
            PHY_HSIF_INT_RX_LANE_OUTPUT_FIFO_OVER_UNDER_FLOW )




/* ------------------- moved from: hal_mac_regs_and_addrs.h ------------------------ */

/********************************/
/*  KERNEL                      */
/********************************/
#define MAC_KERNEL_BASE_ADDRESS                  (0xD1000000)




/* ------------------- moved from: hal_phy_regs_puma6.h --------------------------- */

#define PHY_GENERAL_INTERRUPT_STATUS                (0x1f2b)
/* Bit	Name            Default 	Description                 */
/* 0 	MCR1	        0	        1- Interrupt, 0-No Interrupt*/ 
/* 1	MCR1	        0	        1- Interrupt, 0-No Interrupt*/ 
/* 2 	HSIF	        0	        1- Interrupt, 0-No Interrupt*/ 
/* 3	MPEG OUT	    0	        1- Interrupt, 0-No Interrupt*/ 
/* 15:4	Reserved	    0	                                    */
/*-------------------------------------------------------------*/




/* ------------------- moved from: hal_phy_regs_and_addrs.h ----------------------- */



#define HAL_PHY_BASE_OFFSET             (0x01450000)

#define HAL_PHY_ADDRESS_SPACE_SIZE      (0x8000)

/* Phy register address shift value*/
#define HAL_PHY_REGISTER_SHIFT         (2)

#define HAL_PHY_KERNEL_BASE_ADDRESS     (0xD1000000 + HAL_PHY_BASE_OFFSET)





/* ------------------- moved from: hal_phy_regs.h ---------------------------- */



typedef enum
{
    PHY_RECEIVER_CHANNEL_1 = 0,
    PHY_RECEIVER_CHANNEL_2,
    PHY_RECEIVER_CHANNEL_3,
    PHY_RECEIVER_CHANNEL_4,
    PHY_RECEIVER_CHANNEL_5,
    PHY_RECEIVER_CHANNEL_6,
    PHY_RECEIVER_CHANNEL_7,
    PHY_RECEIVER_CHANNEL_8,
    PHY_RECEIVER_CHANNEL_9,
    PHY_RECEIVER_CHANNEL_10,
    PHY_RECEIVER_CHANNEL_11,
    PHY_RECEIVER_CHANNEL_12,
    PHY_RECEIVER_CHANNEL_13,
    PHY_RECEIVER_CHANNEL_14,
    PHY_RECEIVER_CHANNEL_15,
    PHY_RECEIVER_CHANNEL_16,
    PHY_RECEIVER_CHANNEL_17,
    PHY_RECEIVER_CHANNEL_18,
    PHY_RECEIVER_CHANNEL_19,
    PHY_RECEIVER_CHANNEL_20,
    PHY_RECEIVER_CHANNEL_21,
    PHY_RECEIVER_CHANNEL_22,
    PHY_RECEIVER_CHANNEL_23,
    PHY_RECEIVER_CHANNEL_24,
    PHY_RECEIVER_CHANNEL_25,
    PHY_RECEIVER_CHANNEL_26,
    PHY_RECEIVER_CHANNEL_27,
    PHY_RECEIVER_CHANNEL_28,
    PHY_RECEIVER_CHANNEL_29,
    PHY_RECEIVER_CHANNEL_30,
    PHY_RECEIVER_CHANNEL_31,
    PHY_RECEIVER_CHANNEL_32,
    PHY_RECEIVER_COUNT,
    PHY_INTERNAL_COUNT = PHY_RECEIVER_CHANNEL_25
} PHY_RECEIVER_CHANNEL_e;

#define PHY_MAX_RECEIVERS_24   24
#define PHY_MAX_RECEIVERS_32   32


/* ------------------- moved from: hal_phy_basic_regs.h --------------------------- */

/**********************************/
/*     PHY Interrupt Registers    */
/**********************************/

#define PHY_RX_INTERRUPT_ENABLE_REG(ch)             ((((ch)/2)*2 + 0x1F10))
#define PHY_RX_INTERRUPT_STATUS_REG(ch)             ((((ch)/2)*2 + 0x1F11))

/* Bit Mapping */

#define PHY_MPEG_LOST_RX(ch)                                    (((ch) & 0x1) * 6 + 5)
#define PHY_MPEG_LOCKED_RX(ch)                                  (((ch) & 0x1) * 6 + 4)
#define PHY_FEC_LOST_RX(ch)                                     (((ch) & 0x1) * 6 + 3)
#define PHY_FEC_LOCKED_RX(ch)                                   (((ch) & 0x1) * 6 + 2)
#define PHY_QAM_LOST_RX(ch)                                     (((ch) & 0x1) * 6 + 1)
#define PHY_QAM_LOCKED_RX(ch)                                   (((ch) & 0x1) * 6 + 0)


/* --------------------------------------------------------------- */
/* --------------------------- PUMA5 ----------------------------- */
/* --------------------------------------------------------------- */

#else





/* ------------------- moved from: hal_phy_regs.h --------------------------- */

typedef enum
{
    PHY_RECEIVER_CHANNEL_1 = 0,
    PHY_RECEIVER_CHANNEL_2,
    PHY_RECEIVER_CHANNEL_3,
    PHY_RECEIVER_CHANNEL_4,
    PHY_RECEIVER_CHANNEL_5,
    PHY_RECEIVER_CHANNEL_6,
    PHY_RECEIVER_CHANNEL_7,
    PHY_RECEIVER_CHANNEL_8,
    PHY_RECEIVER_COUNT,
    PHY_INTERNAL_COUNT = PHY_RECEIVER_CHANNEL_5
} PHY_RECEIVER_CHANNEL_e;





/* ------------------- moved from: hal_phy_basic_regs.h --------------------------- */


/**********************************/
/*     PHY Interrupt Registers    */
/**********************************/


#define PHY_RX_INTERRUPT_ENABLE_REG(ch)             (((((ch)& 0x3)/2)*2 + 0xC2) + (((ch)/4)*0x400))
#define PHY_RX_INTERRUPT_STATUS_REG(ch)             (((((ch)& 0x3)/2)*2 + 0xC3) + (((ch)/4)*0x400))

/* Bit Mapping */
/********************/
/* PUMA5_HW_REV_1_X */
/********************/
#define PHY_HW_REV_1_X_QAM_LOST_RX(ch)                          (((ch) & 0x1) * 7 + 6)
#define PHY_HW_REV_1_X_QAM_LOCKED_RX(ch)                        (((ch) & 0x1) * 7 + 5)
#define PHY_HW_REV_1_X_MPEG_LOCKED_RX(ch)                       (((ch) & 0x1) * 7 + 4)
#define PHY_HW_REV_1_X_MPEG_LOST_RX(ch)                         (((ch) & 0x1) * 7 + 3)
#define PHY_HW_REV_1_X_FEC_LOCKED_RX(ch)                        (((ch) & 0x1) * 7 + 2)
#define PHY_HW_REV_1_X_FEC_LOST_RX(ch)                          (((ch) & 0x1) * 7 + 1)
#define PHY_HW_REV_1_X_FEC_PMONITOR_RX(ch)                      (((ch) & 0x1) * 7 + 0)

/********************/
/* PUMA5_HW_REV_2_0 */
/********************/
#define PHY_BITWISE_ENABLE_ISR_HW_REV_2_0                      12

#define PHY_HW_REV_2_0_QAM_LOST_RX(ch)                          (((ch) & 0x1) * 6 + 5)
#define PHY_HW_REV_2_0_QAM_LOCKED_RX(ch)                        (((ch) & 0x1) * 6 + 4)
#define PHY_HW_REV_2_0_MPEG_LOCKED_RX(ch)                       (((ch) & 0x1) * 6 + 3)
#define PHY_HW_REV_2_0_MPEG_LOST_RX(ch)                         (((ch) & 0x1) * 6 + 2)
#define PHY_HW_REV_2_0_FEC_LOCKED_RX(ch)                        (((ch) & 0x1) * 6 + 1)
#define PHY_HW_REV_2_0_FEC_LOST_RX(ch)                          (((ch) & 0x1) * 6 + 0)


/* ------------------- moved from: hal_mac_regs_and_addrs.h --------------------------- */

/********************************/
/*  KERNEL                      */
/********************************/
#define MAC_KERNEL_BASE_ADDRESS             (0xD9000000)



/* ------------------- moved from: hal_phy_regs_and_addrs.h --------------------------- */

#define HAL_PHY_BASE_OFFSET             (0x00070000)

#define HAL_PHY_ADDRESS_SPACE_SIZE      (0x1000)

/* Phy register address shift value*/
#define HAL_PHY_REGISTER_SHIFT         (1)

#define HAL_PHY_KERNEL_BASE_ADDRESS     (0xD9000000 + HAL_PHY_BASE_OFFSET)


#endif





#endif
