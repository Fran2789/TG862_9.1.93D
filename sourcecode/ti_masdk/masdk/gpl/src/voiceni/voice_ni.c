/*

  GPL LICENSE SUMMARY

  Copyright(c) 2012-2013 Intel Corporation.

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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include <linux/version.h>
#include <linux/unistd.h>
#include <asm/unistd.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/if.h>

#include <asm/irq.h>

#include "rtxerr.h"
#include "rtxuser.h"
#include "voice_ni.h"

#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include <generic/pal.h>
#include <generic/pal_cppi41.h>
#include <ti_hil.h>
#include <autoconf.h>

#ifdef CONFIG_MACH_PUMA6
#include <puma6_cppi.h>
#else
#include <puma5_cppi.h>
#endif

#ifdef CONFIG_TI_PACKET_PROCESSOR /* For PID base config */
#ifdef CONFIG_MACH_PUMA6
#include <puma6_pp.h>
#include "avalanche_pp_api.h"
#else
#include <puma5_pp.h>
#endif
#endif



//#define VOICENI_DEBUG

#ifdef VOICENI_DEBUG

    #define PRINT_DESC(size,addr)                             \
        {                                                     \
            unsigned int * tmp = (unsigned int *)(addr);      \
            unsigned int i;                                   \
            for (i=0; i<((size)/4); i++)                      \
            {                                                 \
                if (0 == i%4)                                 \
                {                                             \
                    printk("\n");                             \
                }                                             \
                printk("%08X ",*tmp++);                       \
            }                                                 \
            printk("\n");                                     \
        }

    #define DPRINTK(fmt, args...) printk(fmt, ## args)
#else
    #define PRINT_DESC(size,addr)
    #define DPRINTK(fmt, args...)
#endif


#ifdef CONFIG_TI_DEVICE_PROTOCOL_HANDLING
extern int ti_protocol_handler (struct net_device* dev, struct sk_buff *skb);
#endif

/* Master Control Block for Voice Ni network module */
static VOICENID_PRIVATE_T voicenid_mcb;

#ifdef CONFIG_MACH_PUMA6
Cppi4BufPool      c55bufPool = {BUF_POOL_MGR0,PAL_CPPI41_BMGR_POOL17};
#else
Cppi4BufPool      c55bufPool = {BUF_POOL_MGR0,BMGR0_POOL09};
#endif

/* structure for voiceni params retrieval via IOCTL */
static VOICENI_PP_CONFIG_T voiceni_pp_config;

static unsigned char *  c55bufferToSend = NULL;
static unsigned int     c55bufferLength;

/*********************************************************************************
* FUNCTION: voiceni_init_proc
*
**********************************************************************************
*
* DESCRIPTION: Forms output for /proc/voiceni file
*********************************************************************************/
static int voiceni_init_proc(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
    int len = 0;

    len += sprintf(buf + len, "VoiceNI proc entry\n");
    len += sprintf(buf + len, "===========================\n");
    len += sprintf(buf + len, "QueueMgrId:                  %d\n", voiceni_pp_config.cppi_queue_mgr_id);
    len += sprintf(buf + len, "FreeDescriptorDspQueueId:    %d\n", voiceni_pp_config.cppi_bd_queue_id);
    len += sprintf(buf + len, "FreeDescriptorInfraQueueId:  %d\n", voiceni_pp_config.cppi_infra_bd_queue_id);
    len += sprintf(buf + len, "VoiceNIRXQueueId:            %d\n", voiceni_pp_config.cppi_vni_rx_q);
    len += sprintf(buf + len, "VoiceNITXQueueId:            %d\n", voiceni_pp_config.cppi_vni_tx_q);
    len += sprintf(buf + len, "DSPRXQueueId:                %d\n", voiceni_pp_config.cppi_dsp_rx_queue_id);
    len += sprintf(buf + len, "DSPTXQueueId:                %d\n", voiceni_pp_config.cppi_dsp_tx_queue_id);
    len += sprintf(buf + len, "DSPCPPPI41SrcPortId:         %d\n", voiceni_pp_config.pid);
    len += sprintf(buf + len, "BufferPoolMrgId:             %d\n", voiceni_pp_config.buffer_pool_mgr_id);
    len += sprintf(buf + len, "BufferPoolNumId:             %d\n", voiceni_pp_config.buffer_pool_id);
    len += sprintf(buf + len, "BufferDescriptorSize:        %d\n", PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE);

    *eof = 1;

    return len;
}


/*********************************************************************************
* FUNCTION: voiceni_stats_proc
*
**********************************************************************************
*
* DESCRIPTION: Forms output for /proc/voiceni_stats file
*********************************************************************************/
static int voiceni_stats_proc(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{

    int len = 0;
    VOICENID_PRIVATE_T* priv  = (VOICENID_PRIVATE_T*)data;

    len += sprintf(buf + len, "Voice Network Interface Stats:\n");
    len += sprintf(buf + len, "=============================================\n");
    len += sprintf(buf + len, "Accum/RX Interrupts:                     %d\n", priv->rx_interrupt_count);
    len += sprintf(buf + len, "RX packets/BDs (packets from DSP)        %d\n", priv->rx_bd_count);
    len += sprintf(buf + len, "RX packets/BDs with null virtBD pointer  %d\n", priv->rx_bd_with_virtBD_null);
    len += sprintf(buf + len, "RX buffers freed (buffers from DSP)      %d\n", priv->rx_buffers_freed);

    len += sprintf(buf + len, "TX packets to network:                   %d\n", (int)priv->stats.tx_packets);

    len += sprintf(buf + len, "Packets from network:                    %d\n", priv->rx_skb_count);
    len += sprintf(buf + len, "BufferPool alloc failure:                %d\n", priv->buff_pop_failure);
    len += sprintf(buf + len, "BD Queue alloc failure:                  %d\n", priv->bd_queue_pop_failure);
    len += sprintf(buf + len, "TX bytes to network:                     %d\n", (int)priv->stats.tx_bytes);
    len += sprintf(buf + len, "RX packets from network:                 %d\n", (int)priv->stats.rx_packets);
    len += sprintf(buf + len, "RX bytes fom network:                    %d\n", (int)priv->stats.rx_bytes);
    len += sprintf(buf + len, "TX BD count                              %d\n", priv->tx_bd_count);

    *eof = 1;
    return len;
}


static int voiceni_send_cmd_handler(int argc, char* argv[], VOICENID_PRIVATE_T *priv)
{
    if (strcmp(argv[1], "fromC55") == 0)
    {
        Cppi4Desc *virtBD, *phyBD;
        Ptr         pBuffPtr;
        Ptr         vBuffPtr;
        Cppi4EmbdBuf *descBufInfo;

        /* Need to allocate free BD and buffer*/
        pBuffPtr = PAL_cppi4BufPopBuf(priv->pal_hnd, c55bufPool);

        DPRINTK("\n    Got Buffer from BM [%08X] desired len: %d\n", (unsigned  int)pBuffPtr, c55bufferLength);

        if (pBuffPtr == NULL)
        {
            return -ENOMEM;
        }

        phyBD = (Cppi4Desc*) PAL_cppi4QueuePop(priv->free_bd_queue);

        DPRINTK("    Got Descriptor     [%08X]\n", (unsigned int)phyBD);

        if (phyBD == NULL)
        {
            PAL_cppi4BufDecRefCnt(priv->pal_hnd, c55bufPool, pBuffPtr);
            return -ENOMEM;
        }

        virtBD = (Cppi4Desc *)avalanche_no_OperSys_memory_phys_to_virt((unsigned int)phyBD);

        DPRINTK("    Got Descriptor virt[%08X]\n", (unsigned int)virtBD);

        PRINT_DESC( PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE, virtBD );

        /* copy buffer into buffer and update BD if required */
        if (virtBD != NULL)
        {
            vBuffPtr = (Ptr)avalanche_no_OperSys_memory_phys_to_virt((unsigned int)pBuffPtr);

            DPRINTK("    Buffer v[%08X]\n", (unsigned int)vBuffPtr);

            memcpy( (void*)vBuffPtr, c55bufferToSend, c55bufferLength);

#ifdef CONFIG_MACH_PUMA6
            descBufInfo = &(virtBD->Buf);
#else
            descBufInfo = &(virtBD->Buf[1]);
#endif

            memset( descBufInfo, 0, sizeof(virtBD->Buf) );
            descBufInfo->BufInfo = 
                CPPI41_EM_BUF_VALID_MASK |
                (voiceni_pp_config.buffer_pool_mgr_id  << CPPI41_EM_BUF_MGR_SHIFT) |
                (voiceni_pp_config.buffer_pool_id << CPPI41_EM_BUF_POOL_SHIFT) |
                c55bufferLength;
            descBufInfo->BufPtr = (UINT32)pBuffPtr;

            virtBD->descInfo &= ~(CPPI41_EM_DESCINFO_PKTLEN_MASK);
            virtBD->descInfo |= (c55bufferLength) << CPPI41_EM_DESCINFO_PKTLEN_SHIFT;

            PAL_CPPI4_CACHE_WRITEBACK(vBuffPtr, c55bufferLength);
        }
        else
        {
            PAL_cppi4BufDecRefCnt(priv->pal_hnd, c55bufPool, pBuffPtr);
            return -ENOMEM;
        }

        PAL_CPPI4_CACHE_WRITEBACK(virtBD, PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE);

        PRINT_DESC( PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE, virtBD );

        PAL_cppi4QueuePush (priv->dsp_tx_queue,
                            (Ptr) phyBD,
                            PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE),
                            c55bufferLength);

        /* Work is completed. */
        return 0;
    }

    /* Control comes here if the command was not understood. */
    return -EINVAL;
}

