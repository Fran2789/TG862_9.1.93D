/*
 *
 * dfltr_class_ctrl.c
 * Description:
 * Docsis protocol filters/QOS classifers module configuration and control implementation
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

/*! \file dfltr_class_ctrl.c
    \brief Docsis protocol filters/QOS classifers module configuration and control
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm-generic/errno-base.h>
#include <linux/uaccess.h>
#include "_tistdtypes.h"
#include "dfilter.h"
#include "qos_classifier.h"
#include "dfltr_class_ctrl.h"
#include "dfltr_class_utils.h"
#include "us_phs_verify.h"
#include "l2vpn.h"
#include <linux/ti_hil.h>

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
extern int DbridgeCommon_register_GetSysIfIndexByDocsisCB(int (*Dbridge_GetSysIfIndexByDocsisCB)(Uint32 docsisIfIndex, Uint32 *sysIfIndex));
extern int DbridgeCommon_register_GetDocsisIfIndexBySysIndexCB(int (*Dbridge_GetDocsisIfIndexBySysIndex)(Uint32 sysIfIndex, Uint32 *docsisIfIndex));
extern int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierDataCB)(struct sk_buff *skb, int *sfIndex, int *phsIndex));

#ifdef DSG
extern int DFilter_SetDsgData(int subtype, Uint32 param1, void *buf);
extern int DFilter_GetDsgData(int subtype, Uint32 param1, void *buf);
#endif
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define FC_CDEV_MIN_MINOR       0
#define FC_CDEV_NUM_DEVICES     1
#define FC_CDEV_NAME            "docsis_fltr_class"

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static long fc_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int dfc_mod_start(void);
static int dfc_mod_restart(void);
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static struct cdev *fc_cdev;
static dev_t        fc_dev_numbers;
static struct file_operations fc_cdev_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = fc_ioctl,
};
/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/
/**************************************************************************/
/*! \fn static int create_dfc_cdev(void)
 **************************************************************************
 *  \brief Create Docsis filters/classifiers kernel module character device
 *  \return  0 or error code
 */
static int create_dfc_cdev(void)
{
        /*Allocate major and minor numbers for the device*/
    if (alloc_chrdev_region(&fc_dev_numbers, FC_CDEV_MIN_MINOR, FC_CDEV_NUM_DEVICES,
                            FC_CDEV_NAME))
    {
        printk(KERN_WARNING"Failed to allocate docsis filter char device numbers\n");
        return -ENODEV;
    }
    /*Allocate the device*/
    fc_cdev = cdev_alloc();
    if (!fc_cdev) 
    {
        printk(KERN_WARNING"Failed to allocate docsis filters classifiers char device\n");
        unregister_chrdev_region(fc_dev_numbers, FC_CDEV_NUM_DEVICES);
        return -ENOMEM;
    }
    /*Init device structure*/
    fc_cdev->ops = &fc_cdev_fops;
    fc_cdev->owner = THIS_MODULE;
    /*Add the device to the kernel*/
    if (cdev_add(fc_cdev, fc_dev_numbers, FC_CDEV_NUM_DEVICES))
    {
        printk(KERN_WARNING"Failed to add docsis filters classifiers char device\n");
        kfree(fc_cdev);
        unregister_chrdev_region(fc_dev_numbers, FC_CDEV_NUM_DEVICES);
        return -ENODEV;
    }
    return 0;
}

/**************************************************************************/
/*! \fn static void delete_fc_cdev(void)
 **************************************************************************
 *  \brief Delete filters/classifiers kernel module character device
 *  \return  NA
 */
static void delete_fc_cdev(void)
{
    cdev_del(fc_cdev);
    unregister_chrdev_region(fc_dev_numbers, FC_CDEV_NUM_DEVICES);
}

/**************************************************************************/
/*! \fn static int fc_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
 **************************************************************************
 *  \brief filters/classifiers character device ioctl callback
 *  \param[in] struct file *file - device file pointer
 *  \param[in] unsigned int cmd - ioctl command id
 *  \param[in/out] unsigned long arg - pointer to input/output buffer in user space
 *  \return  0 or error code
 */
