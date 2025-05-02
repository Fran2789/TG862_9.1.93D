/* 
  GPL LICENSE SUMMARY

  Copyright(c) 2008-2013 Intel Corporation.

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

/*
 * docint.c
 * Description:
 * DOCSIS driver interrupt implementation
*/

#include <asm/uaccess.h>    /* copy_to/from_user */
#include <asm/irq.h>
#include <linux/init.h>     /* modules */
#include <linux/module.h>   /* module */
#include <linux/device.h>   /* module */
#include <linux/types.h>
#include <linux/fs.h>       /* chrdev allocation */
#include <linux/slab.h>     /* kmalloc and kfree */
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/semaphore.h>
#include "hal_interrupt_cause.h"
#include "docint.h"
#include "pal.h"
#include <avalanche_intc.h>

#define DOCINT_IRQ_FIFO_SIZE    1024

#ifdef DEBUG_DOCINT_PRINTS
/* note: prints function name for you */
#  define DPRINTK(fmt, args...) printk("%-20s:%d " fmt, __FUNCTION__,__LINE__, ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

MODULE_LICENSE("GPL");

#define PUMA5_HW_REV_1_X   0
#define PUMA5_HW_REV_2_0   1

#define INTC_CLEAR_REG_OFFSET               0x280

/* Global variable that defines the IRQ number for the PGA GRT interrupt */
unsigned int pgaGrtIntNum;
#if PUMA6_OR_NEWER_SOC_TYPE
unsigned int siliconStepping;
#endif

/* convert device-minor number to the interrupt-line number */
static int MIN2IRQ(di_t *m)
{
    switch (MINOR((m)->di_cdev.dev))
    {
    case DI_MAC_ERR:        return MAC_ERR_INT_IRQ_NUM      ; break;
    case DI_MAC_UCD:        return MAC_UCD_INT_IRQ_NUM      ; break;
    case DI_MAC_US_FW:      return MAC_US_FW_INT_IRQ_NUM    ; break;
    case DI_MAC_DS_FW:      return MAC_DS_FW_INT_IRQ_NUM    ; break;
    case DI_MAC_PHY:        return PHY_INT_IRQ_NUM          ; break;
    case DI_PHY_PGA_GRT:    return PHY_PGA_GRT_IRQ_NUM      ; break;
    case DI_PHY_MPT:        return PHY_MPT_INT_NUM          ; break;

    default:
        return -1;
        break;
    }

    return -1;
}

static char * IRQ2NAME(int irq)
{
    if (PHY_PGA_GRT_IRQ_NUM == irq)
    {
        return "DOCSIS PHY PGA GRT";
    }

    switch (irq)
    {
    case MAC_ERR_INT_IRQ_NUM    :   return "DOCSIS MAC Errors"  ;
    case MAC_UCD_INT_IRQ_NUM    :   return "DOCSIS MAC UCD"     ;
    case MAC_US_FW_INT_IRQ_NUM  :   return "DOCSIS MAC US"      ;
    case MAC_DS_FW_INT_IRQ_NUM  :   return "DOCSIS MAC DS"      ;
    case PHY_INT_IRQ_NUM        :   return "DOCSIS PHY FEC"     ;
    case PHY_MPT_INT_NUM        :   return "DOCSIS PHY MPEG Tuner" ;

    default:
        return NULL;
        break;
    }
}



/* count and pos are ignored since an ii_t is always read */
static ssize_t
docint_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    di_t *dev = filp->private_data; /* dev was stored in filp during open */

    if (down_interruptible(&dev->di_sem))
    {
        return -ERESTARTSYS;
    }

    while (dev->di_roffset == dev->di_woffset)
    {
        up(&dev->di_sem);
        if (wait_event_interruptible(dev->di_rqueue, dev->di_roffset != dev->di_woffset))
            return -ERESTARTSYS;
        if (down_interruptible(&dev->di_sem))
            return -ERESTARTSYS;
    }

    if (copy_to_user(buf, dev->di_data + dev->di_roffset, sizeof(ii_t)))
    {
        up(&dev->di_sem);
        return -EFAULT;
    }
    dev->di_roffset++;
    dev->di_roffset %= DOCINT_IRQ_FIFO_SIZE;
    atomic_inc(&dev->di_free);
    up(&dev->di_sem);
    *f_pos += sizeof(ii_t);
    return sizeof(ii_t);
}

