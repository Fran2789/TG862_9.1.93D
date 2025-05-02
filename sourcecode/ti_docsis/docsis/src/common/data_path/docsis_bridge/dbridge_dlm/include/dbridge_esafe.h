/*
 *
 * dbridge_esafe.h
 * Description:
 * The DOCSIS bridge E-Safe header file
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

#ifndef _DBRIDGE_ESAFE_H_
#define _DBRIDGE_ESAFE_H_


/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <linux/netdevice.h>
/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/



/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/


/*! \var typedef struct DbridgeEsafeDevice_t
    \brief 
*/

#define DBR_ESAFE_NULL				0x0	 
#define DBR_ESAFE_EMTA	            0x1 
#define DBR_ESAFE_EPS_EROUTER	    0x2 
#define DBR_ESAFE_ESTB_IP           0x4
#define DBR_ESAFE_ESTB_DSG	        0x8 
#define DBR_ESAFE_ETEA	            0x10 


typedef struct DbridgeEsafeDevice
{    
	struct net_device*  ptr_esafeInterface; 
	struct net_device*  ptr_targetInterface; 
	struct DbridgeDevice*    dbridgeDevice;
	unsigned int        eSafeType;
	int     (*push)(struct sk_buff* skb);
}DbridgeEsafeDevice_t;





/**************************************************************************/
/*      Default Values                                          */
/**************************************************************************/



/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */
/**************************************************************************/

/*! \fn int DbridgeEsafeDb_Init(void)                                     
 *  \brief DOCSIS bridge esafe initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeEsafe_Init(void);

/*! \fn DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberByindex(unsigned int index)
 *  \brief get meber from esafe table corresponding to target dev 
 *  \param[in] index.
 *  \return ptr  to esafe device table
 */
DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberByindex(unsigned int index);

/*! \fn int DbridgeEsafe_GetNumOfEsaf(void)
 *  \brief get the number of members in esafe dev table 
 *  \param[in] index.
 *  \return ptr  to esafe device table
 */

/*! \fn DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromEsafeDev(struct net_device* ptr_interface)
 *  \brief get meber from esafe table corresponding to esafe dev 
 *  \param[in] pointer to esafe dev.
 *  \return ptr  to esafe device table
 */
DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromEsafeDev(struct net_device* ptr_interface);


#ifdef DSG
DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromEsafeType(unsigned int eSafe_dev_type);
#endif
int DbridgeEsafe_GetNumOfEsaf(void);

/*! \fn int  DBridgeEsafe_DeviceXmit(struct sk_buff *skb, struct net_device *dev)
 *  \brief The function is the entry point by which packets from esafe are   
 *  \brief process and pushed into the DOCSIS Bridge. 
 *  \brief NOTE: Before calling this function ensure that the MAC RAW Pointer in the 
 *  \brief SKB is valid and points to the start of the MAC header.
 *  \param[in] skb.
 *  \param[in] dev.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int DbridgeEsafe_DeviceXmit(struct sk_buff *skb, struct net_device *dev);

/*! \fn int DbridgeEsafe_AddNetDev(struct net_device* ptr_net_dev,
 *                                  unsigned int esafe_type,int (*push)(struct sk_buff* skb))                                     
 *  \brief connect target device interface to esafe (EMTA,EPS,ESTB_IP,ESTB_DSG,ETEA)
 *  \param[in] ptr_net_dev - pointer to target  device interface.
 *  \param[in] esafe_type - EMTA -1, EPS -2, ESTB_IP-3, ESTB_DSG-4 ,ETEA-5.
 *  \param[in] push - push function to send packet from esafe to target device interface.
 *  \return OK=0 or error status.
 */
int DbridgeEsafe_AddNetDev(struct net_device* ptr_net_dev,unsigned int esafe_type,int (*push)(struct sk_buff* skb));

/*! \fn DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromTargetDev(struct net_device* ptr_interface)
 *  \brief get meber from esafe table corresponding to target dev 
 *  \param[in] pointer to esafe dev.
 *  \return ptr  to esafe device table
 */
DbridgeEsafeDevice_t* DbridgeEsafe_GetMemberFromTargetDev(struct net_device* ptr_interface);


/*! \fn DbridgeDevice* DbridgeEsafe_GetDbridgeDeviceByeSafeType(unsigned int eSafeType);
 *  \brief get Dbridge Device table member from esafe table corresponding to eSafe type
 *  \param[in] eSafe type.
 *  \return ptr to Dbridge Device table
 */
struct DbridgeDevice* DbridgeEsafe_GetDbridgeDeviceByeSafeType(unsigned int eSafeType); 


/**************************************************************************/
/*! \fn int DbridgeEsafe_PrintEsafeCfg(char *page)
 **************************************************************************
 *  \brief Prepare print buffer with esafe config
 *  \param[in] page - pointer to output buffer
 *  \return length of buffer
 **************************************************************************/
int DbridgeEsafe_PrintEsafeCfg(char *page);

/* ARRIS ADD */
int DbridgeEsafe_GetEsafeTypeFromEsafeDev(struct net_device* ptr_interface);
/* ARRIS ADD END */

#endif

