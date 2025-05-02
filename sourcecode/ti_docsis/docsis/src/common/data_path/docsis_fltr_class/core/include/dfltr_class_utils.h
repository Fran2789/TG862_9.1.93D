/*
 *
 * fc_utils.h
 * Description:
 * Protocol filters and QOS classifiers utils header file
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

#ifndef _FC_UTILS_H_
#define _FC_UTILS_H_

/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/list.h>
#include <net/llc_pdu.h>
#include <linux/if_vlan.h>
#include <_tistdtypes.h>


/**************************************************************************/
/*      INTERFACE  Defines and Structs                                    */
/**************************************************************************/

#define SNAP_HDR_LEN	5

/*Max number of interfaces by Docsis*/
#define DOCSIS_MIN_IF_INDEX	0
#define DOCSIS_MAX_IF_INDEX	        46
#define DOCSIS_MAX_IF_CMIM_INDEX	31
#define LINUX_MIN_IF_INDEX	0
#define LINUX_MAX_IF_INDEX	63
#define INVALID_INDEX	            0XFFFF

/*Docsis Filters/Classifiers LLC protocol types*/
#define FC_LLC_PROTO_TYPE_NONE 0
#define FC_LLC_PROTO_ETHERTYPE 1
#define FC_LLC_PROTO_DSAP      2
#define FC_LLC_PROTO_MAC       3
#define FC_LLC_PROTO_ALL_PDU   4

/*Docsis Filters/Classifiers special IP protocol types*/
#define FC_IP_PROTOCOL_ALL         256
#define FC_IP_PROTOCOL_UDP_TCP     257


/*Ethernet encapsulation types*/
typedef enum 
{
	ETH_ENCAP_V2,			/*Ethernet version II*/
	ETH_ENCAP_802_2_3,		/*Ethernet 802.2/802.3 without SNAP*/
	ETH_ENCAP_802_2_3_SNAP,	/*Ethernet 802.2/802.3 with SNAP*/
}EthEncapsType_e;

/*! \var    typedef struct Ipv6hdr_s
    \brief  IPv6 header (the one that we have in kernel is not up-to-date)
*/
struct Ipv6hdr_s {
	Uint8			vcf[4];/* version:4, class:8, flow label:20*/
	Uint16			payload_len;
	Uint8			nexthdr;
	Uint8			hop_limit;
	struct	in6_addr	saddr;
	struct	in6_addr	daddr;
};

/*! \var    typedef SnapHdr_T
    \brief  SNAP header structure
*/

#pragma pack (1)

typedef struct SnapHdrStruct
{
   Uint8  OUI[3];
   Uint16 type;
} SnapHdr_T;

#pragma pack ()

/**************************************************************************/
/*      INTERFACE  Function prototypes                                    */
/**************************************************************************/
/**************************************************************************/
/*! \fn static void ExtractEthEncapsType(struct ethhdr *ethHdr, Uint8 *data, EthEncapsType_e *enctype,
								 Uint16 *proto, struct iphdr **ipHdr)
 **************************************************************************
 *  \brief Determine Ethernet encapsulation type and extract encapsulated protocol
 *  \param[in]   struct ethhdr *ethHdr - ethernet header
 *  \param[in]   Uint8 *ethData	- packet data that comes after eth header
 *  \param[out]  EthEncapsType_e *enctype - encapsulation type
 *  \param[out]  Uint16 *proto - encapsulated protocol type
 *  \param[out]  Bool *withVlan - if 802.1q header is present
 *  \return  NA
 */
void FcUtils_ExtractEthEncapsType(struct ethhdr *ethHdr, Uint8 *ethData, 
								 EthEncapsType_e *enctype, Uint16 *proto, Bool *withVlan);

/**************************************************************************/
/*! \fn void FcUtils_ExtractIpUpLayerHdr(Uint16 ipProto, void *ipHdr, Uint8 *upLayerProto, Uint8 **upLayerHdr)
 **************************************************************************
 *  \brief Extract IP encapsulated upper layer protocol
 *  \param[in]   Uint16 ipProto - IP protocol	
 *  \param[in]   void *ipHdr - IP header pointer
 *  \param[out]  Uint8 *upLayerProto - upper layer protocol number
 *  \param[out]  Uint8 **upLayerHdr - pointer to upper layer protocol header
 *  \return  NA
 */
void FcUtils_ExtractIpUpLayerHdr(Uint16 ipProto, void *ipHdr, Uint8 *upLayerProto, Uint8 **upLayerHdr);

/**************************************************************************/
/*! \fn int FcUtils_GetSysIfIndexByDocsis(Uint32 docsisIfIndex, Uint32 *sysIfIndex)
 **************************************************************************
 *  \brief Map docsis interface index to system interface index 
 *  \param[in]   Uint32 docsisIfIndex - interface index by docsis numbering
 *  \param[out]  Uint32 *sysIfIndex - output buffer
 *  \return  status
 */
int  FcUtils_GetSysIfIndexByDocsis(Uint32 docsisIfIndex, Uint32 *sysIfIndex);

/**************************************************************************/
/*! \fn int FcUtils_GetDocsisIfIndexBySysIndex(Uint32 sysIfIndex, Uint32 *docsisIfIndex)
 **************************************************************************
 *  \brief Map system interface index to docsis interface index 
 *  \param[in]   Uint32  sysIfIndex - interface index by Linux numbering
 *  \param[out]  Uint32 *docsisIfIndex - output buffer
 *  \return  status
 */
