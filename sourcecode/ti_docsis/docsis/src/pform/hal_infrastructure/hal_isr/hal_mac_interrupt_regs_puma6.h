/*
 
  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.
 
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
 
  BSD LICENSE 
 
  Copyright(c) 2008-2013 Intel Corporation. All rights reserved.
 
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:
 
 *    Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
 *    Redistributions in binary form must reproduce the above copyright
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

/*
 * hal_mac_interrupt_regs_puma6.h
 * Description:
 * defines the MAC interrupt registers and their content
*/


/*! \file hal_mac_interrupt_regs_puma6.h                                         */
/*  \brief defines the MAC interrupt registers and their content.          */

#ifndef _HAL_MAC_INTERRUPT_REGS_PUMA6_H_
#define _HAL_MAC_INTERRUPT_REGS_PUMA6_H_


/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/


/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/
//TBD - puma6: ??????????????


#define MAC_HOST_INTERRUPT_MASK_REG(group_base)     (group_base+0x0)
#define MAC_HOST_INTERRUPT_PREMASK_REG(group_base)  (group_base+0x4)
#define MAC_HOST_INTERRUPT_STATUS_REG(group_base)   (group_base+0x8)


/**********************************/
/*  MAC Error-Interrupt Registers */
/**********************************/
#define MAC_INTERRUPT_REG_OFFSET                (0x01460000)
#define MAC_INTERRUPT_REG_OFFSET1               (0x01300000)
/*****************/
/* Error Group 1 */
/*****************/

/* Control Registers */
#define MAC_ERROR_INTERRUPT_1_OFFSET                (MAC_INTERRUPT_REG_OFFSET+ 0x0000400C)
#define MAC_ERROR_INTERRUPT_1_MASK_REG              MAC_HOST_INTERRUPT_MASK_REG(MAC_ERROR_INTERRUPT_1_OFFSET)
#define MAC_ERROR_INTERRUPT_1_PREMASK_REG           MAC_HOST_INTERRUPT_PREMASK_REG(MAC_ERROR_INTERRUPT_1_OFFSET)
#define MAC_ERROR_INTERRUPT_1_STATUS_REG            MAC_HOST_INTERRUPT_STATUS_REG(MAC_ERROR_INTERRUPT_1_OFFSET)

/* bit mapping */

#define MAC_ERR1_INT_SER_AF_FULL1                   31
#define MAC_ERR1_INT_SER_AF_FULL2                   30
#define MAC_ERR1_INT_SER_AF_FULL3                   29
#define MAC_ERR1_INT_SER_AF_FULL4                   28
#define MAC_ERR1_INT_FLTR_UCD_US4                   27
#define MAC_ERR1_INT_FLTR_UCD_US3                   26
#define MAC_ERR1_INT_FLTR_UCD_US2                   25
#define MAC_ERR1_INT_FLTR_UCD_US1                   24
#define MAC_ERR1_INT_TC_ERR_DS8                     23
#define MAC_ERR1_INT_TC_ERR_DS7                     22
#define MAC_ERR1_INT_TC_ERR_DS6                     21
#define MAC_ERR1_INT_TC_ERR_DS5                     20
#define MAC_ERR1_INT_TC_ERR_DS4                     19
#define MAC_ERR1_INT_TC_ERR_DS3                     18
#define MAC_ERR1_INT_TC_ERR_DS2                     17
#define MAC_ERR1_INT_TC_ERR_DS1                     16
#define MAC_ERR1_INT_HCS_ERR_DS8                    15
#define MAC_ERR1_INT_HCS_ERR_DS7                    14
#define MAC_ERR1_INT_HCS_ERR_DS6                    13
#define MAC_ERR1_INT_HCS_ERR_DS5                    12
#define MAC_ERR1_INT_HCS_ERR_DS4                    11
#define MAC_ERR1_INT_HCS_ERR_DS3                    10
#define MAC_ERR1_INT_HCS_ERR_DS2                    9
#define MAC_ERR1_INT_HCS_ERR_DS1                    8
#define MAC_ERR1_INT_PTR_ERR_DS8                    7
#define MAC_ERR1_INT_PTR_ERR_DS7                    6
#define MAC_ERR1_INT_PTR_ERR_DS6                    5
#define MAC_ERR1_INT_PTR_ERR_DS5                    4
#define MAC_ERR1_INT_PTR_ERR_DS4                    3
#define MAC_ERR1_INT_PTR_ERR_DS3                    2
#define MAC_ERR1_INT_PTR_ERR_DS2                    1
#define MAC_ERR1_INT_PTR_ERR_DS1                    0


