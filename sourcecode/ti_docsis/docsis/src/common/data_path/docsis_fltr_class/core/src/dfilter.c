/*
 *
 * dfilter.c
 * Description:
 * Docsis IP and LLC filters implementation
 *
 * Copyright (C) 2008-2014 Texas Instruments Incorporated - http://www.ti.com/ 
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

/*! \file dfilter.c
*   \brief Docsis IP and LLC filters implementation
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
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/list.h>
#include <net/llc_pdu.h>
#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE 
/* This flag can be set in order to do selective PP sessions deletion and not all sessions flush on any filter change event (Add,Del,Upd)
   NOTICE: Need to set this flag also in dfltr_class_ctrl.c !!! */
#include <linux/ti_hil.h>
#endif
#include "_tistdtypes.h"
#include "pal.h"
#include "dfilter.h"
#include "dfltr_class_ctrl.h"
#include "dfltr_class_utils.h"
#include "dbridge_common.h"
#include "dbridge_esafe.h"   /* ARRIS ADD */

#include <net/ip.h>          /* ARRIS ADD */
#include <linux/proc_fs.h>   /* ARRIS ADD */
#include <autoconf.h>

#define GW_HTTP_PORT 8080    /* ARRIS ADD */
#define CM_HTTPS_PORT 443
#define GW_HTTPS_PORT 5540

#ifdef DFILTER_DEBUG
/* note: prints function name for you */

#  define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
extern int Dbridge_register_FilterCB(int (*Dbridge_Filter)(struct sk_buff *skb, unsigned long long destIfMask, 
           unsigned long long *acceptDestIfMask, unsigned int pktDirect));

// ARRIS ADD
extern int Dbridge_register_HttpFilterCB(int (*Dbridge_Filter)(struct sk_buff *skb));

#ifdef INCLUDE_SERCOMM_GW
extern unsigned int DbridgeDb_Getlbr0InterfaceMask();
#endif
// ARRIS ADD

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
/****************** IP/LLC Protocol filters definitions *******************/
#ifdef CONFIG_MACH_PUMA6
#include "avalanche_pp_api.h"
#define MAX_SESSIONS        AVALANCHE_PP_MAX_ACCELERATED_SESSIONS    /* Max sessions supported in PP */
#else
#include <linux/ti_ppm.h> 
#define MAX_SESSIONS        TI_PP_MAX_ACCLERABLE_SESSIONS           /* Max sessions supported in PP */
#endif

#define MAX_NUM_LLC_FILTERS         16
#define MAX_NUM_IP_FILTERS          64
#ifdef DSG
#define MAX_NUM_DSG_FILTERS          64
#endif

/*Definitions according to RFC*/
#define FLTR_CFG_TRUTH                  1
#define FLTR_CFG_FALSE                  2
#define FLTR_CFG_ACT_DISCARD    1
#define FLTR_CFG_ACT_ACCEPT     2

#define FLTR_IFACE_ALL          0xffffffffffffffff

/*IP protocol types*/
#define FLTR_IP_PROTO_ANY            256

/*Traffic direction*/
#define FLTR_TRAFF_DIRECTION_INBOUND    (1<<0) /*inbound*/
#define FLTR_TRAFF_DIRECTION_OUTBOUND   (1<<1) /*outbound*/

/* SAP values */
#define SAP_SNAP    0xAA    /* SNAP encapsulated frames */
#define SAP_BPDU    0x42    /* BPDU */

/*Check if packet traffic direction matches the filter traffic direction*/
#define FLTR_IP_TRAFF_DIRECTION_MATCH(F,P)   (((F) & P) == P)
/*Check if MAC address is multicast*/
#define IS_MAC_MULTICAST(highByte) ((highByte) & 0x01)

/******************************************************************************************/
/* Entry in LLC filter list type */
typedef struct _llcFilterListEntry
{
    struct list_head list;
    FilterLLC_T      filter;
}LlcFilterListEntry_T;

/* Entry in IP filter list type */
typedef struct _ipFilterListEntry
{
    struct list_head list;
    FilterIP_T       filter;

}IpFilterListEntry_T;

#ifdef DSG
typedef struct _dsgFilterListEntry
{
    struct list_head list;
    FilterDSG_T       filter;
}DsgFilterListEntry_T;
#endif
/*Bridge data used for filtering*/
struct FltrBridgeData_s
{
    Uint32 srcIfIndex;          /*Source interface index*/
    Uint32 destIfIndex;         /*Destination interface index*/
    unsigned long long destIntfBitmap;      /*Destination interfaces bitmap (one bit on for unicast packets)*/
    unsigned long long acceptDestInfBitmap;/*Filter output: Accepted destination interfaces bitmap (after filtering)*/
};

struct PktData_s
{
    Uint8 *destMac;
    struct iphdr *ipHdr;
    struct udphdr *udpTcpHdr;
    Bool isUdpTcp;
    Uint16 sPort;
    Uint16 dPort;
};
/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/
MODULE_LICENSE("GPL");

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static LIST_HEAD(llcFiltersListHead);/*LLC Filters list*/
static LIST_HEAD(ipFiltersListHead); /*IP Filters list*/
#ifdef DSG
static LIST_HEAD(dsgFiltersListHead); /*DSG Filters list*/
static Uint32 numDSGFilters;          /*Number of active DSG filters*/
#endif
static Uint32 llcUnmatchedAction;    /*Action to perform on packed in case that no filter matched*/
static Uint32 numLLCFilters;         /*Number of active LLC filters*/
static Uint32 ipDefaultAction;       /*Action to perform on packed in case that no filter matched*/
static Uint32 numIPFilters;          /*Number of active IP filters*/

static StpControl_t   stpFilter;             /*Spanning Tree filtering control variable*/

static Bool ipFiltersEnabled;
static Uint32 custSideSysIfIndex;    // ARRIS ADD

/**************************************************************************/
/*      LOCAL PROTOTYPES:                                                 */
/**************************************************************************/
static int AddLLCFilter(FilterLLC_T *newFilter);
static int AddIpFilter(FilterIP_T *newFilter);
static int DelLLCFilter(Uint32 index);
static int DelIpFilter(Uint32 index);
static int UpdateLLCFilter(FilterLLC_T *filterData);
static int UpdateIpFilter(FilterIP_T *filterData);
static int SetLLCUnmatchAct(Uint32 action);
static int SetIpUnmatchAct(Uint32 action);
static int SetIpFiltersEnabled(Bool enabled);
static int GetLLCFilter(Uint32 index, FilterLLC_T *buf);
static int GetLLCMatches(Uint32 index, Uint32 *matches);
static int GetPpLLCMatches(Uint32 index, Uint32 *matches);
static int PrintPpLLCMatches(void);

static int PrintAllLlcFilters(void);
static int PrintAllIpFilters(void);


static int GetLLCUnmatchAct(Uint8 *action);
static int GetIpFilter(Uint32 index, FilterIP_T *buf);
static int GetIpMatches(Uint32 index, Uint32 *matches);
static int GetPpIpMatches(Uint32 index, Uint32 *matches);
static int PrintPpIpMatches(void);
static int GetIpUnmatchAct(Uint8 *action);
static int CreateNewLLCEntry(LlcFilterListEntry_T **entry, FilterLLC_T *filter);
static int CreateNewIpEntry(IpFilterListEntry_T **entry, FilterIP_T *filter);
static DFltrAction_t ApplyLLCFilter(struct sk_buff *skb, Uint32 srcIfIndex, EthEncapsType_e enctype, Uint16 proto);
#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
static int DeletePpSessionsOfCorrelatingLlcFilters(FilterLLC_T *llcFilter1);
static int DeletePpSessionsOfCorrelatingIpFilters(FilterIP_T *ipFilter1);
#ifdef DSG
static int DeletePpSessionsOfCorrelatingDsgFilters(FilterIP_T *ipFilter1);
#endif
#endif
static inline DFltrAction_t ApplySTPFilter(EthEncapsType_e enctype, Uint16 proto);
static DFltrAction_t ApplyIPFilter(struct sk_buff *skb, struct PktData_s *pktData, struct FltrBridgeData_s *bridgeData) ;
static int FiltersIpInit(void);
static int FiltersLLCInit(void);
static int FiltersLLCCleanup(void);
static int FiltersIpCleanup(void);
static inline int IsBpduFrame(EthEncapsType_e enctype, Uint16 proto);
static int MatchIPAddr(FilterIP_T *ipFilter, Uint32 saddr, Uint32 daddr, Uint8 protocol);
static void SetIpFilterData(IpFilterListEntry_T *entry, FilterIP_T *filter);
static void SetLlcFilterData(LlcFilterListEntry_T *entry, FilterLLC_T *filter);
static void PrintIpFltrData(FilterIP_T *filter);
static void PrintLlcFltrData(FilterLLC_T *filter);
#ifdef DSG
static int AddDsgFilter(FilterDSG_T *newFilter);
static int DelDsgFilter(Uint32 index);
static int UpdateDsgFilter(FilterDSG_T *filterData);
static int GetDsgFilter(Uint32 index, FilterDSG_T *buf);
static int GetDsgMatches(Uint32 index, Uint8 *matches);
static int GetPpDSGMatches(Uint32 index, Uint8 *ppMatches);
static int CreateNewDsgEntry(DsgFilterListEntry_T **entry, FilterDSG_T *filter);
static DFltrAction_t ApplyDSGFilter(struct sk_buff* skb) ;
static int FiltersDsgInit(void);
static int FiltersDsgCleanup(void);
static void SetDsgFilterData(DsgFilterListEntry_T *entry, FilterDSG_T *filter);
static void PrintDsgFltrData(FilterDSG_T *filter);
static int PrintAllDsgFilters(void);
#endif
/**************************************************************************/
/*      INTERFACE FUNCTIONS                                               */
/**************************************************************************/
/**************************************************************************/
/*! \fn int StpControl_Set(Uint32 param1)
 **************************************************************************
 *  \brief Set spanning tree protocol control
 *  \param[in] int param1 - generic integer parameter
 *  \return  0 or error code
 */
int StpControl_Set(Uint32 param1)
{
    switch ((StpControl_t)param1)
    {
        case FLTR_STP_ON:
        case FLTR_NO_STP_DISCARD:
        case FLTR_NO_STP_FORWARD:
            stpFilter = (StpControl_t)param1;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

/**************************************************************************/
/*! \fn int DFilter_SetLLCData(int subtype, void *buf)
 **************************************************************************
 *  \brief Set LLC filter data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] int param1 - generic integer parameter
 *  \param[in] void *buf - input data buffer
 *  \return  0 or error code
 */
int DFilter_SetLLCData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;

    switch (subtype)
    {
    case FLTRIOC_S_SUBTYPE_ADD:
        ret = AddLLCFilter((FilterLLC_T *)buf);
        break;
    case FLTRIOC_S_SUBTYPE_DEL:
        ret = DelLLCFilter(param1);
        break;
    case FLTRIOC_S_SUBTYPE_UPD:
        ret = UpdateLLCFilter((FilterLLC_T *)buf);
        break;
    case FLTRIOC_S_SUBTYPE_UNMATCH_ACT:
        ret = SetLLCUnmatchAct(param1);
        break;
    case FLTRIOC_S_SUBTYPE_PRINT:
        ret = PrintPpLLCMatches();
        break;
	case FLTRIOC_S_SUBTYPE_PRINT_FILTER_DATA:
		ret = PrintAllLlcFilters();
		break;
    default:
        return -EINVAL;
    }
    return ret;
}

/**************************************************************************/
/*! \fn int DFilter_SetIpData(int subtype, void *buf)
 **************************************************************************
 *  \brief Set IP filter data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] int param1 - generic integer parameter
 *  \param[in] void *buf - input data buffer
 *  \return  0 or error code
 */
int DFilter_SetIpData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case FLTRIOC_S_SUBTYPE_ADD:
        ret = AddIpFilter((FilterIP_T *)buf);
        break;
    case FLTRIOC_S_SUBTYPE_DEL:
        ret = DelIpFilter(param1);
        break;
    case FLTRIOC_S_SUBTYPE_UPD:
        ret = UpdateIpFilter((FilterIP_T *)buf);
        break;
    case FLTRIOC_S_SUBTYPE_UNMATCH_ACT:
        ret = SetIpUnmatchAct(param1);
        break;
    case FLTRIOC_S_SUBTYPE_IPFLTR_ENABLED:
        ret = SetIpFiltersEnabled((Bool)param1);
        break;
    case FLTRIOC_S_SUBTYPE_PRINT:
        ret = PrintPpIpMatches();
        break;
	case FLTRIOC_S_SUBTYPE_PRINT_FILTER_DATA:
		ret = PrintAllIpFilters();
		break;
    default:
        return -EINVAL;
    }
    return ret;
}

/**************************************************************************/
/*! \fn int DFilter_GetLLCData(int subtype, Uint32 param1, void *buf)
 **************************************************************************
 *  \brief Get LLC filter data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] Uint32 param1 - generic input parameter
 *  \param[out] void *buf - output data buffer
 *  \return  0 or error code
 */