static int voiceni_set_cmd_handler(int argc, char* argv[])
{
    unsigned char **dbgPktP     = NULL;
    unsigned int  *dbgPktSizeP  = NULL;

    if (strcmp(argv[1], "C55buffer") == 0)
    {
        dbgPktP     = &c55bufferToSend;
        dbgPktSizeP = &c55bufferLength;

        if (dbgPktSizeP != NULL)
        {
            unsigned int templateSize = 0;
            Char *endptr;

            if (argc != 4)
            {
                printk("command format: set c55buffer PktString PktLength\n");
                return -EINVAL;
            }

            /* if dbgPktP is already allocated - make sure to free it before reallocating */
            if (*dbgPktP)
            {
                kfree(*dbgPktP);
            }

            /* Allocate *dbgPktP and reset it */
            *dbgPktSizeP = (Uint32)simple_strtol(argv[3], NULL, 0);
            *dbgPktP = kmalloc(*dbgPktSizeP, GFP_KERNEL);
            if (*dbgPktP == NULL)
            {
                printk("Could not allocate %d bytes for dbgPktP\n", *dbgPktSizeP);
                return -ENOMEM;
            }
            memset(*dbgPktP, 0, *dbgPktSizeP);

            /* Copy the packet */
            endptr = argv[2];
            while (true)
            {
                long val;
                val = (Int32)simple_strtol(endptr, &endptr, 16);
                if ((val < 0) || (val > 0xFF) || ((*endptr != ':') && (*endptr != '-') && (*endptr != '\0')))
                {
                    printk("\nError found\n");
                    kfree(*dbgPktP);
                    return -EINVAL;
                }
                (*dbgPktP)[templateSize++] = (Char)val;
                if (*endptr == '\0')
                {
                    break;
                }
                if (templateSize > *dbgPktSizeP)
                {
                    printk("Warning: packet size received is shorter than the template - truncating packet\n");
                    break;
                }
                endptr++;
            }

            /* Work is completed. */
            return 0;
        }
    }

    /* Control comes here if the command was not understood. */
    return -EINVAL;
}


static int voiceni_write_cmds (struct file *file, const char *buffer, unsigned long count, void *data)
{
    char*   voiceni_cmd;
    char*   argv[10];
    int     argc = 0;
    char*   ptr_cmd;
    char*   delimitters = " \n\t";
    char*   ptr_next_tok;

    unsigned char   **dbgPktP       = NULL;
    unsigned int     *dbgPktSizeP   = NULL;

    voiceni_cmd = kmalloc(count, GFP_KERNEL);
    if (voiceni_cmd == NULL)
    {
        printk("Could not allocate %d bytes for voiceni_cmd\n", (unsigned int)count);
        return -ENOMEM;
    }

    /* Initialize the buffer before using it. */
    memset ((void *)&voiceni_cmd[0], 0, count);
    memset ((void *)&argv[0], 0, sizeof(argv));

    /* Copy from user space. */
    if (copy_from_user (voiceni_cmd, buffer, count))
    {
        kfree(voiceni_cmd);
        return -EFAULT;
    }

    ptr_next_tok = voiceni_cmd;

    /* Tokenize the command. Check if there was a NULL entry. If so be the case the
     * user did not know how to use the entry. Print the help screen. */
    ptr_cmd = strsep(&ptr_next_tok, delimitters);
    if (ptr_cmd == NULL)
    {
        kfree(voiceni_cmd);
        return -EINVAL;
    }

    /* Parse all the commands typed. */
    do
    {
        /* Extract the first command. */
        argv[argc++] = ptr_cmd;

        /* Validate if the user entered more commands.*/
        if (argc >=10)
        {
            printk ("ERROR: Incorrect too many parameters dropping the command\n");
            kfree(voiceni_cmd);
            return -EFAULT;
        }

        /* Get the next valid command. */
        ptr_cmd = strsep(&ptr_next_tok, delimitters);
    } while (ptr_cmd != NULL);

    /* We have an extra argument when strsep is used instead of strtok */
    argc--;

    /******************************* Command Handlers *******************************/

    /* Display Command Handlers */
    if (strncmp(argv[0], "set", strlen("show")) == 0)
    {
        /* Call the Show Command Handler. */
        if (voiceni_set_cmd_handler (argc, argv) < 0)
        {
            kfree(voiceni_cmd);
            return -EFAULT;
        }
    }
    else if (strcmp(argv[0], "print_buffer") == 0)
    {
        dbgPktP =       &c55bufferToSend;
        dbgPktSizeP =   &c55bufferLength;

        if (dbgPktSizeP != NULL)
        {
            Uint32 i;

            if (*dbgPktP)
            {
                Bool backspaceNeeded = false;
                for (i = 0; i < *dbgPktSizeP; i++)
                {
                    backspaceNeeded = true;
                    printk("%02X:", (*dbgPktP)[i]);
                    if ((i & 0x7) == 0x7)
                    {
                        printk("\b \b\n");
                        backspaceNeeded = false;
                    }
                }
                if (backspaceNeeded)
                {
                    printk("\b \b");
                }
                printk("\n");
            }
            else
            {
                printk("Was not set!!!\n");
            }
        }
    }
    else if (strcmp(argv[0], "send") == 0)
    {
        /* Call the Send Command Handler. */
        if (voiceni_send_cmd_handler (argc, argv, data) < 0)
        {
            kfree(voiceni_cmd);
            return -EFAULT;
        }
    }


    kfree(voiceni_cmd);
    return count;
}


