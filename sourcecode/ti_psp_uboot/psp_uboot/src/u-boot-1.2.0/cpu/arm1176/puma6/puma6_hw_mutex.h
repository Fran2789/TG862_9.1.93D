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


/* HW Mutex block is part of the Cat Mountain SoC */
/* Supports up to 4 masters and 12 HW mutexes */

/* Master allocations */
/* Master0 = ARM11 */
/* Master1 = PP */
/* Master2 = ATOM */
/* Master3 = NOT USED? */


#ifndef _PUMA6_HW_MUTEX_H_
#define _PUMA6_HW_MUTEX_H_



/****************************************************************************/
/****************************************************************************/
/*                          HW Mutex Definitions                            */
/****************************************************************************/
/****************************************************************************/

/* HW Mutex CPU/Masters type (we have 4 masters)*/
typedef enum 
{
    MASTER_ARM11    = 0,
    MASTER_PP       = 1,
    MASTER_ATOM     = 2,
    MASTER_RESV
} hw_mutex_master_type;

/* HW Mutex types (we have 12 types) */
typedef enum 
{
    HW_MUTEX_NOR_SPI   = 0,
    HW_MUTEX_EMMC      = 1,
    HW_MUTEX_ARM_MBX   = 2,  /* Mutex for protect the ARM Mbox Structure  */
    HW_MUTEX_ATOM_MBX  = 3,  /* Mutex for protect the ATOM Mbox Structure */
    HW_MUTEX_GPIO      = 4,  /* Mutex for protect the GPIO configuration  */
    HW_MUTEX_5, /* Not in use */
    HW_MUTEX_6, /* Not in use */
    HW_MUTEX_7, /* Not in use */
    HW_MUTEX_8, /* Not in use */
    HW_MUTEX_9, /* Not in use */
    HW_MUTEX_10,/* Not in use */
    HW_MUTEX_11,/* Not in use */
} hw_mutex_device_type;

/* HW Mutex Base Address (For ARM11) */
#define HW_MUTEX_BASE_ADDR                      (0x0FFE1800)


/* HW Mutex memory map (Registers): */

#define HW_MUTEX_ID                             (0x0000)
#define HW_MUTEX_STATUS                         (0x0004)
/* MUTEX wait Registers */                  
#define HW_MUTEX_WAIT_MASTER0                   (0x0008)
#define HW_MUTEX_WAIT_MASTER1                   (0x000C)
#define HW_MUTEX_WAIT_MASTER2                   (0x0010)
#define HW_MUTEX_WAIT_MASTER3                   (0x0014)
/* MUTEX own registers */                   
#define HW_MUTEX_OWN_MASTER0                    (0x0018)
#define HW_MUTEX_OWN_MASTER1                    (0x001C)
#define HW_MUTEX_OWN_MASTER2                    (0x0020)
#define HW_MUTEX_OWN_MASTER3                    (0x0024)
/* MUTEX config register */                 
#define HW_MUTEX_INTR                           (0x0028)
#define HW_MUTEX_CFG                            (0x002C)
/* MUTEX control register */                
#define HW_MUTEX_CNTL_MASTER0                   (0x0030)
#define HW_MUTEX_CNTL_MASTER1                   (0x0034)
#define HW_MUTEX_CNTL_MASTER2                   (0x0038)
#define HW_MUTEX_CNTL_MASTER3                   (0x003C)
/* MUTEX LOCK/UNLOCK registers */
#define HW_MUTEX_MASTER0_BASE                   (0x0100)
#define HW_MUTEX_MASTER1_BASE                   (0x0180)
#define HW_MUTEX_MASTER2_BASE                   (0x0200)
#define HW_MUTEX_MASTER3_BASE                   (0x0280)


/****************************************************************************/
/****************************************************************************/
/*                          HW Mutex Helper macros                          */
/****************************************************************************/
/****************************************************************************/

/* Helper function for 32 bit endian swapping */

