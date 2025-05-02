/*
 *
 * dpp_tunnel_ctl.c
 * Description:
 * DOCSIS Packet Processor power save mode configuration and control implementation
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

/*! \file dpp_tunnel_ctl.c
    \brief Docsis Packet Processor tunnel configuration and control
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/skbuff.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm-generic/errno-base.h>
#include <linux/uaccess.h>
#include <linux/ti_hil.h>
#include <linux/netdevice.h>
#include "dpp_ctl.h"
#include "dpp_tunnel_ctl.h"


/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/


/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define DPP_TUNNEL_CDEV_MIN_MINOR       0
#define DPP_TUNNEL_CDEV_NUM_DEVICES     1
#define DPP_TUNNEL_CDEV_NAME            "docsis_pp_tunnel"

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static int create_dpp_tunnel_cdev(void);
static void delete_dpp_tunnel_cdev(void);
static long dpp_tunnel_ioctl( struct file *filp, 
                              unsigned int cmd, unsigned long arg);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static struct cdev *dpp_tunnel_cdev;
static dev_t dpp_tunnel_dev_numbers;
static struct file_operations dpp_tunnel_cdev_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = dpp_tunnel_ioctl
};

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/


/**************************************************************************/
/*! \fn int DppTunnelCtl_Init(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor Tunnel Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DppTunnelCtl_Init(void)
{
    int ret;
    
    ret = create_dpp_tunnel_cdev();

    if (ret)
    {
        printk(KERN_ERR"\n%s: Failed to create DOCSIS Packet Processor tunnel char device\n", __FUNCTION__);
        return ret;
    }
    printk(KERN_INFO"\nDOCSIS Packet Processor tunnel control init done\n");

    return 0;
}

/**************************************************************************/
/*! \fn int DppTunnelCtl_Exit(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor Tunnel Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DppTunnelCtl_Exit(void)
{
    delete_dpp_tunnel_cdev();
    printk(KERN_INFO"\nDelete DOCSIS Packet Processor tunnel char device done\n");

    return 0;
}



/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int create_dpp_tunnel_cdev(void)
 **************************************************************************
 *  \brief Create Docsis Packet Processor tunnel kernel module 
 *   character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
static int create_dpp_tunnel_cdev(void)
{
    /* Allocate major and minor numbers for the device */
    if (alloc_chrdev_region(&dpp_tunnel_dev_numbers, DPP_TUNNEL_CDEV_MIN_MINOR, 
                            DPP_TUNNEL_CDEV_NUM_DEVICES, DPP_TUNNEL_CDEV_NAME))
    {
        printk(KERN_WARNING"\n%s: Failed to allocate docsis packet processor tunnel char device numbers\n", __FUNCTION__);
        return -ENODEV;
    }

    /* Allocate the device */
    dpp_tunnel_cdev = cdev_alloc();

    if (!dpp_tunnel_cdev) 
    {
        printk(KERN_WARNING"\n%s: Failed to allocate docsis packet processor tunnel char device\n", __FUNCTION__);
        unregister_chrdev_region(dpp_tunnel_dev_numbers, DPP_TUNNEL_CDEV_NUM_DEVICES);
        return -ENOMEM;
    }

    /* Init device structure */
    dpp_tunnel_cdev->ops = &dpp_tunnel_cdev_fops;
    dpp_tunnel_cdev->owner = THIS_MODULE;

    /* Add the device to the kernel */
    if (cdev_add(dpp_tunnel_cdev, dpp_tunnel_dev_numbers, DPP_TUNNEL_CDEV_NUM_DEVICES))
    {
        printk(KERN_WARNING"\n%s: Failed to add docsis packet processor tunnel char device\n", __FUNCTION__);
        kfree(dpp_tunnel_cdev);
        unregister_chrdev_region(dpp_tunnel_dev_numbers, DPP_TUNNEL_CDEV_NUM_DEVICES);
        return -ENODEV;
    }

    printk(KERN_INFO"\nCreate DOCSIS Packet Processor tunnel char device done\n");

    return 0;
}