#define MAC_ERROR_INTERRUPT_1A_OFFSET                (MAC_INTERRUPT_REG_OFFSET+ 0x0000480C)
#define MAC_ERROR_INTERRUPT_1A_MASK_REG              MAC_HOST_INTERRUPT_MASK_REG(MAC_ERROR_INTERRUPT_1A_OFFSET)
#define MAC_ERROR_INTERRUPT_1A_PREMASK_REG           MAC_HOST_INTERRUPT_PREMASK_REG(MAC_ERROR_INTERRUPT_1A_OFFSET)
#define MAC_ERROR_INTERRUPT_1A_STATUS_REG            MAC_HOST_INTERRUPT_STATUS_REG(MAC_ERROR_INTERRUPT_1A_OFFSET)

/* bit mapping */

#define MAC_ERR1_INT_SER_AF_FULL5                    31
#define MAC_ERR1_INT_SER_AF_FULL6                    30
#define MAC_ERR1_INT_SER_AF_FULL7                    29
#define MAC_ERR1_INT_SER_AF_FULL8                    28
#define MAC_ERR1_INT_FLTR_UCD_US8                    27
#define MAC_ERR1_INT_FLTR_UCD_US7                    26
#define MAC_ERR1_INT_FLTR_UCD_US6                    25
#define MAC_ERR1_INT_FLTR_UCD_US5                    24
#define MAC_ERR1_INT_TC_ERR_DS16                     23
#define MAC_ERR1_INT_TC_ERR_DS15                     22
#define MAC_ERR1_INT_TC_ERR_DS14                     21
#define MAC_ERR1_INT_TC_ERR_DS13                     20
#define MAC_ERR1_INT_TC_ERR_DS12                     19
#define MAC_ERR1_INT_TC_ERR_DS11                     18
#define MAC_ERR1_INT_TC_ERR_DS10                     17
#define MAC_ERR1_INT_TC_ERR_DS9                      16
#define MAC_ERR1_INT_HCS_ERR_DS16                    15
#define MAC_ERR1_INT_HCS_ERR_DS15                    14
#define MAC_ERR1_INT_HCS_ERR_DS14                    13
#define MAC_ERR1_INT_HCS_ERR_DS13                    12
#define MAC_ERR1_INT_HCS_ERR_DS12                    11
#define MAC_ERR1_INT_HCS_ERR_DS11                    10
#define MAC_ERR1_INT_HCS_ERR_DS10                    9
#define MAC_ERR1_INT_HCS_ERR_DS9                     8
#define MAC_ERR1_INT_PTR_ERR_DS16                    7
#define MAC_ERR1_INT_PTR_ERR_DS15                    6
#define MAC_ERR1_INT_PTR_ERR_DS14                    5
#define MAC_ERR1_INT_PTR_ERR_DS13                    4
#define MAC_ERR1_INT_PTR_ERR_DS12                    3
#define MAC_ERR1_INT_PTR_ERR_DS11                    2
#define MAC_ERR1_INT_PTR_ERR_DS10                    1
#define MAC_ERR1_INT_PTR_ERR_DS9                     0


#define MAC_ERROR_INTERRUPT_1B_OFFSET                (MAC_INTERRUPT_REG_OFFSET+ 0x0000490C)
#define MAC_ERROR_INTERRUPT_1B_MASK_REG              MAC_HOST_INTERRUPT_MASK_REG(MAC_ERROR_INTERRUPT_1B_OFFSET)
#define MAC_ERROR_INTERRUPT_1B_PREMASK_REG           MAC_HOST_INTERRUPT_PREMASK_REG(MAC_ERROR_INTERRUPT_1B_OFFSET)
#define MAC_ERROR_INTERRUPT_1B_STATUS_REG            MAC_HOST_INTERRUPT_STATUS_REG(MAC_ERROR_INTERRUPT_1B_OFFSET)

