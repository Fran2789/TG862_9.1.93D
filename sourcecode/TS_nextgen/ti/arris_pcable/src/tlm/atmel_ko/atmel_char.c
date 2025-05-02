/*

Copyright (c) 2010-2016 ARRIS Enterprises, Inc.

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
#include "xnvm.h"
#include "atmel_phy.h"
#include "battery.h"

MODULE_AUTHOR("Matt Snoby - Arris");
MODULE_LICENSE("Dual BSD/GPL");

/********************* Defines          **************************/
#ifndef ATMEL_MAJOR
#define ATMEL_MAJOR 112   /* dynamic major by default */
#endif

#ifndef ATMEL_MINOR
#define ATMEL_MINOR 0   /* dynamic major by default */
#endif

#define SIZE_OF_FLASH      (0x5000)
#define SIZE_OF_EACH_PAGE  (256)
#define CRC_CCITT_POLY     (0x8408)
#define ATMEL_FLASH_BASE   (0x0800000)

extern void reset_gpio_lines(void);

/********************* Structures          **************************/

/* data structures*/
struct atmel_dev 
{
    dev_t      deviceNo;
    unsigned int access_key;    /* used to allow only one user of this device */
    struct mutex lock;          /* mutual exclusion semaphore     */
    struct cdev cdev;	        /* Char device structure		*/
    unsigned int pos;
    unsigned int ImagePos;
    unsigned int CurrentCRC;
};

/********************* Private data          **************************/

static unsigned int gCRC =0;
static uint16 *crctable;
static struct atmel_dev Atmel_Device;
static struct proc_dir_entry *gpAtmel_Proc_File;
bool xnvm_init_read( void );
void hal_pdi_deinit( void );

static long atmel_ioctl(struct file* file, unsigned int cmd, unsigned long arg );
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
static uint16 crc_ccitt (uint16 crc, uint8 *data, uint16 size)
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
static void gen_crctable (uint16 *table)
{
 int i, j;
 uint16 crc;

 for (i = 0; i < 256; i++)
 {
  crc = i;

  for (j = 0; j < 8; j++)
  {
   if (crc & 0x01) { crc >>= 1;  crc ^= CRC_CCITT_POLY; }
   else            { crc >>= 1;                         }
  }
  table[i] = crc;
 }
}








static void initialize_atmel_interface(void)
{
    reset_gpio_lines();
    msleep(10);

}


/*
 * The proc filesystem: function to read and entry
 */
int atmel_read_procmem(char *buf, char **start, off_t offset,int count, int *eof, void *data)
{

    *eof =1;
    memset(buf, 0, count);
    sprintf(buf,"CRC: 0x%x\n", gCRC); 


	return strlen(buf);
}





int atmel_write_procmem(struct file *file, const char *buffer, unsigned long count, void *data)
{

    return count;
}







/*
 * Actually create (and remove) the /proc file(s).
 */

static void atmel_create_proc(void)
{
    gpAtmel_Proc_File = create_proc_entry("atmel", 0644, NULL);

    if (gpAtmel_Proc_File)
    {
        gpAtmel_Proc_File->read_proc = atmel_read_procmem;
        gpAtmel_Proc_File->write_proc = atmel_write_procmem;
    }
}

static void atmel_remove_proc(void)
{
    /* no problem if it was not registered */
    remove_proc_entry("atmel", NULL /* parent dir */);
}

/*
 * Open and close
 */