static ssize_t
docint_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    di_t *dev = filp->private_data;
    ii_t info;
    ssize_t retval;

    if (down_interruptible(&dev->di_sem))
        return -ERESTARTSYS;
    if (copy_from_user(&info, buf, sizeof(ii_t)))
        retval = -EFAULT;
    else
    {
        *f_pos += sizeof(ii_t);
        retval = sizeof(ii_t);

        /* Clear interrupt */
        if (info.ii_clear == 1)
        {
            *(volatile unsigned int *)(INTC_VIRT + INTC_CLEAR_REG_OFFSET + ((info.ii_inum/32)*4)) = 1<<(info.ii_inum%32);
        }
        else
        {
            HAL_IsrMaskInterrupt(info.ii_inum, info.ii_masked);
        }
    }
    up(&dev->di_sem);

    return retval;
}

static unsigned int
docint_poll(struct file *filp, struct poll_table_struct *wait)
{
    di_t *dev = filp->private_data;
    unsigned int mask = 0;

    /*
     * The buffer is circular. It is considered full if
     * di_woffset is right behind di_roffset and empty
     * if the two are equal.
     */
    down(&dev->di_sem);
    poll_wait(filp, &dev->di_rqueue, wait);
    if (dev->di_roffset != dev->di_woffset)
    {
        /* ARRIS MODIFY BEGIN*/
        // printk(KERN_INFO "docint_poll irq %d waked up\n", MIN2IRQ(dev));
        /* ARRIS MODIFY END*/
        mask |= POLLIN | POLLRDNORM;    /* readable */
    }
    up(&dev->di_sem);

    return mask;
}

/* interrupt cause table */
extern dih_t docitbl[];

int
docint_cbregister(unsigned cause, char masked, void(*callback)(unsigned))
{
    if (cause >= MAX_INT_CAUSE)
    {
        DPRINTK(KERN_WARNING "docint_cbregister: bad cause\n");
        return 0;
    }
    docitbl[cause].dih_func = callback;
    docitbl[cause].dih_masked = masked;

    return 1;
}


/*
 * This routine discovers what's the real interrupt cause
 * It also clears the interrupt
 */
static inline int
get_cause(int irq, unsigned int *buf, unsigned int *group)
{
    int sizeOfbufInBits = 32;
    /* this can not be part of the "switch/case" below */
    /* because PHY_PGA_GRT_IRQ_NUM is not constant.    */
    if(irq == PHY_PGA_GRT_IRQ_NUM)
    {
        *(unsigned int *)buf = PHY_PGA_GRT_INT_MASK;
        *group = PHY_PGA_GRT_INT_GROUP;
        return sizeOfbufInBits;
    }

    switch (irq)
    {
    case MAC_ERR_INT_IRQ_NUM:
        HAL_IsrGetCauseMacErrorGroup(buf);
        *group = MAC_ERR1_INT_GROUP;
        ack_irq(irq);
        sizeOfbufInBits = (MAC_ERR_INT_GROUPS_NUM * 32);
        break;

    case MAC_UCD_INT_IRQ_NUM:
        *(unsigned int *)buf = HAL_IsrUcdChange_GetCause();
        *group = MAC_UCD_INT_GROUP;
        sizeOfbufInBits = 32;
        ack_irq(irq);
        break;

    case MAC_US_FW_INT_IRQ_NUM:
        *(unsigned int *)buf = HAL_IsrUsFw_GetCause();
        *group = MAC_US_FW_INT_GROUP;
        sizeOfbufInBits = 32;
        ack_irq(irq);
        break;

    case MAC_DS_FW_INT_IRQ_NUM:
        sizeOfbufInBits = HAL_IsrDsFw_GetCause(buf);
        *group = MAC_DS_FW_INT_GROUP;
        ack_irq(irq);
        break;

    case PHY_INT_IRQ_NUM:
        HAL_IsrPhyFec_GetCause(buf);
        sizeOfbufInBits = (PHY_INT_GROUPS_NUM * 32); // Including HSIF in case of Puma6
        *group = PHY_INT_GROUP;
        ack_irq(irq);
        break; 

    case PHY_MPT_INT_NUM:
        *(unsigned int *)buf = PHY_MPT_INT_MASK;
        sizeOfbufInBits = 32;
        *group = PHY_MPT_INT_GROUP;
        break;

    default:
        DPRINTK(KERN_WARNING "get_cause default %d\n", irq);
        return 0;
    }

    return sizeOfbufInBits;
}