/* bit mapping */

#define MAC_ERR1_INT_TC_ERR_DS24                     23
#define MAC_ERR1_INT_TC_ERR_DS23                     22
#define MAC_ERR1_INT_TC_ERR_DS22                     21
#define MAC_ERR1_INT_TC_ERR_DS21                     20
#define MAC_ERR1_INT_TC_ERR_DS20                     19
#define MAC_ERR1_INT_TC_ERR_DS19                     18
#define MAC_ERR1_INT_TC_ERR_DS18                     17
#define MAC_ERR1_INT_TC_ERR_DS17                     16
#define MAC_ERR1_INT_HCS_ERR_DS24                    15
#define MAC_ERR1_INT_HCS_ERR_DS23                    14
#define MAC_ERR1_INT_HCS_ERR_DS22                    13
#define MAC_ERR1_INT_HCS_ERR_DS21                    12
#define MAC_ERR1_INT_HCS_ERR_DS20                    11
#define MAC_ERR1_INT_HCS_ERR_DS19                    10
#define MAC_ERR1_INT_HCS_ERR_DS18                    9
#define MAC_ERR1_INT_HCS_ERR_DS17                    8
#define MAC_ERR1_INT_PTR_ERR_DS24                    7
#define MAC_ERR1_INT_PTR_ERR_DS23                    6
#define MAC_ERR1_INT_PTR_ERR_DS22                    5
#define MAC_ERR1_INT_PTR_ERR_DS21                    4
#define MAC_ERR1_INT_PTR_ERR_DS20                    3
#define MAC_ERR1_INT_PTR_ERR_DS19                    2
#define MAC_ERR1_INT_PTR_ERR_DS18                    1
#define MAC_ERR1_INT_PTR_ERR_DS17                    0



#define MAC_ERROR_INTERRUPT_1C_OFFSET                (MAC_INTERRUPT_REG_OFFSET+ 0x00004A0C)
#define MAC_ERROR_INTERRUPT_1C_MASK_REG              MAC_HOST_INTERRUPT_MASK_REG(MAC_ERROR_INTERRUPT_1C_OFFSET)
#define MAC_ERROR_INTERRUPT_1C_PREMASK_REG           MAC_HOST_INTERRUPT_PREMASK_REG(MAC_ERROR_INTERRUPT_1C_OFFSET)
#define MAC_ERROR_INTERRUPT_1C_STATUS_REG            MAC_HOST_INTERRUPT_STATUS_REG(MAC_ERROR_INTERRUPT_1C_OFFSET)

/* bit mapping */

#define MAC_ERR1_INT_TC_ERR_DS32                     23
#define MAC_ERR1_INT_TC_ERR_DS31                     22
#define MAC_ERR1_INT_TC_ERR_DS30                     21
#define MAC_ERR1_INT_TC_ERR_DS29                     20
#define MAC_ERR1_INT_TC_ERR_DS28                     19
#define MAC_ERR1_INT_TC_ERR_DS27                     18
#define MAC_ERR1_INT_TC_ERR_DS26                     17
#define MAC_ERR1_INT_TC_ERR_DS25                     16
#define MAC_ERR1_INT_HCS_ERR_DS32                    15
#define MAC_ERR1_INT_HCS_ERR_DS31                    14
#define MAC_ERR1_INT_HCS_ERR_DS30                    13
#define MAC_ERR1_INT_HCS_ERR_DS29                    12
#define MAC_ERR1_INT_HCS_ERR_DS28                    11
#define MAC_ERR1_INT_HCS_ERR_DS27                    10
#define MAC_ERR1_INT_HCS_ERR_DS26                    9
#define MAC_ERR1_INT_HCS_ERR_DS25                    8
#define MAC_ERR1_INT_PTR_ERR_DS32                    7
#define MAC_ERR1_INT_PTR_ERR_DS31                    6
#define MAC_ERR1_INT_PTR_ERR_DS30                    5
#define MAC_ERR1_INT_PTR_ERR_DS29                    4
#define MAC_ERR1_INT_PTR_ERR_DS28                    3
#define MAC_ERR1_INT_PTR_ERR_DS27                    2
#define MAC_ERR1_INT_PTR_ERR_DS26                    1
#define MAC_ERR1_INT_PTR_ERR_DS25                    0



