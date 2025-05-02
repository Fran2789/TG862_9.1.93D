/*
 *
 * gw_parental_control_ctl.c
 * Description:
 * GW parental control implementation
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

/*! \file gw_parental_control_ctl.c
    \brief GW parental control
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
#include "gw_parental_control_ctl.h"
#include "gw_parental_control_db.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define GW_PARENTAL_CONTROL_CDEV_MIN_MINOR       0
#define GW_PARENTAL_CONTROL_CDEV_NUM_DEVICES     1
#define GW_PARENTAL_CONTROL_CDEV_NAME            "gw_parental_control"

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static int create_gw_parental_control_cdev(void);
static void delete_gw_parental_control_cdev(void);
static long gw_parental_control_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static struct cdev *gw_parental_control_cdev;
static dev_t gw_parental_control_dev_numbers;
static struct file_operations gw_parental_control_cdev_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = gw_parental_control_ioctl,
};

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int GwParentalControlCtl_Init(void)
 **************************************************************************
 *  \brief GW parental control Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int GwParentalControlCtl_Init(void)
{
    int ret;

    ret = create_gw_parental_control_cdev();

    if (ret)
    {
        printk(KERN_ERR "Failed to create GW Parental Control char device\n");
        return ret;
    }

	GwParentalControlDb_Init();

    printk(KERN_INFO "GW Parental Control init done\n");
        
    return 0;
}

/**************************************************************************/
/*! \fn int GwParentalControlCtl_Exit(void)
 **************************************************************************
 *  \brief GW parental control Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
void GwParentalControlCtl_Exit(void)
{
    delete_gw_parental_control_cdev();

    printk(KERN_INFO "GW Parental Control exit done\n");

}

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int create_gw_parental_control_cdev(void)
 **************************************************************************
 *  \brief Create GW parental control kernel module character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
static int create_gw_parental_control_cdev(void)
{
    /* Allocate major and minor numbers for the device */
    if (alloc_chrdev_region(&gw_parental_control_dev_numbers, GW_PARENTAL_CONTROL_CDEV_MIN_MINOR, 
                            GW_PARENTAL_CONTROL_CDEV_NUM_DEVICES, GW_PARENTAL_CONTROL_CDEV_NAME))
    {
        printk(KERN_ERR "Failed to allocate GW Parental Control char device numbers\n");
        return -ENODEV;
    }

    /* Allocate the device */
    gw_parental_control_cdev = cdev_alloc();

    if (!gw_parental_control_cdev) 
    {
        printk(KERN_ERR "Failed to allocate GW Parental Control char device\n");
        unregister_chrdev_region(gw_parental_control_dev_numbers, GW_PARENTAL_CONTROL_CDEV_NUM_DEVICES);
        return -ENOMEM;
    }

    /* Init device structure */
    gw_parental_control_cdev->ops = &gw_parental_control_cdev_fops;
    gw_parental_control_cdev->owner = THIS_MODULE;

    /* Add the device to the kernel */
    if (cdev_add(gw_parental_control_cdev, gw_parental_control_dev_numbers, GW_PARENTAL_CONTROL_CDEV_NUM_DEVICES))
    {
        printk(KERN_ERR "\nFailed to add GW Parental Control char device\n");
        kfree(gw_parental_control_cdev);
        unregister_chrdev_region(gw_parental_control_dev_numbers, GW_PARENTAL_CONTROL_CDEV_NUM_DEVICES);
        return -ENODEV;
    }

    return 0;
}

/**************************************************************************/
/*! \fn static void delete_gw_parental_control_cdev(void)
 **************************************************************************
 *  \brief Delete GW parental control kernel module character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return NA.
 **************************************************************************/
static void delete_gw_parental_control_cdev(void)
{
    cdev_del(gw_parental_control_cdev);
    unregister_chrdev_region(gw_parental_control_dev_numbers, GW_PARENTAL_CONTROL_CDEV_NUM_DEVICES);
}

/**************************************************************************/
/*! \fn static int gw_parental_control_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
 **************************************************************************
 *  \brief GW parental control character device ioctl callback.
 *  \param[in] struct inode *inode - device inode.
 *  \param[in] struct file *filp - device file pointer.
 *  \param[in] unsigned int cmd - ioctl command id.
 *  \param[in/out] unsigned long arg - pointer to input/output buffer in user space.
 *  \return 0 or error code.
 **************************************************************************/
static long gw_parental_control_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    GwParentalControlIoctlData_t data;
    Uint8 *buf = NULL;
    int ret = 0;

    /* extract the type and number bitfields, and don't decode
       wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok( ) */

    if (_IOC_TYPE(cmd) != GW_PARENTAL_CONTROL_IOCTL_MAGIC) return -EINVAL;
    if (_IOC_NR(cmd) > GW_PARENTAL_CONTROL_IOCTL_MAX_NUM) return -ENOTTY;

    if (copy_from_user(&data, (void __user *)arg, sizeof(GwParentalControlIoctlData_t)))
    {
        printk(KERN_ERR "\n%s : failed to copy from user\n", __FUNCTION__);
        return -EFAULT;
    }

    /* Copy data buffer contents from user space for those commands that require this */
    if (data.buf && data.buf_len) 
    {
        /* allocate input and/or output buffer */
        buf = kmalloc(data.buf_len, GFP_KERNEL);

        if (!buf) 
        {
            printk(KERN_ERR "\n%s : failed to allocate %d bytes of memory \n", __FUNCTION__, data.buf_len);
            return -ENOMEM;
        }

        if (copy_from_user(buf, data.buf, data.buf_len))
        {
            printk(KERN_ERR "\n%s : failed to copy from user\n", __FUNCTION__);
            return -EFAULT;
        }
    }

    /* Process the data */
    switch (cmd) 
    {
    case PARENTAL_CONTROL_IOCTL_SET_MAC:
        ret = GwParentalControlDb_AddMac(buf, data.buf_len);
        break;
    case PARENTAL_CONTROL_IOCTL_DEL_MAC:
        ret = GwParentalControlDb_DelMac(buf, data.buf_len);
        break;
    case PARENTAL_CONTROL_IOCTL_FLUSH_MAC:
        ret = GwParentalControlDb_FlushAllMac();
        break;
    case PARENTAL_CONTROL_IOCTL_PRINT_DB:
        ret = GwParentalControlDb_Print();
        break;
    case PARENTAL_CONTROL_IOCTL_SET_ENABLE_STATUS:
        ret = GwParentalControlDb_SetEnableStatus(data.param1, buf, data.buf_len);
        break;
	case PC_CPE_MAC_FILTERING_IOCTL_SET_MAC:
        ret = GwCpeMacFilteringDb_AddMac(buf, data.buf_len);
        break;
	case PC_CPE_MAC_FILTERING_IOCTL_DEL_MAC:
        ret = GwCpeMacFilteringDb_DelMac(buf, data.buf_len);
		break;
	case PC_CPE_MAC_FILTERING_IOCTL_FLUSH_MAC:
        ret = GwCpeMacFilteringDb_FlushAllMac();
		break;
	case PC_CPE_MAC_FILTERING_IOCTL_PRINT_DB:
        ret = GwCpeMacFilteringDb_Print();
		break;
    default:
        printk(KERN_ERR "%s : Unknown ioctl command %d\n", __FUNCTION__, cmd);
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


module_init(GwParentalControlCtl_Init)
module_exit(GwParentalControlCtl_Exit)
MODULE_LICENSE("GPL");
