/*
 *
 * erouter_netdev.c
 * Description:
 * Implementation of the eRouter test network device
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
//#include <linux/ip.h>
//#include <linux/udp.h>
//#include <linux/in.h>
#include <linux/proc_fs.h>

#include "dbridge_esafe.h"

#define ER_PROC_ENABLE "/proc/net/erouter/enable"

/* This is the private data the eRouter interface holds */
typedef struct erPrivate_st
{
    /* Keep track of the statistics. */
    struct net_device_stats     stats;

    /* Pointer to the network device. */
    struct net_device*   dev;

} erPrivate_t;

static char *netdevname = NULL;
module_param(netdevname, charp, 0);

#ifdef CONFIG_INTEL_NS_DEVICE_FILTER
extern int intel_ipv6_ns_filter(struct net_device *dev,struct in6_addr* dst_addr,unsigned char banned_flags);
extern int intel_register_ns_handler (struct net_device* dev, int (*ns_handler)(struct net_device *dev,struct in6_addr* dst_addr,unsigned char banned_flags));
#endif

/**************************************************************************/
/*      LOCAL FUNCTIONS DECLERATIONS:                                     */
/**************************************************************************/

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/



static int er_open (struct net_device *dev)
{
    /* Start queue */
    netif_start_queue (dev);
    return 0;
}

static int er_release (struct net_device *dev)
{
    /* Stop queue */
    netif_stop_queue(dev);
    return 0;
}

int er_rx (struct sk_buff *skb)
{
    erPrivate_t *erPrivate;
    struct ethhdr*  ptr_ethhdr;

    /* Get pointer to private area */
    erPrivate = netdev_priv( skb->dev );

    /* Update statistics */
    erPrivate->stats.rx_packets++;
    erPrivate->stats.rx_bytes += skb->len;

    /* Check if this packet is meant for us */
    if (memcmp(erPrivate->dev->dev_addr, skb_mac_header(skb), ETH_ALEN) == 0)
        skb->pkt_type = PACKET_HOST;

    /* Initialize the protocol. */
    ptr_ethhdr   = (struct ethhdr *)skb_mac_header(skb);
    skb->protocol = ptr_ethhdr->h_proto; 

#ifdef CONFIG_INTEL_DOCSIS_ICMP_IIF
    /* We use the docsis_icmp_iif to support ICMP when wan0, mta0 or erouter0 are on the same subnet */
    /* the implementation in file icmp.c*/
    skb->docsis_icmp_iif = skb->dev->ifindex;
#endif

    /* Pull the Ethernet MAC Address before passing it to the stack. */
    skb_pull(skb, ETH_HLEN);

    /* Push the skb to the IP stack */
    netif_rx( skb );
    return 0;
}

static struct net_device_stats *er_get_net_stats(struct net_device *dev)
{
    erPrivate_t *erPrivate = netdev_priv(dev);

    /* Return the statistics */
    return (struct net_device_stats *) &erPrivate->stats;
}

static int er_set_mac_addr(struct net_device *dev, void *addr)
{
    struct sockaddr *sa = addr;

    memcpy( dev->dev_addr, sa->sa_data, dev->addr_len );

    return 0;
}

static int er_tx (struct sk_buff *skb, struct net_device *dev)
{
    erPrivate_t *erPrivate = netdev_priv(dev);

    /* Update statistics */
    erPrivate->stats.tx_packets++;
    erPrivate->stats.tx_bytes += skb->len;
    skb_reset_mac_header(skb);

    /* Push the packet to the DOCSIS Bridge. */
    DbridgeEsafe_DeviceXmit( skb, dev );

    return 0;
}

static const struct net_device_ops erouter_netdev_ops = {
         .ndo_open               = er_open,
         .ndo_start_xmit         = er_tx,
         .ndo_stop               = er_release,
         .ndo_get_stats          = er_get_net_stats,
         .ndo_do_ioctl           = NULL,
         .ndo_set_mac_address    = er_set_mac_addr,
};

static void er_init (struct net_device *dev)
{
    unsigned char erAddr[ ETH_ALEN ] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 }; /* Dummy */

    dev->netdev_ops = &erouter_netdev_ops;
    dev->addr_len = ETH_ALEN;

    ether_setup(dev);
    memcpy( dev->dev_addr, erAddr, ETH_ALEN );

#ifdef CONFIG_INTEL_NS_DEVICE_FILTER
    intel_register_ns_handler(dev, intel_ipv6_ns_filter);
#endif
    printk ("eRouter: Network interface initialized successfully\n");
    return;
}

int __init er_init_module (void)
{
    int result;
    struct net_device *dev;
    erPrivate_t *erPrivate;

    /* Check that device name was defined on the command line */
    if (netdevname == NULL)
    {
        printk("eRouter: Missing mandatory netdevname, aborting\n");
        return EINVAL;
    }

    /* Allocate memory for network interface */
    if ( ( dev = alloc_netdev( sizeof(erPrivate_t), netdevname, er_init ) ) == NULL )
    {
    	printk ("eRouter: Error allocating memory for eRouter device\n");
    	return ENOMEM;
    }

    /* Save the network device in the private area */
    erPrivate = netdev_priv( dev );
    erPrivate->dev = dev;

    /* Register network interface */
    if ((result = register_netdev (dev)))
    {
    	printk ("eRouter: Error %d initializing logical eRouter device\n",result);
    	return result;
    }

    /* Register eRouter network interface to DOCSIS bridge */
    DbridgeEsafe_AddNetDev( dev, DBR_ESAFE_EPS_EROUTER, er_rx );


    printk ("eRouter: Network interface registered successfully\n");
    return 0;
}

void __exit er_cleanup (void)
{
    struct net_device *dev;

    /* Get the pointer to the network device */
    dev = dev_get_by_name(&init_net, netdevname );
    dev_put(dev);

    if ( dev )
    {
        /* Unregister network interface */
        unregister_netdev (dev);

        printk ("eRouter: Cleaning up Network interface\n");
    }
    else
    {
        printk ("eRouter: Failed to clean up Network interface\n");
    }
}

MODULE_AUTHOR("Texas Instruments, Inc");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("eRouter test Network interface");

module_init (er_init_module);
module_exit (er_cleanup);


