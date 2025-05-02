/*
 *
 * dbridge_main.c
 * Description:
 * DOCSIS bridge main implementation
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

#define _DBRIDGE_MAIN_C_

/*! \file dbridge_main.c
    \brief the docsis bridge main module
*/
/**************************************************************************/
/*      architecture overview                                             */
/**************************************************************************/


/*                                       ------------------------------
                                        |                              |
                                        |        Dual IP stack         |
                                        |                              |
                                         ------------------------------
                                           |           |            |
                                           |           |            |
                                         ------      ------      ------
                                        |      |    |      |    |      |
                                        | WAN0 |    | LAN0 |    | MTA0 |
                                        |      |    |      |    |      |
                                         ------      ------      ------
                                           |           |            |
                                           |           |            |
    --------------     -------     ------------------------------   |
   |              |   |       |   |                              |  |
   | Local Bridgr |---| DBR00 |---|        Docsis Bridge         |  |
   |              |   |       |   |                              |  |
    --------------     -------     ------------------------------   |
       |       |                       |              |             |
       |       |                       |              |             |
    ------   ------                  ------   -------------------   |
   |      | |      |                |      | |                   |  |
   | ETH0 | | USB0 |                | CNI0 | |   Esafe adaptor   |--
   |      | |      |                |      | |                   |
    ------   ------                  ------  |-------------------
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/ti_hil.h>


#include "dbridge_main.h"
#include "dbridge_db.h"
#include "dbridge_netdev.h"
#include "dbridge_igmp.h"
#include "dbridge_snif.h"
#include "dbridge_esafe.h"
#include "dbridge_mcast.h"
// ARRIS ADD START
#include <linux/route.h>
#include <asm/processor.h>
//#include "status.h"
// ARRIS ADD END
#include "dbridge_mdf_db.h"
#include "dbridge_l2vpn_ds_db.h"
#include "dbridge_mac_ageing.h"
#include "dbridge_common.h"
#include "dbridge_frwd_rules.h"

/*SERCOMM ADD START*/
#include <linux/if_arp.h>
/*SERCOMM ADD END*/


/* work around for multicast counters */
#define IFINDEX_PRIMARY_CPE             1

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/

#ifdef DBRIDGE_LOG
extern unsigned int dbridgeDbgLevel;
#endif

#ifdef CONFIG_TI_DEVICE_PROTOCOL_HANDLING
extern int ti_protocol_handler (struct net_device* dev, struct sk_buff *skb);
#endif

extern struct net_device * lbr0Dev;
#ifdef CONFIG_MACH_PUMA6
extern unsigned  char AtomMac[6];           // ARRIS ADD
#endif

/* IPTV Bridge CB */
extern int (*IpTvBridge_ReceiveCB)(struct sk_buff *skb);
#if 0
// ARRIS ADD START
extern int inet_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg);
// ARRIS ADD END
#endif
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/


#define FLOOD_TO_ALL_INTERFACE 0xffffffff
/* #define YAMUNA_MUSTER */


/*! \var typedef enum McasrType_e
    \brief enum defines the Mcast type
*/
typedef enum McasrType_e
{
    NOT_MCAST,
    MCAST_IGMP,
    MCAST_DATA
} McasrType_e;

const char ETHER_L2CP_PREFIX[] = {0x01, 0x80, 0xc2, 0x00, 0x00};

#define CNID_DSID_INDEX_BIT_FIELD_OFFSET    24

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

static DbridgeCounters_t*  ptr_dbridge_counters;


/*! \fn void Dbridge_floodPacket (struct sk_buff* skb,unsigned int flood_map,unsigned,
 *  \fn                           unsigned int filter_map,unsigned int classifier_map,
 *  \fn                           unsigned int alt_update_map)
 *  \brief floods the packet flowing the input parameters rules
 *  \param[in] skb.
 *  \param[in] flood_map = map the destanation index the packet send to.
 *  \param[in] filter_map = map on which destanation index to perform filter.
 *  \param[in] filter_map = map on which destanation index to perform classifier.
 *  \return 0-Success, <0-Error.
 */
static void Dbridge_floodPacket (struct sk_buff* skb,unsigned int flood_map,
                                 unsigned int filter_map,unsigned int classifier_map);

/*! \fn void DBridge_registered_receive(struct sk_buff* skb)
 *  \brief The function handle the packet according to Docsis spec
 *  \brief for registered mode
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 */
static void DBridge_registered_receive(struct sk_buff* skb);

/*! \fn void DDBridge_pre_registration_receive(struct sk_buff* skb)
 *  \brief The function handle the packet according to Docsis spec
 *  \brief for pre-registration and naco 0 mode.
 *  \param[in] skb.
 *  \param[in] pointer to  Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_pre_registration_receive(struct sk_buff* skb);

/*! \fn void DBridge_standby_receive(struct sk_buff* skb)
 *  \brief The function handle the packet according to Docsis spec
 *  \brief for standby mode.
 *  \param[in] skb.
 *  \param[in] pointer to  Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_standby_receive(struct sk_buff* skb);


/*! \fn void DBridge_regHandlePacketFromCmci(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 *  \brief The function handle the packet from CMCI according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromCMCI(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn void DBridge_regHandlePacketFromESAFE(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 *  \brief The function handle the packet from eSAFE according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromESAFE(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn void DBridge_regHandlePacketFromCable(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 *  \brief The function handle the packet from cable according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromCable(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 *  \brief The function handle the packet from Lan IP according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 *  \brief The function handle the packet from Wan IP according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 */
static void DBridge_regHandlePacketFromWanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);

/*! \fn static unsigned int  DBridge_Mcast_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev)
 *  \brief   verify if this is Mcast handle the packet and returne the Mcast type
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] flood_map.
 *  \param[in] mcast_filter_type.
 *  \param[out] none
 *  \return Macst rev type (MCAST_IGMP, MCAST_DATA or NOT_MCAST).
 */
static unsigned int  DBridge_Mcast_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev,
                                           unsigned int flood_map, unsigned int mcast_filter_type);

/*! \fn static int DBridge_MDF_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev,
                                       unsigned int flood_map)
 *  \brief verify if this is Mcast packet and handle the packet according to MDF logic.
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] flood_map.
 *  \param[out] none
 *  \return Macst rev type (MCAST_IGMP, MCAST_DATA or NOT_MCAST).
 */
static int  DBridge_MDF_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev,
                                unsigned int flood_map);


/*! \fn static int DBridge_Multicast_L2VPN_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev)
 *  \brief Handle the packet according to L2VPN logic.
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] saidIndex.
 *  \param[out] none
 *  \return TBD
 */
static int DBridge_Multicast_L2VPN_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev, bool isUnicastUnknownDestination);


// ARRIS ADD START 
/*! \fn static void dbridgeSetSockAddr(struct sockaddr_in *sin, u32 addr, u16 port)

 *  \brief     Api to build routing table add message
 *  \param[in] *sin
 *  \param[in] addr
 *  \param[in] port
 *  \return none
 */
#if 0
static void dbridgeSetSockAddr(struct sockaddr_in *sin, u32 addr, u16 port);

/*! \fn static void dbridgeAddRoute(unsigned int pc_addr)

 *  \brief     Add entry to routing table
 *  \param[in] pc_addr
 *  \return none
 */

static void dbridgeAddRoute(unsigned int pc_addr);
// ARRIS ADD END 
#endif

#if 0
#ifdef CONFIG_TI_DOCSIS_EGRESS_HOOK
static int Dbridge_egress_hook(struct sk_buff* skb);
static struct net_device *usb0_netdev;
static struct net_device *eth0_netdev;

#endif /* CONFIG_TI_DOCSIS_EGRESS_HOOK */
#endif

#ifdef DSG
static void DBridge_regHandlePacketFromDSGI(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb);
static void DBridge_DSGTunnel_handler(struct sk_buff* skb);
#endif /* DSG */
void Dbridge_SendIgmpToCable(struct sk_buff* skb);
void Dbridge_SendIgmpToCpe(struct sk_buff* skb);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
// ARRIS MODIFY - added okToIncClassMib parameter
static int (*Dbridge_L2vpnDataCB)(struct sk_buff *skb, int *l2vpnRelated, int okToIncClassMib) = NULL;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/



/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/
/* TODO Change to cfg*/
#define IGMP_DEFAULT_MQI_TIMEOUT        125
#define IGMP_DEFAULT_QRI_TIMEOUT        100
#define IGMP_M1_MAX_TIMEOUT             3
#define IGMP_DOCSIS_ROBUSTNESS          2

/**************************************************************************/
/*! \fn int Dbridge_Init(void)
 **************************************************************************
 *  \brief DOCSIS bridge dinitialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
int Dbridge_Init(void)
{
    struct igmp_cfg igmp_cfg;

    if (DbridgeDb_Init())
        return -1; /* NOK*/

    if (DbridgeEsafe_Init())
        return -1; /* NOK*/

    if (Dbridge_NetDevInit())
        return -1; /* NOK*/

    if (DbridgeMdfDb_Init())
        return -1; /* NOK*/

    if (DbridgeL2vpnDsDb_Init())
        return -1; /* NOK*/

    if (DbridgeMacAgeing_Init())
        return -1; /* NOK*/

    igmp_cfg.enabled=0;
    igmp_cfg.igmp_docsis_robustness = IGMP_DOCSIS_ROBUSTNESS;
    igmp_cfg.igmp_m1_max_timeout =IGMP_M1_MAX_TIMEOUT;
    igmp_cfg.igmp_mqi_timeout=IGMP_DEFAULT_MQI_TIMEOUT;
    igmp_cfg.igmp_qri_timeout=IGMP_DEFAULT_QRI_TIMEOUT;
    igmp_cfg.ti_igmp_send_packet=(void*)Dbridge_SendIgmpPacket;
    igmp_cfg.ti_igmp_message_processed= Dbridge_messageProcessed;
    if (ti_igmp_init(&igmp_cfg))
	{
        return -1; /* NOK*/
	}

    /* IGMP init */
    ptr_dbridge_counters= Dbridge_GetCounters();
    return 0; /* OK*/
}




/**************************************************************************/
/*! \fn void DBridge_receive (struct sk_buff *skb)
 **************************************************************************
 *  \brief The function is the entry point by which packets are pushed into
 *  \brief the DOCSIS Bridge.
 *  \brief NOTE: Before calling this function ensure that the MAC RAW Pointer in the
 *  \brief SKB is valid and points to the start of the MAC header.
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
void DBridge_receive (struct sk_buff *skb)
{
    struct ethhdr*     ptr_ethhdr;
    unsigned int dbridge_mode;
    Bool validEntry;

    /* Get the Ethernet header. */
    ptr_ethhdr = eth_hdr(skb);
    if (ptr_ethhdr == NULL)
    {
        /* No Ethernet header; drop the packet and clean the memory. */
#ifdef DBRIDGE_DBG
        printk (KERN_INFO "Debuge : No Ethernet header; dropping packet\n");
#endif
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
    }

    /* in some cases the inpute_dev is null, docsis bridge use this information */
    /* therefore in these case we set the input_dev to be equal to dev */
     if (!skb->skb_iif)
        skb->skb_iif = skb->dev->ifindex;

    /* ti_docsis_input_dev include information in case of esafe */
    /* don't overwrite this information */
    if(skb->ti_docsis_input_dev == NULL)
    {
        skb->ti_docsis_input_dev = dev_get_by_index (&init_net,skb->skb_iif);
        if (skb->ti_docsis_input_dev)
        {
            DBRIDGE_IFINDEX_CHK(skb->ti_docsis_input_dev->ifindex,validEntry, "dev %p, devname %s, ti_docsis_input_dev %p, ti_docsis_input_dev->name %s", skb->dev, skb->dev ? skb->dev->name : NULL, skb->ti_docsis_input_dev, skb->ti_docsis_input_dev->name);
            if (validEntry == False)
            {
                BUG();
            }
            dev_put(skb->ti_docsis_input_dev);
        }
    }

    /* IPTV Bridge hook */
    if (IpTvBridge_ReceiveCB)
    {
        if (IpTvBridge_ReceiveCB(skb) == 0)
        {
            return; /* This is an IPTV pkt ... Stop Docsis packet processing */
        }
    }


#ifdef CONFIG_TI_DEVICE_PROTOCOL_HANDLING
    /* Pass the packet to the device specific protocol handler */
    if (ti_protocol_handler (skb->dev, skb) < 0)
    {
        /* Device Specific Protocol handler has "captured" the packet
         * and does not want to send it up the networking stack; so
         * return immediately. */
        //cable_pp: don't do it, return NET_RX_SUCCESS;
    }
#endif /* CONFIG_TI_DEVICE_PROTOCOL_HANDLING */
    dbridge_mode = DbridgeDb_GetMode();

    DBRIDGE_IFINDEX_CHK(skb->dev->ifindex,validEntry, "dev %p, devname %s", skb->dev, skb->dev ? skb->dev->name : NULL);
    if (validEntry == False)
        {
        BUG();
        }

    switch (dbridge_mode)
    {
    case DBR_OFF_MODE:
        dbridgeAddDiscards(skb);
#ifdef DBRIDGE_DBG
        printk (KERN_INFO "Docsis bridge off - drop  Packet 0x%p\n",skb);
#endif
        kfree_skb (skb);
        return;
    case DBR_REGISTERED_MODE:
        DBridge_registered_receive(skb);
        break;
    case DBR_PRE_REGISTRATION_MODE:
    case DBR_NACO_OFF_MODE:
        DBridge_pre_registration_receive(skb);
        break;
    case DBR_STANDBY_MODE:
        DBridge_standby_receive(skb);
        break;
    default:
        return;
    }
    return;
}

// ARRIS MODIFY - added okToIncClassMib parameter
/**************************************************************************/
/*! \fn int Dbridge_register_L2vpnDataCB(int (*Dbridge_L2vpnData)(struct sk_buff *skb,int *l2vpnRelated,int okToIncClassMib))
 **************************************************************************
 *  \brief register the l2vpn data CB function
 *  \param[in] the l2vpn data CB function
 *  \return OK
 */
