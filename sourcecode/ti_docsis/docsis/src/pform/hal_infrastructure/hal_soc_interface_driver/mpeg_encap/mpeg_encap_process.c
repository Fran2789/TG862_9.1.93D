/*
 *
 * mpeg_encap_process.c
 * Description:
 * MPEG Encapsulation functionality implementation
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/ 
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

#define UDP_CSUM

#ifdef MPEG_ENCAP_DEBUG
#define MPEG_ENCAP_INC_DBG_COUNT(V)	++(V)
#else
#define MPEG_ENCAP_INC_DBG_COUNT(V)
#endif

/**************************************************************************/
/*      DEFINES                                                           */
/**************************************************************************/

typedef struct {
        Uint8   adaptation_field_length;
        Uint8   adaptation_field_flags;
        Uint32  optional_fields[2];
} __attribute__((packed)) adaptation_field_t;

typedef struct {
    Uint32              basic_mpeg_header;
    adaptation_field_t  adaptation_field;
} __attribute__((packed)) mpeg_header_t;

/************************************************************************/
/*     CNI driver private data                                          */
/************************************************************************/

/* Static functions */
static void recycle_rx_frame_buffer(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd);
static void recycle_rx_frame_bd(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd);
static Cppi4ExtEmbdDesc* allocate_tx_resources(mpeg_process_private* priv, Ptr* buffer);
static Ptr allocate_rx_buf(mpeg_process_private* priv);
static Ptr copy_rx_frame_buffer(mpeg_process_private* priv, Ptr origBufPtr);
static Uint32 inline pcr_sysclk_to_90K_clk(Uint32 sysclk, Uint32 *old_sysclk, long long *old_90K_clk);
static Bool inline is_pcr(mpeg_process_private* priv, mpeg_header_t *mpeg_headers, Uint8 outstream_idx);
static void calc_delta_pcr_factor(mpeg_process_private* priv, mpeg_header_t *mpeg_headers, Uint8 outstream_idx, Uint32 local_timestamp);
static Uint32 inline do_rtp_timestamp(mpeg_process_private* priv, mpeg_header_t *mpeg_headers, Uint8 outstream_idx, Uint32 local_timestamp);
static int ip_forward_frame(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd, Uint8 ch_idx, Uint32 outstream_idx, Uint32 pid_index);
static int mpegout_forward_frame(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd);
static int send_nl_msg(mpeg_process_private *priv, Cppi4EmbdDesc_ps *bd, mpeg_header_t *mpeg_headers);
static void mpegEncapFastForward(mpeg_process_private* priv, Uint8 chn_idx, Uint16 pid, Cppi4EmbdDesc_ps* bd);
static void mpegEncapHostForward(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd);


/**************************************************************************/
/*      STATIC  Functions                                                 */
/**************************************************************************/

/**************************************************************************/
/*! \fn static void duplicate_rx_frame_buffer_nocopy(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd)
 **************************************************************************
 *  \brief Duplicate a buffer by incrementing its reference counter without copying
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Cppi4EmbdDesc_ps* bd - Buffer descriptor
 *  \return none.
 **************************************************************************/
static void duplicate_rx_frame_buffer_nocopy(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd)
{
    Cppi4BufPool pool;

	pool.bMgr = 0;
#if PUMA6_OR_NEWER_SOC_TYPE
    pool.bPool = PAL_CPPI41_BMGR_POOL12;
#else
    pool.bPool = PAL_CPPI41_BMGR_POOL2;
#endif

    PAL_cppi4BufIncRefCnt (priv->pal_hnd, pool, (Ptr)(bd->Buf[0].BufPtr));             

}

/**************************************************************************/
/*! \fn static void recycle_rx_frame_buffer(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd)
 **************************************************************************
 *  \brief Recycle a buffer by decreasing its reference counter
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Cppi4EmbdDesc_ps* bd - Buffer descriptor
 *  \return none.
 **************************************************************************/
static void recycle_rx_frame_buffer(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd)
{
    Cppi4BufPool pool;

    pool.bMgr = 0;
#if PUMA6_OR_NEWER_SOC_TYPE
    pool.bPool = PAL_CPPI41_BMGR_POOL12;
#else
    pool.bPool = PAL_CPPI41_BMGR_POOL2;
#endif

	PAL_cppi4BufDecRefCnt (priv->pal_hnd, pool, (Ptr)(bd->Buf[0].BufPtr));             

}

/**************************************************************************/
/*! \fn static void recycle_rx_frame_bd(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd)
 **************************************************************************
 *  \brief Recycle frame BD
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Cppi4EmbdDesc_ps* bd - Buffer descriptor
 *  \return none.
 **************************************************************************/
static void recycle_rx_frame_bd(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd)
{
    PAL_cppi4QueuePush(priv->rxMpeg_free_queue_hnd, (Uint32 *) PAL_CPPI4_VIRT_2_PHYS(bd), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(MPEG_BD_SIZE), 0);            
}

/**************************************************************************/
/*! \fn static Cppi4ExtEmbdDesc* allocate_tx_resources(mpeg_process_private* priv, Ptr* buffer)
 **************************************************************************
 *  \brief Allocate CPPI buffer from Mpeg Encap TX pool
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[out]  Ptr* buffer - output buffer
 *  \return output BD pointing the buffer.
 **************************************************************************/
