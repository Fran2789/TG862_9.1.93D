/*
 *
 * dpp_tunnel_ctl.h
 * Description:
 * DOCSIS Packet Processor tunnel control header file
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

#ifndef _DPP_TUNNEL_CTL_H_
#define _DPP_TUNNEL_CTL_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/
#define DPP_TUNNEL_HEADER_MAX_LEN           128
#define DPP_TUNNEL_IOCTL_MAGIC			    'f'
#define DPP_TUNNEL_IOCTL_MAX_NUM            3

/************************** Main Ioctl commands *************************/

#define DPP_TUNNEL_SET_TUNNEL_MODE  _IOW(DPP_TUNNEL_IOCTL_MAGIC, 0, DppTunnelIoctlData_t)
#define DPP_TUNNEL_SET_CM_ADDRESS   _IOW(DPP_TUNNEL_IOCTL_MAGIC, 1, DppTunnelIoctlData_t)
#define DPP_TUNNEL_CREATE_TUNNEL    _IOW(DPP_TUNNEL_IOCTL_MAGIC, 2, DppTunnelIoctlData_t)
#define DPP_TUNNEL_DELETE_TUNNEL    _IOW(DPP_TUNNEL_IOCTL_MAGIC, 3, DppTunnelIoctlData_t)

/*! \var typedef struct DppTunnelIoctlData DppTunnelIoctlData_t
    \brief Structure defines the format of the the Packet Processor tunnel ioctl data structure. 
*/
typedef struct DppTunnelIoctlData
{
	int buf_len;		/* buffer length */
	void  *buf;	/* data buffer   */

}DppTunnelIoctlData_t;


typedef struct DppTunnelCreateTunnel
{
    Char   tunnelHeader[DPP_TUNNEL_HEADER_MAX_LEN];
    Uint8  tunnelHeaderLen;
    Uint8  l2L3HeaderLen;
    Uint8  tunnelType;
    Uint8  udpMode;
} DppTunnelCreateTunnel_t;




/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DppTunnelCtl_Init(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor Tunnel Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DppTunnelCtl_Init(void);


/**************************************************************************/
/*! \fn int DppTunnelCtl_Exit(void)
 **************************************************************************
 *  \brief DOCSIS Packet Processor Tunnel Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DppTunnelCtl_Exit(void);

#endif