/*
 * The handler discovers what's the interrupt cause
 * and calls the proper routine. Such a routine may
 * activate a tasklet, but this is decided by its writter
 */
static irqreturn_t
doc_ihandler(int irq, void *dev_id)
{
    unsigned int cause, i, bits, group = 0;
    unsigned int mask[ MAX_INT_GROUP ];

    memset(&mask, 0, sizeof (mask));
    DPRINTK(KERN_INFO "doc_ihandler irq %d \n", irq);
    bits = get_cause(irq, &mask[0], &group);  /* bits: size of 'mask' in bits*/

    for ( i = find_first_bit((void *)&mask, bits);
          i < bits ;
          i = find_next_bit((void *)&mask, bits, i + 1) )
    {
        cause = HAL_GET_INTERRUPT_CAUSE(group, i);
        /* ARRIS MODIFY BEGIN*/
        // DPRINTK(KERN_INFO "doc_ihandler irq %d cause %d\n", irq, cause);
        /* ARRIS MODIFY END*/
        if ((cause < MAX_INT_CAUSE) &&
            (docitbl[cause].dih_func))
        {
             docitbl[cause].dih_func(cause);
        }
    }

    return IRQ_HANDLED;
}

static int
docint_open(struct inode *inode, struct file *filp)
{
    di_t *dev;
    unsigned long flags;

    dev = container_of(inode->i_cdev, di_t, di_cdev);
    down(&dev->di_sem);
    DPRINTK(KERN_INFO "docint_open dev %p ref %d\n", dev, dev->ref);
    dev->ref++;
    filp->private_data = dev;   /* store for future use */

/*
    if (dev->di_irq != -1) {
        up(&dev->di_sem);
        return 0;
    }
*/
    if (dev->ref > 1)
    {
        up(&dev->di_sem);
        return 0;
    }

#if PUMA5_SOC_TYPE
    /* In case of Rev 1 we do not support the MPEG-Tuner interrupt, */
    /* so we don't call "request_irq".                              */
    if ((system_rev == PUMA5_HW_REV_1_X) && ((dev->di_cdev.dev) == DI_PHY_MPT))
    {
        up(&dev->di_sem);
        return 0;
    }
#endif

    dev->di_irq = MIN2IRQ(dev);

    /* for the PGA GRT interrupt - need specific initialization */
    if(dev->di_irq == PHY_PGA_GRT_IRQ_NUM)
    {
#if PUMA5_SOC_TYPE
        flags = IRQ_TYPE_EDGE_RISING;
#else
        if (siliconStepping < SILICON_STEPPING_ID_C_0)
        {
            flags = IRQ_TYPE_LEVEL_LOW;
        }
        else
        {
            flags = IRQ_TYPE_EDGE_RISING;
        }
#endif
    }
    else if(dev->di_irq == PHY_MPT_INT_NUM)
    {
        flags = IRQ_TYPE_LEVEL_LOW;
    }
    else
    {
        flags = IRQ_TYPE_LEVEL_HIGH;
    }

    if (dev->di_irq == MAC_DS_FW_INT_IRQ_NUM)
    {
        /* Set Pacer 0 particular parameters */
        /* --------------------------------- */
        /* Set normal operation (no bypass) */
        avalanche_intc_disable_pacer_bypass(0);
        /* Set to time-based pacing (Generates an interrupt after a programmed amount of time) */
        avalanche_intc_set_pacer_mode(0, INTC_PACER_TIME_BASED);
        /* Set Restart Mode. Determines whether the timer starts after the last paced interrupt or the next input interrupt. Used for timer-based pacing only. 0 = After the last paced output interrupt */
        avalanche_intc_clear_pacer_restart_mode(0);
        /* Set the interrupt max value. For time-based pacing this is the time in ticks to count to. 65000*15us=975ms */
        avalanche_intc_set_pacer_max_val(0, 65000);
        /* Map interrupt 18(MAC_DS_FW_INT_IRQ_NUM) to pacer 0 */
        avalanche_intc_unmap_interrupt_to_pacer(MAC_DS_FW_INT_IRQ_NUM, 0); /* Notice - unmap and map functions do the opposite!!! */
    }
#if PUMA6_OR_NEWER_SOC_TYPE
    if (dev->di_irq == PHY_INT_IRQ_NUM)
    {
        /* Set Pacer 1 particular parameters */
        /* --------------------------------- */
        /* Set normal operation (no bypass) */
        avalanche_intc_disable_pacer_bypass(1);
        /* Set to time-based pacing (Generates an interrupt after a programmed amount of time) */
        avalanche_intc_set_pacer_mode(1, INTC_PACER_TIME_BASED);
        /* Set Restart Mode. Determines whether the timer starts after the last paced interrupt or the next input interrupt. Used for timer-based pacing only. 0 = After the last paced output interrupt */
        avalanche_intc_clear_pacer_restart_mode(1);
        /* Set the interrupt max value. For time-based pacing this is the time in ticks to count to. 6600*15us=99ms */
        avalanche_intc_set_pacer_max_val(1, 6600);
        /* Map interrupt 18(PHY_INT_IRQ_NUM) to pacer 1 */
        avalanche_intc_unmap_interrupt_to_pacer(PHY_INT_IRQ_NUM, 1); /* Notice - unmap and map functions do the opposite!!! */
    }
#endif

    if (request_irq(dev->di_irq, doc_ihandler, flags, IRQ2NAME(dev->di_irq), (void *)dev))
    {
        DPRINTK(KERN_WARNING "Can't get assigned irq %i\n", dev->di_irq);
    }

#if PUMA6_OR_NEWER_SOC_TYPE
    if ((dev->di_irq == PHY_PGA_GRT_IRQ_NUM) && (siliconStepping < SILICON_STEPPING_ID_C_0))
    {
        /* We want to start with masked interrupt. Only when setting the PGA we will enable the interrupt again */
        /* This is a WA for HW issue that uses AMP signal for the GRT interrupt */
        disable_irq(PHY_PGA_GRT_IRQ_NUM);
    }
#endif

    /* ARRIS MODIFY BEGIN*/
    // printk(KERN_INFO "request_irq %d in docint_open\n", dev->di_irq);
    /* ARRIS MODIFY END*/
    up(&dev->di_sem);

    return 0;
}