static Cppi4ExtEmbdDesc* allocate_tx_resources(mpeg_process_private* priv, Ptr* buffer)
{
    Cppi4BufPool pool;
    Cppi4ExtEmbdDesc *bd;

    pool.bMgr = 0;
#if PUMA6_OR_NEWER_SOC_TYPE
    pool.bPool = PAL_CPPI41_BMGR_POOL13;
#else
    pool.bPool = PAL_CPPI41_BMGR_POOL8;
#endif

    *buffer = PAL_cppi4BufPopBuf(priv->pal_hnd, pool);
    bd = (Cppi4ExtEmbdDesc *)(PAL_cppi4QueuePop(priv->txMpegEncap_free_queue_hnd));

    ++(priv->try2alloc);
    /* if no resources recycle buffer and descriptor */
    if (NULL==(*buffer) || NULL==bd)
    {
        /* recycle buffer */
        if (NULL!=(*buffer))
        {
            PAL_cppi4BufDecRefCnt (priv->pal_hnd, pool, (*buffer));             
        }
        else
            ++(priv->outof_buf);

        /* recycle descriptor */
        if (NULL!=bd)
        {
            PAL_cppi4QueuePush(priv->txMpegEncap_free_queue_hnd, bd, PAL_CPPI4_DESCSIZE_2_QMGRSIZE(MPEG_ENCAP_BD_SIZE), 0);            
        }
        else
            ++(priv->outof_pd);

        return NULL;
    }

    bd = PAL_CPPI4_PHYS_2_VIRT(bd);
    PAL_CPPI4_CACHE_INVALIDATE(bd, MPEG_ENCAP_BD_SIZE);

    return bd;
}

/**************************************************************************/
/*! \fn static Ptr allocate_rx_buf(mpeg_process_private* priv)
 **************************************************************************
 *  \brief Allocate CPPI buffer from Mpeg Encap RX pool
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \return Output buffer pointer.
 **************************************************************************/
static Ptr allocate_rx_buf(mpeg_process_private* priv)
{
    Cppi4BufPool pool;
	Ptr			 bufPtr;

    pool.bMgr = 0;
#if PUMA6_OR_NEWER_SOC_TYPE
    pool.bPool = PAL_CPPI41_BMGR_POOL12;
#else
    pool.bPool = PAL_CPPI41_BMGR_POOL2;
#endif

    bufPtr = PAL_cppi4BufPopBuf(priv->pal_hnd, pool);

	if (!bufPtr)
		++(priv->outof_buf);

	return bufPtr;
}

/**************************************************************************/
/*! \fn static Ptr copy_rx_frame_buffer(mpeg_process_private* priv, Ptr origBufPtr)
 **************************************************************************
 *  \brief Allocate new buffer from Mpeg Encap RX pool and copy into it the content of original buffer
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Ptr buffer - original frame buffer
 *  \return output buffer.
 **************************************************************************/
static Ptr copy_rx_frame_buffer(mpeg_process_private* priv, Ptr origBufPtr)
{
	Ptr			 bufPtr;

	bufPtr = allocate_rx_buf(priv);
	if (!bufPtr)
		return NULL;

	bufPtr = (Ptr)PAL_CPPI4_PHYS_2_VIRT(bufPtr);
	origBufPtr = (Ptr)PAL_CPPI4_PHYS_2_VIRT(origBufPtr);
	PAL_CPPI4_CACHE_INVALIDATE(origBufPtr, MPEG2TS_LEN);
	memcpy(bufPtr, origBufPtr, MPEG2TS_LEN);
	PAL_CPPI4_CACHE_WRITEBACK(bufPtr, MPEG2TS_LEN);

	return (Ptr)PAL_CPPI4_VIRT_2_PHYS(bufPtr);
}

/**************************************************************************/
/*! \fn static Uint32 inline pcr_sysclk_to_90K_clk(Uint32 sysclk, Uint32 *old_sysclk, long long *old_90K_clk)
 **************************************************************************
 **************************************************************************/
static Uint32 inline pcr_sysclk_to_90K_clk(Uint32 sysclk, Uint32 *old_sysclk, long long *old_90K_clk)
{
    Uint32 delta;
    long long new_90K_clk;

    /* find the delta between to sample in 1024Mhz*/
    if ( sysclk==(*old_sysclk) )
    {
        return (Uint32)((*old_90K_clk) & 0xFFFFFFFF);
    }
    else if ( sysclk>(*old_sysclk) )
    {
        delta = sysclk - (*old_sysclk); 
    }
    else
    {
        /* not engouth bit so we must calc it w/ the good old two coplement */
        delta = (~(*old_sysclk)) + 1 - sysclk; /* same as: (2^32 - old) + new */
    }

    /* now we are traslating the delta to 90Khz clock w/ 10 bits fix point (assuming the we does not have over flow) */
    /* Now we factor it by 90K/1024M Mega: 1/128 + 1/1024 assuming 10 bits fix point*/
    delta = delta + (delta<<3);

    /* Now we will calcualte the new 90KHz clock */
    new_90K_clk = *old_90K_clk + delta;

    /* save sysclk and 90KHz_clk to later use*/
    *old_sysclk = sysclk;
    *old_90K_clk = new_90K_clk;

    return (Uint32)(((new_90K_clk+512)>>10) & 0xFFFFFFFF);

}

/**************************************************************************/
/*! \fn static Bool inline is_pcr(mpeg_process_private* priv, mpeg_header_t *mpeg_headers, Uint8 outstream_idx)
 **************************************************************************
 *  \brief Determine if mpeg header has PCR 
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  mpeg_header_t *mpeg_headers
 *  \param[in]  Uint8 outstream_idx
 *  \return True/False.
 **************************************************************************/
