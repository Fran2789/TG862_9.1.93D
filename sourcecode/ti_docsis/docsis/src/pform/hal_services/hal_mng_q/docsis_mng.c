/*

   docsis_mng.c
   Description:
   DOCSIS management driver implementation


  GPL LICENSE SUMMARY

  Copyright(c) 2011-2013 Intel Corporation.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:
    Intel Corporation
    2200 Mission College Blvd.
    Santa Clara, CA  97052

*/
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <puma_autoconf.h>

#include "_tistdtypes.h"
#include "pal.h"
#include "pal_cppi41.h"
#include "pal_osWait.h"
#include "docsis_mng.h"
#include "mac_mngt_msg.h"

#if PUMA6_OR_NEWER_SOC_TYPE
#include <puma6_cppi.h>
#else
#include <puma5_cppi.h>
#endif


#define DOCSIS_MNG_MAJOR 32
#define DOCSIS_MNG_NAME "docsis_mng"

/* accumulator defines */
#define DOCSIS_MNG_ACC_ENTRY_TYPE   PAL_CPPI41_ACC_ENTRY_TYPE_D
#define DOCSIS_MNG_ACC_MAXPAGENTRY  4
#define DOCSIS_MNG_ACC_ENTRY_SIZE   4 /* 4 bytes pointer */

#define DOCSIS_MNG_ACC_INTR_MODE    2
#define DOCSIS_MNG_ACC_INTR_DELAY   0
#define DOCSIS_MNG_ACC_STALL_AVOID  0
#define DOCSIS_MNG_ACC_TERM_MODE    0
#define DOCSIS_MNG_ACC_PAGE_COUNT   2

/* Protocol specific bits - US */
#define US_PSI_PRIVACY_OFF_BIT_MASK        0x00008000 /* bit 15 */
#define US_PSI_SFINDEX_MASK                0xFF000000 /* bits 31-24 */
#define US_PSI_SFINDEX_SHIFT               24

/* Protocol specific bits - DS */
#define DS_PROTOCOL_SPECIFIC_TS_PRESENT    0x00000020 /* bit 5 */
#define DS_PROTOCOL_SPECIFIC_TS_SIZE       4          /* Timestamp size in bytes */
#define DS_PROTOCOL_SPECIFIC_PORT_SHIFT    8          /* Port is located in bits 8..15 */
#define DS_PROTOCOL_SPECIFIC_PORT_SIZE     1          /* Port size in bytes */

DECLARE_WAIT_QUEUE_HEAD(docsis_mng_wait);
static int                  ref = 0;
static struct semaphore     wr_sem;

static PAL_Handle palHandle;

static Cppi4Queue           rxfdQueue = {PAL_CPPI41_QUEUE_MGR_PARTITION_SR,   PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_Q_NUM};
static Cppi4Queue           rxQueue =   {PAL_CPPI41_QUEUE_MGR_PARTITION_SR,   PAL_CPPI41_SR_DOCSIS_MGMT_HOST_RX_Q_NUM};
static PAL_Cppi4QueueHnd    rxfdQueueHdl;
static PAL_Cppi4QueueHnd    rxQueueHdl;

static Cppi4Queue           txfdQueue = {PAL_CPPI41_QUEUE_MGR_PARTITION_SR,   PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_Q_NUM};
static Cppi4Queue           txQueue =   {PAL_CPPI41_QUEUE_MGR_PARTITION_SR,   PAL_CPPI41_SR_DOCSIS_TX_MGMT_Q_NUM};
static PAL_Cppi4QueueHnd    txfdQueueHdl;
static PAL_Cppi4QueueHnd    txQueueHdl;

static Cppi4HostDescLinux*  curBD4Read = NULL;
static Bool                 dataReady = False;

int     debug_enable = 0;

#ifdef MNG_Q_POLLING

struct timer_list mngPollTimer;

#else

static Uint8 accWrkArea[DOCSIS_MNG_ACC_MAXPAGENTRY*DOCSIS_MNG_ACC_PAGE_COUNT*4];
static Uint32 accCurPage = 0;
static Uint32 accCurBdOnPage = 0;
static Uint32 accDataListPtr[DOCSIS_MNG_ACC_PAGE_COUNT];
static Cppi4Queue  accRxQueue = {DOCSIS_MNG_QMGR,DOCSIS_MNG_RX_QNUM1};
static PAL_Cppi4QueueHnd accRxQueueHdl;