static long fc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    fc_ioctl_data_t data;
    Uint8 *buf=NULL;
    int ret = 0;

    /*
    * extract the type and number bitfields, and don't decode
    * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok( )
    */
    if (_IOC_TYPE(cmd) != FC_IOCTL_MAGIC) return -EINVAL;
    if (_IOC_NR(cmd) > FC_IOC_MAX_NUM) return -ENOTTY;

    if (copy_from_user(&data, (void __user *)arg, sizeof(fc_ioctl_data_t)))
    {
        printk("%s : failed to copy from user\n", __FUNCTION__);
        return -EFAULT;
    }

    /*Copy data buffer contents from user space for those commands that require this */
    if (data.buf && data.buf_len) 
    {
        /*allocate input and/or output buffer */
        buf = kmalloc(data.buf_len, GFP_KERNEL);
        if (!buf) {
            printk("%s : failed to allocate %d bytes of memory \n", __FUNCTION__, data.buf_len);
            return -ENOMEM;
        }
        if (copy_from_user(buf, data.buf, data.buf_len))
        {
            printk("%s : failed to copy from user\n", __FUNCTION__);
            //ARRIS MODIFY BEGIN
			kfree(buf);
            //ARRIS MODIFY END
            return -EFAULT;
        }
    }

#ifndef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
/* This flag can be set in order to do selective PP sessions deletion and not all sessions flush on any filter change event (Add,Del,Upd)
   NOTICE: Need to set this flag also in dfilter.c !!! */
    switch (cmd)
    {
        case FLTRIOC_S_LLC:
        case FLTRIOC_S_IP:
        {
            switch (data.subtype)
            {
                case FLTRIOC_S_SUBTYPE_ADD:
                {
                    ti_hil_pp_event (TI_DOCSIS_FLTR_ADD, (void *)buf);
                    break;
                }
                
                case FLTRIOC_S_SUBTYPE_DEL:
                {
                    ti_hil_pp_event (TI_DOCSIS_FLTR_DEL, (void *)buf);
                    break;
                }
                
                case FLTRIOC_S_SUBTYPE_UPD:
                case FLTRIOC_S_SUBTYPE_UNMATCH_ACT:
                case FLTRIOC_S_SUBTYPE_IPFLTR_ENABLED:
                {
                    ti_hil_pp_event (TI_DOCSIS_FLTR_CHG, (void *)buf);
                    break;
                }
            }
            
            break;
        }
    }
#endif

    /*Process the data*/
    switch (cmd) 
    {
    case FLTRIOC_S_LLC:
        ret = DFilter_SetLLCData(data.subtype, data.param1, buf);
        break;
    case FLTRIOC_G_LLC:
        ret = DFilter_GetLLCData(data.subtype, data.param1, buf);
        break;
    case FLTRIOC_S_IP:
        ret = DFilter_SetIpData(data.subtype, data.param1, buf);
         break;
    case FLTRIOC_G_IP:
        ret = DFilter_GetIpData(data.subtype, data.param1, buf);
        break;
    case CLIOC_S_CL:
        ret = QosClass_Set(data.subtype, data.param1, data.param2, buf);
        break;
    case US_PHSIOC_S_RULE:
        ret = UsPhsRules_Set(data.subtype, data.param1, data.param2, buf, data.buf_len);
        break;
    case CLIOC_G_CL:
        ret = QosClass_Get(data.subtype, data.param1, data.param2, buf);
        break;
    case FCIOC_S_IF:
        ret = FcUtils_AddSysInterface(data.param1, buf, data.param2);
        break;
    case FCIOC_G_IF:
        ret = FcUtils_GetSysIfData(data.param1, buf);
        break;
    case FCIOC_S_START:
        ret = dfc_mod_start();
        break;
    case FCIOC_S_RESTART:
        ret = dfc_mod_restart();
        break;
    case FLTRIOC_S_STP:
        ret = StpControl_Set(data.param1);
        break;
    case L2VPN_S_CL:
        ret = L2vpn_Set(data.subtype, data.param1, data.param2, buf);
        break;
    case FCIOC_R_IF:
        ret = FcUtils_ResetSysInterface();
        break;
#ifdef DSG
    case FLTRIOC_S_DSG:
        ret = DFilter_SetDsgData(data.subtype, data.param1, buf);
         break;
    case FLTRIOC_G_DSG:
        ret = DFilter_GetDsgData(data.subtype, data.param1, buf);
        break;
#endif
    default:
        printk(KERN_ERR"%s : Unknown ioctl command %d\n", __FUNCTION__,cmd);      
        //ARRIS Add Begin
        if(buf)
        {
          kfree(buf);
        }
        //ARRIS Add End
        return -EINVAL;
    }

    /*Copy kernel buffer contents to user space for those commands that require this */
    if (_IOC_DIR(cmd) &  _IOC_READ) /*Only for GET ioctls*/
    {
        if (data.buf && data.buf_len) 
        {
            if (copy_to_user(data.buf, buf, data.buf_len))
            {
                printk(KERN_ERR"%s : failed to copy to user\n", __FUNCTION__);
                //ARRIS Add Begin
                kfree(buf);
                //ARRIS Add End
                return -EFAULT;
            }
        }
    }

    if( buf )
    {
        kfree(buf);
    }

    return ret;

}