/*****************/
/* Error Group 2 */
/*****************/

/* Control Registers */
#define MAC_ERROR_INTERRUPT_2_OFFSET                (MAC_INTERRUPT_REG_OFFSET+ 0x0000402C)
#define MAC_ERROR_INTERRUPT_2_MASK_REG              MAC_HOST_INTERRUPT_MASK_REG(MAC_ERROR_INTERRUPT_2_OFFSET)
#define MAC_ERROR_INTERRUPT_2_PREMASK_REG           MAC_HOST_INTERRUPT_PREMASK_REG(MAC_ERROR_INTERRUPT_2_OFFSET)
#define MAC_ERROR_INTERRUPT_2_STATUS_REG            MAC_HOST_INTERRUPT_STATUS_REG(MAC_ERROR_INTERRUPT_2_OFFSET)

/* bit mapping */
#define MAC_ERR2_INT_FIFO_EMPTY_US1                 31
#define MAC_ERR2_INT_FIFO_EMPTY_US2                 30
#define MAC_ERR2_INT_FIFO_EMPTY_US3                 29
#define MAC_ERR2_INT_FIFO_EMPTY_US4                 28
#define MAC_ERR2_INT_FIFO_FULL_US1                  27
#define MAC_ERR2_INT_FIFO_FULL_US2                  26
#define MAC_ERR2_INT_FIFO_FULL_US3                  25
#define MAC_ERR2_INT_FIFO_FULL_US4                  24
#define MAC_ERR2_INT_TX_ERR_US1                     23
#define MAC_ERR2_INT_TX_ERR_US2                     22
#define MAC_ERR2_INT_TX_ERR_US3                     21
#define MAC_ERR2_INT_TX_ERR_US4                     20
#define MAC_ERR2_INT_PHY_ERR_US1                    19
#define MAC_ERR2_INT_PHY_ERR_US2                    18
#define MAC_ERR2_INT_PHY_ERR_US3                    17
#define MAC_ERR2_INT_PHY_ERR_US4                    16
#define MAC_ERR2_INT_PRIMARY_DS_QAM_LOST            15
#define MAC_ERR2_INT_CURRUPT_MAP_US1                14
#define MAC_ERR2_INT_CURRUPT_MAP_US2                13
#define MAC_ERR2_INT_CURRUPT_MAP_US3                12
#define MAC_ERR2_INT_CURRUPT_MAP_US4                11
#define MAC_ERR2_INT_CRC_ERR_US1                    10
#define MAC_ERR2_INT_CRC_ERR_US2                    9
#define MAC_ERR2_INT_CRC_ERR_US3                    8
#define MAC_ERR2_INT_CRC_ERR_US4                    7
#define MAC_ERR2_INT_CRC_ERR_SYNC                   6
#define MAC_ERR2_INT_MAP_LOST_US1                   5
#define MAC_ERR2_INT_MAP_LOST_US2                   4
#define MAC_ERR2_INT_MAP_LOST_US3                   3
#define MAC_ERR2_INT_MAP_LOST_US4                   2
#define MAC_ERR2_INT_SYNC_LOST                      1
#define MAC_ERR2_INT_SYNC_THRESHOLD                 0


#define MAC_ERROR_INTERRUPT_2A_OFFSET                (MAC_INTERRUPT_REG_OFFSET+ 0x0000482C)
#define MAC_ERROR_INTERRUPT_2A_MASK_REG              MAC_HOST_INTERRUPT_MASK_REG(MAC_ERROR_INTERRUPT_2A_OFFSET)
#define MAC_ERROR_INTERRUPT_2A_PREMASK_REG           MAC_HOST_INTERRUPT_PREMASK_REG(MAC_ERROR_INTERRUPT_2A_OFFSET)
#define MAC_ERROR_INTERRUPT_2A_STATUS_REG            MAC_HOST_INTERRUPT_STATUS_REG(MAC_ERROR_INTERRUPT_2A_OFFSET)