irqreturn_t docsisMngIsr(int irq, void *dev_id, struct pt_regs *regs)
{
    /* wake up (unblock) for reading data from userspace   */
    dataReady = True;
    wake_up_interruptible(&docsis_mng_wait);
    return IRQ_HANDLED;
}

#endif

static Cppi4HostDescLinux* docsisMngGetNextBd(void);
extern int docsis_cppi_init(void);

static int (* HAL_DocsisQosClass_ClassifyMacMngCB)(Uint8 *buf,Int16 *sfIndex,Int16 *phsIndex) = NULL;


/*
 * ref count the total number of opens (both read and write).
 * It makes the code simpler, by allocating all the needs of r/w
 * at open time and dealocating them (together) at the last close.
 */
static int docsisMngOpen(struct inode *inode, struct file *file)
{
    down(&wr_sem);
    printk(KERN_INFO "%s: ref %d\n",__FUNCTION__, ref);
    if (++ref > 1)
    {
        up(&wr_sem);
        return (0);
    }

#ifdef MNG_Q_POLLING
    mngPollTimer.expires = jiffies + HZ / 100;
    add_timer(&mngPollTimer);
    printk(KERN_INFO "%s: start polling timer\n",__FUNCTION__);
#else
    {
        int r;
        if (r = avalanche_intd_enable_interrupt(0, PAL_CPPI41_DOCSIS_RX_MGMT_ACC_CH_NUM)
        {
            printk(KERN_WARNING "docsisMngOpen: Unable to enable INTD interrupt \n");
            up(&wr_sem);
            return r;
        }

        /* register to the interrupt on the apdsp !!!*/
        if (r = request_irq(LNXINTNUM(MAP_INTD_TO_INTC(PAL_CPPI41_DOCSIS_RX_MGMT_ACC_INTV_NUM)), docsisMngIsr, SA_INTERRUPT, "docsis mng", NULL)) {
            printk(KERN_WARNING "Failed to register the irq for docsis mng.\n");
            up(&wr_sem);
            return r;
        }
        printk(KERN_INFO "%s: set intr on line : %d\n , %d\n",__FUNCTION__, LNXINTNUM(MAP_INTD_TO_INTC(PAL_CPPI41_DOCSIS_RX_MGMT_ACC_INTV_NUM)),MAP_INTD_TO_INTC(PAL_CPPI41_DOCSIS_RX_MGMT_ACC_INTV_NUM));
    }
#endif

    up(&wr_sem);
    return 0;
}

/* close function - called when the "file" /dev/docsis_mng is closed in userspace   */
static int docsisMngRelease(struct inode *inode, struct file *file)
{
    down(&wr_sem);
    printk(KERN_INFO "%s: ref %d\n",__FUNCTION__, ref);
    --ref;
    up(&wr_sem);
    return 0;
}

static Cppi4HostDescLinux* docsisMngGetNextBd()
{
#ifdef MNG_Q_POLLING
    Cppi4HostDescLinux *curBD = (Cppi4HostDescLinux *)PAL_cppi4QueuePop(rxQueueHdl);
    if (curBD == NULL)
    {
        dataReady = False;
    }
    else
    {
        curBD = PAL_CPPI4_PHYS_2_VIRT(curBD);
    }
    return (curBD);
#else
    /* all acc handling should be moved to the pal !!! */
    /* I don't want to duplicate it here    but i NEED to :( */
    Uint32 curBD;

    curBD = accDataListPtr[accCurPage] + accCurBdOnPage*DOCSIS_MNG_ACC_ENTRY_SIZE;
    if (debug_enable)
    {
        printk(KERN_INFO "docsisMngGetNextBd: curBd =%x \n",curBD);
        printk(KERN_INFO "docsisMngGetNextBd: accCurPage =%x, accCurBdOnPage=%x \n",accCurPage,accCurBdOnPage);
    }

    if (((Ptr)curBD) != NULL)
    {
        curBD = PAL_CPPI4_PHYS_2_VIRT(curBD);
        accCurBdOnPage++;
        return((Cppi4HostDescLinux *)(curBD & QMGR_QUEUE_N_REG_D_DESC_ADDR_MASK));
    }
    /* last entry on the page */
    /* invalidate the working area page */
    PAL_CPPI4_CACHE_INVALIDATE(accDataListPtr[accCurPage], DOCSIS_MNG_ACC_MAXPAGENTRY * 4);

    accCurPage++;
    if (accCurPage>=DOCSIS_MNG_ACC_PAGE_COUNT)
    {
        accCurPage = 0;
    }
    accCurBdOnPage = 0;
    dataReady = False;
    /* decrement the interrupt counter page */
    avalanche_intd_set_interrupt_count (0, PAL_CPPI41_DOCSIS_RX_MGMT_ACC_CH_NUM, 1);
    /* clear the interrupt mask to get next interrupt */
    avalanche_intd_write_eoi (PAL_CPPI41_DOCSIS_RX_MGMT_ACC_CH_NUM);
    return(NULL);
#endif
}

/* read function called when from /dev/docsis_mng is read */
static ssize_t docsisMngRead(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    static int  offset = 0;
    Uint32      user_count = count; /* save original count specified by the user */
    Uint32      timestamp;          /* timestamp associated with the packet */
    Uint8       portId;             /* Port where message arrived */
    int         err;

    /* i can add a semaphore to protect the critical section, but should I ??? */
    /* is it possible to have two entities reading from this device ??? */

    if (debug_enable)
    {
        printk(KERN_INFO "%s: in with buffer size %d\n",__FUNCTION__,count);
    }

    /* if we are not in the middle of the message , go bring a new one*/
    if (curBD4Read == NULL)
    {
        /* get the message from queue */
        while ((curBD4Read = docsisMngGetNextBd()) == NULL)
        {
            if (file->f_flags & O_NONBLOCK)
                return(-EAGAIN);

            /* do i have an issue with race condition ??? dataReady*/
            /* need to change to wait_event_interruptible(docsis_mng_wait, condition); */
            if (debug_enable)
            {
                printk(KERN_INFO "%s: going sleep %x\n",__FUNCTION__,(Uint32)curBD4Read);
            }

            wait_event_interruptible(docsis_mng_wait, (dataReady == True));

            if (debug_enable)
            {
                printk(KERN_INFO "%s: i am awake !\n",__FUNCTION__);
            }
        }
        offset = 0;
    }

    /* invalidate the cache bd */
    PAL_CPPI4_CACHE_INVALIDATE(curBD4Read, PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE);

    if (debug_enable)
    {
        printk(KERN_INFO "%s: processing %x\n, data %x,size %d\n",__FUNCTION__,
            (Uint32)curBD4Read, (Uint32)(PAL_CPPI4_PHYS_2_VIRT(curBD4Read->hw.bufPtr)),
            curBD4Read->hw.buffLen);
    }

    /* message is smaller than buffer */
    if (count > (curBD4Read->hw.buffLen - offset))
        count = (curBD4Read->hw.buffLen - offset);

    PAL_CPPI4_CACHE_INVALIDATE(PAL_CPPI4_PHYS_2_VIRT(curBD4Read->hw.bufPtr), curBD4Read->hw.buffLen);

    /* should i change the addr from phy to virt??? probably yes */
    err = copy_to_user(buf, (char*)PAL_CPPI4_PHYS_2_VIRT(curBD4Read->hw.bufPtr) + offset, count);

    if (debug_enable)
    {
        printk(KERN_INFO "%s: copy to user %d bytes with offset %x\n",__FUNCTION__,count,offset);
    }

    /* lets keep the message for the next read ???*/
    if (err != 0)
    {
        return -EFAULT;
    }

    /* if we done with the message */
    if (count == (curBD4Read->hw.buffLen - offset))
    {
        /* check if timestamp present */
        if (curBD4Read->psi[0] & DS_PROTOCOL_SPECIFIC_TS_PRESENT)
        {
            if ((count + DS_PROTOCOL_SPECIFIC_TS_SIZE)> user_count)
            {
                if (debug_enable)
                {
                    printk(KERN_WARNING "%s: Not enough space to copy timestamp to user!\n",__FUNCTION__);
            }
            }
            else
            {
                /* get timestamp from BD and append it at end of packet */
                timestamp = curBD4Read->psi[1];
                err = copy_to_user(buf + count, (char*)&timestamp, DS_PROTOCOL_SPECIFIC_TS_SIZE);
                if (debug_enable)
                {
                    printk(KERN_INFO "%s: Copy timestamp 0x%x to user (%d bytes) - %s.\n",__FUNCTION__,
                                            timestamp, DS_PROTOCOL_SPECIFIC_TS_SIZE, err ? "failed" : "ok");
                }
                if (err == 0)
                {
                    count += DS_PROTOCOL_SPECIFIC_TS_SIZE;
            }
        }
        }

        if ((count + DS_PROTOCOL_SPECIFIC_PORT_SIZE)> user_count) 
        {
            if (debug_enable)
            {
                printk(KERN_WARNING "%s: Not enough space to copy port to user!\n",__FUNCTION__);
        }
        }
        else 
        {
            portId = (curBD4Read->psi[0] >> DS_PROTOCOL_SPECIFIC_PORT_SHIFT) + 1;
            err = copy_to_user(buf + count, (char*)&portId, DS_PROTOCOL_SPECIFIC_PORT_SIZE);

            if (debug_enable)
            {
                printk(KERN_INFO "%s: Copy port %d to user (%d bytes) at offset %d - %s.\n",__FUNCTION__,
                                         portId, DS_PROTOCOL_SPECIFIC_PORT_SIZE, count, err ? "failed" : "ok");
            }

            if (err == 0)
            {
                count += DS_PROTOCOL_SPECIFIC_PORT_SIZE;
        }
        }

        /* return the bd to the free queue */
        PAL_cppi4QueuePush(rxfdQueueHdl,(Ptr)PAL_CPPI4_VIRT_2_PHYS((Uint32)curBD4Read), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_SR_DOCSIS_MGMT_RX_FD_HOST_DESC_SIZE), 0);
        curBD4Read = NULL;

        {
            UINT32 status;
            volatile static Int32 packet_counter = 0;

            packet_counter++;
            PAL_cppi4Control(palHandle,PAL_CPPI41_IOCTL_GET_QUEUE_ENTRY_COUNT,&rxfdQueue, &status);
            if (debug_enable)
            {
                printk("Read : after packet %d , free %d\n", packet_counter,status);
    }
        }
    }
    else
    {
        offset += count;
    }

    if (debug_enable)
    {
        printk(KERN_INFO "%s: return %d bytes \n",__FUNCTION__,count);
    }
    return count;
}


