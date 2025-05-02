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
 * docint.h
 * Description:
 * declaration of types used in the interrupt cause-detection module
*/

#ifndef __DOCINT_H_
#define __DOCINT_H_
#include <puma_autoconf.h>
#include "sram_api.h"

extern unsigned int pgaGrtIntNum;
#if PUMA6_OR_NEWER_SOC_TYPE
extern unsigned int siliconStepping;
#endif

#ifndef DI_NAME
typedef enum 
{
    DI_MINOR_START,                 /* First!!! */
    DI_MAC_ERR = DI_MINOR_START,    /* 0 */
    DI_MAC_UCD,                     /* 1 */
    DI_MAC_US_FW,                   /* 2 */
    DI_MAC_DS_FW,                   /* 3 */
    DI_MAC_PHY,                     /* 4 */
    DI_PHY_PGA_GRT,                 /* 5 */
    DI_PHY_MPT,                     /* 6 */
    DI_COUNT                        /* Last!!! */
} DiDevices_e;
#define DI_NAME     "DOCINT"
#endif

#if PUMA6_OR_NEWER_SOC_TYPE
/* MAPPING of the DOCSIS Interrupts to IRQ lines at the SoC Interrupt Controller    */
/* From the Puma6 Device Specification ("Chip-Global" chapter):                     */
/* 20   DMACNWEUCD  Active high level   Docsis MAC  dmac_top_hst_new_ucd_int_sync   */
/* 21   DMACPDSP    Active high level   Docsis MAC  dmac_top_pdsp_int_sync          */
/* 22   DMACARMINT  Active high level   Docsis MAC  dmac_top_arm_int_sync;          */
/* 23   DPHYINT Active high level       Docsis PHY  docsis_phy_interrupt            */
/* 72   DMACHSTERR  Active high level   DMAC    dmac_top_hst_error                  */
#define MAC_ERR_INT_IRQ_NUM                         72
#define MAC_UCD_INT_IRQ_NUM                         20
#define MAC_US_FW_INT_IRQ_NUM                       22
#define MAC_DS_FW_INT_IRQ_NUM                       21
#define PHY_INT_IRQ_NUM                             23
#define PHY_PGA_GRT_IRQ_NUM                         (pgaGrtIntNum)
#define PHY_PGA_GRT_IRQ_NUM_PRE_C0                  63
#define PHY_PGA_GRT_IRQ_NUM_C0                      17
#define PHY_MPT_INT_NUM                             86
#else
/* MAPPING of the DOCSIS Interrupts to IRQ lines at the SoC Interrupt Controller */
/* From the Puma5 Device Specification ("Chip-Global" chapter):                  */
/* 15   DMACERRINT  Active high level   Docsis MAC  docsis_mac_hst_error_int     */
/* 16   DMACUCDINT  Active high level   Docsis MAC  docsis_mac_hst_new_ucd_int   */
/* 17   DMACARMINT  Active high level   Docsis MAC  docsis_arm_interrupt         */
/* 18   DMAPDSPINT  Active high level   Docsis MAC  docsis_pdsp_interrupt        */
/* 19   DPHYINT     Active high level   Docsis PHY  docsis_phy_interrupt         */
/* 43   GRTINT      Active falling Edge Docsis SOC  docsis_pga_grt_interrupt     */
#define MAC_ERR_INT_IRQ_NUM                         15
#define MAC_UCD_INT_IRQ_NUM                         16
#define MAC_US_FW_INT_IRQ_NUM                       17
#define MAC_DS_FW_INT_IRQ_NUM                       18
#define PHY_INT_IRQ_NUM                             19
#define PHY_PGA_GRT_IRQ_NUM                         (pgaGrtIntNum)
#define PHY_PGA_GRT_IRQ_NUM_REV1                    1
#define PHY_PGA_GRT_IRQ_NUM_REV2                    43
#define PHY_MPT_INT_NUM                             1
#endif


/* For the GRT interrupt - there is only one interrupt related to this IRQ, */
/* therefore the status mask will always return that bit 0 is active.       */
#define PHY_PGA_GRT_INT_MASK                       0x1
#define PHY_PGA_GRT_INT_INDEX                      0

/* If working with system REV-1, the interrupt is muxed with GPIO */
#define PHY_PGA_GRT_INT_REV1_GPIO                  27

/* For the MPT interrupt - there is only one interrupt related to this IRQ, */
/* therefore the status mask will always return that bit 0 is active.       */
#define PHY_MPT_INT_MASK                           0x1
#define PHY_MPT_INT_INDEX                          0


typedef struct intinfo
{
    short   ii_inum;
    char    ii_masked;
    char    ii_clear;
} ii_t;

#ifdef __KERNEL__


/* Device structure */
typedef struct docint {
    ii_t                *di_data;
    off_t               di_roffset;
    off_t               di_woffset;
    atomic_t            di_free;    /* buffer free entry count */
    struct semaphore    di_sem;
    wait_queue_head_t   di_rqueue;  /* unsatisfied read from device sleeps on it */
    struct cdev         di_cdev;    /* char device structure */
    int                 ref;
    int                 di_irq;     /*
                                     * Set on first open. Reset on _release
                                     * Thus serving as an indication of the
                                     * device being open
                                     */
} di_t;

#define IMASKED     0x1

typedef struct dihndlr {
    void (*dih_func)(unsigned);
    char dih_masked;        /* the interrupt was masked */
    di_t *dih_dev;          /* pointer to the device structure assigned at init() */
} dih_t;


void docint_default_handler         (unsigned int index);
void docint_mask_interrupt_handler  (unsigned int index);
#if PUMA6_OR_NEWER_SOC_TYPE
void docint_pga_interrupt_handler   (unsigned int index);
#endif

#endif

#endif