/* bit mapping */
#define MAC_ERR2_INT_FIFO_EMPTY_US5                 31
#define MAC_ERR2_INT_FIFO_EMPTY_US6                 30
#define MAC_ERR2_INT_FIFO_EMPTY_US7                 29
#define MAC_ERR2_INT_FIFO_EMPTY_US8                 28
#define MAC_ERR2_INT_FIFO_FULL_US5                  27
#define MAC_ERR2_INT_FIFO_FULL_US6                  26
#define MAC_ERR2_INT_FIFO_FULL_US7                  25
#define MAC_ERR2_INT_FIFO_FULL_US8                  24
#define MAC_ERR2_INT_TX_ERR_US5                     23
#define MAC_ERR2_INT_TX_ERR_US6                     22
#define MAC_ERR2_INT_TX_ERR_US7                     21
#define MAC_ERR2_INT_TX_ERR_US8                     20
#define MAC_ERR2_INT_PHY_ERR_US5                    19
#define MAC_ERR2_INT_PHY_ERR_US6                    18
#define MAC_ERR2_INT_PHY_ERR_US7                    17
#define MAC_ERR2_INT_PHY_ERR_US8                    16
#define MAC_ERR2_INT_CURRUPT_MAP_US5                14
#define MAC_ERR2_INT_CURRUPT_MAP_US6                13
#define MAC_ERR2_INT_CURRUPT_MAP_US7                12
#define MAC_ERR2_INT_CURRUPT_MAP_US8                11
#define MAC_ERR2_INT_CRC_ERR_US5                    10
#define MAC_ERR2_INT_CRC_ERR_US6                    9
#define MAC_ERR2_INT_CRC_ERR_US7                    8
#define MAC_ERR2_INT_CRC_ERR_US8                    7
#define MAC_ERR2_INT_MAP_LOST_US5                   5
#define MAC_ERR2_INT_MAP_LOST_US6                   4
#define MAC_ERR2_INT_MAP_LOST_US7                   3
#define MAC_ERR2_INT_MAP_LOST_US8                   2



/*****************/
/* Error Group 3 */
/*****************/

/* Control Registers */
#define MAC_ERROR_INTERRUPT_3_OFFSET                (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C00C)
#define MAC_ERROR_INTERRUPT_3_MASK_REG              MAC_HOST_INTERRUPT_MASK_REG(MAC_ERROR_INTERRUPT_3_OFFSET)
#define MAC_ERROR_INTERRUPT_3_PREMASK_REG           MAC_HOST_INTERRUPT_PREMASK_REG(MAC_ERROR_INTERRUPT_3_OFFSET)
#define MAC_ERROR_INTERRUPT_3_STATUS_REG            MAC_HOST_INTERRUPT_STATUS_REG(MAC_ERROR_INTERRUPT_3_OFFSET)

/* bit mapping */
#define MAC_ERR3_INT_CNCT_SIZE_US5                  30
#define MAC_ERR3_INT_CNCT_SIZE_US6                  29
#define MAC_ERR3_INT_CNCT_SIZE_US7                  28
#define MAC_ERR3_INT_CNCT_SIZE_US8                  27
#define MAC_ERR3_INT_MP_ORD_FIFO_FULL               26
#define MAC_ERR3_INT_CNCT_SIZE_US1                  25
#define MAC_ERR3_INT_CNCT_SIZE_US2                  24
#define MAC_ERR3_INT_CNCT_SIZE_US3                  23
#define MAC_ERR3_INT_CNCT_SIZE_US4                  22
#define MAC_ERR3_INT_BP_DS_OFFSET                   21
#define MAC_ERR3_INT_CRC_DS                         20
#define MAC_ERR3_INT_PHS_DS_OFFSET                  19
#define MAC_ERR3_INT_CP_DS_CNCT_SIZE                18
#define MAC_ERR3_INT_BP_US_OFFSET                   17
#define MAC_ERR3_INT_CP_US_CNCT_SIZE                16
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS1              15
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS2              14
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS3              13
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS4              12
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS5              11
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS6              10
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS7              9
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS8              8
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS9              7
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS10             6
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS11             5
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS12             4
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS13             3
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS14             2
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS15             1
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS16             0