// ARRIS MODIFY - added okToIncClassMib parameter
int Dbridge_register_L2vpnDataCB(int (*Dbridge_L2vpnData)(struct sk_buff *skb,int *l2vpnRelated,int okToIncClassMib))
{
    Dbridge_L2vpnDataCB = Dbridge_L2vpnData;
    return 0;
}

/**************************************************************************/
/*! \fn void Dbridge_CableRecive (struct sk_buff *skb)
 **************************************************************************
 *  \brief
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
void Dbridge_CableRecive (struct sk_buff *skb)
{
       /* This is the DOCSIS Interface; pass the packet to the DOCSIS Bridge; initialize the
        * MAC RAW Pointer before doing so. */


       skb_reset_mac_header(skb);
       DBridge_receive (skb);

}

/*SERCOMM ADD START*/
/**************************************************************************/
/*! \fn int lbr0_hack(struct sk_buff *skb, unsigned int *flood_map) 
 **************************************************************************
 *  \brief If the target ip = 192.168.100.1, only flooding the packet to lan0
 *  \param[in] skb.
 *  \param[in] flood_map.
 *  \return 1-Success, 0-Error.
 **************************************************************************/
static int lbr0_hack(struct sk_buff *skb, unsigned int *flood_map)
{
    __be16 proto;
    const char lbr0_dev_name[] = "lbr0";
    unsigned int lan0ip = ((192<<24) | (168<<16) | (100<<8) | (1));

    if (!skb || !(skb->dev))
    {
        return 0;
    }

    proto = skb->protocol;

    if((proto == ETH_P_ARP) && (0 == memcmp(skb->dev->name, lbr0_dev_name, sizeof(lbr0_dev_name))))
    {
        struct arphdr *arp = NULL;
        unsigned char *target_ip_ptr = NULL;
        unsigned int target_ip;

        arp = arp_hdr(skb);

        if(arp->ar_pro != ETH_P_IP)
        {
            return 0;
        }

        if (arp->ar_op != ARPOP_REQUEST)
        {
            return 0;
        }

        /* Forward packets that have a target IP of 192.168.100.1 (lan0) */
        target_ip_ptr = (unsigned char *)((unsigned char *)arp + sizeof(struct arphdr) + (2*ETH_ALEN) + sizeof(unsigned int));

        memcpy(&target_ip, target_ip_ptr, 4);

    	if( target_ip == lan0ip )
	{            
            *flood_map = DBR_LAN_IP_NET_DEV_TYPE; 
            return 1;
        }
    }

    return 0;
}
/*SERCOMM ADD END*/

/**************************************************************************/
/*! \fn void Dbridge_floodPacket (struct sk_buff* skb,unsigned int flood_map,unsigned,
 *  \fn                            unsigned int filter_map,unsigned int classifier_map,
 *  \fn                            unsigned int alt_update_map, DbridgeDevice_t* ptr_dbridge_src_dev)
 **************************************************************************
 *  \brief floods the packet flowing the input parameters rules
 *  \param[in] skb.
 *  \param[in] flood_map = map the destanation index the packet send to.
 *  \param[in] filter_map = map on which destanation index to perform filter.
 *  \param[in] filter_map = map on which destanation index to perform classifier.
 *  \return 0-Success, <0-Error.
 **************************************************************************/
static void Dbridge_floodPacket (struct sk_buff* skb,unsigned int flood_map,
                                 unsigned int filter_map, unsigned int classifier_map)
{
    struct sk_buff* skbclone;
    int    index = 0;
    int    num_of_device = 0;
    DbridgeDevice_t*   ptr_dbridge_dev;

    /*SERCOMM ADD START*/
    /*If the target ip = 192.168.100.1, only flooding the packet to lan0*/
    lbr0_hack(skb, &flood_map);    
    /*SERCOMM ADD END*/

    num_of_device=DbridgeDb_GetNumOfDevice();
    /* Cycle through all the entries. */
    for (index = 0; index < num_of_device; index++)
    {

        ptr_dbridge_dev = DbridgeDb_DevGetByIndex(index);
        /* Do we get the packet on this interface? */
        if (skb->dev == ptr_dbridge_dev->ptr_interface)
            continue;
        /* clone the packet onlt to map interface */
        if (ptr_dbridge_dev->net_dev_type & flood_map)
        {
            unsigned int fc_flags=DBR_NO_FILTER_CLASS_FLAG;
            /* Clone the packet. */
            skbclone = skb_clone(skb, GFP_ATOMIC);
            if (skbclone == NULL)
            {
                /* Memory Problems: Clean the original SKB before leaving. */
#ifdef DBRIDGE_DBG
                printk (KERN_ERR "Cloning Failed for Device %s\n", ptr_dbridge_dev->ptr_interface->name);
#endif
                dbridgeAddDiscards(skb);
                kfree_skb(skb);
                return;
            }
            /* Initialize the packet structure to indicate the device on which the packet will be transmitted */

            skbclone->dev = ptr_dbridge_dev->ptr_interface;

            /* if the packet is for CMCI, change ti_selective_fwd_dev_info to have only local bridge interfaces mask */
            if(ptr_dbridge_dev->net_dev_type == DBR_CMCI_NET_DEV_TYPE)
            {
                skbclone->ti_selective_fwd_dev_info &= DbridgeDB_GetlbridgeInterfaceMask();
            }
            
            /* if packet is L2VPN related, mark the PP bypass for Esafe/Wan0/Lan0 so they will not create NULL session */
            if( (skb->ti_meta_info & CNID_L2VPN_RELATED) &&
                ((ptr_dbridge_dev->net_dev_type == DBR_ESAFE_NET_DEV_TYPE) ||
                 (ptr_dbridge_dev->net_dev_type == DBR_WAN_IP_NET_DEV_TYPE)|| 
                 (ptr_dbridge_dev->net_dev_type == DBR_LAN_IP_NET_DEV_TYPE)) )
            {
                skbclone->pp_packet_info.flags |= TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS;
            }


                /* we filter only packet from cable or CMCI */
            if(ptr_dbridge_dev->net_dev_type & filter_map)
            {
                if(skbclone->ti_selective_fwd_dev_info ==0)
                {
                    if(ptr_dbridge_dev->net_dev_type == DBR_CMCI_NET_DEV_TYPE)
                    {
                        /* the destination is CMCI */
                        skbclone->ti_selective_fwd_dev_info = DbridgeDB_GetlbridgeInterfaceMask();
                    }
                    else
                    {
                        /* the destination is Cable */
                        skbclone->ti_selective_fwd_dev_info = (1LL << ((skbclone->dev->ifindex)-1));
                    }
                }
                fc_flags |= DBR_FILTER_FLAG;
            }

            if (ptr_dbridge_dev->net_dev_type & classifier_map)
            {
                fc_flags |= DBR_CLASSIFIER_FLAG;
            }


            /* Push the packet via the DOCSIS Device */
#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX_FLOOD)
            {
                printk("\nDBRIDGE_DBG_REG_TX_FLOOD FROM %s TO %s",skb->dev->name, skbclone->dev->name);
                DBridge_logPacket(skbclone);
            }
#endif

            ptr_dbridge_dev->push(skbclone,fc_flags);

            /* update counters */

        }
    }
    /* Cleanup the original packet. */
    kfree_skb (skb);
    return;
}


/**************************************************************************/
/*! \fn void DBridge_registered_receive(struct sk_buff* skb)
 **************************************************************************
 *  \brief The function handle the packet according to Docsis spec
 *  \brief for registered mode
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_registered_receive(struct sk_buff* skb)
{

    DbridgeDevice_t*   ptr_dbridge_dev;

    unsigned int       src_dev_type;

    Bool validEntry;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RECIVE)
    {
        printk("\nDBRIDGE_DBG_REG_RECIVE");
        DBridge_logPacket(skb);
    }
#endif

    /* Get the DOCSIS Device information. */
    ptr_dbridge_dev = DbridgeDb_DevGetByInterface (skb->dev);
    if (ptr_dbridge_dev == NULL)
    {
        /* Packet was passed from a device not attached to the DOCSIS Bridge. */
#ifdef DBRIDGE_DBG
        printk (KERN_WARNING "No DOCSIS Device; packet from %s\n", skb->dev->name);
#endif
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
    }

    src_dev_type = ptr_dbridge_dev->net_dev_type;

    DBRIDGE_IFINDEX_CHK(skb->dev->ifindex,validEntry, "dev %p, devname %s", skb->dev, skb->dev ? skb->dev->name : NULL);
    if (validEntry == False)
    {
        BUG();
    }

    switch(src_dev_type)
    {
    case DBR_CABLE_NET_DEV_TYPE:
        DBridge_regHandlePacketFromCable(ptr_dbridge_dev,skb);
        break;
    case DBR_CMCI_NET_DEV_TYPE:
        DBridge_regHandlePacketFromCMCI(ptr_dbridge_dev,skb);
        break;
    case DBR_ESAFE_NET_DEV_TYPE:
        DBridge_regHandlePacketFromESAFE(ptr_dbridge_dev,skb);
        break;

    case DBR_WAN_IP_NET_DEV_TYPE:
        DBridge_regHandlePacketFromWanIP(ptr_dbridge_dev,skb);
        break;
    case DBR_LAN_IP_NET_DEV_TYPE:
#ifndef YAMUNA_MUSTER
        DBridge_regHandlePacketFromLanIP(ptr_dbridge_dev,skb);
#else
        /* patch to transfer packet from lan to cable for master slave protocol */
        DBridge_regHandlePacketFromWanIP(ptr_dbridge_dev,skb);
#endif
        break;
#ifdef DSG
    case DBR_DSGI_NET_DEV_TYPE:
        DBridge_regHandlePacketFromDSGI(ptr_dbridge_dev,skb);
        break;
#endif
    default:
        break;
    }
    return;
}


/**************************************************************************/
/*! \fn void DDBridge_pre_registration_receive(struct sk_buff* skb)
 **************************************************************************
 *  \brief The function handle the packet according to Docsis spec
 *  \brief for pre-registration and naco0 mode
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_pre_registration_receive(struct sk_buff* skb)
{
    struct ethhdr*     ptr_ethhdr;
    DbridgeDevice_t*   ptr_dbridge_dev;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    unsigned int       src_dev_type  = 0;
    unsigned int       flood_map = DBR_WAN_IP_NET_DEV_TYPE;
    unsigned int       Mcast_type;
    unsigned int       mdfMode = 0;
    DBRALT_PROT_DCL(lockKey);
#ifdef DSG
    unsigned int  mcast_filter_type = MCAST_FILTER_NOT_ACTIVE;
    DbridgeMcastMac_t* mcast_filter_info;
#endif
    unsigned int       rules_to_apply; // ARRIS ADD

    /* Get the Ethernet header. */
    ptr_ethhdr = eth_hdr(skb);
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RECIVE)
    {
        printk("\n DBRIDGE_DBG_PRE_REG_RECIVE");
        DBridge_logPacket(skb);
    }
#endif

    /* Get the DOCSIS Device information. */
    ptr_dbridge_dev = DbridgeDb_DevGetByInterface (skb->dev);
    if (ptr_dbridge_dev == NULL)
    {
        /* Packet was passed from a device not attached to the DOCSIS Bridge. */
#ifdef DBRIDGE_DBG
        printk (KERN_WARNING "1No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
    }
    src_dev_type = ptr_dbridge_dev->net_dev_type;

    switch (src_dev_type)
    {
    case DBR_CABLE_NET_DEV_TYPE:
#ifdef DBRIDGE_LOG

    if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
    {
        printk("\nDBRIDGE_DBG_PREREG_RX_CABLE");
        DBridge_logPacket(skb);
    }
#endif
    if( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {   /* brodcast or Mcast packet */
        if (!MAC_ISBROADCAST(ptr_ethhdr, h_dest))
        {
            /* if MDF is enabled */
            DbridgeMdfDb_GetMdfMode(&mdfMode);
            if (mdfMode == DBR_MDF_ENABLE_MODE)
            {
                Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_dev, flood_map);
    #ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk("\nDBridge_pre_registration_receive: DBRIDGE MDF Mode is enabled\n");
                }
    #endif
                if (Mcast_type == MCAST_DATA)
                {
                    return;
                }
            }

            /* if MDF is disabled */
            else
            {
                /*     Mcast filtering */
    #ifdef DSG
                if((mcast_filter_info = DbridgeDb_GetMcastByMac(ptr_ethhdr->h_dest)) == NULL)
    #else
                if(DbridgeDb_GetMcastByMac(ptr_ethhdr->h_dest) == NULL)
    #endif
                {
                    /* Unknown destination or illegal destination */
    #ifdef DBRIDGE_LOG
                    if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
                    {
                        printk (KERN_WARNING "No Filter Drop Mcast packet from %s\n", skb->dev->name);
                        DBridge_logPacket(skb);
                    }
    #endif
                    dbridgeAddDiscards(skb);
                    kfree_skb (skb);
                    return;
                }
    #ifdef DSG
                else
                {
                    if(DbridgeDB_IsDsgLoobEnabled())
                    {
                        mcast_filter_type = mcast_filter_info->Type;
                        if (DBridge_Mcast_handler(skb,ptr_dbridge_dev,FLOOD_TO_ALL_INTERFACE,mcast_filter_type)==NOT_MCAST)
                        {
                            dbridgeAddInFrames(skb);
                            Dbridge_floodPacket(skb,FLOOD_TO_ALL_INTERFACE, DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE,
                            DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE);
                        }                  
                        return;
                   }
              }
    #endif
            }
        }
    }

    /* at this stage alt table can point to LAN_IP or WAN_IP only */
        DBRALT_PROT_ON(lockKey);
        ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
        if (ptr_dbridge_alt != NULL)
        {
            if(ptr_dbridge_alt->dbridgeDevice != NULL)
            {
                DbridgeDevice_t *dbridge_dev = ptr_dbridge_alt->dbridgeDevice;

                /* Got what we wanted, can rempve protection, the DBR devices are NEVER (yet...) removed */
                DBRALT_PROT_OFF(lockKey);

                /* Match Found: Send the packet only on that interface */
                /* update counter */
                dbridgeAddInFrames(skb);
                skb->dev = dbridge_dev->ptr_interface;

                dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);
                return;
            }
        }
        DBRALT_PROT_OFF(lockKey);
        /* No Match found; flood the packet to LAN and WAN ip interface */
        dbridgeAddInFrames(skb);
        Dbridge_floodPacket(skb,DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE,0,0);
        return;

