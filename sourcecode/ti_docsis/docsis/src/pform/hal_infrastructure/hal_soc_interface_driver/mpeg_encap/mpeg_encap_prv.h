/*
 *
 * mpeg_encap_prv.h
 * Description:
 * MPEG Encapsulation private data header file
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
/***************************************************************************/

/*! \file hal_mpeg_process.h
 *  \brief HAL raw mpeg process (filtering and encapsulate) header file
****************************************************************************/


#ifndef _HAL_MPEG_ENCAP_PRV_H_
#define _HAL_MPEG_ENCAP_PRV_H_

//#include <puma5/puma5.h>
//#include <puma5/puma5_cppi.h>

/**************************************************************************/
/*      DEFINES                                                           */
/**************************************************************************/

#define CPPI4_BD_LENGTH_FOR_CACHE       32
#define MPEG_QM_EMB_DESC_SIZE_CODE      7 

#define MPEG_ACC_PAGE_NUM_ENTRY         (32)
#define MPEG_ACC_NUM_PAGE               (2)
#define MPEG_ACC_ENTRY_TYPE             (PAL_CPPI41_ACC_ENTRY_TYPE_D)

/* Byte size of page = number of entries per page * size of each entry */
#define MPEG_ACC_PAGE_BYTE_SZ           (MPEG_ACC_PAGE_NUM_ENTRY * ((MPEG_ACC_ENTRY_TYPE + 1) * sizeof(unsigned int*)))
/* byte size of list = byte size of page * number of pages * */
#define MPEG_ACC_LIST_BYTE_SZ           (MPEG_ACC_PAGE_BYTE_SZ * MPEG_ACC_NUM_PAGE)

#define MPEG_RXINT_NUM                  (MAP_INTD_TO_INTC(PAL_CPPI41_MPEG_ACC_RX_INTV_NUM)) 
#define MPEG_INTD_HOST_NUM              0

#ifdef MPEG_ENCAP_DEBUG
#define MAX_MPEG_CHNS                   2
#else
#define MAX_MPEG_CHNS                   8
#endif/*MPEG_ENCAP_DEBUG*/

#define MAX_MPEG_ACTIVE_PIDS			32 /* Maximum active PIDs on mpeg channel */
#define MAX_MPEG_ENCAP_OUT_STREAMS   	12 /* Maximum supported encapsulation IP streams */
#define MAX_CONCAT_FRAMES               7
#define MPEG2TS_LEN                     188

#define GET_MPEG_BD_PTR(base, num)              (((Uint32)base) + (((Uint32)num) * MPEG_BD_SIZE))
#define GET_MPEG_ENCAP_BD_PTR(base, num)        (((Uint32)base) + (((Uint32)num) * MPEG_ENCAP_BD_SIZE))

#define HAL_PORT_ID_2_CHANNEL_INDEX(x)  ((x)-1)
#define HAL_CHANNEL_ID_2_PORT_INDEX(x)  ((x)+1)


#define PID_OPERATION_OFFSET            0
#define PID_ACTIVITY_OFFSET             2
#define PID_ACTIVE_IDX_OFFSET           3

#define PID_OPERATION_MASK              0x03 /*0000 0011 - Supports 4 types of operations                        */
#define PID_ACTIVITY_MASK               0x04 /*0000 0100 - If zero - no activity detected for this PID (per DS). */
#define PID_ACTIVE_IDX_MASK             0xF8 /*1111 1000 - Supports up to 32 active parameters per PID per DS.   */


#define PID_OPERATION_FF_BIT            0x01
#define PID_OPERATION_HF_BIT            0x02

#define GET_PID_OPERATION(value)        (((value)&PID_OPERATION_MASK) >> PID_OPERATION_OFFSET)
#define GET_ACTIVE_PID_INDEX(value)     (((value)&PID_ACTIVE_IDX_MASK) >> PID_ACTIVE_IDX_OFFSET)

#define SET_PID_OPERATION(value, operation)   (value)=(((value)&(~PID_OPERATION_MASK))    | ((operation) << PID_OPERATION_OFFSET))
#define SET_ACTIVE_PID_INDEX(value, idx)      (value)=(((value)&(~PID_ACTIVE_IDX_MASK))   | ((idx) << PID_ACTIVE_IDX_OFFSET))