static Bool inline is_pcr(mpeg_process_private* priv, mpeg_header_t *mpeg_headers, Uint8 outstream_idx)
{
    if (0!=priv->encap_out_streams[outstream_idx].pcr_pid)
        if (priv->encap_out_streams[outstream_idx].pcr_pid==MPEG_GET_PID(mpeg_headers->basic_mpeg_header))
            if (MPEG_ENCAP_RTP_HEADER==priv->encap_out_streams[outstream_idx].encap_header_type)
                if (IS_ADAPTEATION_FIELD_EXIST(mpeg_headers->basic_mpeg_header))
                    if (0!=mpeg_headers->adaptation_field.adaptation_field_length)
                        if (IS_PCR_EXIST(mpeg_headers->adaptation_field.adaptation_field_flags))
                            return True;

    return False;
}

/**************************************************************************/
/*! \fn static void calc_delta_pcr_factor(mpeg_process_private* priv, mpeg_header_t *mpeg_headers, Uint8 outstream_idx, Uint32 local_timestamp)
 **************************************************************************
 *  \brief Calculate delta PCR factor
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  mpeg_header_t *mpeg_headers
 *  \param[in]  Uint8 outstream_idx
 *  \return none.
 **************************************************************************/
static void calc_delta_pcr_factor(mpeg_process_private* priv, mpeg_header_t *mpeg_headers, Uint8 outstream_idx, Uint32 local_timestamp)
{
    Uint32 base_pcr;

    if (MPEG_ENCAP_RTP_TS_DLNA!=priv->encap_out_streams[outstream_idx].rtp_ts_flag)
        return;

    base_pcr = 
        ( (mpeg_headers->adaptation_field.optional_fields[0])<<1 ) + 
        ( (mpeg_headers->adaptation_field.optional_fields[1]>>31)?1:0 );

    /* if no older information is exist then skip calculations */
    if (-1==priv->encap_out_streams[outstream_idx].pcr2pcr_frames_counter)
    {
        /* Start new counter */
        priv->encap_out_streams[outstream_idx].pcr2pcr_frames_counter = 0;
        priv->encap_out_streams[outstream_idx].last_pcr_ts_at90K = base_pcr;
        priv->encap_out_streams[outstream_idx].deltaN_pcr_factor = 0;
        priv->encap_out_streams[outstream_idx].accumulated_deltaN_pcr_factor = 0;
        return;
    }

    /* calculate Delta_PCR/N=X.Y when X is 16 bits and Y is 16 bits */
    {
        Uint32  new_pcr     = base_pcr;
        Uint32  N           = priv->encap_out_streams[outstream_idx].pcr2pcr_frames_counter;
        Uint32  delta90K;

        if (new_pcr>priv->encap_out_streams[outstream_idx].last_pcr_ts_at90K)
        {
            delta90K = (new_pcr - priv->encap_out_streams[outstream_idx].last_pcr_ts_at90K)<<16;
        } else
        {
            delta90K = ( ~priv->encap_out_streams[outstream_idx].last_pcr_ts_at90K + 1 - new_pcr)<<16;
        }


        /* calculate delta_PCR/N factor */
        priv->encap_out_streams[outstream_idx].deltaN_pcr_factor = delta90K/N;
        priv->encap_out_streams[outstream_idx].accumulated_deltaN_pcr_factor = 0;

        /* Start new counter */
        priv->encap_out_streams[outstream_idx].pcr2pcr_frames_counter = 0;
        priv->encap_out_streams[outstream_idx].last_pcr_ts_at90K = new_pcr; 

        /* Let's calcualte local timestamp in 90KHz counter */
        priv->encap_out_streams[outstream_idx].old_90K_clk_32bits = pcr_sysclk_to_90K_clk(local_timestamp,
                                                                              &(priv->encap_out_streams[outstream_idx].old_sysclk), 
                                                                              &(priv->encap_out_streams[outstream_idx].old_90K_clk));
    }
}

/**************************************************************************/
/*! \fn static Uint32 inline do_rtp_timestamp(mpeg_process_private* priv, mpeg_header_t *mpeg_headers, Uint8 outstream_idx, Uint32 local_timestamp)
 **************************************************************************
 *  \brief Hand;e RTP timestamp
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  mpeg_header_t *mpeg_headers
 *  \param[in]  Uint8 outstream_idx
 *  \return timestamp.
 **************************************************************************/
static Uint32 inline do_rtp_timestamp(mpeg_process_private* priv, mpeg_header_t *mpeg_headers, Uint8 outstream_idx, Uint32 local_timestamp)
{
    Uint32 local_timestamp_90K;

    if (MPEG_ENCAP_RTP_TS_DLNA==priv->encap_out_streams[outstream_idx].rtp_ts_flag)
    {
        /* Let's calcualte local timestamp in 90KHz counter */
        local_timestamp_90K = priv->encap_out_streams[outstream_idx].old_90K_clk_32bits;
        /* now everything is ready for generating RTP timestamp following DLNA1.5 7.4.160 requeiement */
        return ( local_timestamp_90K + (priv->encap_out_streams[outstream_idx].accumulated_deltaN_pcr_factor>>16) );
    }
    else if (MPEG_ENCAP_RTP_TS_LOCAL==priv->encap_out_streams[outstream_idx].rtp_ts_flag)
    {
        local_timestamp_90K = pcr_sysclk_to_90K_clk(local_timestamp, 
                                                    &(priv->encap_out_streams[outstream_idx].old_sysclk), 
                                                    &(priv->encap_out_streams[outstream_idx].old_90K_clk));
        return local_timestamp_90K;
    }
    else /* MPEG_ENCAP_RTP_TS_DISABLE or none exist value */
        return 0;

}

