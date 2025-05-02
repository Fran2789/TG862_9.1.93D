
/*
 *  arm_atom_mbx.h
 *
 *  GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2012 Intel Corporation. All rights reserved.
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *    Intel Corporation
 *    2200 Mission College Blvd.
 *    Santa Clara, CA  97052
 *
 */
#include <puma6_hw_mutex.h>

enum arm11_mbx_event_id
{
    ARM11_EVENT_GPIO_INIT_EXIT   = 0x0001,
    ARM11_EVENT_SPI_INIT_EXIT    = 0x0002,
    ARM11_EVENT_EMMC_INIT_EXIT   = 0x0004, 
    ARM11_EVENT_IPV4_OBTAIN_ADDR = 0x0008, 
    ARM11_EVENT_IPV6_OBTAIN_ADDR = 0x00c0 

};

enum atom_mbx_event_id
{
    ATOM_EVENT_RSVD                = 0x0001,
    ATOM_EVENT_SPI_ADVANCE_EXIT    = 0x0002,
    ATOM_EVENT_EMMC_ADVANCE_EXIT   = 0x0004, 
    ATOM_EVENT_IPV4_OBTAIN_ADDR    = 0x0008, 
    ATOM_EVENT_IPV6_OBTAIN_ADDR    = 0x00c0 

};


#define AVALANCHE_SRAM_BASE                 (0x00000000)
#define ARM_MBX_STRUCT_BASE_ADDR            (AVALANCHE_SRAM_BASE + 0x00001E00)
#define ATOM_MBX_STRUCT_BASE_ADDR           (AVALANCHE_SRAM_BASE + 0x00001E04)

#define ARM_MBX_STRUCT_EVENT_MASK_ADDR      (ARM_MBX_STRUCT_BASE_ADDR)
#define ARM_MBX_STRUCT_ACK_MASK_ADDR        (ARM_MBX_STRUCT_BASE_ADDR + 2)

#define ATOM_MBX_STRUCT_EVENT_MASK_ADDR     (ATOM_MBX_STRUCT_BASE_ADDR)
#define ATOM_MBX_STRUCT_ACK_MASK_ADDR       (ATOM_MBX_STRUCT_BASE_ADDR + 2)

/****************************************************************************/
/****************************************************************************/
/*                  ATOM-ARM11 MBOX Helper macros                           */
/****************************************************************************/
/****************************************************************************/

/* Helper function for 32 bit endian swapping */
#define ENDIAN_SWAP32(x) ({ __typeof__ (x) val = (x); \
                          (((val & 0x000000FF)<<24) | ((val & 0x0000FF00)<<8) | ((val & 0x00FF0000)>>8)  | ((val & 0xFF000000)>>24)); \
                         })

/* Helper function for 16 bit endian swapping */
#define ENDIAN_SWAP16(x) ({ __typeof__ (x) val = (x); \
                          ((((x) >> 8) & 0x00FF) | (((x) << 8) & 0xFF00)); \
                          })

/* Macro to read/write MBOX register from SRAM */
#define PUMA6_MBOX_REG32_GET(reg)                   (*((volatile unsigned int *)(reg)))
#define PUMA6_MBOX_REG32_SET(reg, val)              ((*((volatile unsigned int *)(reg))) = (val))
#define PUMA6_MBOX_REG16_GET(reg)                   (*((volatile unsigned short *)(reg)))
#define PUMA6_MBOX_REG16_SET(reg, val)              ((*((volatile unsigned short *)(reg))) = (val))

/* This Macro will help to set ARM11 MBOX event reg, the eventID will be swap to LE */
#define PUMA6_ARM11_MBOX_SET_EVENT(eventID)   \
        (PUMA6_MBOX_REG16_SET(ARM_MBX_STRUCT_EVENT_MASK_ADDR, (PUMA6_MBOX_REG16_GET(ARM_MBX_STRUCT_EVENT_MASK_ADDR) | ENDIAN_SWAP16(eventID)))) 

/* To update ARM11 MBOX event please use this Macro only */
#define PUMA6_ARM11_MBOX_UPDATE_EVENT(eventID)  \
do {                                            \
        hw_mutex_lock(HW_MUTEX_ARM_MBX);        \
        PUMA6_ARM11_MBOX_SET_EVENT(eventID);    \
        hw_mutex_unlock(HW_MUTEX_ARM_MBX);      \
} while(0)
