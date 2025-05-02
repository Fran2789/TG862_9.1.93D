/*
 *
 * soc_interface_driver.c
 * Description:
 * SOC interface driver implementation
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

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/proc_fs.h>

#include "pal.h"
#include "pal_osWait.h"
#include "soc_interface_driver.h"
#include "soc_docsis_global_driver.h"
#include "mpeg_out_driver.h"
#include "mpeg_encap_driver.h"



#define SOC_INTERFACE_DRIVER_MAJOR  36


static Uint32     ref = 0;
static struct     semaphore wr_sem;
static Uint32     debug_enable = 1;
static SoCDriverOperations_t  *soc_driver_operations[SOC_MODULE_EOF];
static struct proc_dir_entry *soc_mem = NULL;

static int  socDriverOpen       (struct inode *inode, struct file *file);
static int  socDriverRelease    (struct inode *inode, struct file *file);
static long  socDriverIoctl     (struct file *file, unsigned int ioctl_num,unsigned long ioctl_param);
static long  socTopIoctl        (struct file *file, unsigned int ioctl_num,unsigned long ioctl_param,struct semaphore *soc_sem);

static int soc_mem_write     (struct file *fp, const char * buf, unsigned long count, void * data);

/* Module Declarations */
/*
 * This structure will hold the functions to be called
 * when a process does something with the SoC interface driver.
 * NULL is for unimplemented functions.
*/
struct file_operations soc_driver_fops = {
    .owner   = THIS_MODULE,
    .llseek  = NULL,
    .read    = NULL,
    .write   = NULL,
    .poll    = NULL,
    .unlocked_ioctl   = socDriverIoctl,
    .open             = socDriverOpen,
    .release          = socDriverRelease,
};

/*
 * This function is called whenever a process tries to do an ioctl to our
 * SoC driver file. We get two extra parameters (additional to the file
 * structure, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 * _IOC macros are used to decode ioctl numbers:
 *    _IOC_DIR(nr)  - get direction number
 *    _IOC_TYPE(nr) - get type number
 *    _IOC_NR(nr)   - get ordinal (sequential) number
 *    _IOC_SIZE(nr) - get size number
 */
static long   socDriverIoctl(struct file *file,
                            unsigned int ioctl_num, /* number and param for ioctl */
                            unsigned long ioctl_param)
{
   Uint32 err = 0;
   Uint32 i;
   SoC_ModuleIds_e   moduleId;
   /*
    * the direction is a bitmask, and VERIFY_WRITE catches R/W
    * transfers. `Type' is user-oriented, while
    * access_ok is kernel-oriented, so the concept of "read" and
    * "write" is reversed
    */
   if (_IOC_DIR(ioctl_num) & _IOC_READ)
       err = !access_ok(VERIFY_WRITE, (void __user *)ioctl_param, _IOC_SIZE(ioctl_num));
   else if (_IOC_DIR(ioctl_num) & _IOC_WRITE)
       err =  !access_ok(VERIFY_READ, (void __user *)ioctl_param, _IOC_SIZE(ioctl_num));
   if (err)
      return -EFAULT;


    /*
     * Go over all registered modules and run the required ioctl callback
     */
   moduleId = (SoC_ModuleIds_e)_IOC_TYPE(ioctl_num);
   for (i=SOC_MODULE_START; i<SOC_MODULE_EOF; i++)
   {
      if (soc_driver_operations[i]->socModuleID == moduleId)
      {
         return (soc_driver_operations[i]->soc_ioctl(file,ioctl_num,ioctl_param,&wr_sem));
      }
   }
   /* Unknown SoC Module -Error- */
   return -ENOTTY;
}