static int
docint_release(struct inode *inode, struct file *filp)
{
    di_t *dev;

    dev = container_of(inode->i_cdev, di_t, di_cdev);
    down(&dev->di_sem);
    DPRINTK(KERN_INFO "docint_release: dev %p ref %d\n", dev, dev->ref);
    dev->ref--;
    if (dev->ref) {
        up(&dev->di_sem);
        return 0;
    }
    DPRINTK(KERN_INFO "docint_release: irq %d\n", dev->di_irq);
    free_irq(dev->di_irq, dev);
    dev->di_irq = -1;
    up(&dev->di_sem);
    return 0;
}

struct file_operations di_fops = {
    .owner      = THIS_MODULE,
    .read       = docint_read,
    .write      = docint_write,
    .poll       = docint_poll,
    .open       = docint_open,
    .release    = docint_release,
};

static dev_t dev; /* Contains major and first minor number */
static di_t *docint_devices;

static void
docint_cleanup(int count)
{
    int i;

    if (docint_devices)
    {
        for (i = 0; i < count; i++)
        {
            if (docint_devices[i].di_data)
            {
                kfree(docint_devices[i].di_data);
            }
            cdev_del(&docint_devices[i].di_cdev);
        }
        kfree(docint_devices);
    }
    unregister_chrdev_region(dev, DI_COUNT);
}