/* PID activity detection macros */
#define SET_PID_ACTIVITY_ON(value)                  (value)=( (value) | (PID_ACTIVITY_MASK) )
#define SET_PID_ACTIVITY_OFF(value)                 (value)=( (value) & (~PID_ACTIVITY_MASK) )
#define IS_PID_ACTIVITY_ON(value)                   ( (value) & (PID_ACTIVITY_MASK) )

/*Toggle Fast and Host Forward bit operations separately*/
#define SET_PID_OPERATION_FF(value, bit)            value=(((value)&(~PID_OPERATION_FF_BIT)) | ((bit)&PID_OPERATION_FF_BIT))
#define SET_PID_OPERATION_HF(value, bit)            value=(((value)&(~PID_OPERATION_HF_BIT)) | ((bit)&PID_OPERATION_HF_BIT))

/*Invalid index into active PIDs array*/
#define INVALID_ACTIVE_PID_IDX          0x1F    /* 0001 1111 */
#define MPEG_INVALID_PID                0xFFFF

/* MPEG header */
#define MPEG_SET_PID(header, pid)                   ( (header)=(((header)&(~(0x1FFF<<8))) | (((Uint32)(pid)&0x1FFF)<<8)))
#define MPEG_GET_PID(header)                        ( ((header) >> 8) & 0x0001FFF)
#define MPEG_GET_ADAPTATION_FIELD_CONTROL(header)   ( ((header) >> 4) & 0x0000003)
#define IS_ADAPTEATION_FIELD_EXIST(header)          ( MPEG_GET_ADAPTATION_FIELD_CONTROL(header)>>1 )
#define IS_PCR_EXIST(adaptation_field_flags)        ( (adaptation_field_flags) & 0x10 )


/************************************************************************/
/*     MPEG Encap driver private data                                   */
/************************************************************************/

typedef struct {
    Uint8   frame_offset;
    Uint8   new_frame_length;
    Uint8   new_frame[MPEG2TS_LEN];
} mpeg_modified_frame;

typedef struct {
    Uint8   filter;
} mpeg_pid_filter;

struct rtphdr {
    Uint8   ver:2,
            p:1,
            x:1,
            cc:4;
    Uint8   m:1,
            pt:7;
    Uint16  seq_num;
    Uint32  timestamp;
    Uint32  ssrc;
} __attribute__((packed)) ;

struct encaphdr {
    struct ethhdr   eth_header;
    Uint16 pad;
    struct iphdr    ip_header;
    struct udphdr   udp_header;
    struct rtphdr   rtp_header;
}; // __attribute__((packed));


typedef struct {
    Bool                    enabled;
    Uint8                   acc_frames;
    struct encaphdr         encap_header;
    Cppi4ExtEmbdDesc*       encap_bd;
    Uint8                   eth_dst_addr[ETH_ALEN];
    Uint16                  ip_id;
    Uint32                  udp_payload_csum;
    Uint16                  rtp_seq_num;
    Uint16                  pcr_pid;
    encapHeaderType_e       encap_header_type;
    udpCsum_e               udp_csum;
    Uint32                  encap_frame_headers_size;
    
    // For PCR calculation
    rtpTsProperties_e       rtp_ts_flag;
    Uint32                  rtp_timestamp;
    Uint32                  last_pcr_ts_at90K;
    Int32                   pcr2pcr_frames_counter;
    Uint32                  deltaN_pcr_factor;
    Uint32                  accumulated_deltaN_pcr_factor;
    Uint32                  old_sysclk;
    long long               old_90K_clk;
    Uint32                  old_90K_clk_32bits;
#ifdef MPEG_ENCAP_DEBUG
	Uint32					output_packets_count;
#endif
} encap_out_stream_t;

