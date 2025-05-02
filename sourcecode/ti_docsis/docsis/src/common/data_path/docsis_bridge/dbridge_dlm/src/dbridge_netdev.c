/*
 *
 * dbridge_netdev.c
 * Description:
 * DOCSIS bridge networking implementation
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

#define _DBRIDGE_NERDEV_C_

/*! \file dbridge_netdev.c
    \brief This module manage the docsis net device
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <asm/system.h>
#include "dbridge_netdev.h"
#include "dbridge_db.h"
#include "dbridge_main.h"
#include "dbridge_esafe.h"      /* ARRIS MOD */
#ifdef DSG                      /* ARRIS MOD */
#include <linux/ip.h>
#endif
#include "dbridge_snif.h"
#include <linux/if_ether.h>
#include "dbridge_l2vpn_ds_db.h"
#include "dbridge_common.h"

// ARRIS ADD
// LRO PATCH
#ifdef CONFIG_INET_LRO
#include <linux/inet_lro.h>
#endif
// SOCKET FILTER
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/in.h>
// END ARRIS
#include <linux/inet_lro.h>


  
/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
#ifdef CONFIG_INTEL_NS_DEVICE_FILTER
extern int intel_ipv6_ns_filter(struct net_device *dev,struct in6_addr* dst_addr,unsigned char banned_flags);
extern int intel_register_ns_handler (struct net_device* dev, int (*ns_handler)(struct net_device *dev,struct in6_addr* dst_addr,unsigned char banned_flags));
#endif
 
#ifdef DBRIDGE_LOG
extern unsigned int dbridgeDbgLevel;
#endif
extern unsigned int ipv4WanInterface_enable;
extern unsigned int ipv6WanInterface_enable;
extern unsigned int ipv4LanInterface_enable;
extern unsigned int ipv6LanInterface_enable;

#include <linux/ti_hil.h>

struct net_device * wanDev =0;
struct net_device * lbr0Dev =0;
static struct net_device * lanDev =0;
extern struct net_device *gCni0Netdev;

// ARRIS ADD
extern unsigned long ArrisCertFlag;
// END ARRIS ADD
  
/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/
#define QOS_INVALID_SF_INDEX -1

#ifndef br_port_exists
#define br_port_exists(dev) (dev->priv_flags & IFF_BRIDGE_PORT)
#endif

static int (*Dbridge_QosClassifierDataCB)(struct sk_buff *skb, int *sfIndex, int *phsIndex) = NULL;
int (*Dbridge_FilterCB)(struct sk_buff *skb, unsigned long long destIfMask, unsigned long long *acceptDestIfMask, unsigned int pktDirect) = NULL;

/* IPTV Bridge CB */
int (*IpTvBridge_ReceiveCB)(struct sk_buff *skb) = NULL;

// ARRIS ADD
int (*Dbridge_HttpFilterCB)(struct sk_buff *skb, int esafeType) = NULL;
// END ARRIS ADD

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/


/*! \fn int Dbridge_interconnectReceive(struct sk_buff *skb)                                     
 *  \brief Call back function called from the DOCSIS Bridge to push the packet
 *  \brief through the interconnect device back to the LOCAL Bridge.
 *  \param[in] pointer to skb.
 *  \return Always returns 0
 */

static int Dbridge_interconnectReceive(struct sk_buff *skb, unsigned int fc_flags);

/*! \fn int Dbridge_IpStackNetifReceive(struct sk_buff *skb)                                     
 *  \brief The function is called by the DOCSIS Bridge to push a packet through
 *  \brief the NET devices to IP stack.
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0.
 */
#ifdef DSG
static int Dbridge_interconnectDsgReceive(struct sk_buff *skb, unsigned int fc_flags);
#endif
static int Dbridge_IpStackNetifReceive(struct sk_buff *skb, unsigned int fc_flags);

/*! \fn int Dbridge_netifReceive(struct sk_buff *skb)                                     
 *  \brief The function is called by the DOCSIS Bridge to push a packet through
 *  \brief the Cable NI
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0.
 */
static int Dbridge_CableNetifReceive(struct sk_buff *skb,unsigned int fc_flags);

/*! \fn int Dbridge_netDeviceXmit(struct sk_buff *skb, struct net_device *ptr_netdev)                                     
 *  \brief Registered Transmit function called when a packet is to be transmitted
 *  \brief on to the net device. 
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0.
 */
static int Dbridge_netDeviceXmit(struct sk_buff *skb, struct net_device *ptr_netdev);


/*! \fn struct net_device_stats* Dbridge_netDeviceGetStats(struct net_device *ptr_netdev))                                     
 *  \brief et the network statistics
 *  \param[in] pointer to net_device.
 *  \return Pointer to the network statistics block
 */
static struct net_device_stats* Dbridge_netDeviceGetStats(struct net_device *ptr_netdev);

/*! \fn int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierData)(struct sk_buff *skb, 
 *                                       unsigned int *sfIndex, unsigned int *phsIndex))
 *  \brief register the QOS data CB function
 *  \param[in] the QOS data CB function
 *  \return OK
 */
int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierData)(struct sk_buff *skb, 
                                               int *sfIndex, int *phsIndex));

/*! \fn static int Dbridge_DropFltrPacket(struct sk_buff *skb)
 *  \brief Drop the given packet as a result of filtering
 *  \param[in] struct sk_buff *skb - skb containing the packet
 *  \return none
 **************************************************************************/