static void initctab(di_t *dev)
{
    int i, j, groups, group, irq = MIN2IRQ(dev);

    DPRINTK(KERN_INFO "initctab: irq %d dev 0x%x\n", irq, (Uint32)dev);


    /* this can not be part of the "switch/case" below */
    /* because PHY_PGA_GRT_IRQ_NUM is not constant.    */
    if(irq == PHY_PGA_GRT_IRQ_NUM)
    {
        /* This interrupt line only contains one interrupt (it is not really a group), */
        /* therefore the initialization method of the table is a little different.     */
        docitbl[HAL_GET_INTERRUPT_CAUSE(PHY_PGA_GRT_INT_GROUP, PHY_PGA_GRT_INT_INDEX)].dih_dev = dev;
        return; /* we do not want to continue in this function*/
    }

    groups = 1;

    switch (irq)
    {
    case MAC_ERR_INT_IRQ_NUM:
        groups = MAC_ERR_INT_GROUPS_NUM;
        group = MAC_ERR_INT_GROUP_FIRST;
        break;

    case MAC_UCD_INT_IRQ_NUM:
        group = MAC_UCD_INT_GROUP;
        break;

    case MAC_US_FW_INT_IRQ_NUM:
        group = MAC_US_FW_INT_GROUP;
        break;

    case MAC_DS_FW_INT_IRQ_NUM:
        groups = MAC_DS_FW_INT_GROUPS;
        group = MAC_DS_FW_INT_GROUP;
        break;

    case PHY_INT_IRQ_NUM:
        groups = PHY_INT_GROUPS_NUM;
        group = PHY_INT_GROUP;
        break;

    case PHY_MPT_INT_NUM:
        group = PHY_MPT_INT_GROUP;
        break;


    default:
        DPRINTK(KERN_WARNING "initctab: bad irq %d\n", irq);
        return;
    }

    for (j = 0; j < groups; j++)
    {
        for (i = 0; i < 32; i++)
        {
            docitbl[HAL_GET_INTERRUPT_CAUSE(group + j, i)].dih_dev = dev;
        }
    }
}

#define dump_proc_name(type,op,group)      docint_dump_##group##_##op##type
#define dump_proc_define(type,op,group)                                                                 \
                                                                                                        \
int docint_dump_##group##_##op##type(char* buf, char **start, off_t offset, int count, int *eof, void *data) \
{                                                                                                       \
    int len = 0;                                                                                        \
    static int total_size;                                                                              \
                                                                                                        \
                                                                                                        \
    if (0 == offset)                                                                                    \
    {                                                                                                   \
        len = HAL_Isr##group##_##op##type(buf,4096);                                                    \
        total_size = len;                                                                               \
    }                                                                                                   \
    else                                                                                                \
    {                                                                                                   \
        len = total_size - offset;                                                                      \
    }                                                                                                   \
                                                                                                        \
    if (len > count)                                                                                    \
    {                                                                                                   \
        len = count;                                                                                    \
    }                                                                                                   \
    else                                                                                                \
    {                                                                                                   \
        *eof = 1;                                                                                       \
    }                                                                                                   \
                                                                                                        \
    *start = buf+offset;                                                                                \
                                                                                                        \
    return len;                                                                                         \
}

dump_proc_define(Config,  Print,    PhyFec)

#if PUMA6_OR_NEWER_SOC_TYPE
dump_proc_define(Stats,   Print,    Hsif)
#endif

dump_proc_define(Stats,   Print,    PhyFec)
dump_proc_define(Stats,   Print,    DsFw)
dump_proc_define(Stats,   Print,    UsFw)
dump_proc_define(Stats,   Print,    UcdChange)

dump_proc_define(Stats,   Reset,    PhyFec)
dump_proc_define(Stats,   Reset,    DsFw)
dump_proc_define(Stats,   Reset,    UsFw)
dump_proc_define(Stats,   Reset,    UcdChange)

int docint_stats_enable (struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    HAL_IsrStatsEnableSet(True);
    return count;
}

int docint_stats_disable (struct file *file, const char __user *buffer, unsigned long count, void *data)
{
    HAL_IsrStatsEnableSet(False);
    return count;
}


static struct class *class;