/*********************************************************************************
* FUNCTION: voiceni_rx_processing
*
**********************************************************************************
*
* DESCRIPTION:  Tasklet routine to process packets sent by DSP via the PP. Runs in
*               context of voiceni_rx_isr
*
*********************************************************************************/
static void voiceni_rx_processing(unsigned long data)
{
    struct net_device *dev  = (struct net_device*)data;
    VOICENID_PRIVATE_T *priv = netdev_priv(dev);
    Cppi4Desc *virtBD, *phyBD;
    struct sk_buff *newskb;
    Cppi4EmbdBuf *descBufInfo;

    DPRINTK("%s:%d\n",__FUNCTION__,__LINE__);

    while (avalanche_intd_get_interrupt_count(VOICENI_INTD_HOST_NUM, voiceni_pp_config.cppi_acc_rx_ch))
    {
        while ((phyBD = (Cppi4Desc*)((unsigned long)*priv->rxAcc_chan_list[0] & QMGR_QUEUE_N_REG_D_DESC_ADDR_MASK)))
        {
            unsigned int vBuffPtr;
            priv->rx_bd_count++;

            DPRINTK("%s:%d Got BD[%08X]",__FUNCTION__,__LINE__,(unsigned int)phyBD);

            if (((UINT32)phyBD >= AVALANCHE_PP_PHY_ADDR_BASE) && ((UINT32)phyBD < AVALANCHE_PP_PHY_ADDR_END))
            {
                /* Regular case, we get a descriptor from the PP prefetch internal RAM */
                virtBD = (Cppi4Desc*) IO_PHY2VIRT((UINT32)phyBD);
            }
            else
            {
                /* Only in PSM - no prefetch is being done, so we get the descriptor from the DSP reserved memory */
                virtBD = (Cppi4Desc *)avalanche_no_OperSys_memory_phys_to_virt((unsigned int)phyBD);
            }

#ifdef CONFIG_MACH_PUMA6
            descBufInfo = &virtBD->Buf;
#else
            descBufInfo = &(virtBD->Buf[1]);
#endif

            DPRINTK(" v[%08X]\n", (unsigned int)virtBD);

            if (virtBD == NULL)
            {
                printk("ERROR: voiceni_rx_processing, virtual BD is NULL, physical BD address: 0x%x\n", (UINT32)phyBD);
                priv->rxAcc_chan_list[0]++;
                priv->rx_bd_with_virtBD_null++;
                continue;
            }

            PAL_CPPI4_CACHE_INVALIDATE(virtBD, PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE);

            PRINT_DESC( PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE, virtBD );

            vBuffPtr = avalanche_no_OperSys_memory_phys_to_virt(descBufInfo->BufPtr);
            PAL_CPPI4_CACHE_INVALIDATE(vBuffPtr, (descBufInfo->BufInfo & CPPI41_EM_BUF_BUFFLEN_MASK));

            newskb = dev_alloc_skb(MAX_SKB_SIZE);

            if(virtBD->EPI[1])
            {
                memcpy(newskb->pp_packet_info.ti_epi_header, &(virtBD->EPI[0]), sizeof(newskb->pp_packet_info.ti_epi_header));
            }

            memcpy(newskb->data, (void*)vBuffPtr, descBufInfo->BufInfo & CPPI41_EM_BUF_BUFFLEN_MASK);

            PAL_cppi4EmbDescRecycle( priv->pal_hnd, virtBD, phyBD );

            skb_put(newskb, descBufInfo->BufInfo & CPPI41_EM_BUF_BUFFLEN_MASK);
            skb_reset_mac_header(newskb);

#ifdef CONFIG_TI_DEVICE_PROTOCOL_HANDLING
            newskb->dev = dev;
            newskb->skb_iif = dev->ifindex; /* Set vni0 as the ingress inerface for the PP_Path counters */
            /* Pass the packet to the device specific protocol handler */
            if (ti_protocol_handler (newskb->dev, newskb) < 0)
            {
                /* Device Specific Protocol handler has "captured" the packet
                * and does not want to send it up the networking stack; so
                * return immediately.after freeing bd/buffer*/
                priv->rxAcc_chan_list[0]++;
                continue;
            }
#endif
            newskb->dev = priv->ptr_emtani_device;
            dev_queue_xmit(newskb);
            priv->stats.rx_packets++;
            priv->stats.rx_bytes += newskb->len;

            priv->rxAcc_chan_list[0]++;
        }

        /* Update the list entry for next time */
        priv->rxAcc_chan_list[0] = PAL_cppi4AccChGetNextList(priv->acc_hnd);
        avalanche_intd_set_interrupt_count (0, voiceni_pp_config.cppi_acc_rx_ch, 1);
    }

    avalanche_intd_write_eoi (voiceni_pp_config.cppi_acc_rx_intv);
}

/*******************************************************************************
* FUNCTION:    voiceni_rx_isr
*
********************************************************************************
*
* DESCRIPTION: voiceni receive isr routine
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static irqreturn_t voiceni_rx_isr (int irq, void *context, struct pt_regs *regs)
{
    VOICENID_PRIVATE_T* priv  = ((VOICENID_PRIVATE_T*)context);
    priv->rx_interrupt_count++;
    tasklet_schedule(&(priv->tx_tasklet));

    return IRQ_RETVAL(1);
}


/*******************************************************************************
* FUNCTION:    init_voiceni_ccpi4_accum_channel
*
********************************************************************************
*
* DESCRIPTION: Initialize and open voiceni accumulator channel
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static int init_voiceni_ccpi4_accum_channel(VOICENID_PRIVATE_T *priv)
{
    Cppi4Queue                 voiceni_rx_queue = { 0, voiceni_pp_config.cppi_vni_rx_q };
    Cppi4AccumulatorCfg        cfg;

    cfg.accChanNum             = voiceni_pp_config.cppi_acc_rx_ch;
    cfg.queue                  = voiceni_rx_queue;
    cfg.mode                   = 0;
    cfg.list.maxPageEntry      = VOICENI_ACC_PAGE_NUM_ENTRY;
    cfg.list.listEntrySize     = VOICENI_ACC_ENTRY_TYPE;        /* Only interested in register 'D' which has the desc pointer */
    cfg.list.listCountMode     = 0;                             /* Zero indicates null terminated list. */
    cfg.list.pacingMode        = 1;                             /* dont Wait for time since last interrupt, should we wait delay will be
                                                                   at most 1 ms/packet, tm: changes from 0 to 3*/
    cfg.pacingTickCnt          = 40;                            /* Wait for 1000uS == 1ms, changed from 40 to 0 */
    cfg.list.maxPageCnt        = VOICENI_ACC_NUM_PAGE;          /* Use two pages */
    cfg.list.stallAvoidance    = 1;                             /* Use the stall avoidance feature */

    if(!(cfg.list.listBase = kzalloc(VOICENI_ACC_LIST_BYTE_SZ, GFP_KERNEL)))
    {
        printk(" init_voiceni_ccpi4_accum_channel: Unable to allocate list page\n");
        return -1;
    }
    else
    {
         DPRINTK(" init_voiceni_ccpi4_accum_channel: Able to allocate list page\n");
    }
    /* make sure memory allocated in kzalloc which is probably cached is actually written to
    actual memory by doging cache writeback */
    PAL_CPPI4_CACHE_WRITEBACK((unsigned long)cfg.list.listBase, VOICENI_ACC_LIST_BYTE_SZ);

    if(!(priv->acc_hnd = PAL_cppi4AccChOpen(priv->pal_hnd, &cfg)))
    {
        printk("VOICE NI module, Unable to open accumulator channel\n");
        kfree(cfg.list.listBase);
        return -1;
    }

    priv->rxAcc_chan_list_base[0] = priv->rxAcc_chan_list[0] = PAL_cppi4AccChGetNextList(priv->acc_hnd);

    if(request_irq (VOICENI_RXINT_NUM, (irq_handler_t)voiceni_rx_isr, IRQF_DISABLED, VOICENI_DEV_NAME,(void*) priv))
    {
        printk("VOICE NI module, Unable to get IRQ\n");
        return -1;
    }

    return 0;
}