/* open function - called when the "file" /dev/soc_driver is opened in userspace   */
static int socDriverOpen(struct inode *inode, struct file *file)
{
    Uint32 i;
    ref++;
    printk(KERN_INFO "socDriverOpen: ref %d\n", ref);

    /*
      * Go over all registered modules and run the Open callback (if any)
      */
    for (i=SOC_MODULE_START; i<SOC_MODULE_EOF; i++)
    {
      if (soc_driver_operations[i]->soc_open)
      {
         return (soc_driver_operations[i]->soc_open(inode,file,&wr_sem));
      }
    }

    return 0;
}
/* close function - called when the "file" /dev/soc_driver is closed in userspace   */
static int socDriverRelease(struct inode *inode, struct file *file)
{
    Uint32 i;
    ref--;
    printk(KERN_INFO "socDriverRelease: ref %d\n", ref);

    /*
      * Go over all registered modules and run the Release callback (if any)
      */
    for (i=SOC_MODULE_START; i<SOC_MODULE_EOF; i++)
    {
      if (soc_driver_operations[i]->soc_release)
      {
         return (soc_driver_operations[i]->soc_release(inode,file,&wr_sem));
      }
    }

    return 0;
}

SoCDriverOperations_t   socTopDescriptor =
{
    .soc_ioctl = socTopIoctl,
};


/* initialize module */
static int __init socDriverInit (void)
{
    Uint32 rc;
    Uint32 i;

    sema_init(&wr_sem,1);
    if (debug_enable)
        printk(KERN_INFO "socDriverInit: initializing module\n");

    rc = register_chrdev(SOC_INTERFACE_DRIVER_MAJOR, SOC_INTERFACE_DRIVER_NAME, &soc_driver_fops);
    if (rc)
    {
       printk(KERN_WARNING "socDriverInit: registered with major %d failed.\n",SOC_INTERFACE_DRIVER_MAJOR);
       return (rc);
    }

    /* Registering all SoC modules function pointers */
    soc_driver_operations[ SOC_TOP_MODULE_ID ] = &socTopDescriptor;
    mpegOutRegisterSoCModule        (&soc_driver_operations[ SOC_MPEG_OUT_MODULE_ID       ]);
    socDocsisGlobalRegisterSoCModule(&soc_driver_operations[ SOC_DOCSIS_GLOBAL_MODULE_ID  ]);
    mpegEncapRegisterSoCModule      (&soc_driver_operations[ SOC_MPEG_ENCAP_MODULE_ID     ]);
    /* --- Add new SoC Modules Registering here --- */

    /*
      * Go over all registered modules and run the Init callback (if any)
      */
    for (i=SOC_MODULE_START; i<SOC_MODULE_EOF; i++)
    {
       if (soc_driver_operations[i]->soc_init)
       {
          soc_driver_operations[i]->soc_init(&wr_sem);
       }
    }

    /* create soc_mem proc */
    soc_mem = create_proc_entry("soc", 0644,NULL);
    if (soc_mem)
    {
        soc_mem->read_proc  = NULL;
        soc_mem->write_proc = soc_mem_write;
    }

    return 0;
}

/* close and cleanup module */
static void __exit socDriverCleanup (void)
{
   Uint32 i;
   if (debug_enable)
      printk(KERN_INFO "socDriverCleanup: cleaning up module\n");

   /*
     * Go over all registered modules and run the Cleanup callback (if any)
     */
   for (i=SOC_MODULE_START; i<SOC_MODULE_EOF; i++)
   {
     if (soc_driver_operations[i]->soc_cleanup)
     {
        soc_driver_operations[i]->soc_cleanup(&wr_sem);
     }
   }

   unregister_chrdev(SOC_INTERFACE_DRIVER_MAJOR, SOC_INTERFACE_DRIVER_NAME);
   return;
}


/**************************************************************************/
/*! \fn void socWritePhysRegister (Uint32  addr, Uint32  value, Bool printEn)
 **************************************************************************
 *  \brief Write SoC Register.
 *  \param[in] addr    - Register addr.
 *  \param[in] value   - Value to be written.
 *  \return none.
 **************************************************************************/
void socWritePhysRegister (Uint32  addr, Uint32  value )
{
   volatile Uint32 *reg;
    reg = (Uint32 *) IO_PHY2VIRT(addr);

   *reg = value;
}

