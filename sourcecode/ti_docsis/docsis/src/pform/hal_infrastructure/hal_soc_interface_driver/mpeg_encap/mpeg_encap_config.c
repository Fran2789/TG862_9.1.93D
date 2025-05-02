/*
 *
 * mpeg_encap_config.c
 * Description:
 * MPEG Encapsulation configuration functions implementation
 *
 *
 * GPL LICENSE SUMMARY
 *
 *  Copyright(c) 2009-2013 Intel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify 
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but 
 *  WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 *  General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License 
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *  The full GNU General Public License is included in this distribution 
 *  in the file called LICENSE.GPL.
 *
 *  Contact Information:
 *  Intel Corporation
 *  2200 Mission College Blvd.
 *  Santa Clara, CA  97052
 */
/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h> 
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>

#include <linux/if_ether.h>
#include <net/ip.h>
#include <linux/udp.h>
#include <linux/inet.h>

#include <puma_autoconf.h>

#include <puma.h>
#include <hardware.h>
#include <pal.h>
#include <pal_cppi41.h>
#if PUMA6_OR_NEWER_SOC_TYPE
#include <puma6_cppi.h>
#else
#include <puma5_cppi.h>
#endif

#include "_tistdtypes.h"
#include "mpeg_encap_driver.h"
#include "mpeg_encap_prv.h"

/**************************************************************************/
/*      DEFINES                                                           */
/**************************************************************************/

/************************************************************************/
/*     MPEG Encap driver private data                                   */
/************************************************************************/
static Uint8 pidFwdOpStr[MPEG_NUM_PID_FWD_OP][32] = {"Discard", "Fast Forward", "Host Forward", "Host&Fast Forward"};

/* Static functions */
static Cppi4EmbdDesc_ps* allocate_host2tgw_resources(mpeg_process_private* priv, Ptr* buffer);
static int sendHostData2OutStream (mpeg_process_private* priv, Ptr* HostBuffer, Uint32 timestamp, Uint32 streamDestId);
static int mpeg_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data);
static int mpeg_write_proc(struct file *file, const char *buffer, unsigned long count, void *data);
static inline int mpeg_init_acc_chan(PAL_Handle pal_hnd, int chan_num, Cppi4Queue queue, PAL_Cppi4AccChHnd* acc_hnd);
static void init_cppi_rx(mpeg_process_private* priv);
static void init_cppi_tx(mpeg_process_private* priv);
static void init_work_queue(mpeg_process_private* priv);
static void set_packet_header(struct encaphdr *hdr, 
                              Uint8* mac_src_addr, 
                              Uint8* mac_dst_addr, 
                              Uint32 ip4_src_addr, 
                              Uint32 ip4_dst_addr, 
                              Uint16 udp_src_port,
                              Uint16 udp_dst_port,
                              Uint32 rtp_ssrc);


/**************************************************************************/
/*      STATIC  Functions                                                 */
/**************************************************************************/

/**************************************************************************/
/*! \fn static int mpeg_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
 **************************************************************************
 *  \brief Mpeg encap Read proc callback
 *  \return  0 or error code
 */
static int mpeg_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    
    len += sprintf(page + len, "Usage\n");
    len += sprintf(page + len, "\ts: statistics\n");
    len += sprintf(page + len, "\ta PORT PID: add filter, enable all PIDs when PID is bigger then %d\n", PID_SPACE);
    len += sprintf(page + len, "\td PORT PID: delete filter, disable all PIDs when PID is bigger then %d\n", PID_SPACE);
    len += sprintf(page + len, "\tu PORT UDP header: set encapsulate header to MAC+IP+UDP+payload\n");
    len += sprintf(page + len, "\tr PORT RTP header: set encapsulate header to MAC+IP+UDP+RTP+payload\n");
    
	if (len <= off+count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count) len = count;
	if (len < 0) len = 0;
	
    return(len);
}

/**************************************************************************/
/*! \fn static int mpeg_write_proc(struct file *file,
                           const char *buffer,
                           unsigned long count, 
                           void *data)
 **************************************************************************
 *  \brief Mpeg encap write proc callback
 *  \return  0 or error code
 */