/*******************************************************************************
* FUNCTION:    init_voiceni_free_bd_queue
*
********************************************************************************
*
* DESCRIPTION: Initialize and open voiceni free buffer descriptor queue
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static int init_voiceni_free_bd_queue(VOICENID_PRIVATE_T* priv)
{
    Cppi4Queue  free_bd_queue;

    free_bd_queue.qMgr = voiceni_pp_config.cppi_queue_mgr_id;
    free_bd_queue.qNum = voiceni_pp_config.cppi_bd_queue_id;

    priv->free_bd_queue = PAL_cppi4QueueOpen(priv->pal_hnd, free_bd_queue);
    if (priv->free_bd_queue == NULL)
    {
        printk("VOICE NI module, DSP Free BD Queue Handle is NULL\n");
        return -1;
    }
    else
    {
        DPRINTK("VOICE NI module, DSP Free BD Queue Handle is VALID, free bd queue: %x\n", (int)priv->free_bd_queue);
    }

    free_bd_queue.qNum = voiceni_pp_config.cppi_infra_bd_queue_id;
    priv->free_infra_bd_queue = PAL_cppi4QueueOpen(priv->pal_hnd, free_bd_queue);
    if (priv->free_infra_bd_queue == NULL)
    {
        printk("VOICE NI module, Infrastructure DMA Free BD Queue Handle is NULL\n");
        return -1;
    }
    else
    {
        DPRINTK("VOICE NI module, Infrastructure DMA Free BD Queue Handle is VALID, free bd queue: %x\n", (int)priv->free_infra_bd_queue);
    }

    return 0;
}

/*******************************************************************************
* FUNCTION:    init_voiceni_free_bd_pool
*
********************************************************************************
*
* DESCRIPTION:  Initialize and open voiceni free buffer descriptor pool, initialize
*               all buffer descriptors
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static int init_voiceni_free_bd_pool(VOICENID_PRIVATE_T* priv)
{
    int i_bd;

    priv->free_bd_pool =  PAL_cppi4AllocDesc(priv->pal_hnd, 0, PAL_CPPI41_VOICE_DSP_C55_EMB_BD_COUNT, PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE);

    if ( priv->free_bd_pool == NULL )
    {
        return -1;
    }
    else
    {
        priv->free_bd_pool = (Ptr)avalanche_no_OperSys_memory_phys_to_virt((unsigned int)priv->free_bd_pool);

        DPRINTK("VOICE NI module, Free BD Pool Handle is VALID\n");
        DPRINTK("VOICE NI Fill free discriptor queue\n");
        DPRINTK("****init_voiceni_free_bd_pool, buffer descriptor pool base address: 0x%08x\n", (unsigned int)priv->free_bd_pool);
        for (i_bd = 0; i_bd < PAL_CPPI41_VOICE_DSP_C55_EMB_BD_COUNT; i_bd++)
        {
            UINT16 bd_queue_id;
            PAL_Cppi4QueueHnd free_bd_queue;

            if (i_bd < PAL_CPPI41_SR_VOICE_DSP_VNI_FD_EMB_Q_COUNT)
            {
                bd_queue_id = voiceni_pp_config.cppi_bd_queue_id;
                free_bd_queue = priv->free_bd_queue;
            }
            else
            {
                bd_queue_id = voiceni_pp_config.cppi_infra_bd_queue_id;
                free_bd_queue = priv->free_infra_bd_queue;
            }

            Cppi4Desc *bd = (Cppi4Desc *)GET_VOICENI_BD_PTR(priv->free_bd_pool, i_bd);
            Cppi4Desc *phyBd = (Cppi4Desc *) avalanche_no_OperSys_memory_virt_to_phys((unsigned int) bd);

            DPRINTK("==[%p]->[%p]==\n", bd, phyBd);

            PAL_osMemSet(bd, 0, PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE);
            /* descInfo correlates to seciotn 2.2.3.1, page 17 */
            bd->descInfo = CPPI41_EM_DESCINFO_DTYPE_EMBEDDED | CPPI41_EM_DESCINFO_SLOTCNT_MYCNT;
            bd->tagInfo  =  (voiceni_pp_config.pid<< CPPI41_EM_TAGINFO_SRCPORT_SHIFT) | (0x3fff << CPPI41_EM_TAGINFO_DSTTAG_SHIFT);
            /* pktInfo correlates to section 2.2.3.3, page 18 of cppi specification */
            bd->pktInfo  =  (PAL_CPPI4_HOSTDESC_PKT_TYPE_ETH << CPPI41_EM_PKTINFO_PKTTYPE_SHIFT)
                          | (CPPI41_EM_PKTINFO_RETPOLICY_RETURN)
                          | (voiceni_pp_config.cppi_queue_mgr_id    << CPPI41_EM_PKTINFO_RETQMGR_SHIFT)
                          | (1                  << CPPI41_EM_PKTINFO_EOPIDX_SHIFT)
                          | (0                  << CPPI41_EM_PKTINFO_ONCHIP_SHIFT)
                          | (bd_queue_id << CPPI41_EM_PKTINFO_RETQ_SHIFT);

            PAL_CPPI4_CACHE_WRITEBACK(bd, PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE);


            PAL_cppi4QueuePush (free_bd_queue, phyBd, PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE), 0);
        }
    }

    {
        unsigned int *  poolPhysAddr;

        if ( avalanche_alloc_no_OperSys_memory(eNO_OperSys_VoiceNI, voiceni_pp_config.buffer_pool_count * voiceni_pp_config.buffer_pool_size, (unsigned int *)&poolPhysAddr) != 0)
        {
            printk("ERROR: [VoiceNI] Alloc API failed!\n");
            return -1;
        }

        if ((PAL_cppi4BufPoolDirectInit(priv->pal_hnd, c55bufPool,
                                    voiceni_pp_config.buffer_pool_refcount,
                                    voiceni_pp_config.buffer_pool_size,
                                    voiceni_pp_config.buffer_pool_count,
                                    (void *)poolPhysAddr )) == NULL)
        {
            printk ("PAL_cppi4BufPoolDirectInit for pool %d FAILED.\n", voiceni_pp_config.buffer_pool_id);
            return -1;
        }
    }

    return 0;
}


/*******************************************************************************
* FUNCTION:    init_voiceni_ccpi41_queues
*
********************************************************************************
*
* DESCRIPTION:  Initialize and open voiceni transmit and receive cppi queues
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static int init_voiceni_ccpi41_queues(VOICENID_PRIVATE_T *priv)
{
    Cppi4Queue         voiceni_rx_queue = {voiceni_pp_config.cppi_queue_mgr_id, voiceni_pp_config.cppi_vni_rx_q};
    Cppi4Queue         voiceni_tx_queue = {voiceni_pp_config.cppi_queue_mgr_id, voiceni_pp_config.cppi_vni_tx_q};

    /* Init the CPPI RX queue for voice NI */
    priv->voiceni_rx_queue =  PAL_cppi4QueueOpen(priv->pal_hnd, voiceni_rx_queue);
    if (priv->voiceni_rx_queue == NULL)
    {
         printk("ERROR: VOICE NI module, Voice NI TX Queue Handle is NULL\n");
    }
    else
    {
        DPRINTK("VOICE NI module, Voice NI RX Queue Handle is VALID\n");
    }

    /* Now init the CPPI TX queue for voice NI */
    priv->voiceni_tx_queue =  PAL_cppi4QueueOpen(priv->pal_hnd, voiceni_tx_queue);

    if (priv->voiceni_rx_queue == NULL)
    {
         printk("ERROR: VOICE NI module, Voice NI TX Queue Handle is NULL\n");
    }
    else
    {
        DPRINTK("VOICE NI module, Voice NI TX Queue Handle is VALID\n");
    }

    /* for now return 0 */
    return 0;
}


