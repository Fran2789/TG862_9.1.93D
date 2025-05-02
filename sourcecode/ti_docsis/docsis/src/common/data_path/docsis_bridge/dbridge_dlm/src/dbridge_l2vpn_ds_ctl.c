/*
 *
 * dbridge_l2vpn_ds_ctl.c
 * Description:
 * DOCSIS bridge L2VPN DS configuration and control implementation
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/ 
 * 
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the   
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

/*! \file dbridge_l2vpn_ds_ctl.c
    \brief Docsis Bridge L2VPN DS Forwarding configuration and control
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/uaccess.h>
#include "dbridge_l2vpn_ds_ctl.h"
#include "dbridge_l2vpn_ds_db.h"

#ifdef CONFIG_MACH_PUMA5
#include <linux/ti_ppm.h>
#else
#include <asm-arm/arch-avalanche/generic/avalanche_pp_api.h>
#endif
/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define DBR_L2VPN_DS_CDEV_MIN_MINOR       0
#define DBR_L2VPN_DS_CDEV_NUM_DEVICES     1
#define DBR_L2VPN_DS_CDEV_NAME            "docsis_dbridge_l2vpn_ds"

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static int create_dbridge_l2vpn_ds_cdev(void);
static void delete_dbridge_l2vpn_ds_cdev(void);
static long dbridge_l2vpn_ds_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static struct cdev *dbridge_l2vpn_ds_cdev;
static dev_t dbridge_l2vpn_ds_dev_numbers;
static struct file_operations dbridge_l2vpn_ds_cdev_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = dbridge_l2vpn_ds_ioctl,
};

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DbridgeL2vpnDsCtl_Init(void)
 **************************************************************************
 *  \brief DOCSIS Bridge L2VPN DS Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DbridgeL2vpnDsCtl_Init(void)
{
    int ret;

    ret = create_dbridge_l2vpn_ds_cdev();

    if (ret)
    {
        printk(KERN_ERR "Failed to create docsis dbridge L2VPN DS char device\n");
        return ret;
    }

    printk(KERN_INFO "Dbridge L2VPN DS control init done\n");
        
    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeL2vpnDsCtl_Exit(void)
 **************************************************************************
 *  \brief DOCSIS Bridge L2VPN DS Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DbridgeL2vpnDsCtl_Exit(void)
{
    delete_dbridge_l2vpn_ds_cdev();

    printk(KERN_INFO "Dbridge L2VPN DS control exit done\n");
    
    return 0;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int create_dbridge_l2vpn_ds_cdev(void)
 **************************************************************************
 *  \brief Create Docsis Bridge L2VPN DS Forwarding kernel module character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
static int create_dbridge_l2vpn_ds_cdev(void)
{
    /* Allocate major and minor numbers for the device */
    if (alloc_chrdev_region(&dbridge_l2vpn_ds_dev_numbers, DBR_L2VPN_DS_CDEV_MIN_MINOR, 
                            DBR_L2VPN_DS_CDEV_NUM_DEVICES, DBR_L2VPN_DS_CDEV_NAME))
    {
        printk(KERN_WARNING "Failed to allocate docsis L2VPN DS char device numbers\n");
        return -ENODEV;
    }

    /* Allocate the device */
    dbridge_l2vpn_ds_cdev = cdev_alloc();

    if (!dbridge_l2vpn_ds_cdev) 
    {
        printk(KERN_WARNING "Failed to allocate docsis dbridge L2VPN DS char device\n");
        unregister_chrdev_region(dbridge_l2vpn_ds_dev_numbers, DBR_L2VPN_DS_CDEV_NUM_DEVICES);
        return -ENOMEM;
    }

    /* Init device structure */
    dbridge_l2vpn_ds_cdev->ops = &dbridge_l2vpn_ds_cdev_fops;
    dbridge_l2vpn_ds_cdev->owner = THIS_MODULE;

    /* Add the device to the kernel */
    if (cdev_add(dbridge_l2vpn_ds_cdev, dbridge_l2vpn_ds_dev_numbers, DBR_L2VPN_DS_CDEV_NUM_DEVICES))
    {
        printk(KERN_WARNING"\nFailed to add docsis dbridge L2VPN DS char device\n");
        kfree(dbridge_l2vpn_ds_cdev);
        unregister_chrdev_region(dbridge_l2vpn_ds_dev_numbers, DBR_L2VPN_DS_CDEV_NUM_DEVICES);
        return -ENODEV;
    }

    return 0;
}