static ssize_t docsisMngWrite(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    int retval;
    Cppi4HostDescLinux *currBD = NULL;
    Uint32              currBuffer = 0;
    HalDocsisMng_t halMngData;
    struct CM_mng_t *macHdr;

    printk(KERN_INFO "Enter %s count %d\n",__FUNCTION__, count);

    if (down_interruptible(&wr_sem))
    {
        return -ERESTARTSYS;
    }

    /* get the bd + buffer from the free queue */
    currBD = (Cppi4HostDescLinux *) PAL_cppi4QueuePop(txfdQueueHdl);

    if (NULL == currBD)
    {
        printk(" %s can't allocate the free bd \n", __FUNCTION__);
        up(&wr_sem);
        return (-EAGAIN);
    }

    currBD = PAL_CPPI4_PHYS_2_VIRT(currBD);

    /* set the data pointer to copy from user */
    currBD->hw.bufPtr = currBD->hw.orgBufPtr;

    currBuffer = PAL_CPPI4_PHYS_2_VIRT(currBD->hw.bufPtr);

    /*Copy the internal HAL data structure*/
    if (copy_from_user((void *)&halMngData, buf, count))
    {
        printk("%s : failed to copy HAL Mngmt data from user\n", __FUNCTION__);
        PAL_cppi4QueuePush (txfdQueueHdl,(Ptr) PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE), 0);
        return -EFAULT;
    }

    printk(KERN_INFO "%s: encrFlag = %d msgBufLen=%d\n", __FUNCTION__, halMngData.encrFlag, halMngData.msgBufLen);

    if (halMngData.msgBufLen >= PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_BUFF_SIZE)
    {
        PAL_cppi4QueuePush (txfdQueueHdl,(Ptr) PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE), 0);
        return -EINVAL;     /* should be a whole message */
    }

    /* Copy the management message buffer */
    if (copy_from_user((void *)currBuffer, halMngData.msgBuf, halMngData.msgBufLen))
    {
        /* return the bd to the free queue */
        printk("%s : failed to copy management message buffer from user\n", __FUNCTION__);
        currBD->hw.bufPtr = 0;
        PAL_CPPI4_CACHE_WRITEBACK(currBD, PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE);
        PAL_cppi4QueuePush (txfdQueueHdl,(Ptr) PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE), 0);
        return -EINVAL;
    }
    else
    {
        /* set the bd fields and writeback the bd */
        currBD->hw.buffLen = halMngData.msgBufLen;
        currBD->hw.descInfo = PAL_CPPI4_HOSTDESC_DESC_TYPE_HOST << PAL_CPPI4_HOSTDESC_DESC_TYPE_SHIFT | halMngData.msgBufLen;

        if (!halMngData.encrFlag)
        {
            currBD->psi[0] = US_PSI_PRIVACY_OFF_BIT_MASK;
        }
        else
        {
            currBD->psi[0] = 0;
        }

        macHdr = (struct CM_mng_t *)currBuffer;
        if (HAL_DocsisQosClass_ClassifyMacMngCB)
        {
            Int16 sfIndex;
            Int16 phsIndex;
            //ARRIS Add Begin
            Bool ignoreClassifierValue = False;
            //ARRIS Add End

            /* Classifier exceptions - don't classify */
            if(!((macHdr->type == MAC_MNGT_MSG_TYPE_RNG_REQ) ||
                 (macHdr->type == MAC_MNGT_MSG_TYPE_REG_REQ) ||
                 (macHdr->type == MAC_MNGT_MSG_TYPE_REG_RSP) ||
                 (macHdr->type == MAC_MNGT_MSG_TYPE_REG_ACK) ||
                 (macHdr->type == MAC_MNGT_MSG_TYPE_INIT_RNG_REQ)   ||
                 (macHdr->type == MAC_MNGT_MSG_TYPE_B_INIT_RNG_REQ) ||
                 (macHdr->type == MAC_MNGT_MSG_TYPE_REG_REQ_MP) ||
                 (macHdr->type == MAC_MNGT_MSG_TYPE_REG_RSP_MP) ||
                 (macHdr->type == MAC_MNGT_MSG_TYPE_DPV_RSP)))
            {
                HAL_DocsisQosClass_ClassifyMacMngCB((Uint8 *)currBuffer, &sfIndex,  &phsIndex);
                currBD->psi[0] |= ((((Uint32) sfIndex) << US_PSI_SFINDEX_SHIFT));

                //ARRIS Add Begin
                if (halMngData.forceSF)
                {
                    int sfIndexClassifier = sfIndex;
                    sfIndex = halMngData.sfIndex;
                    printk(KERN_INFO "%s: sfIndex value %d is ignored. 0 is used instead.\n", __FUNCTION__, sfIndexClassifier);
                }
                //ARRIS Add End
        
            }
         }

        /* DPV-RSP is sent over user-specified service flow if so requested. */
        if ((macHdr->type == MAC_MNGT_MSG_TYPE_DPV_RSP) && halMngData.forceSF)
        {
            printk(KERN_INFO "%s: Forcing SF index 0x%x for DPV-RSP\n", __FUNCTION__, halMngData.sfIndex);
            currBD->psi[0] |= (halMngData.sfIndex << US_PSI_SFINDEX_SHIFT);
        }

        printk(KERN_INFO "%s: currBD->protocolSpec0 = %x currBD->buffLen=%d\n", __FUNCTION__, currBD->psi[0], currBD->hw.buffLen);

        PAL_CPPI4_CACHE_WRITEBACK(currBD, PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE);

        /* writeback the data */
        PAL_CPPI4_CACHE_WRITEBACK(currBuffer, halMngData.msgBufLen);

        /* push the bd to the management tx queue */
        /* return policy in the bd should be set to auto recycling */
        PAL_cppi4QueuePush (txQueueHdl,(Ptr) PAL_CPPI4_VIRT_2_PHYS((Uint32)currBD),PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_SR_DOCSIS_MGMT_TX_FD_HOST_DESC_SIZE), halMngData.msgBufLen);
    }

    *ppos += count;
    retval = count;

    up(&wr_sem);

    return retval;
}