/**************************************************************************/
/*! \fn static int ip_forward_frame(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd, Uint8 ch_idx, Uint32 outstream_idx, Uint32 pid_index)
 **************************************************************************
 *  \brief Forward a frame to encapsulating IP stream
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Cppi4EmbdDesc_ps* bd - frame BD
 *  \param[in]  Uint8 ch_idx - channel index
 *  \param[in]  Uint8 outstream_idx - IP stream index
 *  \param[in]  Uint32 pid_index - frame PID index
 *  \return 0 or error code.
 **************************************************************************/
static int ip_forward_frame(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd, Uint8 ch_idx, Uint32 outstream_idx, Uint32 pid_index)
{
    /* if rtp then update the counters */
    if (MPEG_ENCAP_RTP_HEADER==priv->encap_out_streams[outstream_idx].encap_header_type && 
        MPEG_ENCAP_RTP_TS_DLNA==priv->encap_out_streams[outstream_idx].rtp_ts_flag)
    {
        /* we have more frame in the basket now, let's increment the counter and the accumelated deltaN factor */
        priv->encap_out_streams[outstream_idx].accumulated_deltaN_pcr_factor += priv->encap_out_streams[outstream_idx].deltaN_pcr_factor;
        priv->encap_out_streams[outstream_idx].pcr2pcr_frames_counter+=1;
    }

    if (0==priv->encap_out_streams[outstream_idx].acc_frames)
    {
        Ptr buffer;

        priv->encap_out_streams[outstream_idx].encap_bd = allocate_tx_resources(priv, &buffer);
        if (NULL==priv->encap_out_streams[outstream_idx].encap_bd)
            return -1;

        /* Attached header and increament the accumelated frames */
        priv->encap_out_streams[outstream_idx].encap_bd->Buf[0].BufInfo = 
            CPPI41_EM_BUF_VALID_MASK | 
            (PAL_CPPI41_QUEUE_MGR_PARTITION_SR << CPPI41_EM_BUF_MGR_SHIFT) |
            (8 << CPPI41_EM_BUF_POOL_SHIFT) |
            priv->encap_out_streams[outstream_idx].encap_frame_headers_size;
        priv->encap_out_streams[outstream_idx].encap_bd->Buf[0].BufPtr = (Uint32)buffer;

        /* zero UDP payload csum */
        priv->encap_out_streams[outstream_idx].udp_payload_csum = 0;

        /* if rtp then */
        if (MPEG_ENCAP_RTP_HEADER==priv->encap_out_streams[outstream_idx].encap_header_type)
        {
            mpeg_header_t *mpeg_headers = (mpeg_header_t *)PAL_CPPI4_PHYS_2_VIRT(bd->Buf[0].BufPtr);
            priv->encap_out_streams[outstream_idx].rtp_timestamp = do_rtp_timestamp(priv, mpeg_headers, outstream_idx, bd->PS[0]);
        }
    }
    ++(priv->encap_out_streams[outstream_idx].acc_frames);
    priv->encap_out_streams[outstream_idx].encap_bd->Buf[priv->encap_out_streams[outstream_idx].acc_frames] = bd->Buf[0];

    /* Modify frame if needed */
	if (priv->encap_pid_params[ch_idx][pid_index].frame_manip)  
    {
		/*If frame manipulation is defined for the given PID for this particular IP stream*/
		if (priv->encap_pid_params[ch_idx][pid_index].frame_manip[outstream_idx])
		{
			Uint8* payload_ptr;
			Ptr frame_buf_copy;

			frame_buf_copy = copy_rx_frame_buffer(priv, (Ptr)bd->Buf[0].BufPtr);
			if (!frame_buf_copy)
				return -1;

			/*Replace the original buffer by its copy before modifying it - original content may be required for other IP streams*/
			priv->encap_out_streams[outstream_idx].encap_bd->Buf[priv->encap_out_streams[outstream_idx].acc_frames].BufPtr = (Uint32)frame_buf_copy;
			/*Return the original buffer*/
			recycle_rx_frame_buffer(priv, bd);

			payload_ptr = (Uint8 *)PAL_CPPI4_PHYS_2_VIRT(frame_buf_copy);

			memcpy(payload_ptr+priv->encap_pid_params[ch_idx][pid_index].frame_manip[outstream_idx]->frame_offset, 
				   &(priv->encap_pid_params[ch_idx][pid_index].frame_manip[outstream_idx]->new_frame[0]),
				   priv->encap_pid_params[ch_idx][pid_index].frame_manip[outstream_idx]->new_frame_length);
	
			PAL_CPPI4_CACHE_WRITEBACK(payload_ptr+priv->encap_pid_params[ch_idx][pid_index].frame_manip[outstream_idx]->frame_offset, 
									  priv->encap_pid_params[ch_idx][pid_index].frame_manip[outstream_idx]->new_frame_length);
		}
    }

    /* calculate buffer csum */
    if (MPEG_ENCAP_ENABLE_UDP_CSUM==priv->encap_out_streams[outstream_idx].udp_csum)
    {
        Uint8* payload_ptr = (Uint8 *)PAL_CPPI4_PHYS_2_VIRT(bd->Buf[0].BufPtr);
        PAL_CPPI4_CACHE_INVALIDATE(payload_ptr, 188);
        priv->encap_out_streams[outstream_idx].udp_payload_csum = csum_partial(payload_ptr, 188, priv->encap_out_streams[outstream_idx].udp_payload_csum);
    }

    /* for last frame calcualte IP/UDP/RTP headers and update descriptors */
    {
        mpeg_header_t *mpeg_headers = (mpeg_header_t *)PAL_CPPI4_PHYS_2_VIRT(bd->Buf[0].BufPtr);
        Bool    is_pcr_flag = is_pcr(priv, mpeg_headers, outstream_idx);
        Uint16  num_of_frames = (True==is_pcr_flag)?priv->encap_out_streams[outstream_idx].acc_frames:MAX_CONCAT_FRAMES;

        if (True==is_pcr_flag ||    
            MAX_CONCAT_FRAMES==priv->encap_out_streams[outstream_idx].acc_frames)
        {
            struct encaphdr* hdr = &(priv->encap_out_streams[outstream_idx].encap_header);
            Uint8* bd_hdr = (Uint8*)PAL_CPPI4_PHYS_2_VIRT(priv->encap_out_streams[outstream_idx].encap_bd->Buf[0].BufPtr);
            Uint32 udp_rtp_headers_size = sizeof(hdr->udp_header);
            Uint32 udp_packet_size = sizeof(hdr->udp_header) + num_of_frames * MPEG2TS_LEN;
    
            /* find headers size */
            if (MPEG_ENCAP_RTP_HEADER==priv->encap_out_streams[outstream_idx].encap_header_type)
            {
                udp_rtp_headers_size += sizeof(hdr->rtp_header);
                udp_packet_size += sizeof(hdr->rtp_header);
            }
    
            /* calc IP/UDP/RTP headers */
            hdr->ip_header.tot_len = sizeof(hdr->ip_header) + udp_packet_size;
            hdr->ip_header.id = ++(priv->encap_out_streams[outstream_idx].ip_id);
            hdr->ip_header.check = 0;
            hdr->ip_header.check = ip_fast_csum((unsigned char *)(&hdr->ip_header), hdr->ip_header.ihl);
            hdr->udp_header.len = udp_packet_size;
            hdr->udp_header.check = 0;

            /* work hard only for RTP and for the leader ES */
            if (MPEG_ENCAP_RTP_HEADER==priv->encap_out_streams[outstream_idx].encap_header_type)
            {
                hdr->rtp_header.seq_num = ++(priv->encap_out_streams[outstream_idx].rtp_seq_num);
                hdr->rtp_header.timestamp = priv->encap_out_streams[outstream_idx].rtp_timestamp;
                if (True==is_pcr_flag)
                {
                    calc_delta_pcr_factor(priv, mpeg_headers, outstream_idx, bd->PS[0]);
                }
            }

            if (MPEG_ENCAP_ENABLE_UDP_CSUM==priv->encap_out_streams[outstream_idx].udp_csum)
            {
                Uint32 sum = csum_partial((Uint8*)&hdr->udp_header, udp_rtp_headers_size, priv->encap_out_streams[outstream_idx].udp_payload_csum);
                hdr->udp_header.check = csum_tcpudp_magic(hdr->ip_header.saddr, hdr->ip_header.daddr, udp_packet_size, 0x11, sum);
            }

            /* Update headers into the buffer */
            memcpy(bd_hdr, hdr, sizeof(hdr->eth_header));
            memcpy(bd_hdr + sizeof(hdr->eth_header), &hdr->ip_header, sizeof(hdr->ip_header) + udp_rtp_headers_size);
            PAL_CPPI4_CACHE_WRITEBACK(bd_hdr, sizeof(hdr->eth_header) + sizeof(hdr->ip_header) + udp_rtp_headers_size);
    
            /* set descriptors */
            priv->encap_out_streams[outstream_idx].encap_bd->descInfo     = (priv->encap_out_streams[outstream_idx].encap_bd->descInfo & ~(CPPI41_EM_DESCINFO_PKTLEN_MASK));  
            priv->encap_out_streams[outstream_idx].encap_bd->descInfo     = priv->encap_out_streams[outstream_idx].encap_bd->descInfo | 
                ((num_of_frames*MPEG2TS_LEN + priv->encap_out_streams[outstream_idx].encap_frame_headers_size) & CPPI41_EM_DESCINFO_PKTLEN_MASK);
            priv->encap_out_streams[outstream_idx].encap_bd->pktInfo      = (priv->encap_out_streams[outstream_idx].encap_bd->pktInfo & ~(CPPI41_EM_PKTINFO_EOPIDX_MASK));
            priv->encap_out_streams[outstream_idx].encap_bd->pktInfo      = priv->encap_out_streams[outstream_idx].encap_bd->pktInfo | (num_of_frames<<CPPI41_EM_PKTINFO_EOPIDX_SHIFT);
            PAL_CPPI4_CACHE_WRITEBACK(priv->encap_out_streams[outstream_idx].encap_bd, MPEG_ENCAP_BD_SIZE);
            PAL_cppi4QueuePush(priv->txCpgmac_queue_hnd, (Uint32 *) PAL_CPPI4_VIRT_2_PHYS(priv->encap_out_streams[outstream_idx].encap_bd), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(MPEG_ENCAP_BD_SIZE), 0);                    
            priv->encap_out_streams[outstream_idx].acc_frames = 0;
			MPEG_ENCAP_INC_DBG_COUNT(priv->encap_out_streams[outstream_idx].output_packets_count);
        }
    }
    return 0;
}

