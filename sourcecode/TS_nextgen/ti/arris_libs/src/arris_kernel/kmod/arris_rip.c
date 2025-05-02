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

/* Kernel module API */
#include "arris_mod.h"

/* arris kernel hooks api */
#include "arris_mod_api.h"

typedef struct _rip_subnet_t {
    unsigned int        addr;
    unsigned int        prefix;
    unsigned int        mask;
    struct list_head    list;
} rip_subnet_t;

static struct proc_dir_entry *pd_rip_subnets = NULL;

static struct net_device* wan0_dev     = NULL;
static struct net_device* erouter0_dev = NULL;
LIST_HEAD( rip_subnets );

/* Hook prototypes */
#ifndef CONFIG_MACH_PUMA5
static int arris_rip_create_pp_session_v2(AVALANCHE_PP_SESSION_INFO_t *ptr_session, void *ptr);
#endif
static int arris_rip_create_pp_session( TI_PP_SESSION* ptr_session, void *ptr );
static int arris_rip_src_mac(struct sk_buff *skb);
static int arris_rip_dst_mac(struct sk_buff *skb);

/* utility functions */
static int isRipSubnet( unsigned int ip );
static int isRouterMac( unsigned char* mac );
static int isWanMac(    unsigned char* mac );

#if !defined(CONFIG_ARM_AVALANCHE_PPD)
ARRISMOD_DEFINE( int, arris_rip_create_pp_session_hook, TI_PP_SESSION*, void* ); /* UNIHAN ADD for PROD00222629 */
#endif
#ifndef CONFIG_MACH_PUMA5
#if !defined(CONFIG_ARM_AVALANCHE_PDSP_PP)
ARRISMOD_DEFINE(int, arris_rip_create_pp_session_hook_v2, AVALANCHE_PP_SESSION_INFO_t*, void* );
#endif
#endif
ARRISMOD_EXTERN( int, arris_rip_check_src_mac_hook, struct sk_buff *skb );
ARRISMOD_EXTERN( int, arris_rip_check_dst_mac_hook, struct sk_buff *skb );
ARRISMOD_EXTERN( int, arris_rip_create_pp_session_hook, TI_PP_SESSION*, void* );
ARRISMOD_EXTERN( int, arris_rip_subnet_check_hook, unsigned int ip );
#ifndef CONFIG_MACH_PUMA5
ARRISMOD_EXTERN(int, arris_rip_create_pp_session_hook_v2, AVALANCHE_PP_SESSION_INFO_t*, void* );
#endif

static int rip_read_subnets(char* page, char** start, off_t offset, int count, int* eof, void* data)
{
    rip_subnet_t* sn;
    int           len=0;
        
    len += sprintf(page+len, "Currently registered RIP subnets\n" );
    list_for_each_entry(sn, &rip_subnets, list)
    {
        // wow - kernel 2.6.36 has a print formatter for IPv4 address
        len += sprintf(page+len, "\t%pI4/%d\n", &sn->addr, sn->mask);
    }

    *eof = 1;
    return len;
}

void arris_rip_init( struct proc_dir_entry* root )
{
    /* create the proc file for debugging */
    if (root)
    {
        pd_rip_subnets = create_proc_entry("rip_subnets", 0644, root);
        if (pd_rip_subnets)
        {
            pd_rip_subnets->read_proc  = rip_read_subnets;
            pd_rip_subnets->write_proc = NULL;
        }
    }

    /* get the needed interfaces */
    wan0_dev       = __dev_get_by_name(&init_net, "wan0");
    erouter0_dev   = __dev_get_by_name(&init_net, "erouter0");

    /* Install the hooks */
    arris_rip_check_src_mac_hook        = arris_rip_src_mac;
    arris_rip_check_dst_mac_hook        = arris_rip_dst_mac;
    arris_rip_create_pp_session_hook    = arris_rip_create_pp_session;
#ifndef CONFIG_MACH_PUMA5
    arris_rip_create_pp_session_hook_v2 = arris_rip_create_pp_session_v2;
#endif
    arris_rip_subnet_check_hook          =isRipSubnet;
}

void arris_rip_deinit( struct proc_dir_entry* root )
{
    rip_subnet_t *ent, *next;

    /* remove the proc files */
    if (root && pd_rip_subnets)
    {
        remove_proc_entry("rip_subnets", root);
    }

    // delete the routed subnets list
    list_for_each_entry_safe(ent, next, &rip_subnets, list)
    {
        list_del(&ent->list);
        kfree( ent );
    }

    /* Remove the hooks */
    arris_rip_check_src_mac_hook        = NULL;
    arris_rip_check_dst_mac_hook        = NULL;
    arris_rip_create_pp_session_hook    = NULL;
#ifndef CONFIG_MACH_PUMA5
    arris_rip_create_pp_session_hook_v2 = NULL;
#endif
    arris_rip_subnet_check_hook   = NULL;
}