static int mpeg_write_proc(struct file *file,
                           const char *buffer,
                           unsigned long count, 
                           void *data)
{
#define MPEG_WRITE_PROC_MAX_BUF_SIZE  255
    int len;
    char tmp_buffer[MPEG_WRITE_PROC_MAX_BUF_SIZE+1];
    unsigned int pid;
    unsigned int port;
    unsigned int chn_idx;
    unsigned int outstream_idx;
    unsigned int active_pid_idx;
    mpeg_process_private* priv = (mpeg_process_private*)data;
	pidOperation_e pidFwdOp;

    if (count > MPEG_WRITE_PROC_MAX_BUF_SIZE)
        len = MPEG_WRITE_PROC_MAX_BUF_SIZE;
    else
        len = count;

    if (copy_from_user(tmp_buffer, buffer, len))
        return -EFAULT;
    tmp_buffer[len] = '\0';

    switch (tmp_buffer[0])
    {
    case 's':
        printk("Number of rcv interrupts is: %d\n", priv->irq_counter);
        printk("Number of processed frames is: %d\n", priv->frames_processed);      
        printk("Number of frames forwarded to Host is: %d\n", priv->host_forwarded);      
        printk("Number allocation trys is: %d\n", priv->try2alloc);
        printk("Number buffers underfflow is: %d\n", priv->outof_buf);
        printk("Number PDs underflow is: %d\n", priv->outof_pd);
#ifdef MPEG_ENCAP_DEBUG
		{
			unsigned int pid_idx;
			for (chn_idx=0; chn_idx<MAX_MPEG_CHNS; ++chn_idx)
			{
				printk("***PORT(%d) frames ***\n", chn_idx+1); 
	
				for (pid_idx=0; pid_idx<MAX_MPEG_ACTIVE_PIDS; ++pid_idx)
				{
					if (priv->encap_pid_params[chn_idx][pid_idx].pid != MPEG_INVALID_PID)
					{
						printk("PID %u: ", priv->encap_pid_params[chn_idx][pid_idx].pid); 
						printk("Input frames : %u\n",priv->encap_pid_params[chn_idx][pid_idx].input_counter);
						for (outstream_idx=0; outstream_idx < MAX_MPEG_ENCAP_OUT_STREAMS; outstream_idx++)
						{
							if (priv->encap_pid_params[chn_idx][pid_idx].out_stream_map[outstream_idx])
							{
								printk("Frames forwarded to IP stream %u: %u\n", outstream_idx, priv->encap_pid_params[chn_idx][pid_idx].fwd_counter[outstream_idx]); 
								printk("Failed forwarding to IP stream %u: %u frames\n", outstream_idx, priv->encap_pid_params[chn_idx][pid_idx].failed_counter[outstream_idx]); 
							}
						}
					}
				}
			}
			printk("Active Out streams:\n");

			for (outstream_idx=0; outstream_idx < MAX_MPEG_ENCAP_OUT_STREAMS; outstream_idx++)
			{
				if (priv->encap_out_streams[outstream_idx].enabled)
				{
					printk("Out stream %u output IP packets: %u\n", outstream_idx, priv->encap_out_streams[outstream_idx].output_packets_count);
				}
			}
		}
#endif

        break;
    case 'a':
        sscanf( &tmp_buffer[2], "%u %u", &port, &pid );
        if (pid<PID_SPACE && HAL_PORT_ID_2_CHANNEL_INDEX(port)<MAX_MPEG_CHNS)
        {
            printk("Port(%d): Add filter %d\n", port, pid);
            priv->pid_filters[HAL_PORT_ID_2_CHANNEL_INDEX(port)][pid].filter = 1;
        } 
        if (pid>PID_SPACE && HAL_PORT_ID_2_CHANNEL_INDEX(port)<MAX_MPEG_CHNS)
        {
            printk("Add all filters\n");
            for (pid=0; pid<PID_SPACE; ++pid)
            {
                priv->pid_filters[HAL_PORT_ID_2_CHANNEL_INDEX(port)][pid].filter = 1;
            }
        }
        break;
    case 'd':
        sscanf( &tmp_buffer[2], "%u %u", &port, &pid );
        if (pid<PID_SPACE && HAL_PORT_ID_2_CHANNEL_INDEX(port)<MAX_MPEG_CHNS)
        {
            printk("Port(%d): Delete filter %d\n", port, pid);
            priv->pid_filters[HAL_PORT_ID_2_CHANNEL_INDEX(port)][pid].filter = 0;
        } 
        if (pid>PID_SPACE && HAL_PORT_ID_2_CHANNEL_INDEX(port)<MAX_MPEG_CHNS)
        {
            printk("Delete all filters\n");
            for (pid=0; pid<PID_SPACE; ++pid)
                priv->pid_filters[HAL_PORT_ID_2_CHANNEL_INDEX(port)][pid].filter = 0;
        }
        break;
    case 'u': 
        sscanf( &tmp_buffer[2], "%u", &outstream_idx );
        if (outstream_idx < MAX_MPEG_ENCAP_OUT_STREAMS)
        {
            printk("UDP header format, ETH+IP+UDP+payload\n");
            priv->encap_out_streams[outstream_idx].encap_header_type = MPEG_ENCAP_UDP_HEADER;
        }
        break;
    case 'r': 
        sscanf( &tmp_buffer[2], "%u", &outstream_idx );
        if (outstream_idx < MAX_MPEG_ENCAP_OUT_STREAMS)
        {
            printk("RTP header format, ETH+IP+UDP+RTP+payload\n");
            priv->encap_out_streams[outstream_idx].encap_header_type = MPEG_ENCAP_RTP_HEADER;
        }
        break;
	 case 'c':/*current configuration*/
		for (chn_idx=0; chn_idx<MAX_MPEG_CHNS; ++chn_idx)
		{
			printk("********* Channel %u **********\n", chn_idx);
			printk("Non-dropped PIDs:\n");
			for (pid=0; pid<PID_SPACE; ++pid)
			{
				pidFwdOp = GET_PID_OPERATION(priv->pid_filters[chn_idx][pid].filter);
				if ( pidFwdOp != MPEG_ENCAP_PID_DISCARD)
				{
					printk("PID %u (%s):\n", pid, pidFwdOpStr[pidFwdOp]);
					active_pid_idx = GET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][pid].filter);
					if (active_pid_idx != INVALID_ACTIVE_PID_IDX)
					{
						printk("Active PID index = %u; ", active_pid_idx);
						printk("Associated IP stream indices (%u): ", priv->encap_pid_params[chn_idx][active_pid_idx].num_assoc_outstreams);
						for (outstream_idx=0; outstream_idx<MAX_MPEG_ENCAP_OUT_STREAMS; outstream_idx++)
						{
							if (priv->encap_pid_params[chn_idx][active_pid_idx].out_stream_map[outstream_idx])
							{
								printk("%u ", outstream_idx);
							}
						}
						printk("\n");

						if (priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip)
						{
							printk("Frame modification rules:\n");
							for (outstream_idx=0; outstream_idx<MAX_MPEG_ENCAP_OUT_STREAMS; outstream_idx++)
							{
								if (priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip[outstream_idx])
								{
									printk("For IP stream %u: frame offset = %u, new frame len=%u\n", outstream_idx,
										   priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip[outstream_idx]->frame_offset,
										   priv->encap_pid_params[chn_idx][active_pid_idx].frame_manip[outstream_idx]->new_frame_length);
								}
							}
							printk("\n");
						}						
					}
				}
			}
			
		}
		printk("=================================\n");
		printk("Enabled IP streams:\n");
		for (outstream_idx=0; outstream_idx<MAX_MPEG_ENCAP_OUT_STREAMS; outstream_idx++)
		{
			if (priv->encap_out_streams[outstream_idx].enabled)
			{
				printk("IP stream %u:",outstream_idx);
				printk("Header type %s, Dest. IP addr 0x%.8x, UDP Port: %u\n", 
					   priv->encap_out_streams[outstream_idx].encap_header_type == MPEG_ENCAP_UDP_HEADER ? "UDP" : "RTP",
					   priv->encap_out_streams[outstream_idx].encap_header.ip_header.daddr,
					   priv->encap_out_streams[outstream_idx].encap_header.udp_header.dest);			}
		}
		break;
    default:
        printk( "Unsupported command.\n");
        break;
    }

    return len;
}

