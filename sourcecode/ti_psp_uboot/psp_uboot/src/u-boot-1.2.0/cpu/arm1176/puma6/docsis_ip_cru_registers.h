/*
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2011 Intel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify 
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *  General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License 
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution 
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *  Intel Corporation
 *  2200 Mission College Blvd.
 *  Santa Clara, CA  97052
 */



/* CRU - Clock and Reset Unit */
/* Docsis IP has 33 CRUs */
/* Clock Control registers set memory map: */
/* Start address 0x000D_0000 */
/* End address   0x000D_FFFF */

#define CRU_MOD_STATE_BASE    0x000D0000
#define CRU_MOD_STATUS_BASE   0x000D0004
#define CRU_RSTN_CLK_EN_BASE  0x000D0008 /* should not be used, for debug only */

/* CRU_MOD_STATE register fields */
/* 31:2 Reserved (R) */
/* 1:0  MOD_STATE_REG (R/W) */
#define CRU_MOD_STATE_DISABLED      0
#define CRU_MOD_STATE_SYNC_RST      1
#define CRU_MOD_STATE_CLK_DISABLE   2
#define CRU_MOD_STATE_ENABLE        3

/* CRU_MOD_STATUS register fields */
/* 31:10 Reserved (R) */
/* 9:6	CRU_CG_EN_1-4 (R) - Module CG status (4 lines) */
/* 5:2	CRU_RST_N_1-4 (R) - Module resets status (4 lines) */
/* 1:0	CRU_SM_STATE (R) - Module State Machine State (same as in CRU_MOD_STATE values) */

/* CRU_RSTN_CLK_EN register fields */
/* 31:1 Reserved (R) */
/* 0    CRU_RSTN_CLK_EN_FORCE (R/W) Force module reset and opens the clock gater */

#define CRU_MOD_STATE(cru_num)   *((volatile unsigned int *)(CRU_MOD_STATE_BASE   | ((cru_num)<<4)))
#define CRU_MOD_STATUS(cru_num)  *((volatile unsigned int *)(CRU_MOD_STATUS_BASE  | ((cru_num)<<4)))
#define CRU_RSTN_CLK_EN(cru_num) *((volatile unsigned int *)(CRU_RSTN_CLK_EN_BASE | ((cru_num)<<4)))

#define CRU_NUM_ARM11               0    /* ATOM must enable this module's clocks to take ARM11 out of reset, after it uses DOCSIS_CNTL register to take Docsis IP out of reset */
#define CRU_NUM_C55                 1
#define CRU_NUM_P24                 2
#define CRU_NUM_DOCSIS_MAC0         3
#define CRU_NUM_DOCSIS_MAC1         4
#define CRU_NUM_DOCSIS_PHY0         5
#define CRU_NUM_DOCSIS_PHY1         6
#define CRU_NUM_DOCSIS_PHY2         7
#define CRU_NUM_PKT_PROCESSOR       8
#define CRU_NUM_DOCSIS_IP_INFRA     9     /* This module's clocks are enabled by defualt */
#define CRU_NUM_BBU                 10
#define CRU_NUM_WDT                 11
#define CRU_NUM_RAM                 12    /* This module's clocks are enabled by defualt */
#define CRU_NUM_TIMER0              13
#define CRU_NUM_TIMER1              14
#define CRU_NUM_TIMER2              15
#define CRU_NUM_UART0               16
#define CRU_NUM_UART1               17
#define CRU_NUM_UART2               18
#define CRU_NUM_CPSPDMA0            19
#define CRU_NUM_CPSPDMA1            20
#define CRU_NUM_BOOT_CFG            21    /* This module's clocks are enabled by defualt */
#define CRU_NUM_TDM00               22
#define CRU_NUM_TDM01               23
#define CRU_NUM_TDM10               24
#define CRU_NUM_TDM11               25
#define CRU_NUM_DSP_PROXY           26
#define CRU_NUM_DSP_INC             27
#define CRU_NUM_I2C                 28
#define CRU_NUM_PREF_MON            29
#define CRU_NUM_C55_CLK             30
#define CRU_NUM_NBADC               31
#define CRU_NUM_DAC                 32