#ifdef MNG_Q_POLLING
void docsisTimerFunc(unsigned long data)
{
    UINT32 status;

    PAL_cppi4Control (palHandle,PAL_CPPI41_IOCTL_GET_QUEUE_PEND_STATUS,&rxQueue, &status);

    if (status)
    {
        if (debug_enable)
        {
            printk(KERN_INFO "%s : wake the read \n", __FUNCTION__);
        }

        /* wake up (unblock) for reading data from userspace   */
        dataReady = True;
        wake_up_interruptible(&docsis_mng_wait);
    }

    mngPollTimer.expires = jiffies + HZ/100;
    add_timer(&mngPollTimer);
}
#endif

static unsigned int docsisMngPoll(struct file *file, poll_table * wait)
{
    if (debug_enable)
    {
        printk("%s: curBD4Read 0x%p\n",__FUNCTION__, curBD4Read);
    }

    poll_wait(file, &docsis_mng_wait, wait);

    if (debug_enable)
    {
        printk("%s: post poll_wait curBD4Read 0x%p\n",__FUNCTION__, curBD4Read);
    }

    if (curBD4Read || (curBD4Read = docsisMngGetNextBd()))
    {
        if (debug_enable)
        {
            printk("%s: before POLLIN return curBD4Read 0x%p\n",__FUNCTION__, curBD4Read);
        }
        return (POLLIN | POLLRDNORM);
    }

    return 0;
}