int FcUtils_GetDocsisIfIndexBySysIndex(Uint32 sysIfIndex, Uint32 *docsisIfIndex);

/**************************************************************************/
/*! \fn int FcUtils_GetSysCmciIfBitmap(Uint32 *sysCmciIfBitmap)
 **************************************************************************
 *  \brief Get CMCI interfaces bitmap in Kernel numeration
 *  \param[out]  Uint32 *sysCmciIfBitmap -  CMCI interfaces bitmap
 *  \return  status
 */
int FcUtils_GetSysCmciIfBitmap(unsigned long long *sysCmciIfBitmap);

/**************************************************************************/
/*! \fn int FcUtils_AddSysInterface(Uint32 docsisIfIndex, Uint8 *sysIfName)
 **************************************************************************
 *  \brief Add system interface to interface mapping table
 *  \param[in]   Uint32 docsisIfIndex - interface index by docsis numbering
 *  \param[in]   Uint8 *sysIfName - system interface name
 *  \param[in]   Bool isCmciIf - flag that indicates if the interface is CMCI
 *  \return  status
 */
int FcUtils_AddSysInterface(Uint32 docsisIfIndex, Uint8 *sysIfName, Bool isCmciIf);

/**************************************************************************/
/*! \fn int FcUtils_GetSysIfData(Uint32 docsisIfIndex,  Uint8 *buf)
 **************************************************************************
 *  \brief Get system interface data 
 *  \param[in]   Uint32 docsisIfIndex - interface index by docsis numbering
 *  \param[out]  Uint8 *buf - output buffer
 *  \return  status
 */
int  FcUtils_GetSysIfData(Uint32 docsisIfIndex,  Uint8 *buf);

/**************************************************************************/
/*! \fn int FcUtils_ResetSysInterface()
 **************************************************************************
 *  \brief Add system interface to interface mapping table
 *  \return  status
 */
int FcUtils_ResetSysInterface(void);

/**************************************************************************/
/*! \fn static inline STATUS ExtractNetworkHeaders(Uint8 *ethData, EthEncapsType_e enctype, Uint16 proto,
									Bool withVlan, void **ipHdr, struct vlan_hdr **vlanHdr, struct udphdr **udpTcpHdr)
 **************************************************************************
 *  \brief Get network headers from packet
 *  \param[in]  Uint8 *ethData	- packet data that comes after eth header
 *  \param[in]  EthEncapsType_e enctype - encapsulation type
 *  \param[in]  Uint16 proto - encapsulated protocol type
 *  \param[in]  Bool withVlan - if 802.1q header is present
 *  \param[out] void **ipHdr - pointer to IP header (v4 or v6)
 *  \param[out] struct vlan_hdr **vlanHdr - pointer to 802.1q header
 *  \return  OK/NOK
 */
static inline int FcUtils_ExtractNetHdrs(Uint8 *ethData, EthEncapsType_e enctype, Uint16 proto,
									Bool withVlan, void **ipHdr, struct vlan_hdr **vlanHdr);


/**************************************************************************/
/*      INTERFACE  Inline functions                                       */
/**************************************************************************/

/**************************************************************************/
/*! \fn static inline STATUS ExtractNetworkHeaders(Uint8 *ethData, EthEncapsType_e enctype, Uint16 proto,
									Bool withVlan, void **ipHdr, struct vlan_hdr **vlanHdr, struct udphdr **udpTcpHdr)
 **************************************************************************
 *  \brief Get network headers from packet
 *  \param[in]  Uint8 *ethData	- packet data that comes after eth header
 *  \param[in]  EthEncapsType_e enctype - encapsulation type
 *  \param[in]  Uint16 proto - encapsulated protocol type
 *  \param[in]  Bool withVlan - if 802.1q header is present
 *  \param[out] void **ipHdr - pointer to IP header (v4 or v6)
 *  \param[out] struct vlan_hdr **vlanHdr - pointer to 802.1q header
 *  \return  OK/NOK
 */
static inline int FcUtils_ExtractNetHdrs(Uint8 *ethData, EthEncapsType_e enctype, Uint16 proto,
									Bool withVlan, void **ipHdr, struct vlan_hdr **vlanHdr)
{
	*vlanHdr = NULL;
	switch (enctype) 
	{
	case ETH_ENCAP_V2:
		if (!withVlan) {
			*ipHdr = (void *)ethData;
		}
		else {
			*vlanHdr = (struct vlan_hdr *)ethData;
			*ipHdr = (void *)(ethData + VLAN_HLEN);
		}
		break;
	case ETH_ENCAP_802_2_3_SNAP:
		*ipHdr = (void *)(ethData + LLC_PDU_LEN_U + SNAP_HDR_LEN);
		break;
	case ETH_ENCAP_802_2_3:
		*ipHdr = (void *)(ethData + LLC_PDU_LEN_U);
		break;
	default:
		return -1;
	}

	return 0;
}

/* ARRIS ADD START */
int FcUtils_Ipv6IsNonFirstFragment(void *ipHdr);
/* ARRIS ADD END */

#endif

