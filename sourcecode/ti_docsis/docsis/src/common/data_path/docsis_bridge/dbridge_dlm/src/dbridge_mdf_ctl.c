/*
 *
 * dbridge_mdf_ctl.c
 * Description:
 * DOCSIS bridge Multicast DSID Forwarding configuration and control implementation
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

/*! \file dbridge_mdf_ctl.c
    \brief Docsis Bridge Multicast DSID Forwarding configuration and control
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
#include <linux/uaccess.h>
#include <asm-generic/errno-base.h>
#include <linux/ti_hil.h>
#include "dbridge_mdf_ctl.h"
#include "dbridge_mdf_db.h"



/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define DBR_MDF_CDEV_MIN_MINOR       0
#define DBR_MDF_CDEV_NUM_DEVICES     1
#define DBR_MDF_CDEV_NAME            "docsis_dbridge_mdf"

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static int create_dbridge_mdf_cdev(void);
static void delete_dbridge_mdf_cdev(void);
static long dbridge_mdf_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static struct cdev *dbridge_mdf_cdev;
static dev_t dbridge_mdf_dev_numbers;
static struct file_operations dbridge_mdf_cdev_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = dbridge_mdf_ioctl,
};

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DbridgeMdfCtl_Init(void)
 **************************************************************************
 *  \brief DOCSIS Bridge MDF Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DbridgeMdfCtl_Init(void)
{
    int ret;

    ret = create_dbridge_mdf_cdev();

    if (ret)
    {
        printk(KERN_ERR"\nFailed to create docsis dbridge MDF char device\n");
        return ret;
    }

    printk(KERN_INFO"\nDbridge MDF control init done\n");
        
    return 0;
}

/**************************************************************************/
/*! \fn int DbridgeMdfCtl_Exit(void)
 **************************************************************************
 *  \brief DOCSIS Bridge MDF Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DbridgeMdfCtl_Exit(void)
{
    delete_dbridge_mdf_cdev();

    printk(KERN_INFO"\nDbridge MDF control exit done\n");
    
    return 0;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int create_dbridge_mdf_cdev(void)
 **************************************************************************
 *  \brief Create Docsis Bridge Multicast DSID Forwarding kernel module. 
 *   character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
static int create_dbridge_mdf_cdev(void)
{
    /* Allocate major and minor numbers for the device */
    if (alloc_chrdev_region(&dbridge_mdf_dev_numbers, DBR_MDF_CDEV_MIN_MINOR, 
                            DBR_MDF_CDEV_NUM_DEVICES, DBR_MDF_CDEV_NAME))
    {
        printk(KERN_WARNING"\nFailed to allocate docsis dbridge MDF char device numbers\n");
        return -ENODEV;
    }

    /* Allocate the device */
    dbridge_mdf_cdev = cdev_alloc();

    if (!dbridge_mdf_cdev) 
    {
        printk(KERN_WARNING"\nFailed to allocate docsis dbridge MDF char device\n");
        unregister_chrdev_region(dbridge_mdf_dev_numbers, DBR_MDF_CDEV_NUM_DEVICES);
        return -ENOMEM;
    }

    /* Init device structure */
    dbridge_mdf_cdev->ops = &dbridge_mdf_cdev_fops;
    dbridge_mdf_cdev->owner = THIS_MODULE;

    /* Add the device to the kernel */
    if (cdev_add(dbridge_mdf_cdev, dbridge_mdf_dev_numbers, DBR_MDF_CDEV_NUM_DEVICES))
    {
        printk(KERN_WARNING"\nFailed to add docsis dbridge MDF char device\n");
        kfree(dbridge_mdf_cdev);
        unregister_chrdev_region(dbridge_mdf_dev_numbers, DBR_MDF_CDEV_NUM_DEVICES);
        return -ENODEV;
    }

    return 0;
}

/**************************************************************************/
/*! \fn static void delete_dbridge_mdf_cdev(void)
 **************************************************************************
 *  \brief Delete Docsis Bridge Multicast DSID Forwarding kernel module. 
 *   character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return NA.
 **************************************************************************/
static void delete_dbridge_mdf_cdev(void)
{
    cdev_del(dbridge_mdf_cdev);
    unregister_chrdev_region(dbridge_mdf_dev_numbers, DBR_MDF_CDEV_NUM_DEVICES);
}

/**************************************************************************/
/*! \fn static int dbridge_mdf_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
 **************************************************************************
 *  \brief Docsis Bridge Multicast DSID Forwarding character device ioctl callback.
 *  \param[in] struct inode *inode - device inode.
 *  \param[in] struct file *filp - device file pointer.
 *  \param[in] unsigned int cmd - ioctl command id.
 *  \param[in/out] unsigned long arg - pointer to input/output buffer in user space.
 *  \return 0 or error code.
 **************************************************************************/
static long dbridge_mdf_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    DbridgeMdfIoctlData_t data;
    Uint8 *buf = NULL;
    int ret = 0;

    /* extract the type and number bitfields, and don't decode
       wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok( ) */

    if (_IOC_TYPE(cmd) != DBR_MDF_IOCTL_MAGIC) return -EINVAL;
    if (_IOC_NR(cmd) > DBR_MDF_IOCTL_MAX_NUM) return -ENOTTY;

    if (copy_from_user(&data, (void __user *)arg, sizeof(DbridgeMdfIoctlData_t)))
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
    case MDF_IOCTL_S_MODE:
        ret = DbridgeMdfDb_SetMdfMode(data.subtype, data.param1);
        break;
    case MDF_IOCTL_S_PREREGDSID:
        ret = DbridgeMdfDb_SetPreRegDsid(data.subtype, data.param1);
        break;
    case MDF_IOCTL_S_MCASTDSID:
        ret = DbridgeMdfDb_SetMcastDsid(data.subtype, data.param1, buf);
        ti_hil_pp_event (TI_DOCSIS_DSID_CHG, (void *)buf);
        break;
    case MDF_IOCTL_G_MCASTDSIDFWDCMIM:
        ret = DbridgeMdfDb_GetMcastDocsisClientCmim(data.subtype, data.param1, buf);
        break;
    case MDF_IOCTL_G_MCASTDSIDFLOODMAP:
        ret = DbridgeMdfDb_GetMcastFloodMap(data.subtype, data.param1, buf);
        break;
    case MDF_IOCTL_G_MCASTDSIDCONF:
        ret = DbridgeMdfDb_GetMcastDsidConfCode(data.subtype, data.param1, buf);
        break;
    case MDF_IOCTL_G_ALTGETMEMBEREXIST:
        ret = DbridgeMdfDb_GetAltMemeberExist(data.subtype, buf);
        break;
    case MDF_IOCTL_S_MCASTDSIDCONF:
        ret = DbridgeMdfDb_SetMcastDsidConfCode(data.subtype, data.param1);
        break;
    case MDF_IOCTL_S_PRINTDBRIDGEMDFDB:
        ret = DbridgeMdfDb_Print();
        break;
    default:
        printk(KERN_ERR"\n%s : Unknown ioctl command %d\n", __FUNCTION__,cmd);
        //ARRIS Add Begin
        if (buf)
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
                printk(KERN_ERR"\n%s : failed to copy to user\n", __FUNCTION__);
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