long arris_rip_add_subnet( arris_dev_t *dev, arris_add_rip_subnet_t* sn )
{
    rip_subnet_t *ent = NULL;
    int i;
    
    PRINT_INFO( DEBUG_RIP, "enter" );

    if( sn->addr ==0 ||  sn->prefix ==0)
    {
       PRINT_ERROR( DEBUG_RIP, "Ignore invalid RIP subnet: addr: %08x, mask: %08x \n", sn->addr , sn->prefix );
    }
    else
    {
    ent = kmalloc( sizeof( rip_subnet_t ), GFP_KERNEL );
    if( ent )
    {   
        memset( ent, 0, sizeof( rip_subnet_t ) );
        INIT_LIST_HEAD( &ent->list );
        ent->addr   = sn->addr;
        ent->prefix = sn->prefix;
        ent->mask   = 0;
        for (i=0; i<ent->prefix; i++)
        {
            ent->mask >>= 1;
            ent->mask |= 0x80000000;
        }
        list_add_tail( &ent->list, &rip_subnets );
    }
    }
        
    return 0;
}

long arris_rip_clear_subnets( arris_dev_t *dev )
{
    // delete the routed subnets list
    rip_subnet_t *ent, *next;

    PRINT_INFO( DEBUG_RIP, "enter" );

    list_for_each_entry_safe(ent, next, &rip_subnets, list)
    {
        list_del(&ent->list);
        kfree( ent );
    }
    
    return 0;
}

#ifndef CONFIG_MACH_PUMA5
// Packet Processor create session hook
static int arris_rip_create_pp_session_v2( AVALANCHE_PP_SESSION_INFO_t* ptr_session, void *ptr )
{    
    AVALANCHE_PP_INGRESS_SESSION_PROPERTY_t *ingress = &ptr_session->ingress;
    AVALANCHE_PP_EGRESS_SESSION_PROPERTY_t  *egress = &ptr_session->egress;
    unsigned int daddr;

    if (ingress->lookup.LUT1.u.fields.L3.enable_flags & AVALANCHE_PP_LUT1_FIELD_ENABLE_LAN_IPv4)
    {
        if (ingress->dev_type == AVALANCHE_PP_DEV_TYPE_WAN)
        {
            daddr = ingress->lookup.LUT1.u.fields.L3.LAN_addr_IP.v4;
        }
        else
        {
            daddr = ingress->lookup.LUT2.u.fields.WAN_addr_IP.v4;
        }

        if (isRipSubnet(daddr) && 
            isRouterMac(ingress->lookup.LUT1.u.fields.L2.dstmac))
        {
            PRINT_INFO( DEBUG_RIP, "PP session: change dest mac from %pM to %pM\n",
                     ingress->lookup.LUT1.u.fields.L2.dstmac, wan0_dev->dev_addr );
            ingress->lookup.LUT1.u.fields.L2.dstmac[3] = wan0_dev->dev_addr[3];
            ingress->lookup.LUT1.u.fields.L2.dstmac[4] = wan0_dev->dev_addr[4];
            ingress->lookup.LUT1.u.fields.L2.dstmac[5] = wan0_dev->dev_addr[5];
            
        }
        else if (isRipSubnet(egress->SRC_IP.v4) &&
            isRouterMac(egress->srcmac))
        {
            PRINT_INFO( DEBUG_RIP, "PP session: change src mac from %pM to %pM\n",
                     egress->srcmac, wan0_dev->dev_addr );

            egress->srcmac[3] = wan0_dev->dev_addr[3];
            egress->srcmac[4] = wan0_dev->dev_addr[4];
            egress->srcmac[5] = wan0_dev->dev_addr[5];
        }
            
    }

    return 0;
}
#endif

// Packet Processor create session hook
static int arris_rip_create_pp_session( TI_PP_SESSION* ptr_session, void *ptr )
{    
    // Check if we need to modify the session for TWC ECN7
    if ( ptr_session->ingress.l3l4_packet.packet_type == TI_PP_IPV4_TYPE )
    {
        if ( isRipSubnet( ptr_session->ingress.l3l4_packet.u.ipv4_desc.dst_ip ) &&
             isRouterMac( ptr_session->ingress.l2_packet.u.eth_desc.dstmac ) )
        {
            PRINT_INFO( DEBUG_RIP, "PP session: change dest mac from %pM to %pM\n",
                     ptr_session->ingress.l2_packet.u.eth_desc.dstmac, wan0_dev->dev_addr );

            ptr_session->ingress.l2_packet.u.eth_desc.dstmac[3] = wan0_dev->dev_addr[3];
            ptr_session->ingress.l2_packet.u.eth_desc.dstmac[4] = wan0_dev->dev_addr[4];
            ptr_session->ingress.l2_packet.u.eth_desc.dstmac[5] = wan0_dev->dev_addr[5];
        }
        /* Add for TWC CPR mode , modify the src MAC to wan0 because packet must send from wan0 */
        else if ( isRipSubnet( ptr_session->egress[0].l3l4_packet.u.ipv4_desc.src_ip) &&
                  isRouterMac( ptr_session->egress[0].l2_packet.u.eth_desc.srcmac ) ) 
        {
            PRINT_INFO( DEBUG_RIP, "PP session: change src mac from %pM to %pM\n",
                     ptr_session->egress[0].l2_packet.u.eth_desc.srcmac, wan0_dev->dev_addr );

            ptr_session->egress[0].l2_packet.u.eth_desc.srcmac[3] = wan0_dev->dev_addr[3];
            ptr_session->egress[0].l2_packet.u.eth_desc.srcmac[4] = wan0_dev->dev_addr[4];
            ptr_session->egress[0].l2_packet.u.eth_desc.srcmac[5] = wan0_dev->dev_addr[5];
        }
    }

    return 0;
}