/*******************************************************************************
* FUNCTION:    init_dsp_ccpi41_queues
*
********************************************************************************
*
* DESCRIPTION:  Initialize and open DSP transmit and receive cppi queues
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static int init_dsp_ccpi41_queues(VOICENID_PRIVATE_T *priv)
{
    Cppi4Queue      dsp_rx_queue = { voiceni_pp_config.cppi_queue_mgr_id, voiceni_pp_config.cppi_dsp_rx_infra_qid };
    Cppi4Queue      dsp_tx_queue = { voiceni_pp_config.cppi_queue_mgr_id, voiceni_pp_config.cppi_dsp_tx_queue_id};

    /* Init the CCPI RX queue for DSP */
    priv->dsp_rx_queue =  PAL_cppi4QueueOpen(priv->pal_hnd, dsp_rx_queue);
    if (priv->dsp_rx_queue == NULL)
    {
         printk("ERROR: VOICE NI module, DSP RX Queue Handle is NULL\n");
    }
    else
    {
        DPRINTK("VOICE NI module, DSP RX Queue Handle is VALID\n");
    }


    /* Now init the CCPI TX queue for DSP */
    priv->dsp_tx_queue  =  PAL_cppi4QueueOpen(priv->pal_hnd, dsp_tx_queue);
    if (priv->dsp_tx_queue  == NULL)
    {
        printk("ERROR: VOICE NI module, DSP TX Queue Handle is NULL\n");
    }
    else
    {
        DPRINTK("VOICE NI module, DSP TX Queue Handle is VALID\n");
    }

    /* for now return 0 */
    return 0;
}


/**************************************************************************/
/*! \fn static void setupRecycleInfra (PAL_Handle hnd)
 **************************************************************************
 *  \brief Setup infrastructure DMA for recycling use resources.
 *  \return none.
 **************************************************************************/
static int init_c55_input_DMA(VOICENID_PRIVATE_T *priv)
{
    Cppi4TxChInitCfg txCh;
    Cppi4RxChInitCfg rxCh;
    PAL_Cppi4TxChHnd cppi4TxChHnd;
    PAL_Cppi4RxChHnd cppi4RxChHnd;

    /* Set up Rx channel */
    rxCh.chNum              = voiceni_pp_config.cppi_dsp_rx_infra_ch;
    rxCh.dmaNum             = voiceni_pp_config.cppi_dsp_rx_infra_dma;
    rxCh.defDescType        = CPPI41_DESC_TYPE_EMBEDDED;
    rxCh.sopOffset          = 0;
    rxCh.rxCompQueue.qMgr   = voiceni_pp_config.cppi_queue_mgr_id;
    rxCh.rxCompQueue.qNum   = voiceni_pp_config.cppi_dsp_rx_queue_id;
    rxCh.u.embeddedPktCfg.fdQueue.qMgr = voiceni_pp_config.cppi_queue_mgr_id;
    rxCh.u.embeddedPktCfg.fdQueue.qNum = voiceni_pp_config.cppi_infra_bd_queue_id;
    rxCh.u.embeddedPktCfg.numBufSlot = (EMSLOTCNT-1);
    rxCh.u.embeddedPktCfg.sopSlotNum = 1;
    rxCh.u.embeddedPktCfg.fBufPool[0].bMgr  = voiceni_pp_config.buffer_pool_mgr_id;
    rxCh.u.embeddedPktCfg.fBufPool[0].bPool = voiceni_pp_config.buffer_pool_id;
    rxCh.u.embeddedPktCfg.fBufPool[1].bMgr  = voiceni_pp_config.buffer_pool_mgr_id;
    rxCh.u.embeddedPktCfg.fBufPool[1].bPool = voiceni_pp_config.buffer_pool_id;
    rxCh.u.embeddedPktCfg.fBufPool[2].bMgr  = voiceni_pp_config.buffer_pool_mgr_id;
    rxCh.u.embeddedPktCfg.fBufPool[2].bPool = voiceni_pp_config.buffer_pool_id;
    rxCh.u.embeddedPktCfg.fBufPool[3].bMgr  = voiceni_pp_config.buffer_pool_mgr_id;
    rxCh.u.embeddedPktCfg.fBufPool[3].bPool = voiceni_pp_config.buffer_pool_id;
    cppi4RxChHnd        = PAL_cppi4RxChOpen(priv->pal_hnd, &rxCh, NULL);

    /* Set up Tx channel */
    txCh.chNum          = voiceni_pp_config.cppi_dsp_rx_infra_ch;
    txCh.dmaNum         = voiceni_pp_config.cppi_dsp_rx_infra_dma;
    txCh.tdQueue.qMgr   = voiceni_pp_config.cppi_queue_mgr_id;
    txCh.tdQueue.qNum   = voiceni_pp_config.cppi_dsp_rx_infra_tdq;

    cppi4TxChHnd        = PAL_cppi4TxChOpen(priv->pal_hnd, &txCh, NULL);

    if (!cppi4TxChHnd || !cppi4RxChHnd)
    {
        printk ("ERROR in %s: infra channel setup failed for channel %d\n", __FUNCTION__, voiceni_pp_config.cppi_dsp_rx_infra_ch);
        return -1;
    }

    /* Enable Tx-Rx channels */
    PAL_cppi4EnableRxChannel (cppi4RxChHnd, NULL);
    PAL_cppi4EnableTxChannel (cppi4TxChHnd, NULL);

    return 0;
}

#ifdef CONFIG_MACH_PUMA5
/*******************************************************************************
* FUNCTION:    voiceni_open
*
********************************************************************************
*
* DESCRIPTION:  Routine to create voiceni/DSP PID range and PID
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static int voiceni_create_pid_puma5(struct net_device *dev)
{

    VOICENID_PRIVATE_T * priv = netdev_priv(dev);

    TI_PP_PID voiceni_pid;
    TI_PP_PID_RANGE voiceni_pid_range;
    memset(&voiceni_pid, 0, sizeof(TI_PP_PID));
    /* Need to 1st create create_pid_range */
    memset(&voiceni_pid, 0, sizeof(TI_PP_PID_RANGE));

    voiceni_pid_range.base_index = PP_C55_PID_BASE;
    voiceni_pid_range.count = PP_C55_PID_COUNT;
    voiceni_pid_range.port_num = voiceni_pp_config.pid;
    voiceni_pid_range.type = TI_PP_PID_TYPE_INFRASTRUCTURE;

    if (ti_ppm_config_pid_range(&voiceni_pid_range) != 0)
    {
        printk("ERROR: [VOICENI] ti_ppm_config_pid_range() failed\n");
    }
    else
    {
        DPRINTK("[VOICENI] ti_ppm_config_pid_range() success\n");
    }


    /* populate TI_PP_PID struct */
    voiceni_pid.type = TI_PP_PID_TYPE_INFRASTRUCTURE;
    voiceni_pid.ingress_framing = TI_PP_PID_TYPE_ETHERNET;
    voiceni_pid.dflt_pri_drp    = 0;
    voiceni_pid.dflt_dst_tag    = 0x3FFF;                                           /* 0x3FFF implies no explicit tag applied */
    voiceni_pid.dflt_fwd_q      = voiceni_pp_config.cppi_vni_rx_q;
    voiceni_pid.pri_mapping     = 1;                                                /* Num prio Qs for fwd, we have only 1  */
    voiceni_pid.tx_pri_q_map[0] = voiceni_pp_config.cppi_dsp_rx_infra_qid;          /* default Q for Egress, PP to DSP      */
    voiceni_pid.tx_hw_data_len  = 0;                                                /* dont fill this field */
    voiceni_pid.pid_handle      = PP_C55_PID_BASE;
    priv->pid_hnd = ti_ppm_create_pid(&voiceni_pid, dev);
    dev->pid_handle = priv->pid_hnd;


    /* Preparations for VPID creation */
    dev->vpid_block.type               = TI_PP_ETHERNET;
    dev->vpid_block.parent_pid_handle  = dev->pid_handle;
    dev->vpid_block.egress_mtu         = 0;
    dev->vpid_block.priv_tx_data_len   = 0;

    DPRINTK("voiceni_create_pid_puma5() handle returned %d\n", priv->pid_hnd);
    return 0;
}