/**************************************************************************/
/*! \fn static void delete_dpp_tunnel_cdev(void)
 **************************************************************************
 *  \brief Delete Docsis Packet Processor tunnel kernel module.
 *   character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return NA.
 **************************************************************************/
static void delete_dpp_tunnel_cdev(void)
{
    cdev_del(dpp_tunnel_cdev);
    unregister_chrdev_region(dpp_tunnel_dev_numbers, DPP_TUNNEL_CDEV_NUM_DEVICES);
}

/**************************************************************************/
/*! \fn static int dpp_tunnel_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
 **************************************************************************
 *  \brief Docsis Packet Processor tunnel character device ioctl callback.
 *  \param[in] struct inode *inode - device inode.
 *  \param[in] struct file *filp - device file pointer.
 *  \param[in] unsigned int cmd - ioctl command id.
 *  \param[in/out] unsigned long arg - pointer to input/output buffer in user space.
 *  \return 0 or error code.
 **************************************************************************/
static long dpp_tunnel_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
#ifdef CONFIG_INTEL_PP_TUNNEL_SUPPORT
    DppTunnelIoctlData_t data;
    Uint8 *buf = NULL;
    int ret = 0;
    DppTunnelCreateTunnel_t* tunnelCreate;
    
    /* extract the type and number bitfields, and don't decode
       wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok( ) */

    if (_IOC_TYPE(cmd) != DPP_TUNNEL_IOCTL_MAGIC) return -EINVAL;
    if (_IOC_NR(cmd) > DPP_TUNNEL_IOCTL_MAX_NUM) return -ENOTTY;

    if (copy_from_user(&data, (void __user *)arg, sizeof(DppTunnelIoctlData_t)))
    {
        printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
        return -EFAULT;
    }

    /* Copy data buffer contents from user space for those commands that require this */
    if (data.buf && data.buf_len) 
    {
        /* allocate input and/or output buffer */
        buf = kmalloc(data.buf_len, GFP_KERNEL);

        if (!buf) 
        {
            printk(KERN_ERR"\n%s: failed to allocate %d bytes of memory \n", __FUNCTION__, data.buf_len);
            return -ENOMEM;
        }

        if (copy_from_user(buf, data.buf, data.buf_len))
        {
            printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
            return -EFAULT;
        }
    }

    /* Process the data */
    switch (cmd) 
    {
    case DPP_TUNNEL_SET_TUNNEL_MODE:
        ti_hil_set_tunnel_mode(*((unsigned char*)buf));
        break;

    case DPP_TUNNEL_SET_CM_ADDRESS:
        ti_hil_set_cm_mac_address((unsigned char *)buf);
        break;

    case DPP_TUNNEL_CREATE_TUNNEL:
        tunnelCreate = (DppTunnelCreateTunnel_t*)buf;
        ret = ti_hil_create_tunnel(tunnelCreate->tunnelHeader, 
                                   tunnelCreate->tunnelHeaderLen, 
                                   tunnelCreate->l2L3HeaderLen, 
                                   tunnelCreate->tunnelType, 
                                   tunnelCreate->udpMode);
        break;

    case DPP_TUNNEL_DELETE_TUNNEL:
        ti_hil_delete_tunnel();
        break;

    default:
        printk(KERN_ERR"\n%s: Unknown ioctl command %d\n", __FUNCTION__,cmd);
        return -EINVAL;
    }

    /* Copy kernel buffer contents to user space for those commands that require this */
    if (_IOC_DIR(cmd) & _IOC_READ) /* Only for GET ioctls */
    {
        if (data.buf && data.buf_len) 
        {
            if (copy_to_user(data.buf, buf, data.buf_len))
            {
                printk(KERN_ERR"\n%s: failed to copy to user\n", __FUNCTION__);
                return -EFAULT;
            }
        }
    }

    if (buf)
    {
        kfree(buf);
    }

    return ret;
#else
    printk(KERN_ERR"\n%s: INTEL_PP_TUNNEL_SUPPORT disabled in menuconfig\n", __FUNCTION__);

    return -EFAULT;
#endif
}