/**************************************************************************/
/*! \fn static inline int mpeg_init_acc_chan(PAL_Handle pal_hnd, int chan_num, Cppi4Queue queue, PAL_Cppi4AccChHnd* acc_hnd)
 **************************************************************************
 *  \brief Init mpeg encapsulation CPPI accumulator
 *  \return  0 or error code
 */
static inline int mpeg_init_acc_chan(PAL_Handle pal_hnd, int chan_num, Cppi4Queue queue, PAL_Cppi4AccChHnd* acc_hnd)
{
    Cppi4AccumulatorCfg cfg;
    *acc_hnd = NULL;

    cfg.accChanNum             = chan_num;
    cfg.list.maxPageEntry      = MPEG_ACC_PAGE_NUM_ENTRY;        /* This is entries per page (and we have 2 pages) */
    cfg.list.listEntrySize     = MPEG_ACC_ENTRY_TYPE;            /* Only interested in register 'D' which has the desc pointer */
    cfg.list.listCountMode     = 0;                              /* Zero indicates null terminated list. */
    cfg.list.pacingMode        = 1;                              /* Wait for time since last interrupt */
    cfg.pacingTickCnt          = 40;                             /* Wait for 1000uS == 1ms */
    cfg.list.maxPageCnt        = MPEG_ACC_NUM_PAGE;              /* Use two pages */
    cfg.list.stallAvoidance    = 1;                              /* Use the stall avoidance feature */
    cfg.queue                  = queue;                
    cfg.mode                   = 0;
   
    /* kmalloc returns cache line aligned memory unless you are debugging the slab allocator (2.6.18) */
    if(!(cfg.list.listBase = kzalloc(MPEG_ACC_LIST_BYTE_SZ, GFP_KERNEL)))
    {
        printk(KERN_ERR "%s: Unable to allocate list page\n", __FUNCTION__);
        return -1;
    }

    PAL_CPPI4_CACHE_WRITEBACK((unsigned long)cfg.list.listBase, MPEG_ACC_LIST_BYTE_SZ);

    if(!(*acc_hnd = PAL_cppi4AccChOpen(pal_hnd, &cfg)))
    {
        printk(KERN_ERR "%s: Unable to open accumulator channel #%d\n", __FUNCTION__, chan_num);
        kfree(cfg.list.listBase);
        return -1;
    }

    return 0;
}

/**************************************************************************/
/*! \fn static void init_cppi_rx(mpeg_process_private* priv)
 **************************************************************************
 *  \brief Init Mpeg Encap RX
 *  \return  0 or error code
 */
