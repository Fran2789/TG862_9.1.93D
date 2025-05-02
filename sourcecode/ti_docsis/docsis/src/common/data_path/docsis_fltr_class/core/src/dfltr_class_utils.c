/*
 *
 * dfltr_class_utils.c
 * Description:
 * Docsis Filters and Classifiers common utils implementation
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

/*! \file dfltr_class_utils.c
*   \brief Docsis Filters and Classifiers common utils
*/
/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/list.h>
#include <net/llc_pdu.h>
#include <net/ipv6.h>
#include "pal.h"
#include "dfilter.h"
#include "dfltr_class_ctrl.h"
#include "dfltr_class_utils.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
/*Maximum supported interface index (in Docsis notation)*/
/*Verify if next header is an external one*/
#define IPV6_EXT_HDR(hdr)  ((hdr == NEXTHDR_HOP)     || \
							(hdr == NEXTHDR_DEST)    || \
							(hdr == NEXTHDR_ROUTING) || \
							(hdr == NEXTHDR_FRAGMENT)|| \
							(hdr == NEXTHDR_AUTH)    || \
							(hdr == NEXTHDR_ESP)     || \
							(hdr == NEXTHDR_NONE))

//ARRIS ADD
#define IP6F_OFF_MASK		0xfff8	/* mask out offset from ip6f_offlg */
//ARRIS ADD END							

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
/*Network interfaces translation table from Docsis IfIndex to Kernel ifIndex*/
static Uint32 ifTable[DOCSIS_MAX_IF_INDEX+1];

/*CMCI interfaces indices (in Kernel numeration) bitmap*/
static unsigned long long cmciIfBitmap = 0;

/**************************************************************************/
/*      LOCAL PROTOTYPES:                                                 */
/**************************************************************************/
static void Ipv6SkipExtHdrs(struct Ipv6hdr_s  *ipHdr, Uint8 **upLayerHdr, Uint8 *upLayerProto);
static int GetSysIfName(Uint32 docsisIfIndex, Uint8 *sysIfName);
/**************************************************************************/
/*      INTERFACE FUNCTIONS                                               */
/**************************************************************************/
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
void FcUtils_ExtractIpUpLayerHdr(Uint16 ipProto, void *ipHdr, Uint8 *upLayerProto, Uint8 **upLayerHdr)
{
	/*Get next header after IP*/
	/*IPv4*/
	if (ipProto == ETH_P_IP) {
		*upLayerHdr = (Uint8 *)ipHdr + ((struct iphdr *)ipHdr)->ihl*4;
		*upLayerProto = ((struct iphdr *)ipHdr)->protocol;
	}
	/*IPv6: skip over extension headers*/
	else if (ipProto == ETH_P_IPV6) {
		Ipv6SkipExtHdrs((struct Ipv6hdr_s *)ipHdr, upLayerHdr, upLayerProto);
	}
}

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
								  EthEncapsType_e *enctype, Uint16 *proto, Bool *withVlan)
{
	struct llc_pdu_un *llcHdr;
	SnapHdr_T *snapHdr;

	*withVlan = False;
	if (ethHdr->h_proto > ETH_DATA_LEN) /*typeLen field contains type - Ethernet version II*/
	{
		*enctype = ETH_ENCAP_V2;
		*proto = ntohs(ethHdr->h_proto);
		if (*proto == ETH_P_8021Q)
		{
			*withVlan = True;
			*proto = ntohs(((struct vlan_hdr *)ethData)->h_vlan_encapsulated_proto);
		}
	}
	else /*typeLen field contains length - 802.2/802.3 with LLC*/
	{
		llcHdr = (struct llc_pdu_un *)ethData;
		if (llcHdr->dsap == 0xAA) /*SNAP header comes afterwards */
		{
			snapHdr = (SnapHdr_T *)(ethData + LLC_PDU_LEN_U);/* skip LLC header */

			*enctype = ETH_ENCAP_802_2_3_SNAP;
			*proto = ntohs(snapHdr->type);		   /* encapsulated protocol is determined by type field of SNAP header*/	
		}
		else
		{
			*enctype = ETH_ENCAP_802_2_3;
			*proto = llcHdr->dsap;		  /* encapsulated protocol is determined by SAP field */
		}
	}
}

