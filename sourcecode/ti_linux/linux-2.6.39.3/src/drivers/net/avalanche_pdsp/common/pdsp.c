/*
  GPL LICENSE SUMMARY

  Copyright(c) 2014 Intel Corporation.

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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>       /* everything... */
#include <linux/types.h>    /* size_t */
#include <linux/uaccess.h>
#include <linux/cdev.h>     /* cdev utilities */
#include <linux/device.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include "pdsp.h"
#include "ramtest.h"
#include <mach/puma.h>
#include <avalanche_pdsp_api.h>


#if (0)
#define PDSP_DBG(fmt,args...) printk(fmt , ##args)
#else
#define PDSP_DBG(x,...)
#endif


#if (1)
    #define PDSP_LOCK(sem)      __pdsp_lock()
    #define PDSP_UNLOCK(sem)    __pdsp_unlock()
#else
    #define PDSP_LOCK(sem)      down(sem)
    #define PDSP_UNLOCK(sem)    up(sem)
#endif 


/*
 * PDSP Registers structure.
 *  The structure instance variable points to PDSP register space directly.
 */
typedef volatile struct
{
    Uint32    control;             /* 0x00 */
    Uint32    status;              /* 0x04 */
    Uint32    wakeup_en;           /* 0x08 */
    Uint32    cycle_count;         /* 0x0C */
    Uint32    stall_count;         /* 0x10 */
    Uint32    pad0[3];
    Uint32    table_block_index0;  /* 0x20 */
    Uint32    table_block_index1;  /* 0x24 */
    Uint32    table_program_ptr0;  /* 0x28 */
    Uint32    table_program_ptr1;  /* 0x2C */

} pdsp_regs_t;

#define PDSP_CTL_BIG_EN_BIT         (1 << 14)
#define PDSP_CTL_EN_BIT             (1 <<  1)
#define PDSP_CTL_N_RST_BIT          (1 <<  0)
#define PDSP_REG_RUN_STATE_BIT      (1<<15)
#define PDSP_REG_SLEEP_BIT          (1<<2)
#define PDSP_REG_COUNT_ENABLE_BIT   (1<<3)
#define PDSP_REG_SINGLE_STEP_BIT    (1<<8)


/*
 * PDSP register space overlay pointer.
 *
 */
typedef pdsp_regs_t* PDSP_RegsOvly;

typedef Int32 (*pdsp_trg_cmd_func_t)(struct pdsp_info *, pdsp_cmd_t);
typedef Int32 (*pdsp_rsp_sts_func_t)(struct pdsp_info *);

typedef volatile struct pdsp_info
{
    pdsp_id_t               id;
    struct semaphore *      sem_ptr;
    Uint32 *                cmd_rsp_ram;
    Uint32                  cmd_id_mask;
    Uint32                  cmd_status_mask;
    Uint32 *                param_ram;
    Uint32                  wait_count;
    Uint32 *                isr_reg;
    Uint32                  isr_mask;

    pdsp_trg_cmd_func_t     trg_cmd;
    pdsp_rsp_sts_func_t     rsp_sts;

    PDSP_RegsOvly           regs;
    Uint32                  iram_size;
    Uint32 *                iram;
    Uint32 *                dbg_regs;

}
pdsp_info_t;


typedef volatile struct sram_info
{
    Uint32 *                sram;
    Uint32                  sram_size;
}
scr_ram_info_t;

static struct semaphore *   __pdsp_download_sem;
static Uint8 *              __pdsp_download_current_addr = NULL;
static Int32    __pdsp_trigger_cmd   (pdsp_info_t *pdsp, pdsp_cmd_t cmd);
static Int32    __pdsp_rsp_status    (pdsp_info_t *pdsp);




static scr_ram_info_t g_ram_info[] =
{
    { .sram = (Uint32 *)    IO_PHY2VIRT(0x03200000),   .sram_size = 0x0000A000 },
    { .sram = (Uint32 *)    IO_PHY2VIRT(0x03300000),   .sram_size = 0x0002D000 },
    { .sram = (Uint32 *)    IO_PHY2VIRT(0x03400000),   .sram_size = 0x0000E000 }
};