static void init_cppi_rx(mpeg_process_private* priv)
{
    int i_bd=0;

    priv->irq_counter = -1;
    priv->frames_processed = -1;
    priv->host_forwarded = -1;
    priv->try2alloc = -1;
    priv->outof_buf = -1;
    priv->outof_pd = -1;

    printk("RX Init CPPI: open free descriptor queue\n");
    /* open free descriptor queue */
    {
        Cppi4Queue queue = {PAL_CPPI41_QUEUE_MGR_PARTITION_SR, PAL_CPPI41_SR_MPEG_FD_EMB_Q_NUM};
        if(!(priv->rxMpeg_free_queue_hnd = PAL_cppi4QueueOpen(priv->pal_hnd, queue)))
        {
            printk(KERN_ERR "%s: Unable to open free desc queue %d priority channel\n", __FUNCTION__, PAL_CPPI41_SR_MPEG_FD_EMB_Q_NUM);
            goto err;
        }
    }

    printk("RX Init CPPI: allocate descriptors\n");
    /*
     * Define free embedded descriptors for channel
     * --------------------------------------------
     */
    printk("%s: Allocate descriptors to the embedded free queues\n", __FUNCTION__);

    if(!(priv->rxMpeg_pdpool = PAL_cppi4AllocDesc (priv->pal_hnd, PAL_CPPI41_QUEUE_MGR_PARTITION_SR, DMAC_MPEG_RX_EMBEDDED_BD_NUM, 64)))
    {
        printk(KERN_ERR "%s: Unable to init BD pool\n", __FUNCTION__);
        goto err;        
    }

    printk("RX Init CPPI: Fill free discriptor queue\n");
    for (i_bd = 0; i_bd < DMAC_MPEG_RX_EMBEDDED_BD_NUM; i_bd++) 
    {
        Cppi4EmbdDesc *bd = (Cppi4EmbdDesc *)GET_MPEG_BD_PTR(priv->rxMpeg_pdpool, i_bd);

        PAL_osMemSet(bd, 0, 64);
        bd->descInfo     = CPPI41_EM_DESCINFO_DTYPE_EMBEDDED | CPPI41_EM_DESCINFO_SLOTCNT_MPEG;
        bd->tagInfo      = 0;
        bd->pktInfo      = (PAL_CPPI4_HOSTDESC_PKT_TYPE_GENERIC << CPPI41_EM_PKTINFO_PKTTYPE_SHIFT) 
                            | (CPPI41_EM_PKTINFO_RETPOLICY_RETURN) 
                            | (1 << CPPI41_EM_PKTINFO_PROTSPEC_SHIFT) 
                            | (PAL_CPPI41_QUEUE_MGR_PARTITION_SR << CPPI41_EM_PKTINFO_RETQMGR_SHIFT) 
                            | (PAL_CPPI41_SR_MPEG_FD_EMB_Q_NUM << CPPI41_EM_PKTINFO_RETQ_SHIFT);
        PAL_CPPI4_CACHE_WRITEBACK(bd, CPPI4_BD_LENGTH_FOR_CACHE);
        PAL_cppi4QueuePush (priv->rxMpeg_free_queue_hnd, (Ptr) PAL_CPPI4_VIRT_2_PHYS(bd), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(64), 0);        
    }


    printk("RX Init CPPI: Set DMA channel\n");
    {
        volatile Cppi4RxChInitCfg MpegRxChInfo;
        PAL_Cppi4RxChHnd MpegRxChHdl;

        MpegRxChInfo.chNum = MPEG_CPPI4x_RX_DMA_CHNUM;
        MpegRxChInfo.dmaNum = 1;
        MpegRxChInfo.rxCompQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
        MpegRxChInfo.rxCompQueue.qNum = PAL_CPPI41_SR_MPEG_HOST_RX_Q_NUM;
        MpegRxChInfo.sopOffset = 0; // SOF 
        MpegRxChInfo.retryOnStarvation = 0;

        MpegRxChInfo.defDescType = CPPI41_DESC_TYPE_EMBEDDED;
        MpegRxChInfo.u.embeddedPktCfg.fdQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
        MpegRxChInfo.u.embeddedPktCfg.fdQueue.qNum = PAL_CPPI41_SR_MPEG_FD_EMB_Q_NUM;
        MpegRxChInfo.u.embeddedPktCfg.numBufSlot = CPPI41_EM_DESCINFO_SLOTCNT_MPEG;
        MpegRxChInfo.u.embeddedPktCfg.sopSlotNum = 0;
        MpegRxChInfo.u.embeddedPktCfg.fBufPool[0].bMgr = 0;  
        MpegRxChInfo.u.embeddedPktCfg.fBufPool[0].bPool = 2; 

        printk("%s: Call PAL_cppi4RxChOpen channel=%d, REF_CNT=%u BUF COUNT=%u\n", __FUNCTION__, MpegRxChInfo.chNum, BMGR0_POOL06_REF_CNT,
			   BMGR0_POOL06_BUF_COUNT);
        MpegRxChHdl = PAL_cppi4RxChOpen (priv->pal_hnd, (Cppi4RxChInitCfg *)(&MpegRxChInfo), NULL);
        if(MpegRxChHdl == NULL)
        {
            printk(KERN_ERR "%s: Unable to open %d channel \n", __FUNCTION__, MpegRxChInfo.chNum);
            goto err;
        }
        PAL_cppi4EnableRxChannel (MpegRxChHdl, NULL);
        priv->rxMpeg_chan_hnd = MpegRxChHdl;
    }

    printk("RX Init CPPI: Init accumulator\n");
    {
        unsigned int i = 0;
        //for (i=0; i<MPEG_CPPI4x_RX_Q_COUNT; ++i)
        {
            Cppi4Queue rx_queue = {PAL_CPPI41_QUEUE_MGR_PARTITION_SR, PAL_CPPI41_SR_MPEG_HOST_RX_Q_NUM};
            
            if(!(priv->rx_queue_hnd[i] = PAL_cppi4QueueOpen(priv->pal_hnd, rx_queue)))
            {
                printk(KERN_ERR "%s: Unable to open rx queue %d channel\n", __FUNCTION__, PAL_CPPI41_SR_MPEG_HOST_RX_Q_NUM);
                goto err;
            }

            priv->rxAcc_chan_hnd[i] = NULL;
            if(mpeg_init_acc_chan(priv->pal_hnd, PAL_CPPI41_MPEG_ACC_RX_CH_NUM, rx_queue, &priv->rxAcc_chan_hnd[i]))
            {
                printk(KERN_ERR "%s: Unable to open accumulator channel for %d priority channel\n", __FUNCTION__, i);
                goto err;                        
            }
            priv->rxAcc_chan_list_base[i] = priv->rxAcc_chan_list[i] = PAL_cppi4AccChGetNextList(priv->rxAcc_chan_hnd[i]);
        }
    }

    /* request the Rx IRQs */            
    if(request_irq (MPEG_RXINT_NUM, mpegEncapRootIsr, IRQF_DISABLED, "mpeg", priv))
    {
        printk(KERN_ERR "%s: unable to get IRQ #%d!\n", __FUNCTION__, MPEG_RXINT_NUM);
        goto err;
    }
    priv->irq_counter = 0;
    priv->frames_processed = 0;
    priv->host_forwarded = 0;

    printk("RX Init RX CPPI pass\n");
    return;
err:
    printk("RX Init RX CPPI failed\n");
    return;
}

