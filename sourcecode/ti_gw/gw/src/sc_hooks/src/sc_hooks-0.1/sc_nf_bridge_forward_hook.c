/*

Copyright (c) 2013-2014 ARRIS Enterprises, LLC

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

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <net/tcp.h>
#include <linux/tcp.h>
#include <linux/in6.h>
#include <linux/ipv6.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

MODULE_LICENSE("Dual BSD/GPL");

//#define SC_NF_BRIDGE_FORWARD_HOOK_DEBUG
#ifdef SC_NF_BRIDGE_FORWARD_HOOK_DEBUG
#define DPRINTK(format, argument...) printk(format,##argument)
#else
#define DPRINTK(format, argument...)
#endif

#define DROP 0
#define PASS 1

#define DHCP_S_PORT 67
#define DHCP_C_PORT 68
#define DHCP6CLIENT	546
#define DHCP6SERVER	547

#define MAX_GRE 8

#define HOTSPOTMTU	1400

#define GRE_TCPMSS 1360

typedef struct xwifi_gre_s {
    char listen_if[IFNAMSIZ];
    char forward_if[IFNAMSIZ];
    struct list_head list; /* kernel's list structure */
} xwifi_gre_t;

typedef struct gre_dscp_mark_s {
    char listen_if[IFNAMSIZ];
    char mark[5];
    struct list_head list; /* kernel's list structure */
} gre_dscp_mark_t;

typedef struct gre_change_tcpmss_s {
    char listen_if[IFNAMSIZ];
    char tcpmss[5];
    struct list_head list; /* kernel's list structure */
} gre_change_tcpmss_t;

typedef struct gre_ip_fragment_s {
    char listen_if[IFNAMSIZ];
    char enabled[2];
    struct list_head list; /* kernel's list structure */
} gre_ip_fragment_t;

static xwifi_gre_t xwifi_gre_listhead;
static gre_dscp_mark_t gre_dscp_mark_listhead;
static gre_change_tcpmss_t gre_change_tcpmss_listhead;
static gre_ip_fragment_t gre_ip_fragment_listhead;
static struct proc_dir_entry *gre_dhcp_filter;
static struct proc_dir_entry *gre_dscp_mark;
static struct proc_dir_entry *gre_change_tcpmss;
static DEFINE_SPINLOCK(gre_list_lock);
unsigned long gre_list_lock_flags;

extern int (*sc_nf_bridge_forward_hook)(const struct net_device *indev, struct sk_buff *skb);

#if 0
static void DumpData(char *data, unsigned int len)
{
    char *p = (char *)data;
    int i;
    for(i=1;i<=len;i++)
    {
        printk("%02x ", *(p+(i-1)));

        if( (i%16) == 0 )
        {
            printk("\n");
        }
    }
    printk("\n");
}
#endif 