static int arris_rip_src_mac( struct sk_buff *skb )
{
    struct ethhdr *eth;
    struct iphdr *iph;

    if ( !wan0_dev )
    {
        wan0_dev = __dev_get_by_name(&init_net, "wan0");
    }
    
    if ( !erouter0_dev )
    {
        erouter0_dev = __dev_get_by_name(&init_net, "erouter0");
    }

    if ( !wan0_dev || !erouter0_dev )
    {
        return 0;
    }

    if ( !(wan0_dev->flags & IFF_UP) || !(erouter0_dev->flags & IFF_UP) )
    {
        return 0;
    }
    
    if(skb->protocol != ETH_P_IP)
    {
        return 0;
    }
    
    eth = (struct ethhdr *)skb_mac_header(skb);
    iph = (struct iphdr *)skb_network_header(skb);
    
    if ( isRipSubnet( iph->saddr ) && isRouterMac( eth->h_source ) )
    {
        PRINT_INFO( DEBUG_RIP, "CNI TX: change source mac from %pM to %pM\n", eth->h_source, wan0_dev->dev_addr );
        
        /* Change the src MAC to wan0 */
        eth->h_source[3] = wan0_dev->dev_addr[3];
        eth->h_source[4] = wan0_dev->dev_addr[4];
        eth->h_source[5] = wan0_dev->dev_addr[5];
    }

    return 0;
}

static int arris_rip_dst_mac( struct sk_buff *skb )
{
    struct ethhdr *eth;
    struct iphdr *iph;

    if ( !wan0_dev )
    {
        wan0_dev = __dev_get_by_name(&init_net, "wan0");
    }

    if ( !erouter0_dev )
    {
        erouter0_dev = __dev_get_by_name(&init_net, "erouter0");
    }

    if ( !wan0_dev || !erouter0_dev )
    {
        return 0;
    }

    if ( !(wan0_dev->flags & IFF_UP) || !(erouter0_dev->flags & IFF_UP) )
    {
        return 0;
    }

    eth = (struct ethhdr *)skb_mac_header(skb);
    iph = (struct iphdr *)(skb_mac_header(skb) + ETH_HLEN);

    if(eth->h_proto != ETH_P_IP)
        return 0;

    if ( isRipSubnet( iph->daddr ) && isWanMac( eth->h_dest ) )
    {
        PRINT_INFO( DEBUG_RIP, "CNI RX: change dest mac from %pM to %pM\n", eth->h_dest, erouter0_dev->dev_addr );

        /* Change the dest MAC to erouter0 */
        eth->h_dest[3] = erouter0_dev->dev_addr[3];
        eth->h_dest[4] = erouter0_dev->dev_addr[4];
        eth->h_dest[5] = erouter0_dev->dev_addr[5];
    }

    return 0;
}

static int isRipSubnet( unsigned int ip )
{
    rip_subnet_t*   subnet = NULL;
    int             isRip = 0;

    list_for_each_entry(subnet, &rip_subnets, list)
    {
        if ( (ip & subnet->mask) == (subnet->addr & subnet->mask) )
        {
            PRINT_VERBOSE( DEBUG_RIP, "check ip %pI4 in subnet: %pI4/%d - MATCH\n", &ip, &subnet->addr, subnet->prefix );
            isRip = 1;
            break;
        }
        else
        {
            PRINT_VERBOSE( DEBUG_RIP, "check ip %pI4 in subnet: %pI4/%d - NO MATCH\n", &ip, &subnet->addr, subnet->prefix );
        }
    }

    return isRip;
}

static int isRouterMac( unsigned char* mac )
{
    if ( memcmp(mac, erouter0_dev->dev_addr, ETH_ALEN)==0 )
    {
        PRINT_VERBOSE( DEBUG_RIP, "check mac %pM against eRouter %pM - MATCH\n", mac, erouter0_dev->dev_addr );
        return 1;
    }
    else
    {
        PRINT_VERBOSE( DEBUG_RIP, "check mac %pM against eRouter %pM - NO MATCH\n", mac, erouter0_dev->dev_addr );
        return 0;
    }
}

static int isWanMac( unsigned char* mac )
{
    if ( memcmp(mac, wan0_dev->dev_addr, ETH_ALEN)==0 )
    {
        PRINT_VERBOSE( DEBUG_RIP, "check mac %pM against wan %pM - MATCH\n", mac, wan0_dev->dev_addr );
        return 1;
    }
    else
    {
        PRINT_VERBOSE( DEBUG_RIP, "check mac %pM against wan %pM - NO MATCH\n", mac, wan0_dev->dev_addr );
        return 0;
    }        
}
