/*

Copyright (c) 2013-2016 ARRIS Enterprises, LLC

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products 
   derived from this software without specific prior written permission.


Alternatively, this software may be distributed under the terms of the
GNU General Public License ("GPL") version 2 as published by the Free
Software Foundation. 
 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

GPLv2 license:

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License as published by the 
Free Software Foundation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details.

You should have received a copy of the GNU General Public License along with 
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/ti_ppm.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <br_private.h>

/* Kernel module API */
#include "arris_mod.h"

/* arris kernel hooks api */
#include "arris_mod_api.h"

// Defines
#ifdef CONFIG_MACH_PUMA6
#define IFNAME_L2SD0_2          "l2sd0.2"
#define IFNAME_L2SD0            "l2sd0"
#else
#define IFNAME_L2SD0_2          "eth0.2"
#define IFNAME_L2SD0            "eth0"
#endif
#define MAX_MAC_ADDR_STR_LEN    50

// Variables
static struct net_device *l2sd0_2_dev = NULL;
static struct net_device *l2sd0_dev = NULL;

// Function Declarations
ARRISMOD_EXTERN( int, arris_loop_detected_hook, unsigned char * );
static int arris_skb_rx_filter( struct sk_buff *skb );
static int arris_bridge_forward_filter( const struct sk_buff *skb, const struct net_bridge_port *p );
static void arris_bridge_free_dev(struct net_device *dev);

// Externs
ARRISMOD_EXTERN( int, arris_skb_rx_filter_hook, struct sk_buff* );
extern int DbridgeDb_GetCmtsMacAddr(unsigned char* mac_address);
ARRISMOD_EXTERN( int, arris_bridge_forward_filter_hook, const struct sk_buff*, const struct net_bridge_port *p );
extern struct net_device *vlan_dev_real_dev(const struct net_device *dev);
ARRISMOD_EXTERN( void, arris_bridge_free_dev_hook, struct net_device *dev );

void arris_bridge_init( void )
{
#if defined(CONFIG_SYSTEM_BROADCOMSWITCH) || defined(CONFIG_SYSTEM_REALTEKSWITCH)
    l2sd0_2_dev = dev_get_by_name( &init_net, IFNAME_L2SD0_2 );

    // Only adding the RX filter hook for Broadcom switches since Realtek switch has its own loop 
    // detect mechanism
    /* Install the hooks */
    arris_skb_rx_filter_hook        = arris_skb_rx_filter;
#endif

#if ( defined( CONFIG_MACH_PUMA6 ) && defined ( INCLUDE_INTEL_GW ) )
    /* add bridge forward hook to prevent network loops in bridge mode */
    /* PUMA6 GW loads only until tested on PUMA5 if needed */
    l2sd0_dev = dev_get_by_name( &init_net, IFNAME_L2SD0 );
    arris_bridge_forward_filter_hook = arris_bridge_forward_filter;
#endif

    /* this hook re-initializes the l2sd0_2 and l2sd0 pointers if their interfaces are unregistered. 
       initialize this hook only if either of the above hooks has been initialized */
    if( (arris_skb_rx_filter_hook) || (arris_bridge_forward_filter_hook) )
    {
        arris_bridge_free_dev_hook = arris_bridge_free_dev;
    }
}


void arris_bridge_deinit( void )
{
    /* Remove the hooks */
    arris_skb_rx_filter_hook        = NULL;
    arris_bridge_forward_filter_hook = NULL;
    arris_bridge_free_dev_hook = NULL;
}


#if defined(CONFIG_SYSTEM_BROADCOMSWITCH) || defined(CONFIG_SYSTEM_REALTEKSWITCH)
// Kernel hook function to filter out packets being received in __netif_receive_skb()
static int arris_skb_rx_filter( struct sk_buff *skb )
{
    struct ethhdr *eth;
    unsigned char cmtsMac[ETH_ALEN];

    if( l2sd0_2_dev == NULL )
    {
        // Grab L2SD0.2 device
        l2sd0_2_dev = dev_get_by_name( &init_net, IFNAME_L2SD0_2 );
    }

    // External switch loop detect code
    // If L2SD0.2 is up and this skb is being received by it
    if( l2sd0_2_dev && (l2sd0_2_dev->flags & IFF_UP) && (skb->dev == l2sd0_2_dev) )
    {
        // Grab CMTS MAC address
        DbridgeDb_GetCmtsMacAddr( cmtsMac );

        // Grab ptr to ETH header of packet
        eth = (struct ethhdr *)skb_mac_header(skb);

        // First look for a packet entering L2SD0.2 that was sourced from L2SD0.2.  This should
        // catch packet loops on devices in router mode
        if( (compare_ether_addr(l2sd0_2_dev->dev_addr, eth->h_source) == 0 ) )
        {
            //printk(KERN_ERR "LOOP DETECT1!!!!\n");
            // Call External Switch driver to search for MAC and shutdown port
            ARRISMOD_CALL( arris_loop_detected_hook, eth->h_source );
            kfree_skb(skb);
            return( 1 );
        }
        // Second look for a packet entering L2SD0.2 that was sourced from the CMTS MAC.  This
        // should catch packet loops on devices in both router and bridge mode
        else if( (compare_ether_addr(cmtsMac, eth->h_source) == 0 ) )
        {
            //printk(KERN_ERR "LOOP DETECT2!!!!\n");
            // Call External Switch driver to search for MAC and shutdown port
            ARRISMOD_CALL( arris_loop_detected_hook, eth->h_source );
            kfree_skb(skb);
            return( 1 );
        }
    }

    return( 0 );
}
#endif


/* Filter out "looped" packets from L2SD0 VLAN interfaces.  This happens when the modem is in
   global bridge mode and multiple interfaces are connected to br0.
   example: l2sd0.2 -> l2sd0.3. */
static int arris_bridge_forward_filter( const struct sk_buff *skb, const struct net_bridge_port *p )
{
    /* get l2sd0_dev if NULL */
    if( l2sd0_dev == NULL )
    {
        /* flood packet if l2sd0 is not ready */
        if( (l2sd0_dev = dev_get_by_name( &init_net, IFNAME_L2SD0 )) == NULL )
        {
            return 1;
        }
    }

    /* inspect packets when both interfaces are VLANS */
    if( (skb->dev->priv_flags & IFF_802_1Q_VLAN) && (p->dev->priv_flags & IFF_802_1Q_VLAN) )
    {
        /* if the "real_dev" device for transmit and receive 
           is l2sd0_dev, drop the packet to prevent loops */
        if( ((vlan_dev_real_dev(skb->dev)) == l2sd0_dev) && 
            ((vlan_dev_real_dev(p->dev)) == l2sd0_dev) )
        {
            return 0;
        }
    }

    return 1;
}

/* free device pointer whenever device is unregistered (called from unregister_netdevice_queue(), dev.c) */
void arris_bridge_free_dev(struct net_device *dev)
{
    if( l2sd0_dev && (l2sd0_dev == dev) )
    {
        dev_put(l2sd0_dev);
        l2sd0_dev = NULL;
    }
    else if( l2sd0_2_dev && (l2sd0_2_dev == dev)  )
    {
        dev_put(l2sd0_2_dev);
        l2sd0_2_dev = NULL;
    }
}