#define MAC_ERROR_INTERRUPT_3A_OFFSET                (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C80C)
#define MAC_ERROR_INTERRUPT_3A_MASK_REG              MAC_HOST_INTERRUPT_MASK_REG(MAC_ERROR_INTERRUPT_3A_OFFSET)
#define MAC_ERROR_INTERRUPT_3A_PREMASK_REG           MAC_HOST_INTERRUPT_PREMASK_REG(MAC_ERROR_INTERRUPT_3A_OFFSET)
#define MAC_ERROR_INTERRUPT_3A_STATUS_REG            MAC_HOST_INTERRUPT_STATUS_REG(MAC_ERROR_INTERRUPT_3A_OFFSET)

/* bit mapping */

#define MAC_ERR3_INT_IFIFO_OVERRUN_DS17              15
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS18              14
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS19              13
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS20              12
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS21              11
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS22              10
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS23              9
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS24              8
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS25              7
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS26              6
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS27              5
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS28              4
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS29              3
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS30              2
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS31              1
#define MAC_ERR3_INT_IFIFO_OVERRUN_DS32              0




/*****************/
/* Error Group 4 */
/*****************/

/* Control Registers */
#define MAC_ERROR_INTERRUPT_4_OFFSET                (MAC_INTERRUPT_REG_OFFSET+ 0x0000C02C)
#define MAC_ERROR_INTERRUPT_4_MASK_REG              MAC_HOST_INTERRUPT_MASK_REG(MAC_ERROR_INTERRUPT_4_OFFSET)
#define MAC_ERROR_INTERRUPT_4_PREMASK_REG           MAC_HOST_INTERRUPT_PREMASK_REG(MAC_ERROR_INTERRUPT_4_OFFSET)
#define MAC_ERROR_INTERRUPT_4_STATUS_REG            MAC_HOST_INTERRUPT_STATUS_REG(MAC_ERROR_INTERRUPT_4_OFFSET)

/* bit mapping */
#define MAC_ERR4_INT_DSG2_SOP_BUFFER_OVERRUN_RX         7
#define MAC_ERR4_INT_DSG2_SOP_DESC_OVERRUN_RX           6
#define MAC_ERR4_INT_DSG2_MOP_BUFFER_OVERRUN_RX         5
#define MAC_ERR4_INT_DSG2_MOP_DESC_OVERRUN_RX           4
#define MAC_ERR4_INT_DSG1_SOP_BUFFER_OVERRUN_RX         3
#define MAC_ERR4_INT_DSG1_SOP_DESC_OVERRUN_RX           2
#define MAC_ERR4_INT_DSG1_MOP_BUFFER_OVERRUN_RX         1
#define MAC_ERR4_INT_DSG1_MOP_DESC_OVERRUN_RX           0




/**************************************/
/*  MAC UCD-Change Interrupt Register */
/**************************************/
#define MASK_UCD_CHANGE_GROUP_NEW_UCD_INTERRUPT     (0XF0F0)
/* Control Registers */
#define MAC_UCD_CHANGE_INTERRUPT_OFFSET             (MAC_INTERRUPT_REG_OFFSET+ 0x0000404C)
#define MAC_UCD_CHANGE_INTERRUPT_MASK_REG           MAC_HOST_INTERRUPT_MASK_REG(MAC_UCD_CHANGE_INTERRUPT_OFFSET)
#define MAC_UCD_CHANGE_INTERRUPT_PREMASK_REG        MAC_HOST_INTERRUPT_PREMASK_REG(MAC_UCD_CHANGE_INTERRUPT_OFFSET)
#define MAC_UCD_CHANGE_INTERRUPT_STATUS_REG         MAC_HOST_INTERRUPT_STATUS_REG(MAC_UCD_CHANGE_INTERRUPT_OFFSET)

/* bit mapping */
#define MAC_UCD_INT_NEW_UCD_US8                     15
#define MAC_UCD_INT_NEW_UCD_US7                     14
#define MAC_UCD_INT_NEW_UCD_US6                     13
#define MAC_UCD_INT_NEW_UCD_US5                     12
#define MAC_UCD_INT_NEW_MAP_ACTIVE_US8              11
#define MAC_UCD_INT_NEW_MAP_ACTIVE_US7              10
#define MAC_UCD_INT_NEW_MAP_ACTIVE_US6              9
#define MAC_UCD_INT_NEW_MAP_ACTIVE_US5              8
#define MAC_UCD_INT_NEW_UCD_US4                     7
#define MAC_UCD_INT_NEW_UCD_US3                     6
#define MAC_UCD_INT_NEW_UCD_US2                     5
#define MAC_UCD_INT_NEW_UCD_US1                     4
#define MAC_UCD_INT_NEW_MAP_ACTIVE_US4              3
#define MAC_UCD_INT_NEW_MAP_ACTIVE_US3              2
#define MAC_UCD_INT_NEW_MAP_ACTIVE_US2              1
#define MAC_UCD_INT_NEW_MAP_ACTIVE_US1              0