struct file_operations docsis_mng_fops =
{
    .owner   = THIS_MODULE,
    .llseek  = NULL,
    .read    = docsisMngRead,
    .write   = docsisMngWrite,
    .poll    = docsisMngPoll,
    .open    = docsisMngOpen,
    .release = docsisMngRelease,
};

static void cleanup(int level)
{
    switch (level)
    {
    default:
    case 5:
        PAL_cppi4QueueClose(palHandle, txQueueHdl);
    case 4:
        PAL_cppi4QueueClose(palHandle, txfdQueueHdl);
    case 3:
        PAL_cppi4QueueClose(palHandle, rxQueueHdl);
    case 2:
        PAL_cppi4QueueClose(palHandle, rxfdQueueHdl);
    case 1:
        PAL_cppi4Exit(palHandle, NULL);
        break;
    }
}

/* initialize module (and interrupt) */
static int __init docsisMngInit (void)
{
    int i;

    sema_init(&wr_sem,1);
    if (debug_enable)
    {
        printk(KERN_INFO "%s: Initializing the DOCSIS CPPI Resources\n",__FUNCTION__);
    }

    if (docsis_cppi_init() != 0)
    {
        if (debug_enable)
        {
            printk(KERN_INFO "Failed to initialize the DOCSIS CPPI resources \n");
        }
        return(-ENOMEM);
    }

    /* get a handle to the PAL structure */
    if ((palHandle =  PAL_cppi4Init(NULL, NULL)) == NULL)
    {
        printk(KERN_WARNING "%s: PAL CPPI is not initialized yet \n",__FUNCTION__);
        return(-EAGAIN);
    }

    printk(KERN_INFO "%s: pal handle %x\n",__FUNCTION__, (Uint32)palHandle);

    /*  open a queue objects one for free descriptor queue
     *  and one for rx queue
     */
    if ((rxfdQueueHdl = PAL_cppi4QueueOpen(palHandle, rxfdQueue)) == NULL)
    {
        printk(KERN_WARNING "%s: Failed to open fdQueue\n",__FUNCTION__);
        cleanup(1);
        return -1;
    }
    if ((rxQueueHdl = PAL_cppi4QueueOpen(palHandle, rxQueue)) == NULL)
    {
        printk(KERN_WARNING "%s: Failed to open rxQueueHdl\n",__FUNCTION__);
        cleanup(2);
        return -1;
    }


#ifdef MNG_Q_POLLING
    init_timer(&mngPollTimer);
    mngPollTimer.function = docsisTimerFunc;
    mngPollTimer.expires = 0;
#else
    /* don't forget to init the accumulator !!!! */
    chInfo.accuCfg.accChanNum = PAL_CPPI41_DOCSIS_RX_MGMT_ACC_CH_NUM;
    chInfo.accuCfg.listBase = accWrkArea; /*malloc or static allocation ???*/
    chInfo.accuCfg.maxPageEntry = DOCSIS_MNG_ACC_MAXPAGENTRY;
    chInfo.accuCfg.pacingTickCnt = DOCSIS_MNG_ACC_INTR_DELAY;
    chInfo.accuCfg.pacingMode = DOCSIS_MNG_ACC_INTR_MODE; /* delay since first new packet */
    chInfo.accuCfg.stallAvoidance = DOCSIS_MNG_ACC_STALL_AVOID;
    chInfo.accuCfg.listCountMode = DOCSIS_MNG_ACC_TERM_MODE; /* NULL Terminate Mode - The last list entry is used to store a NULL pointer record <BR> */
    chInfo.accuCfg.listEntrySize = DOCSIS_MNG_ACC_ENTRY_TYPE;
    chInfo.accuCfg.maxPageCnt = DOCSIS_MNG_ACC_PAGE_COUNT;

    PAL_osMemSet(accWrkArea,0,sizeof(accWrkArea));

    /* invalidate the page !!!*/
    accCurPage = 0;
    accDataListPtr[0] = (Uint32) accWrkArea;
    accDataListPtr[1] = (Uint32) accWrkArea + DOCSIS_MNG_ACC_MAXPAGENTRY*4 ;
    accRxQueueHdl = PAL_cppi4QueueOpen (palHandle,accRxQueue);
#endif

    /* ******************* init TX part of the driver ******************* */
    /*  open a queue objects one for free descriptor queue
     *  and one for tx queue
     */
    if ((txfdQueueHdl = PAL_cppi4QueueOpen(palHandle, txfdQueue)) == NULL)
    {
        printk(KERN_WARNING "%s: Failed to open fdQueue\n",__FUNCTION__);
        cleanup(3);
        return -1;
    }
    if ((txQueueHdl = PAL_cppi4QueueOpen(palHandle, txQueue)) == NULL)
    {
        printk(KERN_WARNING "%s: Failed to open rxQueueHdl\n",__FUNCTION__);
        cleanup(4);
        return -1;
    }

    i = register_chrdev(DOCSIS_MNG_MAJOR, DOCSIS_MNG_NAME, &docsis_mng_fops);
    if (i != 0)
    {
        return - EIO;
    }

    return 0;
}