static void Dbridge_DropFltrPacket(struct sk_buff *skb);

/*! \fn static int Dbridge_InterfaceEnableStatusHandler(struct sk_buff *skb)
 *  \brief Drop the given packet if InterfaceEnableStatus mib is down
 *  \param[in] struct sk_buff *skb - skb containing the packet
 *  \return 1 if packet dropt 
 **************************************************************************/
static int Dbridge_InterfaceEnableStatusHandler(struct sk_buff *skb);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*! \fn int Dbridge_NetDevInit(void)                                     
 **************************************************************************
 *  \brief DOCSIS net device initialization.
 *  \param[in] no input.
 *  \param[out] no output.
 *  \return 0=OK or error status.
 **************************************************************************/

int Dbridge_NetDevInit(void)
{
    struct net_device *tmp_net_device;

    /******************************************/
    /* register Cable device to docsis bridge */
    /******************************************/


    tmp_net_device=dev_get_by_name (&init_net,CABLE_DEV_NAME);
    if(tmp_net_device==NULL)
    {
        printk(KERN_INFO "unknown device %s\n",CABLE_DEV_NAME);
        return -1; 
    }
    DbridgeDb_DevAdd(tmp_net_device,Dbridge_CableNetifReceive,DBR_CABLE_NET_DEV_TYPE);

    gCni0Netdev = tmp_net_device;
    dev_put(tmp_net_device);

    /**********************************************************************************************************/
    /* create and register lbr0 device to docsis bridge (the conaction between local bridge and docsis bridge)*/
    /**********************************************************************************************************/
    
    tmp_net_device=Dbridge_NetDevCreate(LBRIGE_CONNECTION_DEV_NAME,Dbridge_NetDevSetup);
    if(tmp_net_device==NULL)
    {
        printk(KERN_INFO "unknown device %s\n",LBRIGE_CONNECTION_DEV_NAME);
        return -1; 
    }
    
    DbridgeDb_DevAdd(tmp_net_device,Dbridge_interconnectReceive,DBR_CMCI_NET_DEV_TYPE);

    lbr0Dev = tmp_net_device;
#ifdef DSG
    if(DbridgeDB_IsDsgLoobEnabled())
    {
        tmp_net_device=Dbridge_NetDevCreate(LBRIGE_CONNECTION_4DSGI_DEV_NAME,Dbridge_NetDevSetup);
        if(tmp_net_device==NULL)
        {
            printk(KERN_INFO "unknown device %s\n",LBRIGE_CONNECTION_4DSGI_DEV_NAME);
            return -1;
        }
        DbridgeDb_DevAdd(tmp_net_device,Dbridge_interconnectDsgReceive,DBR_DSGI_NET_DEV_TYPE);
    }
#endif //DSG
    /*****************************************************/
    /* create and register lan0 device to docsis bridge  */
    /*****************************************************/
    tmp_net_device=Dbridge_NetDevCreate(LAN_IP_DEV_NAME,Dbridge_NetDevSetup);

    if(tmp_net_device==NULL)
    {
        printk(KERN_INFO "unknown device %s\n",LAN_IP_DEV_NAME);
        return -1; 
    }
    DbridgeDb_DevAdd(tmp_net_device,Dbridge_IpStackNetifReceive,DBR_LAN_IP_NET_DEV_TYPE);
	lanDev = tmp_net_device;


    /*****************************************************/
    /* create and register wan0 device to docsis bridge  */
    /*****************************************************/
    tmp_net_device=Dbridge_NetDevCreate(WAN_IP_DEV_NAME,Dbridge_NetDevSetup);
    if(tmp_net_device==NULL)
    {
        printk(KERN_INFO "unknown device %s\n",WAN_IP_DEV_NAME);
        return -1; 
    }
    DbridgeDb_DevAdd(tmp_net_device,Dbridge_IpStackNetifReceive,DBR_WAN_IP_NET_DEV_TYPE);
    wanDev = tmp_net_device;
#ifdef CONFIG_INTEL_NS_DEVICE_FILTER
    intel_register_ns_handler(wanDev, intel_ipv6_ns_filter);
#endif
    return 0; /* OK*/
}

/**************************************************************************/
/*! \fn struct net_device* Dbridge_NetDevCreate(void)                                     
 **************************************************************************
 *  \brief alloc and register net device for docsis bridge.
 *  \param[in] device name.
 *  \param[in] device setup function.
 *  \return net_device.
 **************************************************************************/
struct net_device* Dbridge_NetDevCreate(char* name,void (*setup)(struct net_device *))
{
    struct net_device*       ptr_netdev;


    /* Allocate memory for the network device. */
    ptr_netdev = alloc_netdev(sizeof(DbridgeNetDev_t), name, setup);
    if(ptr_netdev == NULL)
    {
        printk (KERN_ERR "Unable to allocate memory for the DOCSIS to Network stack connectivity\n");
        return NULL;
    } 
    /* Register the network device */   
    if (register_netdev(ptr_netdev) < 0)
    {
        printk (KERN_ERR "Unable to register the DOCSIS to Network stack connectivity\n");
        return NULL;
    }

    /* Return the pointer to the network device. */
    return ptr_netdev;
}