#define SIZE_4K     0x1000
#define SIZE_8K     0x2000

#if defined (CONFIG_MACH_PUMA6)
static pdsp_info_t g_pdsp_info[] =
{
    {   /* PPDSP */
        .id             = PDSP_ID_Prefetcher                        ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_PPDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_PPDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_status_mask= 0xFF000000                                ,
        .cmd_id_mask    = 0x0000FF00                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03400100) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03400104) ,
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status ,
        .isr_reg        = NULL                                      ,
        .isr_mask       = 0                                         ,
    },
    {   /* CPDSP */
        .id             = PDSP_ID_Classifier                        ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_CPDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_CPDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_id_mask    = 0x000000FF                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03400000) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03400010) ,
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status ,
        .isr_reg        = (Uint32 *)        AVALANCHE_NWSS_GENERAL_MAILBOX_STAT_REG ,
        .isr_mask       = 0x00000001                                ,
    },
    {   /* CPDSP2 */
        .id             = PDSP_ID_Classifier2                       ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_CPDSP2_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_CPDSP2_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_id_mask    = 0x000000FF                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03400004) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03400010) ,
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status ,
        .isr_reg        = (Uint32 *)        AVALANCHE_NWSS_GENERAL_MAILBOX_STAT_REG ,
        .isr_mask       = 0x00000002                                ,
    },
    {   /* MPDSP */
        .id             = PDSP_ID_Modifier                          ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_MPDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_MPDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_8K                                   ,
        .cmd_id_mask    = 0x000000FF                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03400008) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03400010) ,
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status ,
        .isr_reg        = (Uint32 *)        AVALANCHE_NWSS_GENERAL_MAILBOX_STAT_REG ,
        .isr_mask       = 0x00000004                                ,
    },
    {   /* QPDSP */
        .id             = PDSP_ID_QoS                               ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_QPDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_QPDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_id_mask    = 0x000000FF                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x0340000C) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03400010) ,
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status ,
        .isr_reg        = (Uint32 *)        AVALANCHE_NWSS_GENERAL_MAILBOX_STAT_REG ,
        .isr_mask       = 0x00000008                                ,
    },
    {   /* PrxPDSP */
        .id             = PDSP_ID_LAN_Proxy                         ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_LAN_PrxyPDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_LAN_PrxyPDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_id_mask    = 0x000000FF                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03400180) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03400184) ,
        .wait_count     = 10000000                              ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status ,
        .isr_reg        = NULL                                      ,
        .isr_mask       = 0                                         ,
    },
    {   /* CoePDSP */
        .id             = PDSP_ID_CoE                                ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_COE_PDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_COE_PDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_id_mask    = 0x000000FF                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03400280) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03400284) ,
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status ,
        .isr_reg        = NULL                                      ,
        .isr_mask       = 0                                         ,
    },
};

#else

static pdsp_info_t g_pdsp_info[] =
{
    {   /* PPDSP */
        .id             = PDSP_ID_Prefetcher                        ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_PPDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_PPDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_status_mask= 0xFF000000                                ,
        .cmd_id_mask    = 0x0000FF00                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03167C00) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03167C04) ,
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status 
        
    },
    {   /* CPDSP */
        .id             = PDSP_ID_Classifier                        ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_CPDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_CPDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_id_mask    = 0x000000FF                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03100000) , 
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03100004) , 
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status 
        
    },
    {   /* MPDSP */
        .id             = PDSP_ID_Modifier                          ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_MPDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_MPDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_id_mask    = 0x000000FF                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03110000) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03110004) ,
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status 
    },
    {   /* QPDSP */
        .id             = PDSP_ID_QoS                               ,
        .regs           = (PDSP_RegsOvly)   AVALANCHE_NWSS_QPDSP_CTRL_RGN_BASE ,
        .iram           = (Uint32 *)        AVALANCHE_NWSS_QPDSP_IRAM_RGN_BASE ,
        .iram_size      = SIZE_4K                                   ,
        .cmd_id_mask    = 0x000000FF                                ,
        .cmd_rsp_ram    = (Uint32 *)        IO_PHY2VIRT(0x03120000) ,
        .param_ram      = (Uint32 *)        IO_PHY2VIRT(0x03120004) ,
        .wait_count     = 10000000                                  ,
        .trg_cmd        = (pdsp_trg_cmd_func_t)   __pdsp_trigger_cmd,
        .rsp_sts        = (pdsp_rsp_sts_func_t)   __pdsp_rsp_status 
    },
};