/**************************************************************************/
/*! \fn static int dfc_mod_init(void)
 **************************************************************************
 *  \brief Docsis filters/classifiers kernel module init function
 *  \return  0 or error code
 */
int dfc_mod_init(void)
{
    int ret;

    ret = create_dfc_cdev();
    if (ret)
    {
        printk(KERN_ERR "Failed to create docsis filters classifiers char device\n");
        return ret;
    }

    DFilters_Init();
    QosClass_Init();
    UsPhsRules_Init();
    L2vpn_Init();

    printk(KERN_INFO "Filter Module is up!\n");

    return 0;
}

/**************************************************************************/
/*! \fn static int dfc_mod_start(void)
 **************************************************************************
 *  \brief Start docsis filters/classifiers kernel module operation
 *  \return  0 or error code
 */
static int dfc_mod_start(void)
{
    if (DFilters_Start())
    {
        printk(KERN_ERR "Failed to start Docsis Protocol Filters operation!\n");
        return -1;
    }

    if (QosClass_Start())
    {
        printk(KERN_ERR "Failed to start Docsis Qos Classifiers operation!\n");
        return -1;
    }

    /* Register Common callback in Docsis bridge */
    if (DbridgeCommon_register_GetSysIfIndexByDocsisCB(FcUtils_GetSysIfIndexByDocsis))
    {
        printk(KERN_ERR "Failed to register to MDF callback in Docsis bridge\n");
        return -1;
    }

    if (DbridgeCommon_register_GetDocsisIfIndexBySysIndexCB(FcUtils_GetDocsisIfIndexBySysIndex))
    {
        printk(KERN_ERR "Failed to register callback in Docsis bridge\n");
        return -1;
    }
    
    
    printk(KERN_INFO "Filter Module is up!\n");

    return 0;
}

/**************************************************************************/
/*! \fn static int dfc_mod_restart(void)
 **************************************************************************
 *  \brief Restart docsis filters/classifiers kernel module operation
 *  \return  0 or error code
 */
static int dfc_mod_restart(void)
{
    if (DFilters_Cleanup())
    {
        printk(KERN_ERR "Failed to cleanup Docsis Protocol Filters module!\n");
        return 0;
    }

    if (QosClass_Cleanup())
    {
        printk(KERN_ERR "Failed to cleanup Docsis Classifiers module!\n");
        return 0;
    }
	
    /*Unregister Data classifer callback in the Docsis Bridge */
    Dbridge_register_QosClassifierDataCB(NULL);

    DFilters_ReInit();
    UsPhsRules_Init();
    L2vpn_Init();

    printk(KERN_INFO "Docsis Filters Classifiers module is restart!\n");

    return 0;
}


/**************************************************************************/
/*! \fn static void dfc_mod_exit(void)
 **************************************************************************
 *  \brief Docsis filters/classifiers kernel module cleanup function
 *  \return  NA
 */
void dfc_mod_exit(void)
{
    if (DFilters_Stop())
    {
        printk(KERN_ERR "Failed to stop Docsis Protocol Filters module!\n");
        return ;
    }

    if (DFilters_Cleanup())
    {
        printk(KERN_ERR "Failed to cleanup Docsis Protocol Filters module!\n");
        return ;
    }

    if (QosClass_Stop())
    {
        printk(KERN_ERR "Failed to stop Docsis Classifiers module!\n");
        return ;
    }

    if (QosClass_Cleanup())
    {
        printk(KERN_ERR "Failed to cleanup Docsis Classifiers module!\n");
        return ;
    }

    /* Unregister Common callback in Docsis bridge */
    if (DbridgeCommon_register_GetSysIfIndexByDocsisCB(NULL))
    {
        printk(KERN_ERR "Failed to Unregister MDF callback in Docsis bridge\n");
        return ;
    }

    if (DbridgeCommon_register_GetDocsisIfIndexBySysIndexCB(NULL))
    {
        printk(KERN_ERR "Failed to Unregister MDF callback in Docsis bridge\n");
        return ;
    }

    delete_fc_cdev();
    printk(KERN_INFO "Docsis Filters Classifiers module is down!\n");
}

module_init(dfc_mod_init);
module_exit(dfc_mod_exit);