/* close and cleanup module */
static void __exit docsisMngCleanup (void)
{
    if (debug_enable)
    {
        printk(KERN_INFO "%s: cleaning up module\n",__FUNCTION__);
    }

    unregister_chrdev(DOCSIS_MNG_MAJOR, DOCSIS_MNG_NAME);

    PAL_cppi4QueueClose (palHandle, rxfdQueueHdl);
    PAL_cppi4QueueClose (palHandle, rxQueueHdl);
    PAL_cppi4QueueClose (palHandle, txfdQueueHdl);
    PAL_cppi4QueueClose (palHandle, txQueueHdl);
    /* should i support DMA teardown */
}



/**************************************************************************/
/*! \fn int HAL_DocsisRegisterQosClassifierMacMgmtCB(int (*QosClass_ClassifyMacMng)
 *  (Uint8 *buf, int16 *sfIndex, int16 *phsIndex))
 *
 **************************************************************************
 *  \brief register the QOS Classifier Mac Mgmt CB function
 *  \param[in] the QOS Mac Mgmt CB function
 *  \return OK
 **************************************************************************/
int HAL_DocsisRegisterQosClassifierMacMgmtCB(int (*QosClass_ClassifyMacMng)(Uint8 *buf,Int16 *sfIndex,Int16 *phsIndex))
{
    Uint32 flags;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &flags);
    HAL_DocsisQosClass_ClassifyMacMngCB = QosClass_ClassifyMacMng;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, flags);

    return 0;
}

/***************************************************************************************************************/
/*                                  Internal DDOCSIS CPPI code - end                                           */
/***************************************************************************************************************/


EXPORT_SYMBOL(HAL_DocsisRegisterQosClassifierMacMgmtCB);


module_init(docsisMngInit);
module_exit(docsisMngCleanup);
MODULE_AUTHOR("Texas Instruments, Inc");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cable Modem Management Interface driver");