/**************************************************************************/
/*! \fn int FcUtils_AddSysInterface(Uint32 docsisIfIndex, Uint8 *sysIfName)
 **************************************************************************
 *  \brief Add system interface to interface mapping table
 *  \param[in]   Uint32 docsisIfIndex - interface index by docsis numbering
 *  \param[in]   Uint8 *sysIfName - system interface name
 *  \param[in]   Bool isCmciIf - flag that indicates if the interface is CMCI
 *  \return  status
 */
int FcUtils_AddSysInterface(Uint32 docsisIfIndex, Uint8 *sysIfName, Bool isCmciIf)
{
	struct net_device *netDev;
	Uint32 lockKey;

    if (docsisIfIndex > DOCSIS_MAX_IF_INDEX ) 
	{
		printk(KERN_ERR"%s: Invalid interface index: %d\n", __FUNCTION__, docsisIfIndex);
		return -EINVAL;
	}
	netDev = dev_get_by_name(&init_net , sysIfName);
	if (!netDev) 
	{
		printk(KERN_WARNING"%s: Failed to get net device struct for %s\n", __FUNCTION__,sysIfName);
		return 0;
	}

	printk(KERN_DEBUG"%s: docsisIfIndex=%u, sysIfName=%s, sysIfIndex = %u\n",__FUNCTION__,docsisIfIndex,sysIfName,netDev->ifindex);

	/* if we are here - we have a match */
	PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
	ifTable[docsisIfIndex] = netDev->ifindex;
	if (isCmciIf) 
	{	 /*Add the interface to CMCI kernel interfaces bitmap*/
		 cmciIfBitmap |= (1LL << (netDev->ifindex-1));
		 DFilter_UpdateIpFilterCmciMask(cmciIfBitmap);      //ARRIS ADD - Update to IP filter
	}

	/* ARRIS ADD */
	if (docsisIfIndex == DFLTR_IFACE_CUSTSIDE_ETH)
	{
	    DFilter_UpdateCustSideSysIfIndex(netDev->ifindex);
	}
	/* ARRIS ADD END */
	
	PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

	dev_put(netDev);

	return 0;
}

/**************************************************************************/
/*! \fn int FcUtils_GetSysIfData(Uint32 docsisIfIndex,  Uint8 *buf)
 **************************************************************************
 *  \brief Get system interface data 
 *  \param[in]   Uint32 docsisIfIndex - interface index by docsis numbering
 *  \param[out]  Uint8 *buf - output buffer
 *  \return  status
 */
int FcUtils_GetSysIfData(Uint32 docsisIfIndex,  Uint8 *buf)
{
	return GetSysIfName(docsisIfIndex, buf);
}

/**************************************************************************/
/*! \fn int FcUtils_GetSysIfIndexByDocsis(Uint32 docsisIfIndex, Uint32 *sysIfIndex)
 **************************************************************************
 *  \brief Map docsis interface index to system interface index 
 *  \param[in]   Uint32 docsisIfIndex - interface index by docsis numbering
 *  \param[out]  Uint32 *sysIfIndex - output buffer
 *  \return  status
 */
int FcUtils_GetSysIfIndexByDocsis(Uint32 docsisIfIndex, Uint32 *sysIfIndex)
{
	if (docsisIfIndex > DOCSIS_MAX_IF_INDEX ) 
		return -1;
    *sysIfIndex = ifTable[docsisIfIndex];

    // ARRIS MOD - if the modem tries to reference a non-existing docsis interface,
    // such as the WBM750 trying to reference the PacketCable interface, it will
    // return 0 as the IF index, which is invalid. As a quick and dirty fix, let's
    // return 32. This should not match any IF, as there are not 32 IFs.
    if( *sysIfIndex == 0 )
    {
        printk(KERN_DEBUG"%s: Interface %d does not exist\n", __FUNCTION__,docsisIfIndex);
        *sysIfIndex = 32;
    }
    //else
    //{
    //    printk(KERN_ERR"%s: Interface %d = %d\n", __FUNCTION__,docsisIfIndex, *sysIfIndex);
    //}
    // END ARRIS MOD

    return 0;
}