#define ENDIAN_SWAP32(x) ({ __typeof__ (x) val = (x); \
                               (((val & 0x000000FF)<<24) | ((val & 0x0000FF00)<<8) | ((val & 0x00FF0000)>>8)  | ((val & 0xFF000000)>>24)); \
                          })

/* Macro to read HW Mutex register from base address + offset */
#define HW_MUTEX_REG_GET(offset)                (*((volatile unsigned int *)(HW_MUTEX_BASE_ADDR + (offset))))
#define HW_MUTEX_REG_SET(offset, val)           ((*((volatile unsigned int *)(HW_MUTEX_BASE_ADDR + (offset)))) = (val))

/* HW_MUTEX_MASTER_REG is used to request/release the HW mutex.*/
#define HW_MUTEX_ARM11_LOCK_REG(mutex_num)      (ENDIAN_SWAP32(HW_MUTEX_REG_GET(HW_MUTEX_MASTER0_BASE + ((mutex_num)<<2))))
#define HW_MUTEX_MASTER1_REG(mutex_num)         (HW_MUTEX_REG_GET(HW_MUTEX_MASTER1_BASE + ((mutex_num)<<2)))
#define HW_MUTEX_ATOM_LOCK_REG(mutex_num)       (HW_MUTEX_REG_GET(HW_MUTEX_MASTER2_BASE + ((mutex_num)<<2)))
#define HW_MUTEX_MASTER3_REG(mutex_num)         (HW_MUTEX_REG_GET(HW_MUTEX_MASTER3_BASE + ((mutex_num)<<2)))

#define HW_MUTEX_ARM11_UNLOCK(mutex_num)        (HW_MUTEX_REG_GET(HW_MUTEX_MASTER0_BASE + ((mutex_num)<<2))) = ENDIAN_SWAP32(1)
#define HW_MUTEX_ATOM_UNLOCK(mutex_num)         (HW_MUTEX_REG_GET(HW_MUTEX_MASTER2_BASE + ((mutex_num)<<2))) = (1)


/* HW_MUTEX_INTR_REG is used to release the mutex interrupt */
#define HW_MUTEX_INTR_REG                       (HW_MUTEX_REG_GET(HW_MUTEX_INTR))
#define HW_MUTEX_INTC_CONFIG_REG                (HW_MUTEX_REG_GET(HW_MUTEX_CFG))

/* HW_MUTEX_OWN_MASTER_REG is used to pool the mutex ownership */
#define HW_MUTEX_ARM11_OWN_REG                  (ENDIAN_SWAP32(HW_MUTEX_REG_GET(HW_MUTEX_OWN_MASTER0)))
#define HW_MUTEX_OWN_MASTER1_REG                (HW_MUTEX_REG_GET(HW_MUTEX_OWN_MASTER1))
#define HW_MUTEX_ATOM_OWN_REG                   (HW_MUTEX_REG_GET(HW_MUTEX_OWN_MASTER2))
#define HW_MUTEX_OWN_MASTER3_REG                (HW_MUTEX_REG_GET(HW_MUTEX_OWN_MASTER3))

/* HW_MUTEX_CNTL_MASTER_REG is used to config the mutex scheduler mode NULL or FIFO */
#define HW_MUTEX_CNTL_ARM11_REG                 (ENDIAN_SWAP32(HW_MUTEX_REG_GET(HW_MUTEX_CNTL_MASTER0)))
#define HW_MUTEX_CNTL_MASTER1_REG               (HW_MUTEX_REG_GET(HW_MUTEX_CNTL_MASTER1))
#define HW_MUTEX_CNTL_ATOM_REG                  (HW_MUTEX_REG_GET(HW_MUTEX_CNTL_MASTER2))
#define HW_MUTEX_CNTL_MASTER3_REG               (HW_MUTEX_REG_GET(HW_MUTEX_CNTL_MASTER3))

/* HW_MUTEX_STATUS_REG is used to get the Lock/Unlock status of a mutex.*/
#define HW_MUTEX_STATUS_REG                     (ENDIAN_SWAP32(HW_MUTEX_REG_GET(HW_MUTEX_STATUS)))



