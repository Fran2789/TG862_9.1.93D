/*

Copyright (c) 2015 ARRIS Enterprises, Inc.
Copyright (c) 2016 ARRIS Enterprises, LLC

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products 
   derived from this software without specific prior written permission.


Alternatively, this software may be distributed under the terms of the
GNU General Public License ("GPL") version 2 as published by the Free
Software Foundation. 
 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

GPLv2 license:

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along with 
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

//#include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/vmalloc.h>      /* vmalloc*/
#include <linux/mutex.h>    /* Mutex */
#include <sys/ioctl.h>
#include <asm/system.h>  /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_*_user */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <asm-arm/arch-avalanche/generic/pal.h>
#include "tlm_arris.h"
#include "battery.h"

#include "jtagfunc430.h"       // JTAG functions
#include "config430.h"         // High level user configuration
#include "lowlevelfunc430.h"  // Low level functions

MODULE_AUTHOR("Jarod Guo - Arris");
MODULE_DESCRIPTION("Support for TI Battery Charger.");
MODULE_LICENSE("Dual BSD/GPL");

/********************* Defines          **************************/
#ifndef TIBAT_MAJOR
#define TIBAT_MAJOR 112   /* dynamic major by default */
#endif

#ifndef TIBAT_MINOR
#define TIBAT_MINOR 0   /* dynamic major by default */
#endif

#define CRC_CCITT_POLY     (0x8408)
#define TIBAT_FLASH_BASE   (0xC000)


/********************* Structures          **************************/

/* data structures*/
struct tibat_dev 
{
    dev_t      deviceNo;
    unsigned int access_key;    /* used to allow only one user of this device */
    struct mutex lock;          /* mutual exclusion semaphore     */
    struct cdev cdev;	        /* Char device structure		*/
    unsigned int pos;
    unsigned int ImagePos;
};

/********************* Private data          **************************/

static uint16 *crctable;
static struct tibat_dev Tibat_device;
static boolean sbwMode =  False;
static long tibat_ioctl(struct file* file, unsigned int cmd, unsigned long arg );

/**************************************************************************/
/*! \fn  uint16 crc_ccitt (uint16 crc, uint8 *data, uint16 size)
 **************************************************************************
 *  \brief Calculates a CRC value for the binary
 *  \param[in] crc this is the previous CRC value
 *  \param[in] *data - pointer to the data buffer that is being calculated
 *  \param [in] size - length of the buffer 
 *  \return [out] result - New CRC value.
 *  \note:  
 **************************************************************************/
static uint16 tibat_crc_ccitt (uint16 crc, uint8 *data, uint16 size)
{
 crc = ~crc;
 
     
 for ( ; size; size--)
 {
   crc = (crc >> 8) ^ crctable[(*data++ ^ crc) & 0xff];
 }

 return ~crc;
}



/**************************************************************************/
/*! \fn  void gen_crctable (uint16 *table)
 **************************************************************************
 *  \brief Generate a table used for calculating CRC this will only happen once.
 *  \param[in] *table 
 *  \return [out] none
 *  \note:  
 **************************************************************************/
static void tibat_gen_crctable (uint16 *table)
{
    int i, j;
    uint16 crc;

    /* table size is TIBAT_PAGE_SIZE (1024) bytes or TIBAT_PAGE_SIZE/2 words */
    for (i = 0; i < (TIBAT_PAGE_SIZE/2); i++)
    {
        crc = i;

        for (j = 0; j < 8; j++)
        {
            if (crc & 0x01) 
            { 
                crc >>= 1;  
                crc ^= CRC_CCITT_POLY; 
            }
            else
            { 
                crc >>= 1;                         
            }
        }
        table[i] = crc;
    }
}

static word initialize_tibat_interface(void)
{
    word ret = STATUS_OK;

    /* initialize interface only once per download */
    if(sbwMode)
    {
        return ret;
    }

    /* initial SBW mode sequence */
    EnterSBWmode();

    /* Run GetDevice sequence */
    if (GetDevice() != STATUS_OK)         // Set DeviceId
    {
        printk(KERN_ERR " GetDevice failure\n");
        ret = STATUS_ERROR;
    }  
    else
    {
        sbwMode =  True;
    }

    msleep(10); /* 10 msecs looks completely arbitrary */

    return ret;
}