/**************************************************************************/
/*! \fn static int mpegout_forward_frame(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd, Uint8 ch_idx, Uint32 outstream_idx, Uint32 pid_index)
 **************************************************************************
 *  \brief Forward a frame to encapsulating IP stream
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Cppi4EmbdDesc_ps* bd - frame BD
 *  \param[in]  Uint8 outstream_idx - IP stream index
 *  \return 0 or error code.
 **************************************************************************/
static int mpegout_forward_frame(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd)
{
    Cppi4ExtEmbdDesc *mpeg_bd;
    const Uint32 num_of_frames = 0;

    /* Allocate BD */
    mpeg_bd = (Cppi4ExtEmbdDesc *)(PAL_cppi4QueuePop(priv->txMpegEncap_free_queue_hnd));
    ++(priv->try2alloc);
    /* if no resources recycle buffer and descriptor */
    if (NULL==mpeg_bd)
    {
        ++(priv->outof_pd);
        return -1;
    }
    mpeg_bd = PAL_CPPI4_PHYS_2_VIRT(mpeg_bd);
    PAL_CPPI4_CACHE_INVALIDATE(mpeg_bd, MPEG_ENCAP_BD_SIZE);

    /* Fill in db */
    mpeg_bd->Buf[0].BufInfo = 
        CPPI41_EM_BUF_VALID_MASK | 
        (PAL_CPPI41_QUEUE_MGR_PARTITION_SR << CPPI41_EM_BUF_MGR_SHIFT) |
        (8<<CPPI41_EM_BUF_POOL_SHIFT);
    mpeg_bd->Buf[0] = bd->Buf[0];

    mpeg_bd->descInfo     = (mpeg_bd->descInfo & ~(CPPI41_EM_DESCINFO_PKTLEN_MASK));  
    mpeg_bd->descInfo     = mpeg_bd->descInfo | (MPEG2TS_LEN & CPPI41_EM_DESCINFO_PKTLEN_MASK);
    mpeg_bd->pktInfo      = (mpeg_bd->pktInfo & ~(CPPI41_EM_PKTINFO_EOPIDX_MASK));
    mpeg_bd->pktInfo      = mpeg_bd->pktInfo | (num_of_frames<<CPPI41_EM_PKTINFO_EOPIDX_SHIFT);
    PAL_CPPI4_CACHE_WRITEBACK(mpeg_bd, MPEG_ENCAP_BD_SIZE);
    PAL_cppi4QueuePush(priv->txMpegOut_queue_hnd, (Uint32 *) PAL_CPPI4_VIRT_2_PHYS(mpeg_bd), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(MPEG_ENCAP_BD_SIZE), 0);                    
    return 0;
}

