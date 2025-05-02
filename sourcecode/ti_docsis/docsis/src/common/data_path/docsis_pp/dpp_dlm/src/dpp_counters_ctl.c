/*
 *
 * dpp_counters_ctl.c
 * Description:
 * DOCSIS Packet Processor counters configuration and control implementation
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

/*! \file dpp_counters_ctl.c
    \brief Docsis Packet Processor counters configuration and control
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
#include "dpp_counters_ctl.h"
#include "dpp_counters_db.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

#ifdef DPP_LOG
extern unsigned int dppDbgLevel;
#endif

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define DPP_COUNTERS_CDEV_MIN_MINOR       0
#define DPP_COUNTERS_CDEV_NUM_DEVICES     1
#define DPP_COUNTERS_CDEV_NAME            "docsis_pp_counters"

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
static int create_dpp_counters_cdev(void);
static void delete_dpp_counters_cdev(void);
static long dpp_counters_ioctl(struct file *filp, 
                              unsigned int cmd, unsigned long arg);
static int DppCountersCtl_StartSessionNotification(unsigned int sessionHandle, 
                                                   unsigned int sessionType,
                                                   struct sk_buff* skb);

static int DppCountersCtl_DeleteSessionNotification(unsigned int sessionHandle, 
                                                    unsigned int sessionPacketsFw,
													unsigned long long sessionOctetsFw);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static struct cdev *dpp_counters_cdev;
static dev_t dpp_counters_dev_numbers;
static struct file_operations dpp_counters_cdev_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = dpp_counters_ioctl,
};

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DppCountersCtl_Init(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor counters Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DppCountersCtl_Init(void)
{
    int ret;

    ret = create_dpp_counters_cdev();

    if (ret)
    {
        printk(KERN_ERR"\n%s: Failed to create DOCSIS Packet Processor counters char device\n", __FUNCTION__);
        return ret;
    }
    printk(KERN_INFO"\nCreate DOCSIS Packet Processor counters char device done\n");

    /* Docsis Packet Processor counters data base initialization */
    DppCountersDb_Init();

#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
    /* Register Docsis packet processor start session notification callback in HIL */
    if (ti_hil_register_start_session_notification(DppCountersCtl_StartSessionNotification))
    {
        printk(KERN_ERR"\n%s: Failed to register Docsis packet processor start session notification callback in HIL\n",
               __FUNCTION__);
        return -1;
    }

    /* Register Docsis packet processor delete session notification callback in HIL */
    if (ti_hil_register_delete_session_notification(DppCountersCtl_DeleteSessionNotification))
    {
        printk(KERN_ERR"\n%s: Failed to register Docsis packet processor start session notification callback in HIL\n",
               __FUNCTION__);
        return -1;
    }
#endif

    printk(KERN_INFO"\nDOCSIS Packet Processor counters control init done\n");
        
    return 0;
}

/**************************************************************************/
/*! \fn int DppCountersCtl_Exit(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor counters Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DppCountersCtl_Exit(void)
{
    delete_dpp_counters_cdev();
    printk(KERN_INFO"\nDelete DOCSIS Packet Processor counters char device done\n");

#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
    /* Un-Register Docsis packet processor start session notification callback in HIL */
    if (ti_hil_unregister_start_session_notification())
    {
        printk(KERN_ERR"\n%s: Failed to un-register Docsis packet processor start session notification callback in HIL\n",
               __FUNCTION__);
        return -1;
    }

    /* Un-Register Docsis packet processor start session notification callback in HIL */
    if (ti_hil_unregister_delete_session_notification())
    {
        printk(KERN_ERR"\n%s: Failed to un-register Docsis packet processor start session notification callback in HIL\n",
               __FUNCTION__);
        return -1;
    }
#endif

    printk(KERN_INFO"\nDOCSIS Packet Processor counters control exit done\n");
    
    return 0;
}


/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int create_dpp_counters_cdev(void)
 **************************************************************************
 *  \brief Create Docsis Packet Processor counters kernel module 
 *   character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