#else

static int voiceni_create_pid_puma6(struct net_device *dev)
{

    VOICENID_PRIVATE_T * priv = netdev_priv(dev);

    AVALANCHE_PP_PID_t          voiceni_pid;
    AVALANCHE_PP_PID_RANGE_t    voiceni_pid_range;


    memset(&voiceni_pid, 0, sizeof( AVALANCHE_PP_PID_t ));
    memset(&voiceni_pid, 0, sizeof( AVALANCHE_PP_PID_RANGE_t ));

    voiceni_pid_range.base_index    = PP_C55_PID_BASE;
    voiceni_pid_range.count         = PP_C55_PID_COUNT;
    voiceni_pid_range.port_num      = voiceni_pp_config.pid;
    voiceni_pid_range.type          = AVALANCHE_PP_PID_TYPE_INFRASTRUCTURE;

    if (avalanche_pp_pid_config_range( &voiceni_pid_range ) != 0)
    {
        printk("ERROR: [VOICENI] %s failed\n",__FUNCTION__);
    }
    else
    {
        DPRINTK("[VOICENI] %s success\n",__FUNCTION__);
    }


    /* populate TI_PP_PID struct */
    voiceni_pid.type            = AVALANCHE_PP_PID_TYPE_INFRASTRUCTURE;
    voiceni_pid.ingress_framing = AVALANCHE_PP_PID_INGRESS_ETHERNET;
    voiceni_pid.dflt_pri_drp    = 0;
    voiceni_pid.dflt_dst_tag    = 0x3FFF;                                           /* 0x3FFF implies no explicit tag applied */
    voiceni_pid.dflt_fwd_q      = voiceni_pp_config.cppi_vni_rx_q;
    voiceni_pid.pri_mapping     = 1;                                                /* Num prio Qs for fwd, we have only 1  */
    voiceni_pid.tx_pri_q_map[0] = voiceni_pp_config.cppi_dsp_rx_infra_qid;          /* default Q for Egress, PP to DSP      */
    voiceni_pid.tx_hw_data_len  = 0;                                                /* dont fill this field */
    voiceni_pid.pid_handle      = PP_C55_PID_BASE;

    if (avalanche_pp_pid_create( &voiceni_pid, dev ))
    {
        voiceni_pid.pid_handle = -1;
    }

    dev->pid_handle = voiceni_pid.pid_handle;


    /* Preparations for VPID creation */
    dev->vpid_block.type               = AVALANCHE_PP_VPID_ETHERNET;
    dev->vpid_block.parent_pid_handle  = dev->pid_handle;

    DPRINTK("voiceni_create_pid_puma6() handle returned %d\n", priv->pid_hnd);
    return 0;
}

#endif

/*******************************************************************************
* FUNCTION:    voiceni_open
*
********************************************************************************
*
* DESCRIPTION:  VoiceNI Device Open routine
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static int voiceni_open (struct net_device *dev)
{
    VOICENID_PRIVATE_T *priv =  netdev_priv(dev);

    /* Initializing resources */
    init_voiceni_free_bd_queue(priv);

    init_voiceni_free_bd_pool(priv);


    /* Init voiceNI CCPI41 queues */
    init_voiceni_ccpi41_queues(priv);

    /* Init DSP CCPI41 queues */
    init_dsp_ccpi41_queues(priv);

    /* Init the accumulator channel */
    init_voiceni_ccpi4_accum_channel(priv);

    tasklet_init(&(priv->tx_tasklet), voiceni_rx_processing, (unsigned long) dev);
    netif_start_queue (dev);

    ti_hil_pp_event (TI_BRIDGE_PORT_FORWARD, (void *)dev);

    return 0;
}

/*******************************************************************************
* FUNCTION:    voiceni_open
*
********************************************************************************
*
* DESCRIPTION:  VoiceNI Device Close routine
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static int voiceni_close (struct net_device *dev)
{
    VOICENID_PRIVATE_T *priv = netdev_priv(dev);

    disable_irq(VOICENI_RXINT_NUM);
    PAL_cppi4AccChClose(priv->acc_hnd, NULL);

    DPRINTK("voiceni_close(): calling netif_stop_queue(), rx interrupt count%d\n", priv->rx_interrupt_count);
    netif_stop_queue(dev);

    /*close vni cppi queues */
    PAL_cppi4QueueClose(priv->pal_hnd, priv->voiceni_rx_queue);
    PAL_cppi4QueueClose(priv->pal_hnd, priv->voiceni_tx_queue);
    /* clse dsp cppi queues */
    PAL_cppi4QueueClose(priv->pal_hnd, priv->dsp_rx_queue);
    PAL_cppi4QueueClose(priv->pal_hnd, priv->dsp_tx_queue);

    /* close Free BD queue */
    PAL_cppi4QueueClose(priv->pal_hnd, priv->free_bd_queue);
    PAL_cppi4QueueClose(priv->pal_hnd, priv->free_infra_bd_queue);
    /* Dealloc DB Descriptors */
    PAL_cppi4DeallocDesc( priv->pal_hnd, voiceni_pp_config.cppi_queue_mgr_id, (Ptr)avalanche_no_OperSys_memory_virt_to_phys((unsigned int)priv->free_bd_pool));

    if (priv->rxAcc_chan_list_base[0])
        kfree (priv->rxAcc_chan_list_base[0]);

    free_irq(VOICENI_RXINT_NUM, priv);

    ti_hil_pp_event (TI_BRIDGE_PORT_DISABLED, (void *)dev);
    return 0;
}