static const struct net_device_ops dbridge_netdev_ops = {
 .ndo_start_xmit = Dbridge_netDeviceXmit,
 .ndo_get_stats = Dbridge_netDeviceGetStats,
 .ndo_set_mac_address = eth_mac_addr,
};
/**************************************************************************/
/*! \fn void Dbridge_NetDevSetup(struct net_device *ptr_netdev)                                     
 **************************************************************************
 *  \brief Initialize and override the internal Connection Dev network structure.
 *  \param[in] pointer to Lbridge Connection Device.
 *  \return NONE.
 **************************************************************************/
void Dbridge_NetDevSetup(struct net_device *ptr_netdev)
{
    DbridgeNetDev_t* ptr_docsis_net_dev = netdev_priv(ptr_netdev);

    /* Initialize the Ethernet device. */
    ether_setup(ptr_netdev);

    /* Setup the pointers */
    ptr_docsis_net_dev->ptr_device  = ptr_netdev;
    ptr_netdev->netdev_ops=&dbridge_netdev_ops;

   return;
}


/**************************************************************************/
/*! \fn int Dbridge_interconnectReceive(struct sk_buff *skb)                                     
 **************************************************************************
 *  \brief Call back function called from the DOCSIS Bridge to push the packet
 *  \brief through the interconnect device back to the LOCAL Bridge.
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0
 **************************************************************************/