/* PID encapsulation parameters */
typedef struct
{
	Uint16               pid;
   /* IP stream output */
	Uint8                out_stream_map[MAX_MPEG_ENCAP_OUT_STREAMS];
	Uint32               num_assoc_outstreams;				 
	mpeg_modified_frame  **frame_manip;
   /* MPEG out stream output */
   Uint8                pid_map_operation;
   Uint16               pid_map_value;
#ifdef MPEG_ENCAP_DEBUG
	Uint32               input_counter;
	Uint32               fwd_counter[MAX_MPEG_ENCAP_OUT_STREAMS];
	Uint32               failed_counter[MAX_MPEG_ENCAP_OUT_STREAMS];
#endif
}mpeg_encap_pid_t; 

typedef struct {
    PAL_Handle              pal_hnd;                                    /* The handle to PAL layer */

    /* RX */
    PAL_Cppi4QueueHnd       rx_queue_hnd        [1]; /* Rx Proxy queue handles */
    PAL_Cppi4AccChHnd       rxAcc_chan_hnd      [1]; /* The Rx accumulator channel handle */
    PAL_Cppi4QueueHnd       rxAcc_queue         [1];
    struct Cppi4EmbdDesc**  rxAcc_chan_list     [1];  /* Rx acc channel lists */
    Ptr                     rxAcc_chan_list_base[1];  /* Rx acc channel lists base*/
    PAL_Cppi4AccChHnd       rxMpeg_chan_hnd;                            /* The Rx channel handle */
    PAL_Cppi4QueueHnd       rxMpeg_free_queue_hnd;                      /* Rx Cni free queue handles */
    Ptr                     rxMpeg_pdpool;                              /* Rx Cni bd pools */

    /* TX */
    PAL_Cppi4QueueHnd       txMpegEncap_free_queue_hnd;                 /* TX MPEG encapsulation free queue handles */
    Ptr                     txMpegEncap_pdpool;                         /* TX MPEG encapsulation bd pools */
    PAL_Cppi4QueueHnd       txCpgmac_queue_hnd;                         /* Tx CPGMAC high priority queue handle */
    PAL_Cppi4TxChHnd        txMpegOut_chn_hdl;                          /* MPEG-OUT TX channel handle */
    PAL_Cppi4QueueHnd       txMpegOut_queue_hnd;                        /* Tx MPEGOUT queue handle */

    /* statistics and filters */
    Uint32                  irq_counter;
    Uint32                  frames_processed;
    Uint32                  host_forwarded;
    Uint32                  try2alloc;
    Uint32                  outof_buf;
    Uint32                  outof_pd;

    /* channel PID parameters */
    mpeg_pid_filter         pid_filters[MAX_MPEG_CHNS][PID_SPACE]; 
    mpeg_encap_pid_t        encap_pid_params[MAX_MPEG_CHNS][MAX_MPEG_ACTIVE_PIDS];
	 /*encapsulation parameters*/
    encap_out_stream_t      encap_out_streams[MAX_MPEG_ENCAP_OUT_STREAMS];


    /* work queue */
    struct work_struct      mpeg_encap_wq;
    struct work_struct      mpeg_host2tgw_wq;
    PAL_Cppi4QueueHnd       wqMpegEncap_queue_hnd;                 /* Rx Cni free queue handles */
    struct sock             *mpeg_nl_sk;
    Uint32                  mpeg_nl_counter;

    /* Others */
    struct proc_dir_entry   *mpeg_proc;
} mpeg_process_private;

void mpegEncapUnitInit(mpeg_process_private* priv);
void mpegEncapUnitCleanup(mpeg_process_private* priv);
void mpegEncapSetPacketHeader(encap_out_stream_t *out_stream,
                              Uint8*            mac_src_addr, 
                              Uint8*            mac_dst_addr, 
                              Uint32            ip4_src_addr, 
                              Uint32            ip4_dst_addr, 
                              Uint16            udp_src_port,
                              Uint16            udp_dst_port,
                              encapHeaderType_e encap_type,
                              udpCsum_e         udp_csum,
                              Uint32            rtp_ssrc);
void mpegEncapSetRtpTsProperties(encap_out_stream_t     *out_stream,
                                 Uint16                pcr_pid,
                                 rtpTsProperties_e     rtp_ts);
irqreturn_t mpegEncapRootIsr(int irq, void * arg);
void mpegEncapSectionWorkQueue(struct work_struct *work);

#endif