/**************************************************************************/
/*! \fn int FcUtils_GetDocsisIfIndexBySysIndex(Uint32 sysIfIndex, Uint32 *docsisIfIndex)
 **************************************************************************
 *  \brief Map system interface index to docsis interface index 
 *  \param[in]   Uint32  sysIfIndex - interface index by Linux numbering
 *  \param[out]  Uint32 *docsisIfIndex - output buffer
 *  \return  status
 */
int FcUtils_GetDocsisIfIndexBySysIndex(Uint32 sysIfIndex, Uint32 *docsisIfIndex)
{
    Uint32 index;
    for (index=0; index <= DOCSIS_MAX_IF_INDEX; index++)
    {
        if (ifTable[index] == sysIfIndex)
        {
            *docsisIfIndex = index;
            return 0;
        }
    }
	return -1;
}

/**************************************************************************/
/*! \fn int FcUtils_GetSysCmciIfBitmap(Uint32 *sysCmciIfBitmap)
 **************************************************************************
 *  \brief Get CMCI interfaces bitmap in Kernel numeration
 *  \param[out]  Uint32 *sysCmciIfBitmap -  CMCI interfaces bitmap
 *  \return  status
 */
int FcUtils_GetSysCmciIfBitmap(unsigned long long *sysCmciIfBitmap)
{
	*sysCmciIfBitmap = cmciIfBitmap;
	return 0;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS	                                                  */
/**************************************************************************/
/**************************************************************************/
/*! \fn static void Ipv6SkipExtHdrs(struct Ipv6hdr_s  *ipHdr, Uint8 **upLayerHdr, Uint8 *upLayerProto)
 **************************************************************************
 *  \brief Get next header after extension headers in IPv6 header
 *  \param[in]   struct Ipv6hdr_s  *ipHdr - IPv6 header
 *  \param[out]  Uint8 **upLayerHdr - pointer to the upper layer header
 *  \param[out]  Uint8 *upLayerProto - encapsulated upper layer protocol type
 *  \return  NA
 */
static void Ipv6SkipExtHdrs(struct Ipv6hdr_s  *ipHdr, Uint8 **upLayerHdr, Uint8 *upLayerProto)
{
	Uint8 offset = sizeof(struct Ipv6hdr_s);
    Uint32 nextOffset;
	Uint8 nexthdr = ipHdr->nexthdr;
	struct ipv6_opt_hdr *hdr;
	Uint32 hdrlen;

	/*Iterate through well-known extenstion headers till we reach the first with uknown type*/
	while (IPV6_EXT_HDR(nexthdr))
	{			
		/*If this is the last next header*/
		if (nexthdr == NEXTHDR_NONE) {
			break;
		}
		/*Encrypted header - cannot parse it, treat as uknown header */
		if (nexthdr == NEXTHDR_ESP) {
			break;
		}
		hdr = (struct ipv6_opt_hdr *)((Uint8 *)ipHdr + offset);
		/*Calculate the extension header length*/
		/* RFC 2460: Each extension header is an integer multiple of 8 octets long, in
		   order to retain 8-octet alignment for subsequent headers.  Multi-
		   octet fields within each extension header are aligned on their
		   natural boundaries, i.e., fields of width n octets are placed at an
		   integer multiple of n octets from the start of the header, for n = 1,
		   2, 4, or 8.
		*/
		if (nexthdr == NEXTHDR_FRAGMENT)
			hdrlen = 8;
		else if (nexthdr == NEXTHDR_AUTH)
			hdrlen = (hdr->hdrlen+2)<<2;
		else
			hdrlen = ipv6_optlen(hdr);
        
        nextOffset = (Uint32)offset + hdrlen;
        /* preventing a wrap - if the offset exceeds 255 bytes we exit the loop. */
		if(nextOffset > 0xFF)
            break;
        else
            offset = (Uint8)nextOffset;

		nexthdr = hdr->nexthdr;
	}
	*upLayerHdr = (Uint8 *)ipHdr + offset;
	*upLayerProto = nexthdr;
}

/**************************************************************************/
/*! \fn static int GetSysIfName(Uint32 docsisIfIndex,  Uint8 *sysIfName)
 **************************************************************************
 *  \brief Get system interface name by docsis index
 *  \param[in]   Uint32 docsisIfIndex - interface index by docsis numbering
 *  \param[out]  Uint8 *sysIfName - output buffer
 *  \return  status
 */
static int GetSysIfName(Uint32 docsisIfIndex,  Uint8 *sysIfName)
{
	struct net_device *netDev;
	if (docsisIfIndex > DOCSIS_MAX_IF_INDEX ) 
	{
		printk(KERN_ERR"%s: Invalid interface index: %d\n", __FUNCTION__, docsisIfIndex);
		return -EINVAL;
	}
	netDev = dev_get_by_index(&init_net, ifTable[docsisIfIndex]);
	if (!netDev) 
	{
		printk(KERN_ERR"%s: Failed to get net device struct for if index %d\n", __FUNCTION__,ifTable[docsisIfIndex]);
		return -EINVAL;
	}
	
	PAL_osMemCopy(sysIfName, netDev->name, IFNAMSIZ);
	dev_put(netDev);

	return 0;
}

/**************************************************************************/
/*! \fn static int GFcUtils_ResetSysInterface()
 **************************************************************************
 *  \brief Reset Sys Interface
 *  \return  status
 */
int FcUtils_ResetSysInterface()
{
    Uint32 index;
    for (index=0; index <= DOCSIS_MAX_IF_INDEX; index++)
        ifTable[index] = 0;
    cmciIfBitmap =0;
    return 0;
}

/* ARRIS ADD START */
int FcUtils_Ipv6IsNonFirstFragment(void *ipHdr)
{
	Uint8 offset = sizeof(struct Ipv6hdr_s);
        Uint32 nextOffset;
	Uint8 nexthdr = ((struct Ipv6hdr_s *)ipHdr)->nexthdr;
	struct ipv6_opt_hdr *hdr;
	Uint32 hdrlen;
	struct frag_hdr *fhdr;

	/*Iterate through well-known extenstion headers till we reach the first with uknown type*/
	while (IPV6_EXT_HDR(nexthdr))
	{			
		/*If this is the last next header*/
		if (nexthdr == NEXTHDR_NONE) {
			break;
		}
		/*Encrypted header - cannot parse it, treat as uknown header */
		if (nexthdr == NEXTHDR_ESP) {
			break;
		}
		hdr = (struct ipv6_opt_hdr *)((Uint8 *)ipHdr + offset);
		/*Calculate the extension header length*/
		/* RFC 2460: Each extension header is an integer multiple of 8 octets long, in
		   order to retain 8-octet alignment for subsequent headers.  Multi-
		   octet fields within each extension header are aligned on their
		   natural boundaries, i.e., fields of width n octets are placed at an
		   integer multiple of n octets from the start of the header, for n = 1,
		   2, 4, or 8.
		*/
		if (nexthdr == NEXTHDR_FRAGMENT)
		{
			fhdr = (struct frag_hdr *)hdr;
                        if (fhdr->frag_off & htons(IP6F_OFF_MASK)) 
                        {
                            /* frag offset != 0 */
                            return 1;
                        }
                        else
                        {
                            return 0;
                        }
		}
		else if (nexthdr == NEXTHDR_AUTH)
			hdrlen = (hdr->hdrlen+2)<<2;
		else
			hdrlen = ipv6_optlen(hdr);
        
                nextOffset = (Uint32)offset + hdrlen;
                /* preventing a wrap - if the offset exceeds 255 bytes we exit the loop. */
		if(nextOffset > 0xFF)
                    break;
                else
                    offset = (Uint8)nextOffset;

		nexthdr = hdr->nexthdr;
	}

	return 0;
}
/* ARRIS ADD END */