static void reset_tibat_interface(void)
{
    /* exit sbwmode if it was enabled */
    if(sbwMode)
    {
        ExitSBWmode();
        sbwMode =  False;
    }
     
    return;
}

/*
 * Open and close
 */

int tibat_open(struct inode *inode, struct file *filp)
{
    struct tibat_dev *pDevice = NULL; /* device information */
    int result = 0;

    pDevice = container_of(inode->i_cdev, struct tibat_dev, cdev);
    filp->private_data = pDevice; /* Save the pointer to our device structure so we can use it later. */
    
    if (!mutex_trylock(&pDevice->lock))
    {
        printk(KERN_ERR " mutex_trylock failed %s\n",__FUNCTION__);
        return -ERESTARTSYS;
    }

    do
    {
        /* only one instance is allowed open at a time.*/
        if (pDevice->access_key)
        {
            /* device is already open.*/
            result = -EMFILE;
            break;
        }

        /* 
            reset total number of bytes written.
            This is the most important piece of information.
        */
        pDevice->pos =0;
        pDevice->ImagePos =0;
    
        pDevice->access_key++;

     } while (0);
    
     mutex_unlock(&pDevice->lock);
     return result;          
}

/* close the interface*/
int tibat_release(struct inode *inode, struct file *filp)
{
    struct tibat_dev *pDevice = NULL; /* device information */
    int result = 0;

    pDevice = filp->private_data;
    
    if (!mutex_trylock(&pDevice->lock))
    {
        printk(KERN_ERR " mutex_trylock failed %s\n",__FUNCTION__);
        return -ERESTARTSYS;
    }

    do
    {
        pDevice->access_key--;

     } while (0);
    
     mutex_unlock(&pDevice->lock);

     return result;          
}
/* here is where the magic happens we will read out of the TI MSP430 flash 1024 bytes or one segment at a time.*/
ssize_t tibat_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct tibat_dev *pDevice = filp->private_data;
    ssize_t retval = 0;
    uint32 location =0 ;
    unsigned char *pKbuf;
    word startAddr,endAddr;

    /* validata count */
    if(count < 1)
    {
        printk(KERN_ERR "%s: invalid count\n",__FUNCTION__);
        return -ERESTARTSYS;
    }

    if (!mutex_trylock(&pDevice->lock))
    {
        printk(" mutex_trylock failed %s\n",__FUNCTION__);
        return -ERESTARTSYS;
    }

    location = pDevice->ImagePos;
    pKbuf = kmalloc(count,GFP_KERNEL);

    if(pKbuf == NULL)
    {
        printk(KERN_ERR "Unable to allocate pKbuf\n");
        mutex_unlock(&pDevice->lock);
        return -EFAULT;
    }

    /* calculate start/end addresses of flash */
    startAddr =  TIBAT_FLASH_BASE + location;
    endAddr =    (TIBAT_FLASH_BASE + location) + (count-1);

    /* initialized SBW mode if needed */
    if(initialize_tibat_interface() != STATUS_OK)
    {
        printk (KERN_ERR " initialize_tibat_interface failed\n");
        kfree(pKbuf);
        mutex_unlock(&pDevice->lock);
        return -EFAULT;
    }

    // read data from FLASH
    if(ReadFLASH(startAddr, endAddr, pKbuf) != 0 )
    {
        printk(KERN_ERR "unable to read flash \n");
        kfree(pKbuf);
        mutex_unlock(&pDevice->lock);
        return -EFAULT;
    }

    /*free memory */
    kfree(pKbuf);

    pDevice->ImagePos += count;
    *f_pos += count;
    retval = count;// count;

    mutex_unlock(&pDevice->lock);
    return retval;
}


