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

#include <puma6_hw_mutex.h>
#include <common.h>

/* #define INVALID_MUTEX_ACCESS_TEST */

#ifdef INVALID_MUTEX_ACCESS_TEST
#warning "INVALID_MUTEX_ACCESS_TEST is define"
volatile unsigned int last_read = 0;
#endif

/**************************************************************************/
/*! \fn void hw_mutex_init(void)
 **************************************************************************
 *  \brief   Init HW Mutex
 *  \return  Void                                   
 **************************************************************************/
void hw_mutex_init(void)
{
#ifdef INVALID_MUTEX_ACCESS_TEST
    last_read = *((volatile unsigned int *)(0x08000000));
#endif

    /* Unlock all HW Mutex we own. */
    hw_mutex_unlock(HW_MUTEX_NOR_SPI);
    hw_mutex_unlock(HW_MUTEX_EMMC);
    hw_mutex_unlock(HW_MUTEX_ARM_MBX);
    hw_mutex_unlock(HW_MUTEX_ATOM_MBX);
    hw_mutex_unlock(HW_MUTEX_GPIO);
}

/**************************************************************************/
/*! \fn int hw_mutex_is_locked(hw_mutex_device_type resource_id)
 **************************************************************************
 *  \brief    Test if ARM11 already own the mutex.
 *  \param[in] resource_id - mutex resource (SPI Flash or eMMC Flash)
 *  \return  1: locked, 0: unlocked
 **************************************************************************/
int hw_mutex_is_locked(hw_mutex_device_type resource_id)
{
    /* test if we already own this HW Mutex */
    if (HW_MUTEX_ARM11_OWN_REG & (1 << resource_id))
    {
        return 1;  /* HW Mutex is lock */
    }
    return 0;
}


/**************************************************************************/
/*! \fn int hw_mutex_lock(hw_mutex_device_type resource_id)
 **************************************************************************
 *  \brief    Take the HW mutex
 *            This function halt the code until the HW mutex is taken.
 *  \param[in] resource_id - mutex resource (SPI Flash or eMMC Flash)
 *  \return  1: locked, 0: unlocked
 **************************************************************************/
int hw_mutex_lock(hw_mutex_device_type resource_id)
{
    unsigned int lock = 0;
    
    /* Read Operation from HW mutex register, activate a mutex request. */
    /* if the HW mutex is free, then the 'read' returns 1 ('HW_MUTEX_MASTER_TAKEN_SUCCESSFULLY'). */
    /* if the HW mutex is busy, then the 'read' return 0 ('HW_MUTEX_MASTER_NOT_TAKEN'). */
    /*    How can we know that the HW mutex is free and we can use it: */
    /*    1. If we work in FIFO mode --> We will get an Int after we got the HW mutex. --> So we only need to read the owner reg. */
    /*    2. If we work in NULL mode --> We will get an Int after the HW mutex is Free. --> So we need to re-request the HW mutex. */
    /* Since Boot Ram does not support interrupts, it enter to polling loop, until it get the Mutex.  */

#ifdef INVALID_MUTEX_ACCESS_TEST
     unsigned int addr = *((volatile unsigned int *)(0x0FFE0108));
    if ((addr&0xFFFFFF0F) != 0x00000008)
    {
        printf("\n **** HW Hutex problem **** Reg Addr = 0x%p \n",addr);
    }
#endif

    /* Before makeing any new HW Mutex request, we must test if we already own this HW Mutex (this is to avoid making an illegal request) */
    if (HW_MUTEX_ARM11_OWN_REG & (1 << resource_id))
    {
        return 1;  /* HW Mutex is lock */
    }
    /* Request the HW mutex. */
    lock = HW_MUTEX_ARM11_LOCK_REG(resource_id);    
    if ( HW_MUTEX_MASTER_TAKEN_SUCCESSFULLY == lock )
    {
       return 1;  /* HW Mutex is lock */
    }

    if ( HW_MUTEX_CNTL_ARM11_REG == HW_MUTEX_CNTL_FIFO_SCHEDULER_MODE )  /* if we work in FIFO mode */
    {
        /* We do polling till we own this HW Mutex. */
        while ( (HW_MUTEX_ARM11_OWN_REG & (1 << resource_id)) == 0 );
    }
    else
    {   /* We work in NULL or Polling mode */
        do
        {
            /* Wait till the Mutex is free by polling the mutex status. */
            while( (HW_MUTEX_STATUS_REG & (1 << resource_id)) == HW_MUTEX_STATUS_LOCK ); /* If bit is clear the corresponding MUTEX is locked */

        }while( HW_MUTEX_ARM11_LOCK_REG(resource_id) == HW_MUTEX_MASTER_NOT_TAKEN );     /* re-request the HW mutex till we get it. */
    }

    /* Although we are not reciving interrupts, the HW mutex is raising the interrupt, and we must clear the interrupt bit. */
    /* Clear the interrupt by writing 1. */
    HW_MUTEX_INTR_REG = HW_MUTEX_ARM11_INTR_RELEASE_BIT;

    /* make sure the mutex was taken */
    if ( HW_MUTEX_ARM11_OWN_REG & (1 << resource_id) )
    {
        return 1; /* HW Mutex is lock */
    }

    return 0; /* HW Mutex is NOT lock !! */
}


/**************************************************************************/
/*! \fn void hw_mutex_unlock(hw_mutex_device_type resource_id)
 **************************************************************************
 *  \brief    Release the HW Mutex (only if we own it)
 *  \param[in] resource_id - Mutex resource (SPI Flash or eMMC Flash)
 *  \return  Void                                   
 **************************************************************************/
void hw_mutex_unlock(hw_mutex_device_type resource_id)
{
    if ( HW_MUTEX_ARM11_OWN_REG & (1 << resource_id) )
    {

        /* To release the mutex, all we need to do, is to write something to bit#0 in the register */
        HW_MUTEX_ARM11_UNLOCK(resource_id);

#ifdef INVALID_MUTEX_ACCESS_TEST
        last_read = *((volatile unsigned int *)(0x08000000));
#endif
    }
}