/**************************************************************************/
/*! \fn static int send_nl_msg(mpeg_process_private *priv, Cppi4EmbdDesc_ps *bd, mpeg_header_t *mpeg_headers)
 **************************************************************************
 *  \brief Send a message to NL socket
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Cppi4EmbdDesc_ps* bd - frame BD
 *  \param[in]  mpeg_header_t *mpeg_headers - mpeg header
 *  \return 0 or error code.
 **************************************************************************/
static int send_nl_msg(mpeg_process_private *priv, Cppi4EmbdDesc_ps *bd, mpeg_header_t *mpeg_headers)
{
    struct sk_buff *skb = NULL;
    struct nlmsghdr *nlh;
    unsigned char *b;
    SoCMPEGEncapSectionFrame *in_frame;

    skb = alloc_skb(MPEG_SECTION_MAX_PAYLOAD, GFP_KERNEL);
    if (NULL==skb)
        return -1;

    b = skb->tail;
    nlh = NLMSG_PUT(skb, 0, priv->mpeg_nl_counter++, 0, MPEG_SECTION_MAX_PAYLOAD);
    nlh->nlmsg_flags = 0;

    in_frame = (SoCMPEGEncapSectionFrame*)NLMSG_DATA(nlh);
    in_frame->in_port = HAL_CHANNEL_ID_2_PORT_INDEX( (bd->tagInfo & CPPI41_EM_TAGINFO_SRCSUBCHN_MASK)>>CPPI41_EM_TAGINFO_SRCSUBCHN_SHIFT );
    in_frame->timestamp = priv->mpeg_nl_counter;
    memcpy(&in_frame->frame_payload[0], &(mpeg_headers->basic_mpeg_header), MPEG2TS_LEN);

    nlh->nlmsg_len = skb->tail - b;

    NETLINK_CB(skb).dst_group = SECTION_SOCKET_GROUP;
    return netlink_broadcast(priv->mpeg_nl_sk, skb, 0, SECTION_SOCKET_GROUP, GFP_KERNEL);

nlmsg_failure:
    kfree_skb(skb);
    return -1;
}


/**************************************************************************/
/*! \fn static void mpegEncapFastForward(mpeg_process_private* priv, Uint8 chn_idx, Uint16 pid, Cppi4EmbdDesc_ps* bd)
 **************************************************************************
 *  \brief Fast forward a frame to all associated IP streams
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Uint8 ch_idx - channel index
 *  \param[in]  Uint32 pid - frame PID 
 *  \param[in]  Cppi4EmbdDesc_ps* bd - frame BD
 *  \return none.
 **************************************************************************/