int atmel_open(struct inode *inode, struct file *filp)
{
    struct atmel_dev *pDevice = NULL; /* device information */
    int result = 0;

    pDevice = container_of(inode->i_cdev, struct atmel_dev, cdev);
    filp->private_data = pDevice; /* Save the pointer to our device structure so we can use it later. */
    
    if (!mutex_trylock(&pDevice->lock))
    {
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
        pDevice->CurrentCRC =0;

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
int atmel_release(struct inode *inode, struct file *filp)
{
    struct atmel_dev *pDevice = NULL; /* device information */
    int result = 0;

    pDevice = filp->private_data;
    
    if (!mutex_trylock(&pDevice->lock))
    {
        return -ERESTARTSYS;
    }

    do
    {
        
        gCRC = pDevice->CurrentCRC;
        pDevice->access_key--;
        hal_pdi_releaseprocessor();


     } while (0);
    
     mutex_unlock(&pDevice->lock);

     return result;          
}
/* here is where the magic happens we will read out of the atmel part 256 bytes or one page at a time.*/
ssize_t atmel_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct atmel_dev *pDevice = filp->private_data;
    ssize_t retval = 0;
    uint32 location =0 ;
    unsigned char *pKbuf;
    char page[ATMEL_PAGE_SIZE];
    uint32  int_status =0;
    uint32  result =0;


    if (count > SIZE_OF_EACH_PAGE)
    {
        count = SIZE_OF_EACH_PAGE;
    }

    if (!mutex_trylock(&pDevice->lock))
        return -ERESTARTSYS;

    /* only allow them to read one page at a time which is 256 bytes.*/
    location = pDevice->ImagePos;
    pKbuf = page;

    initialize_atmel_interface();
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);      /* lock out the world*/
    PDI_sysGpioControlPUPD(false, 0x40);                           /* turn off bit 6 for the pull up resistor*/
    udelay(20);

    if (true ==xnvm_init())                                       /* start up the PDI interface*/
    {
        result = nvm_read_memory(pKbuf, ATMEL_PAGE_SIZE, (location + ATMEL_FLASH_BASE));
        if (result < 1)
        {
            printk(" failure to read memory\n");
            count =0; // nothing was able to be written.
        }
    }
    else
    {
     printk (" interface is not functioning\n");
    }


    hal_pdi_deinit( );                                            /* shut down the interface*/
    PDI_sysGpioControlPUPD(true, 0x40);                           /* turn on the pull up resistor*/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);       /* allow the world to run again.*/

    
   if (location == SIZE_OF_FLASH )
   {
       /* we have reached the end of the file, we are done.*/
       retval =0;
       goto out;
   }

   if (location + count > SIZE_OF_FLASH)
   {
       count = SIZE_OF_FLASH - location;
   }
    /* calculate the CRC */
   pDevice->CurrentCRC =  crc_ccitt (pDevice->CurrentCRC, pKbuf, count );


    if (copy_to_user(buf, pKbuf, count)) {
        retval = -EFAULT;
        goto out;
    }
    pDevice->ImagePos += count;
    *f_pos += count;
    retval = count;// count;

  out:

    mutex_unlock(&pDevice->lock);
    return retval;
}


/* atmel_write is where we will write one page at a time to the atmel part which is 256 bytes.*/
ssize_t atmel_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct atmel_dev *pDevice = filp->private_data;
    char *pKbuf = NULL;
    ssize_t numBytesWritten = 0;
    ssize_t length = 0;
    char page[ATMEL_PAGE_SIZE];
    unsigned int location =0;
    uint32  int_status =0;



    if (!mutex_trylock(&pDevice->lock))
    {
        return -ERESTARTSYS;
    }
    do
    {
        *f_pos = pDevice->ImagePos;
        location = pDevice->ImagePos;
        
        memset(page, 0, ATMEL_PAGE_SIZE);
        pKbuf = page;
        
        /* we can only write one page at a time, we shouldn't be passed in more than 256 bytes*/
        
        length = count;
        if (length == 0)
        {
            printk(" Length of write = 0\n");
        }
        if (length > ATMEL_PAGE_SIZE)
        {
           printk(" WARNING: You are passing in a write that is too large, I only accept 256 bytes of data\n");
           length = ATMEL_PAGE_SIZE;
        }
        /* offset to where we are*/
        
        
        // this function call can sleep
        if (0 != copy_from_user(pKbuf, buf,length))
        {
            numBytesWritten = -EFAULT;
            break;
        }
        
        /* 
            now we contain the complete binary file in memory we need to bit bang it down to 
        */
        /* calculate where we are going to write this page too in the atmel flash space.*/
        location += ATMEL_FLASH_BASE; /* we have to offset the location to the base address in the atmel part.*/ 

        if (location > (ATMEL_FLASH_BASE + SIZE_OF_FLASH ) )
        {
            printk (" Attempt to write beyond the size of flash!!! - 0x%x\n", location);
            numBytesWritten =0;
            break;


        }

        initialize_atmel_interface();
        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);      /* lock out the world*/
        PDI_sysGpioControlPUPD(false, 0x40);                           /* turn off bit 6 for the pull up resistor*/
        udelay(20);
        if (true ==xnvm_init())                                       /* start up the PDI interface*/
        {
            if (nvm_program_page(pKbuf, length , location)!= true)
            {
                printk(" failure to write\n");
                length =0; // nothing was able to be written.
            }
        }
        else
        {
         printk (" interface is not functioning\n");
        }
        
        
        hal_pdi_deinit( );                                            /* shut down the interface*/
        PDI_sysGpioControlPUPD(true, 0x40);                           /* turn on the pull up resistor*/
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);       /* allow the world to run again.*/ 
       
        
        // do work.
        numBytesWritten = length;
        

    }while(0);

    pDevice->ImagePos += numBytesWritten;
    *f_pos = pDevice->ImagePos;
    mutex_unlock(&pDevice->lock);

    return numBytesWritten;
}