static int xwifi_mark_process(const struct net_device *indev, struct sk_buff *skb)
{
	struct net_device *dev = skb->dev; /*to dev*/

	char name_string[32];
	char vlan_string[4];

	int index = 0;
	gre_dscp_mark_t* gre_mark_info;

//	printk("[SKBUFF]LINE=%d|indev->name=%s| dev->name=%s\n",__LINE__, indev->name, dev->name);

	memset(name_string, 0, sizeof(name_string));
	memset(vlan_string, 0, sizeof(vlan_string));

	strncpy(name_string, dev->name, sizeof(name_string));

	spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
    list_for_each_entry(gre_mark_info, &(gre_dscp_mark_listhead.list), list) {
        if(strncmp(indev->name, gre_mark_info->listen_if, sizeof(indev->name))==0) { /*client to server*/
			//printk("[SKBUFF]LINE=%d\n",__LINE__);
			
			if(strlen(gre_mark_info->mark)!=0)
			{
				//printk("[SKBUFF]LINE=%d|gre_mark_info->mark=%s\n",__LINE__, gre_mark_info->mark);
				skb->mark = simple_strtoul(gre_mark_info->mark,NULL,16);
				//printk("[SKBUFF]LINE=%d|skb->mark=%lx\n",__LINE__, skb->mark);
			}			
        }        
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
	
	return 0;
	
}

/*
** used to filter DHCPv4 packet which is sent by user(gre_dhcp_relay will send the same packet with option 82)
** return 0(DROP): drop packet
** return 1(PASS): keep going
*/
static int xwifi_dhcpv4_handler(const struct net_device *indev, struct sk_buff *skb)
{
    struct net_device *dev = skb->dev; /*to dev*/
    struct iphdr *iph = ip_hdr(skb);
    struct udphdr *udph = (struct udphdr *)((unsigned char*)iph + ((iph->ihl)*4));
    int i;
    xwifi_gre_t* xwifi_gre;

    spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
    list_for_each_entry(xwifi_gre, &(xwifi_gre_listhead.list), list) {
        if( (strncmp(indev->name, xwifi_gre->listen_if, sizeof(indev->name))==0) &&
            (strncmp(dev->name, xwifi_gre->forward_if, sizeof(dev->name))==0) ) { /*LAN to WAN*/

            if( skb->protocol == htons(ETH_P_IP) && iph->protocol == IPPROTO_UDP) { /*it's UDP packet*/
                if( ntohs(udph->dest) == DHCP_S_PORT ) { /* client to server, handled by gre_dhcp_relay */
                    DPRINTK("DROP it: DHCP packet, client to server\n");
                    goto DROP_PACKET;
                }

                /*FIX PROD00218463: avoid dhcp messages flooding, drop all DHCP packets in Xwifi bridge*/
                if( ntohs(udph->dest) == DHCP_C_PORT ) { /* server to client, forbidden */
                    DPRINTK("DROP it: DHCP packet, forbidden\n");
                    goto DROP_PACKET;
                }
                /*END FIX PROD00218463*/
            }
        }
        
        if( (strncmp(indev->name, xwifi_gre->forward_if, sizeof(indev->name))==0) &&
            (strncmp(dev->name, xwifi_gre->listen_if, sizeof(dev->name))==0) ) { /*WAN to LAN*/

            if( skb->protocol == htons(ETH_P_IP) && iph->protocol == IPPROTO_UDP) { /*it's UDP packet*/
                if( ntohs(udph->dest) == DHCP_C_PORT ) { /*server to client, handled by gre_dhcp_relay*/
                    DPRINTK("DROP it: DHCP packet, server to client\n");
                    goto DROP_PACKET;
                }

                /*FIX PROD00218463: avoid dhcp messages flooding, drop all DHCP packets in Xwifi bridge*/
                if( ntohs(udph->dest) == DHCP_S_PORT ) { /* client to server, forbidden */
                    DPRINTK("DROP it: DHCP packet, forbidden\n");
                    goto DROP_PACKET;
                }
                /*END FIX PROD00218463*/
            }
        }
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    return PASS; 

DROP_PACKET:
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    return DROP;
}

/*
** used to filter DHCPv6 packet which is sent by user(gre_dhcp6_relay will send the same packet with option 18 & 37)
** return 0(DROP): drop packet
** return 1(PASS): keep going
*/

static int xwifi_dhcpv6_handler(const struct net_device *indev, struct sk_buff *skb)
{
    struct net_device *dev = skb->dev; /*to dev*/
	struct ipv6hdr *ip6hr = (struct ipv6hdr*)(skb->mac_header + sizeof(struct ethhdr));
    struct udphdr *udph = (struct udphdr *)(ip6hr + 1);
    int i;
    xwifi_gre_t* xwifi_gre;

    spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
    list_for_each_entry(xwifi_gre, &(xwifi_gre_listhead.list), list) {
        if( (strncmp(indev->name, xwifi_gre->listen_if, sizeof(indev->name))==0) &&
            (strncmp(dev->name, xwifi_gre->forward_if, sizeof(dev->name))==0) ) { /*LAN to WAN*/

            if( skb->protocol == htons(ETH_P_IPV6) && ip6hr->nexthdr == IPPROTO_UDP) { /*it's UDP packet*/
                if( ntohs(udph->dest) == DHCP6SERVER ) { /* client to server, handled by gre_dhcp6_relay */
                    DPRINTK("DROP it: DHCPv6 packet, client to server\n");
                    goto DROP_PACKET;
                }

                /*FIX PROD00218463: avoid dhcp messages flooding, drop all DHCP packets in Xwifi bridge*/
                if( ntohs(udph->dest) == DHCP6CLIENT ) { /* server to client, forbidden */
                    DPRINTK("DROP it: DHCPv6 packet, forbidden\n");
                    goto DROP_PACKET;
                }
                /*END FIX PROD00218463*/
            }
        }
        
        if( (strncmp(indev->name, xwifi_gre->forward_if, sizeof(indev->name))==0) &&
            (strncmp(dev->name, xwifi_gre->listen_if, sizeof(dev->name))==0) ) { /*WAN to LAN*/
            if( skb->protocol == htons(ETH_P_IPV6) && ip6hr->nexthdr == IPPROTO_UDP) { /*it's UDP packet*/

                if( ntohs(udph->dest) == DHCP6CLIENT ) { /*server to client, handled by gre_dhcp6_relay*/
                    DPRINTK("DROP it: DHCPv6 packet, server to client\n");
                    goto DROP_PACKET;
                }

                /*FIX PROD00218463: avoid dhcp messages flooding, drop all DHCP packets in Xwifi bridge*/
                if( ntohs(udph->dest) == DHCP6SERVER ) { /* client to server, forbidden */
                    DPRINTK("DROP it: DHCPv6 packet, forbidden\n");
                    goto DROP_PACKET;
                }
                /*END FIX PROD00218463*/
            }
        }
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    return PASS; 

DROP_PACKET:
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    return DROP;	
}

static int xwifi_dhcpvx_handler_main(const struct net_device *indev, struct sk_buff *skb)
{
	if(skb->protocol == htons(ETH_P_IP))
		return xwifi_dhcpv4_handler(indev, skb);
	else if(skb->protocol == htons(ETH_P_IPV6))
		return xwifi_dhcpv6_handler(indev, skb);
    else
    {
        return PASS;
    }
}

static inline unsigned int
optlen(const u_int8_t *opt, unsigned int offset)
{
        /* Beware zero-length options: make finite progress */
        if (opt[offset] <= TCPOPT_NOP || opt[offset+1] == 0) return 1;
        else return opt[offset+1];
}


static u_int16_t
cheat_check(u_int32_t oldvalinv, u_int32_t newval, u_int16_t oldcheck)
{
        u_int32_t diffs[] = { oldvalinv, newval };
        return csum_fold(csum_partial((char *)diffs, sizeof(diffs),
                                      oldcheck^0xFFFF));
}

static void xwifi_tcpmss_modify(struct tcphdr *tcph, char *tcpmss)
{
int i;
	u_int16_t oldmss, newmss;

        if (tcph->syn == 1) // Got SYN message
	    {
	        u_int8_t *opt;
	        opt = (u_int8_t *)tcph;
	        for (i = sizeof(struct tcphdr); i < tcph->doff*4; i += optlen(opt, i)){
    	        if ((opt[i] == TCPOPT_MSS))// MSS option
    	        {
    	        //printk("[SKBUFF]%s %d|OPTION=%u|LENGTH=%u|VALUE 1=%u|VALUE 2=%u \n", __FILE__, __LINE__ , ntohs(opt[i]), ntohs(opt[i+1]), ntohs(opt[i+2]), ntohs(opt[i+3]) );

                    oldmss = (opt[i+2] << 8) | opt[i+3];
				newmss = simple_strtoul(tcpmss,NULL,10);
    
    	            opt[i+2] = (newmss & 0xff00) >> 8;
                    opt[i+3] = (newmss & 0x00ff);
    
                    tcph->check = cheat_check(htons(oldmss)^0xFFFF,
                    htons(newmss),
                    tcph->check);
    
                    //printk("[SKBUFF 2]%s %d|OPTION=%u|LENGTH=%u|VALUE 1=%u|VALUE 2=%u \n", __FILE__, __LINE__ , ntohs(opt[i]), ntohs(opt[i+1]), ntohs(opt[i+2]), ntohs(opt[i+3]) );
    	            break;
    	         }
	         }
	     }
	}

static void xwifi_tcpmss_handler_v4(const struct net_device *indev, struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph;
	gre_change_tcpmss_t* gre_tcpmss_info;
	char tcpmss[5];
	memset(tcpmss, 0, sizeof(tcpmss));
	
	//tcph = (void *)iph + ntohs(iph->ihl<<2);
	spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
	
	list_for_each_entry(gre_tcpmss_info, &(gre_change_tcpmss_listhead.list), list) {
		if(strncmp(indev->name, gre_tcpmss_info->listen_if, sizeof(indev->name))==0) { /*client to server*/
			if (iph->protocol == IPPROTO_TCP) {
				tcph = (void *)iph + ntohs(iph->ihl<<2);
				strncpy(tcpmss,gre_tcpmss_info->tcpmss,sizeof(tcpmss));
								  //printk("[SKBUFF]%s %d,| TCP SP=%u, TCP DP=%u \n", __FILE__, __LINE__ , ntohs(tcph->source), ntohs(tcph->dest));
				xwifi_tcpmss_modify(tcph, tcpmss);

			}
		}
	}
	spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
}

static void xwifi_tcpmss_handler_v6(const struct net_device *indev, struct sk_buff *skb)
{
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
//	struct tcphdr *tcph;
	struct tcphdr *tcph = (void *)ipv6h + sizeof(struct ipv6hdr);
	gre_change_tcpmss_t* gre_tcpmss_info;
	char tcpmss[5];
	memset(tcpmss, 0, sizeof(tcpmss));

	spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
	
	list_for_each_entry(gre_tcpmss_info, &(gre_change_tcpmss_listhead.list), list) {
		if(strncmp(indev->name, gre_tcpmss_info->listen_if, sizeof(indev->name))==0) { /*client to server*/
			if (ipv6h->nexthdr == IPPROTO_TCP) {
				//tcph = (void *)iph + ntohs(iph->ihl<<2);
				strncpy(tcpmss,gre_tcpmss_info->tcpmss,sizeof(tcpmss));

								  //printk("[SKBUFF]%s %d,| TCP SP=%u, TCP DP=%u \n", __FILE__, __LINE__ , ntohs(tcph->source), ntohs(tcph->dest));
				xwifi_tcpmss_modify(tcph, tcpmss);

			}
		}
	}
	spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
}

static int xwifi_tcpmss_handler_main(const struct net_device *indev, struct sk_buff *skb)
{
    struct net_device *dev = skb->dev; /*to dev*/
		
	if(skb->protocol == htons(ETH_P_IP))
		xwifi_tcpmss_handler_v4(indev, skb);
	else if(skb->protocol == htons(ETH_P_IPV6))
		xwifi_tcpmss_handler_v6(indev, skb);
	else{	
		//printk("%s %d, Not ipv4/v6 packet\n", __FUNCTION__, __LINE__ );
		return 0;
	}
	
    return 0; 
}

//#define GRE_DBG
int print_skb_data(struct sk_buff *skb2, char *str, int len)
{
			int i =0;
			unsigned char *qq;
			qq = skb2->data;
			printk("\n\n");
			printk("%s skb data=", str);
			for (i=0;i<len;i++) {
				printk("%02x ", *qq);
				qq++;
			}
			printk("\n\n");
}

static int xwifi_ipv4_fragment_gre(const struct net_device *indev, struct sk_buff *skb)
{
   struct net *net = dev_net(skb->dev);
	unsigned int mtu, hlen, left, len, ll_rs;
	int ptr, offset, raw = 0, err = 0, pk_num = 0;
   __be16 not_last_frag;
	struct iphdr *iph;
	struct net_device *dev;
	struct sk_buff *skb2;
  	struct rtable *rt = skb_rtable(skb);
   struct ethhdr *mac_head, *skb2_mac;


#ifdef GRE_DBG
	printk("skb->mac_header= %x skb->network_header =%x skb->transport_header=%x\n",skb_mac_header(skb), skb_network_header(skb) , skb_transport_header(skb)); 
#endif

	mac_head = eth_hdr(skb);
	iph = (struct iphdr*)skb->data;
	hlen = iph->ihl * 4;
	skb_reset_network_header(skb);
	skb_set_transport_header(skb, hlen);

#ifdef GRE_DBG
	printk("## Enter fragment\n");
#endif


	mtu = GRE_TCPMSS; /* Size of data space */
	IPCB(skb)->flags |= IPSKB_FRAG_COMPLETE;

#ifdef GRE_DBG
	print_skb_data(skb, "[Original SKB]", 100);
#endif

	left = skb->len - hlen;		/* Space per frame */
	ptr = hlen;		/* Where to start from */

	/* for bridged IP traffic encapsulated inside f.e. a vlan header,
	 * we need to make room for the encapsulating header
	 */
	ll_rs = LL_RESERVED_SPACE_EXTRA(rt->dst.dev, 0);

	/*
	 *	Fragment the datagram.
	 */

	offset = (ntohs(iph->frag_off) & IP_OFFSET) << 3;
	not_last_frag = iph->frag_off & htons(IP_MF);

	/*
	 *	Keep copying data until we run out.
	 */

	while (left > 0) {
		len = left;
		/* IF: it doesn't fit, use 'mtu' - the data space left */
		if (len > mtu)
			len = mtu;
		/* IF: we are not sending up to and including the packet end
		   then align the next start on an eight byte boundary */
		if (len < left)	{
			len &= ~7;
		}
		/*
		 *	Allocate buffer.
		 */

		if ((skb2 = alloc_skb(len+hlen+ll_rs, GFP_ATOMIC)) == NULL) {
			NETDEBUG(KERN_INFO "IP: frag: no memory for new fragment!\n");
			err = -ENOMEM;
			goto fail;
		}

		/*
		 *	Set up data on packet
		 */

		//ip_copy_metadata(skb2, skb);
	   skb2->pkt_type = skb->pkt_type;
    	skb2->priority = skb->priority;
    	skb2->protocol = skb->protocol;
    	skb_dst_drop(skb2);
    	skb_dst_copy(skb2, skb);
    	skb2->dev = skb->dev;
    	skb2->mark = skb->mark;

    	/* Copy the flags to each fragment. */
    	IPCB(skb2)->flags = IPCB(skb)->flags;
      
      nf_copy(skb2, skb);
      skb_copy_secmark(skb2, skb);
  
		skb_reserve(skb2, ll_rs);
		skb_put(skb2, len + hlen);
		skb_reset_network_header(skb2);
		skb2->transport_header = skb2->network_header + hlen;

		/*
		 *	Charge the memory for the fragment to any owner
		 *	it might possess
		 */

		if (skb->sk)
			skb_set_owner_w(skb2, skb->sk);

		/*
		 *	Copy the packet header into the new buffer.
		 */

		skb_copy_from_linear_data(skb, skb_network_header(skb2), hlen);

		/*
		 *	Copy a block of the IP datagram.
		 */
		if (skb_copy_bits(skb, ptr, skb_transport_header(skb2), len))
			BUG();
		left -= len;

		/*
		 *	Fill in the new header fields.
		 */
		iph = ip_hdr(skb2);
		iph->frag_off = htons((offset >> 3));

		/* ANK: dirty, but effective trick. Upgrade options only if
		 * the segment to be fragmented was THE FIRST (otherwise,
		 * options are already fixed) and make it ONCE
		 * on the initial skb, so that all the following fragments
		 * will inherit fixed options.
		 */
		if (offset == 0)
			ip_options_fragment(skb);

		/*
		 *	Added AC : If we are fragmenting a fragment that's not the
		 *		   last fragment then keep MF on each bit
		 */
		if (left > 0 || not_last_frag)
			iph->frag_off |= htons(IP_MF);
		ptr += len;
		offset += len;

		/*
		 *	Put this fragment into the sending queue.
		 */
		iph->tot_len = htons(len + hlen);

		ip_send_check(iph);

		skb2->pp_packet_info.flags |= TI_HIL_PACKET_FLAG_PP_SESSION_BYPASS;
    
		skb_push(skb2, ETH_HLEN);
       skb_reset_mac_header(skb2);
       skb2_mac = eth_hdr(skb2);
       memcpy(skb2_mac, mac_head, sizeof(struct ethhdr));

		pk_num++;
    
#ifdef GRE_DBG
	   printk("skb2->mac_header= %x skb2->network_header =%x skb2->transport_header=%x\n",skb_mac_header(skb2), skb_network_header(skb2) , skb_transport_header(skb2)); 
		print_skb_data(skb2, "[###sc_legre:Ready to dev_queue_xmit ###]", iph->tot_len);
		printk("skb->protocol=%02x, irqs=%d\n",ntohs(skb->protocol), irqs_disabled());
#endif		

		err = dev_queue_xmit(skb2);
		if (err) {
			printk("Output Error and goto fail\n");
			goto fail;
		}

		IP_INC_STATS(net, IPSTATS_MIB_FRAGCREATES);
	}
	IP_INC_STATS(net, IPSTATS_MIB_FRAGOKS);
	return err;

fail:
	IP_INC_STATS(net, IPSTATS_MIB_FRAGFAILS);
	return err;
}

static int xwifi_ip_fragment_handler_main(const struct net_device *indev, struct sk_buff *skb)
{
    struct net_device *dev = skb->dev; /*to dev*/
    struct iphdr *iph;
    gre_ip_fragment_t* gre_ip_fragment_info;

	spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
    list_for_each_entry(gre_ip_fragment_info, &(gre_ip_fragment_listhead.list), list) {
        if(strncmp(indev->name, gre_ip_fragment_info->listen_if, sizeof(indev->name))==0) { /*client to server*/
			//printk("[SKBUFF]LINE=%d\n",__LINE__);
			
			if(strlen(gre_ip_fragment_info->enabled)!=0)
			{
#if 1
            	if(skb->protocol == htons(ETH_P_IP))
              { 
                  iph = ip_hdr(skb);
                  
            		if (iph->tot_len > HOTSPOTMTU && !skb_is_gso(skb))
                {  
                    //printk("%s %d, Big Packet need fragment.\n", __FUNCTION__, __LINE__ );
                    /*unlock irq before sending out packets with dev_queue_xmit */
                    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
                    xwifi_ipv4_fragment_gre(indev, skb);
                    return DROP; 
                }
              }
#endif
			}			
        }        
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/

    return PASS; 
}

/*
** return 0(DROP): drop packet
** return 1(PASS): keep going
*/
static int sc_nf_bridge_forward_handler(const struct net_device *indev, struct sk_buff *skb)
{
    if(xwifi_dhcpvx_handler_main(indev, skb)==DROP) {
        return DROP;
    }

	xwifi_tcpmss_handler_main(indev, skb);

	xwifi_mark_process(indev, skb);

    /*since all the ip fragments have been sent out, drop old packet.
    NOTE: keep ip_fragment at the bottom of this function*/
    if(xwifi_ip_fragment_handler_main(indev, skb) == DROP)
    {
       return DROP;
    }

    return PASS;
}

static int gre_dscp_mark_read(char* page, char **start, off_t offset, int count, int *eof, void *data)
{
    int limit = count;
    char* buf = page + offset;
    int len=0;
	gre_dscp_mark_t* gre_mark_info;
//    DPRINTK("offset=%d\n",offset);
//    DPRINTK("limit=%d\n",limit);
    len += snprintf(buf+len, limit-len, "Usage:\n");
    len += snprintf(buf+len, limit-len, "    Add: A <listen interface> <dscp mark>\n");
    len += snprintf(buf+len, limit-len, "    Del: D <listen interface> <dscp mark>\n");
    len += snprintf(buf+len, limit-len, "\n");
    len += snprintf(buf+len, limit-len, "Current GRE:\n");
    len += snprintf(buf+len, limit-len, " <listen interface>  <--->  <dscp mark>\n");

	spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags);
	list_for_each_entry(gre_mark_info, &(gre_dscp_mark_listhead.list), list) {
        len += snprintf(buf+len, limit-len, " %18.18s  <--->  %s\n", gre_mark_info->listen_if, gre_mark_info->mark);
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags);

    return len;
   
}

static int gre_dscp_mark_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char cmd[128];
    char action[128], listen_if[128], mark[128];
    gre_dscp_mark_t* entry;
    gre_dscp_mark_t* temp_entry;

    if (count > (sizeof(cmd) - 1)) {
        return -EFAULT;
    }

    memset(cmd, 0, sizeof(cmd));
    memset(action, 0, sizeof(action));
    memset(listen_if, 0, sizeof(listen_if));
    memset(mark, 0, sizeof(mark));
    
    if (copy_from_user(cmd, buffer, count)) {
        return -EFAULT;
    }
    cmd[count] = '\0';

    sscanf(cmd,"%s %s %s", action, listen_if, mark);

    DPRINTK("action=%s\n", action);
    DPRINTK("listen_if=%s\n", listen_if);
    DPRINTK("mark=%s\n", mark);

    if( strncmp(action, "A", sizeof(action)) == 0 ) { /*Add*/
        entry = kmalloc(sizeof(gre_dscp_mark_t), GFP_KERNEL);
		strncpy(entry->listen_if, listen_if, sizeof(entry->listen_if));
        strncpy(entry->mark, mark, sizeof(entry->mark));
        INIT_LIST_HEAD(&(entry->list));

        spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
        list_add_tail(&(entry->list), &(gre_dscp_mark_listhead.list)); /*modify list*/
        spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    }
    else if (strncmp( action, "D", sizeof(action)) == 0 ) { /*Del*/
        spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
        list_for_each_entry_safe(entry, temp_entry, &(gre_dscp_mark_listhead.list), list) {
            if( strncmp(listen_if, entry->listen_if, sizeof(listen_if))==0 ) { /*match, del it*/
                list_del(&entry->list);
                kfree(entry);
            }
        }
        spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    }
    else {
        printk("command not correct\n");
        return -EFAULT;
    }

    return count;

}

static int gre_change_tcpmss_read(char* page, char **start, off_t offset, int count, int *eof, void *data)
{
    int limit = count;
    char* buf = page + offset;
    int len=0;
	gre_change_tcpmss_t* gre_tcpmss_info;
//    DPRINTK("offset=%d\n",offset);
//    DPRINTK("limit=%d\n",limit);
    len += snprintf(buf+len, limit-len, "Usage:\n");
    len += snprintf(buf+len, limit-len, "    Add: A <listen interface> <tcpmss>\n");
    len += snprintf(buf+len, limit-len, "    Del: D <listen interface> <tcpmss>\n");
    len += snprintf(buf+len, limit-len, "\n");
    len += snprintf(buf+len, limit-len, "Current GRE:\n");
    len += snprintf(buf+len, limit-len, " <listen interface>  <--->  <tcpmss>\n");

	spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags);
	list_for_each_entry(gre_tcpmss_info, &(gre_change_tcpmss_listhead.list), list) {
        len += snprintf(buf+len, limit-len, " %18.18s  <--->  %s\n", gre_tcpmss_info->listen_if, gre_tcpmss_info->tcpmss);
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags);

    return len;
   
}

static int gre_change_tcpmss_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char cmd[128];
	char action[128], listen_if[128], tcpmss[128];
    	
	gre_change_tcpmss_t* entry;
    gre_change_tcpmss_t* temp_entry;

    if (count > (sizeof(cmd) - 1)) {
        return -EFAULT;
    }

    memset(cmd, 0, sizeof(cmd));
    memset(action, 0, sizeof(action));
    memset(listen_if, 0, sizeof(listen_if));    
	memset(tcpmss, 0, sizeof(tcpmss));
    
    if (copy_from_user(cmd, buffer, count)) {
        return -EFAULT;
    }
    cmd[count] = '\0';

	sscanf(cmd,"%s %s %s", action, listen_if, tcpmss);

    DPRINTK("action=%s\n", action);
    DPRINTK("listen_if=%s\n", listen_if);
    DPRINTK("tcpmss=%s\n", tcpmss);

    if( strncmp(action, "A", sizeof(action)) == 0 ) { /*Add*/
		entry = kmalloc(sizeof(gre_change_tcpmss_t), GFP_KERNEL);        
		strncpy(entry->listen_if, listen_if, sizeof(entry->listen_if));
		strncpy(entry->tcpmss, tcpmss, sizeof(entry->tcpmss));
        INIT_LIST_HEAD(&(entry->list));

        spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
        list_add_tail(&(entry->list), &(gre_change_tcpmss_listhead.list)); /*modify list*/
        spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    }
    else if (strncmp( action, "D", sizeof(action)) == 0 ) { /*Del*/
        spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
        list_for_each_entry_safe(entry, temp_entry, &(gre_change_tcpmss_listhead.list), list) {
            if( strncmp(listen_if, entry->listen_if, sizeof(listen_if))==0 ) { /*match, del it*/
                list_del(&entry->list);
                kfree(entry);
            }
        }
        spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    }
    else {
        printk("command not correct\n");
        return -EFAULT;
    }

    return count;

}