/**************************************************************************/
/*! \fn static void init_cppi_tx(mpeg_process_private* priv)
 **************************************************************************
 *  \brief Init Mpeg Encap TX
 *  \return  0 or error code
 */
static void init_cppi_tx(mpeg_process_private* priv)
{
    int i_bd=0;
	Uint32 outstream_idx;

    priv->irq_counter = -1;
    priv->frames_processed = -1;
    for(outstream_idx=0; outstream_idx<MAX_MPEG_CHNS; ++outstream_idx)
        priv->encap_out_streams[outstream_idx].acc_frames = 0;

    printk("TX Init CPPI: open free descriptor queue\n");
    /* open free descriptor queue */
    {
        Cppi4Queue queue = {PAL_CPPI41_QUEUE_MGR_PARTITION_SR, PAL_CPPI41_SR_MPEG_ENCAP_FD_EMB_Q_NUM};
        if(!(priv->txMpegEncap_free_queue_hnd = PAL_cppi4QueueOpen(priv->pal_hnd, queue)))
        {
            printk(KERN_ERR "%s: Unable to open free desc queue %d priority channel\n", __FUNCTION__, PAL_CPPI41_SR_MPEG_ENCAP_FD_EMB_Q_NUM);
            goto err;
        }
    }

    printk("TX Init CPPI: allocate descriptors\n");
    /*
     * Define free embedded descriptors for channel
     * --------------------------------------------
     */
    printk("%s: Allocate descriptors to the embedded free queues\n", __FUNCTION__);
    if(!(priv->txMpegEncap_pdpool = PAL_cppi4AllocDesc (priv->pal_hnd, PAL_CPPI41_QUEUE_MGR_PARTITION_SR, DMAC_MPEG_ENCAP_RX_EMBEDDED_BD_NUM, MPEG_ENCAP_BD_SIZE)))
    {
        printk(KERN_ERR "%s: Unable to init BD pool\n", __FUNCTION__);
        goto err;        
    }

    printk("TX Init CPPI: Fill free discriptor queue\n");
    for (i_bd = 0; i_bd < DMAC_MPEG_ENCAP_RX_EMBEDDED_BD_NUM; i_bd++) 
    {
        Cppi4ExtEmbdDesc *bd = (Cppi4ExtEmbdDesc *)GET_MPEG_ENCAP_BD_PTR(priv->txMpegEncap_pdpool, i_bd);

        PAL_osMemSet(bd, 0, MPEG_ENCAP_BD_SIZE);
        bd->descInfo     = CPPI41_EM_DESCINFO_DTYPE_EMBEDDED | CPPI41_EM_DESCINFO_SLOTCNT_MPEG_ENCAP;
        bd->tagInfo      = 0;
        bd->pktInfo      = (PAL_CPPI4_HOSTDESC_PKT_TYPE_ETH << CPPI41_EM_PKTINFO_PKTTYPE_SHIFT) 
                            | (CPPI41_EM_PKTINFO_RETPOLICY_RETURN) 
                            | (0 << CPPI41_EM_PKTINFO_PROTSPEC_SHIFT) 
                            | (PAL_CPPI41_QUEUE_MGR_PARTITION_SR << CPPI41_EM_PKTINFO_RETQMGR_SHIFT) 
                            | (PAL_CPPI41_SR_MPEG_ENCAP_FD_EMB_Q_NUM << CPPI41_EM_PKTINFO_RETQ_SHIFT);
        PAL_CPPI4_CACHE_WRITEBACK(bd, CPPI4_BD_LENGTH_FOR_CACHE);
        PAL_cppi4QueuePush (priv->txMpegEncap_free_queue_hnd, (Ptr) PAL_CPPI4_VIRT_2_PHYS(bd), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(MPEG_ENCAP_BD_SIZE), 0);        
    }

    printk("TX Init CPPI: Create CPGMAC TX handler\n");
    {
        Cppi4Queue queue = {PAL_CPPI41_QUEUE_MGR_PARTITION_SR, 212};
        if(!(priv->txCpgmac_queue_hnd = PAL_cppi4QueueOpen(priv->pal_hnd, queue)))
        {
            printk(KERN_ERR "%s: Unable to open cpgmac  queue\n", __FUNCTION__);
            goto err;
        }
    }

    priv->try2alloc = 0;
    priv->outof_buf = 0;
    priv->outof_pd = 0;

    printk("TX Init CPPI: open mpeg TX channel\n");
    /*
     * Open MPEG-OUT TX channel and is TX-queue 
     * --------------------------------------------
     */
    {
        /* DMA channel configuration */
        {
            PAL_Cppi4TxChHnd mpegOutTxChHdl;
            volatile Cppi4TxChInitCfg mpegOutTxChInfo;
    
            mpegOutTxChInfo.chNum = MPEGOUT_CPPI4x_CHNUM;
            mpegOutTxChInfo.dmaNum = PAL_CPPI41_DMA_BLOCK1; 
            mpegOutTxChInfo.tdQueue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
            mpegOutTxChInfo.tdQueue.qNum = 218;
            mpegOutTxChInfo.defDescType = CPPI41_DESC_TYPE_EMBEDDED;
    
            mpegOutTxChHdl = PAL_cppi4TxChOpen (priv->pal_hnd, (Cppi4TxChInitCfg *)(&mpegOutTxChInfo), NULL);
            if(mpegOutTxChHdl == NULL)
            {
                printk("%s: Unable to open %d channel \n", __FUNCTION__, mpegOutTxChInfo.chNum);
                goto err;
            }
            PAL_cppi4EnableTxChannel (mpegOutTxChHdl, NULL);
            priv->txMpegOut_chn_hdl = &mpegOutTxChHdl;
        }
        /* TX queue channel configuration */
        {
            Cppi4Queue queue;   /* used generically */     

            queue.qNum = 218;
            queue.qMgr = PAL_CPPI41_QUEUE_MGR_PARTITION_SR;
            if(!(priv->txMpegOut_queue_hnd = PAL_cppi4QueueOpen(priv->pal_hnd, queue)))
            {
                printk("%s: unable to open Tx Queue #%d!\n", __FUNCTION__, queue.qNum);                
                goto err;
            }
        }
    }


    printk("TX Init CPPI pass\n");
    return;
err:
    printk("TX Init CPPI failed\n");
    return;
}