#ifdef DSG
        case DBR_DSGI_NET_DEV_TYPE:
#endif
        case DBR_CMCI_NET_DEV_TYPE:
        // ARRIS ADD
        // This is to allow access to the local IP from any subnet. If this is an IPv4 packet going to
        // 192.168.100.1, the dest MAC may or may not be wrong since it could be the MAC of the CPE's
        // gateway. So let's overwrite the dest MAC to match the LAN MAC.
        if( src_dev_type == DBR_CMCI_NET_DEV_TYPE && ptr_ethhdr->h_proto == ETH_P_IP )
        {
            struct iphdr *iph = (struct iphdr*)skb_network_header(skb);
            
            if( iph->daddr == ((192<<24) | (168<<16) | (100<<8) | (1)) )
            {
#if 0
                // Check if request is from the 192.168.100 network
                if( (iph->saddr & 0xffffff00) != ((192<<24) | (168<<16) | (100<<8)) )
                {
                    // Add PC entry to the routing table
                    // This is required if the PC is not in the same subnet as the CM
                    dbridgeAddRoute(iph->saddr);
                }
#endif
                // get the MAC for the LAN interface and overwrite the dest MAC
                ptr_dbridge_dev = DbridgeDb_DevGetByType(DBR_LAN_IP_NET_DEV_TYPE);
                memcpy( ptr_ethhdr->h_dest, ptr_dbridge_dev->ptr_interface->dev_addr, ETH_ALEN );

                //printk (KERN_INFO "Packet from CMCI to 192.168.100.1 from %x\n", iph->saddr);
            }
        }
        // END ARRIS ADD

/*
  The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
   HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from the CMCI ports;
*/
     // ARRIS allow LAN TFTP if told to do so
     if( DBridge_TftpAllowed() ) 
     {
         rules_to_apply = TI_FWRD_RULE_FROM_CPE_ALLOW_TFTP;
     }
     else
     {
         rules_to_apply = TI_FWRD_RULE_FROM_CPE;
     }
     if( apply_fwrd_rule( NULL, rules_to_apply, TI_DHCPv4_DHCPOFFER_IND, TI_ROUTER_ADVERTISEMENTS_IND, skb) )
     {
     // END ARRIS
#ifdef DBRIDGE_DBG
         if(dbridgeDbgLevel)
         {
             printk ("CM Pre-Operational: The CM MUST NOT accept this packet from the CMCI ports\n");
             DBridge_logPacket(skb);
         }
#endif
        dbridgeAddForwardDiscards(skb);
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
     }
        /* drop to the next case */
    case DBR_ESAFE_NET_DEV_TYPE:
        /* at this stage alt table can point to LAN_IP or WAN_IP only */
        DBRALT_PROT_ON(lockKey);
        ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
        if (ptr_dbridge_alt != NULL)
        {
            if(ptr_dbridge_alt->dbridgeDevice != NULL)
            {
                DbridgeDevice_t *dbridge_dev = ptr_dbridge_alt->dbridgeDevice;

                /* Got what we wanted, can rempve protection, the DBR devices are NEVER (yet...) removed */
                DBRALT_PROT_OFF(lockKey);

                /* Match Found: Send the packet only on that interface */
                /* update counter */
                dbridgeAddInFrames(skb);
                skb->dev = dbridge_dev->ptr_interface;

                dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);
                return;
            }
        }
        DBRALT_PROT_OFF(lockKey);
        // ARRIS MOD
        // if we did not match ALT table above, then must not be for CM since those will always
        // be in the table. So if packet is not broadcast/multicast, free it below.
        if( ptr_ethhdr->h_dest[0] & 0x01 )
        {
            /* No Match found; flood the packet to LAN and WAN ip interface */
            dbridgeAddInFrames(skb);
            Dbridge_floodPacket(skb,DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE,0,0);
        }
        else
        {
            dbridgeAddDiscards(skb);
            kfree_skb (skb);
        }
        // ARRIS MOD END
        return;

    case DBR_WAN_IP_NET_DEV_TYPE:

        /* send the packet to cable interface only */
        ptr_dbridge_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
        dbridgeAddInFrames(skb);

        skb->dev = ptr_dbridge_dev->ptr_interface;
#ifdef DBRIDGE_LOG
        if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
        {
            printk("BRIDGE_DBG_PREREG_RX_CABLE FROM %s TO %s","wan0",skb->dev->name);
            DBridge_logPacket(skb);
        }
#endif
        ptr_dbridge_dev->push(skb,DBR_CLASSIFIER_FLAG);

        return;

    case DBR_LAN_IP_NET_DEV_TYPE:
    /* send the packet to CMCI interface only */
/*
   The CM MUST NOT send any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or Request,
   TFTP-RRQ, HTTP Request, Time Protocol Request, or IPv6 Router Solicitation messages to any interface
   except the RF Interface;
*/
         // ARRIS allow LAN TFTP if told to do so
         if( DBridge_TftpAllowed() ) 
         {
             rules_to_apply = TI_FWRD_RULE_IPSTACK_TO_RF_ALLOW_TFTP;
         }
         else
         {
             rules_to_apply = TI_FWRD_RULE_IPSTACK_TO_RF;
         }
         if( apply_fwrd_rule( NULL, rules_to_apply, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb) )
         {
         // END ARRIS
#ifdef DBRIDGE_DBG
             if(dbridgeDbgLevel)
             {
                 printk ("CM Pre-Operational: The CM MUST NOT send this messages to any interface except the RF Interface\n");
                 DBridge_logPacket(skb);
             }
#endif
            dbridgeAddForwardDiscards(skb);
            dbridgeAddDiscards(skb);
            kfree_skb (skb);
            return;
         }
        ptr_dbridge_dev = DbridgeDb_DevGetByType(DBR_CMCI_NET_DEV_TYPE);
        dbridgeAddInFrames(skb);

#ifdef DSG /* send the packet to DSGI interface too */
        DbridgeDevice_t*   ptr_dbridge_dsg_dev;
        struct sk_buff* skbclone;
        if(DbridgeDB_IsDsgLoobEnabled())
        {
            ptr_dbridge_dsg_dev = DbridgeDb_DevGetByType(DBR_DSGI_NET_DEV_TYPE);
            skbclone = skb_clone(skb, GFP_ATOMIC);
            if (skbclone == NULL)
            {
        #ifdef DBRIDGE_DBG
                printk (KERN_ERR "Cloning Failed for Device %s\n", ptr_dbridge_dsg_dev->ptr_interface->name);
        #endif
                dbridgeAddDiscards(skb);
                kfree_skb(skb);
                return;
            }
            skbclone->dev = ptr_dbridge_dsg_dev->ptr_interface;
        #ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
            {
                printk("\nDBRIDGE_DBG_REG_TX FROM lan0 TO %s",skbclone->dev->name);
                DBridge_logPacket(skb);
            }
        #endif

            ptr_dbridge_dsg_dev->push(skbclone,DBR_NO_FILTER_CLASS_FLAG);
        }
#endif
        skb->dev = ptr_dbridge_dev->ptr_interface;
#ifdef DBRIDGE_LOG
        if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
        {
            printk("BRIDGE_DBG_PREREG_RX_CABLE FROM %s TO %s","lan0",skb->dev->name);
            DBridge_logPacket(skb);
        }
#endif

        ptr_dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);


        return;

    default:
        return;
    }
}


/**************************************************************************/
/*! \fn void DBridge_standby_receive(struct sk_buff* skb)
 **************************************************************************
 *  \brief The function handle the packet according to Docsis spec
 *  \brief for standby mode
 *  \param[in] skb.
 *  \param[out] no output.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_standby_receive(struct sk_buff* skb)
{
    struct ethhdr*     ptr_ethhdr;
    DbridgeDevice_t*   ptr_dbridge_dev;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    unsigned int       src_dev_type  = 0;
    unsigned int       dest_dev_type = 0;
    unsigned int       flood_map = DBR_WAN_IP_NET_DEV_TYPE;
    unsigned int       Mcast_type;
    unsigned int       mdfMode = 0;
    DBRALT_PROT_DCL(lockKey);
    unsigned int       rules_to_apply; // ARRIS ADD

    /* Get the Ethernet header. */
    ptr_ethhdr = eth_hdr(skb);
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RECIVE)
    {
        printk("\n DBRIDGE_DBG_PRE_REG_RECIVE");
        DBridge_logPacket(skb);
    }
#endif

    /* Get the DOCSIS Device information. */
    ptr_dbridge_dev = DbridgeDb_DevGetByInterface (skb->dev);
    if (ptr_dbridge_dev == NULL)
    {
        /* Packet was passed from a device not attached to the DOCSIS Bridge. */
#ifdef DBRIDGE_DBG
        printk (KERN_WARNING "1No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
    }
    src_dev_type = ptr_dbridge_dev->net_dev_type;

    switch (src_dev_type)
    {
    case DBR_CABLE_NET_DEV_TYPE:
#ifdef DBRIDGE_LOG

    if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
    {
        printk("\nDBRIDGE_DBG_PREREG_RX_CABLE");
        DBridge_logPacket(skb);
    }
#endif


    ptr_ethhdr = eth_hdr(skb);
    if( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {   /* brodcast or Mcast packet */
        if (!MAC_ISBROADCAST(ptr_ethhdr, h_dest))
        {
            /* if MDF is enabled */
            DbridgeMdfDb_GetMdfMode(&mdfMode);
            if (mdfMode == DBR_MDF_ENABLE_MODE)
            {
                Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_dev, flood_map);
    #ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk("\nDBridge_pre_registration_receive: DBRIDGE MDF Mode is enabled\n");
                }
    #endif
                if (Mcast_type == MCAST_DATA)
                {
                    return;
                }
            }

            /* if MDF is disabled */
            else
            {
                /*     Mcast filtering */
                if(DbridgeDb_GetMcastByMac(ptr_ethhdr->h_dest) == NULL)
                {
                    /* Unknown destination or illegal destination */
    #ifdef DBRIDGE_LOG
                    if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
                    {
                        printk (KERN_WARNING "No Filter Drop Mcast packet from %s\n", skb->dev->name);
                        DBridge_logPacket(skb);
                    }
    #endif
                    dbridgeAddDiscards(skb);
                    kfree_skb (skb);
                    return;
                }
            }
        }
    }

        DBRALT_PROT_ON(lockKey);
        ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
        if (ptr_dbridge_alt != NULL)
        {
            if(ptr_dbridge_alt->dbridgeDevice != NULL)
            {
                DbridgeDevice_t *dbridge_dev = ptr_dbridge_alt->dbridgeDevice;

                /* Got what we wanted, can rempve protection, the DBR devices are NEVER (yet...) removed */
                DBRALT_PROT_OFF(lockKey);

                dest_dev_type = dbridge_dev->net_dev_type;
                /* in standby mode we forword packet from cable IF to LAN or WAN IF only*/
                if((DBR_WAN_IP_NET_DEV_TYPE == dest_dev_type) || (DBR_LAN_IP_NET_DEV_TYPE == dest_dev_type))
                {
                    dbridgeAddInFrames(skb);
                    skb->dev = dbridge_dev->ptr_interface;

                    dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);
                }
                else
                {
                    dbridgeAddDiscards(skb);
                    kfree_skb (skb);
                }
                return;
            }
        }
        DBRALT_PROT_OFF(lockKey);
        /* No Match found; flood the packet to LAN and WAN ip interface */
        dbridgeAddInFrames(skb);
        Dbridge_floodPacket(skb,DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE,0,0);
        return;

        case DBR_CMCI_NET_DEV_TYPE:
/*
  The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
   HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from the CMCI ports;
*/
     // ARRIS allow LAN TFTP if told to do so
     if( DBridge_TftpAllowed() ) 
     {
         rules_to_apply = TI_FWRD_RULE_FROM_CPE_ALLOW_TFTP;
     }
     else
     {
         rules_to_apply = TI_FWRD_RULE_FROM_CPE;
     }
     if( apply_fwrd_rule( NULL, rules_to_apply, TI_DHCPv4_DHCPOFFER_IND, TI_ROUTER_ADVERTISEMENTS_IND, skb) )
     {
     // END ARRIS
#ifdef DBRIDGE_DBG
         if(dbridgeDbgLevel)
         {
             printk ("CM Pre-Operational: The CM MUST NOT accept this packet from the CMCI ports\n");
             DBridge_logPacket(skb);
         }
#endif
        dbridgeAddForwardDiscards(skb);
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
     }
        /* drop to the next case */
    case DBR_ESAFE_NET_DEV_TYPE:
        DBRALT_PROT_ON(lockKey);
        ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
        if (ptr_dbridge_alt != NULL)
        {
            if(ptr_dbridge_alt->dbridgeDevice != NULL)
            {
                DbridgeDevice_t *dbridge_dev = ptr_dbridge_alt->dbridgeDevice;

                /* Got what we wanted, can rempve protection, the DBR devices are NEVER (yet...) removed */
                DBRALT_PROT_OFF(lockKey);

                dest_dev_type = dbridge_dev->net_dev_type;
                /* in standby mode we forword packet from CMCI or ESAFE IF to LAN or WAN IF only*/
                if((DBR_WAN_IP_NET_DEV_TYPE == dest_dev_type) || (DBR_LAN_IP_NET_DEV_TYPE == dest_dev_type))
                {
                    dbridgeAddInFrames(skb);
                    skb->dev = dbridge_dev->ptr_interface;

                    dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);
                }
                else
                {
                    dbridgeAddDiscards(skb);
                    kfree_skb (skb);
                }
                return;
            }
        }
        DBRALT_PROT_OFF(lockKey);
        /* No Match found; flood the packet to LAN and WAN ip interface */
        dbridgeAddInFrames(skb);
        Dbridge_floodPacket(skb,DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE,0,0);
        return;

    case DBR_WAN_IP_NET_DEV_TYPE:

        /* send the packet to cable interface only */
        ptr_dbridge_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
        dbridgeAddInFrames(skb);

        skb->dev = ptr_dbridge_dev->ptr_interface;