static int gre_dhcp_filter_read(char* page, char **start, off_t offset, int count, int *eof, void *data)
{
    int limit = count;
    char* buf = page + offset;
    int len=0;
    xwifi_gre_t* xwifi_gre;
//    DPRINTK("offset=%d\n",offset);
//    DPRINTK("limit=%d\n",limit);
    len += snprintf(buf+len, limit-len, "Usage:\n");
    len += snprintf(buf+len, limit-len, "    Add: A <listen interface> <forward interface>\n");
    len += snprintf(buf+len, limit-len, "    Del: D <listen interface> <forward interface>\n");
    len += snprintf(buf+len, limit-len, "\n");
    len += snprintf(buf+len, limit-len, "Current GRE:\n");
    len += snprintf(buf+len, limit-len, " <listen interface>  <--->  <forward interface>\n");

    spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags);
    list_for_each_entry(xwifi_gre, &(xwifi_gre_listhead.list), list) {
        len += snprintf(buf+len, limit-len, " %18.18s  <--->  %s\n", xwifi_gre->listen_if, xwifi_gre->forward_if);
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags);

    return len;
   
}

static int gre_dhcp_filter_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char cmd[128];
    char action[128], listen_if[128], forward_if[128];
    xwifi_gre_t* gre;
    xwifi_gre_t* temp_gre;

    if (count > (sizeof(cmd) - 1)) {
        return -EFAULT;
    }

    memset(cmd, 0, sizeof(cmd));
    memset(action, 0, sizeof(action));
    memset(listen_if, 0, sizeof(listen_if));
    memset(forward_if, 0, sizeof(forward_if));

    if (copy_from_user(cmd, buffer, count)) {
        return -EFAULT;
    }
    cmd[count] = '\0';
    
    sscanf(cmd,"%s %s %s", action, listen_if, forward_if);
    
    DPRINTK("action=%s\n", action);
    DPRINTK("listen_if=%s\n", listen_if);
    DPRINTK("forward_if=%s\n", forward_if);

    if( strncmp(action, "A", sizeof(action)) == 0 ) { /*Add*/
        gre = kmalloc(sizeof(xwifi_gre_t), GFP_KERNEL);
		strncpy(gre->listen_if, listen_if, sizeof(gre->listen_if));
        strncpy(gre->forward_if, forward_if, sizeof(gre->forward_if));
        INIT_LIST_HEAD(&(gre->list));

        spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
        list_add_tail(&(gre->list), &(xwifi_gre_listhead.list)); /*modify list*/
        spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    }
    else if (strncmp( action, "D", sizeof(action)) == 0 ) { /*Del*/
        spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
        list_for_each_entry_safe(gre, temp_gre, &(xwifi_gre_listhead.list), list) {
            if( (strncmp(listen_if, gre->listen_if, sizeof(listen_if))==0) &&
            (strncmp(forward_if, gre->forward_if, sizeof(forward_if))==0) ) { /*match, del it*/
                list_del(&gre->list);
                kfree(gre);
            }
        }
        spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    }
    else {
        printk("command not correct\n");
        return -EFAULT;
    }

    return count;
}