static void mpegEncapFastForward(mpeg_process_private* priv, Uint8 chn_idx, Uint16 pid, Cppi4EmbdDesc_ps* bd)
{
    Uint32 outstream_idx=0;
    Uint32 active_pid_idx = GET_ACTIVE_PID_INDEX(priv->pid_filters[chn_idx][pid].filter);
    Uint32 num_multiplexed=0;

    /*if there are associated IP streams*/
    if (active_pid_idx != INVALID_ACTIVE_PID_IDX)
    {
        MPEG_ENCAP_INC_DBG_COUNT(priv->encap_pid_params[chn_idx][active_pid_idx].input_counter);

        if (priv->encap_pid_params[chn_idx][active_pid_idx].pid_map_operation != MPEG_OUT_STREAM_OFF)
        {
            duplicate_rx_frame_buffer_nocopy(priv, bd); /*increment referrence counter*/

            /* This frame is MPEG out stream */
            if (priv->encap_pid_params[chn_idx][active_pid_idx].pid_map_operation == MPEG_OUT_STREAM_ON_PID_MAP)
            {
                /* We need to change the frame PID with new PID value. */
                mpeg_header_t *mpeg_headers;
                
                mpeg_headers = (mpeg_header_t *)PAL_CPPI4_PHYS_2_VIRT(bd->Buf[0].BufPtr);
                MPEG_SET_PID(mpeg_headers->basic_mpeg_header,priv->encap_pid_params[chn_idx][active_pid_idx].pid_map_value);
                PAL_CPPI4_CACHE_WRITEBACK(mpeg_headers, sizeof(mpeg_header_t));
            }

            /* forward frame to MPEG out CPPI queue */
            if (0 != mpegout_forward_frame(priv, bd))
            {
                recycle_rx_frame_buffer(priv, bd);/*dec. referrence counter*/
            }
        }
        else
        {   /* multiplex the frame to associated IP streams */
            for (outstream_idx=0; outstream_idx <MAX_MPEG_ENCAP_OUT_STREAMS; outstream_idx++)
            {
                /*if this PID is associated with this IP stream*/
                if (priv->encap_pid_params[chn_idx][active_pid_idx].out_stream_map[outstream_idx])
                {
                    num_multiplexed++;
                    duplicate_rx_frame_buffer_nocopy(priv, bd); /*increment referrence counter*/

                    if (0 != ip_forward_frame(priv, bd, chn_idx, outstream_idx, active_pid_idx))
                    {
                        recycle_rx_frame_buffer(priv, bd);/*dec. referrence counter*/
                        MPEG_ENCAP_INC_DBG_COUNT(priv->encap_pid_params[chn_idx][active_pid_idx].failed_counter[outstream_idx]);
                    }
                    else
                    {
                        MPEG_ENCAP_INC_DBG_COUNT(priv->encap_pid_params[chn_idx][active_pid_idx].fwd_counter[outstream_idx]);
                    }
                }
                /*Don't continue if there are no more associated IP streams*/
                if (num_multiplexed == priv->encap_pid_params[chn_idx][active_pid_idx].num_assoc_outstreams)
                    break;
            }
        }
    }
}

/**************************************************************************/
/*! \fn static void mpegEncapHostForward(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd)
 **************************************************************************
 *  \brief Forward a frame to Host
 *  \param[in]  mpeg_process_private* priv - Mpeg Encap internal DB.
 *  \param[in]  Cppi4EmbdDesc_ps* bd - frame BD
 *  \return none.
 **************************************************************************/
static void mpegEncapHostForward(mpeg_process_private* priv, Cppi4EmbdDesc_ps* bd)
{
   PAL_cppi4QueuePush(priv->wqMpegEncap_queue_hnd, (Uint32 *) PAL_CPPI4_VIRT_2_PHYS(bd), PAL_CPPI4_DESCSIZE_2_QMGRSIZE(MPEG_BD_SIZE), 0);            
   schedule_work(&(priv->mpeg_encap_wq));
   priv->host_forwarded++;
}

/**************************************************************************/
/*      INTERFACE  Functions                                              */
/**************************************************************************/

/**************************************************************************/
/*! \fn irqreturn_t mpegEncapRootIsr(int irq, void *context, struct pt_regs *regs)
 **************************************************************************
 *  \brief Mpeg Encap Interrupt hanlder
 **************************************************************************/