#ifdef DBRIDGE_LOG
        if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
        {
            printk("BRIDGE_DBG_PREREG_RX_CABLE FROM %s TO %s","wan0",skb->dev->name);
            DBridge_logPacket(skb);
        }
#endif

        ptr_dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);

        return;

    case DBR_LAN_IP_NET_DEV_TYPE:
    /* send the packet to CMCI interface only */
/*
   The CM MUST NOT send any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or Request,
   TFTP-RRQ, HTTP Request, Time Protocol Request, or IPv6 Router Solicitation messages to any interface
   except the RF Interface;
*/
        // ARRIS allow LAN TFTP if told to do so
        if( DBridge_TftpAllowed() ) 
        {
            rules_to_apply = TI_FWRD_RULE_IPSTACK_TO_RF_ALLOW_TFTP;
        }
        else
        {
            rules_to_apply = TI_FWRD_RULE_IPSTACK_TO_RF;
        }
        if( apply_fwrd_rule( NULL, rules_to_apply, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb) )
        {
        // END ARRIS
#ifdef DBRIDGE_DBG
             if(dbridgeDbgLevel)
             {
                 printk ("CM Pre-Operational: The CM MUST NOT send this messages to any interface except the RF Interface\n");
                 DBridge_logPacket(skb);
             }
#endif
            dbridgeAddForwardDiscards(skb);
            dbridgeAddDiscards(skb);
            kfree_skb (skb);
            return;
         }
        ptr_dbridge_dev = DbridgeDb_DevGetByType(DBR_CMCI_NET_DEV_TYPE);
        dbridgeAddInFrames(skb);

        skb->dev = ptr_dbridge_dev->ptr_interface;
#ifdef DBRIDGE_LOG
        if(dbridgeDbgLevel & DBRIDGE_DBG_PREREG_RX_CABLE)
        {
            printk("BRIDGE_DBG_PREREG_RX_CABLE FROM %s TO %s","lan0",skb->dev->name);
            DBridge_logPacket(skb);
        }
#endif

        ptr_dbridge_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);


        return;

    default:
        return;
    }
}


/**************************************************************************/
/*! \fn static bool isL2vpnL2CPFilterPacket (Uint8* destAddress)
 **************************************************************************
 *  \brief The function checks if the destination address of US L2VPN packet should be filtered 
 *         according to L2 Control Protocol requirements of L2VPN spec
 *  \param[in] destAddress - the destination address of the packet.
 *  \return true - filter. false - farward
**************************************************************************/
static bool isL2vpnL2CPFilterPacket (Uint8* destAddress)
{

    if (memcmp(destAddress, ETHER_L2CP_PREFIX, 5) == 0)
    {
        if (( destAddress[5] >= 0x01 && destAddress[5] <= 0x0A ) || (destAddress[5] == 0x0E))
        {
            return true;
        }
    }
    return false;
}


/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromCmci(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 **************************************************************************
 *  \brief The function handle the packet from CMCI according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
**************************************************************************/
static void DBridge_regHandlePacketFromCMCI(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
    /* check packet ip and perform mac override */
    struct ethhdr*     ptr_ethhdr;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    DbridgeDevice_t*   ptr_dbridge_dest_dev;
    unsigned int       dest_dev_type = 0;
    unsigned int       flood_map = FLOOD_TO_ALL_INTERFACE;
    unsigned int       Mcast_type = NOT_MCAST;
    unsigned int       mdfMode = 0;
    int                l2vpnPacket = 0;
    bool               rf_only = false;
    bool               skip_cpe_table_check = false; // ARRIS ADD
    DBRALT_PROT_DCL(lockKey);
    DbridgeDevice_t *dbridge_dev;
    unsigned int       rules_to_apply; // ARRIS ADD
    unsigned int       l2vpn_prom_mode = 0; // ARRIS ADD

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_CMCI)
    {
        printk("\nDBRIDGE_DBG_REG_RX_CMCI");
        DBridge_logPacket(skb);
    }
#endif

    // ARRIS ADD - start
    // Check if it's an L2VPN packet and if L2VPN promiscuous mode is enabled.
    // If so, don't check or update the CPE table.  Allow the packet to be transmitted.
    l2vpn_prom_mode = DbridgeDB_GetL2vpnPromMode();

    if (Dbridge_L2vpnDataCB)
    {        
        // Don't increment the Classifier MIB counter since this is only a test
        // to see if this is an L2VPN packet.
        Dbridge_L2vpnDataCB(skb, &l2vpnPacket, False);
    }

    if (l2vpn_prom_mode && l2vpnPacket)
    {
        skip_cpe_table_check = true;
    }
    // ARRIS ADD - end

    /********************************/
    /* Lookup destanation interface */
    /********************************/

    /* Get the Ethernet header. */
    ptr_ethhdr = eth_hdr(skb);
    if ( ptr_ethhdr->h_dest[0] & 0x1 )
    {
/*
  The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
   HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from any of the CPE ports
*/
        rf_only = apply_fwrd_rule( NULL, TI_FWRD_RULE_IPSTACK_TO_RF, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb);
        /* Update ALT table  */

        /* ARRIS MODIFY - start */
        if (!skip_cpe_table_check)
        {
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY
            /* ARRIS MODIFY START */
            //if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev,skb->nfmark)!=0)
            if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev, 0)!=0)
            /* ARRIS MODIFY END */    
#else
            if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev)!=0)
#endif
        {
            /* CPE table is full don't flood the packet to cable interface */
#ifdef DBRIDGE_DBG
            printk (KERN_INFO "CPE table is full don't flood the packet to cable interface %s\n", skb->dev->name);
#endif
            if(!rf_only)
                flood_map ^= DBR_CABLE_NET_DEV_TYPE;
            else
            {
                if(dbridgeDbgLevel)
                    {
                    printk ("CM Operational: The CM MUST NOT send this messages to any interface except the RF Interface\n");
                    DBridge_logPacket(skb);
                }

                dbridgeAddForwardDiscards(skb);
                dbridgeAddDiscards(skb);
                kfree_skb (skb);
                return;
            }
        }
        } 
        /* ARRIS MODIFY - end (added check for skip_cpe_table_check, gateway) */

        /* Chack if this packet was already checked to be L2VPN related */
        if (! (skb->ti_meta_info & US_L2VPN_CHECKED))
        {
            /* The packet was not checked to be L2VPN related. Let's check now */
            if (Dbridge_L2vpnDataCB)
            {
                Dbridge_L2vpnDataCB(skb,&l2vpnPacket, True); //ARRIS MODIFY - added okToIncClassMib parameter
            }
        }
        l2vpnPacket = skb->ti_meta_info & CNID_L2VPN_RELATED;

        /*skip igmp for L2VPN packet*/
        if (!l2vpnPacket)
        {
            DbridgeMdfDb_GetMdfMode(&mdfMode);
            /* if MDF is enabled */
            if (mdfMode == DBR_MDF_ENABLE_MODE)
            {
#ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk("\nDBridge_regHandlePacketFromCMCI: DBRIDGE MDF Mode is enabled\n");
                }
#endif
                //Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_src_dev, flood_map);
                if(rf_only)
                    flood_map = DBR_CABLE_NET_DEV_TYPE;
                else
                    flood_map = DBR_CABLE_NET_DEV_TYPE | DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE;

                // ARRIS ADD BEGIN    
                /* Filter packet from ATOM Mac on the models which do not program unique ATOM Mac address.*/
#ifdef CONFIG_MACH_PUMA6
                if (memcmp(ptr_ethhdr->h_source, AtomMac, 6) == 0)
                {
                    // Not allow flood to Cable RF interface
                    printk(KERN_INFO "\nAtom mac matched <2>\n");
                    flood_map &= (~DBR_CABLE_NET_DEV_TYPE);
                }
#endif
                // ARRIS ADD  END

                dbridgeAddInFrames(skb);
                Dbridge_floodPacket(skb, flood_map, flood_map ,flood_map);
                Mcast_type = MCAST_DATA;
            }

            /* if MDF is disabled */
            else
            {
                /* viserify if we need to handle this IGMP packet */
                Mcast_type = DBridge_Mcast_handler(skb,ptr_dbridge_src_dev,flood_map, MCAST_FILTER_NOT_ACTIVE);
            }
        }
        else
        {
            /* L2VPN US packet, check L2 Control Protocol mac address and act according to L2VPN spec requirements */	
            if (isL2vpnL2CPFilterPacket(ptr_ethhdr->h_dest))
            {
                dbridgeAddForwardDiscards(skb);
                dbridgeAddDiscards(skb);
                kfree_skb (skb);
                return;
            }
        }
        if (Mcast_type==NOT_MCAST)
        {
            /* broadcast packet */
            if(rf_only)
                flood_map = DBR_CABLE_NET_DEV_TYPE;
            dbridgeAddInFrames(skb);
            Dbridge_floodPacket(skb,flood_map,DBR_CABLE_NET_DEV_TYPE ,DBR_CABLE_NET_DEV_TYPE);
        }

        return;
    }
    


    // ARRIS ADD
    // This is to allow access to the local IP from any subnet. If this is an IPv4 packet going to
    // 192.168.100.1, the dest MAC may or may not be wrong since it could be the MAC of the CPE's
    // gateway. So let's overwrite the dest MAC to match the LAN MAC.
    if( ptr_ethhdr->h_proto == ETH_P_IP )
    {
        struct iphdr *iph = (struct iphdr*)skb_network_header(skb);

        if( iph->daddr == ((192<<24) | (168<<16) | (100<<8) | (1)) )
        {
#if 0
            // Check if request is from the 192.168.100 network
            if( (iph->saddr & 0xffffff00) != ((192<<24) | (168<<16) | (100<<8)) )
            {
                // Add PC entry to the routing table
                // This is required if the PC is not in the same subnet as the CM
                dbridgeAddRoute(iph->saddr);
            }
#endif
            // get the MAC for the LAN interface and overwrite the dest MAC
            ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_LAN_IP_NET_DEV_TYPE);
            memcpy( ptr_ethhdr->h_dest, ptr_dbridge_dest_dev->ptr_interface->dev_addr, ETH_ALEN );

            //printk (KERN_INFO "Packet from CMCI to 192.168.100.1 from %x\n", iph->saddr);
        }
    }
    // END ARRIS ADD

  DBRALT_PROT_ON(lockKey);
  ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
  if (ptr_dbridge_alt != NULL)
  {
      dbridge_dev = ptr_dbridge_alt->dbridgeDevice;
      /* Got what we wanted, can rempve protection, the DBR devices are NEVER (yet...) removed */
      DBRALT_PROT_OFF(lockKey);

     dest_dev_type = dbridge_dev->net_dev_type;
     switch (dest_dev_type)
     {
     case DBR_CABLE_NET_DEV_TYPE:
        
        break; 
     case DBR_ESAFE_NET_DEV_TYPE:
        
        break; 
     case DBR_WAN_IP_NET_DEV_TYPE: 
     case DBR_LAN_IP_NET_DEV_TYPE:
			/*  
		    The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
		    HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from any of the CPE ports
			*/  
            // ARRIS allow LAN TFTP if told to do so
            if( DBridge_TftpAllowed() ) 
            {
                rules_to_apply = TI_FWRD_RULE_FROM_CPE_ALLOW_TFTP;
            }
            else
            {
                rules_to_apply = TI_FWRD_RULE_FROM_CPE;
            }
			if ( apply_fwrd_rule( NULL, rules_to_apply, TI_DHCPv4_DHCPOFFER_IND, TI_ROUTER_ADVERTISEMENTS_IND, skb) )
            {
            // END ARRIS
				#ifdef DBRIDGE_DBG
	    		if(dbridgeDbgLevel)
				{
					printk ("CM Operational: The CM MUST NOT accept this packet from any of the CPE ports\n");
					DBridge_logPacket(skb);
				}
				#endif
				dbridgeAddForwardDiscards(skb);
				dbridgeAddDiscards(skb);
				kfree_skb (skb);
				return;
			}

    
			break; 
		default:
#ifdef DBRIDGE_LOG
			printk (KERN_WARNING "2No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
			dbridgeAddDiscards(skb);
			kfree_skb (skb);
			return;
		}
		
	} else
	{
        DBRALT_PROT_OFF(lockKey);
		/* Unknown destination we send the packet to CABLE interface */
		
		/* performer filter - ToDo */
		/* performer classifier - ToDo */
		
		/* Update ALT table with src MAC address */

        /* ARRIS MODIFY - start */
        if (!skip_cpe_table_check)
        {
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY
            /* ARRIS MODIFY ADD */
            //if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev,skb->nfmark)!=0)
            if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev,0)!=0)
            /* ARRIS MODIFY END*/    
#else
            if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev)!=0)
#endif
            {
#ifdef DBRIDGE_DBG
                printk (KERN_INFO "CPE table if full drop %s\n", skb->dev->name);
#endif
                dbridgeAddDiscards(skb);
                kfree_skb (skb);
                return;
            }
		} 
        /* ARRIS MODIFY - end (added check for skip_cpe_table_check) */

        ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
        dbridgeAddInFrames(skb);

        skb->dev = ptr_dbridge_dest_dev->ptr_interface;
        skb->ti_selective_fwd_dev_info = (1LL << ((skb->dev->ifindex)-1));

#ifdef DBRIDGE_LOG
#ifdef DSG
        if((dbridgeDbgLevel & DBRIDGE_DBG_REG_TX) || (dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_DSGI))
#else
        if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
#endif
        {
            printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
            DBridge_logPacket(skb);
        }
#endif


        ptr_dbridge_dest_dev->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
        return;
    }

    dbridgeAddInFrames(skb);
    skb->dev = dbridge_dev->ptr_interface;
    skb->ti_selective_fwd_dev_info = (1LL << ((skb->dev->ifindex)-1));

#ifdef DBRIDGE_LOG
#ifdef DSG
    if((dbridgeDbgLevel & DBRIDGE_DBG_REG_TX) || (dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_DSGI))
