/*
 *
 * mpeg_encap_driver.h
 * Description:
 * MPEG Encapsulation configuration header file
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


#ifndef _HAL_MPEG_ENCAP_DRIVER_H_
#define _HAL_MPEG_ENCAP_DRIVER_H_

#include <linux/if_ether.h>
#include <asm/ioctl.h>
#include "sys_ptypes.h"
#include "soc_modules.h"

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/
#define SOC_MPEG_ENCAP_DRIVER_DEV_NAME  SOC_INTERFACE_DRIVER_DEV_NAME
/*****************************************/
/* Definitions for MPEG-Encap SoC Unit     */
/*****************************************/


#define PID_SPACE               (1024*8) // 2^13  
#define MPEG2TS_LEN          	  188
#define MAX_MPEG_ENCAP_OUT_STREAMS 12

typedef enum
{
    MPEG_ENCAP_CFG_DEL  = 0,
    MPEG_ENCAP_CFG_ADD  = 1,
} cfgAction_e;


typedef enum
{
    MPEG_ENCAP_PID_DISCARD  = 0,
    MPEG_ENCAP_PID_FF       = 1,
    MPEG_ENCAP_PID_HF       = 2,
    MPEG_ENCAP_PID_FF_HF    = 3,
	MPEG_NUM_PID_FWD_OP		= 4, /*total number of forwarding options*/
} pidOperation_e;

typedef enum
{
    MPEG_ENCAP_PID_FRAME_NOP    = 0,
    MPEG_ENCAP_PID_FRAME_MODIFY = 1,
} pidFrameOperation_e;

typedef enum 
{
    MPEG_ENCAP_UDP_HEADER = 0x00,
    MPEG_ENCAP_RTP_HEADER = 0x01
} encapHeaderType_e;

typedef enum 
{
    MPEG_OUT_STREAM_OFF        = 0x00,
    MPEG_OUT_STREAM_ON         = 0x01,
    MPEG_OUT_STREAM_ON_PID_MAP = 0x02
} mpegOutOperationsType_e;

typedef enum 
{
    MPEG_ENCAP_DISABLE_UDP_CSUM = 0x00,
    MPEG_ENCAP_ENABLE_UDP_CSUM  = 0x01
} udpCsum_e;

typedef enum
{
    MPEG_ENCAP_RTP_TS_DISABLE   = 0x00,
    MPEG_ENCAP_RTP_TS_DLNA      = 0x01,
    MPEG_ENCAP_RTP_TS_LOCAL     = 0x02
} rtpTsProperties_e;

typedef struct {
    Uint32              out_stream_idx;
	 cfgAction_e			action;
    encapHeaderType_e   encap_type;
    udpCsum_e           udp_csum;
    Uint8               mac_src_addr[ETH_ALEN];
    Uint8               mac_dst_addr[ETH_ALEN];
    Uint32              ip4_src_addr;
    Uint32              ip4_dst_addr;
    Uint32              udp_src_port;
    Uint32              udp_dst_port;
    Uint32              rtp_ssrc;
} SoCMPEGEncapHeaderInfo_t;

typedef struct {
    Uint32               port;
    Uint16               pid;
	Uint8				 fwd_op;
} SoCMPEGEncapPidOp_t;

typedef struct {
    Uint8               frame_offset;
    Uint8               new_frame_length;
    Uint8               new_frame[MPEG2TS_LEN];
} SoCMPEGEncapFrameModify_t;

typedef struct {
    Uint32                    port;
    Uint32                    outstream_idx;
    mpegOutOperationsType_e   mpegOutOperation;
    Uint16                    pidMapValue;
    Uint16                    pid;
    cfgAction_e               action; 
    Uint8                     outstream_map[MAX_MPEG_ENCAP_OUT_STREAMS];
    SoCMPEGEncapFrameModify_t frame_mod;
} SoCMPEGEncapPidOutStream_t;

typedef struct {
    Uint32              outstream_idx;
    Uint16              pcr_pid;
    rtpTsProperties_e   rtp_ts;
} SoCMPEGEncapRtpTsInfo_t;