/*********************************/
/*  MAC US FW Interrupt Register */
/*********************************/

/* Control Registers */
#define MAC_US_FW_INTERRUPT_ENABLE_REG             (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C3C0)
#define MAC_US_FW_INTERRUPT_OFFSET                 (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C3CC)
#define MAC_US_FW_INTERRUPT_MASK_REG                MAC_HOST_INTERRUPT_MASK_REG(MAC_US_FW_INTERRUPT_OFFSET)
#define MAC_US_FW_INTERRUPT_PREMASK_REG             MAC_HOST_INTERRUPT_PREMASK_REG(MAC_US_FW_INTERRUPT_OFFSET)
#define MAC_US_FW_INTERRUPT_STATUS_REG              MAC_HOST_INTERRUPT_STATUS_REG(MAC_US_FW_INTERRUPT_OFFSET)

/* bit mapping */
#define MAC_US_FW_INT_EVENT_NEW                     2
#define MAC_US_FW_INT_DBG_BUFFER_NEW                1
#define MAC_US_FW_INT_CMD_COMPLETE                  0
#define MAC_US_FW_INT_RNG_REQ_SENT_0                8
#define MAC_US_FW_INT_RNG_REQ_SENT_1                9
#define MAC_US_FW_INT_RNG_REQ_SENT_2                10
#define MAC_US_FW_INT_RNG_REQ_SENT_3                11
#define MAC_US_FW_INT_RNG_REQ_SENT_4                12
#define MAC_US_FW_INT_RNG_REQ_SENT_5                13
#define MAC_US_FW_INT_RNG_REQ_SENT_6                14
#define MAC_US_FW_INT_RNG_REQ_SENT_7                15

#define MAC_US_FW_INT_BCST_RNG_OPPORTUNITY_CH0      16
#define MAC_US_FW_INT_BCST_RNG_OPPORTUNITY_CH1      17
#define MAC_US_FW_INT_BCST_RNG_OPPORTUNITY_CH2      18
#define MAC_US_FW_INT_BCST_RNG_OPPORTUNITY_CH3      19
#define MAC_US_FW_INT_BCST_RNG_OPPORTUNITY_CH4      20
#define MAC_US_FW_INT_BCST_RNG_OPPORTUNITY_CH5      21
#define MAC_US_FW_INT_BCST_RNG_OPPORTUNITY_CH6      22
#define MAC_US_FW_INT_BCST_RNG_OPPORTUNITY_CH7      23

#define MAC_US_FW_INT_PHY_FEC_LOST_LOCK             30
#define MAC_US_FW_INT_MBOX_READY                    31

/*********************************/
/*  MAC DS FW Interrupt Register */
/*********************************/

/* Control Registers */
#define MAC_DS_FW_INTERRUPT_ENABLE_REG             (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C3E0)
#define MAC_DS_FW_INTERRUPT_SWINT_REG              (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C3E4)
#define MAC_DS_FW_INTERRUPT_EDGE_LEVEL_REG         (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C3E8)
#define MAC_DS_FW_INTERRUPT_OFFSET                 (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C3EC)
#define MAC_DS_FW_INTERRUPT_MASK_REG                MAC_HOST_INTERRUPT_MASK_REG(MAC_DS_FW_INTERRUPT_OFFSET)
#define MAC_DS_FW_INTERRUPT_PREMASK_REG             MAC_HOST_INTERRUPT_PREMASK_REG(MAC_DS_FW_INTERRUPT_OFFSET)
#define MAC_DS_FW_INTERRUPT_STATUS_REG              MAC_HOST_INTERRUPT_STATUS_REG(MAC_DS_FW_INTERRUPT_OFFSET)