/* tibat_write is where we will write one page at a time to the tibat part which is 1024 bytes.*/
ssize_t tibat_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct tibat_dev *pDevice = filp->private_data;
    word *pKbuf = NULL;
    ssize_t numBytesWritten = 0;
    ssize_t length = 0;
    word convertEndianness;
    unsigned int wordLength;

    unsigned int location =0;
    uint32 j;

    if (!mutex_trylock(&pDevice->lock))
    {
        printk(KERN_ERR " mutex_trylock failed %s\n",__FUNCTION__);
        return -ERESTARTSYS;
    }

    do
    {
        *f_pos = pDevice->ImagePos;
        location = pDevice->ImagePos;
        
        /* we can only write one page at a time, we shouldn't be passed in more than 1024 bytes*/
        length = count;
        if ( (length == 0) || (length > TIBAT_PAGE_SIZE))
        {
            printk(KERN_ERR "%s: invalid length\n",__FUNCTION__);
            break;
        }

        /* calculate length in word - length/2*/
        wordLength = length >> 1;

        /* allocate memory to copy data from application */
        pKbuf = (word *)kmalloc(length,GFP_KERNEL);
        if(pKbuf == NULL)
        {
            printk(KERN_ERR "Unable to allocate pKbuf\n");
            break;
        }
        memset((unsigned char *)pKbuf, 0, length);

        // this function call can sleep
        if (0 != copy_from_user((unsigned char *)pKbuf, buf,length))
        {
            numBytesWritten = 0;
            break;
        }

        // Convert from big endian to little endian
        for(j = 0; j < wordLength; j++)
        {
            convertEndianness = pKbuf[j];
            pKbuf[j] = convertEndianness << 8; // MSB
            pKbuf[j] |= ( convertEndianness  >> 8);  // LSB
        }
		
        /* calculate where we are going to write this segment in the tibat flash space.*/
        /* we have to offset the location to the base address in the tibat part.*/ 
        /* The segment 15 starting from 0xC000 contains production test calibration data should not be erased */
        /* or programmed, start from address 0xC400*/
        location = location + TIBAT_FLASH_BASE + 0x400; 

        if (location > (TIBAT_FLASH_BASE + TIBAT_SIZE_OF_FLASH - 0x400) )
        {
            printk (KERN_ERR " Attempt to write beyond the size of flash!!! - 0x%x\n", location);
            numBytesWritten =0;
            break;
        }

        /* initialize interface - sbw mode */
        if(initialize_tibat_interface() != STATUS_OK)
        {
            printk (KERN_ERR " initialize_tibat_interface failed\n");
            numBytesWritten =0;
            break;
        }


        // write to flash one segment at a time
        if (!WriteFLASH(location, wordLength, pKbuf))
        {
            printk(KERN_ERR " failure to write\n, the address is invalid");
            numBytesWritten=0;
            break;
        }
        
        numBytesWritten = length; // calculate the num of bytes
    }while(0);

    pDevice->ImagePos += numBytesWritten;
    *f_pos = pDevice->ImagePos;
    mutex_unlock(&pDevice->lock);

    /*free memory */
    if(pKbuf)
    {
        kfree(pKbuf);
    }

    return numBytesWritten;
}