static int create_dpp_counters_cdev(void)
{
    /* Allocate major and minor numbers for the device */
    if (alloc_chrdev_region(&dpp_counters_dev_numbers, DPP_COUNTERS_CDEV_MIN_MINOR, 
                            DPP_COUNTERS_CDEV_NUM_DEVICES, DPP_COUNTERS_CDEV_NAME))
    {
        printk(KERN_WARNING"\n%s: Failed to allocate docsis packet processor counters char device numbers\n", __FUNCTION__);
        return -ENODEV;
    }

    /* Allocate the device */
    dpp_counters_cdev = cdev_alloc();

    if (!dpp_counters_cdev) 
    {
        printk(KERN_WARNING"\n%s: Failed to allocate docsis packet processor counters char device\n", __FUNCTION__);
        unregister_chrdev_region(dpp_counters_dev_numbers, DPP_COUNTERS_CDEV_NUM_DEVICES);
        return -ENOMEM;
    }

    /* Init device structure */
    dpp_counters_cdev->ops = &dpp_counters_cdev_fops;
    dpp_counters_cdev->owner = THIS_MODULE;

    /* Add the device to the kernel */
    if (cdev_add(dpp_counters_cdev, dpp_counters_dev_numbers, DPP_COUNTERS_CDEV_NUM_DEVICES))
    {
        printk(KERN_WARNING"\n%s: Failed to add docsis packet processor counters char device\n", __FUNCTION__);
        kfree(dpp_counters_cdev);
        unregister_chrdev_region(dpp_counters_dev_numbers, DPP_COUNTERS_CDEV_NUM_DEVICES);
        return -ENODEV;
    }

    return 0;
}

/**************************************************************************/
/*! \fn static void delete_dpp_counters_cdev(void)
 **************************************************************************
 *  \brief Delete Docsis Packet Processor counters kernel module.
 *   character device.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return NA.
 **************************************************************************/
static void delete_dpp_counters_cdev(void)
{
    cdev_del(dpp_counters_cdev);
    unregister_chrdev_region(dpp_counters_dev_numbers, DPP_COUNTERS_CDEV_NUM_DEVICES);
}

/**************************************************************************/
/*! \fn static int dpp_counters_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
 **************************************************************************
 *  \brief Docsis Packet Processor counters character device ioctl callback.
 *  \param[in] struct inode *inode - device inode.
 *  \param[in] struct file *filp - device file pointer.
 *  \param[in] unsigned int cmd - ioctl command id.
 *  \param[in/out] unsigned long arg - pointer to input/output buffer in user space.
 *  \return 0 or error code.
 **************************************************************************/
