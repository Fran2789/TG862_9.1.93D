/*
 *
 * dbridge_l2vpn_ds_ctl.h
 * Description:
 * DOCSIS Bridge L2VPN DS Forwarding control header file
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

#ifndef _DBRIDGE_L2VPN_DS_CTL_H_
#define _DBRIDGE_L2VPN_DS_CTL_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <_tistdtypes.h>

/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

#define DBR_L2VPN_DS_IOCTL_MAGIC			'f'
#define DBR_L2VPN_DS_IOCTL_MAX_NUM          (ENUM_L2VPN_DS_IOCTL_NUM_OPS)

/*! \var typedef struct DbridgeL2vpnDsIoctlData DbridgeL2vpnDsIoctlData_t
    \brief Structure defines the format of the the Multicast DSID Forwarding ioctl data structure. 
*/
typedef struct DbridgeL2vpnDsIoctlData
{
	int subtype;		/* ioctl command subtype */
	int buf_len;		/* buffer length */
	Uint32 param1;		/* additional interger parameter*/
	void   *buf;	     /* data buffer   */

}DbridgeL2vpnDsIoctlData_t;


/************************** Main Ioctl commands *************************/

/*! \var typedef enum DbridgeL2vpnDsIoctlOerations_e
    \brief Enumerate Dbridge L2VPN DS ioctl-s
*/
typedef enum
{
    ENUM_L2VPN_DS_IOCTL_SET_SAID,
    ENUM_L2VPN_DS_IOCTL_DEL_SAID,
    ENUM_L2VPN_DS_IOCTL_SET_DUT,
    ENUM_L2VPN_DS_IOCTL_PRINT_L2VPN_DS_DB,
    ENUM_L2VPN_DS_IOCTL_NUM_OPS
} DbridgeL2vpnDsIoctlOperations_e;

#define L2VPN_DS_IOCTL_SET_SAID          _IOW(DBR_L2VPN_DS_IOCTL_MAGIC, ENUM_L2VPN_DS_IOCTL_SET_SAID, DbridgeL2vpnDsIoctlData_t)
#define L2VPN_DS_IOCTL_DEL_SAID          _IOW(DBR_L2VPN_DS_IOCTL_MAGIC, ENUM_L2VPN_DS_IOCTL_DEL_SAID, DbridgeL2vpnDsIoctlData_t)
#define L2VPN_DS_IOCTL_SET_DUT           _IOW(DBR_L2VPN_DS_IOCTL_MAGIC, ENUM_L2VPN_DS_IOCTL_SET_DUT, DbridgeL2vpnDsIoctlData_t)
#define L2VPN_DS_IOCTL_PRINT_L2VPN_DS_DB _IOW(DBR_L2VPN_DS_IOCTL_MAGIC, ENUM_L2VPN_DS_IOCTL_PRINT_L2VPN_DS_DB, DbridgeL2vpnDsIoctlData_t)



/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/**************************************************************************/
/*! \fn int DbridgeL2vpnDsCtl_Init(void)
 **************************************************************************
 *  \brief DOCSIS Bridge L2VPN DS Ctl initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DbridgeL2vpnDsCtl_Init(void);

/**************************************************************************/
/*! \fn int DbridgeL2vpnDsCtl_Exit(void)
 **************************************************************************
 *  \brief DOCSIS Bridge L2VPN DS Ctl exit.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0 or error code.
 **************************************************************************/
int DbridgeL2vpnDsCtl_Exit(void);

#endif