/*******************************************************************************
* FUNCTION:    voiceni_start_xmit
*
********************************************************************************
*
* DESCRIPTION:  voiceni transmit routine. Called by MTANI when it needs to
*               transmit packet to voiceni.
*
* RETURN:     0 if successful.
*
*******************************************************************************/
static int voiceni_start_xmit (struct sk_buff *skb, struct net_device *dev)
{
    Cppi4Desc *virtBD, *phyBD;
    Ptr pBuffPtr;
    unsigned int   queueNum = voiceni_pp_config.cppi_dsp_rx_infra_qid;
    VOICENID_PRIVATE_T *priv = netdev_priv(dev);


    priv->rx_skb_count++;

    /* Need to allocate free BD and buffer*/
    pBuffPtr = PAL_cppi4BufPopBuf(priv->pal_hnd, c55bufPool);
    if (pBuffPtr == NULL)
    {
        priv->buff_pop_failure++;
        priv->stats.tx_dropped++;
        dev_kfree_skb(skb);
        return 0;
    }

    phyBD = (Cppi4Desc*) PAL_cppi4QueuePop(priv->free_bd_queue);

    DPRINTK("%s:%d Got BD[%08X]\n",__FUNCTION__,__LINE__,(unsigned int)phyBD);

    if (phyBD == NULL)
    {
        printk("ERROR: voiceni_start_xmit() physical bd is NULL\n");

        priv->bd_queue_pop_failure++;
        priv->stats.tx_dropped++;

        PAL_cppi4BufDecRefCnt(priv->pal_hnd, c55bufPool, pBuffPtr);
        dev_kfree_skb(skb);
        return 0;
    }

    virtBD = (Cppi4Desc *)avalanche_no_OperSys_memory_phys_to_virt((unsigned int)phyBD);
    PAL_CPPI4_CACHE_INVALIDATE(virtBD, PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE);

    PRINT_DESC( PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE, virtBD );

     /* copy skb into buffer and update BD if required */
    if (virtBD != NULL)
    {
        Cppi4EmbdBuf *descBufInfo;

#ifdef CONFIG_MACH_PUMA6
        descBufInfo = &(virtBD->Buf);
#else
        descBufInfo = &(virtBD->Buf[1]);
#endif

        memcpy( (void*)avalanche_no_OperSys_memory_phys_to_virt((unsigned int)pBuffPtr), skb->data, skb->len);

        PAL_osMemSet(virtBD, 0, PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE);
        /* descInfo correlates to seciotn 2.2.3.1, page 17 */
        virtBD->descInfo =
            CPPI41_EM_DESCINFO_DTYPE_EMBEDDED |
            CPPI41_EM_DESCINFO_SLOTCNT_MYCNT  |
            skb->len;
        virtBD->tagInfo =   (voiceni_pp_config.pid<< CPPI41_EM_TAGINFO_SRCPORT_SHIFT)
                          | (0x3fff << CPPI41_EM_TAGINFO_DSTTAG_SHIFT);
        /* pktInfo correlates to section 2.2.3.3, page 18 of cppi specification */
        virtBD->pktInfo =   (PAL_CPPI4_HOSTDESC_PKT_TYPE_ETH << CPPI41_EM_PKTINFO_PKTTYPE_SHIFT)
                          | (CPPI41_EM_PKTINFO_RETPOLICY_RETURN)
                          | (voiceni_pp_config.cppi_queue_mgr_id    << CPPI41_EM_PKTINFO_RETQMGR_SHIFT)
                          | (1                  << CPPI41_EM_PKTINFO_EOPIDX_SHIFT)
                          | (0                  << CPPI41_EM_PKTINFO_ONCHIP_SHIFT)
                          | (voiceni_pp_config.cppi_bd_queue_id << CPPI41_EM_PKTINFO_RETQ_SHIFT);

        descBufInfo->BufInfo = 
            CPPI41_EM_BUF_VALID_MASK |
            (voiceni_pp_config.buffer_pool_mgr_id  << CPPI41_EM_BUF_MGR_SHIFT) |
            (voiceni_pp_config.buffer_pool_id << CPPI41_EM_BUF_POOL_SHIFT) |
            skb->len;
        descBufInfo->BufPtr = (UINT32)pBuffPtr;

        PAL_CPPI4_CACHE_WRITEBACK(avalanche_no_OperSys_memory_phys_to_virt((unsigned int)pBuffPtr), skb->len);
    }
    else
    {
        priv->bd_queue_pop_failure++;
        priv->stats.tx_dropped++;

        PAL_cppi4BufDecRefCnt(priv->pal_hnd, c55bufPool, pBuffPtr);
        dev_kfree_skb(skb);
        return 0;
    }

    /* update the queueNm as well */
    /* Set SYNC Q PTID info in egress descriptor */
    if(skb->pp_packet_info.flags == TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED)
    {
        memcpy(&(virtBD->EPI[0]), skb->pp_packet_info.ti_epi_header, sizeof(virtBD->EPI));
        virtBD->EPI[1] &= ~(0xFFFF);
        virtBD->EPI[1] |= queueNum;

#ifdef CONFIG_MACH_PUMA6
        if (virtBD->EPI[0] & PAL_CPPI4_HOSTDESC_NETINFW0_DIVERT_FLAG_MASK)
        {
            virtBD->EPI[0] = PAL_CPPI4_HOSTDESC_NETINFW0_DO_NOT_DISCARD;
        }
        else
        {
            virtBD->EPI[0] = 0;
        }
        if ((skb->pp_packet_info.pp_session.egress.enable & AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_CTRL) != AVALANCHE_PP_EGRESS_FIELD_ENABLE_TCP_CTRL)
        {
            virtBD->EPI[0] |= skb->pp_packet_info.pp_session.session_handle;
        }
        else
        {
            virtBD->EPI[0] |= AVALANCHE_PP_MAX_ACCELERATED_SESSIONS;
        }
#endif
    }
    else
    {
        virtBD->EPI[1] = queueNum;
#ifdef CONFIG_MACH_PUMA6
        virtBD->EPI[0] = AVALANCHE_PP_MAX_ACCELERATED_SESSIONS;
#endif
    }

    /* push BD and buffer onto PP receive Q destined for DSP */
    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;
    priv->tx_bd_count++;
    PAL_CPPI4_CACHE_WRITEBACK(virtBD, PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE);

    PRINT_DESC( PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE, virtBD );

    PAL_cppi4QueuePush (priv->voiceni_tx_queue,
                        (Ptr) phyBD,
                        PAL_CPPI4_DESCSIZE_2_QMGRSIZE(PAL_CPPI41_VOICE_DSP_C55_EMB_BD_SIZE),
                        skb->len);
    dev_kfree_skb(skb);
    return 0;
}

static  struct net_device_stats *voiceni_get_stats (struct net_device *dev)
{
    VOICENID_PRIVATE_T *priv = netdev_priv(dev);
    return &(priv->stats);
}

static int voiceni_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    int err = -EFAULT;
    void __user *addr = (void __user *) ifr->ifr_ifru.ifru_data;

    switch (cmd)
    {
        case SIOCGETVOICENI_PARAMS:
        if (copy_to_user(addr, &voiceni_pp_config, sizeof(voiceni_pp_config)))
            break;
        err = 0;

        break;
    default:
        err = -EINVAL;
    }
    return err;
}

static const struct net_device_ops netdev_ops = {
        .ndo_open       = voiceni_open,
        .ndo_start_xmit = voiceni_start_xmit,
        .ndo_stop       = voiceni_close,
        .ndo_get_stats  = voiceni_get_stats,
        .ndo_do_ioctl   = voiceni_ioctl
};

void voiceni_netdev_setup(struct net_device *ptr_netdev)
{
    DPRINTK("voiceni_netdev_setup being called for  for VOICENI network device\n");
    ptr_netdev->netdev_ops = &netdev_ops;

    /* setup up generic ethernet property fields in net device structure */
    ether_setup(ptr_netdev);
   return;
}

static int voiceni_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
    struct net_device *dev = ptr;
    VOICENID_PRIVATE_T * priv;

    DPRINTK("voiceni_netdev_event, got event: %d\n", (int)event);

    /* Only care about events for CONFIG_TI_PACM_MTA_INTERFACE net device */
    if (strcmp(dev->name, CONFIG_TI_PACM_MTA_INTERFACE) != 0)
    {
        return 0;
    }

    priv = netdev_priv(voicenid_mcb.net_dev.ptr_device);
    /* at this point, you have a newly registered/unregistered device */
    if (event == NETDEV_UP)
    {
        DPRINTK("voiceni_netdev_event, interface %s is up\n", dev->name);

        priv->ptr_emtani_device = dev_get_by_name(&init_net, CONFIG_TI_PACM_MTA_INTERFACE);
        if(priv->ptr_emtani_device == NULL)
        {
            printk(KERN_ERR "Voice NI Failed to dev_get_by_name\n");
            return -1;
        }
        dev_put(priv->ptr_emtani_device);
    }

    if (event == NETDEV_DOWN)
    {
        /* unregister_netdevice of MTA... Need to reset the MTA NI pointer in VNI */
        priv->ptr_emtani_device = NULL;
        DPRINTK("voiceni_netdev_event, interface %s is down\n", dev->name);
        return 0;
    }
    return 0;
}

