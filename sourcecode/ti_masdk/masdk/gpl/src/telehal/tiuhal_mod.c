/*
 *  File name: tiuhal_mod.c
 *
 * Description: This file is used to encapsulate the kernel/user interface 
 * and to provide a generic TIUHAL module.  Board specific and SOC specific 
 * functions are handled elsewhere.
 *
 * Copyright (C) 2008 Texas Instruments, Incorporated
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details. *   
 *
 */

#include <linux/module.h>	/* Needed by all modules */
#include <linux/moduleparam.h>
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include "rtos_hal.h"
#include "tiuhal.h"
#include "tiuhal_lin.h"

#define DRIVER_AUTHOR "Texas Instruments, Inc."
#define DRIVER_DESC "Texas Instruments Telephony SPI HAL"
#define DRIVER_LICENSE "GPL v2"

extern TIUHW_CHIPSEL_T tiuhal_chipselect[];
extern void ti_codec_spi_set_zsimode( int zsimode );  // ARRIS ADD for ZSI runtime

/*******************************************************************************
 *  Private datatypes...
 ******************************************************************************/
struct tiuhal_device
{
	struct miscdevice dev;
};

/*******************************************************************************
 *  Export to the world the license, author description, and any parameters.
 ******************************************************************************/
MODULE_LICENSE(DRIVER_LICENSE);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

/*******************************************************************************
 *  Forward references and private globals.
 ******************************************************************************/
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static long device_ioctl(struct file *, unsigned, unsigned long);

static struct file_operations fops = 
{
	.owner= THIS_MODULE,
	.open = device_open,
	.release = device_release,
	.unlocked_ioctl = device_ioctl
};