/* bit mapping */

#define MAC_DS_FW_INT_LAST                          31
#define MAC_DS_FW_PDSP2_SEQ_OUT_OF_RNG_ERROR        25
#define MAC_DS_FW_PDSP1_KEY_SEQ_ERROR               24
#define MAC_DS_FW_INT_MDD_ARRIVED_DS24              23
#define MAC_DS_FW_INT_MDD_ARRIVED_DS23              22
#define MAC_DS_FW_INT_MDD_ARRIVED_DS22              21
#define MAC_DS_FW_INT_MDD_ARRIVED_DS21              20
#define MAC_DS_FW_INT_MDD_ARRIVED_DS20              19
#define MAC_DS_FW_INT_MDD_ARRIVED_DS19              18
#define MAC_DS_FW_INT_MDD_ARRIVED_DS18              17
#define MAC_DS_FW_INT_MDD_ARRIVED_DS17              16
#define MAC_DS_FW_INT_MDD_ARRIVED_DS16              15
#define MAC_DS_FW_INT_MDD_ARRIVED_DS15              14
#define MAC_DS_FW_INT_MDD_ARRIVED_DS14              13
#define MAC_DS_FW_INT_MDD_ARRIVED_DS13              12
#define MAC_DS_FW_INT_MDD_ARRIVED_DS12              11
#define MAC_DS_FW_INT_MDD_ARRIVED_DS11              10
#define MAC_DS_FW_INT_MDD_ARRIVED_DS10              9
#define MAC_DS_FW_INT_MDD_ARRIVED_DS9               8
#define MAC_DS_FW_INT_MDD_ARRIVED_DS8               7
#define MAC_DS_FW_INT_MDD_ARRIVED_DS7               6
#define MAC_DS_FW_INT_MDD_ARRIVED_DS6               5
#define MAC_DS_FW_INT_MDD_ARRIVED_DS5               4
#define MAC_DS_FW_INT_MDD_ARRIVED_DS4               3
#define MAC_DS_FW_INT_MDD_ARRIVED_DS3               2
#define MAC_DS_FW_INT_MDD_ARRIVED_DS2               1
#define MAC_DS_FW_INT_MDD_ARRIVED_DS1               0
#define MAC_DS_FW_INT_MDD_ARRIVED_DS(ch)            ((ch))  /* ch - DS channel: 0,1,2,3, ... */


/* Control Registers */
#define MAC_DS_FW_INTERRUPT2_ENABLE_REG             (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C440)
#define MAC_DS_FW_INTERRUPT2_SWINT_REG              (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C444)
#define MAC_DS_FW_INTERRUPT2_EDGE_LEVEL_REG         (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C448)
#define MAC_DS_FW_INTERRUPT2_OFFSET                 (MAC_INTERRUPT_REG_OFFSET1+ 0x0000C44C)
#define MAC_DS_FW_INTERRUPT2_MASK_REG                MAC_HOST_INTERRUPT_MASK_REG(MAC_DS_FW_INTERRUPT2_OFFSET)
#define MAC_DS_FW_INTERRUPT2_PREMASK_REG             MAC_HOST_INTERRUPT_PREMASK_REG(MAC_DS_FW_INTERRUPT2_OFFSET)
#define MAC_DS_FW_INTERRUPT2_STATUS_REG              MAC_HOST_INTERRUPT_STATUS_REG(MAC_DS_FW_INTERRUPT2_OFFSET)

#define MAC_DS_FW_INT2_MDD_ARRIVED_DS32              31
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS31              30
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS30              29
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS29              28
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS28              27
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS27              26
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS26              25
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS25              24
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS24              23
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS23              22
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS22              21
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS21              20
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS20              19
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS19              18
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS18              17
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS17              16
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS16              15
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS15              14
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS14              13
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS13              12
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS12              11
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS11              10
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS10              9
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS9               8
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS8               7
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS7               6
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS6               5
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS5               4
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS4               3
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS3               2
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS2               1
#define MAC_DS_FW_INT2_MDD_ARRIVED_DS1               0


/**************************************************************************/
/*      INTERFACE VARIABLES (prefix with EXTERN)                          */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

#endif