static int gre_ip_fragment_read(char* page, char **start, off_t offset, int count, int *eof, void *data)
{
    int limit = count;
    char* buf = page + offset;
    int len=0;
    gre_ip_fragment_t* gre_ip_fragment_info;
//    DPRINTK("offset=%d\n",offset);
//    DPRINTK("limit=%d\n",limit);
    len += snprintf(buf+len, limit-len, "Usage:\n");
    len += snprintf(buf+len, limit-len, "    Add: A <listen interface> <Enabled>\n");
    len += snprintf(buf+len, limit-len, "    Del: D <listen interface> <Enabled>\n");
    len += snprintf(buf+len, limit-len, "\n");
    len += snprintf(buf+len, limit-len, "Current GRE:\n");
    len += snprintf(buf+len, limit-len, " <listen interface>  <--->  <Enabled>\n");

    spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags);
    list_for_each_entry(gre_ip_fragment_info, &(gre_ip_fragment_listhead.list), list) {
        len += snprintf(buf+len, limit-len, " %18.18s  <--->  %s\n", gre_ip_fragment_info->listen_if, gre_ip_fragment_info->enabled);
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags);

    return len;
   
}

static int gre_ip_fragment_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    char cmd[128];
    char action[128], listen_if[128], enabled[128];
    gre_ip_fragment_t* entry;
    gre_ip_fragment_t* temp_entry;

    if (count > (sizeof(cmd) - 1)) {
        return -EFAULT;
    }

    memset(cmd, 0, sizeof(cmd));
    memset(action, 0, sizeof(action));
    memset(listen_if, 0, sizeof(listen_if));
    memset(enabled, 0, sizeof(enabled));
    
    if (copy_from_user(cmd, buffer, count)) {
        return -EFAULT;
    }
    cmd[count] = '\0';

    sscanf(cmd,"%s %s %s", action, listen_if, enabled);

    DPRINTK("action=%s\n", action);
    DPRINTK("listen_if=%s\n", listen_if);
    DPRINTK("enabled=%s\n", enabled);

    if( strncmp(action, "A", sizeof(action)) == 0 ) { /*Add*/

        spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
        list_for_each_entry_safe(entry, temp_entry, &(gre_ip_fragment_listhead.list), list) {
            if( strncmp(listen_if, entry->listen_if, sizeof(listen_if))==0 ) { /*match, don't add it*/
                spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
                return count;
            }
        }
        spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/

        
        entry = kmalloc(sizeof(gre_ip_fragment_t), GFP_KERNEL);
		strncpy(entry->listen_if, listen_if, sizeof(entry->listen_if));
        strncpy(entry->enabled, enabled, sizeof(entry->enabled));
        INIT_LIST_HEAD(&(entry->list));

        spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
        list_add_tail(&(entry->list), &(gre_ip_fragment_listhead.list)); /*modify list*/
        spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    }
    else if (strncmp( action, "D", sizeof(action)) == 0 ) { /*Del*/
        spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
        list_for_each_entry_safe(entry, temp_entry, &(gre_ip_fragment_listhead.list), list) {
            if( strncmp(listen_if, entry->listen_if, sizeof(listen_if))==0 ) { /*match, del it*/
                list_del(&entry->list);
                kfree(entry);
            }
        }
        spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
    }
    else {
        printk("command not correct\n");
        return -EFAULT;
    }
    return count;
}
static int __init init(void)
{
    INIT_LIST_HEAD(&(xwifi_gre_listhead.list));
    gre_dhcp_filter = create_proc_entry("gre_dhcp_filter" ,0666, NULL);
    gre_dhcp_filter->read_proc = gre_dhcp_filter_read;
    gre_dhcp_filter->write_proc = gre_dhcp_filter_write; 

	INIT_LIST_HEAD(&(gre_dscp_mark_listhead.list));
	gre_dscp_mark = create_proc_entry("gre_dscp_mark" ,0666, NULL);
    gre_dscp_mark->read_proc = gre_dscp_mark_read;
    gre_dscp_mark->write_proc = gre_dscp_mark_write; 

	INIT_LIST_HEAD(&(gre_change_tcpmss_listhead.list));
	gre_change_tcpmss = create_proc_entry("gre_change_tcpmss" ,0666, NULL);
    gre_change_tcpmss->read_proc = gre_change_tcpmss_read;
    gre_change_tcpmss->write_proc = gre_change_tcpmss_write; 

	INIT_LIST_HEAD(&(gre_ip_fragment_listhead.list));
	gre_change_tcpmss = create_proc_entry("gre_ip_fragment" ,0666, NULL);
    gre_change_tcpmss->read_proc = gre_ip_fragment_read;
    gre_change_tcpmss->write_proc = gre_ip_fragment_write; 

    sc_nf_bridge_forward_hook = sc_nf_bridge_forward_handler;
    return 0;
}