typedef struct {
    Uint32              in_port;
    Uint32              timestamp;
    Uint32              host2tgw_ps; /* Host to Tgw 32 bit protocol-specific */
    Uint8               frame_payload[MPEG2TS_LEN];
} SoCMPEGEncapSectionFrame;

typedef struct {
    Uint32      ds_port;                         /* The DS port          */
    Uint8       is_activityDetected[PID_SPACE];  /* The index is the PID index */
} SoCMPEGEncapPidActivity_t;


/* Host to Tgw 32 bit protocol-specific:
    bit  31  valid bit          - If set to 1 this is host 2 tgw frame
    bits 0-7 IP stream index    - The index of the output ip stream.
    bit  8   Mpeg out valid bit - If set to 1 this is Mpeg-Out stream.
*/
#define HOST2TGW_PS_SET_VALID(ps)               ( (ps) = (ps) | (0x80000000) ) /* bit 31 set to 1                  */
#define HOST2TGW_PS_IS_VALID(ps)                ( (ps) )                       /* a valid host2tgw PS is not zero  */
#define HOST2TGW_PS_IS_MPEG_OUT(ps)             ( (ps) & (0x100) )             /* if bit 8 is set (1) return true  */ 
#define HOST2TGW_PS_SET_MPEG_OUT(ps)            ( (ps) = (ps) | (0x100) )      /* if bit 8 is set (1) return true  */
#define HOST2TGW_PS_GET_IP_STREAM_IDX(ps)       ( (ps) & (0xFF) )              /* bits 0-7 is the IP-Stream index  */ 
#define HOST2TGW_PS_SET_IP_STREAM_IDX(ps,idx)   ( (ps) = ((ps) & (~0xFF)) | (0xFF&idx) ) /* bits 0-7 is the IP-Stream index  */ 

#define MPEG_SECTION_MAX_PAYLOAD    (sizeof(SoCMPEGEncapSectionFrame))  
#define SECTION_SOCKET_GROUP        1


/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

/* IOCTL commands:

   If you are adding new ioctl's to the kernel, you should use the _IO
   macros defined in <linux/ioctl.h> _IO macros are used to create ioctl numbers:

	_IO(type, nr)         - an ioctl with no parameter. 
   _IOW(type, nr, size)  - an ioctl with write parameters (copy_from_user), kernel would actually read data from user space 
   _IOR(type, nr, size)  - an ioctl with read parameters (copy_to_user), kernel would actually write data to user space 
   _IOWR(type, nr, size) - an ioctl with both write and read parameters 

   'Write' and 'read' are from the user's point of view, just like the
	system calls 'write' and 'read'.  For example, a SET_FOO ioctl would
	be _IOW, although the kernel would actually read data from user space;
	a GET_FOO ioctl would be _IOR, although the kernel would actually write
	data to user space.

	The first argument to _IO, _IOW, _IOR, or _IOWR is an identifying letter
	or number from the SoC_ModuleIds_e enum located in this file. 

	The second argument to _IO, _IOW, _IOR, or _IOWR is a sequence number
	to distinguish ioctls from each other.  

   The third argument to _IOW, _IOR, or _IOWR is the type of the data going
   into the kernel or coming out of the kernel (e.g.  'int' or 'struct foo').

   NOTE!  Do NOT use sizeof(arg) as the third argument as this results in 
   your ioctl thinking it passes an argument of type size_t.

*/