irqreturn_t mpegEncapRootIsr(int irq, void *context)
{   
    mpeg_process_private* priv = ((mpeg_process_private*)context);
    Cppi4EmbdDesc_ps* bd;
    Int32 i;
    mpeg_header_t *mpeg_headers;
    Uint16 pid;
    Uint8 chn_idx;
    Uint32 host2tge_ps;
    
    priv->irq_counter++;

    /* process high priority packets first */    
    for (i=0; i<1; ++i)
    {            
        while(avalanche_intd_get_interrupt_count(MPEG_INTD_HOST_NUM, PAL_CPPI41_MPEG_ACC_RX_CH_NUM))
        {
            while((bd = (Cppi4EmbdDesc_ps*)((unsigned long)*priv->rxAcc_chan_list[i] & QMGR_QUEUE_N_REG_D_DESC_ADDR_MASK)))
            {
                bd = PAL_CPPI4_PHYS_2_VIRT(bd);
                PAL_CPPI4_CACHE_INVALIDATE(bd, MPEG_BD_SIZE); 
                
                if (HOST2TGW_PS_IS_VALID(bd->PS[1])) /* Host2Tgw Protocol-Specific (if bit-32 is 1 this is an Host2Tgw data) */
                {
                    host2tge_ps = bd->PS[1];

                    bd->PS[1] = 0; /* delete the Host2Tgw Protocol-Specific */
                    PAL_CPPI4_CACHE_WRITEBACK(bd, MPEG_BD_SIZE);

                    /* Host2Tgw data - Frame go directly to stream output no filtering and classification is needed... */
                    if (HOST2TGW_PS_IS_MPEG_OUT(host2tge_ps))
                    {
                        duplicate_rx_frame_buffer_nocopy(priv, bd); /*increment referrence counter*/
                        /* forward frame to MPEG out CPPI queue (currently Host data is dest to MPEG out only) */
                        if (0 != mpegout_forward_frame(priv, bd))
                        {
                            recycle_rx_frame_buffer(priv, bd);/*dec. referrence counter*/
                        }
                        else
                        {  
                            recycle_rx_frame_buffer(priv, bd);
                            recycle_rx_frame_bd(priv, bd);
                        }
                    }
                    else
                    {
                        /* TBD :: forward frame to IP stream index - STILL NOT SUPPORTED  */
                        /* Uint32 ip_stream_id = HOST2TGW_PS_GET_IP_STREAM_IDX(host2tge_ps); */
                        recycle_rx_frame_buffer(priv, bd);
                        recycle_rx_frame_bd(priv, bd);
                    }
                        
                }
                else
                {
                    /* Filter the PID */
                    mpeg_headers = (mpeg_header_t *)PAL_CPPI4_PHYS_2_VIRT(bd->Buf[0].BufPtr);
                    PAL_CPPI4_CACHE_INVALIDATE(mpeg_headers, sizeof(mpeg_header_t));
    
                    chn_idx = (bd->tagInfo & CPPI41_EM_TAGINFO_SRCSUBCHN_MASK)>>CPPI41_EM_TAGINFO_SRCSUBCHN_SHIFT;
                    pid = MPEG_GET_PID(mpeg_headers->basic_mpeg_header); 

                    /* PID activity was detected - Mark this PID activity to ON */
                    SET_PID_ACTIVITY_ON(priv->pid_filters[chn_idx][pid].filter);

                    switch (GET_PID_OPERATION(priv->pid_filters[chn_idx][pid].filter))
                    {
                        case MPEG_ENCAP_PID_DISCARD:
                            /* recycle buffer */                                    
                            recycle_rx_frame_buffer(priv, bd);
                            recycle_rx_frame_bd(priv, bd);
                            break;
                        case MPEG_ENCAP_PID_FF:
                            mpegEncapFastForward(priv, chn_idx, pid, bd);
                            recycle_rx_frame_buffer(priv, bd);
                            recycle_rx_frame_bd(priv, bd);
                            break;
                        case MPEG_ENCAP_PID_HF:
                            mpegEncapHostForward(priv,bd);
                            /*the BD and the buffer are recycled during Host Fwd*/
                            break;
                        case MPEG_ENCAP_PID_FF_HF:
                            mpegEncapFastForward(priv, chn_idx, pid, bd);
                            mpegEncapHostForward(priv,bd);
                            /*the BD and the buffer are recycled during Host Fwd*/
                            break;
                        default:
                            /* recycle buffer */                                    
                            recycle_rx_frame_buffer(priv, bd);
                            recycle_rx_frame_bd(priv, bd);
                            break;
                    }
                }
                priv->frames_processed++;                                    
                priv->rxAcc_chan_list[i]++;                                    
            }   

            /* Update the list entry for next time */        
            priv->rxAcc_chan_list[i] = PAL_cppi4AccChGetNextList(priv->rxAcc_chan_hnd[i]);        
            avalanche_intd_set_interrupt_count (MPEG_INTD_HOST_NUM, PAL_CPPI41_MPEG_ACC_RX_CH_NUM, 1);                
        }
    }

    avalanche_intd_write_eoi (PAL_CPPI41_MPEG_ACC_RX_INTV_NUM);            
    return IRQ_RETVAL(1);
}

/**************************************************************************/
/*! \fn void mpegEncapSectionWorkQueue(void *data)
 **************************************************************************
 *  \brief Put a data into the Work Queue
 *  \param[in]  void *data - data buffer
 *  \return none.
 **************************************************************************/
void mpegEncapSectionWorkQueue(struct work_struct *work)
{
    mpeg_process_private        *priv = container_of(work, mpeg_process_private, mpeg_encap_wq);

    Cppi4EmbdDesc_ps            *bd;
    mpeg_header_t               *mpeg_headers;

    while ( NULL!=(bd = (Cppi4EmbdDesc_ps *)(PAL_cppi4QueuePop(priv->wqMpegEncap_queue_hnd))) )
    {
        bd = PAL_CPPI4_PHYS_2_VIRT(bd);
        PAL_CPPI4_CACHE_INVALIDATE(bd, MPEG_BD_SIZE);

        mpeg_headers = (mpeg_header_t *)PAL_CPPI4_PHYS_2_VIRT(bd->Buf[0].BufPtr);
        PAL_CPPI4_CACHE_INVALIDATE(mpeg_headers, MPEG2TS_LEN);

        send_nl_msg(priv, bd, mpeg_headers);

        /* recycle buffer */                                    
        recycle_rx_frame_buffer(priv, bd);
        recycle_rx_frame_bd(priv, bd);
    }
}