/**************************************************************************/
/*! \fn void socReadPhysRegister(Uint32  addr, volatile Uint32 *value, Bool printEn)
 **************************************************************************
 *  \brief Read SoC Register.
 *  \param[in] addr    - Register addr.
 *  \param[in] *value  - Pointer for returned register value.
 *  \return none.
 **************************************************************************/
void socReadPhysRegister(Uint32  addr, Uint32 *value)
{
   volatile Uint32 *reg;

   reg = (Uint32 *) IO_PHY2VIRT(addr);

   *value = *(reg);
}

/**************************************************************************/
/*! \fn void socWriteRegister (Uint32  addr, Uint32  value, Bool printEn)
 **************************************************************************
 *  \brief Write SoC Register.
 *  \param[in] addr    - Register addr.
 *  \param[in] value   - Value to be written.
 *  \param[in] printEn - If True a debug print is enable.
 *  \return none.
 **************************************************************************/
void socWriteRegister (Uint32  addr, Uint32  value, Bool printEn)
{
   volatile Uint32 *reg;
    reg = (Uint32 *) addr;

   *reg = value;

   if (printEn)
      if (debug_enable)
         printk(KERN_INFO "Write Virtual: [%p] <-- 0x%08x ; Physical: [%p] <-- 0x%08x\n", reg, value, (Uint32*)IO_VIRT2PHY((Uint32)reg), value);

   PAL_sysCacheFlush(PAL_OSMEM_ADDR_DAT, (void*)(reg), sizeof(Uint32));
}

/**************************************************************************/
/*! \fn void socReadRegister(Uint32  addr, volatile Uint32 *value, Bool printEn)
 **************************************************************************
 *  \brief Read SoC Register.
 *  \param[in] addr    - Register addr.
 *  \param[in] *value  - Pointer for returned register value.
 *  \param[in] printEn - If True a debug print is enable.
 *  \return none.
 **************************************************************************/
void socReadRegister(Uint32  addr, Uint32 *value, Bool printEn)
{
   volatile Uint32 *reg;

   reg = (Uint32 *) addr;
   PAL_sysCacheInvalidate(PAL_OSMEM_ADDR_DAT, (void*)(reg), sizeof(Uint32));

   *value = *(reg);
   if (printEn)
      if (debug_enable)
         printk(KERN_INFO "Read Virtual: [%p] <-- 0x%08x ; Read Physical: [%p] <-- 0x%08x\n", reg, *value, (Uint32*)IO_VIRT2PHY((Uint32)reg), *value);
}

/**************************************************************************/
/*! \fn void socBitField32Set(Uint32  *addr, Uint32  data, Uint32  offset, Uint32  width)
 **************************************************************************
 *  \brief Set specific bits in a 32 bit variable.
 *  \param[in] *addr   - pointer to 32 bit variable to change.
 *  \param[in] data    - Data to write.
 *  \param[in] offset  - Write from this offset.
 *  \param[in] width   - Write width bits.
 *  \return none.
 **************************************************************************/
void socBitField32Set(Uint32  *addr, Uint32  data, Uint32  offset, Uint32  width)
{
    Uint32 mask;

    mask = ((0xFFFFFFFF >> (32-width)) << offset);

    *addr &= ~mask;
    *addr |=  mask & (data << offset);
}

/**************************************************************************/
/*! \fn int soc_mem_write(struct file *fp, const char * buf, unsigned long count, void * data)
 **************************************************************************
 *  \brief proc file to access device memory
 **************************************************************************/