#endif

static struct
{
    atomic_t                            lock_nesting;
    Uint32                              lock_state;
}
gPdspPrivate = { {0},0 };

static void  __pdsp_lock( void )
{
    Uint32  state;
    local_irq_save( state );  

    if ( 1 == atomic_add_return( 1, &gPdspPrivate.lock_nesting ) )
    {
        gPdspPrivate.lock_state = state;  
    }
}

static void  __pdsp_unlock( void )
{
    if ( 0 == atomic_sub_return( 1, &gPdspPrivate.lock_nesting ) )
    {
        local_irq_restore( gPdspPrivate.lock_state );
    }
}



static Int32 __pdsp_trigger_cmd(pdsp_info_t *pdsp, pdsp_cmd_t cmd)
{
    *pdsp->cmd_rsp_ram = cmd;
    if (pdsp->isr_reg)
    {
        *pdsp->isr_reg = pdsp->isr_mask;
    }
    return 0;
}

static Int32 __pdsp_rsp_status(pdsp_info_t *pdsp)
{
    Uint32 wait_count = pdsp->wait_count;

    if (pdsp->isr_reg)
    {
        while((*pdsp->isr_reg & pdsp->isr_mask) && wait_count)
        {
            wait_count--;
        }
    }
    else
    {
        while((*pdsp->cmd_rsp_ram & pdsp->cmd_id_mask) && wait_count)
        {
            wait_count--;
        }
    }

    if(!wait_count)
    {
        printk ("pdsp_rsp_status: Timeout waiting for PDSDP(%d).\n", pdsp->id);
        return (-1);
    }

    return 0;
}





Int32 pdsp_cmd_send    (pdsp_id_t               pdsp_id,
                        pdsp_cmd_t              cmd_word, 
                        void *wr_ptr,   Uint32  wr_word, 
                        void *rd_ptr,   Uint32  rd_word)
{
    pdsp_info_t *pdsp = &g_pdsp_info[pdsp_id];
    volatile Uint32 *param_buf    = pdsp->param_ram;
    Uint32* wr_wptr = (Uint32*) wr_ptr;
    Uint32* rd_wptr = (Uint32*) rd_ptr;
    Uint32 ret_code;

    if (pdsp->id != pdsp_id)
    {
        return -1;
    }

    PDSP_LOCK( g_pdsp_info[ pdsp_id ].sem_ptr );

    PDSP_DBG("COMMAND: %#x = %#x: ", (Uint32)pdsp->cmd_rsp_ram, (Uint32)cmd_word);
    while(wr_word--)
    {
        PDSP_DBG ("%#x = %#x ", (Uint32)param_buf, *wr_wptr);
        *param_buf++ = *wr_wptr++;
    }
    PDSP_DBG ("\n");

    pdsp->trg_cmd(pdsp, cmd_word);

    if(-1 == pdsp->rsp_sts(pdsp))
    {
        PDSP_UNLOCK( g_pdsp_info[ pdsp_id ].sem_ptr );
        return (-1);
    }

    if (pdsp->cmd_status_mask)
    {
        Uint32 mask = pdsp->cmd_status_mask;
        ret_code = *pdsp->cmd_rsp_ram & pdsp->cmd_status_mask;
        while (0 ==(mask & 0x1))
        {
            ret_code >>= 1;
            mask >>= 1;
        }
        param_buf = pdsp->param_ram;
    }
    else
    {
        ret_code = *pdsp->param_ram;
        param_buf = pdsp->param_ram + 1;
    }

    while(rd_word--)
        *rd_wptr++ = *param_buf++;

    PDSP_UNLOCK( g_pdsp_info[ pdsp_id ].sem_ptr );

    return ((ret_code == SR_RETCODE_SUCCESS) ? 0 : -ret_code);
}
EXPORT_SYMBOL(pdsp_cmd_send);