#else
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
#endif
    {
        printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif

    dbridge_dev->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
    return;
}
#ifdef DSG
/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromDSGI(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 **************************************************************************
 *  \brief The function handle the packet from DSGI according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
**************************************************************************/
static void DBridge_regHandlePacketFromDSGI(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
    /* check packet ip and perform mac override */


    struct ethhdr*     ptr_ethhdr;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    DbridgeDevice_t* ptr_dbridge_dest_dev;
    unsigned int       dest_dev_type = 0;
    unsigned int       flood_map = FLOOD_TO_ALL_INTERFACE;
    unsigned int       Mcast_type;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_DSGI)
    {
        printk("\nDBRIDGE_DBG_REG_RX_DSGI");
        DBridge_logPacket(skb);
    }
#endif
    /********************************/
    /* Lookup destanation interface */
    /********************************/

    /* Get the Ethernet header. */
    ptr_ethhdr = eth_hdr(skb);
    if ( ptr_ethhdr->h_dest[0] & 0x1 )
    {
        /* Update ALT table  */
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS MODIFY BEGIN
        if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_ESAFE, ptr_dbridge_src_dev,skb->ti_docsis_input_dev, 0)!=0)
#else
        if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_ESAFE, ptr_dbridge_src_dev,skb->ti_docsis_input_dev)!=0)
#endif // ARRIS MODIFY END
        {
            /* CPE table is full don't flood the packet to cable interface */
            flood_map ^= DBR_CABLE_NET_DEV_TYPE;
#ifdef DBRIDGE_DBG
            printk (KERN_INFO "CPE table is full don't flood the packet to cable interface %s\n", skb->dev->name);
#endif
        }

        /* viserify if we need to handle this IGMP packet */
        Mcast_type = DBridge_Mcast_handler(skb,ptr_dbridge_src_dev,flood_map,MCAST_FILTER_NOT_ACTIVE);
        if (Mcast_type==NOT_MCAST)
        {
            /* broadcast packet */
            dbridgeAddInFrames(skb);
            Dbridge_floodPacket(skb,flood_map,DBR_CABLE_NET_DEV_TYPE ,DBR_CABLE_NET_DEV_TYPE);
        }

        return;

    }
    ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
    if (ptr_dbridge_alt != NULL)
    {
        dest_dev_type = ptr_dbridge_alt->dbridgeDevice->net_dev_type;
        switch (dest_dev_type)
        {
            case DBR_CABLE_NET_DEV_TYPE:
                break;
            case DBR_ESAFE_NET_DEV_TYPE:
                break;
            case DBR_WAN_IP_NET_DEV_TYPE:
            case DBR_LAN_IP_NET_DEV_TYPE:
            /* performer Hacker protection - need to check if perform for lan and wan */
                break;
            default:
#ifdef DBRIDGE_LOG
                printk (KERN_WARNING "2No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
            dbridgeAddDiscards(skb);
            kfree_skb (skb);
            return;
        }

    }
    else
    {
        /* Unknown destination we send the packet to CABLE interface */

        /* performer filter - ToDo */
        /* performer classifier - ToDo */

        /* Update ALT table with src MAC address */

#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS MODIFY BEGIN
        if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_ESAFE, ptr_dbridge_src_dev,skb->ti_docsis_input_dev, 0)!=0)
#else
        if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_ESAFE, ptr_dbridge_src_dev,skb->ti_docsis_input_dev)!=0)
#endif //ARRIS MODIFY END           
        {
#ifdef DBRIDGE_DBG
            printk (KERN_INFO "CPE table if full drop %s\n", skb->dev->name);
#endif
            dbridgeAddDiscards(skb);
            kfree_skb (skb);
            return;
        }

        ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
        dbridgeAddInFrames(skb);

        skb->dev = ptr_dbridge_dest_dev->ptr_interface;
        skb->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((skb->dev->ifindex)-1));

#ifdef DBRIDGE_LOG
        if((dbridgeDbgLevel & DBRIDGE_DBG_REG_TX) || (dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_DSGI))
        {
            printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
            DBridge_logPacket(skb);
        }
#endif


        ptr_dbridge_dest_dev->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
        return;
    }

    dbridgeAddInFrames(skb);
    skb->dev = ptr_dbridge_alt->dbridgeDevice->ptr_interface;
    skb->ti_selective_fwd_dev_info = ((unsigned long) 1 << ((skb->dev->ifindex)-1));

#ifdef DBRIDGE_LOG
    if((dbridgeDbgLevel & DBRIDGE_DBG_REG_TX) || (dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_DSGI))
    {
        printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif

    ptr_dbridge_alt->dbridgeDevice->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
    return;
}
#endif

/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromESAFE(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 **************************************************************************
 *  \brief The function handle the packet from eSAFE according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_regHandlePacketFromESAFE(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
    /* check packet ip and perform mac override */
    struct ethhdr*     ptr_ethhdr;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    DbridgeDevice_t* ptr_dbridge_dest_dev;
    unsigned int       dest_dev_type = 0;
    unsigned int       flood_map = FLOOD_TO_ALL_INTERFACE  ^ (DBR_WAN_IP_NET_DEV_TYPE | DBR_LAN_IP_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE);
    unsigned int       Mcast_type = NOT_MCAST;
    unsigned int       mdfMode = 0;
    int                l2vpnPacket = 0;
    DBRALT_PROT_DCL(lockKey);
    DbridgeDevice_t *dbridge_dev;
    struct net_device*  sourceInterface = NULL;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_CMCI)
    {
        printk("\nDBRIDGE_DBG_REG_RX_ESAFE");
        DBridge_logPacket(skb);
    }
#endif

    /********************************/
    /* Lookup destanation interface */
    /********************************/

    /* Get the Ethernet header. */
    ptr_ethhdr = eth_hdr(skb);

    if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {
        /* Update ALT table  */
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS ADD
                /* ARRIS MODIFY BEGIN */
		//if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev,skb->nfmark)!=0)
		if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev, 0)!=0)
                /* ARRIS MODIFY END */    
#else        
		if (DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev)!=0)
#endif // ARRIS ADD END                  
        {
            /* CPE table is full don't flood the packet to cable interface */
            flood_map ^= DBR_CABLE_NET_DEV_TYPE;
#ifdef DBRIDGE_DBG
            printk (KERN_INFO "CPE table is full don't flood the packet to cable interface %s\n", skb->dev->name);
#endif
        }

        if (Dbridge_L2vpnDataCB)
        {
            Dbridge_L2vpnDataCB(skb,&l2vpnPacket, True); // ARRIS MODIFY - added okToIncClassMib parameter
        }
        /*skip igmp for L2VPN packet*/
        if (!l2vpnPacket)
        {
            DbridgeMdfDb_GetMdfMode(&mdfMode);
            /* if MDF is enabled */
            if (mdfMode == DBR_MDF_ENABLE_MODE)
            {
#ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk("\nDBridge_regHandlePacketFromESAFE: DBRIDGE MDF Mode is enabled\n");
                }
#endif
                //Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_src_dev, flood_map);
                flood_map = DBR_CABLE_NET_DEV_TYPE;
                dbridgeAddInFrames(skb);
                if (DbridgeDB_GetlbridgeInterfaceMask()!= 0 &&  MAC_ISBROADCAST(ptr_ethhdr, h_dest)) /*in case bridged interfaces exist and broadcast packet ,flood the packet to cmci and cable*/
                {
                    flood_map |= DBR_CMCI_NET_DEV_TYPE;
                }
                Dbridge_floodPacket(skb, flood_map , DBR_CABLE_NET_DEV_TYPE ,DBR_CABLE_NET_DEV_TYPE);
                Mcast_type = MCAST_DATA;
            }

            /* if MDF is disabled */
            else
            {
                /* viserify if we need to handle this IGMP packet */
                Mcast_type = DBridge_Mcast_handler(skb,ptr_dbridge_src_dev,flood_map,MCAST_FILTER_NOT_ACTIVE);
            }
        }/*l2vpn*/

            if (Mcast_type==NOT_MCAST)
            {
                /* broadcast packet */
                dbridgeAddEsafeInFrames(skb);
                 if (DbridgeDB_GetlbridgeInterfaceMask()!= 0) /*in case bridged interfaces exist and broadcast packet ,flood the packet to cmci and cable*/
                {
                    flood_map |= DBR_CMCI_NET_DEV_TYPE;
                }
                Dbridge_floodPacket(skb, flood_map , DBR_CABLE_NET_DEV_TYPE , DBR_CABLE_NET_DEV_TYPE);
            }

        return;
    }

    DBRALT_PROT_ON(lockKey);
    ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
    if (ptr_dbridge_alt != NULL)
    {
        dbridge_dev = ptr_dbridge_alt->dbridgeDevice;
        sourceInterface = ptr_dbridge_alt->sourceInterface;
        /* Got what we wanted, can rempve protection, the DBR devices are NEVER (yet...) removed */
        DBRALT_PROT_OFF(lockKey);

        dest_dev_type = dbridge_dev->net_dev_type;
        switch (dest_dev_type)
        {
        case DBR_CABLE_NET_DEV_TYPE:

            break;
		case DBR_CMCI_NET_DEV_TYPE:
            break;
        case DBR_ESAFE_NET_DEV_TYPE:

            break;
        case DBR_WAN_IP_NET_DEV_TYPE:
        case DBR_LAN_IP_NET_DEV_TYPE:
            /*
            The CM MUST NOT accept any DHCPv4 DHCPOFFER or DHCPACK, DHCPv6 Advertise or Reply, TFTPDATA,
            HTTP Response, Time Protocol Response, or IPv6 Router Advertisements from any of the CPE ports
            */
            if ( apply_fwrd_rule( NULL, TI_FWRD_RULE_FROM_CPE, TI_DHCPv4_DHCPOFFER_IND, TI_ROUTER_ADVERTISEMENTS_IND, skb) )
                {
                #ifdef DBRIDGE_DBG
                if(dbridgeDbgLevel)
                {
                    printk ("CM Operational: The CM MUST NOT accept this packet from any of the CPE ports\n");
                    DBridge_logPacket(skb);
                }
                #endif
                dbridgeAddForwardDiscards(skb);
                dbridgeAddDiscards(skb);
                kfree_skb (skb);
                return;
            }
            break;
        default:
#ifdef DBRIDGE_DBG
            printk (KERN_WARNING "2No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
            dbridgeAddDiscards(skb);
            kfree_skb (skb);
            return;
        }

    } else
    {
        DBRALT_PROT_OFF(lockKey);

        /* Unknown destination we send the packet to CABLE interface */

        /* performer filter - ToDo */
        /* performer classifier - ToDo */

        /* Update ALT table with src MAC address */
#ifdef CONFIG_VENDOR_ARRIS_GATEWAY  // ARRIS ADD
                /* ARRIS MODIFY START */
		//if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev,skb->nfmark)!=0)
		if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev, 0)!=0)
                /* ARRIS MODIFY END */    
#else        
		if(DbridgeDb_AltAddMac(ptr_ethhdr->h_source, CPE_DYNAMIC, ptr_dbridge_src_dev,skb->ti_docsis_input_dev)!=0)
#endif // ARRIS ADD END           
        {
#ifdef DBRIDGE_DBG
            printk (KERN_INFO "CPE table if full drop %s\n", skb->dev->name);
#endif
            dbridgeAddDiscards(skb);
            kfree_skb (skb);
            return;
        }

        ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
        dbridgeAddEsafeInFrames(skb);
        skb->dev = ptr_dbridge_dest_dev->ptr_interface;
        skb->ti_selective_fwd_dev_info = (1LL << ((skb->dev->ifindex)-1));
#ifdef DBRIDGE_LOG
        if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
        {
            printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
            DBridge_logPacket(skb);
        }