static int  docint_init( void )
{
    int i, r, devno;
    int isPuma6D0 = 0;

#if PUMA6_OR_NEWER_SOC_TYPE
    if ( SRAM_Get_Boot_Param(SILICON_STEPPING_ID, &siliconStepping) )
    {
        printk("%s:%d ERROR .... silicon stepping retrieval unsuccessful \n",__FUNCTION__,__LINE__);
    }
    isPuma6D0 = ((siliconStepping >= SILICON_STEPPING_ID_D_0)?1:0);
#endif

    if ((r = alloc_chrdev_region(&dev, DI_MINOR_START, DI_COUNT, DI_NAME)) != 0)
    {
        DPRINTK(KERN_WARNING "docint_init: could not allocate device\n");
        return r;
    }

    class = class_create(THIS_MODULE, "DOCINT");

    DPRINTK(KERN_INFO "docint_init: registered with major number %i\n", MAJOR(dev));

    if ((docint_devices = kmalloc(DI_COUNT * sizeof(di_t), GFP_KERNEL)) == NULL)
    {
        DPRINTK(KERN_WARNING "docint_init: could not allocate %d devices\n", DI_COUNT);
        docint_cleanup(0);
        return -ENOMEM;
    }

    memset(docint_devices, 0, DI_COUNT * sizeof(di_t));

    for (i = 0; i < DI_COUNT; i++)
    {
        sema_init(&docint_devices[i].di_sem,1);

        if ((docint_devices[i].di_data = kmalloc(DOCINT_IRQ_FIFO_SIZE * sizeof(ii_t), GFP_KERNEL)) == NULL)
        {
            DPRINTK(KERN_WARNING "docint_init: could not allocate data for device %d\n", i);
            docint_cleanup(i);
            return -ENOMEM;
        }

        docint_devices[i].di_roffset = 0;
        docint_devices[i].di_woffset = 0;
        docint_devices[i].ref = 0;

        atomic_set(&docint_devices[i].di_free, DOCINT_IRQ_FIFO_SIZE);

        init_waitqueue_head(&docint_devices[i].di_rqueue);

        cdev_init(&docint_devices[i].di_cdev, &di_fops);

        docint_devices[i].di_cdev.owner = THIS_MODULE;

        devno = MKDEV(MAJOR(dev), MINOR(dev) + i);

        if ((r = cdev_add(&docint_devices[i].di_cdev, devno, 1)) != 0)
        {
            DPRINTK(KERN_WARNING "error %d adding %s%d\n", r, DI_NAME, i);
        }
        else
        {
            DPRINTK(KERN_INFO "%s%d added\n", DI_NAME, i);
            initctab(&docint_devices[i]);
        }

        device_create(class, 0, MKDEV(MAJOR(dev), MINOR(dev) + i), 0, "DOCINT%d", i);

        docint_devices[i].di_irq = -1;
    }

    /* Resetting PHY-interrupts database */
    HAL_IsrResetPhyFecDB();

    HAL_IsrPhyFec_ResetStats(NULL,0);
    HAL_IsrUcdChange_ResetStats(NULL,0);
    HAL_IsrDsFw_Init(isPuma6D0);
    HAL_IsrDsFw_ResetStats(NULL,0);
    HAL_IsrUsFw_ResetStats(NULL,0);

#if defined (CONFIG_MACH_PUMA5)
    /* Check the system revision and define the IRQ number for the PGA GRT interrupt */
    if (system_rev == PUMA5_HW_REV_2_0)
    {
        pgaGrtIntNum = PHY_PGA_GRT_IRQ_NUM_REV2;
    }
    else
    {
        pgaGrtIntNum = PHY_PGA_GRT_IRQ_NUM_REV1;
    }

    /* We use external interrupt #1 for either GRT interrupt or MPEG-Tuner    */
    /* interrupt. However, this is an external interrupt line that is muxed   */
    /* with GPIO #27. Need to disable the GPIO in order to gain ext-interrupt */
    /* functionality.                                                         */
    PAL_sysGpioCtrl(PHY_PGA_GRT_INT_REV1_GPIO, FUNCTIONAL_PIN, GPIO_INPUT_PIN);
#else
    if ( siliconStepping < SILICON_STEPPING_ID_C_0 )
    {
        pgaGrtIntNum = PHY_PGA_GRT_IRQ_NUM_PRE_C0;
    }
    else
    {
        pgaGrtIntNum = PHY_PGA_GRT_IRQ_NUM_C0;
    }
#endif

    {
        struct proc_dir_entry * docint_dir = NULL;
        struct proc_dir_entry * tmp_dir = NULL;

        if (NULL == (docint_dir = proc_mkdir("docint",     NULL ))) { printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); }
        if (NULL == (tmp_dir = create_proc_entry( "enable" , 0644, docint_dir ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        tmp_dir->write_proc = docint_stats_enable;
        if (NULL == (tmp_dir = create_proc_entry( "disable" , 0644, docint_dir ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        tmp_dir->write_proc = docint_stats_disable;

        if (NULL == (tmp_dir = proc_mkdir("phy",docint_dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "stats" ,  0, tmp_dir, dump_proc_name(Stats, Print, PhyFec), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "conf"  ,  0, tmp_dir, dump_proc_name(Config,Print, PhyFec), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "reset" ,  0, tmp_dir, dump_proc_name(Stats, Reset, PhyFec), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

#if PUMA6_OR_NEWER_SOC_TYPE
        if (NULL == (tmp_dir = proc_mkdir("hsif",docint_dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "stats" ,  0, tmp_dir, dump_proc_name(Stats, Print, Hsif), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "reset" ,  0, tmp_dir, dump_proc_name(Stats, Reset, PhyFec), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
#endif

        if (NULL == (tmp_dir = proc_mkdir("ucd",docint_dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "stats" , 0, tmp_dir, dump_proc_name(Stats, Print, UcdChange), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "reset" , 0, tmp_dir, dump_proc_name(Stats, Reset, UcdChange), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

        if (NULL == (tmp_dir = proc_mkdir("dsFw",docint_dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "stats" , 0, tmp_dir, dump_proc_name(Stats, Print, DsFw), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "reset" , 0, tmp_dir, dump_proc_name(Stats, Reset, DsFw), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }

        if (NULL == (tmp_dir = proc_mkdir("usFw",docint_dir))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "stats" , 0, tmp_dir, dump_proc_name(Stats, Print, UsFw), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        if (NULL == (create_proc_read_entry( "reset" , 0, tmp_dir, dump_proc_name(Stats, Reset, UsFw), NULL ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
    }

    return 0;
}

static void
docint_exit(void)
{
    docint_cleanup(DI_COUNT);
}


/****************************************************************************************************/
/*                                      KERNEL SIDE Interrupt Handlers                              */
/****************************************************************************************************/
void docint_default_handler(unsigned int index)
{
    di_t *dev;
    ii_t info;

    info.ii_clear = 0;
    dev = docitbl[index].dih_dev;
    /* ARRIS MODIFY BEGIN*/
    // printk(KERN_INFO "Enter default_handler cause %d dev 0x%x\n", index, (Uint32)dev);
    /* ARRIS MODIFY END*/
    /* Clear of the interrupt is done by get_cause() */

    if (!atomic_read(&dev->di_free))
    {
        panic("%s: no room for interrupt %d\n", __func__, index);
    }

    info.ii_inum = (short)index;
    info.ii_masked = docitbl[index].dih_masked;
    dev->di_data[dev->di_woffset++] = info;
    atomic_dec(&dev->di_free);
    dev->di_woffset %= DOCINT_IRQ_FIFO_SIZE;

    wake_up_interruptible(&dev->di_rqueue);
}

void docint_mask_interrupt_handler(unsigned int index)
{
    HAL_IsrMaskInterrupt(index, True);

    docint_default_handler(index);
}

#if PUMA6_OR_NEWER_SOC_TYPE
void docint_pga_interrupt_handler(unsigned int index)
{
    if (siliconStepping < SILICON_STEPPING_ID_C_0)
    {
        HAL_IsrMaskInterrupt(index, True);
    }

    docint_default_handler(index);
}
#endif

module_init(docint_init);
module_exit(docint_exit);