static int soc_mem_write(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[50];
    int ret_val = 0;
    unsigned int addr, val;

    if (count > 50)
    {
        printk(KERN_ERR "Buffer Overflow\n");
        return -EFAULT;
    }

    if(copy_from_user(local_buf,buf,count))
        return -EFAULT;;
    local_buf[count-1]='\0'; /* Ignoring last \n char */
    ret_val = count;

    if(local_buf[0] == 'r')
    {
        sscanf(local_buf+1,"%x",&addr);
        socReadRegister(addr, &val, 0);
        printk("READ: Address = 0x%08X, Value = 0x%08X\n", addr, val);
    }
    else if(local_buf[0] == 'w')
    {
        sscanf(local_buf+1,"%x %x",&addr, &val);
        socWriteRegister(addr, val, 0);
        printk("Write: Address = 0x%08X, Value = 0x%08X\n", addr, val);
    }
    else
    {
        printk(KERN_ERR "Unknown operation, must be read ('r') or write ('w') \n");
        return -EFAULT;
    }

    return ret_val;
}

#if defined (CONFIG_MACH_PUMA6)
/**************************************************************************/
/*! \fn Uint32 BuildPhyAddr(Uint32 regOffset)
 **************************************************************************
 *  \brief Convert register offset into PHY memory address
 *  \param[in]: UINT32 regOffset - The register offset from phy base address
 *  \param[out]: UINT32 - the phy virtual address
 **************************************************************************/
Uint32 BuildPhyAddr(Uint32 regOffset)
{
    Uint32 regVirtAddr = regOffset;

    regVirtAddr <<= HAL_PHY_REGISTER_SHIFT;
    if(regVirtAddr >= HAL_PHY_ADDRESS_SPACE_SIZE)
    {
        return (Uint32)(-EINVAL);
    }
    regVirtAddr += HAL_PHY_BASE_OFFSET;
    regVirtAddr += MAC_KERNEL_BASE_ADDRESS;
    return regVirtAddr;
}

#endif


/**************************************************************************/
/*! \fn static long socTopIoctl(struct file *file,
                                unsigned int ioctl_num, unsigned long ioctl_param)
 **************************************************************************
 *  \brief This function is called whenever a process tries to do an ioctl to our
 * SoC driver file. We get two extra parameters (additional to the file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 * _IOC macros are used to decode ioctl numbers:
 *    _IOC_DIR(nr)  - get direction number
 *    _IOC_TYPE(nr) - get type number
 *    _IOC_NR(nr)   - get ordinal (sequential) number
 *    _IOC_SIZE(nr) - get size number
 *  \return 0 on success otherwise ioctl error code.
 **************************************************************************/
static long  socTopIoctl(struct file *file,
                         unsigned int ioctl_num,
                         unsigned long ioctl_param,
                         struct semaphore *soc_sem)
{
    socTopParams_t  params;
   /*
    * extract the type and number bitfields, and don't decode
    * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok(  )
    */
   if (_IOC_TYPE(ioctl_num) != SOC_TOP_MODULE_ID)   return -ENOTTY;
   if (_IOC_NR(ioctl_num) > SOC_TOP_IOCTL_MAXNR)    return -ENOTTY;

   /* */
   down(soc_sem);

   if (copy_from_user(&params, (void __user *)ioctl_param, sizeof(socTopParams_t)))
   {
       printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
       return -EFAULT;
   }

   /*
     * Switch according to the ioctl called
     */
    switch (ioctl_num)
    {
        case SOC_TOP_REGISTER_GET:
        {
            socReadPhysRegister(params.regAddr, &params.regValue);

            if (copy_to_user((void __user *)ioctl_param, &params, sizeof(socTopParams_t)))
            {
                printk(KERN_ERR"\n%s: failed to copy to user\n", __FUNCTION__);
                up(soc_sem);
                return -EFAULT;
            }
            break;
        }

        case SOC_TOP_REGISTER_SET:
        {
            socWritePhysRegister(params.regAddr, params.regValue);
            break;
        }

        default:  /* redundant, as cmd was checked against MAXNR */
            up(soc_sem);
            return -ENOTTY;
    }

    up(soc_sem);
    return 0;
}


module_init(socDriverInit);
module_exit(socDriverCleanup);
MODULE_AUTHOR("Texas Instruments, Inc");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cable Modem Management Interface driver");