int DFilter_GetLLCData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case FLTRIOC_G_SUBTYPE_FLTR:
        ret = GetLLCFilter(param1,(FilterLLC_T *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_MATCHES:
        ret = GetLLCMatches(param1, (Uint32 *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_UNMATCH_ACT:
        ret = GetLLCUnmatchAct((Uint8 *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_PP_MATCHES:
        ret = GetPpLLCMatches(param1, (Uint32 *)buf);
        break;
    default:
        return -EINVAL;
    }
    return ret;

}

/**************************************************************************/
/*! \fn int int DFilter_GetIpData(int subtype, Uint32 param1, void *buf)
 **************************************************************************
 *  \brief Get IP filters data
 *  \param[in] int subtype - set command sub - type
 *  \param[in] Uint32 param1 - generic input parameter
 *  \param[out] void *buf - output data buffer
 *  \return  0 or error code
 */

int DFilter_GetIpData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;

    switch (subtype) 
    {
    case FLTRIOC_G_SUBTYPE_FLTR:
        ret = GetIpFilter(param1,(FilterIP_T *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_MATCHES:
        ret = GetIpMatches(param1, (Uint32 *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_UNMATCH_ACT:
        ret = GetIpUnmatchAct((Uint8 *)buf);
        break;
    case FLTRIOC_G_SUBTYPE_PP_MATCHES:
        ret = GetPpIpMatches(param1, (Uint32 *)buf);
        break;
    default:
        return -EINVAL;
    }
    return ret;

}

/**************************************************************************/
/*! \fn int DFilter_Apply(struct sk_buff *skb, Uint destIfMask, Uint *accDestIfMask, Uint pktDirect)
 **************************************************************************
 *  \brief Apply Protocol filters on a given packet. 
 *  \param[in] struct sk_buff *skb - packet buffer
 *  \param[in/out] struct FltrBridgeData_s *bridgeData - bridge data. Output accepted interfaces bitmap (used for non-unicast packet) 
 *  \return  action to perform on packet : accept/discard
 */
int DFilter_Apply(struct sk_buff *skb, unsigned long long destIfMask, unsigned long long *accDestIfMask, Uint pktDirect)
{
    EthEncapsType_e encapsType;
    Uint16 encapsProto;
    struct ethhdr *ethHeader;
    struct vlan_hdr *vlanHdr;
    struct PktData_s pktData;
    struct FltrBridgeData_s bridgeData;
    Bool withVlan;
    Uint8 upLayerProto, *ethData;
    DFltrAction_t ret;
    int l2vpnPacket = 0;

    DPRINTK(KERN_DEBUG"%s: Start. destIfMask = 0x%llx, pktDirect=%u\n", __FUNCTION__, destIfMask, pktDirect);
    /* All interfaces are accepted by default*/
    *accDestIfMask = destIfMask;

    /*Get source interface index from skb*/
    bridgeData.srcIfIndex = skb->ti_docsis_input_dev->ifindex;

    /*Parse ethernet header*/
    ethHeader = eth_hdr(skb);
    ethData = skb->data + ETH_HLEN; 
    FcUtils_ExtractEthEncapsType(ethHeader, ethData, &encapsType, &encapsProto, &withVlan);

#ifdef DSG
    if (pktDirect == DFLTR_TO_DSG)
        return ApplyDSGFilter(skb);
#endif
    /* Optimization - no filter is applied and default is DISCARD, no need to continue */
    if (numLLCFilters == 0) 
    {
       DPRINTK(KERN_DEBUG"%s: No active LLC filters\n", __FUNCTION__);
       if(llcUnmatchedAction == DFLTR_DISCARD)
       {
           DPRINTK(KERN_DEBUG"%s: Discarding packet by unmatched action\n", __FUNCTION__);
           return DFLTR_DISCARD;
       }
    }
    else
    {
        /* LLC inbound filtering if LLC exists */
        if (ApplyLLCFilter(skb, bridgeData.srcIfIndex, encapsType, encapsProto) != DFLTR_ACCEPT)
        {
            DPRINTK(KERN_DEBUG"%s: Discarding packet by LLC filters\n", __FUNCTION__);
            return DFLTR_DISCARD;
        }
    }


    l2vpnPacket = skb->ti_meta_info & CNID_L2VPN_RELATED;
    /* Do not accept BPDU's by STP control filtering */
    if ( (!l2vpnPacket) && (stpFilter == FLTR_NO_STP_DISCARD) && ApplySTPFilter(encapsType, encapsProto) != DFLTR_ACCEPT )
    {
        DPRINTK(KERN_DEBUG"%s: Discarding packet by Bpdu filter\n", __FUNCTION__);
        return DFLTR_DISCARD;
    }

#if 0
    if (pktDirect == DFLTR_FROM_CABLE)
    {
        /* Multicast filtering layer 2 and 3 */
           if ((((*dest_addr) & 0x01) != 0) && ((*dest_addr) != 0xff) /* DEST_MULTICAST */) {
              if(multMacListSize && (cm_brgFilterMultStaticMac(dest_addr) == TRUE)) {
                 /* Packet accepted by static multicast filter */
              }
              else {
                 if(type == 0x800) { /* All-host always exist */
                    /* Only multicast packets managed by IGMP may be accepted through multicast IP filters */
                    if (multIpFilter(ntohl(IPHeader->DADDR)) == FALSE) /* All-host always present in table */
                       return CM_BRG_DFLTR_DISCARD;
                 }
                 else {
                    /* Discard all other type multicast packets */
                    return CM_BRG_DFLTR_DISCARD;
                 }
              }
           }
    }
#endif
    /* IP filtering */
    if (!ipFiltersEnabled) 
    {
        DPRINTK(KERN_DEBUG"%s: IP filters are disabled! Accepting packet\n", __FUNCTION__);
        return DFLTR_ACCEPT;
    }
    if (encapsProto == ETH_P_IP)
    {
        if (FcUtils_ExtractNetHdrs(ethData, encapsType, encapsProto, withVlan,
                                   (void **)(&(pktData.ipHdr)), &vlanHdr) != 0 )
        {
            DPRINTK(KERN_WARNING"%s: Unsupported packet format: failed to get network headers\n",__FUNCTION__);
            return DFLTR_DISCARD;
        }
        FcUtils_ExtractIpUpLayerHdr(encapsProto, (void *)pktData.ipHdr, &upLayerProto, (Uint8 **)(&(pktData.udpTcpHdr)));
#if 0
        if (pktDirect != DFLTR_FROM_CABLE)
        {
            /* CPE IP filters are called if enabled */
            if (cpeIpMax != -1) {
               udpHeader = (udp_hdr_t*)(((unsigned long)IPHeader) + ((unsigned long)((IPHeader->VERS_HLEN & 0x0f) << 2)));
               /* DHCP/BOOTP packets are not discarded nor learnt through IP spoofing filters */
               if (udpHeader->src_port != PORT_DHCP_CLIENT)   {
                  if (cpeIpFilter(ntohl(IPHeader->SADDR)) == FALSE)
                     return CM_BRG_DFLTR_DISCARD;
               }
            }
        }
#endif

       /* Optimization - no filter is applied and default is DISCARD, no need to call the filter */
       if (numIPFilters == 0)
       {
           DPRINTK(KERN_DEBUG"%s: No active IP filters\n", __FUNCTION__);
           if (ipDefaultAction == DFLTR_DISCARD)
           {
               DPRINTK(KERN_DEBUG"%s: Discarding packet by IP default action\n", __FUNCTION__);
               return DFLTR_DISCARD;
           }
           else
           {
               DPRINTK(KERN_DEBUG"%s: Accepting packet by IP default action. accDestIfMask=0x%llx\n", __FUNCTION__, *accDestIfMask);
               return DFLTR_ACCEPT;
           }
       }
       bridgeData.destIntfBitmap = destIfMask;
       pktData.destMac = ethHeader->h_dest;
       ret = ApplyIPFilter(skb, &pktData, &bridgeData);
       *accDestIfMask = bridgeData.acceptDestInfBitmap;
       if (ret == DFLTR_DISCARD)
           DPRINTK(KERN_DEBUG"%s: Discarding packet by IP filter\n", __FUNCTION__);
       else
           DPRINTK(KERN_DEBUG"%s: Accepting packet by IP filter. accDestIfMask=0x%llx\n", __FUNCTION__, *accDestIfMask);
       return ret;
    }

    DPRINTK(KERN_DEBUG"%s: Non-IP packet. Accepting by LLC filters. accDestIfMask=0x%llx\n", __FUNCTION__, *accDestIfMask);
    return DFLTR_ACCEPT;

}

// ARRIS ADD - code for blocking HTTP access
struct proc_dir_entry *dfltrctl = NULL;
static struct proc_dir_entry *dfltrctl_wanhttp = NULL;
static struct proc_dir_entry *dfltrctl_lanhttp = NULL;
static struct proc_dir_entry *dfltrctl_wanAcl = NULL;
static int wanHttpAccess = 0;
static Bool lanHttpBlocked = FALSE;
// ARRIS ADD - code for wan ACL
#define MAX_ACL_GRUOP_NUMBER          40
#define INET_ADDRLEN                  4
#define IPV4_ADDRSTRLEN               16
#define ROUTER_LAN_IPV4_STR_FORM      "%d.%d.%d.%d"
#define IP_TO_INETADDRESS(x)          ( ((*x) << 24) | ((*(x + 1)) << 16) | ((*(x + 2)) << 8) | *(x + 3) )

struct s_wanAcl
{
   unsigned int startAddr;
   unsigned int endAddr;
};
static struct s_wanAcl wanAcl[MAX_ACL_GRUOP_NUMBER];
static unsigned int wanActualAclNum = 0;

int dfltrctl_read_wanhttp(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    if( wanHttpAccess == 0 )
    {
        len += sprintf( page+len, "blocked\n" );
    }
    else if ( wanHttpAccess == 3 )
    {
        len += sprintf( page+len, "cm page\n" );
    }
    else
    {
        len += sprintf( page+len, "not blocked\n" );
    }
    *eof = 1;
    return len;
}

int dfltrctl_write_wanhttp(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[5];

    if( count > 5 )
    {
         printk( KERN_ERR "buffer overflow\n" );
         return -EFAULT;
    }

    if( copy_from_user(local_buf, buf, count) )
    {
        printk( KERN_ERR "copy from user failure\n" );
        return -EFAULT;
    }

    if( local_buf[0] == '0' )
    {
        wanHttpAccess = 0;
    }
    else if( local_buf[0] == '1' )
    {
        wanHttpAccess = 1;
    }
    // arrisCmDoc30AccessHttpWan is set to CM Page(3)
    else if( local_buf[0] == '3' )
    {
        wanHttpAccess = 3;
    }
    else
    {
        printk( KERN_ERR "invalid WAN HTTP value\n" );
        return -EINVAL;
    }
    return count;

}

int dfltrctl_read_lanhttp(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;

    if( lanHttpBlocked == TRUE )
    {
        len += sprintf( page+len, "blocked\n" );
    }
    else
    {
        len += sprintf( page+len, "not blocked\n" );
    }
    *eof = 1;
    return len;
}

int dfltrctl_write_lanhttp(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[5];

    if( count > 5 )
    {
        printk( KERN_ERR "buffer overflow\n" );
	    return -EFAULT;
    }

    if( copy_from_user(local_buf, buf, count) )
    {
        printk( KERN_ERR "copy from user failure\n" );
	    return -EFAULT;
    }
    
    if( local_buf[0] == '0' )
    {
        lanHttpBlocked = TRUE;
    }
    else if( local_buf[0] == '1' )
    {
        lanHttpBlocked = FALSE;
    }
    else
    {
        printk( KERN_ERR "invalid LAN HTTP value\n" );
	    return -EINVAL;
    }
    return count;
}

Bool transfer_ipstr2int(char *ipstr,unsigned int *tempIp)
{
    unsigned int tempIpAddrV4[INET_ADDRLEN]={0};
	if((tempIp == NULL)||(ipstr == NULL))
		return False;
	
	*tempIp =0;
    if( INET_ADDRLEN == sscanf(ipstr, ROUTER_LAN_IPV4_STR_FORM, tempIpAddrV4,
                                                              (tempIpAddrV4 + 1),
                                                              (tempIpAddrV4 + 2),
                                                              (tempIpAddrV4 + 3)) )
    {		
        *tempIp = IP_TO_INETADDRESS(tempIpAddrV4);
		return True;
    }
	return False;
}

int dfltrctl_read_wanacl(char* page, char **start, off_t offset, int count,int *eof, void *data)
{
    int len=0;
	int i;
	
	for(i=0;i<wanActualAclNum;i++)
	{
		len += sprintf( page+len, "%d.%d.%d.%d-",(wanAcl[i].startAddr >> 24)&0xff, 
			                                     (wanAcl[i].startAddr >> 16) & 0xff,
			                                     (wanAcl[i].startAddr >> 8) & 0xff, 
			                                     wanAcl[i].startAddr & 0xff);
		len += sprintf( page+len, "%d.%d.%d.%d;",(wanAcl[i].endAddr >> 24)&0xff, 
			                                     (wanAcl[i].endAddr >> 16) & 0xff,
			                                     (wanAcl[i].endAddr >> 8) & 0xff, 
			                                      wanAcl[i].endAddr & 0xff);
	}

	len += sprintf( page+len,"\n");
	
    *eof = 1;
    return len;
}

int dfltrctl_write_wanacl(struct file *fp, const char * buf, unsigned long count, void * data)
{
    unsigned char local_buf[256]={0};
	unsigned long acl_index = 0;
	unsigned char ipstr[IPV4_ADDRSTRLEN]={0};
	unsigned int i;
	int retVal = 0;
	Bool hasStartIp=False;

    if( count > 255 )
    {
         printk( KERN_ERR "buffer overflow\n" );
         return -EFAULT;
    }

    if( copy_from_user(local_buf, buf, count) )
    {
        printk( KERN_ERR "copy from user failure\n" );
        return -EFAULT;
    }

	//clear previous settig
	memset(wanAcl,0,sizeof(wanAcl));
	wanActualAclNum = 0;

	if((count == 1)&&(local_buf[0]=='$'))
		return count;
	
	//set new setting
	acl_index = 0;
	while(acl_index < count)
	{
	    i=0;
		memset(ipstr, 0 ,sizeof(ipstr));
	    while((local_buf[acl_index]!= ':')&&(local_buf[acl_index]!= ';')&&(local_buf[acl_index]!= '-')&&(acl_index <count)&&(i<IPV4_ADDRSTRLEN))
		{
			ipstr[i] = local_buf[acl_index];
			acl_index++;
			i++;
		}
		if(local_buf[acl_index]== ';')
		{
		    retVal = transfer_ipstr2int(ipstr,&(wanAcl[wanActualAclNum].endAddr));
			if(False == retVal)
			{   
			    printk( KERN_ERR "wanAcl: not a legal IP address \n");
			    break;
			}
			if(!hasStartIp)
			{
			    wanAcl[wanActualAclNum].startAddr = wanAcl[wanActualAclNum].endAddr;
			}
			wanActualAclNum++;
			if(wanActualAclNum >= MAX_ACL_GRUOP_NUMBER)
			{   
			    printk( KERN_ERR "wanAcl: reach MAX access client groups \n");
			    break;
			}
			hasStartIp = False;
		
		}
		else if(local_buf[acl_index]== '-')//is start ip
		{
		    hasStartIp = True;
			retVal = transfer_ipstr2int(ipstr,&(wanAcl[wanActualAclNum].startAddr));
			if(False == retVal)
			{   
			    printk( KERN_ERR "wanAcl: not a legal IP address \n");
			    break;
			}
		}
		else //the last one
		{
		    retVal = transfer_ipstr2int(ipstr,&(wanAcl[wanActualAclNum].endAddr));
			if(False == retVal)
			{   
			    printk( KERN_ERR "wanAcl: not a legal IP address \n");
			    break;
			}
			if(!hasStartIp)
			{
			    wanAcl[wanActualAclNum].startAddr = wanAcl[wanActualAclNum].endAddr;
			}
			wanActualAclNum++;
			break;
		}

		acl_index++;
	}
	
	return count;

}

Bool IsInWanAcl(unsigned int ipAddr)
{
	int i=0;
	if(0 == wanActualAclNum)
	{   //not setting wan ACL, allow to access http 
        /* ARRIS CHANGE START */
        /* For non-CT/non-TWC, ACL is moved to carno/firewall */
#if !defined(CONFIG_VENDOR_TIME_WARNER) && !defined(CONFIG_VENDOR_COMCAST)
        return False;
#else
	    return True;
#endif
        /* ARRIS CHANGE END */
	}
	for(i=0;i<wanActualAclNum;i++)
	{ 
        if((ipAddr>=wanAcl[i].startAddr)&&(ipAddr<=wanAcl[i].endAddr))
			return True;
	}

	return False;
}


#define HTTP_PORT      80
#define TELNET_PORT    23
#define SSH_PORT       22
#define UPNP_HTTP_PORT 5000
/* ARRIS MOD */
/* esafe1 is not meant to be MTA in 702/802 (esafe0->MTA), add new param esafeType */
int DFilter_PortFilter( struct sk_buff *skb, int esafeType )
/* ARRIS MOD END */
{
    EthEncapsType_e encapsType;
    Uint16 encapsProto;
    struct ethhdr *ethHeader;
    struct vlan_hdr *vlanHdr;
    struct PktData_s pktData;
    Bool withVlan;
    Uint8 upLayerProto, *ethData;
    struct udphdr *udpTcpHdr;
    Bool packet_from_wan;
    Bool packet_from_mta = FALSE;
    Bool packet_from_cm = FALSE;   /* UNIHAN ADD */
    /* ARRIS ADD START */
    /* For non-CT/non-TWC, put access WEB GUI via erouter0 under the control
       of arrisRouterWebAccessWANACL
     */
#if 0
#if !defined(CONFIG_VENDOR_TIME_WARNER) && !defined(CONFIG_VENDOR_COMCAST)
    Bool packet_from_gw = FALSE;
#endif
#endif
    /* ARRIS ADD END */
  //  printk( KERN_ERR "Packet from %s (%s)\n", skb->dev->name, skb->input_dev->name );

    // note - esafe0 is meant to erouter0, esafe1 is meant to be MTA, but for future products this could
    // be any embedded device...
    if( (!strcmp(skb->dev->name, "wan0")) ||
        (!strcmp(skb->dev->name, "esafe0"))||
        (!strcmp(skb->dev->name, "esafe1")))
    {
        packet_from_wan = TRUE;
    }
    else
    {
        packet_from_wan = FALSE;
    }

    /* UNIHAN ADD START */
    if( !strcmp(skb->dev->name, "wan0") )
    {
        packet_from_cm = TRUE;
    }
    /* UNIHAN ADD END */

    /* ARRIS ADD START */
#if 0
#if !defined(CONFIG_VENDOR_TIME_WARNER) && !defined(CONFIG_VENDOR_COMCAST)
    if (!strncmp(skb->dev->name, "esafe", strlen("esafe")) &&
         (esafeType == DBR_ESAFE_EPS_EROUTER))
    {
        packet_from_gw = TRUE;
    }
#endif
#endif
    /* ARRIS ADD END */

    /* ARRIS MOD */
    if (!strncmp(skb->dev->name, "esafe", strlen("esafe")) && 
         (esafeType == DBR_ESAFE_EMTA))  
    /* ARRIS MOD END */     
    {
        packet_from_mta = TRUE;
    }
	
    // if we make it past above checks, then we need to examine packet to see if it is
    // destined to the HTTP port. If it is, we need to block it.

    /*Parse ethernet header*/
    ethHeader = eth_hdr(skb);
    ethData = skb->data + ETH_HLEN;  
    FcUtils_ExtractEthEncapsType(ethHeader, ethData, &encapsType, &encapsProto, &withVlan);
  
    /* IP filtering */
    if( (encapsProto == ETH_P_IP) || (encapsProto == ETH_P_IPV6 ) )
    {
        if (FcUtils_ExtractNetHdrs(ethData, encapsType, encapsProto, withVlan,
                                   (void **)(&(pktData.ipHdr)), &vlanHdr) != 0)
        {
            printk(KERN_WARNING"%s: Unsupported packet format: failed to get network headers\n",__FUNCTION__);
            return DFLTR_DISCARD;
        }
        FcUtils_ExtractIpUpLayerHdr(encapsProto, (void *)pktData.ipHdr, &upLayerProto, (Uint8 **)(&(pktData.udpTcpHdr)));
        
        // The below code will cause this filter to accept IPv4 fragments automatically.  It will let the 
        // port filter checks run if we're either not fragmnented or we are fragmented and this is 
        // the first pkt in frag (offset == 0).  In the fragmented case we will see proper TCP or UDP header fragment
        // and can make a good decision.  If it chooses to drop in the fragmented case the stack will not
        // be able to reassemble the pkt and it will be like dropping all of the fragments.  The only sideeffect
        // is the stack will generate an ICMP TTL exceeded / Fragment Reassembly Time exceeded pkt which will be sent
        // back to the sender.
        if( (encapsProto == ETH_P_IP) && (pktData.ipHdr->frag_off & htons(IP_OFFSET)) )
        {
            return DFLTR_ACCEPT;
        }

        if( (encapsProto == ETH_P_IPV6) && (FcUtils_Ipv6IsNonFirstFragment(pktData.ipHdr)) )
        {
            return DFLTR_ACCEPT;
        }

        /* Extract port only if packet is TCP/UDP */
        if ((upLayerProto == IPPROTO_TCP) || (upLayerProto == IPPROTO_UDP))  
        {
            udpTcpHdr = pktData.udpTcpHdr;     

            /* Destination Port */
           pktData.dPort = ntohs(udpTcpHdr->dest);	   
            if((pktData.dPort == TELNET_PORT || pktData.dPort == SSH_PORT || pktData.dPort == HTTP_PORT  || pktData.dPort == CM_HTTPS_PORT
#ifdef INCLUDE_ARRIS_GW
                 || pktData.dPort == GW_HTTPS_PORT || pktData.dPort == GW_HTTP_PORT
#endif
               ) && packet_from_mta)
            {
                printk( KERN_INFO "Blocked access from %s\n", skb->dev->name );               
                return DFLTR_DISCARD;
            }
           if( (pktData.dPort == HTTP_PORT || pktData.dPort == CM_HTTPS_PORT 
#ifdef INCLUDE_ARRIS_GW
                 || pktData.dPort ==  GW_HTTP_PORT ||  pktData.dPort == GW_HTTPS_PORT
#endif
                )  &&   (( packet_from_cm && (wanHttpAccess == 0) ) ||   (!packet_from_wan && lanHttpBlocked)) )
            {
                printk( KERN_INFO "Blocked HTTP access from %s\n", skb->dev->name );               
                return DFLTR_DISCARD;
            }
#ifdef INCLUDE_ARRIS_GW
            // If arrisCmDoc30AccessHttpWan is set to CM Page(3), drop the packet to GW GUI
            if ( (pktData.dPort ==  GW_HTTP_PORT ||  pktData.dPort == GW_HTTPS_PORT)
                 && ( packet_from_cm && wanHttpAccess == 3) )
            {
                printk( KERN_INFO "Blocked GW HTTP access from %s\n", skb->dev->name );               
                return DFLTR_DISCARD;
            }
#endif
#ifndef CONFIG_VENDOR_COMCAST
            //upnp http port just used for lan side, so discard the packets from cm/mta side
            if( (pktData.dPort == UPNP_HTTP_PORT) && (packet_from_cm || packet_from_mta) )   /* UNIHAN MOD */
            {
            	printk( KERN_INFO "Blocked upnp access from %s\n", skb->dev->name );               
                return DFLTR_DISCARD;
            }
#endif
            /* ARRIS CHANGE START */
            /* For non-CT/non-TWC, wanacl should not affect HTPP access to CM WAN IP */
            /* WAN ACL for non-CT/non-TWC is moved to carno/firewall */
#if !defined(CONFIG_VENDOR_TIME_WARNER) && !defined(CONFIG_VENDOR_COMCAST)
            if (0)//packet_from_gw)
#else
            if(packet_from_cm)
#endif
            /* ARRIS CHANGE END */
			{
	            if((pktData.dPort == HTTP_PORT || pktData.dPort == CM_HTTPS_PORT 
#ifdef INCLUDE_ARRIS_GW
	               || pktData.dPort ==  GW_HTTP_PORT ||  pktData.dPort == GW_HTTPS_PORT
#endif
	               )&&(upLayerProto == IPPROTO_TCP) && (encapsProto == ETH_P_IP))
	            {
	            	if(!IsInWanAcl(pktData.ipHdr->saddr))
	            	{   //not in ACL, discard the http request
	                    printk( KERN_ERR "Blocked http access from 0x%x\n",pktData.ipHdr->saddr);
	                    return DFLTR_DISCARD;
	            	}
	            }
			}
        }
    } 

    return DFLTR_ACCEPT;
}
// END ARRIS ADD

/**************************************************************************/
/*! \fn int DFilters_Init(void)
 **************************************************************************
 *  \brief Init Docsis Filters module 
 *  \return  OK/NOK
 */
int DFilters_Init(void)
{
    if (FiltersIpInit() != 0)
        return -1;
    if (FiltersLLCInit() != 0)
        return -1;
#ifdef DSG
    if (FiltersDsgInit() != 0)
        return -1;
#endif
    stpFilter = FLTR_NO_STP_DISCARD;

    // ARRIS ADD
    Dbridge_register_HttpFilterCB( DFilter_PortFilter );

    /* create base directory for dfltrctl */
    if(dfltrctl == NULL)
    {
        dfltrctl = proc_mkdir("dfltrctl", init_net.proc_net);
        if (!dfltrctl) {
            pr_err("Unable to proc dir entry\n");
            remove_proc_entry("dfltrctl", init_net.proc_net);
            return -ENOMEM;			        
        }
    }

    /* create WAN HTTP proc */
    if(dfltrctl_wanhttp == NULL )
    {
        dfltrctl_wanhttp = create_proc_entry("wanhttp", 0644,dfltrctl);
        if (dfltrctl_wanhttp)
        {
            dfltrctl_wanhttp->read_proc  = dfltrctl_read_wanhttp;
            dfltrctl_wanhttp->write_proc = dfltrctl_write_wanhttp;
        }    
    }
    /* create LAN HTTP proc */
    if(dfltrctl_lanhttp == NULL)
    {
        dfltrctl_lanhttp = create_proc_entry("lanhttp", 0644,dfltrctl);
        if (dfltrctl_lanhttp)
        {
            dfltrctl_lanhttp->read_proc  = dfltrctl_read_lanhttp;
            dfltrctl_lanhttp->write_proc = dfltrctl_write_lanhttp;
        }
    }
	if(dfltrctl_wanAcl == NULL )
    {
        dfltrctl_wanAcl = create_proc_entry("wanacl", 0644,dfltrctl);
        if (dfltrctl_wanAcl)
        {
            dfltrctl_wanAcl->read_proc  = dfltrctl_read_wanacl;
            dfltrctl_wanAcl->write_proc = dfltrctl_write_wanacl;
        }    
    }
    // END ARRIS ADD

    return 0;
}
/**************************************************************************/
/*! \fn int DFilters_ReInit(void)
 **************************************************************************
 *  \brief ReInit Docsis Filters module 
 *  \return  OK/NOK
 */
int DFilters_ReInit(void)
{
    if (FiltersIpInit() != 0)
        return -1;
    if (FiltersLLCInit() != 0)
        return -1;

    stpFilter = FLTR_NO_STP_DISCARD;

    return 0;
}

/**************************************************************************/
/*! \fn int DFilters_Cleanup(void)
 **************************************************************************
 *  \brief Cleanup Docsis Filters module 
 *  \return  OK/NOK
 */
int DFilters_Cleanup(void)
{
    if (FiltersIpCleanup() != 0)
        return -1;
    if (FiltersLLCCleanup() != 0)
        return -1;
#if 0
    if (FiltersDsgCleanup() != 0)
        return -1;
#endif

    return 0;
}

/**************************************************************************/
/*! \fn int DFilters_Start(void)
 **************************************************************************
 *  \brief Start Docsis Filters module operation
 *  \return  OK/NOK
 */
int DFilters_Start(void)
{
    /* Register Filter callback in Docsis bridge*/
    if(Dbridge_register_FilterCB(DFilter_Apply))
        return -1;

    return 0;
}

/**************************************************************************/
/*! \fn int DFilters_Stop(void)
 **************************************************************************
 *  \brief Stop Docsis Filters module operation
 *  \return  OK/NOK
 */
int DFilters_Stop(void)
{
    /*Unregister Filter callback in Docsis bridge*/
    if (Dbridge_register_FilterCB(NULL))
        return -1;
    return 0;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS                                                   */
/**************************************************************************/
/**************************************************************************/
/*! \fn static int FiltersLLCInit(void)
 **************************************************************************
 *  \brief Init LLC filters
 *  \return  OK/NOK
 */
static int FiltersLLCInit(void)
{
    numLLCFilters = 0;
    llcUnmatchedAction = DFLTR_ACCEPT;
    return 0;
}

/**************************************************************************/
/*! \fn static int FiltersLLCCleanup(void)
 **************************************************************************
 *  \brief Clean up LLC filters
 *  \return  OK/NOK
 */
static int FiltersLLCCleanup(void)
{
    struct list_head *pos;
    LlcFilterListEntry_T *curr=NULL;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /* free filters list */
    pos = llcFiltersListHead.next;
    while(pos != &llcFiltersListHead)
    {
        curr = list_entry(pos, LlcFilterListEntry_T, list);
        pos = pos->next;
        list_del(&curr->list);
        PAL_osMemFree(0, curr, sizeof(LlcFilterListEntry_T));
    }
    numLLCFilters = 0;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int FiltersIpInit(void)
 **************************************************************************
 *  \brief Init IP filters
 *  \return  OK/NOK
 */
static int FiltersIpInit(void)
{
    numIPFilters = 0;
    ipDefaultAction = DFLTR_ACCEPT;
    ipFiltersEnabled = 1;
    return 0;
}

/**************************************************************************/
/*! \fn static int FiltersIpCleanup(void)
 **************************************************************************
 *  \brief Cleanup IP filters
 *  \return  OK/NOK
 */
static int FiltersIpCleanup(void)
{
    struct list_head *pos;
    IpFilterListEntry_T *curr=NULL;
    Uint32 lockKey;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /* free filters list */

    pos = ipFiltersListHead.next;
    while(pos != &ipFiltersListHead)
    {
        curr = list_entry(pos, IpFilterListEntry_T, list);
        pos = pos->next;
        list_del(&curr->list);
        PAL_osMemFree(0, curr, sizeof(IpFilterListEntry_T));
    }
    numIPFilters = 0;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int GetLlcListEntryByIndex(Uint32 index, LlcFilterListEntry_T **entry)
 **************************************************************************
 *  \brief Get LLC list entry by index
 *  \param[in]  Uint32 index - entry index
 *  \param[out] LlcFilterListEntry_T **entry - pointer to the found entry
 *  \return  OK/NOK
 */
static int GetLlcListEntryByIndex(Uint32 index, LlcFilterListEntry_T **entry)
{
    struct list_head *pos;
    LlcFilterListEntry_T *curr=NULL;

    /* find a place in the list according to the index */
    list_for_each(pos, &llcFiltersListHead)
    {
        curr = list_entry(pos, LlcFilterListEntry_T, list);
        if (curr->filter.LLCIndex == index) 
            break;
    }
    if (pos != &llcFiltersListHead) 
    {
        *entry = curr;
        return 0;
    }
    else
        return -1;
}

/**************************************************************************/
/*! \fn static int GetIpListEntryByIndex(Uint32 index, IpFilterListEntry_T **entry)
 **************************************************************************
 *  \brief Get IP list entry by index
 *  \param[in]  Uint32 index - entry index
 *  \param[out] IpFilterListEntry_T **entry - pointer to the found entry
 *  \return  OK/NOK
 */
static int GetIpListEntryByIndex(Uint32 index, IpFilterListEntry_T **entry)
{
    struct list_head *pos;
    IpFilterListEntry_T *curr=NULL;

    /* find a place in the list according to the index */
    list_for_each(pos, &ipFiltersListHead)
    {
        curr = list_entry(pos, IpFilterListEntry_T, list);
        if (curr->filter.IPIndex == index) 
            break;
    }
    if (pos != &ipFiltersListHead) 
    {
        *entry = curr;
        return 0;
    }
    else
        return -1;
}

/**************************************************************************/
/*! \fn static int AddLLCFilter(FilterLLC_T *newFilter)
 **************************************************************************
 *  \brief Add new LLC filter to the LLC filters list
 *  \param[in] FilterLLC_T *newFilter - filter structure
 *  \return  0 or error code
 */
static int AddLLCFilter(FilterLLC_T *newFilter)
{
    Uint32 lockKey;
    struct list_head *pos;
    LlcFilterListEntry_T *curr=NULL, *new;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, newFilter->LLCIndex);

    if (numLLCFilters == MAX_NUM_LLC_FILTERS) {
        DPRINTK(KERN_WARNING"Failed to add LLC filter - the LLC table is full\n");
        return -EINVAL;
    }

    /* find a place in the list according to the index */
    list_for_each(pos, &llcFiltersListHead)
    {
        curr = list_entry(pos, LlcFilterListEntry_T, list);
        if (curr->filter.LLCIndex >= newFilter->LLCIndex) 
            break;
    }

    /*if filter with such index already exists return error*/
    if (curr && (curr->filter.LLCIndex == newFilter->LLCIndex)) {
        DPRINTK(KERN_WARNING"Failed to add LLC filter - filter with such index already exists\n");
        return -EINVAL;
    }

    /*Create new list entry*/
    if (CreateNewLLCEntry(&new, newFilter)){
        DPRINTK(KERN_ERR"Failed to add LLC filter - failed to create entry in the list\n");
        return -EINVAL;
    }


    /* insert before the current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_add(&new->list, pos->prev);
    numLLCFilters ++;

    /*If it is the first filter that is added register the filter callback in the bridge*/
/*    if ((numLLCFilters == 1) && (numIPFilters == 0)) 
    {
        if (Dbridge_register_FilterCB(DFilter_Apply))
            return -EFAULT;
    }*/

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    DeletePpSessionsOfCorrelatingLlcFilters(newFilter);
    ti_hil_pp_event(TI_DOCSIS_VOICE_SESSIONS_DEL, NULL);
#endif
	

    return 0;
}

/**************************************************************************/
/*! \fn static int AddIpFilter(FilterIP_T *newFilter)
 **************************************************************************
 *  \brief Add new IP filter to the IP filters list
 *  \param[in] FilterIP_T *newFilter - filter structure
 *  \return  OK/NOK
 */
static int AddIpFilter(FilterIP_T *newFilter)
{
    Uint32 lockKey;
    struct list_head *pos;
    IpFilterListEntry_T *curr=NULL, *new;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, newFilter->IPIndex);

    if (numIPFilters == MAX_NUM_IP_FILTERS) {
        DPRINTK(KERN_WARNING"Failed to add IP filter - the IP table is full\n");
        return -EINVAL;
    }

    /* find a place in the list according to the index */
    list_for_each(pos, &ipFiltersListHead)
    {
        curr = list_entry(pos, IpFilterListEntry_T, list);
        if (curr->filter.IPIndex >= newFilter->IPIndex) 
            break;
    }

    /*if filter with such index already exists return error*/
    if (curr && (curr->filter.IPIndex == newFilter->IPIndex)) {
        DPRINTK(KERN_WARNING"Failed to add IP filter - filter with such index already exists\n");
        return -EINVAL;
    }

    /*Create new list entry*/
    if (CreateNewIpEntry(&new, newFilter)){
        DPRINTK(KERN_ERR"Failed to add IP filter - failed to create entry in the list\n");
        return -EINVAL;
    }


    /* insert before the current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_add(&new->list, pos->prev);
    numIPFilters ++;

    /*If it is the first filter that is added register the filter callback in the bridge*/
/*    if ((numLLCFilters == 0) && (numIPFilters == 1)) 
    {
        if (Dbridge_register_FilterCB(DFilter_Apply))
            return -EFAULT;
    }*/

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    DeletePpSessionsOfCorrelatingIpFilters(newFilter);
    ti_hil_pp_event(TI_DOCSIS_VOICE_SESSIONS_DEL, NULL);
#endif

    return 0;
}

/**************************************************************************/
/*! \fn static int UpdateLlcFilter(FilterLLC_T *filterData)
 **************************************************************************
 *  \brief Update LLC filter with new data
 *  \param[in] FilterLLC_T *filterData - filter structure
 *  \return  OK/NOK
 */
static int UpdateLLCFilter(FilterLLC_T *filterData)
{
    Uint32 lockKey;
    LlcFilterListEntry_T *entry;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__,filterData->LLCIndex);

    /* find a filter with the same index */
    if (GetLlcListEntryByIndex(filterData->LLCIndex, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to update Llc filter - index %d not found\n", filterData->LLCIndex);
        return -EINVAL;
    }

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    DeletePpSessionsOfCorrelatingLlcFilters(filterData);
#endif

    /* update the data */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    SetLlcFilterData(entry, filterData);    
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}
/**************************************************************************/
/*! \fn static int UpdateIpFilter(FilterIP_T *filterData)
 **************************************************************************
 *  \brief Update IP filter with new data
 *  \param[in] FilterIP_T *filterData - filter structure
 *  \return  OK/NOK
 */
static int UpdateIpFilter(FilterIP_T *filterData)
{
    Uint32 lockKey;
    IpFilterListEntry_T *entry;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, filterData->IPIndex);

    /* find a filter with the same index */
    if (GetIpListEntryByIndex(filterData->IPIndex, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to update IP filter - index %d not found\n", filterData->IPIndex);
        return -EINVAL;
    }

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    DeletePpSessionsOfCorrelatingIpFilters(filterData);
#endif

    /* update the data */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    SetIpFilterData(entry, filterData);
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}

/**************************************************************************/
/*! \fn static int DelLLCFilter(Uint32 index)
 **************************************************************************
 *  \brief Delete an LLC filter with the given index
 *  \param[in] Uint32 index - filter index
 *  \return  0 or error code
 */
static int DelLLCFilter(Uint32 index)
{
    Uint32 lockKey;
    LlcFilterListEntry_T *entry;
#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    FilterLLC_T delFilter;
#endif

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, index);

    if (!numLLCFilters) {
        DPRINTK(KERN_WARNING"Failed to delete LLC filter - the LLC table is empty\n");
        return -EINVAL;
    }
    if (GetLlcListEntryByIndex(index, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to delete LLC filter - index %d not found\n", index);
        return -EINVAL;
    }

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    GetLLCFilter(index, &delFilter);
    DeletePpSessionsOfCorrelatingLlcFilters(&delFilter);
#endif

    /* delete current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_del(&entry->list);
    numLLCFilters --;

    /*If it was the only filter that was active set filter callback to NULL in docsis bridge*/
/*    if ((numLLCFilters == 0) && (numIPFilters == 0)) 
    {
        if (Dbridge_register_FilterCB(NULL))
            return -EFAULT;
    }*/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    PAL_osMemFree(0, entry, sizeof(LlcFilterListEntry_T));

    return 0;
}

/**************************************************************************/
/*! \fn static int DelIpFilter(Uint32 index)
 **************************************************************************
 *  \brief Delete an IP filter with the given index
 *  \param[in] Uint32 index - filter index
 *  \return  0 or error code
 */
static int DelIpFilter(Uint32 index)
{
    Uint32 lockKey;
    IpFilterListEntry_T *entry;
#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    FilterIP_T delFilter;
#endif

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, index);

    if (!numIPFilters) {
        DPRINTK(KERN_WARNING"Failed to delete IP filter - the IP table is empty\n");
        return -EINVAL;
    }
    if (GetIpListEntryByIndex(index, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to delete IP filter - index %d not found\n", index);
        return -EINVAL;
    }

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    GetIpFilter(index, &delFilter);
    DeletePpSessionsOfCorrelatingIpFilters(&delFilter);
#endif

    /* delete current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_del(&entry->list);
    numIPFilters --;

    /*If it was the only filter that was active set filter callback to NULL in docsis bridge*/
/*    if ((numLLCFilters == 0) && (numIPFilters == 0)) 
    {
        if (Dbridge_register_FilterCB(NULL))
            return -EFAULT;
    }*/
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    PAL_osMemFree(0, entry, sizeof(IpFilterListEntry_T));

    return 0;
}

/**************************************************************************/
/*! \fn static int SetLLCUnmatchAct(Uint32 action)
 **************************************************************************
 *  \brief Set LLC unmatched action
 *  \param[in] Uint32 action - action
 *  \return  0 or error code
 */
static int SetLLCUnmatchAct(Uint32 action)
{
    Uint32 lockKey;

    DPRINTK(KERN_DEBUG"%s. Action = %u\n",__FUNCTION__, action);

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /*translate from RFC numbering to local*/
    llcUnmatchedAction = (action == FLTR_CFG_ACT_DISCARD ? DFLTR_DISCARD : DFLTR_ACCEPT);
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    DPRINTK(KERN_DEBUG"%s. llcUnmatchedAction = %u\n",__FUNCTION__, llcUnmatchedAction);

    return 0;
}

/**************************************************************************/
/*! \fn static int SetIpFiltersEnabled(Bool enabled)
 **************************************************************************
 *  \brief Set IP filters enabled flag
 *  \param[in] Uint32 action - action
 *  \return  0 or error code
 */
static int SetIpFiltersEnabled(Bool enabled)
{
    ipFiltersEnabled = enabled;

    DPRINTK(KERN_DEBUG"%s. ipFiltersEnabled = %d\n",__FUNCTION__, ipFiltersEnabled);

    return 0;
}

/**************************************************************************/
/*! \fn static int SetIpUnmatchAct(Uint32 action)
 **************************************************************************
 *  \brief Set IP unmatched action
 *  \param[in] Uint32 action - action
 *  \return  0 or error code
 */
static int SetIpUnmatchAct(Uint32 action)
{
    Uint32 lockKey;

    DPRINTK(KERN_DEBUG"%s. Action = %u\n",__FUNCTION__, action);

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    /*translate from RFC numbering to local*/
    ipDefaultAction = (action == FLTR_CFG_ACT_DISCARD ? DFLTR_DISCARD : DFLTR_ACCEPT);
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    DPRINTK(KERN_DEBUG"%s. ipDefaultAction = %u\n",__FUNCTION__, ipDefaultAction);

    return 0;
}

/**************************************************************************/
/*! \fn static int GetLLCFilter(Uint32 index, FilterLLC_T *buf)
 **************************************************************************
 *  \brief Get LLC filter by index
 *  \param[in] Uint32 index - filter index
 *  \param[out] FilterLLC_T *buf - output data buffer
 *  \return  0 or error code
 */
static int GetLLCFilter(Uint32 index, FilterLLC_T *buf)
{
    LlcFilterListEntry_T *entry;

    if (GetLlcListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"LLC filter %d not found\n", index);
        return -EINVAL;
    }
    PAL_osMemCopy(buf, &(entry->filter), sizeof(FilterLLC_T));
    return 0;

}

/**************************************************************************/
/*! \fn static int GetIpFilter(Uint32 index, FilterIP_T *buf)
 **************************************************************************
 *  \brief Get IP filter by index
 *  \param[in] Uint32 index - filter index
 *  \param[in] FilterIP_T *buf - input data buffer
 *  \return  0 or error code
 */
static int GetIpFilter(Uint32 index, FilterIP_T *buf)
{
    IpFilterListEntry_T *entry;

    if (GetIpListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"IP filter %d not found\n", index);
        return -EINVAL;
    }
    PAL_osMemCopy(buf, &(entry->filter), sizeof(FilterIP_T));
    return 0;

}

/**************************************************************************/
/*! \fn static int GetLLCMatches(Uint32 index, Uint32 *matches)
 **************************************************************************
 *  \brief Get number of matches for LLC filter
 *  \param[in] Uint32 index- filter index
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetLLCMatches(Uint32 index, Uint32 *matches)
{
    LlcFilterListEntry_T *entry;
    if (GetLlcListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"LLC filter %d not found\n", index);
        return -EINVAL;
    }
    PAL_osMemCopy(matches, &entry->filter.LLCMatches, sizeof(Uint32));
    return 0;
}

/**************************************************************************/
/*! \fn static int GetPpLLCMatches(Uint32 index, Uint32 *matches)
 **************************************************************************
 *  \brief Get number of packet processor matches for LLC filter
 *  \param[in] Uint32 index- filter index
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetPpLLCMatches(Uint32 index, Uint32 *matches)
{
    Uint32 lockKey;
    LlcFilterListEntry_T *entry;
    Uint32 deadSessionsMatches = 0;
    Uint32 currSessionsMatches = 0;
    DppCountSessionEntry_t *currSessionEntry = NULL;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* get matched of dead sessions */
    if (GetLlcListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"LLC filter %d not found\n", index);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -EINVAL;
    }

    deadSessionsMatches = entry->filter.LLCPpCounters.ppMatches;

    /* get matched of current sessions */
    if (entry->filter.LLCPpCounters.activeSessionsList != NULL)
    {
        currSessionEntry = entry->filter.LLCPpCounters.activeSessionsList;

        while (currSessionEntry != NULL)
        {
#ifdef CONFIG_MACH_PUMA5
            TI_PP_SESSION_STATS             sessionStats;
            if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
#else
            AVALANCHE_PP_SESSION_STATS_t    sessionStats;
            if (PP_RC_SUCCESS != avalanche_pp_get_stats_session(currSessionEntry->sessionHandle, &sessionStats))
#endif
            {
                DPRINTK(KERN_INFO"Failed to get session %d statistics for LLC filter index %d\n", 
                        currSessionEntry->sessionHandle, entry->filter.LLCIndex);
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                return -EINVAL;
            }

            currSessionsMatches += sessionStats.packets_forwarded;
            currSessionEntry = currSessionEntry->llcFilterInfo.nextSession;
        }
    }

    *matches = deadSessionsMatches + currSessionsMatches;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
	return 0;
}

/**************************************************************************/
/*! \fn static int PrintPpLLCMatches(void)
 **************************************************************************
 *  \brief Print LLC filters packet processor matches
 *  \param[in]   no inputs
 *  \param[out]  no outputs
 *  \return  0 or error code
 */
static int PrintPpLLCMatches(void)
{
    Uint32 matches = 0;
    Uint32 ppMatches = 0;
    struct list_head *pos = NULL;
    LlcFilterListEntry_T *curr = NULL;

	printk("\n------------------------------------");
	printk("\n LLC Filters match packets counters ");
	printk("\n------------------------------------");
	printk("\nLLCIndex\tLLCIfIndex\tHostPktCount\tPpPktCount");
	printk("\n--------\t----------\t------------\t----------");

    list_for_each(pos, &llcFiltersListHead)
    {
        curr = list_entry(pos, LlcFilterListEntry_T, list);

        GetLLCMatches(curr->filter.LLCIndex, &matches);
        GetPpLLCMatches(curr->filter.LLCIndex, &ppMatches);
        printk("\n0x%x\t0x%x\t%u\t%u", curr->filter.LLCIndex, curr->filter.LLCIfIndex, 
               matches, ppMatches);
    }

	printk("\n");
	printk("\n");
	return 0;
}


/**************************************************************************/
/*! \fn static int PrintAllLlcFilters()
 **************************************************************************
 *  \brief Print All LLC filters
 *  \param[in]   no inputs
 *  \param[out]  no outputs
 *  \return  0 
 */

static int PrintAllLlcFilters()
{
	struct list_head *pos;
    LlcFilterListEntry_T *curr=NULL;
	printk("\n========== LLC Filters ========= \n\n");
    list_for_each(pos, &llcFiltersListHead)
    {
		curr = list_entry (pos, LlcFilterListEntry_T, list);
		PrintLlcFltrData (&(curr->filter));
    }
	return 0;
}


/**************************************************************************/
/*! \fn static int GetIpMatches(Uint32 index, Uint32 *matches)
 **************************************************************************
 *  \brief Get IP filter by index
 *  \param[in] Uint32 index- filter index
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetIpMatches(Uint32 index, Uint32 *matches)
{
    IpFilterListEntry_T *entry;
    if (GetIpListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"IP filter %d not found\n", index);
        return -EINVAL;
    }

    PAL_osMemCopy(matches, &entry->filter.IPMatches, sizeof(Uint32));
    return 0;
}

/**************************************************************************/
/*! \fn static int GetPpIpMatches(Uint32 index, Uint32 *matches)
 **************************************************************************
 *  \brief Get packet processor IP filter by index
 *  \param[in] Uint32 index- filter index
 *  \param[out] Uint8 *matches - output buffer
 *  \return  0 or error code
 */
static int GetPpIpMatches(Uint32 index, Uint32 *matches)
{
    Uint32 lockKey;
    IpFilterListEntry_T *entry;
    Uint32 inDeadSessionsMatches = 0;
    Uint32 inCurrSessionsMatches = 0;
    Uint32 outDeadSessionsMatches = 0;
    Uint32 outCurrSessionsMatches = 0;
    DppCountSessionEntry_t *currSessionEntry = NULL;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* get matched from dead sessions */
    if (GetIpListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"IP filter %d not found\n", index);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -EINVAL;
    }

    /* Inbound */
    if (entry->filter.IPDirection & FLTR_TRAFF_DIRECTION_INBOUND)
    {
        /* get matched of dead sessions */
        inDeadSessionsMatches += entry->filter.IPInPpCounters.ppMatches;

        /* get matched of current sessions */
        if (entry->filter.IPInPpCounters.activeSessionsList != NULL)
        {
            currSessionEntry = entry->filter.IPInPpCounters.activeSessionsList;
            
            while (currSessionEntry != NULL)
            {
#ifdef CONFIG_MACH_PUMA5
                TI_PP_SESSION_STATS             sessionStats;
                if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
#else
                AVALANCHE_PP_SESSION_STATS_t    sessionStats;
                if (PP_RC_SUCCESS != avalanche_pp_get_stats_session(currSessionEntry->sessionHandle, &sessionStats))
#endif
                {
                    DPRINTK(KERN_INFO"Failed to get session %d statistics for Inbound IP filter index %d\n", 
                            currSessionEntry->sessionHandle, entry->filter.IPIndex);
                    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                    return -EINVAL;
                }
    
                inCurrSessionsMatches += sessionStats.packets_forwarded;
                currSessionEntry = currSessionEntry->inIpFilterInfo.nextSession;
            }
        }
    }

    /* Outbond */
    if (entry->filter.IPDirection & FLTR_TRAFF_DIRECTION_OUTBOUND)
    {
        /* get matched of dead sessions */
        outDeadSessionsMatches = entry->filter.IPOutPpCounters.ppMatches;

        /* get matched of current sessions */
        if (entry->filter.IPOutPpCounters.activeSessionsList != NULL)
        {
            currSessionEntry = entry->filter.IPOutPpCounters.activeSessionsList;
            
            while (currSessionEntry != NULL)
            {
#ifdef CONFIG_MACH_PUMA5
                TI_PP_SESSION_STATS             sessionStats;
                if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
#else
                AVALANCHE_PP_SESSION_STATS_t    sessionStats;
                if (PP_RC_SUCCESS != avalanche_pp_get_stats_session(currSessionEntry->sessionHandle, &sessionStats))
#endif
                {
                    DPRINTK(KERN_INFO"Failed to get session %d statistics for Outbond IP filter index %d\n", 
                            currSessionEntry->sessionHandle, entry->filter.IPIndex);
                    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                    return -EINVAL;
                }
    
                outCurrSessionsMatches += sessionStats.packets_forwarded;
                currSessionEntry = currSessionEntry->outIpFilterInfo.nextSession;
            }
        }
    }

    *matches = inDeadSessionsMatches + inCurrSessionsMatches + outDeadSessionsMatches + outCurrSessionsMatches;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
	return 0;
}

/**************************************************************************/
/*! \fn static int PrintPpIpMatches(void)
 **************************************************************************
 *  \brief Print IP filters packet processor matches
 *  \param[in]   no inputs
 *  \param[out]  no outputs
 *  \return  0 or error code
 */
static int PrintPpIpMatches(void)
{
    Uint32 matches = 0;
    Uint32 ppMatches = 0;
    struct list_head *pos = NULL;
    IpFilterListEntry_T *curr = NULL;

	printk("\n-----------------------------------");
	printk("\n IP Filters match packets counters ");
	printk("\n-----------------------------------");
	printk("\nIPIndex\tIPIfIndex\tHostPktCount\tPpPktCount");
	printk("\n-------\t---------\t------------\t----------");

    list_for_each(pos, &ipFiltersListHead)
    {
        curr = list_entry(pos, IpFilterListEntry_T, list);

        GetIpMatches(curr->filter.IPIndex, &matches);
        GetPpIpMatches(curr->filter.IPIndex, &ppMatches);
        printk("\n0x%x\t0x%x\t%u\t%u", curr->filter.IPIndex, curr->filter.IPIfIndex, 
               matches, ppMatches);
    }

	printk("\n");
	printk("\n");
	return 0;
}


/**************************************************************************/
/*! \fn static int PrintAllIpFilters()
 **************************************************************************
 *  \brief Print All IP filters
 *  \param[in]   no inputs
 *  \param[out]  no outputs
 *  \return  0 
 */
static int PrintAllIpFilters()
{
	
	struct list_head *pos;
    IpFilterListEntry_T *curr=NULL;
	printk("\n========== IP Filters ========== \n\n");
	list_for_each(pos, &ipFiltersListHead)
	{
		curr = list_entry(pos, IpFilterListEntry_T, list);
		PrintIpFltrData(&(curr->filter));
	}
	return 0;
}



/**************************************************************************/
/*! \fn static int GetLLCUnmatchAct(Uint8 *action)
 **************************************************************************
 *  \brief Get LLC umatched action
 *  \param[out] Uint8 *action - output buffer
 *  \return  0 or error code
 */
static int GetLLCUnmatchAct(Uint8 *action)
{
    Uint32 actLocal = (llcUnmatchedAction == DFLTR_DISCARD ? FLTR_CFG_ACT_DISCARD : FLTR_CFG_ACT_ACCEPT);
    PAL_osMemCopy(action, &actLocal, sizeof(Uint32));
    return 0;
}

/**************************************************************************/
/*! \fn static int GetIpUnmatchAct(Uint8 *action)
 **************************************************************************
 *  \brief Get number of matches for IP filter
 *  \param[out] Uint8 *action - output buffer
 *  \return  0 or error code
 */
static int GetIpUnmatchAct(Uint8 *action)
{
    Uint32 actLocal = (ipDefaultAction == DFLTR_DISCARD ? FLTR_CFG_ACT_DISCARD : FLTR_CFG_ACT_ACCEPT);
    PAL_osMemCopy(action, &actLocal, sizeof(Uint32));
    return 0;
}

/**************************************************************************/
/*! \fn static int CreateNewLLCEntry(LlcFilterListEntry_T **entry, FilterLLC_T *filter)
 **************************************************************************
 *  \brief Create new entry in the LLC filters list
 *  \param[out] LlcFilterListEntry_T **entry - pointer to the allocated entry
 *  \param[in]  FilterLLC_T *filter - filter data
 *  \return  OK/NOK
 */
static int CreateNewLLCEntry(LlcFilterListEntry_T **entry, FilterLLC_T *filter)
{
    PAL_Result res;

    res = PAL_osMemAlloc(0, sizeof(LlcFilterListEntry_T), 0, (Ptr *)entry);
    if (res != PAL_SOK) {
        DPRINTK(KERN_ERR"%s: Failed to allocate dynamic memory\n",__FUNCTION__);
        return -1;
    }

    SetLlcFilterData(*entry, filter);   

    return 0;
}

/**************************************************************************/
/*! \fn static int CreateNewIpEntry(IpFilterListEntry_T **entry, FilterIP_T *filter)
 **************************************************************************
 *  \brief Create new entry in the IP filters list
 *  \param[out] IpFilterListEntry_T **entry - pointer to the allocated entry
 *  \param[in]  FilterIP_T *filter - filter data
 *  \return  OK/NOK
 */
static int CreateNewIpEntry(IpFilterListEntry_T **entry, FilterIP_T *filter)
{
    PAL_Result res;

    res = PAL_osMemAlloc(0, sizeof(IpFilterListEntry_T), 0, (Ptr *)entry);
    if (res != PAL_SOK) {
        DPRINTK(KERN_ERR"%s: Failed to allocate dynamic memory\n",__FUNCTION__);
        return -1;
    }

    SetIpFilterData(*entry, filter);

    return 0;
}
/**************************************************************************/
/*! \fn static void SetLlcFilterData(LlcFilterListEntry_T *entry, FilterLLC_T *filter)
 **************************************************************************
 *  \brief Set Llc filter data to the list entry
 *  \param[out] LlcFilterListEntry_T *entry - LLC filter entry in the list
 *  \param[in]  FilterLLC_T *filter - filter data
 *  \return  NA
 */
static void SetLlcFilterData(LlcFilterListEntry_T *entry, FilterLLC_T *filter)
{
    Uint32 sysIfIndex;

    if ((filter->LLCIfIndex != DFLTR_IFACE_ALL) && 
         (filter->LLCIfIndex != DFLTR_IFACE_CUSTSIDE_ETH))   // ARRIS MOD - sysIfIndex of 1 (ALL_CMCI) changes when switch between router mode & bridge mode
    {
        /*Translate interface index from docsis numbering to Kernel numbering */
        if (FcUtils_GetSysIfIndexByDocsis(filter->LLCIfIndex, &sysIfIndex) != 0) 
        {
            DPRINTK(KERN_ERR"%s: Failed translate docsis interface index %u to system one\n",__FUNCTION__,filter->LLCIfIndex );
            return;
        }

        filter->LLCIfIndex = (sysIfIndex == 0 ? (INVALID_INDEX) : sysIfIndex) ;
    }



    filter->LLCPpCounters.ppMatches = 0;
    filter->LLCPpCounters.ppBytes = 0;
    filter->LLCPpCounters.activeSessionsList = NULL;

#ifdef DFILTER_DEBUG
    PrintLlcFltrData(filter);
#endif

    PAL_osMemCopy(&(entry->filter), filter, sizeof(FilterLLC_T));
}


/**************************************************************************/
/*! \fn static int SetIpFilterData(IpFilterListEntry_T *entry, FilterIP_T *filter)
 **************************************************************************
 *  \brief Set IP filter data to the list entry
 *  \param[out] IpFilterListEntry_T *entry - IP filter entry in the list
 *  \param[in]  FilterIP_T *filter - filter data
 *  \return  NA
 */
static void SetIpFilterData(IpFilterListEntry_T *entry, FilterIP_T *filter)
{
    Uint32 sysIfIndex;

    switch (filter->IPIfIndex) 
    {
    case DFLTR_IFACE_ALL: /* all interfaces */
        filter->IPIfIndexMask = FLTR_IFACE_ALL;
        break;
    case DFLTR_IFACE_CUSTSIDE_ETH: /* All CMCI interfaces */
        if (FcUtils_GetSysCmciIfBitmap(&filter->IPIfIndexMask) != 0) 
        {
            DPRINTK(KERN_ERR"%s: Failed to get system CMCI interface bitmap\n",__FUNCTION__);
            return;
        }
        break;
    default:
        /*Translate interface index from docsis numbering to Kernel numbering */
        if (FcUtils_GetSysIfIndexByDocsis(filter->IPIfIndex, &sysIfIndex) != 0) 
        {
            DPRINTK(KERN_ERR"%s: Failed to translate docsis interface index %u to system one\n",__FUNCTION__,filter->IPIfIndex );
            return;
        }
        filter->IPIfIndex = (sysIfIndex == 0) ? (INVALID_INDEX) : sysIfIndex;
        filter->IPIfIndexMask = (sysIfIndex == 0) ? 0 : (1LL << (sysIfIndex-1));
    
        break;
    }

    /*Translate action from RFC numbering to local. Treat "Policy(3)" as Accept*/
    filter->IPControl = (filter->IPControl == FLTR_CFG_ACT_DISCARD ? DFLTR_DISCARD : DFLTR_ACCEPT);
    /*Calculate dynamic parameters in advance to improve the performance*/
    filter->IPDAddrMask = filter->IPDaddr & filter->IPDmask;
    filter->IPSAddrMask = filter->IPSaddr & filter->IPSmask;

    filter->IPInPpCounters.ppMatches = 0;
    filter->IPInPpCounters.ppBytes = 0;
    filter->IPInPpCounters.activeSessionsList = NULL;
    filter->IPOutPpCounters.ppMatches = 0;
    filter->IPOutPpCounters.ppBytes = 0;
    filter->IPOutPpCounters.activeSessionsList = NULL;

#ifdef DFILTER_DEBUG
    PrintIpFltrData(filter);
#endif
    PAL_osMemCopy(&(entry->filter), filter, sizeof(FilterIP_T));
}

/**************************************************************************/
/*! \fn static inline FilterAction_e ApplySTPFilter(EthEncapsType_e enctype, Uint16 proto)
 **************************************************************************
 *  \brief Apply STP Filter
 *  \param[in] EthEncapsType_e enctype - ethernet encapsulation type
 *  \param[in] Uint16 proto - encapsulated protocol type
 *  \return  action to perform on packet : accept/discard
 */
static inline DFltrAction_t ApplySTPFilter(EthEncapsType_e enctype, Uint16 proto)
{
    if ( IsBpduFrame(enctype,proto) )
        return DFLTR_DISCARD;
    else
        return DFLTR_ACCEPT;
}
/*******************************************************************************************************/
/*! \fn static FilterAction_e ApplyLLCFilter(Uint32 srcIfIndex, EthEncapsType_e enctype, Uint16 proto) 
 *******************************************************************************************************
 *  \brief Apply LLC filter given packet LLC header
 *  \param[in] struct sk_buff *skb - packet buffer
 *  \param[in] Uint32 srcIfIndex - index of source interface
 *  \param[in] EthEncapsType_e enctype - ethernet encapsulation type
 *  \param[in] Uint16 proto - encapsulated protocol type
 *  \return  action to perform on packet : accept/discard
 */
static DFltrAction_t ApplyLLCFilter(struct sk_buff *skb, Uint32 srcIfIndex, EthEncapsType_e enctype, Uint16 proto) 
{
   struct list_head *pos;
   Uint32 lockKey;
   LlcFilterListEntry_T *entry;
   FilterLLC_T *llcFilter;


   /* go over the filter table and try to find a match */
   list_for_each(pos, &llcFiltersListHead)
   {
      entry = list_entry(pos, LlcFilterListEntry_T, list);
      llcFilter = &(entry->filter);

      DPRINTK(KERN_DEBUG"llcFilter->LLCIfIndex=%u, srcIfIndex=%u\n", llcFilter->LLCIfIndex, srcIfIndex);


      /* ARRIS MOD - sysIfIndex of 1 (ALL_CMCI) changes when switch between router mode & bridge mode*/
      /* check if the source interface in the filter matches        */
      /* the source interface from which the packet came            */
      if ((llcFilter->LLCIfIndex != DFLTR_IFACE_ALL) &&
          (llcFilter->LLCIfIndex != DFLTR_IFACE_CUSTSIDE_ETH) &&  
          (llcFilter->LLCIfIndex != srcIfIndex)) 

          continue;

      if ((llcFilter->LLCIfIndex == DFLTR_IFACE_CUSTSIDE_ETH) && 
          (custSideSysIfIndex != srcIfIndex))
         continue;
      /* ARRIS MOD END */    


      DPRINTK(KERN_DEBUG"llcFilter->LLCProtocolType= %u, enctype=%u\n", llcFilter->LLCProtocolType, enctype);

      /* Verify encapsulated protocol */
      if (enctype == ETH_ENCAP_802_2_3) 
      {
          if (llcFilter->LLCProtocolType != FC_LLC_PROTO_DSAP)
               continue;
      }
      else /*SNAP frames are handled like ethertype frames*/
      {
          if (llcFilter->LLCProtocolType != FC_LLC_PROTO_ETHERTYPE)
            continue;
      }

      DPRINTK(KERN_DEBUG"llcFilter->LLCProtocol= %u, proto=%u\n", llcFilter->LLCProtocol, proto);
    
      if(llcFilter->LLCProtocol != proto)
          continue;

      /* if we are here - we have a match */
      PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
      llcFilter->LLCMatches++;
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
      if (skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED)
      {
          skb->pp_packet_info.ti_match_llc_filter = llcFilter;   
          DPRINTK(KERN_DEBUG"set skb->pp_packet_info.ti_match_llc_filter = %p\n", llcFilter);
      }
#endif
      PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

      DPRINTK(KERN_DEBUG"%s: LLC filter %u matched. Action=%s\n", __FUNCTION__, llcFilter->LLCIndex,
            (llcUnmatchedAction == DFLTR_DISCARD ? "Accept" : "Discard"));
      return (llcUnmatchedAction == DFLTR_DISCARD ? DFLTR_ACCEPT : DFLTR_DISCARD);

   } /* llc filter loop */

   /* no match */
   DPRINTK(KERN_DEBUG"%s: None of LLC filters matched. Action=%s\n", __FUNCTION__, 
         (llcUnmatchedAction == DFLTR_DISCARD ? "Discard" : "Accept"));
   return llcUnmatchedAction;
}

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
/**************************************************************************/
/*! \fn static int DeletePpSessionsOfCorrelatingLlcFilters(FilterLLC_T *llcFilter1)
 **************************************************************************
 *  \brief Delets all PP sessions based on filter rules matching to the given filter
 *  \param[in] FilterLLC_T *llcFilter1 - filter to base the search on 
 *  \return  0 or error code
 */
static int DeletePpSessionsOfCorrelatingLlcFilters(FilterLLC_T *llcFilter1)
{
    struct list_head *pos;
    Uint32 lockKey;
    LlcFilterListEntry_T *entry;
    FilterLLC_T *llcFilter2;
    Uint32 numSessionsFound = 0;
    DppCountSessionEntry_t *currSessionEntry;
    Uint32 *sessList;

    /* Allocate memory for the PP sessions list.*/
	/* We need to allocate MAX_SESSIONS + 1 from the reason that MAX_SESSIONS[0] saved for numSessionsFound */
    sessList = (Uint32 *)kmalloc(sizeof(Uint32)*(MAX_SESSIONS+1), GFP_ATOMIC);
    if (sessList == NULL)
    {
        printk(KERN_ERR "%s kmalloc failed\n", __FUNCTION__);
        return -ENOMEM;
    }

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* go over the filter table and try to find a match */
    list_for_each(pos, &llcFiltersListHead)
    {
        entry = list_entry(pos, LlcFilterListEntry_T, list);
        llcFilter2 = &(entry->filter);

        /* check if the source interface in the filter matche */
        if (llcFilter1->LLCIfIndex != llcFilter1->LLCIfIndex)
        {
            continue;
        }

        /* Verify encapsulated protocol */
        if (llcFilter1->LLCProtocolType != llcFilter2->LLCProtocolType)
        {
            continue;
        }
        if(llcFilter1->LLCProtocol != llcFilter2->LLCProtocol)
        {
            continue;
        }

        /* Found a matching filter, get the PP sessions list */
        currSessionEntry = llcFilter2->LLCPpCounters.activeSessionsList;
        while (currSessionEntry != NULL)
        {
            Uint32 i;
            /* We start from entry 1 and not 0 since entry 0 in sessList is reserved for saving numSessionsFound */
            for (i = 1; i <= numSessionsFound; i++)
            {
                if (sessList[i] == currSessionEntry->sessionHandle)
                {
                    DPRINTK("Session %d already in the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, i);
                    break;
                }
            }
            if (i == numSessionsFound + 1)
            {
                numSessionsFound++;
                sessList[numSessionsFound] = currSessionEntry->sessionHandle;
                DPRINTK("Adding session %d to the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, numSessionsFound);
            }

            currSessionEntry = currSessionEntry->llcFilterInfo.nextSession;
        }

    }

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    if (numSessionsFound)
    {
        DPRINTK("Sending HIL event to delete %d sessions in the TBD sessions list\n", numSessionsFound);

        sessList[0] = numSessionsFound;
        ti_hil_pp_event(TI_DOCSIS_SESSIONS_DEL, (void *)sessList);
    }

    /* Free the session buffer memory */
    kfree(sessList);

    return 0;
}
#endif


/**************************************************************************/
/*! \fn static FilterAction_e MatchIpFilter(struct iphdr *ipHdr, Uint8 *destMac,
                              FltrBridgeData_s *bridgeData,Uint16 sPort, Uint16 dPort)
 **************************************************************************
 *  \brief Find matching IP filter from a list of active IP filters
 *  \param[in] struct sk_buff *skb - packet buffer
 *  \param[in] struct iphdr *ipHdr - packet IP header
 *  \param[in] Uint8 *destMac - destination MAC
 *  \param[in/out] struct FltrBridgeData_s *bridgeData - bridge data
 *                 Output accepted interfaces bitmap (used for non-unicast packet)
 *  \param[in] Uint32 intfDirection - interface direction: inbound or outbound
 *  \param[in] Uint16 sPort, Uint16 dPort - source and destination ports
 *  \return  action to perform on packet : accept/discard or Uknown if no match found
 */
static DFltrAction_t MatchIpFilter(struct sk_buff *skb, struct PktData_s *pktData, struct FltrBridgeData_s *bridgeData,
                                    Uint32 intfDirection)
{
      Uint32 lockKey;
      struct list_head *pos;
      DFltrAction_t action;
      IpFilterListEntry_T   *entry;
      FilterIP_T *ipFilter;
      struct iphdr *ipHdr = pktData->ipHdr;

      action = DFLTR_UNKNOWN;

      DPRINTK(KERN_DEBUG"%s: Interface direction = 0x%x\n", __FUNCTION__, intfDirection);

      /* go over the filter table and try to find a match */
      list_for_each(pos, &ipFiltersListHead)
      {
         entry = list_entry(pos, IpFilterListEntry_T, list);
         ipFilter = &(entry->filter);

         /* check if the direction/interface in the filter matches the type of filters which is currently applied    */
         if (!FLTR_IP_TRAFF_DIRECTION_MATCH(ipFilter->IPDirection, intfDirection))
               continue;

         /* if this filter is only for broadcasts and multicasts skip if MAC is non-broadast/multicast */
         if ((ipFilter -> IPBroadcast == FLTR_CFG_TRUTH) && (!IS_MAC_MULTICAST(pktData->destMac[0])))
               continue;

         /*  check source + destinaion ip address and protocol type    */
         if (!MatchIPAddr(ipFilter,ntohl(ipHdr->saddr),ntohl(ipHdr->daddr), ipHdr->protocol))
            continue;

         /* check source and destination port numbers if packet is tcp or udp */
         if (pktData->isUdpTcp)
         {
            //ARRIS ADD - Start - Bypasses filter check for fragmented packets
            if((ipHdr->frag_off & htons(IP_OFFSET)) != 0)
            {                
                //printk("%s: Fragmented packet being checked, let it go through filter\n",__FUNCTION__);
                continue;
            }
            //ARRIS ADD - End

            if (pktData->sPort < ipFilter->IPSourcePortLow)
               continue;
            if (pktData->sPort > ipFilter->IPSourcePortHigh)
               continue;
            if (pktData->dPort < ipFilter->IPDestPortLow)
               continue;
            if (pktData->dPort > ipFilter->IPDestPortHigh)
               continue;
         }

         /* Check TOS */
         if (ipFilter->IPTos != ( ipHdr->tos & ipFilter->IPTosMask) )
            continue;

         /* Verify CMCI interfaces */
         if(intfDirection & FLTR_TRAFF_DIRECTION_INBOUND) 
         {
             
             /*If filter applies to given source interface */
              if ((ipFilter->IPIfIndex != DFLTR_IFACE_ALL) &&
                  (ipFilter->IPIfIndex != DFLTR_IFACE_CUSTSIDE_ETH) &&
                  (ipFilter->IPIfIndex != bridgeData->srcIfIndex)) 
              {
                  continue;
              }
              else if (ipFilter->IPIfIndex == DFLTR_IFACE_CUSTSIDE_ETH) 
              {   /*match if source interface is one of CMCI interfaces*/
                  if ( (ipFilter->IPIfIndexMask & (1LL << (bridgeData->srcIfIndex-1))) == 0)
                      continue;
              }
         }
         else /*outbound traffic direction*/
         { 
#ifdef INCLUDE_SERCOMM_GW  // ARRIS ADD This allows filters to be applied to gateway devices.

             unsigned int gwBitmap = DbridgeDb_Getlbr0InterfaceMask(); // Get the latest GW bitmap
             unsigned int cmciBitmap;
             FcUtils_GetSysCmciIfBitmap(&cmciBitmap); // Get the latest CMCI bitmap

            if(gwBitmap & bridgeData->destIntfBitmap) // If this packet is headed to the GW, it's probably heading to CMCI eventually
            {
                bridgeData->destIntfBitmap |= cmciBitmap; //Modify the destination bitmap so that CMCI filters apply to GW packets
                bridgeData->destIntfBitmap &= ~gwBitmap;

                bridgeData->acceptDestInfBitmap |= cmciBitmap; // Modify the accepted interfaces bitmap so that CMCI filters apply to GW packets
                bridgeData->acceptDestInfBitmap &= ~gwBitmap;
            }
#endif  // ARRIS ADD END
             /*If filter applies to one interface in the destination interface bitmap */
            if ( (ipFilter->IPIfIndexMask & bridgeData->destIntfBitmap) == 0)
               continue;
            /* Need to be the last operation */

            /* Filter out matched by this filter interface from destination interfaces bitmap if default action is Accept */
            if ((ipDefaultAction == DFLTR_ACCEPT) && (ipFilter->IPControl == DFLTR_DISCARD))
               bridgeData->acceptDestInfBitmap &= ~((ipFilter->IPIfIndexMask) & bridgeData->destIntfBitmap);
            /* Add matched by this filter interface from destination interfaces bitmap
             if default action is Discard */
            if ((ipDefaultAction == DFLTR_DISCARD) && (ipFilter->IPControl != DFLTR_DISCARD))
               bridgeData->acceptDestInfBitmap |= (ipFilter->IPIfIndexMask) & bridgeData->destIntfBitmap;
         }

         /* if we are here - we have a match */
         PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
         ipFilter->IPMatches++;
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
         if (skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED)
         {
             if(intfDirection & FLTR_TRAFF_DIRECTION_INBOUND) 
             {
                 skb->pp_packet_info.ti_match_inbound_ip_filter = ipFilter;
                 DPRINTK(KERN_DEBUG"set skb->pp_packet_info.ti_match_inbound_ip_filter = %p\n", ipFilter);
             }
             else
             {
                 skb->pp_packet_info.ti_match_outbound_ip_filter = ipFilter;
                 DPRINTK(KERN_DEBUG"set skb->pp_packet_info.ti_match_outbound_ip_filter = %p\n", ipFilter);
             }
         }
#endif
         PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

         DPRINTK(KERN_DEBUG"%s: IP filter %u matched\n", __FUNCTION__, ipFilter->IPIndex);

         if (ipFilter->IPControl == DFLTR_DISCARD) 
        {
            /* For outbound check if fully discarded for all interfaces, if there are no destination interfaces in bit mask */
            if ((intfDirection & FLTR_TRAFF_DIRECTION_INBOUND) || (bridgeData->acceptDestInfBitmap == 0)) 
            {
                DPRINTK(KERN_DEBUG"%s: Discarding packet for all interfaces\n", __FUNCTION__);
                action = DFLTR_DISCARD;
                break; /*matched*/
            }
            /* Partially discarded. Treat it as no match since it is not for all interfaces in the destination bitmap */
            else
            {
                DPRINTK(KERN_DEBUG"%s: Partially discarded packet for interfaces 0x%llx. Continuing\n", __FUNCTION__,bridgeData->acceptDestInfBitmap);
                continue;
            }
         } 
         else 
         {
             DPRINTK(KERN_DEBUG"%s: Accepted packet by filter %u for if index %u\n", __FUNCTION__,ipFilter->IPIndex, ipFilter->IPIfIndex);
             action = DFLTR_ACCEPT; 
             /* should we continue */
             if (ipFilter->IPContinue == FLTR_CFG_FALSE) 
             {
                 DPRINTK(KERN_DEBUG"%s: Don't continue to other IP filters\n", __FUNCTION__);
                 break;
             }
             else
                 DPRINTK(KERN_DEBUG"%s: Continuing to other IP filters\n", __FUNCTION__);
         }
      }

      return action;
}

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
/**************************************************************************/
/*! \fn static int DeletePpSessionsOfCorrelatingIpFilters(FilterIP_T *ipFilter1)
 **************************************************************************
 *  \brief Delets all PP sessions based on filter rules matching to the given filter
 *  \param[in] FilterIP_T *ipFilter1 - filter to base the search on 
 *  \return  0 or error code
 */
static int DeletePpSessionsOfCorrelatingIpFilters(FilterIP_T *ipFilter1)
{
    struct list_head *pos;
    IpFilterListEntry_T   *entry;
    FilterIP_T *ipFilter2;
    Uint32 lockKey;
    Uint32 numSessionsFound = 0;
    DppCountSessionEntry_t *currSessionEntry;
    Uint32 *sessList;

    /* Allocate memory for the PP sessions list. */
	/* We need to allocate MAX_SESSIONS + 1 from the reason that MAX_SESSIONS[0] saved for numSessionsFound */
    sessList = (Uint32 *)kmalloc(sizeof(Uint32)*(MAX_SESSIONS+1), GFP_ATOMIC);
    if (sessList == NULL)
    {
        printk(KERN_ERR "%s kmalloc failed\n", __FUNCTION__);
        return -ENOMEM;
    }

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* go over the filter table and try to find a match */
    list_for_each(pos, &ipFiltersListHead)
    {
        entry = list_entry(pos, IpFilterListEntry_T, list);
        ipFilter2 = &(entry->filter);

        /* check if the direction matches */
        if ((ipFilter1->IPDirection & ipFilter2->IPDirection) == 0)
        {
            continue;
        }

        /* if this filter is only for broadcasts and multicasts skip if MAC is non-broadast/multicast */
        if (ipFilter1->IPBroadcast != ipFilter2->IPBroadcast)
        {
            continue;
        }

        /*  check source + destinaion ip address and protocol type */
        if ((ipFilter1->IPSaddr & ipFilter1->IPSmask) != (ipFilter2->IPSaddr & ipFilter2->IPSmask))
        {
            continue;
        }
        if ((ipFilter1->IPDaddr & ipFilter1->IPDmask) != (ipFilter2->IPDaddr & ipFilter2->IPDmask))
        {
            continue;
        }
        if (ipFilter1->IPProtocol != ipFilter2->IPProtocol)
        {
            continue;
        }

        /* check source and destination port numbers */
        if ((ipFilter1->IPSourcePortHigh < ipFilter2->IPSourcePortLow) || (ipFilter1->IPSourcePortLow > ipFilter2->IPSourcePortHigh))
        {
            continue;
        }
        if ((ipFilter1->IPDestPortHigh < ipFilter2->IPDestPortLow) || (ipFilter1->IPDestPortLow > ipFilter2->IPDestPortHigh))
        {
            continue;
        }

        /* Check TOS */
        if (ipFilter1->IPTos != ipFilter2->IPTos)
        {
            continue;
        }

        /* Verify CMCI interfaces */
        if ((ipFilter1->IPIfIndex & ipFilter1->IPIfIndexMask) != (ipFilter2->IPIfIndex & ipFilter2->IPIfIndexMask))
        {
            continue;
        }

        /* Found a matching filter, get the PP sessions list */

        /* Inbound */
        currSessionEntry = ipFilter2->IPInPpCounters.activeSessionsList;
        while (currSessionEntry != NULL)
        {
            Uint32 i;
            /* We start from entry 1 and not 0 since entry 0 in sessList is reserved for saving numSessionsFound */
            for (i = 1; i <= numSessionsFound; i++)
            {
                if (sessList[i] == currSessionEntry->sessionHandle)
                {
                    DPRINTK("Session %d already in the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, i);
                    break;
                }
            }
            if (i == numSessionsFound + 1)
            {
                numSessionsFound++;
                sessList[numSessionsFound] = currSessionEntry->sessionHandle;
                DPRINTK("Adding session %d to the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, numSessionsFound);
            }
            
            currSessionEntry = currSessionEntry->inIpFilterInfo.nextSession;
        }

        /* Outbound */
        currSessionEntry = ipFilter2->IPOutPpCounters.activeSessionsList;
        while (currSessionEntry != NULL)
        {
            Uint32 i;
            /* We start from entry 1 and not 0 since entry 0 in sessList is reserved for saving numSessionsFound */
            for (i = 1; i <= numSessionsFound; i++)
            {
                if (sessList[i] == currSessionEntry->sessionHandle)
                {
                    DPRINTK("Session %d already in the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, i);
                    break;
                }
            }
            if (i == numSessionsFound + 1)
            {
                numSessionsFound++;
                sessList[numSessionsFound] = currSessionEntry->sessionHandle;
                DPRINTK("Adding session %d to the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, numSessionsFound);
            }
            
            currSessionEntry = currSessionEntry->outIpFilterInfo.nextSession;
        }
    }

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    if (numSessionsFound)
    {
        DPRINTK("Sending HIL event to delete %d sessions in the TBD sessions list\n", numSessionsFound);

        sessList[0] = numSessionsFound;
        ti_hil_pp_event(TI_DOCSIS_SESSIONS_DEL, (void *)sessList);
    }

    /* Free the session buffer memory */
    kfree(sessList);

    return 0;
}
#endif

/**************************************************************************/
/*! \fn static int MatchIPAddr(FilterIP_T *ipFilter, Uint32 saddr, Uint32 daddr, Uint8 protocol) 
 **************************************************************************
 *  \brief Check if IP addresses and protocol of packet and filter of given index match
 *  \param[in] FilterIP_T *ipFilter - IP filter
 *  \param[in] Uint32 saddr - source IP address
 *  \param[in] Uint32 daddr - destination IP address
 *  \param[in] Uint8 protocol - IP protocol
 *  \return  action to perform on packet : accept/discard
 */
static int MatchIPAddr(FilterIP_T *ipFilter, Uint32 saddr, Uint32 daddr, Uint8 protocol) 
{
   if ( (ipFilter->IPSAddrMask == (saddr & ipFilter->IPSmask)) &&
        (ipFilter->IPDAddrMask == (daddr & ipFilter->IPDmask)) &&
        ( (ipFilter->IPProtocol == FLTR_IP_PROTO_ANY) || 
          (ipFilter->IPProtocol == protocol) ) ) 
   {
       return 1;
   }

   return 0;
}

/**************************************************************************/
/*! \fn static FilterAction_e ApplyIPFilter(struct iphdr *ipHdr, Uint8 *destMac,
                                            struct FltrBridgeData_s *bridgeData) 
 **************************************************************************
 *  \brief Apply IP filter given packet IP header
 *  \param[in] struct sk_buff *skb - packet buffer
 *  \param[in] struct iphdr *ipHdr - packet IP header
 *  \param[in] struct udphdr *udpTcpHdr - packet UDP/TCP header (first two bytes are the same)
 *  \param[in] Uint8 *destMac - destination MAC
 *  \param[in/out] struct FltrBridgeData_s *bridgeData - bridge data
 *                 Output accepted interfaces bitmap (used for non-unicast packet)
 *  \return  action to perform on packet : accept/discard
 */
static DFltrAction_t ApplyIPFilter(struct sk_buff *skb, struct PktData_s *pktData, struct FltrBridgeData_s *bridgeData)
{
    DFltrAction_t inboundAction, outboundAction;
    struct iphdr *ipHdr = pktData->ipHdr;
    struct udphdr *udpTcpHdr = pktData->udpTcpHdr;

   /* Extract port only if packet is TCP/UDP */
   if ((ipHdr->protocol == IPPROTO_TCP) || (ipHdr->protocol == IPPROTO_UDP))  
   {
       pktData->isUdpTcp = 1;
      /* Source Port */
      pktData->sPort = ntohs(udpTcpHdr->source);
      /* Destination Port */
      pktData->dPort   = ntohs(udpTcpHdr->dest);
   }
   else
   {
        pktData->isUdpTcp = False;
   }
  
   /*Intialize accepted interfaces bitmap in case no filter matches */
   if (ipDefaultAction == DFLTR_ACCEPT)
      bridgeData->acceptDestInfBitmap = bridgeData->destIntfBitmap;
   else
      bridgeData->acceptDestInfBitmap = 0;

   /* Apply first inbound then outbound IP filters */
   inboundAction = MatchIpFilter(skb, pktData, bridgeData, FLTR_TRAFF_DIRECTION_INBOUND);
   if (inboundAction == DFLTR_DISCARD) 
   {
       DPRINTK(KERN_DEBUG"%s: Discarding packet by inbound IP filters\n", __FUNCTION__);
       return DFLTR_DISCARD;
   }

   outboundAction = MatchIpFilter(skb, pktData, bridgeData, FLTR_TRAFF_DIRECTION_OUTBOUND);
   if (outboundAction == DFLTR_DISCARD) 
   {
       DPRINTK(KERN_DEBUG"%s: Discarding packet by outbound IP filters\n", __FUNCTION__);
       return DFLTR_DISCARD;
   }
                                                
   if ( ((inboundAction == DFLTR_UNKNOWN) || (outboundAction == DFLTR_UNKNOWN)) && (ipDefaultAction == DFLTR_DISCARD)) 
   {
       DPRINTK(KERN_DEBUG"%s: None of inbound and/or outbound IP filters matched. Discarding packet by default action\n", __FUNCTION__);
       return DFLTR_DISCARD;
   }
   return DFLTR_ACCEPT;
}
/**************************************************************************/
/*! \fn static inline Bool IsBpduFrame(EthEncapsType_e enctype, Uint16 proto)
 **************************************************************************
 *  \brief Determine if frame is BPDU
 *  \param[out]  EthEncapsType_e enctype - encapsulation type
 *  \param[out]  Uint16 proto - encapsulated protocol type
 *  \return  True/False
 */
static inline int IsBpduFrame(EthEncapsType_e enctype, Uint16 proto)
{
    return ( (enctype == ETH_ENCAP_802_2_3) && (proto == SAP_BPDU) );
}

/**************************************************************************/
/*! \fn static void PrintIpFltrData(FilterIP_T *filter)
 **************************************************************************
 *  \brief Print IP filter data
 *  \param[in] FilterIP_T *ipFilter - IP filter
 *  \return  NA
 */
static void PrintIpFltrData(FilterIP_T *filter)
{
	printk("IP filter %u data:\n", filter->IPIndex);
	printk("IPControl = %u\n", filter->IPControl);
	printk("IPIfIndex = %u\n", filter->IPIfIndex);
	printk("IPIfIndexMask = 0x%llx\n", filter->IPIfIndexMask);
	printk("IPDirection = %u\n", filter->IPDirection);
	printk("IPBroadcast = %u\n", filter->IPBroadcast);
	printk("IPSaddr = 0x%.8x\n", filter->IPSaddr);
	printk("IPSmask = 0x%.8x\n",  filter->IPSmask);
	printk("IPSAddrMask = 0x%.8x\n", filter->IPSAddrMask);
	printk("IPDaddr = 0x%.8x\n", filter->IPDaddr);
	printk("IPDmask = 0x%.8x\n", filter->IPDmask);
	printk("IPDAddrMask = 0x%.8x\n", filter->IPDAddrMask);
	printk("IPProtocol = %u\n", filter->IPProtocol);
	printk("IPSourcePortLow = %u\n", filter->IPSourcePortLow);
	printk("IPSourcePortHigh = %u\n", filter->IPSourcePortHigh);
	printk("IPDestPortLow = %u\n", filter->IPDestPortLow);
	printk("IPDestPortHigh= %u\n", filter->IPDestPortHigh);
	printk("IPMatches = %u\n", filter->IPMatches);
	printk("IPInPpCounters IPPpMatches = %u\n", filter->IPInPpCounters.ppMatches);
	printk("IPOutPpCounters IPPpMatches = %u\n", filter->IPOutPpCounters.ppMatches);
	printk("IPTos = %u\n", filter->IPTos);
	printk("IPTosMask = 0x%x\n", filter->IPTosMask);
    /*padding*/
	printk("IPContinue = %u\n", filter->IPContinue);
	printk("IPPolicyId = %u\n", filter->IPPolicyId); 
	printk("-------------------------------- \n\n");
}

/**************************************************************************/
/*! \fn static void PrintLlcFltrData(FilterLLC_T *filter)
 **************************************************************************
 *  \brief Print LLC filter data
 *  \param[in] FilterLLC_T *filter - LLC filter
 *  \return  NA
 */
static void PrintLlcFltrData(FilterLLC_T *filter)
{
	printk("LLC filter %u data:\n", filter->LLCIndex);
	printk("LLCIfIndex = %u\n", filter->LLCIfIndex);
	printk("LLCProtocolType = %u\n", filter->LLCProtocolType);
	printk("LLCProtocol = %u\n", filter->LLCProtocol);
	printk("LLCMatches = %u\n", filter->LLCMatches);
	printk("LLCPpCounters ppMatches = %u\n", filter->LLCPpCounters.ppMatches);
	printk("-------------------------------- \n\n");
}
#ifdef DSG
int DFilter_SetDsgData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;
    switch (subtype)
    {
        case FLTRIOC_S_SUBTYPE_ADD:
            ret = AddDsgFilter((FilterDSG_T *)buf);
            break;
        case FLTRIOC_S_SUBTYPE_DEL:
            ret = DelDsgFilter(param1);
            break;
        case FLTRIOC_S_SUBTYPE_UPD:
            ret = UpdateDsgFilter((FilterDSG_T *)buf);
            break;
		case FLTRIOC_S_SUBTYPE_PRINT_FILTER_DATA:
			ret = PrintAllDsgFilters();
			break;
        default:
            return -EINVAL;
    }
    return ret;
}
int DFilter_GetDsgData(int subtype, Uint32 param1, void *buf)
{
    int ret = 0;
    switch (subtype)
    {
        case FLTRIOC_G_SUBTYPE_FLTR:
            ret = GetDsgFilter(param1,(FilterDSG_T *)buf);
            break;
        case FLTRIOC_G_SUBTYPE_MATCHES:
            ret = GetDsgMatches(param1, (Uint8 *)buf);
            break;
		case FLTRIOC_G_SUBTYPE_PP_MATCHES:
			ret = GetPpDSGMatches(param1, (Uint8 *)buf);
            break;
        default:
            return -EINVAL;
    }
    return ret;
}
static int FiltersDsgInit(void)
{
    numDSGFilters = 0;
    return 0;
}
static int FiltersDsgCleanup(void)
{
    struct list_head *pos;
    DsgFilterListEntry_T *curr=NULL;
    Uint32 lockKey;
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    pos = dsgFiltersListHead.next;
    while(pos != &dsgFiltersListHead)
    {
        curr = list_entry(pos, DsgFilterListEntry_T, list);
        pos = pos->next;
		list_del(&curr->list);
        PAL_osMemFree(0, curr, sizeof(DsgFilterListEntry_T));
    }
    numDSGFilters = 0;
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
    return 0;
}



static int GetDsgListEntryByIndex(Uint32 index, DsgFilterListEntry_T **entry)
{
    struct list_head *pos;
    DsgFilterListEntry_T *curr=NULL;
    list_for_each(pos, &dsgFiltersListHead)
    {
        curr = list_entry(pos, DsgFilterListEntry_T, list);
        if (curr->filter.DSGIndex == index)
            break;
    }
    if (pos != &dsgFiltersListHead)
    {
        *entry = curr;
        return 0;
    }
    else
        return -1;
}

static int AddDsgFilter(FilterDSG_T *newFilter)
{
    Uint32 lockKey;
    struct list_head *pos;
    DsgFilterListEntry_T *curr=NULL, *new;
    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, newFilter->DSGIndex);
    if (numDSGFilters == MAX_NUM_DSG_FILTERS)
    {
        DPRINTK(KERN_WARNING"Failed to add DSG filter - the DSG table is full\n");
        return -EINVAL;
    }
    list_for_each(pos, &dsgFiltersListHead)
    {
        curr = list_entry(pos, DsgFilterListEntry_T, list);
        if (curr->filter.DSGIndex >= newFilter->DSGIndex)
            break;
    }
    if (curr && (curr->filter.DSGIndex == newFilter->DSGIndex))
    {
        DPRINTK(KERN_WARNING"Failed to add DSG filter - filter with such index already exists\n");
        return -EINVAL;
    }
    if (CreateNewDsgEntry(&new, newFilter))
    {
        DPRINTK(KERN_ERR"Failed to add DSG filter - failed to create entry in the list\n");
        return -EINVAL;
    }

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_add(&new->list, pos->prev);
    numDSGFilters ++;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    DeletePpSessionsOfCorrelatingDsgFilters(newFilter);
    ti_hil_pp_event(TI_DOCSIS_VOICE_SESSIONS_DEL, NULL);
#endif

    return 0;
}

static int UpdateDsgFilter(FilterDSG_T *filterData)
{
    Uint32 lockKey;
    DsgFilterListEntry_T *entry;

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, filterData->DSGIndex);

    /* find a filter with the same index */
    if (GetDsgListEntryByIndex(filterData->DSGIndex, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to update DSG filter - index %d not found\n", filterData->DSGIndex);
        return -EINVAL;
    }

    /* update the data */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    SetDsgFilterData(entry, filterData);
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    return 0;
}

static int DelDsgFilter(Uint32 index)
{
    Uint32 lockKey;
    DsgFilterListEntry_T *entry;
#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    FilterDSG_T delFilter;
#endif

    DPRINTK(KERN_DEBUG"%s. Filter index = %u\n",__FUNCTION__, index);

    if (!numDSGFilters) 
    {
        DPRINTK(KERN_WARNING"Failed to delete DSG filter - the DSG table is empty\n");
        return -EINVAL;
    }
    if (GetDsgListEntryByIndex(index, &entry) != 0)
    {
        DPRINTK(KERN_WARNING"Failed to delete DSG filter - index %d not found\n", index);
        return -EINVAL;
    }

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
    GetDSGFilter(index, &delFilter);
    DeletePpSessionsOfCorrelatingDsgFilters(&delFilter);
#endif

    /* delete current entry */
    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
    list_del(&entry->list);
    numDSGFilters --;

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    PAL_osMemFree(0, entry, sizeof(DsgFilterListEntry_T));

    return 0;
}

static int GetDsgFilter(Uint32 index, FilterDSG_T *buf)
{
    DsgFilterListEntry_T *entry;

    if (GetDsgListEntryByIndex(index, &entry) != 0)
    {
        DPRINTK(KERN_INFO"DSG filter %d not found\n", index);
        return -EINVAL;
    }
    PAL_osMemCopy(buf, &(entry->filter), sizeof(FilterDSG_T));
    return 0;

}

static int GetDsgMatches(Uint32 index, Uint8 *matches)
{
    DsgFilterListEntry_T *entry;
    if (GetDsgListEntryByIndex(index, &entry) != 0)
    {
        DPRINTK(KERN_INFO"DSG filter %d not found\n", index);
        return -EINVAL;
    }
    PAL_osMemCopy(matches, &entry->filter.DSGMatches, sizeof(DSGMatch));
    return 0;
}

static int GetPpDSGMatches(Uint32 index, Uint8 *ppMatches)
{
    Uint32 lockKey;
    DsgFilterListEntry_T *entry;
    Uint64 currSessionsBytes = 0;
    DppCountSessionEntry_t *currSessionEntry = NULL;
    DppFilterCounters_t ppCouner;

    memset(&ppCouner, 0, sizeof(ppCouner));
    ppCouner.activeSessionsList = NULL;

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* get matched of dead sessions */
    if (GetDsgListEntryByIndex(index, &entry) != 0) 
    {
        DPRINTK(KERN_INFO"DSG filter %d not found\n", index);
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
        return -EINVAL;
    }
  
    ppCouner.ppMatches = entry->filter.DSGPpCounters.ppMatches;
    ppCouner.ppBytes = entry->filter.DSGPpCounters.ppBytes;

    /* get matched of current sessions */
    if (entry->filter.DSGPpCounters.activeSessionsList != NULL)
    {
        currSessionEntry = entry->filter.DSGPpCounters.activeSessionsList;

        while (currSessionEntry != NULL)
        {
#ifdef CONFIG_MACH_PUMA5
            TI_PP_SESSION_STATS             sessionStats;
            if (ti_ppm_get_session_stats(currSessionEntry->sessionHandle, &sessionStats) < 0)
#else
            AVALANCHE_PP_SESSION_STATS_t    sessionStats;
            if (PP_RC_SUCCESS != avalanche_pp_get_stats_session(currSessionEntry->sessionHandle, &sessionStats))
#endif
            {
                DPRINTK(KERN_INFO"Failed to get session %d statistics for DSG filter index %d\n", 
                        currSessionEntry->sessionHandle, entry->filter.DSGIndex);
                PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
                return -EINVAL;
            }

            ppCouner.ppMatches += sessionStats.packets_forwarded;

#ifdef CONFIG_MACH_PUMA5
            currSessionsBytes = sessionStats.bytes_forwarded_hi;
            currSessionsBytes <<= 32;
            currSessionsBytes += sessionStats.bytes_forwarded_lo;
#else
            currSessionsBytes = sessionStats.bytes_forwarded;
#endif
            ppCouner.ppBytes  += currSessionsBytes;

            /* add the 4 bytes of checksum the PP is ignoring */
            ppCouner.ppBytes += (4 * sessionStats.packets_forwarded);

            currSessionEntry = currSessionEntry->dsgFilterInfo.nextSession;
        }
    }

    PAL_osMemCopy(ppMatches, &ppCouner, sizeof(DppFilterCounters_t));
    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);
	return 0;
}

static DFltrAction_t MatchDsgFilter(struct sk_buff *skb)
{
    Uint32 lockKey;
    struct list_head *pos;
    DFltrAction_t action;
    DsgFilterListEntry_T   *entry;
    FilterDSG_T *dsgFilter;
    DSG_DB_IEEE_header* layer2_IEEE_Header;
    DSG_DB_Layer2_header* layer2Header;
    DSG_DB_Layer2_LLC_header* layer2_llcHeader;
    unsigned short packetProtocol;
    char* iPHeader;
    char* l4Header;
    char* packet;
    unsigned long len;

    action = DFLTR_DISCARD;
    packet = skb->data;
    len = skb->len + 4;

    /* go over the filter table and try to find a match */
    list_for_each(pos, &dsgFiltersListHead)
    {
        entry = list_entry(pos, DsgFilterListEntry_T, list);
        dsgFilter = &(entry->filter);
        /* First we check the priority */
        if((dsgFilter->DSGPriority) < 0)
        {
            DPRINTK(KERN_INFO"%s: Priority of this entry is < 0, continue...\n", __FUNCTION__);
            continue;
        }

        /* Then we test destination MAC against filter MAC */
        if(!(MACCMP(packet, dsgFilter->DSGMacAddress)))
            continue;

        /* No filter - this indicates that the tunnel was opened with no filter
          in which case, we must pass it */
        if(dsgFilter->DSGDestIpAddr == 0)
        {
            action = DFLTR_ACCEPT;
            break;
        }

        /* Now extract L3 protocol and L3 header */
        layer2Header = (DSG_DB_Layer2_header*)packet;
        if (layer2Header->Type_Length > 1500)
        {
            /* Simple L2 header */
            packetProtocol = layer2Header->Type_Length;
            iPHeader = packet + sizeof(DSG_DB_Layer2_header);

            if ( packetProtocol == 0x8100 )
            {
                /* VLAN header adds 4 bytes */
                layer2_IEEE_Header = (DSG_DB_IEEE_header*)packet;
                packetProtocol = layer2_IEEE_Header->Type_Length;
                iPHeader += 4;
            }
        }
        else
        {
            layer2_llcHeader = (DSG_DB_Layer2_LLC_header*)(packet + sizeof(DSG_DB_Layer2_header));
            packetProtocol = layer2_llcHeader->Type;
            iPHeader = (char*)(layer2_llcHeader + sizeof(DSG_DB_Layer2_LLC_header));
        }

        /* Not IP packet */
        if(packetProtocol != 0x800)
        {
            DPRINTK(KERN_INFO"%s: Not IP packet\n", __FUNCTION__);
            continue;
        }

        /* IP dest match */
        if(*(unsigned long*)(iPHeader + IP_DEST_ADDRESS_FIELD) != dsgFilter->DSGDestIpAddr)
        {
            DPRINTK(KERN_INFO"%s: IP dest don't match\n", __FUNCTION__);
            continue;
        }

        /* IP source match */
        if(dsgFilter->DSGSrcIpAddr)
        {
            if( (*(unsigned long*)(iPHeader + IP_SRC_ADDRESS_FIELD) & dsgFilter->DSGSrcIpMask) !=
                (dsgFilter->DSGSrcIpAddr & dsgFilter->DSGSrcIpMask))
            {
                DPRINTK(KERN_INFO"%s: IP source don't match\n", __FUNCTION__);
                continue;
            }
        }

        /* TCP/UDP port match */
        l4Header = iPHeader + ((*(unsigned char*)(iPHeader) & 0x0f) * 4);
        if( (*(unsigned short*)(l4Header + 2) < dsgFilter->DSGDestPortStart) ||
           (*(unsigned short*)(l4Header + 2) > dsgFilter->DSGDestPortEnd) )
        {
            DPRINTK(KERN_INFO"%s: Port range don't match\n", __FUNCTION__);
            continue;
        }

        /* if we are here - we have a match */
        PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);
        dsgFilter->DSGMatches.pkts++;
        dsgFilter->DSGMatches.octets += len;
#ifdef CONFIG_TI_PACKET_PROCESSOR_STATS
      if (skb->pp_packet_info.flags & TI_HIL_PACKET_FLAG_PP_SESSION_INGRESS_RECORDED)
      {
          skb->pp_packet_info.ti_match_dsg_filter = dsgFilter;   
          DPRINTK(KERN_DEBUG"set skb->pp_packet_info.ti_match_dsg_filter = %p\n", dsgFilter);
      }
#endif
        PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

        DPRINTK(KERN_DEBUG"%s: DSG filter %u matched, len = %d\n", __FUNCTION__, dsgFilter->DSGIndex,len);
        action = DFLTR_ACCEPT;
        break;
    }

    if(action == DFLTR_DISCARD)
        DPRINTK(KERN_DEBUG"%s: DSG filter no matched found, discard...\n", __FUNCTION__);
    return action;
}

static DFltrAction_t ApplyDSGFilter(struct sk_buff *skb)
{
    return MatchDsgFilter(skb);
}

#ifdef DO_SELECTIVE_PP_SESSIONS_DEL_ON_FILTER_CHANGE
/**************************************************************************/
/*! \fn static int DeletePpSessionsOfCorrelatingDsgFilters(FilterLLC_T *llcFilter1)
 **************************************************************************
 *  \brief Delets all PP sessions based on filter rules matching to the given filter
 *  \param[in] FilterLLC_T *llcFilter1 - filter to base the search on 
 *  \return  0 or error code
 */
static int DeletePpSessionsOfCorrelatingDsgFilters(FilterDSG_T *dsgFilter1)
{
    struct list_head *pos;
    Uint32 lockKey;
    DsgFilterListEntry_T *entry;
    FilterDSG_T *dsgFilter2;
    Uint32 numSessionsFound = 0;
    DppCountSessionEntry_t *currSessionEntry;
    Uint32 *sessList;

    /* Allocate memory for the PP sessions list. */
	/* We need to allocate MAX_SESSIONS + 1 from the reason that MAX_SESSIONS[0] saved for numSessionsFound */
    sessList = (Uint32 *)kmalloc(sizeof(Uint32)*(MAX_SESSIONS+1), GFP_ATOMIC);
    if (sessList == NULL)
    {
        printk(KERN_ERR "%s kmalloc failed\n", __FUNCTION__);
        return -ENOMEM;
    }

    PAL_osProtectEntry(PAL_OSPROTECT_INTERRUPT, &lockKey);

    /* go over the filter table and try to find a match */
    list_for_each(pos, &dsgFiltersListHead)
    {
        entry = list_entry(pos, DsgFilterListEntry_T, list);
        dsgFilter2 = &(entry->filter);


        /* check if the source interface in the filter matche */
        if (dsgFilter1->DSGIndex != dsgFilter2->DSGIndex)
        {
            continue;
        }

        /* Found a matching filter, get the PP sessions list */
        currSessionEntry = dsgFilter2->DSGPpCounters.activeSessionsList;
        while (currSessionEntry != NULL)
        {
            Uint32 i;
            /* We start from entry 1 and not 0 since entry 0 in sessList is reserved for saving numSessionsFound */
            for (i = 1; i <= numSessionsFound; i++)
            {
                if (sessList[i] == currSessionEntry->sessionHandle)
                {
                    DPRINTK("Session %d already in the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, i);
                    break;
                }
            }
            if (i == numSessionsFound + 1)
            {
                numSessionsFound++;
                sessList[numSessionsFound] = currSessionEntry->sessionHandle;
                DPRINTK("Adding session %d to the TBD sessions list (at entry %d)\n", currSessionEntry->sessionHandle, numSessionsFound);
            }

            currSessionEntry = currSessionEntry->dsgFilterInfo.nextSession;
        }

    }

    PAL_osProtectExit(PAL_OSPROTECT_INTERRUPT, lockKey);

    if (numSessionsFound)
    {
        DPRINTK("Sending HIL event to delete %d sessions in the TBD sessions list\n", numSessionsFound);

        sessList[0] = numSessionsFound;
        ti_hil_pp_event(TI_DOCSIS_SESSIONS_DEL, (void *)sessList);
    }

    /* Free the session buffer memory */
    kfree(sessList);

    return 0;
}
#endif

static int CreateNewDsgEntry(DsgFilterListEntry_T **entry, FilterDSG_T *filter)
{
    PAL_Result res;

    res = PAL_osMemAlloc(0, sizeof(DsgFilterListEntry_T), 0, (Ptr *)entry);
    if (res != PAL_SOK)
    {
        DPRINTK(KERN_ERR"%s: Failed to allocate dynamic memory\n",__FUNCTION__);
        return -1;
    }

    SetDsgFilterData(*entry, filter);

    return 0;
}

static void SetDsgFilterData(DsgFilterListEntry_T *entry, FilterDSG_T *filter)
{

#ifdef DFILTER_DEBUG	
    PrintDsgFltrData(filter);
#endif

	filter->DSGPpCounters.ppMatches = 0;
    filter->DSGPpCounters.ppBytes = 0;
	filter->DSGPpCounters.activeSessionsList = NULL;
    PAL_osMemCopy(&(entry->filter), filter, sizeof(FilterDSG_T));


}

static void PrintDsgFltrData(FilterDSG_T *filter)
{
	printk("DSG filter %u data:\n", filter->DSGIndex);
	printk("DSGPriority = %d\n", filter->DSGPriority);
	printk("DSGSrcIpAddr = 0x%.8x\n", filter->DSGSrcIpAddr);
	printk("DSGSrcIpMask = 0x%.8x\n",  filter->DSGSrcIpMask);
	printk("DSGDestIpAddr = 0x%.8x\n", filter->DSGDestIpAddr);
	printk("DSGDestPortStart = %u\n", filter->DSGDestPortStart);
	printk("DSGDestPortEnd = %u\n", filter->DSGDestPortEnd);
	printk("DSGTunnelPkts = %u\n", filter->DSGMatches.pkts);
	printk("DSGTunnelOctets = %u\n", filter->DSGMatches.octets);
	printk("DSGMacAddress = %02x:%02x:%02x:%02x:%02x:%02x\n", filter->DSGMacAddress[0], filter->DSGMacAddress[1],filter->DSGMacAddress[2],filter->DSGMacAddress[3],filter->DSGMacAddress[4],filter->DSGMacAddress[5]);
	printk("-------------------------------- \n\n");
			
}



/**************************************************************************/
/*! \fn static int PrintAllDsgFilters()
 **************************************************************************
 *  \brief Print All DSG filters
 *  \param[in]   no inputs
 *  \param[out]  no outputs
 *  \return  0 
 */
static int PrintAllDsgFilters()
{
	struct list_head *pos;
    DsgFilterListEntry_T *curr=NULL;
	printk("\n========== DSG Filters ========= \n\n");
    list_for_each(pos, &dsgFiltersListHead)
    {
		curr = list_entry (pos, DsgFilterListEntry_T, list);
		PrintDsgFltrData (&(curr->filter));
    }
	return 0;
}

#endif //DSG

// ARRIS ADD
void DFilter_UpdateIpFilterCmciMask(unsigned long long sysCmciIfBitmap)
{
    struct list_head *pos;
    IpFilterListEntry_T   *entry;
    FilterIP_T *ipFilter;

    /* go over the filter table */
    list_for_each(pos, &ipFiltersListHead)
    {
       entry = list_entry(pos, IpFilterListEntry_T, list);
       ipFilter = &(entry->filter);

       if (ipFilter->IPIfIndex == DFLTR_IFACE_CUSTSIDE_ETH) 
       {   
           ipFilter->IPIfIndexMask = sysCmciIfBitmap;
       }
    }

    return;

}

void DFilter_UpdateCustSideSysIfIndex(Uint32 sysIfIndex)
{
    custSideSysIfIndex = sysIfIndex;
    DPRINTK("Update custSideSysIfIndex to %u\n", custSideSysIfIndex);
    return;
}
// ARRIS ADD END
