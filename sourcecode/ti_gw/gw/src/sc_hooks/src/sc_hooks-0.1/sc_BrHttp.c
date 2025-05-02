/*

Copyright (c) 2013-2015 ARRIS Enterprises, LLC

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

#define EXPORT_SYMTAB

#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODEVERSIONS
#endif

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/moduleparam.h>
#include <linux/netfilter.h>
#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>
#include <linux/inetdevice.h>
#include <net/dst.h>
#include <net/arp.h>
#include <linux/spinlock.h>

MODULE_LICENSE("Dual BSD/GPL");

/* 
 * This module is designed to handle HTTP requests under bridge mode
 * */
extern int (*bridge_skb_rx_hook)(struct sk_buff *skb);
static char *brif = "br0";
#ifdef CONFIG_MACH_PUMA5
static char *ethif = "eth0.2";
#else
static char *ethif = "l2sd0.2";
#endif

module_param(ethif, charp, S_IRUSR);
module_param(brif, charp, S_IRUSR);


static struct net_device *l2sd02_dev=NULL;
static struct net_device *br0_dev=NULL;

#ifdef DEBUG
#define PRINTK(format,argument...) printk(format,##argument)
#else
#define PRINTK(format,argument...)
#endif

static u32 target_ip = 0;

static int CheckTargetIP(struct sk_buff *skb)
{
    struct ethhdr* skb_ethhdr = NULL;
    struct iphdr *iph;

    if (skb->protocol == __constant_htons(ETH_P_IP)) {

        iph = ip_hdr(skb); 
        skb_ethhdr = (struct ethhdr *)skb->mac_header;

        if (target_ip == iph->daddr) {
            /* Change dest MAC to br MAC */
            skb_ethhdr->h_dest[0]=br0_dev->dev_addr[0];
            skb_ethhdr->h_dest[1]=br0_dev->dev_addr[1];
            skb_ethhdr->h_dest[2]=br0_dev->dev_addr[2];
            skb_ethhdr->h_dest[3]=br0_dev->dev_addr[3];
            skb_ethhdr->h_dest[4]=br0_dev->dev_addr[4];
            skb_ethhdr->h_dest[5]=br0_dev->dev_addr[5];
            PRINTK("SRC MAC=%x:%x:%x:%x:%x:%x\n",
                    skb_ethhdr->h_source[0],skb_ethhdr->h_source[1],
                    skb_ethhdr->h_source[2],skb_ethhdr->h_source[3],
                    skb_ethhdr->h_source[4],skb_ethhdr->h_source[5]);
            PRINTK("Dest MAC=%x:%x:%x:%x:%x:%x\n",
                    skb_ethhdr->h_dest[0],skb_ethhdr->h_dest[1],
                    skb_ethhdr->h_dest[2],skb_ethhdr->h_dest[3],
                    skb_ethhdr->h_dest[4],skb_ethhdr->h_dest[5]);
        }
    }
    return 0;
}

static int sc_rx_BrHttp_handler(struct sk_buff *skb)
{
	if(l2sd02_dev == NULL)
		l2sd02_dev = __dev_get_by_name(&init_net, ethif);
	if(br0_dev == NULL)
		br0_dev = __dev_get_by_name(&init_net, brif);

	if (skb->dev == l2sd02_dev) {
		CheckTargetIP(skb);
	}
	return 0; 
}

static int __init sc_BrHttp_init_module(void)
{
	struct in_device *in_dev;
	struct in_ifaddr *ifa = NULL;

	if(l2sd02_dev == NULL)
		l2sd02_dev = __dev_get_by_name(&init_net, ethif);

	if(br0_dev == NULL)
		br0_dev = __dev_get_by_name(&init_net, brif);

	if (br0_dev && (in_dev=(struct in_device *)br0_dev->ip_ptr)!= NULL) {
        if (in_dev->ifa_list)
        {
    		ifa = in_dev->ifa_list;
    		target_ip = ifa->ifa_address;
        }
	}

	if(l2sd02_dev == NULL || br0_dev == NULL || target_ip==0)
	{
		printk("\nsc_BrHttp module init : Fail\n");
		return 0;
	}

    bridge_skb_rx_hook = sc_rx_BrHttp_handler;
	
	printk("sc_BrHttp module init, ethif=%s, brif=%s \n",ethif,brif);

	return 0;
}

static void __exit sc_BrHttp_cleanup_module(void)
{
    bridge_skb_rx_hook = NULL;

	printk("sc_BrHttp module : Remove!\n");
}

module_init(sc_BrHttp_init_module);
module_exit(sc_BrHttp_cleanup_module);