static int Dbridge_interconnectReceive(struct sk_buff *skb, unsigned int fc_flags)
{
    DbridgeNetDev_t* priv;
#ifdef INCLUDE_SERCOMM_GW
    struct ethhdr*  ptr_ethhdr;
#endif
    struct ethhdr*  ptr_ethhdr=NULL;

     if (!(br_port_exists(skb->dev)))
     {
         /*lbro is not connected to br0. need to drop the packet because otherwise 
         the packet will continue its way to the ip stack and we'll waste cpu time for nothing*/
         kfree_skb (skb);
         return 0;
     }
    /* Initialize the protocol. */
    ptr_ethhdr   = (struct ethhdr *)skb_mac_header(skb);
    skb->protocol = ptr_ethhdr->h_proto;

	if(fc_flags & DBR_FILTER_FLAG)
    {
        if(Dbridge_FilterCB)
        {
            unsigned int acceptPacket;
            unsigned long long acceptDestIfMask;
            
            acceptPacket = Dbridge_FilterCB(skb,skb->ti_selective_fwd_dev_info,&acceptDestIfMask,DFLTR_FROM_CABLE);
#ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_FILTER)
                {
                    printk("\n %s: acceptPacket = %x, skb->ti_selective_fwd_dev_info= %llx, acceptDestIfMask = %llx\n",__FUNCTION__,acceptPacket, 
                           skb->ti_selective_fwd_dev_info, acceptDestIfMask );
                }
#endif

            if(acceptPacket)
            {
                skb->ti_selective_fwd_dev_info = acceptDestIfMask;
            }
            else
            {
                Dbridge_DropFltrPacket(skb);
                return 0;
            }

        }
    }

    /* If packet is from CABLE, need to check L2VPN/DUT rules */
    if (skb->ti_docsis_input_dev == gCni0Netdev)
    {
        if (DbridgeCommon_ReceiveFilterL2VPNorDUT(skb, DBR_CMCI_NET_DEV_TYPE) == True)
        {
            return 0;
        }
    }

    dbridgeAddOutFrames(skb);
    /* Get the DOCSIS network device private area */
    priv = netdev_priv(skb->dev);

    if (!netif_running(skb->dev))
    {
        priv->stats.rx_dropped++;
        kfree_skb (skb);
        return 0;
    }

#ifdef DSG
    struct iphdr *ipHdr;
    ipHdr = (struct iphdr*)(eth_hdr(skb) + sizeof(struct ethhdr));
    if ((ipHdr->protocol != 1) && DbridgeDB_IsDsgEstbIPAddress(ipHdr->daddr))
    {
        kfree_skb (skb);
        return 0;
    }
#endif
    /* Increment the statistics. */
    priv->stats.rx_packets++;
    priv->stats.rx_bytes += skb->len;
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX_IF)
    {
        printk("\nDBRIDGE_DBG_REG_TX_IF TO %s",skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif
#ifdef INCLUDE_SERCOMM_GW //TI missed protocol field.
    ptr_ethhdr   = (struct ethhdr *)skb->mac.raw;
    skb->protocol = ptr_ethhdr->h_proto;
#endif
    /* Pull the Ethernet MAC Address before passing it to the stack. */
    skb_pull(skb,ETH_HLEN);

    /* Push the packet to the networking stacks for the bridge to receive it */
    netif_receive_skb(skb);
    return 0;
}
#ifdef DSG
static int Dbridge_interconnectDsgReceive(struct sk_buff *skb, unsigned int fc_flags)
{
    DbridgeNetDev_t* priv;
    DbridgeNetDev_t* priv_ip;
    DbridgeNetDev_t* priv_dsg;
    DbridgeEsafeDevice_t* ptr_esafe_estb_ip;
    DbridgeEsafeDevice_t* ptr_esafe_estb_dsg;
    struct ethhdr*     ptr_ethhdr;
    int isDsgTnPkt = 0;
    ptr_ethhdr = (struct ethhdr *)skb_mac_header(skb);
    if((ptr_esafe_estb_ip = DbridgeEsafe_GetMemberFromEsafeType(DBR_ESAFE_ESTB_IP)) == NULL)
    {
        printk(KERN_WARNING"\n [%s] ptr_esafe_estb_ip = null \n",__func__);
        kfree_skb (skb);
        return 0;
    }
    if((ptr_esafe_estb_dsg = DbridgeEsafe_GetMemberFromEsafeType(DBR_ESAFE_ESTB_DSG)) == NULL)
    {
        printk(KERN_WARNING"\n [%s] ptr_esafe_estb_dsg = null \n",__func__);
        kfree_skb (skb);
        return 0;
    }
    if (DbridgeDB_IsDsgMulticastAddress(ptr_ethhdr->h_dest) >= 0)
        isDsgTnPkt = 1;
    else
        isDsgTnPkt = 0;
    if((fc_flags & DBR_FILTER_FLAG) || isDsgTnPkt)
    {
        if(Dbridge_FilterCB)
        {
            unsigned int acceptPacket;
            unsigned int acceptDestIfMask;
            acceptPacket = Dbridge_FilterCB(skb,skb->ti_selective_fwd_dev_info,&acceptDestIfMask,DFLTR_TO_DSG);
			/*in DSG there is no meaning to acceptDestIfMask. Only drop the packet if acceptPacket is 0 */
		    if(!acceptPacket)
            {
#ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel &  DBRIDGE_DBG_FILTER)
                {
                    printk("\nFilter drop packet");
                    DBridge_logPacket(skb);
                }
#endif
                ti_hil_pp_event (TI_DOCSIS_FLTR_DISCARD_PKT, (void *)skb);
                dbridgeAddFilterDropPacket(skb);
                kfree_skb (skb);
                return 0;
            }
        }
    }
    dbridgeAddOutFrames(skb);
    priv = netdev_priv(skb->dev);
    if (!netif_running(skb->dev))
    {
        priv->stats.rx_dropped++;
        kfree_skb (skb);
        return 0;
    }
    priv->stats.rx_packets++;
    priv->stats.rx_bytes += skb->len;
    if (isDsgTnPkt)
    {
        dbridgeDsgAddOutFrames(ptr_esafe_estb_dsg->ptr_targetInterface, ptr_ethhdr);
        priv_dsg = netdev_priv(ptr_esafe_estb_dsg->ptr_targetInterface);
        if (!netif_running(ptr_esafe_estb_dsg->ptr_targetInterface))
        {
        #ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_DSGI)
            {
                printk("\nDbridge_interconnectDsgReceive ESTB DSG Interface no active, drop the packet\n");
                DBridge_logPacket(skb);
            }
        #endif
            priv_dsg->stats.rx_dropped++;
            kfree_skb (skb);
            return 0;
        }

        /* Mark packet on DSG device for PPP */
        if (ptr_esafe_estb_dsg->ptr_targetInterface != NULL)
        {
            if (PPP_MarkPacket(skb, ptr_esafe_estb_dsg->ptr_targetInterface->ifindex) != 0)
            {
                printk(KERN_ERR "%s - %d: Could not mark skb for %s\n", __func__, __LINE__, "ptr_esafe_estb_dsg");
            }
        }

        priv_dsg->stats.rx_packets++;
        priv_dsg->stats.rx_bytes += skb->len;
    }
    else
    {
        dbridgeDsgAddOutFrames(ptr_esafe_estb_ip->ptr_targetInterface, ptr_ethhdr);
        priv_ip = netdev_priv(ptr_esafe_estb_ip->ptr_targetInterface);
        if (!netif_running(ptr_esafe_estb_ip->ptr_targetInterface))
        {
        #ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_REG_RX_DSGI)
            {
                printk("\nDbridge_interconnectDsgReceive ESTB IP Interface no active, drop the packet\n");
                DBridge_logPacket(skb);
            }
        #endif
            priv_ip->stats.rx_dropped++;
            kfree_skb (skb);
            return 0;
        }

        /* Mark packet on DSG device for PPP */
        if (ptr_esafe_estb_ip->ptr_targetInterface != NULL)
        {
            if (PPP_MarkPacket(skb, ptr_esafe_estb_ip->ptr_targetInterface->ifindex) != 0)
            {
                printk(KERN_ERR "%s - %d: Could not mark skb for %s\n", __func__, __LINE__, "ptr_esafe_estb_ip");
            }
        }

        priv_ip->stats.rx_packets++;
        priv_ip->stats.rx_bytes += skb->len;
    }
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX_IF)
    {
        printk("\nDBRIDGE_DBG_REG_TX_IF TO %s",skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif
    skb_pull(skb,ETH_HLEN);
    netif_receive_skb(skb);
    return 0;
}
#endif

/**************************************************************************/
/*! \fn int Dbridge_IpStackNetifReceive(struct sk_buff *skb)                                     
 **************************************************************************
 *  \brief The function is called by the DOCSIS Bridge to push a packet through
 *  \brief the NET devices to IP stack.
 *  \param[in] pointer to skb.
 *  \param[in] filter and classifier flags.
 *  \return Always returns 0.
 **************************************************************************/
static int Dbridge_IpStackNetifReceive(struct sk_buff *skb, unsigned int fc_flags)
{
    DbridgeNetDev_t* priv;
    struct ethhdr*  ptr_ethhdr=NULL;
    DbridgeAltEntry_t* ptr_dbridge_alt;
    DBRALT_PROT_DCL(lockKey);
#ifdef CONFIG_INET_LRO
    struct net_device *input_dev_tmp = NULL;
#endif

    /* If packet is from CABLE, need to check L2VPN/DUT rules */
    if (skb->ti_docsis_input_dev == gCni0Netdev)
    {
        /* Traffic from Cable cannot be destined to LAN, only WAN */
        if (DbridgeCommon_ReceiveFilterL2VPNorDUT(skb, DBR_WAN_IP_NET_DEV_TYPE) == True)
        {
            return 0;
        }
    }

    /* Initialize the protocol. */

    ptr_ethhdr   = eth_hdr(skb);
    skb->protocol = ptr_ethhdr->h_proto;
    skb->ti_meta_info = 0;

#ifdef CONFIG_TI_IP_PKTINFO_SOCKOPT
    /* packet dest is WAN0 */
    DBRALT_PROT_ON(lockKey);
    ptr_dbridge_alt = DbridgeDb_AltGetMemberByMac(ptr_ethhdr->h_source);
	if (ptr_dbridge_alt!=NULL)
    {
        if ( NULL != ptr_dbridge_alt->dbridgeDevice && 
             DBR_CMCI_NET_DEV_TYPE == ptr_dbridge_alt->dbridgeDevice->net_dev_type )
        {
            /* packet source  is CMCI */
            skb->ti_meta_info = 1;
        }
    }
    DBRALT_PROT_OFF(lockKey);
#endif

    // ARRIS MOD - move this below the filter application, otherwise filters have no
    // way of knowing packet came from CMCI
    // skb->input_dev = skb->dev;
//#ifdef CONFIG_INTEL_DOCSIS_ICMP_IIF
    /* We use the docsis_icmp_iif to support ICMP when wan0, mta0 or erouter0 are on the same subnet */
    /* the implementation in file icmp.c*/
    //skb->docsis_icmp_iif = skb->dev->ifindex;
//#endif
    
    /* If the packet is not IP, IPv6, ARP or LLC increase unsupported counter and drop the packet */
    if(!((skb->protocol == ETH_P_IP) || (skb->protocol == ETH_P_IPV6) || 
         (skb->protocol == ETH_P_ARP) || (skb->protocol <= ETH_DATA_LEN)))
    {
        /* If the packet is not IP, IPv6, ARP or LLC increase unsupported counter and drop the packet */
        dbridgeAddUnsupps(skb);
        kfree_skb (skb);
        return 0;
    }

    // ARRIS ADD - we want to filter to the IP stack as well. This is only supported
    // from the CMCI to IP stack. It is not possible to filter from the RF to IP stack.
    // We don't want to do this for Certification because it causes a problem with coexistence over the 
    // 192.168.100.1 during OSS tests. This is because the OSS config uses docsDevFilter with index "9" 
    // which filters ethernet packet.
    if( !ArrisCertFlag )
    {
        if(fc_flags & DBR_FILTER_FLAG)
        {
            if(Dbridge_FilterCB)
            {
                unsigned int acceptPacket;
                unsigned int acceptDestIfMask;
    
                acceptPacket = Dbridge_FilterCB(skb,skb->ti_selective_fwd_dev_info,&acceptDestIfMask,DFLTR_FROM_CABLE);
                if(acceptPacket)
                {
                    skb->ti_selective_fwd_dev_info = acceptDestIfMask;
                }
                else
                {
                    Dbridge_DropFltrPacket(skb);
                    return 0;
                }
            }
        }
    }
    // END ARRIS ADD

    // ARRIS MOD - moved from above
#ifdef CONFIG_INTEL_DOCSIS_ICMP_IIF
    /* We use the docsis_icmp_iif to support ICMP when wan0, mta0 or erouter0 are on the same subnet */
    /* the implementation in file icmp.c*/
    skb->docsis_icmp_iif = skb->dev->ifindex;
#endif
  
    dbridgeAddOutFrames(skb);

    /* Get the DOCSIS network device private area */
    priv = netdev_priv(skb->dev);

    if (!netif_running(skb->dev))
    {
        priv->stats.rx_dropped++;
        kfree_skb (skb);
        return 0;
    }
    
    // ARRIS ADD - call the HTTP blocking callback
    if( Dbridge_HttpFilterCB )
    {
        int estype;

        estype = DbridgeEsafe_GetEsafeTypeFromEsafeDev(skb->dev);
        // this was HTTP and it is meant to be blocked
        if( Dbridge_HttpFilterCB(skb, estype) == DFLTR_DISCARD )
        {
            priv->stats.rx_dropped++;
            kfree_skb (skb);
            return 0;
        }
    }
    // END ARRIS ADD
    /* Increment the statistics. */
    priv->stats.rx_packets++;
    priv->stats.rx_bytes += skb->len;

	if(1 == Dbridge_InterfaceEnableStatusHandler(skb))
	{
		return 0;
	}

    /* Set the flag if the packet was meant for us; this is called from the DOCSIS
     * Bridge and we know for sure that the skb eth_hdr points to the start of the
     * Ethernet frame. So we check the Destination MAC address of the packet with
     * the MAC Address of the network device and set the flag. This is required else
     * the packet will be dropped in the IP stack. This is the same that is done in the
     * eth_type_trans code. The reason why I did not call the function again is because
     * I want to preserve the original input interface device and not have it overwritten. */
	if (!compare_ether_addr(eth_hdr(skb)->h_dest, priv->ptr_device->dev_addr)) 
	{
		/* It is for our (changed) MAC-address! */
		skb->pkt_type = PACKET_HOST;
     }   
    #ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel & DBRIDGE_DBG_REG_TX_IF)
    {
        printk("\nDBRIDGE_DBG_REG_TX_IF TO %s",skb->dev->name);
        DBridge_logPacket(skb);
    }
    #endif
    /* Pull the Ethernet MAC Address before passing it to the stack. */
    skb_pull(skb,ETH_HLEN);

#ifdef CONFIG_INET_LRO
    input_dev_tmp = dev_get_by_index(&init_net,skb->skb_iif);
	
    if((input_dev_tmp) && (input_dev_tmp == gCni0Netdev) && (input_dev_tmp->features & NETIF_F_LRO ))
    {
        void *cni_priv = netdev_priv(input_dev_tmp);
        lro_receive_skb((struct net_lro_mgr *)cni_priv, skb, (void *)cni_priv);
    }
    else
#endif
    {
        /* Push the packet to the IP stack. */
        netif_receive_skb(skb);
    }
#ifdef CONFIG_INET_LRO
	if(input_dev_tmp) 
	{
		dev_put(input_dev_tmp);
	}
#endif
    return 0;
} 

/**************************************************************************/
/*! \fn int Dbridge_netifReceive(struct sk_buff *skb)                                     
 **************************************************************************
 *  \brief The function is called by the DOCSIS Bridge to push a packet through
 *  \brief the Cable NI
 *  \param[in] filter and classifier flags.
 *  \param[in] pointer to skb.
 *  \return Always returns 0.
 **************************************************************************/
static int Dbridge_CableNetifReceive(struct sk_buff *skb, unsigned int fc_flags)
{

    int sfIndex;
    int phsIndex;

    if(fc_flags & DBR_FILTER_FLAG)
    {
        if(Dbridge_FilterCB)
        {
            unsigned int acceptPacket;
            unsigned long long acceptDestIfMask;

            acceptPacket = Dbridge_FilterCB(skb,skb->ti_selective_fwd_dev_info,&acceptDestIfMask,DFLTR_TO_CABLE); 
#ifdef DBRIDGE_LOG
            if(dbridgeDbgLevel & DBRIDGE_DBG_FILTER)
            {
                printk("\n %s: acceptPacket = %x, skb->ti_selective_fwd_dev_info= %llx, acceptDestIfMask = %llx\n",__FUNCTION__,acceptPacket, 
                       skb->ti_selective_fwd_dev_info, acceptDestIfMask );
            }
#endif

            if(acceptPacket)
            { 
                skb->ti_selective_fwd_dev_info = acceptDestIfMask;
            }
            else
            {
                Dbridge_DropFltrPacket(skb);
                return 0;
            }
        }
    }

    /* Unset the L2VPN bits in ti_meta_info*/
    skb->ti_meta_info &= (~US_L2VPN_CHECKED);
    skb->ti_meta_info &= (~CNID_L2VPN_RELATED);

    /* Set PHS index */
    skb->ti_meta_info |= 0x00FF0000;
    /* ReSet SF index */    
    skb->ti_meta_info &= 0x00FFFFFF;

    if(fc_flags&DBR_CLASSIFIER_FLAG)
    {
        if(Dbridge_QosClassifierDataCB)
        {   /* call QOS Classifier */
            int temp_meta_info =skb->ti_meta_info;
            Dbridge_QosClassifierDataCB(skb, &sfIndex, &phsIndex);
            
            if (sfIndex != QOS_INVALID_SF_INDEX) /*QoS classifier matched*/
            {
                /* set the meta data info */
                temp_meta_info &= 0x0000FFFF; 
                temp_meta_info |= ((sfIndex << 24) & 0xFF000000);
                temp_meta_info |= ((phsIndex << 16) & 0x00FF0000);
                skb->ti_meta_info = temp_meta_info;
#ifdef DBRIDGE_LOG
                if(dbridgeDbgLevel & DBRIDGE_DBG_CLASSIFIER)
                {
                    printk("\nsfIndex = %x, phsIndex= %x, skb->ti_meta_info = %x\n",sfIndex, phsIndex,skb->ti_meta_info );
                }
#endif
            }
            else /*Upstream Drop Classifier matched*/
            {
                Dbridge_DropFltrPacket(skb);
                return 0;
            }
        }
    }


    /* ARRIS ADD BEGIN : Last resort - check for packets from transcoder */
    {
        struct ethhdr* ptr_ethhdr = (struct ethhdr *)skb_mac_header(skb);
        unsigned char* arrisXcoderMacFilter = "\x00\x00\x00\x00\x00\x03";

        if (compare_ether_addr(ptr_ethhdr->h_source, arrisXcoderMacFilter) == 0) 
        {
            kfree_skb(skb);
            return 0;
        }
    }
    /* ARRIS ADD END */

    dbridgeAddOutFrames(skb);

    dev_queue_xmit(skb);
    return 0;
}

// ARRIS ADD : SOCKET FILTER
int CheckIpv4FilterTx(struct sk_buff* skb, unsigned int address, unsigned int port)
{
    int match = 0;

    if(skb->dev->hard_header_len == ETH_HLEN)
    {
        struct ethhdr* ptr_ethhdr = (struct ethhdr *)skb_mac_header(skb);

        if (ptr_ethhdr->h_proto == ETH_P_IP)   // 0x86DD = IPv6 
        {
            struct iphdr* ptr_iphdr = (struct iphdr*)skb_network_header(skb);

            if ((ptr_iphdr->daddr == address) && (ptr_iphdr->protocol == IPPROTO_TCP))
            {
                struct tcphdr* ptr_tcphdr = (struct tcphdr*)(skb_network_header(skb)+skb_network_header_len(skb));
                if (ptr_tcphdr->dest == port)
                {
                    match = 1;
                }
            }
        }
    }

    return match;
}

/**************************************************************************/
/*! \fn int Dbridge_netDeviceXmit(struct sk_buff *skb, struct net_device *ptr_netdev)                                     
 **************************************************************************
 *  \brief Registered Transmit function called when a packet is to be transmitted
 *  \brief on to the net device. 
 *  \param[in] pointer to skb.
 *  \return Always returns 0.
 **************************************************************************/
int Dbridge_netDeviceXmit(struct sk_buff *skb, struct net_device *ptr_netdev)
{
    DbridgeNetDev_t* priv;

    /* Initialize the data pointers before passing the packet down to the */
    /* DOCSIS Bridge.                                                     */
    skb_reset_mac_header(skb);

    if (1 == Dbridge_InterfaceEnableStatusHandler(skb))
    {
        return 0;
    }

    /* Get the DOCSIS network device private area */

    priv = netdev_priv(skb->dev);

    if (!netif_running(skb->dev))
    {
        priv->stats.tx_dropped++;
        kfree_skb (skb);
        return 0;
    }

    /* Increment the statistics. */
    priv->stats.tx_packets++;
    priv->stats.tx_bytes += skb->len;

    // ARRIS ADD : SOCKET FILTER and DOCSIS BRIDGE BYPASS
    /* On Wan0, check for Docsis Bridge Bypass */
    if (ptr_netdev == wanDev)
    {
        // If the filter is on, keep these stats seperate
        if (priv->Ipv4FilterAddress)
        {
            // find the Source IP address/port to see if we want to seperate this stream
            if (CheckIpv4FilterTx(skb, priv->Ipv4FilterAddress, priv->Ipv4FilterPort))
            {
                skb->filter_match = 1;

                // Check if we should bypass the docsis bridge and send straight to IP layer
                if (priv->DbridgeBypass)
                {
                    // Send directly to cni0 for transmission, and exit
                    skb->dev = gCni0Netdev;
                    dbridgeAddOutFrames(skb);
                    dev_queue_xmit(skb);
                    return 0;
                }
            }
        }
    }
    // END ARRIS

    /* Push the packet to the DOCSIS Bridge. */
    DBridge_receive (skb);

    return 0;
}

// ARRIS ADD : interface from cni0 to wan0 to setup upstream ipv4 filter and 
// docsis bridge bypass 
void dbridgeSetIpv4Filter(struct net_device *ptr_netdev, unsigned int Ipv4FilterAddress, unsigned int Ipv4FilterPort)
{
    DbridgeNetDev_t* priv;

    // printk(KERN_ERR "Docsis bridge netdev %s setting ipv4 filter to %d.%d.%d.%d:%d\n", ptr_netdev->name, NIPQUAD(Ipv4FilterAddress), Ipv4FilterPort);

    /* Get the DOCSIS network device private area */
    priv = netdev_priv(ptr_netdev);

    priv->Ipv4FilterAddress = Ipv4FilterAddress;
    priv->Ipv4FilterPort    = Ipv4FilterPort;
}
EXPORT_SYMBOL(dbridgeSetIpv4Filter);

void dbridgeSetBypass(struct net_device *ptr_netdev, int DbridgeBypass)
{
    DbridgeNetDev_t* priv;

    // printk(KERN_ERR "Docsis bridge netdev %s setting dbridge bypass to %d\n", ptr_netdev->name, DbridgeBypass);

    /* Get the DOCSIS network device private area */
    priv = netdev_priv(ptr_netdev);

    priv->DbridgeBypass = DbridgeBypass;
}
EXPORT_SYMBOL(dbridgeSetBypass);
// END ARRIS

/**************************************************************************/
/*! \fn struct net_device_stats* Dbridge_netDeviceGetStats(struct net_device *ptr_netdev))                                     
 **************************************************************************
 *  \brief get the network statistics
 *  \param[in] pointer to net_device.
 *  \return Pointer to the network statistics block
 **************************************************************************/
static struct net_device_stats* Dbridge_netDeviceGetStats(struct net_device *ptr_netdev)
{
    DbridgeNetDev_t* ptr_docsis_net_dev;

    /* Get the DOCSIS Local Interconnect Information*/
    ptr_docsis_net_dev = (DbridgeNetDev_t *)netdev_priv(ptr_netdev);

    /* Return the network statistics. */
    return &ptr_docsis_net_dev->stats;
}

/**************************************************************************/
/*! \fn int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierData)(struct sk_buff *skb, 
 *                                       unsigned int *sfIndex, unsigned int *phsIndex))
 **************************************************************************
 *  \brief register the QOS data CB function
 *  \param[in] the QOS data CB function
 *  \return OK
 **************************************************************************/
int Dbridge_register_QosClassifierDataCB(int (*Dbridge_QosClassifierData)(struct sk_buff *skb, 
                                              int *sfIndex, int *phsIndex))
{   
    unsigned long flags;

    local_irq_save(flags);
    Dbridge_QosClassifierDataCB = Dbridge_QosClassifierData;
    local_irq_restore(flags);

    return 0;
}

/**************************************************************************/
/*! \fn int Dbridge_register_FilterCB(int (*Dbridge_Filter)(struct sk_buff *skb, unsigned int destIfMask, 
 *                                                  unsigned int acceptDestIfMask, unsigned int pktDirect))
 **************************************************************************
 *  \brief register the QOS data CB function
 *  \param[in] the filter CB function
 *  \return OK
 **************************************************************************/
int Dbridge_register_FilterCB(int (*Dbridge_Filter)(struct sk_buff *skb, unsigned long long destIfMask, 
                                                    unsigned long long *acceptDestIfMask, unsigned int pktDirect))
{   
    unsigned long flags;

    local_irq_save(flags);
    Dbridge_FilterCB = Dbridge_Filter;
    local_irq_restore(flags);

    return 0;
}

/**************************************************************************/
/*! \fn int Dbridge_register_IpTvRecCB(int (*IpTvBridge_Receive)(struct sk_buff *skb))
 **************************************************************************
 *  \brief register the IPTV Bridge receive CB function
 *  \param[in] the IPTV Bridge receive CB function
 *  \return OK
 **************************************************************************/
int Dbridge_register_IpTvRecCB(int (*IpTvBridge_Receive)(struct sk_buff *skb))
{   
    unsigned long flags;

    local_irq_save(flags);
    IpTvBridge_ReceiveCB = IpTvBridge_Receive;
    local_irq_restore(flags);

    return 0;
}

/**************************************************************************/
/*! \fn int int Dbridge_unregister_IpTvRecCB(void)
 **************************************************************************
 *  \brief Un-register the IPTV Bridge receive CB function
 *  \return OK
 **************************************************************************/
int Dbridge_unregister_IpTvRecCB(void)
{   
    unsigned long flags;

    local_irq_save(flags);
    IpTvBridge_ReceiveCB = NULL;
    local_irq_restore(flags);

    return 0;
}
/**************************************************************************/
/*! \fn static int Dbridge_DropFltrPacket(struct sk_buff *skb)
 **************************************************************************
 *  \brief Drop the given packet as a result of filtering
 *  \param[in] struct sk_buff *skb - skb containing the packet
 *  \return none
 **************************************************************************/
static void Dbridge_DropFltrPacket(struct sk_buff *skb)
{

#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel &  DBRIDGE_DBG_FILTER)
    {
        printk("\nFilter drop packet");
        DBridge_logPacket(skb);
    }
#endif

    ti_hil_pp_event (TI_DOCSIS_FLTR_DISCARD_PKT, (void *)skb);
    
    dbridgeAddFilterDropPacket(skb);
    kfree_skb (skb);
}

/**************************************************************************
 *! \fn static int Dbridge_InterfaceEnableStatusHandler(struct sk_buff *skb)
 **************************************************************************
 *  \brief Drop the given packet if InterfaceEnableStatus mib is down
 *  \param[in] struct sk_buff *skb - skb containing the packet
 *  \return 1 if packet dropt 
 **************************************************************************/
static int Dbridge_InterfaceEnableStatusHandler(struct sk_buff *skb)
{
	struct ethhdr*     ptr_ethhdr=NULL;

    int       interfaceEnableStatus =1;

	/* Get the Ethernet header. */    
   
	ptr_ethhdr = eth_hdr(skb);
	

	if(ptr_ethhdr)
	{
			
		if((skb->dev == lanDev) && (ptr_ethhdr->h_proto == ETH_P_IP) && (!ipv4LanInterface_enable))
		{
			interfaceEnableStatus =0;
		}
		else if((skb->dev == wanDev) && (ptr_ethhdr->h_proto == ETH_P_IP) && (!ipv4WanInterface_enable))
		{
			interfaceEnableStatus =0;
		}
		else if((skb->dev == wanDev) && (ptr_ethhdr->h_proto == ETH_P_IPV6) && (!ipv6WanInterface_enable))
		{
			interfaceEnableStatus =0;
		}
		else if((skb->dev == lanDev) && (ptr_ethhdr->h_proto == ETH_P_IPV6) && (!ipv6LanInterface_enable))
		{
			interfaceEnableStatus =0;
		}
	}
	
	if(0 == interfaceEnableStatus)
	{
#ifdef DBRIDGE_LOG
    if(dbridgeDbgLevel)
    {
        printk("\n Drop packet IPv4 or IPv6IP Interface %s Disable",skb->dev->name);
        DBridge_logPacket(skb);
    }
#endif
		kfree_skb (skb);
		return 1;
	}
	return 0;
}

// ARRIS ADD - this function adds the HTTP filter callback
int Dbridge_register_HttpFilterCB(int (*Dbridge_HttpFilter)(struct sk_buff *skb, int esafeType))
{
    unsigned long flags;

    local_irq_save(flags);
    Dbridge_HttpFilterCB = Dbridge_HttpFilter;
    local_irq_restore(flags);

    return 0;
}
// END ARRIS ADD

EXPORT_SYMBOL(Dbridge_register_QosClassifierDataCB); 
EXPORT_SYMBOL(Dbridge_register_FilterCB); 
EXPORT_SYMBOL(Dbridge_register_IpTvRecCB);
EXPORT_SYMBOL(Dbridge_unregister_IpTvRecCB);
EXPORT_SYMBOL(Dbridge_register_HttpFilterCB);       // ARRIS ADD
EXPORT_SYMBOL(wanDev); 
EXPORT_SYMBOL(lbr0Dev); 