/**************************************************************************/
/*! \fn static Cppi4ExtEmbdDesc* allocate_host2tgw_resources(mpeg_process_private* priv, Ptr* buffer)
 **************************************************************************
 *  \brief Allocate CPPI buffer from ...
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[out]  Ptr* buffer - output buffer
 *  \return output BD pointing the buffer.
 **************************************************************************/
static Cppi4EmbdDesc_ps* allocate_host2tgw_resources(mpeg_process_private* priv, Ptr* buffer)
{
    Cppi4BufPool pool;
    Cppi4EmbdDesc_ps *bd;


    pool.bMgr = 0;
    pool.bPool = 8;

    *buffer = PAL_cppi4BufPopBuf(priv->pal_hnd, pool);
    bd = (Cppi4EmbdDesc_ps *)(PAL_cppi4QueuePop(priv->rxMpeg_free_queue_hnd));

    /* if no resources recycle buffer and descriptor */
    if (NULL==(*buffer) || NULL==bd)
    {
        /* recycle buffer */
        if (NULL!=(*buffer))
        {
            pool.bMgr = 0;
            pool.bPool = 8;
            PAL_cppi4BufDecRefCnt (priv->pal_hnd, pool, (*buffer));  
            printk("allocate_host2tgw_resources:: Error descriptor is NULL\n");
        }
        else if (NULL!=bd) /* recycle descriptor */
        {
            PAL_cppi4QueuePush(priv->rxMpeg_free_queue_hnd, bd, PAL_CPPI4_DESCSIZE_2_QMGRSIZE(64), 0);            
            printk("allocate_host2tgw_resources:: Error buffer is NULL\n");
        }
        return NULL;
    }

    bd = PAL_CPPI4_PHYS_2_VIRT(bd);
    PAL_CPPI4_CACHE_INVALIDATE(bd, 64);
 
    buffer = PAL_CPPI4_PHYS_2_VIRT(buffer);
 
    return bd;
}


/**************************************************************************/
/*! \fn static int sendHostData2OutStream (mpeg_process_private* priv, Ptr* buffer, Uint32 timestamp, Uint32 streamDestId)
 **************************************************************************
 *  \brief Put in CPPI queue data from user ...
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Ptr* buffer - output buffer
 *  \param[in]  Uint32 timestamp - Frame time stamp.
 *  \param[in]  Uint32 host2tgw_ps - host2tgw protocol specific.
 *  \return 
 **************************************************************************/
static int sendHostData2OutStream (mpeg_process_private* priv, Ptr* HostBuffer, Uint32 timestamp, Uint32 host2tgw_ps)
{
    Ptr buffer;
    Cppi4EmbdDesc_ps *bd;
    Uint32 num_of_buffers = 1; /* Host data is one Mpeg frame */


    bd = allocate_host2tgw_resources(priv, &buffer);
    if (NULL == bd)
        return -1;
    
    bd->PS[0] = timestamp;
    bd->PS[1] = host2tgw_ps;

    memcpy(buffer, HostBuffer, MPEG2TS_LEN); 

    /* update descriptor */
    bd->Buf[0].BufInfo = CPPI41_EM_BUF_VALID_MASK | 
                         (PAL_CPPI41_QUEUE_MGR_PARTITION_SR << CPPI41_EM_BUF_MGR_SHIFT) | 
                         (8 << CPPI41_EM_BUF_POOL_SHIFT) | MPEG2TS_LEN;

    bd->Buf[0].BufPtr = (Uint32)buffer;

    /* set descriptors */
    bd->descInfo     = (bd->descInfo & ~(CPPI41_EM_DESCINFO_PKTLEN_MASK));  
    bd->descInfo     = (bd->descInfo | ((MPEG2TS_LEN) & CPPI41_EM_DESCINFO_PKTLEN_MASK));
    bd->pktInfo      = (bd->pktInfo & ~(CPPI41_EM_PKTINFO_EOPIDX_MASK));
    bd->pktInfo      = (bd->pktInfo | ((num_of_buffers-1)<<CPPI41_EM_PKTINFO_EOPIDX_SHIFT)); 

    PAL_CPPI4_CACHE_WRITEBACK(bd, 64);

    PAL_cppi4QueuePush(priv->rx_queue_hnd[0], (Uint32 *) PAL_CPPI4_VIRT_2_PHYS(bd), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(64), 0);

    return 0;
}

/**************************************************************************/
/*! \fn void host2tgw_rx_workQueue(struct work_struct *work)
 **************************************************************************
 *  \brief Host2Tgw Work Queue
 *  \param[in]  struct work_struct *work - mpeg_process_private pointer
 *  \return none.
 **************************************************************************/ 