#endif

        ptr_dbridge_dest_dev->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
        return;
    }

    dbridgeAddEsafeInFrames(skb);
    skb->dev = dbridge_dev->ptr_interface;
    if(sourceInterface)
    {
        skb->ti_selective_fwd_dev_info = (1LL << ((sourceInterface->ifindex)-1));
    }
    else
    {
        skb->ti_selective_fwd_dev_info = (1LL << ((skb->dev->ifindex)-1));
    }
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
    {
        printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif

    dbridge_dev->push(skb,DBR_FILTER_FLAG | DBR_CLASSIFIER_FLAG);
    return;
}
/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromCable(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 **************************************************************************
 *  \brief The function handle the packet from cable according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
**************************************************************************/
static void DBridge_regHandlePacketFromCable(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
    struct ethhdr*     ptr_ethhdr;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    struct net_device*  ptr_dest_interface;
    unsigned int dest_dev_type;
    unsigned int Mcast_type;
    unsigned int  mcast_filter_type = MCAST_FILTER_NOT_ACTIVE;
    DbridgeMcastMac_t* mcast_filter_info;
    unsigned int fc_flags = DBR_NO_FILTER_CLASS_FLAG;
    unsigned int flood_map = FLOOD_TO_ALL_INTERFACE;
    unsigned int mdfMode = 0;
    unsigned int l2vpn_prom_mode = 0; // ARRIS ADD
    DBRALT_PROT_DCL(lockKey);
    DbridgeDevice_t *dbridge_dev;
    struct net_device*  sourceInterface = NULL;

#ifdef DBRIDGE_LOG

    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_CABLE)
    {
        printk("\nDBRIDGE_DBG_REG_RX_CABLE");
        DBridge_logPacket(skb);
    }
#endif

    ptr_ethhdr = eth_hdr(skb);

    if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {
        if (skb->ti_meta_info & CNID_L2VPN_RELATED)
        {
            DBridge_Multicast_L2VPN_handler(skb, ptr_dbridge_src_dev, false);
            return;
        }

        /* if MDF is enabled */
        DbridgeMdfDb_GetMdfMode(&mdfMode);
        if (mdfMode == DBR_MDF_ENABLE_MODE)
        {
            Mcast_type = DBridge_MDF_handler(skb, ptr_dbridge_src_dev, flood_map);
#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
            {
                printk("\nDBridge_regHandlePacketFromCable: DBRIDGE MDF Mode is enabled\n");
            }
#endif
        }

        /* if MDF is disabled */
        else
        {
            if (!MAC_ISBROADCAST(ptr_ethhdr, h_dest))
            {
                /*     Mcast filtering */
                mcast_filter_info = DbridgeDb_GetMcastByMac(ptr_ethhdr->h_dest);
                if(mcast_filter_info == NULL)
                {
                    /* Unknown destination or illegal destination */
                    #ifdef DBRIDGE_LOG
                    if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
                    {
                        printk (KERN_WARNING "No Filter Drop Mcast packet from %s\n", skb->dev->name);
                        DBridge_logPacket(skb);
                    }
                    #endif

                    dbridgeAddDiscards(skb);
                    kfree_skb (skb);
                    return;
                }
                else
                {
                    mcast_filter_type = mcast_filter_info->Type;
                }
            }

            /* if the packet is Mcast the rest of the handling done by DBridge_Mcast_handler */
            Mcast_type = DBridge_Mcast_handler(skb,ptr_dbridge_src_dev,flood_map,mcast_filter_type);
        }

        if (Mcast_type==NOT_MCAST)
        {
            /* If Mcast_type==NOT_MCAST the destination address can be broadcast or
               non IPv4 IGMP Multicast Addressnot IPva                                */
            if((!MAC_ISBROADCAST(ptr_ethhdr, h_dest)) && (mcast_filter_type != STATIC_FILTER))
            {
                /*  flood the packet to CMCI only if the packet is broadcast or multicast learn from config file */
                flood_map ^=DBR_CMCI_NET_DEV_TYPE;
            }
            dbridgeAddInFrames(skb);
            /* Flood the packet to the relevant interfaces. */
            Dbridge_floodPacket(skb,flood_map, DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE,
                DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE);
        }

        return;
    }

    /* Unicast Packets. Check the ALT Table for a match on the destination MAC Address */
    DBRALT_PROT_ON(lockKey);
    ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);

    if (ptr_dbridge_alt != NULL)
    {
        if(ptr_dbridge_alt->dbridgeDevice == NULL)
        {
            DBRALT_PROT_OFF(lockKey);

            /* flood the packet to all CPE (esafe and cmci) */
            dbridgeAddInFrames(skb);
                Dbridge_floodPacket(skb,DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE, DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE,
                    DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE);
            return;
        }

        dbridge_dev = ptr_dbridge_alt->dbridgeDevice;
        sourceInterface = ptr_dbridge_alt->sourceInterface;
#ifdef DSG

		if (DBR_ESAFE_NET_DEV_TYPE == dbridge_dev->net_dev_type)
		{
			DbridgeEsafeDevice_t *ptr_esafe_table;
			ptr_esafe_table = DbridgeEsafe_GetMemberFromEsafeDev(dbridge_dev->ptr_interface);
			if (ptr_esafe_table->eSafeType & (DBR_ESAFE_ESTB_IP | DBR_ESAFE_ESTB_DSG))
			{
				if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_DSGI)
				{
					printk("\n DSG Debug send esafe dsg packet to DBridge_DSGTunnel_handler\n");
				}

                DBridge_DSGTunnel_handler(skb);
                DBRALT_PROT_OFF(lockKey);
				return;
			}
		}
#endif
        /* Got what we wanted, can rempve protection, the DBR devices are NEVER (yet...) removed */
        DBRALT_PROT_OFF(lockKey);

        ptr_dest_interface = dbridge_dev->ptr_interface;
        dest_dev_type=dbridge_dev->net_dev_type;

        if(dest_dev_type!=DBR_LAN_IP_NET_DEV_TYPE)
        {
            dbridgeAddInFrames(skb);

            /* the destanation is wan ip, cmci or esafe */
            /* Match Found: Send the packet only on that interface */
            if (dest_dev_type==DBR_CMCI_NET_DEV_TYPE || dest_dev_type==DBR_ESAFE_NET_DEV_TYPE)
            {
                fc_flags |= DBR_FILTER_FLAG;
            }

            skb->dev = ptr_dest_interface;
            if(sourceInterface)
            {
                skb->ti_selective_fwd_dev_info = (1LL << ((sourceInterface->ifindex)-1));
            }
            else
            {
                skb->ti_selective_fwd_dev_info = (1LL << ((skb->dev->ifindex)-1));
            }

#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
            {
                printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
                DBridge_logPacket(skb);
            }
#endif



            dbridge_dev->push(skb,fc_flags);
        }
    }
    else
    {
        DBRALT_PROT_OFF(lockKey);

        /* per L2VPN-N-12.1064-11 - forward l2vpn unknown DMAC unicast to all interfaces */
        /* "The CM MUST forward downstream packets encrypted in an L2VPN SAID with an unknown DMAC */
        /* to all L2VPN interfaces other than the interface on which it was received" */
        if (skb->ti_meta_info & CNID_L2VPN_RELATED)
        {
            DBridge_Multicast_L2VPN_handler(skb, ptr_dbridge_src_dev, true);
            return;
        }

        /* Unknown destination or illegal destination */
#ifdef DBRIDGE_DBG
        printk (KERN_WARNING "No DOCSIS Device; Drop packet from %s\n", skb->dev->name);
#endif
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
    }


}
/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 **************************************************************************
 *  \brief The function handle the packet from Lan IP according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
    DbridgeDevice_t* ptr_dbridge_dest_dev;
    unsigned int rules_to_apply; // ARRIS ADD

#ifdef DBRIDGE_LOG

    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_LANIP)
    {

        printk("\nDBRIDGE_DBG_REG_RX_LANIP");
        DBridge_logPacket(skb);
    }
#endif

/*
  The CM MUST NOT forward any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or
    Request, TFTP-RRQ, HTTP Request, Time Protocol Request, or Router Solicitation messages to any ports
    except the RF port.
*/

    // ARRIS allow LAN TFTP if told to do so
    if( DBridge_TftpAllowed() ) 
    {
        rules_to_apply = TI_FWRD_RULE_IPSTACK_TO_RF_ALLOW_TFTP;
    }
    else
    {
        rules_to_apply = TI_FWRD_RULE_IPSTACK_TO_RF;
    }
    if ( apply_fwrd_rule( NULL, rules_to_apply, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb) )
    {
    // END ARRIS LAN TFTP
#ifdef DBRIDGE_DBG
        if(dbridgeDbgLevel)
        {
            printk (KERN_INFO "CM Operational: The CM MUST NOT forward this messages to any interface except the RF Interface\n");
            DBridge_logPacket(skb);
        }
#endif
        dbridgeAddForwardDiscards(skb);
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
    }

#ifdef DSG
    DbridgeDevice_t*   ptr_dbridge_dsg_dev;
    struct sk_buff* skbclone;
    if(DbridgeDB_IsDsgLoobEnabled())
    {
        ptr_dbridge_dsg_dev = DbridgeDb_DevGetByType(DBR_DSGI_NET_DEV_TYPE);
        skbclone = skb_clone(skb, GFP_ATOMIC);
        if (skbclone == NULL)
        {
            #ifdef DBRIDGE_DBG
                printk (KERN_ERR "Cloning Failed for Device %s\n", ptr_dbridge_dsg_dev->ptr_interface->name);
            #endif
            dbridgeAddDiscards(skb);
            kfree_skb(skb);
            return;
        }
        skbclone->dev = ptr_dbridge_dsg_dev->ptr_interface;
        #ifdef DBRIDGE_LOG
        if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
        {
            printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skbclone->dev->name);
            DBridge_logPacket(skb);
        }
        #endif

        ptr_dbridge_dsg_dev->push(skbclone,DBR_NO_FILTER_CLASS_FLAG);
    }
#endif
    /* send the packet to CMCI interface only */
    ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CMCI_NET_DEV_TYPE);
    dbridgeAddInFrames(skb);

    skb->dev = ptr_dbridge_dest_dev->ptr_interface;
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
    {
        printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif

    ptr_dbridge_dest_dev->push(skb,DBR_NO_FILTER_CLASS_FLAG);
}


/**************************************************************************/
/*! \fn void DBridge_regHandlePacketFromLanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
 **************************************************************************
 *  \brief The function handle the packet from Wan IP according to Docsis spec
 *  \param[in] skb.
 *  \param[out] ptr to Dbridge device table.
 *  \return OK or error status.
 **************************************************************************/
static void DBridge_regHandlePacketFromWanIP(DbridgeDevice_t* ptr_dbridge_src_dev, struct sk_buff* skb)
{
    /* lookup destination interface */
    struct ethhdr*     ptr_ethhdr;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    DbridgeDevice_t*   ptr_dbridge_dest_dev;
    unsigned int       flood_map = DBR_CABLE_NET_DEV_TYPE | DBR_CMCI_NET_DEV_TYPE;
    unsigned int       mdfMode = 0;
    unsigned int       fwrd_map = FLOOD_TO_ALL_INTERFACE;
    bool               rf_only = false;
    DBRALT_PROT_DCL(lockKey);

#ifdef DBRIDGE_LOG

   if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_WANIP)
   {
       printk("\nDBRIDGE_DBG_REG_RX_WANIP");
       DBridge_logPacket(skb);
   }
#endif
/*
  The CM MUST NOT forward any DHCPv4 DHCPDISCOVER or DHCPREQUEST, DHCPv6 Solicit or
    Request, TFTP-RRQ, HTTP Request, Time Protocol Request, or Router Solicitation messages to any ports
    except the RF port.
*/
     if( (rf_only = apply_fwrd_rule( NULL, TI_FWRD_RULE_IPSTACK_TO_RF, TI_DHCPv4_DHCPDISCOVER_IND, TI_ROUTER_SOLICITATION_IND, skb)) )
         fwrd_map = DBR_CABLE_NET_DEV_TYPE;

    /* Get the Ethernet header. */
    ptr_ethhdr = eth_hdr(skb);

    if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {

        DbridgeMdfDb_GetMdfMode(&mdfMode);
        /* if MDF is enabled */
        if (mdfMode == DBR_MDF_ENABLE_MODE)
        {
#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_MDF)
            {
                printk("\nDBridge_regHandlePacketFromWanIP: DBRIDGE MDF Mode is enabled\n");
            }
#endif
            dbridgeAddInFrames(skb);
            Dbridge_floodPacket(skb, flood_map, 0 ,DBR_CABLE_NET_DEV_TYPE);
        }

        /* if MDF is disabled */
        else
        {
            /* broadcast or multicast, bit40 of MAC address is set */
            /* Flood the packet on all allowed interfaces. */
            dbridgeAddInFrames(skb);
            Dbridge_floodPacket(skb, fwrd_map, 0, 0);
        }

        return;
    }

    /* Unicast Packets. Look up for destination interface */
    /* Check the ALT Table for a match on the destination MAC Address */
    DBRALT_PROT_ON(lockKey);
    ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
    if ( rf_only || ptr_dbridge_alt == NULL )
    {
        /* No Match found; Send the packet to Cable interface.*/
        ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
    } else
    {
        /* Match Found: Send the packet only on that interface */
        ptr_dbridge_dest_dev = ptr_dbridge_alt->dbridgeDevice;
    }
    DBRALT_PROT_OFF(lockKey);
    dbridgeAddInFrames(skb);

    skb->dev = ptr_dbridge_dest_dev->ptr_interface;

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX)
    {
        printk("\nDBRIDGE_DBG_REG_TX FROM %s TO %s",ptr_dbridge_src_dev->ptr_interface->name,skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif


    ptr_dbridge_dest_dev->push(skb,DBR_CLASSIFIER_FLAG);

}

/**************************************************************************/
/*! \fn void dbridgeAddInFrames(struct sk_buff* skb);
 **************************************************************************
 *  \brief   increase input packet
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddInFrames(struct sk_buff* skb)
{
    unsigned int dev_index;
    struct ethhdr* ptr_ethhdr;

    if(skb)
    {
        dev_index = skb->ti_docsis_input_dev->ifindex;
        ptr_ethhdr = eth_hdr(skb);
        ptr_dbridge_counters->DevCounters[dev_index - 1].In++;
        if ( !MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
        {   /*If this is unicast packet - increment unicast counter as well*/
            ptr_dbridge_counters->DevCounters[dev_index - 1].inUcast++;
        } 
        else
        {   /* If this is broadcast/multicast packet - increment broadcast/multicast counter as well*/
            if( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
                ptr_dbridge_counters->DevCounters[dev_index - 1].inBcast++ ;
            else
                ptr_dbridge_counters->DevCounters[dev_index - 1].inMcast++ ;
        }
    }
}
/**************************************************************************/
/*! \fn void dbridgeAddEsafeInFrames(struct sk_buff* skb);
 **************************************************************************
 *  \brief   increase input packet from Esafe, in case of esafe we take
 *           the ifindex from dev instead of input_dev;
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddEsafeInFrames(struct sk_buff* skb)
{
    unsigned int dev_index;
    struct ethhdr* ptr_ethhdr;

    if(skb)
    {
         dev_index = skb->dev->ifindex;
         ptr_ethhdr = eth_hdr(skb);
         ptr_dbridge_counters->DevCounters[dev_index - 1].In++;
         if ( !MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
         {   /*If this is unicast packet - increment unicast counter as well*/
             ptr_dbridge_counters->DevCounters[dev_index - 1].inUcast++;
         } else
         {   /* If this is broadcast/multicast packet - increment broadcast/multicast counter as well*/
           if( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
              ptr_dbridge_counters->DevCounters[dev_index - 1].inBcast++ ;
           else
              ptr_dbridge_counters->DevCounters[dev_index - 1].inMcast++ ;
         }
    }
}