static short irq_support =0; /* Set to 1 if we support TID interrupts */
static short device_open_count=0;     /* We allow for just 1 open */
static struct tiuhal_device tiuhal_dev;
static TIUHAL_CAP_T capabilities;
static int zsimode = 0; // ARRIS ADD for ZSI runtime
module_param(irq_support, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(irq_support, "Set to 1 to enable support for tele interrupts");
module_param(zsimode, int, S_IRUGO);  // ARRIS ADD for ZSI runtime

/*******************************************************************************
* FUNCTION:     device_open
*
********************************************************************************
*
* DESCRIPTION:  This function is called when a device is openned.  
* 
* RETURNS:      0 if successful. see man 2 open for possible return values.
*
*******************************************************************************/
static int device_open(struct inode *inode, struct file *pFile)
{

	if(unlikely(device_open_count))
	{
		return -ENFILE;
	}
	device_open_count++;

	try_module_get(THIS_MODULE); /* Increment use count */

	return 0;
}

/*******************************************************************************
* FUNCTION:    device_release
*
********************************************************************************
*
* DESCRIPTION: Called when a process closes the device file. 
* 
* RETURNS:     0 if successful.
*
*******************************************************************************/
static int device_release(struct inode *inode, struct file *file)
{

	if(likely(device_open_count))
	{
		device_open_count--;
	}
	module_put(THIS_MODULE);
	return 0;
}

/*******************************************************************************
* FUNCTION:    device_ioctl 
*
********************************************************************************
*
* DESCRIPTION: This function is the main entry point after initialization.  It
*              is used to handle everything from multiple byte read/write
*              calls to general requests such as configuration data.   
* 
* RETURNS:     0 = OK
*
*******************************************************************************/
static long  device_ioctl(struct file *fp, unsigned ioctl_num, unsigned long ioctl_param)
{
	TIUHAL_LIN_RW_IOCTL_T rw_data;
	TIUHAL_DATA_T bytestream[max(TIUHAL_LIN_MAX_RD_WR_BYTES,TIUHAL_IOCTL_MAX_IOCTL_BYTES)];
	unsigned long bytes_not_copied = 1;
	unsigned long truncated_results = 0;
#ifdef LANTIQ
    UINT8 tcid;
#endif /* LANTIQ */

	/* don't even decode wrong cmds: better returning  ENOTTY than EFAULT */
	if ( unlikely((_IOC_TYPE(ioctl_num) != TIUHAL_LIN_IOCTL_MAGIC) || 
		 (_IOC_NR(ioctl_num) > TIUHAL_LIN_IOCTL_MAXNR) ) )
	{
		return -ENOTTY;
	}

	switch(ioctl_num)
	{
		/* NOTE: we assume TCID boundaries are ok for READ */
		case TIUHAL_LIN_IOCTL_MREAD:

			if(unlikely(_IOC_DIR(ioctl_num) != (_IOC_WRITE|_IOC_READ)))
			{
				return -ENOTTY;
			}

			bytes_not_copied = copy_from_user(&rw_data, __user (TIUHAL_LIN_RW_IOCTL_T *)ioctl_param,sizeof(TIUHAL_LIN_RW_IOCTL_T));

			if(unlikely(bytes_not_copied == 0))
			{
				if(rw_data.data_len > TIUHAL_LIN_MAX_RD_WR_BYTES)
				{
					rw_data.data_len = TIUHAL_LIN_MAX_RD_WR_BYTES;
					truncated_results = 1;
				}
#ifdef LANTIQ
                                /* in case of Lantiq TID pass a cookie */
                                tcid = rw_data.flags & TIUHAL_FLAGS_TID_LANTIQ ?
                                    255 - rw_data.tcid : rw_data.tcid;
                                rw_data.return_value = tiuhw_api->read(tcid, rw_data.reg, bytestream, rw_data.data_len);
#else
				rw_data.return_value = tiuhw_api->read(rw_data.tcid, rw_data.reg, bytestream, rw_data.data_len);
#endif /* LANTIQ */

#ifdef TIUHAL_DBG_RW_DATA
				{
					int i;
					for(i = 0; i < rw_data.data_len; i++)
					{
						printk("RD : %u %u\n",i, bytestream[i]);
					}
				}
#endif
				bytes_not_copied = copy_to_user(__user (((TIUHAL_LIN_RW_IOCTL_T*)ioctl_param)->bytestream), bytestream, sizeof(TIUHAL_DATA_T)*rw_data.data_len);
				put_user(rw_data.return_value, &(((TIUHAL_LIN_RW_IOCTL_T*)ioctl_param)->return_value));
			}
			break;	

		/* NOTE: we assume TCID boundaries are ok for WRITE */
		case TIUHAL_LIN_IOCTL_MWRITE:
			if(unlikely(_IOC_DIR(ioctl_num) != (_IOC_WRITE|_IOC_READ)))
			{
				return -ENOTTY;
			}
			bytes_not_copied = copy_from_user(&rw_data, __user (TIUHAL_LIN_RW_IOCTL_T *)ioctl_param,sizeof(TIUHAL_LIN_RW_IOCTL_T));

			if(unlikely(bytes_not_copied == 0))
			{
				if(unlikely(rw_data.data_len > TIUHAL_LIN_MAX_RD_WR_BYTES))
				{
					rw_data.data_len = TIUHAL_LIN_MAX_RD_WR_BYTES;
					truncated_results = 1;
				}
				bytes_not_copied = copy_from_user(bytestream,__user (((TIUHAL_LIN_RW_IOCTL_T*)ioctl_param)->bytestream), sizeof(TIUHAL_DATA_T)*rw_data.data_len);
				if(unlikely(bytes_not_copied == 0))
				{
#ifdef LANTIQ
                                   /* in case of Lantiq TID pass a cookie */
                                   tcid = rw_data.flags & TIUHAL_FLAGS_TID_LANTIQ ?
                                      255 - rw_data.tcid : rw_data.tcid;
                                   rw_data.return_value = tiuhw_api->write(tcid, rw_data.reg, bytestream, rw_data.data_len);
#else
					rw_data.return_value = tiuhw_api->write(rw_data.tcid, rw_data.reg, bytestream, rw_data.data_len);
#endif /* LANTIQ */

#ifdef TIUHAL_DBG_RW_DATA
				{
					int i;
					for(i = 0; i < rw_data.data_len; i++)
					{
						printk("WR :%u %u\n",i, bytestream[i]);
					}
				}
#endif
					put_user(rw_data.return_value, &(((TIUHAL_LIN_RW_IOCTL_T*)ioctl_param)->return_value));
				}
			}

			break;

		case TIUHAL_LIN_IOCTL_CS_REG:
			{
				TIUHAL_LIN_CS_IOCTL_T cs_data;

				if(unlikely(_IOC_DIR(ioctl_num) != (_IOC_WRITE)))
				{
					return -ENOTTY;
				}

				bytes_not_copied = copy_from_user(&cs_data, __user (TIUHAL_LIN_CS_IOCTL_T*)ioctl_param,sizeof(TIUHAL_LIN_CS_IOCTL_T));

				if(unlikely(bytes_not_copied == 0))
				{
					if(cs_data.tcid < TIUHW_MAX_TCIDS)
					{
						tiuhal_chipselect[cs_data.tcid] = cs_data.chipselect_data;
					}
					else
					{
						/* Indicate an error by chaning TCID to 0xFF */
						put_user(0xFF,&(((TIUHAL_LIN_CS_IOCTL_T *)ioctl_param)->tcid));
					}
				}
			}
			break;

		case TIUHAL_LIN_IOCTL_CONFIG:
			{
				TIUHAL_LIN_CONFIG_IOCTL_T config_data;
				config_data.version =  capabilities.version;
				config_data.interrupts_enabled = irq_support;
				bytes_not_copied = copy_to_user(__user (((TIUHAL_LIN_CONFIG_IOCTL_T*)ioctl_param)), &config_data, sizeof(TIUHAL_LIN_CONFIG_IOCTL_T));
			}	
			break;
	}

	if(unlikely((bytes_not_copied != 0) || (truncated_results != 0)))
	{
		return -ENXIO;
	}
	return 0;
}
/*******************************************************************************
 * Function:  Initialize the Telephony Hardware Abstraction module.
 *******************************************************************************
 * DESCRIPTION:  This is the entry point for the Linux TIUHAL module.
 *
 * RETURNS:      0 if successful.
 *
 ******************************************************************************/

int __init init_module(void)
{
	int result;

	tiuhal_dev.dev.name  = TIUHAL_DEVICE_NAME;
	tiuhal_dev.dev.minor = MISC_DYNAMIC_MINOR;
	tiuhal_dev.dev.fops  = &fops;

	result = misc_register(&(tiuhal_dev.dev));

	if(unlikely(result != 0))
	{
		goto register_error;
	}

	tiuhw_api = NULL;

	if(unlikely(tiuhal_board_init(&capabilities) != 0))
	{
		result = -EIO;
		goto board_init_error;
	}
	ti_codec_spi_set_zsimode(zsimode);  // ARRIS ADD for ZSI runtime
	return 0;

board_init_error:
	tiuhal_board_teardown();
register_error:
	misc_deregister(&(tiuhal_dev.dev));

	return result;
}

/*******************************************************************************
* FUNCTION:      cleanup_module
*
********************************************************************************
* DESCRIPTION:  Exit from module (normaly called part of rmmod)  
*               
*
*******************************************************************************/
void __exit cleanup_module(void)
{
	tiuhal_board_teardown();
	misc_deregister(&(tiuhal_dev.dev));
}