Int32 pdsp_control (pdsp_id_t   pdsp_id, Uint32 ctl_op, Ptr ctl_data)
{
    Int32   retcode = 0;

    if (g_pdsp_info[pdsp_id].id != pdsp_id)
    {
        return SRPDSP_EINVINDEX;
    }
    
    PDSP_LOCK( g_pdsp_info[ pdsp_id ].sem_ptr );

    switch (ctl_op)
    {
        case PDSPCTRL_HLT:
            g_pdsp_info[pdsp_id].regs->control &= ~(PDSP_CTL_EN_BIT);
            break;

        case PDSPCTRL_STEP:
            g_pdsp_info[pdsp_id].regs->control 
                    = (g_pdsp_info[pdsp_id].regs->control & ~(PDSP_CTL_EN_BIT)) | (PDSP_REG_SINGLE_STEP_BIT);
            break;

        case PDSPCTRL_FREERUN:
            g_pdsp_info[pdsp_id].regs->control 
                    = (g_pdsp_info[pdsp_id].regs->control | (PDSP_CTL_EN_BIT)) & ~(PDSP_REG_SINGLE_STEP_BIT);
            break;

        case PDSPCTRL_RESUME:
            g_pdsp_info[pdsp_id].regs->control |= (PDSP_CTL_EN_BIT);
            break;

        case PDSPCTRL_RST:
            {
                
                int wait_count = 10000000;
                g_pdsp_info[pdsp_id].regs->control = 0;

                while(wait_count--)
                {
                    if(g_pdsp_info[pdsp_id].regs->control & (PDSP_CTL_N_RST_BIT))
                    {
                        break;
                    }
                }

                if(!wait_count) 
                {
                    printk ("Timeout waiting for reset complete for PDSDP(%d)\n", pdsp_id);
                    retcode = SRPDSP_EINTERROR;
                    break;
                }
            }
            
            break;
                
        case PDSPCTRL_START:
            {
                g_pdsp_info[pdsp_id].regs->control = ((*((Uint16*)ctl_data)) << 16) 
                                                    | (PDSP_CTL_EN_BIT) 
                                                    | (PDSP_REG_COUNT_ENABLE_BIT);

                if (g_pdsp_info[pdsp_id].rsp_sts( &g_pdsp_info[pdsp_id] ))
                {
                    retcode = SRPDSP_EINTERROR;
                    break;
                }
            }
            break;


        default:
            printk ("Unsupported PDSP control option (%d)\n", ctl_op);
            retcode = SRPDSP_EINVCMD;
            break;
    }

    PDSP_UNLOCK( g_pdsp_info[ pdsp_id ].sem_ptr );

    return retcode;
}



/* **************************************************************************************** */
/*                                                                                          */
/*                                                                                          */
/*                                                                                          */
/*                  PDSP character device and related data                                  */
/*                                                                                          */
/*                                                                                          */
/*                                                                                          */
/* **************************************************************************************** */

static ssize_t __pdspDownload (struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    if (NULL == __pdsp_download_current_addr)
    {
        return -ERESTARTSYS;
    }

    /*Copy the internal HAL data structure*/
    if (copy_from_user((void *)__pdsp_download_current_addr, buf, count))
    {
        printk("%s : failed to copy data from user\n", __FUNCTION__);
        return -EFAULT;
    }

    __pdsp_download_current_addr += count;
    *ppos += count;
    return   count;
}