/**************************************************************************/
/*! \fn void dbridgeAddOutFrames(struct sk_buff* skb);
 **************************************************************************
 *  \brief   increase output packet
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddOutFrames(struct sk_buff* skb)
{
    unsigned int dest_dev_index = 0;
    unsigned long long tmp_selective_fwd = 0;
    struct ethhdr*     ptr_ethhdr;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    DBRALT_PROT_DCL(lockKey);
    Bool validEntry;

    if(skb)
    {
        dest_dev_index = skb->dev->ifindex;
        DBRIDGE_IFINDEX_CHK(dest_dev_index,validEntry, "dev %p, devname %s", skb->dev, skb->dev ? skb->dev->name : NULL);
        if (validEntry == False)
        {
            BUG();
        }
        /* if the dest is CMCI we need to find the  finale destanation device index, we have this information in ALT table */
        ptr_ethhdr = eth_hdr(skb);

        DBRALT_PROT_ON(lockKey);
        ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_dest);
        if (ptr_dbridge_alt)
        {
            if (ptr_dbridge_alt->dbridgeDevice)
            {
                if (ptr_dbridge_alt->dbridgeDevice->net_dev_type == DBR_CMCI_NET_DEV_TYPE)
                {
                    if (ptr_dbridge_alt->sourceInterface)
                    {
                        dest_dev_index = ptr_dbridge_alt->sourceInterface->ifindex;
                        DBRIDGE_IFINDEX_CHK_PR_MAC(dest_dev_index, ptr_dbridge_alt->macAddress,validEntry, 
                                                   "ptr_dbridge_alt %p, sourceInterface %p, sourceInterface->name %s", 
                                                    ptr_dbridge_alt, 
                                                    ptr_dbridge_alt->sourceInterface, ptr_dbridge_alt->sourceInterface->name);
                    }
                }
            }
        }
        DBRALT_PROT_OFF(lockKey);
        if ( !MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
        {  /*If this is unicast packet - increment unicast counter as well*/
            ptr_dbridge_counters->DevCounters[dest_dev_index - 1].outUcast++;
            ptr_dbridge_counters->DevCounters[dest_dev_index - 1].Out++;
        }
        else
        {
            if (skb->dev == lbr0Dev && skb->ti_selective_fwd_dev_info)
            {
                /* in case packet send to local bridge we have to update relevant interfaces
                   only if the bit in ti_selective_fwd_dev_info is set */
                dest_dev_index =0;
                tmp_selective_fwd = skb->ti_selective_fwd_dev_info;
                while (tmp_selective_fwd)
                {
                    dest_dev_index++;
                    if (tmp_selective_fwd & 0x1)
                    {
                        ptr_dbridge_counters->DevCounters[dest_dev_index - 1].Out++;
                        if ( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
                            ptr_dbridge_counters->DevCounters[dest_dev_index - 1].outBcast++ ;
                        else
                            ptr_dbridge_counters->DevCounters[dest_dev_index - 1].outMcast++;
                    }
                    tmp_selective_fwd = tmp_selective_fwd >> 1;
                }
            }
            else
            {
                if ( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
                    ptr_dbridge_counters->DevCounters[dest_dev_index - 1].outBcast++ ;
                else
                    ptr_dbridge_counters->DevCounters[dest_dev_index - 1].outMcast++;
                ptr_dbridge_counters->DevCounters[dest_dev_index - 1].Out++;
            }
        }
    }
}

/**************************************************************************/
/*! \fn void dbridgeAddDiscards(struct sk_buff* skb);
 **************************************************************************
 *  \brief   increase discard  packet
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddDiscards(struct sk_buff* skb)
{
    unsigned int dev_index;
    if(skb)
    {
        if( skb->ti_docsis_input_dev)
        {
            dev_index = skb->ti_docsis_input_dev->ifindex;
            ptr_dbridge_counters->DevCounters[dev_index - 1].Discard++;
        }
    }
}

/**************************************************************************/
/*! \fn void dbridgeAddForwardDiscards(struct sk_buff* skb);
 **************************************************************************
 *  \brief   increase forward discard  packet
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddForwardDiscards(struct sk_buff* skb)
{
    unsigned int dev_index;
    if(skb)
    {
        dev_index = skb->ti_docsis_input_dev->ifindex;
        ptr_dbridge_counters->DevCounters[dev_index - 1].FwrdRuleDrop++;
    }
}

#ifdef DSG
/**************************************************************************/
/*! \fn void dbridgeDsgAddOutFrames(struct net_device* ptr_dsg)  ;
 **************************************************************************
 *  \brief   increase dsg interface output  packet
 *  \param[in] none.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeDsgAddOutFrames(struct net_device* ptr_dsg, struct ethhdr* ptr_ethhdr)
{
    unsigned int dev_index = ptr_dsg->ifindex;
    ptr_dbridge_counters->DevCounters[dev_index - 1].Out++;
    if ( !MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {
        ptr_dbridge_counters->DevCounters[dev_index - 1].outUcast++;
    }
    else
    {
        if ( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
        {
            ptr_dbridge_counters->DevCounters[dev_index - 1].outBcast++;
        }
        else
        {
            ptr_dbridge_counters->DevCounters[dev_index - 1].outMcast++;
        }
    }
}
#endif

/**************************************************************************/
/*! \fn void dbridgeAddUnsupps(struct sk_buff* skb);
 **************************************************************************
 *  \brief   increase Unsupps  packet
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddUnsupps(struct sk_buff* skb)
{
    unsigned int dev_index;
    if(skb)
    {
        dev_index = skb->ti_docsis_input_dev->ifindex;
        ptr_dbridge_counters->DevCounters[dev_index - 1].UnSupport++;
        ptr_dbridge_counters->DevCounters[dev_index - 1].In--;
    }
}
/**************************************************************************/
/*! \fn void dbridgeAddFilterDropPacket(struct sk_buff* skb);
 **************************************************************************
 *  \brief   increase filter drop packet
 *  \param[in] skb.
 *  \param[out] none
 *  \return NONE
 **************************************************************************/
void dbridgeAddFilterDropPacket(struct sk_buff* skb)
{
    unsigned int dev_index;
    if(skb)
    {
        dev_index = skb->ti_docsis_input_dev->ifindex;
        ptr_dbridge_counters->DevCounters[dev_index - 1].FilterDrop++;
    }
}

/**************************************************************************/
/*! \fn static unsigned int  DBridge_Mcast_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev)
 **************************************************************************
 *  \brief   viserify if this is Mcast handle the packet and returne the Mcast type
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] flood_map.
 *  \param[in] mcast_filter_type.
 *  \param[out] none
 *  \return Macst rev type (MCAST_IGMP, MCAST_DATA or NOT_MCAST).
 **************************************************************************/
static unsigned int  DBridge_Mcast_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev,
                                           unsigned int flood_map, unsigned int mcast_filter_type)
{
    struct ethhdr*     ptr_ethhdr;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    unsigned int cpe_num =0;
    unsigned int fc_flags = DBR_FILTER_FLAG;
	unsigned int mcastFloodMap = DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE;
    DBRALT_PROT_DCL(lockKey);

    ptr_ethhdr = eth_hdr(skb);
    if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {
#ifdef DSG
        if((mcast_filter_type == DSG_FILTER)&&DbridgeDB_IsDsgLoobEnabled())
        {
            if (DbridgeDB_IsDsgMulticastAddress(ptr_ethhdr->h_dest) >= 0)
            {
                DBridge_DSGTunnel_handler(skb);
                return MCAST_DATA;
            }
        }
#endif
        if (!ptr_ethhdr->h_dest[1] && (ptr_ethhdr->h_dest[2] & 0x5e))
        {
            struct iphdr    *iph = (struct iphdr *) (((char *)ptr_ethhdr) + ETH_HLEN);
            if (iph != NULL)
            {
                if (iph->protocol == IPPROTO_IGMP)
                {
#ifdef DBRIDGE_LOG
                    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_IGMP)
                    {
                        printk ("\nIGMP Mcast packet from %s to igmp module\n", skb->dev->name);
                        DBridge_logPacket(skb);
                    }
#endif

                    DBRALT_PROT_ON(lockKey);
                    ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_source);
                    if (ptr_dbridge_alt!=NULL)
                    {
                        cpe_num=ptr_dbridge_alt->cpe_number;
                    }
                    DBRALT_PROT_OFF(lockKey);

                    if (ti_igmp_packet_handler ((struct igmphdr *)(((char *)iph) + (iph->ihl << 2)), skb,
                                            ptr_dbridge_src_dev->net_dev_type, cpe_num) ==  1 )
                    {
                        /*this is an igmp version 3, docsis igmp snooping(mdf disabled) not support igmpv3 so we treat it here as a broadcast packet*/
                        return NOT_MCAST;
                    }
                    else
                    {
                        /*this is an igmp version 1 or igmp version 2 packet , treat it as a multicast packet*/
                    return MCAST_IGMP;
                }
                }
                else
                { /* Mcast data packet */

                    DbridgeDevice_t* ptr_dbridge_dest_dev;
                    unsigned long long dev_map = ti_get_igmp_devices(iph->daddr);
                    if((mcast_filter_type == DYNAMIC_FILTER)&&(dev_map == 0))
                        /* The IGMP grop IP is not mach the packet IP - We drop the packet */
                    {
#ifdef DBRIDGE_LOG
                        if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
                        {
                            printk ("Mcast Data packet from dev %s IP:0x%x Drop\n", skb->dev->name , iph->daddr);
                        }
                        #endif
                        dbridgeAddDiscards(skb);
                        kfree_skb (skb);
                    }
                    else
                    {
#ifdef DBRIDGE_LOG
                        if(dbridgeDbgLevel & DBRIDGE_DBG_MCAST_CABLE)
                        {
                            printk ("Mcast Data packet from %s net_dev_type %x mcast_filter_type %x to dev map 0x%llx\n", skb->dev->name ,ptr_dbridge_src_dev->net_dev_type, mcast_filter_type, dev_map);
                        }
#endif


                        skb->ti_selective_fwd_dev_info=dev_map;

                        /* Don't flood packet mcast from cable interface */
                        if((ptr_dbridge_src_dev->net_dev_type !=DBR_CABLE_NET_DEV_TYPE )||(mcast_filter_type != DYNAMIC_FILTER)) // ARRIS ADD: TI fix correcting multicast forwarding
                        {
                            dbridgeAddInFrames(skb);
                            Dbridge_floodPacket(skb,flood_map, DBR_CABLE_NET_DEV_TYPE ,DBR_CABLE_NET_DEV_TYPE);

                        }
                        else
                        {
                            /* send the packet to CPE interface only */
                            if (DbridgeDb_Erouter_GetMode() == EROUTER_DISABLE)
                            {
                                /*dont flood the packet to esafe devices*/
                               mcastFloodMap ^= DBR_ESAFE_NET_DEV_TYPE;
								
                            }
                            if (DbridgeDB_GetlbridgeInterfaceMask()== 0)
								{
                                /*dont flood the packet to cmci devices*/
								mcastFloodMap ^= DBR_CMCI_NET_DEV_TYPE ;
								}

                            if(!dev_map)
                            {
                                dev_map = DbridgeDB_GetlbridgeInterfaceMask();
                            }
                            skb->ti_selective_fwd_dev_info=dev_map;
                            dbridgeAddInFrames(skb);
#ifdef INCLUDE_SERCOMM_GW
							if(skb->dev == ptr_dbridge_dest_dev->ptr_interface)
							{
								 ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
							}
#endif
                            Dbridge_floodPacket(skb,mcastFloodMap,
                                    mcastFloodMap, 0);

                        }
                    }
                    return MCAST_DATA;

                }
            }
        }
    }
    return NOT_MCAST;
}


/**************************************************************************/
/*! \fn static int  DBridge_MDF_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev,
                                        unsigned int flood_map)
 **************************************************************************
 *  \brief verify if this is Mcast packet and handle the packet according to MDF logic.
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] flood_map.
 *  \param[out] none
 *  \return Macst rev type (MCAST_IGMP, MCAST_DATA or NOT_MCAST).
 **************************************************************************/
static int DBridge_MDF_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev,
                               unsigned int flood_map)
{
    struct ethhdr* ptr_ethhdr;
    unsigned int dsid_index = 0;
    unsigned long long mcast_linux_cmim = 0;
    unsigned int mcast_flood_map = 0;

    ptr_ethhdr = eth_hdr(skb);

    /* if the packet is Multicast  */
    if ( MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
    {
        if(!MAC_ISBROADCAST(ptr_ethhdr, h_dest))
        {
            /* get DSID index from the packet - bits 24-31 */
            dsid_index = skb->ti_meta_info >> CNID_DSID_INDEX_BIT_FIELD_OFFSET & 0xff;

            /* The value from the HW is a multiple of 4 */
            if (dsid_index != 0xFF)
            {
                dsid_index = dsid_index >> 2;
            }

            /* if DSID index value not valid - drop the packet */
            if (dsid_index >= DBR_MDF_MAX_DSIDS)
            {
                printk("\nHandle MDF packet from dev %s Failed - DSID index value %u not valid!!! (Max-Dsid = %d)\n",skb->dev->name, dsid_index, DBR_MDF_MAX_DSIDS);
                #ifdef DBRIDGE_LOG
                if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk ("\nMcast Data packet from dev %s Drop\n", skb->dev->name);
                }
                #endif
                dbridgeAddDiscards(skb);
                kfree_skb(skb);
                return MCAST_DATA;
            }

            #ifdef DBRIDGE_LOG
            if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
            {
                printk("\nHandle MDF packet from dev %s with DSID index %u\n", skb->dev->name, dsid_index);
            }
            #endif
        #ifdef DSG
            if((DbridgeDB_IsDsgMulticastAddress(ptr_ethhdr->h_dest) >= 0)&&DbridgeDB_IsDsgLoobEnabled())
            {
                DBridge_DSGTunnel_handler(skb);
                return MCAST_DATA;
            }
            #endif

            /* if DSID index not found - drop the packet */
            if (DbridgeMdfDb_GetMcastBitmaps(dsid_index, &mcast_linux_cmim, &mcast_flood_map) == -1)
            {
                printk("\nHandle MDF packet from dev %s Failed - DSID index %u not found!!!\n",
                       skb->dev->name, dsid_index);
                #ifdef DBRIDGE_LOG
                if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk ("\nMcast Data packet from dev %s Drop\n", skb->dev->name);
                }
                #endif
                dbridgeAddDiscards(skb);
                kfree_skb(skb);
            }

            /* The flood map is 0 - drop the packet */
            else if (mcast_flood_map == 0)
            {
                #ifdef DBRIDGE_LOG
                if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk ("\nMcast Data packet from dev %s to dev map 0x%llx and flood map 0x%x Drop\n",
                            skb->dev->name, mcast_linux_cmim, mcast_flood_map);
                }
                #endif
                dbridgeAddDiscards(skb);
                kfree_skb(skb);
            }

            /* forward the packet */
            else
            {
                #ifdef DBRIDGE_LOG
                if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    printk ("\nMcast Data packet from %s to dev map 0x%llx and flood map 0x%x\n",
                            skb->dev->name , mcast_linux_cmim, mcast_flood_map);
                }
                #endif

                /* Flood the packet to all interfaces and perform filter for CMCI, ESAFE and WAN_IP. */
                dbridgeAddInFrames(skb);
                skb->ti_selective_fwd_dev_info = mcast_linux_cmim;

                #ifdef DBRIDGE_LOG
                if (dbridgeDbgLevel & DBRIDGE_DBG_MDF)
                {
                    /* Print the information before flooding, because the flood function frees the skb */
                    printk("\n xx  DBRIDGE_DBG_MDF Mcast mcast_flood_map %x mcast_linux_cmim %llx\n",
                           mcast_flood_map, mcast_linux_cmim);
                    printk("dev type %x send Mcast date packet with selective_fwd %llx",
                           ptr_dbridge_src_dev->net_dev_type,
                           skb->ti_selective_fwd_dev_info);
                    DBridge_logPacket(skb);
                }
                #endif

                Dbridge_floodPacket(skb, mcast_flood_map,
                                    DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE | DBR_WAN_IP_NET_DEV_TYPE, 0);
            }

            return MCAST_DATA;
        }
    }

    return NOT_MCAST;
}