void host2tgw_rx_workQueue(struct work_struct *work)
{
   mpeg_process_private        *priv = container_of(work, mpeg_process_private, mpeg_host2tgw_wq);
    struct sk_buff *skb = NULL;
    struct nlmsghdr *nlh = NULL;
    SoCMPEGEncapSectionFrame *host_data = NULL;

    while ((skb = skb_dequeue(&priv->mpeg_nl_sk->sk_receive_queue)) != NULL)
    {
        /* process netlink message pointed by skb->data */
        nlh = (struct nlmsghdr *)skb->data;
        host_data = (SoCMPEGEncapSectionFrame *) NLMSG_DATA(nlh);
        /* process netlink message with header pointed by
         * nlh	and payload pointed by host_data
         */
        if (-1 == sendHostData2OutStream(priv, (Ptr*)host_data->frame_payload , host_data->timestamp, host_data->host2tgw_ps))
        {
            printk("TGW kernel -host2tgw_rx_workQueue sendHostData2OutStream FAILED \n");
        }
        kfree_skb(skb);
    }
}


/**************************************************************************/
/*! \fn static void nl_tgw_data_ready (struct sock *sk, int len)
 **************************************************************************
 *  \brief Mpeg Encap Netlink socket data ready callback
 *  \return  0 or error code
 */
static void nl_tgw_data_ready (struct sk_buff *skb)
{
    if(skb && skb->sk)
        schedule_work((struct work_struct *)skb->sk->sk_wq);
}

/**************************************************************************/
/*! \fn static void init_work_queue(mpeg_process_private* priv)
 **************************************************************************
 *  \brief Init Mpeg Encap and Host2Tgw Work queues 
 *  \return  0 or error code
 */
static void init_work_queue(mpeg_process_private* priv)
{
    INIT_WORK(&priv->mpeg_encap_wq, mpegEncapSectionWorkQueue);

    printk("WQ Init CPPI: open queue descriptor queue\n");
    /* open free descriptor queue */
    {
        Cppi4Queue queue = {PAL_CPPI41_QUEUE_MGR_PARTITION_SR, MPEG_CPPI4x_TX_SESSION_QNUM(0)};
        if(!(priv->wqMpegEncap_queue_hnd = PAL_cppi4QueueOpen(priv->pal_hnd, queue)))
        {
            printk(KERN_ERR "%s: Unable to open free desc queue %d priority channel\n", __FUNCTION__, MPEG_CPPI4x_TX_SESSION_QNUM(0));
            goto err;
        }
    }

    priv->mpeg_nl_sk = netlink_kernel_create(&init_net,NETLINK_TI_MPEG_ENCAP, 1, nl_tgw_data_ready, NULL ,THIS_MODULE);
    if ( priv->mpeg_nl_sk == NULL ) 
    {
        printk("NetLink-Socket: sock fail\n");
        return;
    }
    priv->mpeg_nl_counter = 0;

    INIT_WORK(&priv->mpeg_host2tgw_wq, host2tgw_rx_workQueue);
    priv->mpeg_nl_sk->sk_wq = (void *)&priv->mpeg_host2tgw_wq;

    printk("WQ Init pass\n");
    return;
err:
    printk("WQ Init failed\n");
    return;
}

/**************************************************************************/
/*! \fn static void set_packet_header(struct encaphdr *hdr, 
                              Uint8* mac_src_addr, 
                              Uint8* mac_dst_addr, 
                              Uint32 ip4_src_addr, 
                              Uint32 ip4_dst_addr, 
                              Uint16 udp_src_port,
                              Uint16 udp_dst_port,
                              Uint32 rtp_ssrc)
 **************************************************************************
 *  \brief Set Mpeg Encap IP packet header
 *  \return  0 or error code
 */
static void set_packet_header(struct encaphdr *hdr, 
                              Uint8* mac_src_addr, 
                              Uint8* mac_dst_addr, 
                              Uint32 ip4_src_addr, 
                              Uint32 ip4_dst_addr, 
                              Uint16 udp_src_port,
                              Uint16 udp_dst_port,
                              Uint32 rtp_ssrc)
{
    memcpy(hdr->eth_header.h_dest, mac_dst_addr,  ETH_ALEN);
    memcpy(hdr->eth_header.h_source, mac_src_addr,  ETH_ALEN);
    hdr->eth_header.h_proto = 0x0800;

    hdr->ip_header.version =   0x4;
    hdr->ip_header.ihl =       0x5;
    hdr->ip_header.tos =       0x0;
    hdr->ip_header.tot_len =   0;
    hdr->ip_header.id =        0x0;
    hdr->ip_header.frag_off =  0x0;
    hdr->ip_header.ttl =       64;
    hdr->ip_header.protocol =  17;
    hdr->ip_header.check =     0;
    hdr->ip_header.saddr =     ip4_src_addr; //in_aton(ip_src_addr);
    hdr->ip_header.daddr =     ip4_dst_addr;

    hdr->udp_header.source =   udp_src_port;
    hdr->udp_header.dest =     udp_dst_port;
    hdr->udp_header.len =      0;
    hdr->udp_header.check =    0x0000;

    hdr->rtp_header.ver =          2;
    hdr->rtp_header.p =            0;
    hdr->rtp_header.x =            0;
    hdr->rtp_header.cc =           0;
    hdr->rtp_header.m =            0;
    hdr->rtp_header.pt =           33;
    hdr->rtp_header.seq_num =      0;
    hdr->rtp_header.timestamp =    0;
    hdr->rtp_header.ssrc =         rtp_ssrc;

//    return ip_fast_csum((unsigned char *)(&hdr->ip_header), hdr->ip_header.ihl);
}

/**************************************************************************/
/*      INTERFACE  Functions                                              */
/**************************************************************************/

/**************************************************************************/
/*! \fn void mpegEncapUnitInit(mpeg_process_private* priv)
 **************************************************************************
 *  \brief Init Mpeg Encap Unit
 *  \return  0 or error code
 */