static long __pdspIoctl ( struct file * filp , unsigned int cmd , unsigned long arg )
{
    pdsp_id_t   pdsp_id;
    Int32       rc;
    Uint32      pdsp_param = 0;
   
    if (copy_from_user(&pdsp_id, (void __user *)arg, sizeof(pdsp_id)))
    {
        printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
        return -EFAULT;
    }

    switch (cmd)
    {
    case PDSP_DRIVER_PUT_CMD:
        {
            pdsp_cmd_params_t   usr_params;

            if (copy_from_user(&usr_params, (void __user *)arg, sizeof(usr_params)))
            {
                printk(KERN_ERR"\n%s: failed to copy from user\n", __FUNCTION__);
                return -EFAULT;
            }

            if (rc = pdsp_cmd_send( pdsp_id, 
                           usr_params.cmd,
                           usr_params.params,   usr_params.params_len/sizeof(Uint32),
                           usr_params.params,   usr_params.params_len/sizeof(Uint32) ))
            {
                printk(KERN_ERR"\n%s: failed to put command(%X) to the PDSP %d rc(%d)\n", __FUNCTION__, usr_params.cmd, pdsp_id, rc );
                return -EINVAL;
            }

            if (copy_to_user((void __user *)arg, &usr_params, sizeof(usr_params)))
            {
                printk(KERN_ERR"\n%s: failed to copy response to user\n", __FUNCTION__);
                return -EFAULT;
            }
        }
        break;

    case PDSP_DRIVER_RESET_PDSP:
        {
            if (rc = pdsp_control( pdsp_id, PDSPCTRL_RST, &pdsp_param ))
            {
                printk(KERN_ERR"\n%s: failed to reset the PDSP %d rc(%d)\n", __FUNCTION__, pdsp_id, rc );
                return -EINVAL;
            }
        }
        break;

    case PDSP_DRIVER_START_PDSP:
        {
            if (rc = pdsp_control( pdsp_id, PDSPCTRL_START, &pdsp_param ))
            {
                printk(KERN_ERR"\n%s: failed to start the PDSP %d rc(%d)\n", __FUNCTION__, pdsp_id, rc );
                return -EINVAL;
            }
        }
        break;

    case PDSP_DRIVER_TEST_IRAM:
        {
            if ( g_pdsp_info[pdsp_id].id != pdsp_id )
            {
                return -EINVAL;
            }

            PDSP_LOCK( g_pdsp_info[ pdsp_id ].sem_ptr );

            if (rc = avalanche_do_ram_test( g_pdsp_info[pdsp_id].iram , g_pdsp_info[pdsp_id].iram_size ))
            {
                printk("###############################################################################\n");
                printk(" PP PDSP(%d) IRAM check failed, ret=%d \n", pdsp_id, rc );
                printk("###############################################################################\n");
                BUG();
            }

            memset( g_pdsp_info[pdsp_id].iram, 0, g_pdsp_info[pdsp_id].iram_size );

            PDSP_UNLOCK( g_pdsp_info[ pdsp_id ].sem_ptr );

            if (rc)
            {
                return -EIO; 
            }
        }
        break;

    case PDSP_DRIVER_DOWNLOAD_START:
        {
            down(__pdsp_download_sem);
            __pdsp_download_current_addr = g_pdsp_info[ pdsp_id ].iram;
        }
        break;

    case PDSP_DRIVER_DOWNLOAD_FINISH:
        {
            *g_pdsp_info[ pdsp_id ].cmd_rsp_ram = 0xFFFFFFFF;

            __pdsp_download_current_addr = NULL;
            up(__pdsp_download_sem);
        }
        break;

        default:
        break;
    }

    return 0; 
}

#define DEVICE_MAJOR    100
#define DEVICE_NAME     "pdsp"

static struct file_operations   pdspCdevFops =
{
    .owner              = THIS_MODULE,
    .unlocked_ioctl     = __pdspIoctl,
    .write              = __pdspDownload,
};