/**************************************************************************/
/*! \fn static int DBridge_Multicast_L2VPN_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev)
 **************************************************************************
 *  \brief Handle the packet according to L2VPN logic.
 *  \param[in] skb.
 *  \param[in] ptr_dbridge_src_dev.
 *  \param[in] saidIndex.
 *  \param[out] none
 *  \return TBD
 **************************************************************************/
static int DBridge_Multicast_L2VPN_handler(struct sk_buff* skb, DbridgeDevice_t* ptr_dbridge_src_dev, bool isUnicastUnknownDestination)
{
    unsigned long long linuxCmim = 0;
    unsigned int floodMap = 0;
    unsigned int saidIndex = skb->ti_meta_info2 & CNID_L2VPN_RELATED_SAID_MASK;
    unsigned int filterMap = 0;

    if (DbridgeL2vpnDb_GetSaidBitmaps(saidIndex, &linuxCmim, &floodMap) < 0)
    {
#ifdef DBRIDGE_LOG
        if (dbridgeDbgLevel & DBRIDGE_DBG_L2VPN_DS)
        {
            printk ("\nL2VPN packet from dev %s dropped due to unconfigured L2VPN SAID\n", skb->dev->name);
        }
#endif
        dbridgeAddDiscards(skb);
        kfree_skb(skb);
        return -1;
    }

    /* else forward the packet according to CMIM */

#ifdef DBRIDGE_LOG
    if (dbridgeDbgLevel & DBRIDGE_DBG_L2VPN_DS)
    {
        printk ("\nL2VPN Data packet from %s to dev map 0x%llx and flood map 0x%x\n", skb->dev->name, linuxCmim, floodMap);
    }
#endif

    /* Flood the packet to all interfaces and perform filter for CMCI, ESAFE and WAN_IP. */
    dbridgeAddInFrames(skb);
    skb->ti_selective_fwd_dev_info = linuxCmim;


#ifdef DBRIDGE_LOG
    if (dbridgeDbgLevel & DBRIDGE_DBG_L2VPN_DS)
    {
        if (ptr_dbridge_src_dev != NULL)
    {
        /* Print the information before flooding, because the flood function frees the skb */
        printk("\nL2VPN pkt FROM %s TO %s\n", ptr_dbridge_src_dev->ptr_interface->name, skb->dev->name);
            printk("dev type %x send L2VPN pkt with flood_map %u and selective_fwd %llx", ptr_dbridge_src_dev->net_dev_type,floodMap, skb->ti_selective_fwd_dev_info);
        DBridge_logPacket(skb);
    }
    }
#endif

    /* in case of unicast to unknown DMAC, need to apply the filter rules */
    if (isUnicastUnknownDestination)
    {
        filterMap = DBR_CMCI_NET_DEV_TYPE;
    }
        
    Dbridge_floodPacket(skb, floodMap, filterMap, 0); 

    return 0;
}


/**************************************************************************/
/*! \fn void Dbridge_SendIgmpPacket(struct sk_buff *skb, unsigned long device_map, unsigned long device_type)
 **************************************************************************
 *  \brief   Send Mcasr packet, this function call from igmp module
 *  \param[in] skb.
 *  \param[in] device_map, The devices that conect to the Mcast grope.
 *  \param[in] device_type, the docsis bridge dev type (cable or CPE)
 *  \param[out] none
 *  \return none
 **************************************************************************/
 void Dbridge_SendIgmpPacket(struct sk_buff *skb, unsigned long long device_map, unsigned long device_type)
 {
     DbridgeDevice_t* ptr_dbridge_dest_dev = NULL;
     unsigned int fc_flags = DBR_FILTER_FLAG;
	 unsigned int mcastFloodMap = DBR_CMCI_NET_DEV_TYPE | DBR_ESAFE_NET_DEV_TYPE;
    if (device_type & DBR_CMCI_NET_DEV_TYPE)
    {  /* send the packet to CPE interface only */
		if (DbridgeDb_Erouter_GetMode() == EROUTER_DISABLE)
			{
			mcastFloodMap ^= DBR_ESAFE_NET_DEV_TYPE;
			}
		if (DbridgeDB_GetlbridgeInterfaceMask()== 0)
			{
			mcastFloodMap ^= DBR_CMCI_NET_DEV_TYPE ;
			}
	}
     else if (device_type & DBR_CABLE_NET_DEV_TYPE)
     {
         ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_CABLE_NET_DEV_TYPE);
         fc_flags |= DBR_CLASSIFIER_FLAG;
     }
     else
     {
#ifdef DBRIDGE_DBG
         printk (KERN_WARNING "Dbridge Mcast unknown device type %x\n", (unsigned int)device_type);
#endif
         kfree_skb (skb);
         return;
     }
     /*if the packet's destination is not cable interface,cmci interface or esafe then deop the packet*/
     if ((ptr_dbridge_dest_dev == NULL) && (mcastFloodMap == 0))
     {
         printk(KERN_ERR "ptr_dbridge_dest_dev == NULL, Drop Packet\n");
         kfree_skb (skb);
         return;
     }

     if (skb == NULL)
     {
        printk (KERN_WARNING "Dbridge Mcast skb is NULL !!\n");
        return;
     }

     skb->ti_selective_fwd_dev_info=device_map;
     dbridgeAddInFrames(skb);
	 if (ptr_dbridge_dest_dev != NULL)
		 skb->dev = ptr_dbridge_dest_dev->ptr_interface;

#ifdef DBRIDGE_LOG
     if(dbridgeDbgLevel & DBRIDGE_DBG_REG_IGMP)
     {
         printk (KERN_WARNING "IGMP packet from igmp module to %s\n", skb->dev->name);
         DBridge_logPacket(skb);
     }
#endif

 if (ptr_dbridge_dest_dev != NULL)
 {
     /*packet's destination is  cable*/
	 ptr_dbridge_dest_dev->push(skb,fc_flags);
 }
 else
 {
     /*packet's destination is cmci and/or esafe*/
     Dbridge_floodPacket(skb,mcastFloodMap,mcastFloodMap, 0);
 }

}

/**************************************************************************/
/*! \fn int Dbridge_messageProcessed(char *mac_address, int msg_type)
 **************************************************************************
 *  \brief
 *  \param[out] mac_address.
 *  \param[msg_type] device_map, The devices that conect to the Mcast grope.
 *  \return
 **************************************************************************/
int Dbridge_messageProcessed(char *mac_address, int msg_type)
{
#ifdef DBRIDGE_DBG
    printk (KERN_DEBUG "IGMP message from %.2x:%.2x:%.2x:%.2x:%.2x:%.2x processsed for type (%d) interface\n",
            mac_address[0],mac_address[1],mac_address[2],
            mac_address[3],mac_address[4],mac_address[5], msg_type);
#endif
    if (msg_type == JOIN_STATE)
    {
        (void)DbridgeDb_AddMcastMac(mac_address,DYNAMIC_FILTER);
    }else
    {
        DbridgeDb_DelMcastMac(mac_address);
    }
    return 0;
}

#ifdef DSG
/**************************************************************************/
/*! \fn static void DBridge_DSGTunnel_handler(struct sk_buff* skb)
 **************************************************************************
 *  \brief   Handle the DSG tunnel data.
 *  \param[in] skb.
 *  \param[out] none
 *  \return none.
 **************************************************************************/
static void DBridge_DSGTunnel_handler(struct sk_buff* skb)
{
    DbridgeDevice_t* ptr_dbridge_dest_dev;
	int DSG_dev_index;
	static int DSG_dev_index_mask = 0;

    if((ptr_dbridge_dest_dev = DbridgeDb_DevGetByType(DBR_DSGI_NET_DEV_TYPE)) == NULL)
    {
        printk ("DSGI device not found\n");
        dbridgeAddDiscards(skb);
        kfree_skb (skb);
        return;
    }
	
    /*initiate the DSG_dev_index_mask*/
	if (!DSG_dev_index_mask)
	{
		DbridgeDB_GetDsgDeviceIndex (&DSG_dev_index);
		DSG_dev_index_mask = (1LL << ((DSG_dev_index)-1));
	}
	skb->ti_selective_fwd_dev_info = DSG_dev_index_mask;
	
    dbridgeAddInFrames(skb);
    skb->dev = ptr_dbridge_dest_dev->ptr_interface;

    ptr_dbridge_dest_dev->push(skb,0);
    return;
}
#endif
#if 0
#ifdef CONFIG_TI_DOCSIS_EGRESS_HOOK
/**************************************************************************/
/*! \fn int Dbridge_egress_hook(struct sk_buff *skb)
 **************************************************************************
 *  \brief   In
 *  \param[in] SKB.
 *  \return
 **************************************************************************/
static int Dbridge_egress_hook(struct sk_buff *skb)
{
    struct ethhdr* ptr_ethhdr;

#ifdef INCLUDE_SERCOMM_GW
    if(skb && skb->mac.raw && skb->input_dev)
#else
    if(skb)
#endif
    {
        ptr_ethhdr = eth_hdr(skb);

        if((skb->input_dev == eth0_netdev) || (skb->input_dev == usb0_netdev))
        {
            ptr_dbridge_counters->DevCounters[skb->input_dev->ifindex].In++;
            ptr_dbridge_counters->DevCounters[skb->dev->ifindex].Out++;
            if ( !MAC_ISMULTICAST(ptr_ethhdr, h_dest) )
            {/*If this is unicast packet - increment unicast counters as well*/
                ptr_dbridge_counters->DevCounters[skb->input_dev->ifindex].inUcast++;
                ptr_dbridge_counters->DevCounters[skb->dev->ifindex].outUcast++;
            } else
            {  /*If this is broadcast packet - increment broadcast counter as well*/
                if ( MAC_ISBROADCAST(ptr_ethhdr, h_dest) )
                {
                    ptr_dbridge_counters->DevCounters[skb->input_dev->ifindex].inBcast++;
                    ptr_dbridge_counters->DevCounters[skb->dev->ifindex].outBcast++;
                } else
                {   /*If this is multicast packet - increment multicast counter as well*/
                    ptr_dbridge_counters->DevCounters[skb->input_dev->ifindex].inMcast++;
                    ptr_dbridge_counters->DevCounters[skb->dev->ifindex].outMcast++;
                }
            }
        }
    }
    return 0;
}
#endif
#endif /* CONFIG_TI_DOCSIS_EGRESS_HOOK */
#if 0
// ARRIS ADD START 
// Add PC IP addresses to routing table
/**************************************************************************/
/*! \fn static void dbridgeSetSockAddr(struct sockaddr_in *sin, u32 addr, u16 port)
 **************************************************************************
 *  \brief     Api to build routing table add message
 *  \param[in] *sin
 *  \param[in] addr
 *  \param[in] port
 *  \return none
 **************************************************************************/
static void dbridgeSetSockAddr(struct sockaddr_in *sin, u32 addr, u16 port)
{
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = addr;
	sin->sin_port = port;
}

/**************************************************************************/
/*! \fn static void dbridgeAddRoute(unsigned int pc_addr)
 **************************************************************************
 *  \brief     Add entry to routing table
 *  \param[in] pc_addr
 *  \return none
 **************************************************************************/
static void dbridgeAddRoute(unsigned int pc_addr)
{
    struct rtentry rm;
    int res;
    struct socket dummy;

    memset(&rm, 0, sizeof(rm));

    dbridgeSetSockAddr((struct sockaddr_in *) &rm.rt_dst, pc_addr, 0);
    dbridgeSetSockAddr((struct sockaddr_in *) &rm.rt_genmask, 0xFFFFFFFF, 0);

    rm.rt_flags = RTF_UP;

    // Hardcode device to be lan0, if we ever need to add from a different device, 
    // that information can be extracted from the sk_buff structure.
    rm.rt_dev = "lan0";

    // inet_ioctl requires a socket, even if it is not needed for adding a route;
    // set sk to NULL; not used
    dummy.sk = NULL;

    // Call kernel function that adds entry to route, af_inet.c
    res = inet_ioctl(&dummy,SIOCADDRT, (unsigned long)&rm);

    // Print error message only if return code is not EEXIST (-17, already exist), 
    if ( (res < 0) && (res != -EEXIST) )
    {
        printk(KERN_ERR "Cannot add route %s (%d).\n",rm.rt_dev, res);
    }
}
// ARRIS ADD END
#endif

EXPORT_SYMBOL(DBridge_receive);
EXPORT_SYMBOL(Dbridge_register_L2vpnDataCB);

