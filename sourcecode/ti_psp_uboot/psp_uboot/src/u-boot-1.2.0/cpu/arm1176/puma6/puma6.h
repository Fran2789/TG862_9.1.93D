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


#ifndef _INCLUDE_PUMA6_H
#define _INCLUDE_PUMA6_H


 
/* UART Base address */
//#define UART0_REGS_BASE                   (0x00050000)
//#define UART1_REGS_BASE                   (0x00060000)
//#define UART2_REGS_BASE                   (0x00070000)

/* Boot And Config Modlue */
#define BOOTCONFIG_BASE         (0x000C0000)
#define BOOTCFG_DOCSIS_IP_IO_ENABLE_REG     (BOOTCONFIG_BASE + 0x144)
#define DOCSIS_IP_IO_ENABLE_BBU_MASK        (0x00000001)
#define DOCSIS_IP_IO_ENABLE_UART0_MASK      (0x00000040)
#define DOCSIS_IP_IO_ENABLE_UART1_MASK      (0x00000080)
#define DOCSIS_IP_IO_ENABLE_UART2_MASK      (0x00000100)

/* Watchdog Timer */
/* Watchdog Base Address */
#define WDT_BASE                            (0x00010000)
#define REG_WT_KICK_LOCK                    (WDT_BASE + 0)
#define REG_WT_KICK                         (WDT_BASE + 0x04)
#define REG_WT_CHNG_LOCK                    (WDT_BASE + 0x08)
#define REG_WT_CHNG                         (WDT_BASE + 0x0C)
#define REG_WT_DISABLE_LOCK                 (WDT_BASE + 0x10)
#define REG_WT_DISABLE                      (WDT_BASE + 0x14)

#define WT_CHNG_UNLOCK_WORD1                (0x6666)
#define WT_CHNG_UNLOCK_WORD2                (0xBBBB)
#define WT_DISABLE_UNLOCK_WORD1             (0x7777)
#define WT_DISABLE_UNLOCK_WORD2             (0xCCCC)
#define WT_DISABLE_UNLOCK_WORD3             (0xDDDD)

#define WT_STATE_DISABLED                   (0)
#define WT_STATE_ENABLED                    (1)

#endif /* !_INCLUDE_PUMA6_H */