static void         __module_pdsp_exit (void)
{
    printk ( KERN_INFO " MODULE: PDSP Stop  ...\n");

#if 1
    /* Freeing the major number */
    unregister_chrdev ( DEVICE_MAJOR , DEVICE_NAME );
#else
    device_destroy( pdspdev_class, pdsp_dev );
    class_destroy( pdspdev_class );
    cdev_del( pdspc_dev );
    unregister_chrdev_region( pdsp_dev, 1 );
#endif

    if (__pdsp_download_sem)
    {
        kfree(__pdsp_download_sem);
    }

    if (g_pdsp_info[PDSP_ID_Classifier].sem_ptr) 
    {
        kfree(g_pdsp_info[PDSP_ID_Classifier].sem_ptr); 
    }

#if defined (CONFIG_MACH_PUMA6)
    if (g_pdsp_info[PDSP_ID_LAN_Proxy].sem_ptr)
    {
        kfree(g_pdsp_info[PDSP_ID_LAN_Proxy].sem_ptr); 
    }
#endif
    if (g_pdsp_info[PDSP_ID_Prefetcher].sem_ptr)
    {
        kfree(g_pdsp_info[PDSP_ID_Prefetcher].sem_ptr); 
    }

#if defined (CONFIG_MACH_PUMA6)
    if (g_pdsp_info[PDSP_ID_CoE].sem_ptr)
    {
        kfree(g_pdsp_info[PDSP_ID_CoE].sem_ptr); 
    }
#endif
}


static int __init   __module_pdsp_init (void)
{
    int                 ret;
    struct device * dev_ret;

    printk ( " MODULE: PDSP Start ...\n");

    /* ************************************************************************ */
    /*                                                                          */
    /*         Interface device initialization stuff ....                       */
    /*                                                                          */
    /* ************************************************************************ */

    /* Registering device */
    if ( register_chrdev ( DEVICE_MAJOR , DEVICE_NAME , &pdspCdevFops ) < 0 )
    {
        printk ( KERN_WARNING "memory: cannot obtain major number %d\n", DEVICE_MAJOR );
        return -EIO;
    }

    {
        struct semaphore *      sem;
        if (NULL ==(sem = kmalloc( sizeof(struct semaphore), GFP_KERNEL )))
        {
            __module_pdsp_exit();
            return -ENOMEM;
        }

        sema_init( sem, 1 );

        g_pdsp_info[ PDSP_ID_Classifier  ].sem_ptr =
#if defined (CONFIG_MACH_PUMA6)
        g_pdsp_info[ PDSP_ID_Classifier2 ].sem_ptr = 
#endif
        g_pdsp_info[ PDSP_ID_Modifier    ].sem_ptr = 
        g_pdsp_info[ PDSP_ID_QoS         ].sem_ptr = sem;

#if defined (CONFIG_MACH_PUMA6)
        if (NULL ==(sem = kmalloc( sizeof(struct semaphore), GFP_KERNEL )))
        {
            __module_pdsp_exit();
            return -ENOMEM;
        }

        sema_init( sem, 1 );


        g_pdsp_info[ PDSP_ID_LAN_Proxy   ].sem_ptr = sem;
#endif

        if (NULL ==(sem = kmalloc( sizeof(struct semaphore), GFP_KERNEL )))
        {
            __module_pdsp_exit();
            return -ENOMEM;
        }

        sema_init( sem, 1 );

        g_pdsp_info[ PDSP_ID_Prefetcher  ].sem_ptr = sem;

        if (NULL ==(sem = kmalloc( sizeof(struct semaphore), GFP_KERNEL )))
        {
            __module_pdsp_exit();
            return -ENOMEM;
        }

        sema_init( sem, 1 );

#if defined (CONFIG_MACH_PUMA6)
        if (NULL ==(sem = kmalloc( sizeof(struct semaphore), GFP_KERNEL )))
        {
            __module_pdsp_exit();
            return -ENOMEM;
        }

        sema_init( sem, 1 );

        g_pdsp_info[ PDSP_ID_CoE  ].sem_ptr = sem;
#endif
        __pdsp_download_sem = sem;
    }

    return 0;
}

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PDSP Driver");

module_init(__module_pdsp_init);
module_exit(__module_pdsp_exit);