static long dpp_counters_ioctl( struct file *filp, unsigned int cmd, unsigned long arg)
{
    DppCountersIoctlData_t data;
    Uint8 *buf = NULL;
    int ret = 0;

    /* extract the type and number bitfields, and don't decode
       wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok( ) */

    if (_IOC_TYPE(cmd) != DPP_COUNTERS_IOCTL_MAGIC) return -EINVAL;
    if (_IOC_NR(cmd) > DPP_COUNTERS_IOCTL_MAX_NUM) return -ENOTTY;

    if (copy_from_user(&data, (void __user *)arg, sizeof(DppCountersIoctlData_t)))
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
            //ARRIS Add Begin
            kfree(buf);
            //ARRIS Add End
            return -EFAULT;
        }
    }

    /* Process the data */
    switch (cmd) 
    {
    case DPP_COUNTERS_G_DEV:
        ret = DppCountersDb_GetIfCounters((DppIfCounters_t*)buf);
        break;
    case DPP_COUNTERS_G_TPPORT:
        ret = DppCountersDb_GetTpPortCounters((DppTpPortCounters_t*)buf);
        break;
    case DPP_COUNTERS_G_SF:
        ret = DppCountersDb_GetSfCounters((DppSfCounters_t*)buf);
        break;
    case DPP_COUNTERS_P_TPPORT:
        ret = DppCountersDb_PrintTpPortCounters();
        break;
    case DPP_COUNTERS_P_DEV:
        ret = DppCountersDb_PrintIfCounters();
        break;
    default:
        printk(KERN_ERR"\n%s: Unknown ioctl command %d\n", __FUNCTION__,cmd);
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
                printk(KERN_ERR"\n%s: failed to copy to user\n", __FUNCTION__);
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


/**************************************************************************/
/*! \fn static int DppCountersDb_StartSessionNotification(unsigned int sessionHandle, unsigned int sessionType,
                                                          struct sk_buff* skb)
 **************************************************************************
 *  \brief DOCSIS Packet Processor counters start session notification.
 *   Updates Docsis PP data structures to reflect the necessary 
 *   associations between the new session and all the clients it feeds 
 *  (filter(s), classifier, ingress port).
 *   To be called by HIL upon session creation.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionType - PP session type.
 *  \param[in] skb - packet buffer.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
static int DppCountersCtl_StartSessionNotification(unsigned int sessionHandle, unsigned int sessionType,
                                                   struct sk_buff* skb)
{
    int res = 0;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk("\n%s: ***************************** START ******************************************\n", __FUNCTION__);
        printk("\n%s: start Session %d notification.\n", __FUNCTION__, sessionHandle);
    }
#endif

#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS

    if (skb)
    {
        res = DppCountersDb_CreateSession((Uint32)sessionHandle, (Uint8)sessionType, 
                                          (Uint32)(skb->skb_iif), 
                                          skb->pp_packet_info.ti_match_llc_filter, 
                                          skb->pp_packet_info.ti_match_inbound_ip_filter,
                                          skb->pp_packet_info.ti_match_outbound_ip_filter,
                                          skb->pp_packet_info.ti_match_qos_classifier,
										  skb->pp_packet_info.ti_match_dsg_filter);   
    }

#endif

    if (!res)
    {  
        #ifdef DPP_LOG
        if(dppDbgLevel & DPP_DBG_COUNTERS)
        {
            printk(KERN_INFO"\n%s: DOCSIS Packet Processor start Session %d Notification done.\n", 
                   __FUNCTION__, sessionHandle);
            printk("\n%s: ***************************** END ********************************************\n", __FUNCTION__);
        }
        #endif
    }
    else
    {
        printk(KERN_ERR"\n%s: DOCSIS Packet Processor start Session %d Notification failed.\n", 
               __FUNCTION__, sessionHandle);
    }

    return res;
}

/**************************************************************************/
/*! \fn static int DppCountersDb_DeleteSessionNotification(unsigned int sessionHandle, 
                                                           unsigned int sessionPacketsFw,
														   unsigned long long sessionOctetsFw)
 **************************************************************************
 *  \brief DOCSIS Packet Processor counters delete session notification.
 *   The Docsis PP uses the deleted session forwarded packets counter to 
 *   increment the relevant counter(s) fed by the session. 
 *   Then it removes all existing associations between the deleted session 
 *   and all the clients it feeds (filter(s), classifier, ingress port). 
 *   To be called by HIL upon session deletion.
 *  \param[in] sessionHandle - PP session handle number.
 *  \param[in] sessionPacketsFw - PP session forwarded packets number.
 *  \param[in] sessionOctetsFw - PP session forwarded bytes number.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
static int DppCountersCtl_DeleteSessionNotification(unsigned int sessionHandle, 
                                                    unsigned int sessionPacketsFw,
													unsigned long long sessionOctetsFw)
{
    int res = 0;

#ifdef DPP_LOG
    if(dppDbgLevel & DPP_DBG_COUNTERS)
    {
        printk("\n%s: ***************************** START ******************************************\n", __FUNCTION__);
        printk("\n%s: delete Session %d notification, sessionPacketsFw %d.\n", 
               __FUNCTION__, sessionHandle, sessionPacketsFw);
    }
#endif

    res = DppCountersDb_DeleteSession(sessionHandle, sessionPacketsFw, sessionOctetsFw);

    if (!res)
    {  
        #ifdef DPP_LOG
        if(dppDbgLevel & DPP_DBG_COUNTERS)
        {
            printk(KERN_INFO"\n%s: DOCSIS Packet Processor delete Session %d Notification done.\n", 
                   __FUNCTION__, sessionHandle);
            printk("\n%s: ***************************** END ********************************************\n", __FUNCTION__);
        }
        #endif
    }
    else
    {
        printk(KERN_ERR"\n%s: DOCSIS Packet Processor delete Session %d Notification failed.\n", 
               __FUNCTION__, sessionHandle);
    }

    return res;
}
