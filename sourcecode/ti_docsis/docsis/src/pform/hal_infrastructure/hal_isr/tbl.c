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
 * tbl.c
 * Description:
 * DOCSIS interrupt table implementation
*/

#include <linux/fs.h>       /* chrdev allocation */
#include <linux/cdev.h>

#include <puma_autoconf.h>
#include "hal_interrupt_cause.h"
#include "docint.h"

/* interrupt cause table */
dih_t docitbl[] =
{
    /*------------------------------------------*/
    /* MAC_ERR1_INT_GROUP (start cause = 0)     */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

#if PUMA6_OR_NEWER_SOC_TYPE
    /*------------------------------------------*/
    /* MAC_ERR1A_INT_GROUP (start cause = 32)   */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* MAC_ERR1B_INT_GROUP (start cause = 64)   */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* MAC_ERR1C_INT_GROUP (start cause = 96)   */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */
#endif

    /*------------------------------------------*/
    /* MAC_ERR2_INT_GROUP (start cause = 128)   */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 1 */   /* MAC_ERR2_INT_SYNC_LOST */
    { docint_default_handler, IMASKED, NULL },          /* 2 */   /* MAP lost for CH04 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */   /* MAP lost for CH03 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */   /* MAP lost for CH02 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */   /* MAP lost for CH01 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 16 */  /* MAC_ERR2_INT_PHY_ERR_US4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 17 */  /* MAC_ERR2_INT_PHY_ERR_US3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 18 */  /* MAC_ERR2_INT_PHY_ERR_US2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 19 */  /* MAC_ERR2_INT_PHY_ERR_US1 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 20 */  /* MAC_ERR2_INT_TX_ERR_US4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 21 */  /* MAC_ERR2_INT_TX_ERR_US3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 22 */  /* MAC_ERR2_INT_TX_ERR_US2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 23 */  /* MAC_ERR2_INT_TX_ERR_US1 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 24 */  /* MAC_ERR2_INT_FIFO_FULL_US4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 25 */  /* MAC_ERR2_INT_FIFO_FULL_US3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 26 */  /* MAC_ERR2_INT_FIFO_FULL_US2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 27 */  /* MAC_ERR2_INT_FIFO_FULL_US1 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

#if PUMA6_OR_NEWER_SOC_TYPE
    /*------------------------------------------*/
    /* MAC_ERR2A_INT_GROUP (start cause = 160)  */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 16 */  /* MAC_ERR2A_INT_PHY_ERR_US8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 17 */  /* MAC_ERR2A_INT_PHY_ERR_US7 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 18 */  /* MAC_ERR2A_INT_PHY_ERR_US6 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 19 */  /* MAC_ERR2A_INT_PHY_ERR_US5 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 20 */  /* MAC_ERR2A_INT_TX_ERR_US8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 21 */  /* MAC_ERR2A_INT_TX_ERR_US7 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 22 */  /* MAC_ERR2A_INT_TX_ERR_US6 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 23 */  /* MAC_ERR2A_INT_TX_ERR_US5 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 24 */  /* MAC_ERR2A_INT_FIFO_FULL_US8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 25 */  /* MAC_ERR2A_INT_FIFO_FULL_US7 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 26 */  /* MAC_ERR2A_INT_FIFO_FULL_US6 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 27 */  /* MAC_ERR2A_INT_FIFO_FULL_US5 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */
#endif

#if PUMA6_OR_NEWER_SOC_TYPE
    /*------------------------------------------*/
    /* MAC_ERR3_INT_GROUP (start cause = 192)   */
    /*------------------------------------------*/
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 0 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS16 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 1 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS15 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 2 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS14 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 3 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS13 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 4 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS12 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 5 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS11 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 6 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS10 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 7 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS9 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 8 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 9 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS7 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 10 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS6 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 11 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS5 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 12 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 13 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 14 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 15 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS1 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 22 */  /* MAC_ERR3_INT_CNCT_SIZE_US4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 23 */  /* MAC_ERR3_INT_CNCT_SIZE_US3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 24 */  /* MAC_ERR3_INT_CNCT_SIZE_US2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 25 */  /* MAC_ERR3_INT_CNCT_SIZE_US1 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 26 */  /* MAC_ERR3_INT_MP_ORD_FIFO_FULL */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 27 */  /* MAC_ERR3_INT_CNCT_SIZE_US8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 28 */  /* MAC_ERR3_INT_CNCT_SIZE_US7 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 29 */  /* MAC_ERR3_INT_CNCT_SIZE_US6 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 30 */  /* MAC_ERR3_INT_CNCT_SIZE_US5 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* MAC_ERR3A_INT_GROUP (start cause = 224)  */
    /*------------------------------------------*/
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 0 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS32 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 1 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS31 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 2 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS30 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 3 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS29 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 4 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS28 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 5 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS27 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 6 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS26 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 7 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS25 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 8 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS24 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 9 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS23 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 10 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS22 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 11 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS21 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 12 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS20 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 13 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS19 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 14 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS18 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 15 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS17 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

#else
    /*------------------------------------------*/
    /* MAC_ERR3_INT_GROUP (start cause = 192)   */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */   /* MAC_ERR3_INT_MDD_TIMER_EXP_DS1 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */   /* MAC_ERR3_INT_MDD_TIMER_EXP_DS2 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */   /* MAC_ERR3_INT_MDD_TIMER_EXP_DS3 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */   /* MAC_ERR3_INT_MDD_TIMER_EXP_DS4 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */   /* MAC_ERR3_INT_MDD_TIMER_EXP_DS5 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */   /* MAC_ERR3_INT_MDD_TIMER_EXP_DS6 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */   /* MAC_ERR3_INT_MDD_TIMER_EXP_DS7 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */   /* MAC_ERR3_INT_MDD_TIMER_EXP_DS8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 8 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 9 */   /* MAC_ERR3_INT_IFIFO_OVERRUN_DS7 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 10 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS6 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 11 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS5 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 12 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 13 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 14 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 15 */  /* MAC_ERR3_INT_IFIFO_OVERRUN_DS1 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 22 */  /* MAC_ERR3_INT_CNCT_SIZE_US4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 23 */  /* MAC_ERR3_INT_CNCT_SIZE_US3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 24 */  /* MAC_ERR3_INT_CNCT_SIZE_US2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 25 */  /* MAC_ERR3_INT_CNCT_SIZE_US1 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 26 */  /* MAC_ERR3_INT_MP_ORD_FIFO_FULL */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */
#endif

    /*------------------------------------------*/
    /* MAC_ERR4_INT_GROUP (start cause = 256)   */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* MAC_UCD_INT_GROUP (start cause = 288)    */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* MAC_US_FW_INT_GROUP (start cause = 320)  */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* MAC_DS_FW_INT_GROUP (start cause = 352)  */
    /*------------------------------------------*/
#if PUMA6_OR_NEWER_SOC_TYPE
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 0 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS1 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 1 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 2 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 3 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 4 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS5 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 5 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS6 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 6 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS7 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 7 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 8 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS9 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 9 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS10 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 10 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS11 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 11 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS12 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 12 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS13 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 13 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS14 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 14 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS15 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 15 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS16 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 16 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS17 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 17 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS18 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 18 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS19 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 19 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS20 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 20 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS21 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 21 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS22 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 22 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS23 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 23 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS24 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 24 */  /* MAC_DS_FW_PDSP1_KEY_SEQ_ERROR */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 25 */  /* MAC_DS_FW_PDSP2_SEQ_OUT_OF_RNG_ERROR */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */
#else
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 0 */   /* MAC_DS_FW_PDSP1_KEY_SEQ_ERROR */
    { docint_default_handler, IMASKED, NULL },          /* 1 */   /* Reserved */
    { docint_default_handler, IMASKED, NULL },          /* 2 */   /* Reserved */
    { docint_default_handler, IMASKED, NULL },          /* 3 */   /* Reserved */
    { docint_default_handler, IMASKED, NULL },          /* 4 */   /* Reserved */
    { docint_default_handler, IMASKED, NULL },          /* 5 */   /* Reserved */
    { docint_default_handler, IMASKED, NULL },          /* 6 */   /* Reserved */
    { docint_default_handler, IMASKED, NULL },          /* 7 */   /* Reserved */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 8 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS1 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 9 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 10 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 11 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 12 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS5 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 13 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS6 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 14 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS7 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 15 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 16 */  /* MAC_DS_FW_PDSP2_SEQ_OUT_OF_RNG_ERROR */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */
#endif
#if PUMA6_OR_NEWER_SOC_TYPE
    /*------------------------------------------*/
    /* MAC_DS_FW_INT_GROUP2 (start cause = 384) Only Valid for Puma6D0 */
    /*------------------------------------------*/
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 0 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS1 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 1 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS2 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 2 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS3 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 3 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS4 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 4 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS5 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 5 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS6 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 6 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS7 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 7 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS8 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 8 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS9 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 9 */   /* MAC_DS_FW_INT_MDD_ARRIVED_DS10 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 10 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS11 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 11 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS12 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 12 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS13 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 13 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS14 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 14 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS15 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 15 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS16 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 16 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS17 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 17 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS18 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 18 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS19 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 19 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS20 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 20 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS21 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 21 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS22 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 22 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS23 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 23 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS24 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 24 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS25 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 25 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS26 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 26 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS27 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 27 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS28 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 28 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS29 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 29 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS30 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 30 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS31 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 31 */  /* MAC_DS_FW_INT_MDD_ARRIVED_DS32 */
#endif
    /*------------------------------------------*/
    /* PHY_INT_GROUP (start cause = 416)        */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0  */  /*Rx1: 0-5*/
    { docint_default_handler, IMASKED, NULL },          /* 1  */
    { docint_default_handler, IMASKED, NULL },          /* 2  */
    { docint_default_handler, IMASKED, NULL },          /* 3  */
    { docint_default_handler, IMASKED, NULL },          /* 4  */
    { docint_default_handler, IMASKED, NULL },          /* 5  */
    { docint_default_handler, IMASKED, NULL },          /* 6  */  /*Rx2: 6-11*/
    { docint_default_handler, IMASKED, NULL },          /* 7  */
    { docint_default_handler, IMASKED, NULL },          /* 8  */
    { docint_default_handler, IMASKED, NULL },          /* 9  */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */  /*Rx3: 12-17*/
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */  /*Rx4: 18-23*/
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

#if PUMA6_OR_NEWER_SOC_TYPE
    /*------------------------------------------*/
    /* PHY_INT_GROUP1 (start cause = 448)       */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0  */  /*Rx5: 0-5*/
    { docint_default_handler, IMASKED, NULL },          /* 1  */
    { docint_default_handler, IMASKED, NULL },          /* 2  */
    { docint_default_handler, IMASKED, NULL },          /* 3  */
    { docint_default_handler, IMASKED, NULL },          /* 4  */
    { docint_default_handler, IMASKED, NULL },          /* 5  */
    { docint_default_handler, IMASKED, NULL },          /* 6  */  /*Rx6: 6-11*/
    { docint_default_handler, IMASKED, NULL },          /* 7  */
    { docint_default_handler, IMASKED, NULL },          /* 8  */
    { docint_default_handler, IMASKED, NULL },          /* 9  */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */  /*Rx7: 12-17*/
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */  /*Rx8: 18-23*/
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* PHY_INT_GROUP2 (start cause = 480)       */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0  */  /*Rx9: 0-5*/
    { docint_default_handler, IMASKED, NULL },          /* 1  */
    { docint_default_handler, IMASKED, NULL },          /* 2  */
    { docint_default_handler, IMASKED, NULL },          /* 3  */
    { docint_default_handler, IMASKED, NULL },          /* 4  */
    { docint_default_handler, IMASKED, NULL },          /* 5  */
    { docint_default_handler, IMASKED, NULL },          /* 6  */  /*Rx10: 6-11*/
    { docint_default_handler, IMASKED, NULL },          /* 7  */
    { docint_default_handler, IMASKED, NULL },          /* 8  */
    { docint_default_handler, IMASKED, NULL },          /* 9  */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */  /*Rx11: 12-17*/
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */  /*Rx12: 18-23*/
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* PHY_INT_GROUP3 (start cause = 512)       */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0  */  /*Rx13: 0-5*/
    { docint_default_handler, IMASKED, NULL },          /* 1  */
    { docint_default_handler, IMASKED, NULL },          /* 2  */
    { docint_default_handler, IMASKED, NULL },          /* 3  */
    { docint_default_handler, IMASKED, NULL },          /* 4  */
    { docint_default_handler, IMASKED, NULL },          /* 5  */
    { docint_default_handler, IMASKED, NULL },          /* 6  */  /*Rx14: 6-11*/
    { docint_default_handler, IMASKED, NULL },          /* 7  */
    { docint_default_handler, IMASKED, NULL },          /* 8  */
    { docint_default_handler, IMASKED, NULL },          /* 9  */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */  /*Rx15: 12-17*/
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */  /*Rx16: 18-23*/
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* PHY_INT_GROUP4 (start cause = 544)       */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0  */  /*Rx17: 0-5*/
    { docint_default_handler, IMASKED, NULL },          /* 1  */
    { docint_default_handler, IMASKED, NULL },          /* 2  */
    { docint_default_handler, IMASKED, NULL },          /* 3  */
    { docint_default_handler, IMASKED, NULL },          /* 4  */
    { docint_default_handler, IMASKED, NULL },          /* 5  */
    { docint_default_handler, IMASKED, NULL },          /* 6  */  /*Rx18: 6-11*/
    { docint_default_handler, IMASKED, NULL },          /* 7  */
    { docint_default_handler, IMASKED, NULL },          /* 8  */
    { docint_default_handler, IMASKED, NULL },          /* 9  */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */  /*Rx19: 12-17*/
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */  /*Rx20: 18-23*/
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* PHY_INT_GROUP5 (start cause = 576)       */
    /*------------------------------------------*/
    { docint_default_handler, IMASKED, NULL },          /* 0  */  /*Rx21: 0-5*/
    { docint_default_handler, IMASKED, NULL },          /* 1  */
    { docint_default_handler, IMASKED, NULL },          /* 2  */
    { docint_default_handler, IMASKED, NULL },          /* 3  */
    { docint_default_handler, IMASKED, NULL },          /* 4  */
    { docint_default_handler, IMASKED, NULL },          /* 5  */
    { docint_default_handler, IMASKED, NULL },          /* 6  */  /*Rx22: 6-11*/
    { docint_default_handler, IMASKED, NULL },          /* 7  */
    { docint_default_handler, IMASKED, NULL },          /* 8  */
    { docint_default_handler, IMASKED, NULL },          /* 9  */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */  /*Rx23: 12-17*/
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */  /*Rx24: 18-23*/
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* PHY_INT_HSIF_GROUP (start cause = 608)   */
    /*------------------------------------------*/
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 0  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 1  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 2  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 3  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 4  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 5  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 6  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 7  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 8  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 9  */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 10 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 11 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 12 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 13 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 14 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 15 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 16 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 17 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 18 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 19 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 20 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 21 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 22 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 23 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 24 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 25 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 26 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 27 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 28 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 29 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 30 */
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 31 */

#endif

    /*------------------------------------------*/
    /* PHY GRT INT (start cause = 640)          */
    /*------------------------------------------*/
#if PUMA6_OR_NEWER_SOC_TYPE
    { docint_pga_interrupt_handler, IMASKED, NULL },    /* 0 */
#else
    { docint_default_handler, IMASKED, NULL },          /* 0 */
#endif
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */

    /*------------------------------------------*/
    /* PHY_MPT_INT_GROUP (start cause = 672)    */
    /*------------------------------------------*/
    { docint_mask_interrupt_handler, IMASKED, NULL },   /* 0 */
    { docint_default_handler, IMASKED, NULL },          /* 1 */
    { docint_default_handler, IMASKED, NULL },          /* 2 */
    { docint_default_handler, IMASKED, NULL },          /* 3 */
    { docint_default_handler, IMASKED, NULL },          /* 4 */
    { docint_default_handler, IMASKED, NULL },          /* 5 */
    { docint_default_handler, IMASKED, NULL },          /* 6 */
    { docint_default_handler, IMASKED, NULL },          /* 7 */
    { docint_default_handler, IMASKED, NULL },          /* 8 */
    { docint_default_handler, IMASKED, NULL },          /* 9 */
    { docint_default_handler, IMASKED, NULL },          /* 10 */
    { docint_default_handler, IMASKED, NULL },          /* 11 */
    { docint_default_handler, IMASKED, NULL },          /* 12 */
    { docint_default_handler, IMASKED, NULL },          /* 13 */
    { docint_default_handler, IMASKED, NULL },          /* 14 */
    { docint_default_handler, IMASKED, NULL },          /* 15 */
    { docint_default_handler, IMASKED, NULL },          /* 16 */
    { docint_default_handler, IMASKED, NULL },          /* 17 */
    { docint_default_handler, IMASKED, NULL },          /* 18 */
    { docint_default_handler, IMASKED, NULL },          /* 19 */
    { docint_default_handler, IMASKED, NULL },          /* 20 */
    { docint_default_handler, IMASKED, NULL },          /* 21 */
    { docint_default_handler, IMASKED, NULL },          /* 22 */
    { docint_default_handler, IMASKED, NULL },          /* 23 */
    { docint_default_handler, IMASKED, NULL },          /* 24 */
    { docint_default_handler, IMASKED, NULL },          /* 25 */
    { docint_default_handler, IMASKED, NULL },          /* 26 */
    { docint_default_handler, IMASKED, NULL },          /* 27 */
    { docint_default_handler, IMASKED, NULL },          /* 28 */
    { docint_default_handler, IMASKED, NULL },          /* 29 */
    { docint_default_handler, IMASKED, NULL },          /* 30 */
    { docint_default_handler, IMASKED, NULL },          /* 31 */
};