static void __exit fini(void)
{
    xwifi_gre_t* gre;
    xwifi_gre_t* temp_gre;

    gre_dscp_mark_t* dscp;
    gre_dscp_mark_t* temp_dscp;
    
	gre_change_tcpmss_t* tcpmss;
	gre_change_tcpmss_t* temp_tcpmss;

   gre_ip_fragment_t* fragment;
   gre_ip_fragment_t* temp_fragment;
    
    sc_nf_bridge_forward_hook = NULL;
    
    if(gre_dhcp_filter) {
        remove_proc_entry("gre_dhcp_filter", NULL);
		remove_proc_entry("gre_dscp_mark", NULL);
		remove_proc_entry("gre_change_tcpmss", NULL);
       remove_proc_entry("gre_ip_fragment", NULL);
    }

    spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
    list_for_each_entry_safe(gre, temp_gre, &(xwifi_gre_listhead.list), list) {
        list_del(&gre->list);
        kfree(gre);
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/

    spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
    list_for_each_entry_safe(dscp, temp_dscp, &(gre_dscp_mark_listhead.list), list) {
        list_del(&dscp->list);
        kfree(dscp);
    }
	spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
  
	spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
	list_for_each_entry_safe(tcpmss, temp_tcpmss, &(gre_change_tcpmss_listhead.list), list) {
        list_del(&tcpmss->list);
        kfree(tcpmss);
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/

	spin_lock_irqsave(&gre_list_lock, gre_list_lock_flags); /*lock*/
	list_for_each_entry_safe(fragment, temp_fragment, &(gre_ip_fragment_listhead.list), list) {
        list_del(&fragment->list);
        kfree(fragment);
    }
    spin_unlock_irqrestore(&gre_list_lock, gre_list_lock_flags); /*unlock*/
}

module_init(init);
module_exit(fini);

EXPORT_SYMBOL(sc_nf_bridge_forward_handler);