/****************************************************************************/
/****************************************************************************/
/*                          HW Mutex Bit Field definitions                  */
/****************************************************************************/
/****************************************************************************/

/* HW_MUTEX_STATUS_REG Bit Field definitions */
/* default value for this Reg is 0xFFF (all HW mutex are free)
   This is a non-destructive view of the MUTEX Lock Status.
   The corresponding MUTEX is locked if the bit is cleared (0). The MUTEX is available for lock if the corresponding bit is set (1).*/
#define HW_MUTEX_STATUS_LOCK                    (0)
#define HW_MUTEX_STATUS_UNLOCK                  (1)

/* HW_MUTEX_CFG Bit Field definitions */
/* ATOM code is responsible to configure the working mode */
#define HW_MUTEX_CFG_POLLING_MODE               (0)
#define HW_MUTEX_CFG_INTERRUPT_MODE             (1)
#define HW_MUTEX_ARM11_CFG_INTC_MODE            (ENDIAN_SWAP32(HW_MUTEX_CFG_INTERRUPT_MODE))

/* HW_MUTEX_CNTL_MASTERx Bit Field definitions */
#define HW_MUTEX_CNTL_FIFO_SCHEDULER_MODE       (0)
#define HW_MUTEX_CNTL_NULL_SCHEDULER_MODE       (1)
/* HW_MUTEX_MASTERx read access is used to take the mutex (lock) */
#define HW_MUTEX_MASTER_NOT_TAKEN               (0)
#define HW_MUTEX_MASTER_TAKEN_SUCCESSFULLY      (1)

/* HW_MUTEX_MASTER0_RELEASE_BIT set the mask bit to release the Mutex after interrupt */
#define HW_MUTEX_MASTER0_RELEASE_BIT            (0x1)
#define HW_MUTEX_MASTER1_RELEASE_BIT            (0x2)
#define HW_MUTEX_MASTER2_RELEASE_BIT            (0x4)
#define HW_MUTEX_MASTER3_RELEASE_BIT            (0x8)

#define HW_MUTEX_ARM11_INTR_RELEASE_BIT         (ENDIAN_SWAP32(HW_MUTEX_MASTER0_RELEASE_BIT))
#define HW_MUTEX_ATOM_INTR_RELEASE_BIT          (HW_MUTEX_MASTER2_RELEASE_BIT)


/****************************************************************************/
/****************************************************************************/
/*                          HW Mutex API                                    */
/****************************************************************************/
/****************************************************************************/



/**************************************************************************/
/*! \fn void hw_mutex_init(void)
 **************************************************************************
 *  \brief    Init HW Mutex
 *  \return  Void                                   
 **************************************************************************/
void hw_mutex_init(void);

/**************************************************************************/
/*! \fn int hw_mutex_is_locked(hw_mutex_device_type resource_id)
 **************************************************************************
 *  \brief    Test if ARM11 already own the mutex.
 *  \param[in] resource_id - mutex resource (SPI Flash or eMMC Flash)
 *  \return  1: locked, 0: unlocked
 **************************************************************************/
int hw_mutex_is_locked(hw_mutex_device_type resource_id);

/**************************************************************************/
/*! \fn int hw_mutex_lock(hw_mutex_device_type resource_id)
 **************************************************************************
 *  \brief    Take the HW mutex
 *            This function halt the code until the HW mutex is taken.
 *  \param[in] resource_id - mutex resource (SPI Flash or eMMC Flash)
 *  \return  1: locked, 0: unlocked
 **************************************************************************/
int hw_mutex_lock(hw_mutex_device_type resource_id);



/**************************************************************************/
/*! \fn void hw_mutex_unlock(hw_mutex_device_type resource_id)
 **************************************************************************
 *  \brief    Release the HW Mutex
 *  \param[in] resource_id - Mutex resource (SPI Flash or eMMC Flash)
 *  \return  Void                                   
 **************************************************************************/
void hw_mutex_unlock(hw_mutex_device_type resource_id);

#endif /* _PUMA6_HW_MUTEX_H_ */