/**************************************************************************/
/*! \fn static void delete_dbridge_l2vpn_ds_cdev(void)
 **************************************************************************
 *  \brief Delete Docsis Bridge Multicast DSID Forwarding kernel module. 
 *   character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return NA.
 **************************************************************************/
static void delete_dbridge_l2vpn_ds_cdev(void)
{
    cdev_del(dbridge_l2vpn_ds_cdev);
    unregister_chrdev_region(dbridge_l2vpn_ds_dev_numbers, DBR_L2VPN_DS_CDEV_NUM_DEVICES);
}

/**************************************************************************/
/*! \fn static int dbridge_l2vpn_ds_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
 **************************************************************************
 *  \brief Docsis Bridge L2VPN DS Forwarding character device ioctl callback.
 *  \param[in] struct inode *inode - device inode.
 *  \param[in] struct file *filp - device file pointer.
 *  \param[in] unsigned int cmd - ioctl command id.
 *  \param[in/out] unsigned long arg - pointer to input/output buffer in user space.
 *  \return 0 or error code.
 **************************************************************************/
static long dbridge_l2vpn_ds_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    DbridgeL2vpnDsIoctlData_t data;
    Uint8 *buf = NULL;
    int ret = 0;

    /* extract the type and number bitfields, and don't decode
       wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok( ) */

    if (_IOC_TYPE(cmd) != DBR_L2VPN_DS_IOCTL_MAGIC) return -EINVAL;
    if (_IOC_NR(cmd) > DBR_L2VPN_DS_IOCTL_MAX_NUM) return -ENOTTY;

    if (copy_from_user(&data, (void __user *)arg, sizeof(DbridgeL2vpnDsIoctlData_t)))
    {
        printk(KERN_ERR"\n%s : failed to copy from user\n", __FUNCTION__);
        return -EFAULT;
    }

    /* Copy data buffer contents from user space for those commands that require this */
    if (data.buf && data.buf_len) 
    {
        /* allocate input and/or output buffer */
        buf = kmalloc(data.buf_len, GFP_KERNEL);

        if (!buf) 
        {
            printk(KERN_ERR"\n%s : failed to allocate %d bytes of memory \n", __FUNCTION__, data.buf_len);
            return -ENOMEM;
        }

        if (copy_from_user(buf, data.buf, data.buf_len))
        {
            printk(KERN_ERR"\n%s : failed to copy from user\n", __FUNCTION__);
            //ARRIS Add Begin
            kfree(buf);
            //ARRIS Add End
            return -EFAULT;
        }
    }

    /* Process the data */
    switch (cmd) 
    {
    case L2VPN_DS_IOCTL_SET_SAID:
        ret = DbridgeL2vpnDsDb_SetSaid(data.subtype, data.param1);
#ifdef CONFIG_MACH_PUMA5
        ti_ppm_flush_sessions(-1);
#else
        avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL );
#endif
        break;
    case L2VPN_DS_IOCTL_DEL_SAID:
        ret = DbridgeL2vpnDsDb_DelSaid(data.subtype);
        break;
    case L2VPN_DS_IOCTL_SET_DUT:
        ret = DbridgeL2vpnDsDb_SetDut(data.subtype, data.param1);
#ifdef CONFIG_MACH_PUMA5
        ti_ppm_flush_sessions(-1);
#else
        avalanche_pp_flush_sessions( AVALANCHE_PP_MAX_VPID, PP_LIST_ID_ALL );
#endif
        break;
    case L2VPN_DS_IOCTL_PRINT_L2VPN_DS_DB:
        ret = DbridgeL2vpnDsDb_Print();
        break;
    default:
        printk(KERN_ERR "%s : Unknown ioctl command %d\n", __FUNCTION__, cmd);
        //ARRIS Add Begin
        if(buf)
        {
           kfree(buf);
        }
        //ARRIS Add End
        return -EINVAL;
    }

    /* Copy kernel buffer contents to user space for those commands that require this */
    if (_IOC_DIR(cmd) & _IOC_READ) /* Only for GET ioctls */
    {
        if (data.buf && data.buf_len) 
        {
            if (copy_to_user(data.buf, buf, data.buf_len))
            {
                printk(KERN_ERR "%s : failed to copy to user\n", __FUNCTION__);
                //ARRIS Add Begin
                kfree(buf);
                //ARRIS Add End
                return -EFAULT;
            }
        }
    }

    if (buf)
    {
        kfree(buf);
    }

    return ret;
}
