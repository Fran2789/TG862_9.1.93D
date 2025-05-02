/*
 *
 * dbridge_main.h
 * Description:
 * Declaration of DOCSIS bridge functions and types.
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

#ifndef _DBRIDGE_MAIN_H_
#define _DBRIDGE_MAIN_H_

 
/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <linux/netdevice.h>

/**************************************************************************/
/*  Various defines                                                       */
/**************************************************************************/

/* #define DBRIDGE_DBG */
#define DBRIDGE_LOG

#define DBRIDGE_DBG_TYPES \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_TX,            0x00000001) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_RECIVE,        0x00000002) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_RX_CMCI,       0x00000004) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_RX_LANIP,      0x00000008) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_RX_WANIP,      0x00000010) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_RX_CABLE,      0x00000020) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_TX_FLOOD,      0x00000040) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_TX_IF,         0x00000080) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_MCAST_CABLE,       0x00000100) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_IGMP,          0x00000200) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_PREREG_RX_CABLE,   0x00000400) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_CLASSIFIER,        0x00000800) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_FILTER,            0x00001000) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_MDF,               0x00002000) \
    DBRIDGE_DBG_TYPE(DBRIDGE_DBG_L2VPN_DS,          0x00004000) \
	DBRIDGE_DBG_TYPE(DBRIDGE_DBG_REG_RX_DSGI,       0x00008000) \



#define DBRIDGE_DBG_TYPE(__name, __mask) __name = __mask,
typedef enum
{
    DBRIDGE_DBG_TYPES
} DBR_DebugTypes;
#undef DBRIDGE_DBG_TYPE

#ifdef DSG
#define BROADCAST_MAC         { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
#endif

/* Protocol filter result */
typedef enum FilterAction_e
{
      DFLTR_DISCARD,
      DFLTR_ACCEPT,
      DFLTR_UNKNOWN,
}DFltrAction_t;

/* Filtered packet direction */
typedef enum FilterPktDirect_e
{
      DFLTR_FROM_CABLE,
      DFLTR_TO_CABLE,
#ifdef DSG
    DFLTR_TO_DSG,
#endif
}DFltrPktDirect_t;



/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/


/**************************************************************************/
/*      Default Values                                          */
/**************************************************************************/



/**************************************************************************/
/*      INTERFACE FUNCTIONS Prototypes:                                   */


/**************************************************************************/


/*! \fn STATUS DbridgeDb_Init(void)
 *  \brief DOCSIS bridge initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
int Dbridge_Init(void);

/*! \fn void DBridge_receive (struct sk_buff *skb)                                     
 *  \brief The function is the entry point by which packets are pushed into  
 *  \brief the DOCSIS Bridge. 
 *  \brief NOTE: Before calling this function ensure that the MAC RAW Pointer in the 
 *  \brief SKB is valid and points to the start of the MAC header.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 */
void DBridge_receive (struct sk_buff *skb);

/*! \fn void Dbridge_CableRecive (struct sk_buff *skb)                                     
 *  \brief  
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 */
void Dbridge_CableRecive (struct sk_buff *skb);

/*! \fn void Dbridge_SendIgmpPacket(struct sk_buff *skb, unsigned long device_map, unsigned long device_type)
 *  \brief   Send Mcasr packet, this function call from igmp module 
 *  \param[in] skb.
 *  \param[in] device_map, The devices that conect to the Mcast grope.
 *  \param[in] device_type, the docsis bridge dev type (cable or CPE)
 *  \param[out] none
 *  \return none
 */

void Dbridge_SendIgmpPacket(struct sk_buff *skb, unsigned long long device_map, unsigned long device_type);
/*! \fn int Dbridge_messageProcessed(char *mac_address, int msg_type)
 *  \brief   
 *  \param[out] mac_address.
 *  \param[msg_type] device_map, The devices that conect to the Mcast grope.
 *  \return 
 */
int Dbridge_messageProcessed(char *mac_address, int msg_type);

/*! \fn void dbridgeAddInFrames(struct sk_buff* skb);                                    
 *  \brief   increase input packet  
 *  \param[in] skb.
  *  \param[out] none
 *  \return NONE
 */
void dbridgeAddInFrames(struct sk_buff* skb);   

/*! \fn void dbridgeAddEsafeInFrames(struct sk_buff* skb);                                    
 *  \brief   increase input packet from Esafe.
 *  Note: In case of esafe we take the ifindex from dev instead of input_dev
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 */
void dbridgeAddEsafeInFrames(struct sk_buff* skb);

/*! \fn void dbridgeAddOutFrames(struct sk_buff* skb);                                    
 *  \brief   increase output packet  
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 */
void dbridgeAddOutFrames(struct sk_buff* skb);   

/*! \fn void dbridgeAddDiscards(struct sk_buff* skb);                                    
 *  \brief   increase discard  packet  
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 */
void dbridgeAddDiscards(struct sk_buff* skb);   

/*! \fn void dbridgeAddForwardDiscards(struct sk_buff* skb);                                    
 *  \brief   increase Forward Discards  packet  
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 */
void dbridgeAddForwardDiscards(struct sk_buff* skb);   

/*! \fn void dbridgeAddUnsupps(struct sk_buff* skb);                                    
 *  \brief   increase Unsupps  packet  
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 */
void dbridgeAddUnsupps(struct sk_buff* skb);

/*! \fn void dbridgeAddFilterDropPacket(struct sk_buff* skb);                                    
 *  \brief   increase filter drop packet    
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 */
void dbridgeAddFilterDropPacket(struct sk_buff* skb);

// ARRIS MODIFY - added okToIncClassMib parameter
/*! \fn int Dbridge_register_L2vpnDataCB(int (*Dbridge_L2vpnData)(struct sk_buff *skb,int *l2vpnRelated,int okToIncClassMib))
 *  \brief register the l2vpn data CB function
 *  \param[in] the l2vpn data CB function
 *  \return OK
 */
// ARRIS MODIFY - added okToIncClassMib parameter
int Dbridge_register_L2vpnDataCB(int (*Dbridge_L2vpnData)(struct sk_buff *skb,int *l2vpnRelated,int okToIncClassMib));

// ARRIS ADD USB runtime support
/*! \fn void DBridge_UsbEnabled(void);                                    
 *  \brief   returns value of USB enabled flag for kernel    
 *  \param[in] none.
 *  \param[out] none
 *  \return setting
 */
int DBridge_UsbEnabled( void );
// END ARRIS

// ARRIS ADD 
/*! \fn void DBridge_TftpAllowed(void);                                    
 *  \brief   returns value of TftpAllowed flag for kernel    
 *  \param[in] none.
 *  \param[out] none
 *  \return setting
 */
int DBridge_TftpAllowed( void );
// END ARRIS

#ifdef DSG
/**************************************************************************/
/*! \fn void dbridgeDsgAddOutFrames(struct net_device* ptr_dsg)  ;
 **************************************************************************
 *  \brief   increase dsg interface output  packet
 *  \param[in] none.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeDsgAddOutFrames(struct net_device* ptr_dsg, struct ethhdr* ptr_ethhdr);
#endif

#endif
