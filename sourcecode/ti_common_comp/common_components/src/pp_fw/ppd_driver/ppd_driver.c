/*
*
*  GPL LICENSE SUMMARY
*
*  Copyright(c) 2014 Intel Corporation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>   /* printk() */
#include <puma_autoconf.h>
#include <linux/fs.h>       /* everything... */
#include <linux/types.h>    /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <linux/uaccess.h>
#include <asm/system.h>     /* cli(), *_flags */
#include "ppd_driver.h"
#include "ramtest.h"
#include <asm-arm/arch-avalanche/generic/ti_ppd.h>
#include <linux/ti_ppm.h>
//#include <pal_cppi41.h>


#define DEVICE_NAME         "ppdDriver"
#define DEVICE_MAJOR        22

#include <asm-arm/arch-avalanche/puma5/puma5_cppi.h>
    /* CHK: Forcing discard for packets not matching any installed PID */
static TI_PPD_CONFIG ppCfg_g =
    {
        .dflt_host_rx_q         = 0                         ,
        .dflt_host_rx_dst_tag   = 0                         ,
        .host_ev_queue          = PP_HOST_EVENTQ            ,
        .host_q_mgr             = PP_HOST_EVENTQMGR         ,
    #define SYNC_TIMEOUT_MICROSEC           1000
    #define SYNC_TIMEOUT_MAXPACKET          48
        .sync_max_pkt           = SYNC_TIMEOUT_MAXPACKET    ,
        .sync_timeout_10us      = SYNC_TIMEOUT_MICROSEC/10  ,
        .buff_pool_indx         = BMGR0_POOL13
    };

extern int avalanche_prefetcher_init (void);

static long ppdDriverIOCTL ( struct file * filp , unsigned int cmd , unsigned long arg )
{
    switch ( cmd )
    {
        case( PPD_DRIVER_INIT_PPD ):
            if ( ti_ppd_init (&ppCfg_g ) == 0 )
            {
                return 0;
            }
            else
            {
                printk( "ppdDriverIOCTL: ERROR: ti_ppd_init failed!\n");
                return -1;
            }
        case ( PPD_DRIVER_INIT_PREFETCHER ):
            if (avalanche_prefetcher_init() == 0 )
            {
                return 0;
            }
            else
            {
                printk( KERN_WARNING "ppdDriverIOCTL: ERROR: avalanche_prefetcher_init faild!\n");
                return -1;
            }
    }
    return 0;
}

static int ppdDriverOpen ( struct inode *inode , struct file *filp )
{
    /* Success */
    return 0;
}

static int ppdDriverRelease ( struct inode *inode , struct file *filp )
{
    /* Success */
    return 0;
}

/* Structure that declares the usual file */
/* Access functions */
static struct file_operations ppdDriver_fops =
{
    .owner           = THIS_MODULE      ,
    .llseek          = NULL             ,
    .read            = NULL             ,
    .write           = NULL             ,
    .unlocked_ioctl  = ppdDriverIOCTL   ,
    .open            = ppdDriverOpen    ,
    .release         = ppdDriverRelease ,
};
static int ppdDriverInit ( void )
{
    printk ( KERN_INFO "Initializing ppd driver module\n");
    /* Registering device */
    if ( register_chrdev ( DEVICE_MAJOR , DEVICE_NAME , & ppdDriver_fops ) < 0 )
    {

        printk ( KERN_WARNING "memory: cannot obtain major number %d\n", DEVICE_MAJOR );
        return -EIO;
    }
    return 0;

}

static void ppdDriverExit ( void )
{
    /* Freeing the major number */
    unregister_chrdev ( DEVICE_MAJOR , DEVICE_NAME );
    printk ( KERN_INFO "Removing ppd driver module\n");
}


/* Declaration of the init and exit functions */
module_init ( ppdDriverInit );
module_exit ( ppdDriverExit );


MODULE_AUTHOR ("Intel Corporation");
MODULE_LICENSE ("GPL");
MODULE_DESCRIPTION ("Cable Modem PPD Init Driver");