#define	SOC_MPEGE_UNIT_ENABLE               _IO   (SOC_MPEG_ENCAP_MODULE_ID, 1)	
#define	SOC_MPEGE_UNIT_DISABLE              _IO   (SOC_MPEG_ENCAP_MODULE_ID, 2)	
#define	SOC_MPEGE_PORT_ENCAP_OUTSTREAM      _IOWR (SOC_MPEG_ENCAP_MODULE_ID, 3, SoCMPEGEncapHeaderInfo_t)	
#define  SOC_MPEGE_PORT_SET_RTP              _IOW  (SOC_MPEG_ENCAP_MODULE_ID, 4, SoCMPEGEncapRtpTsInfo_t)
#define	SOC_MPEGE_PORT_PID_DISCARD          _IOW  (SOC_MPEG_ENCAP_MODULE_ID, 5, Uint32)
#define	SOC_MPEGE_PORT_PID_FF               _IOW  (SOC_MPEG_ENCAP_MODULE_ID, 6, Uint32)
#define	SOC_MPEGE_PORT_PID_HF               _IOW  (SOC_MPEG_ENCAP_MODULE_ID, 7, Uint32)
#define	SOC_MPEGE_PORT_PID_FF_HF            _IOW  (SOC_MPEG_ENCAP_MODULE_ID, 8, Uint32)
#define  SOC_MPEGE_PORT_PID_FRAME_MODIFY     _IOW  (SOC_MPEG_ENCAP_MODULE_ID, 9, SoCMPEGEncapPidOutStream_t)
#define  SOC_MPEGE_PORT_ASSOC_PID_OUTSTREAM  _IOW  (SOC_MPEG_ENCAP_MODULE_ID, 10, SoCMPEGEncapPidOutStream_t)
#define	SOC_MPEGE_PORT_GET_PID_FWD_OP       _IOR  (SOC_MPEG_ENCAP_MODULE_ID, 11, SoCMPEGEncapPidOp_t)
#define	SOC_MPEGE_PORT_GET_PID_OUTSTREAMS   _IOR  (SOC_MPEG_ENCAP_MODULE_ID, 12, SoCMPEGEncapPidOutStream_t)
#define	SOC_MPEGE_PORT_GET_PID_ACTIVITY     _IOWR (SOC_MPEG_ENCAP_MODULE_ID, 13, SoCMPEGEncapPidActivity_t)

/* SOC_MPEGO_IOCTL_MAXNR: In case of adding new MPEG IOCTL commands this number must be update */
#define SOC_MPEGE_IOCTL_MAXNR       13

/**************************************************************************/
/*! \fn void mpegOutRegisterSoCModule (SoCDriverOperations_t **mpeg_out_operations)
 **************************************************************************
 *  \brief Register the MPEG-Out driver function pointers object.
 *  \param[in] **mpeg_out_operations - pointer to MPEG-Out .
 *  \return none.
 **************************************************************************/
#ifdef __KERNEL__

void mpegEncapRegisterSoCModule (SoCDriverOperations_t **mpeg_out_operations_out);

Int32 mpegEncapUnitEnable( void );
Int32 mpegEncapUnitDisable( void );

Int32 mpegEncapAddOutStream( SoCMPEGEncapHeaderInfo_t *mpegEncapHeaderInfo);
Int32 mpegEncapRemOutStream( SoCMPEGEncapHeaderInfo_t *mpegEncapHeaderInfo );
Int32 mpegEncapPortSetRtpTs( SoCMPEGEncapRtpTsInfo_t *mpegEncapRtpTs );
Int32 mpegEncapPortSetPidOp( Uint8 port, Uint16 pid, pidOperation_e pid_operation );
Int32 mpegEncapPortFrameModify( SoCMPEGEncapPidOutStream_t *mpegEncapFrameModify );
Int32 mpegEncapAddPidToOutStream( SoCMPEGEncapPidOutStream_t *pidEncap );	
Int32 mpegEncapRemPidFromOutStream( SoCMPEGEncapPidOutStream_t *pidEncap );
Int32 mpegEncapPortGetPidOp( Uint8 port, Uint16 pid, Uint8 *pid_op);
Int32 mpegEncapGetPidAssocOutStreams( SoCMPEGEncapPidOutStream_t *pidEncap );
Int32 mpegEncapFrameModDelPidOutStreamRule(Uint32 chn_idx, Uint32 active_pid_idx, Uint32 outstream_idx);

#endif /* __KERNEL__ */

#endif  /* _HAL_MPEG_SOC_DRIVER_H_ */