struct file_operations atmel_file_operations = 
{
    .owner          = THIS_MODULE,
    .write          = atmel_write,
    .read           = atmel_read,
    .unlocked_ioctl = atmel_ioctl,
    .open           = atmel_open,
    .release        = atmel_release,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static void __exit atmel_cleanup_module(void)
{
    struct atmel_dev *pDevice = &Atmel_Device;

	/* Get rid of our char dev entries */
	cdev_del(&pDevice->cdev);

    // bye bye proc entry
    atmel_remove_proc();

    kfree(crctable);

	/* cleanup_module is never called if registering failed */

	unregister_chrdev_region(pDevice->deviceNo, ATMEL_MINOR);


}

// Rigth now this is only use to program fuse
// it can be modified to do other  things
static long atmel_ioctl(struct file* file, unsigned int cmd, unsigned long arg )                     

{  
    struct atmel_dev *pDevice = file->private_data; 
    ssize_t retval = 0; 
    uint32  int_status =0;
    uint32  result =0;
    if (cmd != BATTERY_IOCTWRITEFUSE && cmd != BATTERY_IOCTERASEAPPANDBOOT)
    {
        printk ("TLM:atmel_ioctl error. Cmd not BATTERY_IOCTWRITEFUSE\n");
        return (-EINVAL);
    } 
    if (!mutex_trylock(&pDevice->lock))
        return -ERESTARTSYS;

    initialize_atmel_interface();
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &int_status);      /* lock out the world*/
    PDI_sysGpioControlPUPD(false, 0x40);                           /* turn off bit 6 for the pull up resistor*/
    udelay(20);
    if (true ==xnvm_init())                                       /* start up the PDI interface*/
    {
        if(cmd == BATTERY_IOCTWRITEFUSE )
        {
            result = nvm_set_FuseBytes();
        }
        else if (cmd == BATTERY_IOCTERASEAPPANDBOOT)
        {
            result = nvm_erase_appsec();             
            result = nvm_erase_bootloader();
        }
        
        if (result < 1)
        {            
            retval = -EIO;
        }       
    }
    else
    {
     printk (" interface is not functioning\n");
    }
    hal_pdi_deinit( );                                            /* shut down the interface*/
    PDI_sysGpioControlPUPD(true, 0x40);                           /* turn on the pull up resistor*/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, int_status);       /* allow the world to run again.*/

    mutex_unlock(&pDevice->lock);
    return retval;
}

static int __init atmel_init_module(void)
{
    int result;
    struct atmel_dev *pDevice = &Atmel_Device;


    do
    {
        memset(pDevice, 0, sizeof(struct atmel_dev));

        /* register my device number for the firmware writer device.*/
        pDevice->deviceNo = MKDEV(ATMEL_MAJOR, ATMEL_MINOR);
        result = register_chrdev_region(pDevice->deviceNo, ATMEL_MINOR, "atmel_fw_ldr");
        if (result)
        {
            printk(KERN_ALERT "Error %d registering chrdrv atmel device\n", result);
            break;
        }
    
        /* initialize the mutex and the character device values*/
        mutex_init( &pDevice->lock );
        
        cdev_init(&pDevice->cdev, &atmel_file_operations);
        pDevice->cdev.owner = THIS_MODULE;
        pDevice->cdev.ops = &atmel_file_operations;
        result = cdev_add (&pDevice->cdev, pDevice->deviceNo, 1);
        /* Fail gracefully if need be */
        if (result)
        {
            printk(KERN_ALERT "Error %d adding atmel device\n", result);
            break;
        }

         atmel_create_proc();

         if (!(crctable = (uint16 *) kmalloc(sizeof(uint16) * 256, GFP_KERNEL)))
         {
           printk ("CHRGR_dnld: kmalloc failed\n");
           return FALSE;
         }
         gen_crctable (crctable);


        printk(KERN_ALERT "successfull loaded atmel bootloader\n");
	    return 0; /* succeed */

    } while (0);

    /* will only get here on an error*/
    atmel_cleanup_module();
	return result;

}


module_init (atmel_init_module);
module_exit (atmel_cleanup_module);