static struct notifier_block voiceni_netdev_notifier = {
                .notifier_call = voiceni_netdev_event,
};

/*********************************************************************************
* FUNCTION: voiceni_init_module
*
* DESCRIPTION:Initialization routine for voice network interface kernel device
*********************************************************************************/
static int __init voiceni_init_module(void)
{
    struct net_device*  netdev;
    VOICENID_PRIVATE_T* priv;
    int ret;


    DPRINTK("voiceni_init_module: calling alloc_netdev for VOICNI network device\n");
    /* Allocate memory for the network device. */
    netdev = alloc_netdev(sizeof(VOICENID_PRIVATE_T), VOICENI_DEV_NAME, voiceni_netdev_setup);
    if(netdev == NULL)
    {
        printk(KERN_ERR "Unable to allocate memory for the VOICNI network device\n");
        return -ENOMEM;
    }
      /* Register the network device */
    ret = register_netdev (netdev);
    if(ret)
    {
        printk(KERN_ERR "Unable to register device named %s (%p)...\n", netdev->name, netdev);
        return ret;
    }

    DPRINTK(KERN_DEBUG "Registered device named %s (%p)...\n", netdev->name, netdev);

    /* need handle to private data of net device */
    priv = netdev_priv(netdev);
    voicenid_mcb.net_dev.ptr_device = netdev;

    /* Get handle to platform abrastraction layer (PAL) CPPI instance */
    if ((priv->pal_hnd= PAL_cppi4Init(NULL, NULL)) == NULL)
    {
        printk(KERN_WARNING "voiceni_init_module: PAL CPPI is not initialized yet \n");
        return(-EAGAIN); /* need to check return value */
    }

    voiceni_pp_config.cppi_queue_mgr_id     = PAL_CPPI41_VOICE_DSP_C55_QMGR;
    voiceni_pp_config.cppi_bd_queue_id      = PAL_CPPI41_SR_VOICE_DSP_C55_FD_EMB_Q_NUM;
    voiceni_pp_config.cppi_infra_bd_queue_id = PAL_CPPI41_SR_VOICE_INFRA_FD_EMB_Q_NUM;
    voiceni_pp_config.buffer_pool_mgr_id    = c55bufPool.bMgr;
    voiceni_pp_config.buffer_pool_id        = c55bufPool.bPool;
#ifdef CONFIG_MACH_PUMA6
    voiceni_pp_config.buffer_pool_size = BMGR0_POOL17_BUF_SIZE;
    voiceni_pp_config.buffer_pool_count = BMGR0_POOL17_BUF_COUNT;
    voiceni_pp_config.buffer_pool_refcount = BMGR0_POOL17_REF_CNT;
#else
    voiceni_pp_config.buffer_pool_size = BMGR0_POOL09_BUF_SIZE;
    voiceni_pp_config.buffer_pool_count = BMGR0_POOL09_BUF_COUNT;
    voiceni_pp_config.buffer_pool_refcount = BMGR0_POOL09_REF_CNT;
#endif
    voiceni_pp_config.cppi_dsp_rx_queue_id  = PAL_CPPI41_VOICE_DSP_C55_INPUT_QNUM;
    voiceni_pp_config.cppi_dsp_tx_queue_id  = PAL_CPPI41_SR_PPDSP_HIGH_Q_NUM;
    voiceni_pp_config.pid                   = CPPI41_SRCPORT_VOICE_DSP_C55;
    voiceni_pp_config.cppi_vni_rx_q         = PAL_CPPI41_VOICE_DSP_C55_HOST_RX_Q_NUM;
    voiceni_pp_config.cppi_vni_tx_q         = PAL_CPPI41_VOICE_NI_OUTPUT_QNUM;
    voiceni_pp_config.cppi_acc_rx_ch        = PAL_CPPI41_VOICE_DSP_C55_ACC_RX_CHNUM;
    voiceni_pp_config.cppi_acc_rx_intv      = PAL_CPPI41_VOICE_DSP_C55_ACC_RX_INTV;
    voiceni_pp_config.cppi_dsp_rx_infra_qid = PAL_CPPI41_VOICE_DSP_C55_INFRA_INPUT_LOW_Q_NUM;
    voiceni_pp_config.cppi_dsp_rx_infra_ch  = PAL_CPPI41_VOICE_DSP_C55_INFRA_CHN;
    voiceni_pp_config.cppi_dsp_rx_infra_dma = PAL_CPPI41_VOICE_DSP_C55_INFRA_DMA_ID;
    voiceni_pp_config.cppi_dsp_rx_infra_tdq = PAL_CPPI41_VOICE_DSP_C55_INFRA_TD_QNUM;

    init_c55_input_DMA(priv);

    /* Create PID and VPID, moved from above PAL_cppi4Init*/
#ifdef CONFIG_MACH_PUMA5
    voiceni_create_pid_puma5(netdev);
#else
    voiceni_create_pid_puma6(netdev);
#endif

#ifdef CONFIG_TI_DOCSIS
    /* Need handle to MTANI device in order to transmit packets to it */
    priv->ptr_emtani_device = dev_get_by_name(&init_net, CONFIG_TI_PACM_MTA_INTERFACE);
    if(priv->ptr_emtani_device == NULL)
    {
        printk(KERN_ERR "Failed to dev_get_by_name for %s device\n", CONFIG_TI_PACM_MTA_INTERFACE);
        return -1;
    }
    dev_put(priv->ptr_emtani_device);
#endif
    /* Regisger voiceni network device notifier in order to receive network
       device related events */
    register_netdevice_notifier(&voiceni_netdev_notifier);

    {
        struct proc_dir_entry * voiceni_dir = NULL;
        struct proc_dir_entry * tmp_dir = NULL;

        if (NULL == (voiceni_dir = proc_mkdir("voiceni",     NULL ))) { printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); }
        if (NULL == (tmp_dir = create_proc_entry( "show_init" , 0644, voiceni_dir ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        tmp_dir->read_proc = voiceni_init_proc;
        tmp_dir->data      = priv;

        if (NULL == (tmp_dir = create_proc_entry( "stats" ,     0644, voiceni_dir ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        tmp_dir->read_proc = voiceni_stats_proc;
        tmp_dir->data      = priv;

        if (NULL == (tmp_dir = create_proc_entry( "cmd",        0644, voiceni_dir ))) {   printk("%s:%d ERROR ....\n",__FUNCTION__,__LINE__); return -1;  }
        tmp_dir->write_proc = voiceni_write_cmds;
        tmp_dir->data      = priv;
    }

    DPRINTK("VOICE NI module loaded \n");
    return 0;
}

/*********************************************************************************
 * FUNCTION: voiceni_cleanup_module
 *
 * DESCRIPTION:
 *********************************************************************************/
static void __exit voiceni_cleanup_module(void)
{
    /* need to remove proc entries, etc */
    DPRINTK("voice module unloaded\n");
    return;
}

module_init(voiceni_init_module);
module_exit(voiceni_cleanup_module);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments Incorporated");
MODULE_DESCRIPTION("TI voice VOICENI core module.");
MODULE_SUPPORTED_DEVICE("Texas Instruments voiceni");