void mpegEncapUnitInit(mpeg_process_private* priv)
{
	int chn_idx, pid_idx;

    priv->mpeg_proc = create_proc_entry("mpeg", 0, NULL);
    
    if (NULL==priv->mpeg_proc)
    {
        printk("MPEG driver failure, can not create MPEG proc node\n");
        BUG();
        return;
    }
    
    priv->mpeg_proc->read_proc = mpeg_read_proc;
    priv->mpeg_proc->write_proc = mpeg_write_proc;
    priv->mpeg_proc->data = (void*)priv;
    priv->irq_counter = ~0;

	for (chn_idx=0; chn_idx<MAX_MPEG_CHNS; chn_idx++)
	{
		for (pid_idx=0; pid_idx<PID_SPACE; pid_idx++)
		{
			SET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][pid_idx].filter, INVALID_ACTIVE_PID_IDX);
		}
		for (pid_idx=0; pid_idx<MAX_MPEG_ACTIVE_PIDS; pid_idx++)
		{
			priv->encap_pid_params[chn_idx][pid_idx].pid = MPEG_INVALID_PID;
		}

	}

    /* get a PAL handle */
    priv->pal_hnd = PAL_cppi4Init(NULL, NULL);
    init_cppi_rx(priv);
    init_cppi_tx(priv);
    init_work_queue(priv);

    printk("MPEG driver initialization complete.\n");
}

/**************************************************************************/
/*! \fn void mpegEncapUnitCleanup(mpeg_process_private* priv) 
 **************************************************************************
 *  \brief Mpeg Encap Unit cleanup
 *  \return  0 or error code
 */
void mpegEncapUnitCleanup(mpeg_process_private* priv) 
{
    int chn_idx, pid_idx, out_stream_idx;

    /* Remove netlink socket */
    if (priv->mpeg_nl_sk)
        sock_release(priv->mpeg_nl_sk->sk_socket);
    
    remove_proc_entry("mpeg", NULL);

    /*Free frame modification data*/
    for (chn_idx=0; chn_idx<MAX_MPEG_CHNS; chn_idx++)
        for (pid_idx=0; pid_idx<MAX_MPEG_ACTIVE_PIDS; pid_idx++)
            for (out_stream_idx=0; out_stream_idx < MAX_MPEG_ENCAP_OUT_STREAMS; out_stream_idx++)
            {
                if (priv->encap_pid_params[chn_idx][pid_idx].frame_manip)
                    mpegEncapFrameModDelPidOutStreamRule(chn_idx, pid_idx, out_stream_idx);
                else
                    break;
            }

    printk("MPEG driver cleanup complete.\n");
}

/**************************************************************************/
/*! \fn void mpegEncapSetPacketHeader(encap_out_stream_t *out_stream,
                                      Uint8*            mac_src_addr, 
                                      Uint8*            mac_dst_addr, 
                                      Uint32            ip4_src_addr, 
                                      Uint32            ip4_dst_addr, 
                                      Uint16            udp_src_port,
                                      Uint16            udp_dst_port,
                                      encapHeaderType_e encap_type,
                                      udpCsum_e         udp_csum,
                                      Uint32            rtp_ssrc)
 **************************************************************************
 *  \brief Set Mpeg Encap IP packet header API
 *  \return  0 or error code
 */
void mpegEncapSetPacketHeader(encap_out_stream_t *out_stream,
                              Uint8*            mac_src_addr, 
                              Uint8*            mac_dst_addr, 
                              Uint32            ip4_src_addr, 
                              Uint32            ip4_dst_addr, 
                              Uint16            udp_src_port,
                              Uint16            udp_dst_port,
                              encapHeaderType_e encap_type,
                              udpCsum_e         udp_csum,
                              Uint32            rtp_ssrc)
{
    memcpy(out_stream->eth_dst_addr, mac_dst_addr, ETH_ALEN);
    out_stream->ip_id                  = 0;
    out_stream->rtp_seq_num            = 0;
    out_stream->pcr2pcr_frames_counter = -1;
    out_stream->old_sysclk             = 0;
    out_stream->old_90K_clk            = 0;
    set_packet_header(&(out_stream->encap_header), 
                      mac_src_addr, mac_dst_addr, 
                      ip4_src_addr, ip4_dst_addr,
                      udp_src_port, udp_dst_port, 
                      rtp_ssrc);
    out_stream->encap_header_type    = encap_type;    
    out_stream->udp_csum             = udp_csum;    

    out_stream->encap_frame_headers_size = sizeof(out_stream->encap_header.eth_header) +
                                           sizeof(out_stream->encap_header.ip_header) +
                                           sizeof(out_stream->encap_header.udp_header); 
    if (MPEG_ENCAP_RTP_HEADER==out_stream->encap_header_type)
        out_stream->encap_frame_headers_size += sizeof(out_stream->encap_header.rtp_header);
}

/**************************************************************************/
/*! \fn void mpegEncapSetRtpTsProperties(encap_out_stream_t     *out_stream,
										 Uint16                pcr_pid,
										 rtpTsProperties_e     rtp_ts)

 **************************************************************************
 *  \brief Set RTP TS properties API
 *  \return  0 or error code
 */
void mpegEncapSetRtpTsProperties(encap_out_stream_t     *out_stream,
                                 Uint16                pcr_pid,
                                 rtpTsProperties_e     rtp_ts)
{
    if (MPEG_ENCAP_RTP_TS_DISABLE==rtp_ts)
        out_stream->pcr_pid    = 0;
    else
        out_stream->pcr_pid    = pcr_pid;
    out_stream->rtp_ts_flag    = rtp_ts;
}