struct file_operations tibat_file_operations = 
{
    .owner          = THIS_MODULE,
    .write          = tibat_write,
    .read           = tibat_read,
    .unlocked_ioctl = tibat_ioctl,
    .open           = tibat_open,
    .release        = tibat_release,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static void __exit tibat_cleanup_module(void)
{
    struct tibat_dev *pDevice = &Tibat_device;

    /* exit sbw mode before unloading the module */
    reset_tibat_interface();

    kfree(crctable);

    /* Get rid of our char dev entries */
    cdev_del(&pDevice->cdev);

    /* cleanup_module is never called if registering failed */
    unregister_chrdev_region(pDevice->deviceNo, TIBAT_MINOR);
}

// Rigth now this is only use to program fuse
// it can be modified to do other  things
static long tibat_ioctl(struct file* file, unsigned int cmd, unsigned long arg )                     
{  
    struct tibat_dev *pDevice = file->private_data; 
    ssize_t retval = 0; 
    uint32  i;
    word startAddr, endAddr;
    unsigned char *dataArray;
    uint16 currentCRC=0;
    unsigned int ix, ret; 
    unsigned int crcInfo;

    if (!mutex_trylock(&pDevice->lock))
    {
        printk(" mutex_trylock failed %s\n",__FUNCTION__);
        return -ERESTARTSYS;
    }

    switch(cmd)
    {
#ifdef TIBAT_DEBUG
        case BATTERY_IOCTDEBUG:
            printk(" BATTERY_IOCTDEBUG \n");
            break;
#endif 

        case BATTERY_IOCTERASEAPPANDBOOT:
            /* setup sbw interface if needed */
            if(initialize_tibat_interface() != STATUS_OK)
            {
                printk (KERN_ERR " initialize_tibat_interface failed\n");
                retval = -EFAULT;
                break;
            }

            // Erase main FRAME memory.
            // Flash must not be mass erased or data that is stored in Segment 15 will be lost
            for (i = 0; i < 15; i++)
            {
               // Flash is erased one segment at a time, there are maximum 15 segment (0-14) to be erasable
               // segment 15 at 0xc000 should not be erased.
               // Flash should be erased starting at Segment 14 and ending with Segment 0
               if (!EraseFLASH(ERASE_SGMT, TIBAT_FLASH_BASE + 0x400 + i*(TIBAT_PAGE_SIZE)))
               {
                   printk(KERN_ERR " failure to erase\n, the address is invalid");
                   retval = -EIO;
               }
            }
            /* TODO */
            /* NEED TO VERIFY THAT ERASE WORKED */

            break;

        case BATTERY_IOCTCRC:
            /* setup sbw interface if needed */
            if(initialize_tibat_interface() != STATUS_OK)
            {
                printk (KERN_ERR " initialize_tibat_interface failed\n");
                retval = -EFAULT;
                break;
            }


            dataArray = kmalloc(TIBAT_PAGE_SIZE,GFP_KERNEL);

            /* 15 sectors total, skip first sector that contains manufacturing data */
            /* read flash and calculate the CRC */
            for(ix=1;ix<16;ix++)
            {
                startAddr =  MAIN_START_ADDRESS + (TIBAT_PAGE_SIZE*ix);
                endAddr = startAddr + (TIBAT_PAGE_SIZE - 1);
                /* read one segment at a time and calculate CRC */
                ReadFLASH(startAddr,endAddr,dataArray);
                currentCRC =  tibat_crc_ccitt (currentCRC, dataArray, TIBAT_PAGE_SIZE );
            }
            kfree(dataArray);

            // this function call can sleep
            if (0 != (ret = copy_from_user(&crcInfo, (unsigned int *)arg,sizeof(crcInfo))))
            {
                printk(KERN_ERR "CRC ioctl copy_from_user failure (%d)  \n",ret);
                retval = -EFAULT;
            }

            if(crcInfo != currentCRC)
            {
                printk(KERN_ERR "Bad CRC.  Expected: 0x%x, Received: 0x%x\n", crcInfo, currentCRC);
                retval = -EIO;
            }
            break;

        default:
                printk(KERN_ERR "Invalid ioctl cmd %x\n",cmd);
                retval = -EFAULT;
            break;
    }

    mutex_unlock(&pDevice->lock);
    return retval;
}

static int __init tibat_init_module(void)
{
    int result;
    struct tibat_dev *pDevice = &Tibat_device;

    do
    {
        memset(pDevice, 0, sizeof(struct tibat_dev));

        /* register my device number for the firmware writer device.*/
        pDevice->deviceNo = MKDEV(TIBAT_MAJOR, TIBAT_MINOR);
        result = register_chrdev_region(pDevice->deviceNo, TIBAT_MINOR, "tibat_fw_ldr");
        if (result)
        {
            printk(KERN_ALERT "Error %d registering chrdrv tibat device\n", result);
            break;
        }
    
        /* initialize the mutex and the character device values*/
        mutex_init( &pDevice->lock );
        
        cdev_init(&pDevice->cdev, &tibat_file_operations);
        pDevice->cdev.owner = THIS_MODULE;
        pDevice->cdev.ops = &tibat_file_operations;
        result = cdev_add (&pDevice->cdev, pDevice->deviceNo, 1);
        /* Fail gracefully if need be */
        if (result)
        {
            printk(KERN_ALERT "Error %d adding tibat device\n", result);
            break;
        }

         if (!(crctable = (uint16 *) kmalloc(TIBAT_PAGE_SIZE, GFP_KERNEL)))
         {
           printk ("CHRGR_dnld: kmalloc failed\n");
           return FALSE;
         }
         tibat_gen_crctable (crctable);

        printk("successfull loaded tibat bootloader\n");
        return 0; /* succeed */

    } while (0);

    /* will only get here on an error*/
    tibat_cleanup_module();
	return result;

}
module_init (tibat_init_module);
module_exit (tibat_cleanup_module);
