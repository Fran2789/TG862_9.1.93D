/*
 *	Spanning tree protocol; BPDU handling
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/netfilter_bridge.h>
#include <linux/etherdevice.h>
#include <linux/llc.h>
#include <linux/slab.h>
#include <net/net_namespace.h>
#include <net/llc.h>
#include <net/llc_pdu.h>
#include <net/stp.h>
#include <asm/unaligned.h>

// SNIFF
#include <linux/ip.h>
#include <net/icmp.h>
#include <linux/igmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/route.h>

#include <linux/cat_l2switch_netdev.h>

#include "swt_stp.h"

#define STP_HZ		256
#define LLC_RESERVE sizeof(struct llc_pdu_un)

/* Need the l2sd0 device */
#define RTK_HLEN        (8)
#define RTK_ETHPROT     (0x8899)

struct rtk_ethhdr {
    unsigned short ethertype;
    unsigned char  protocol;
    unsigned char  reason;
    unsigned char  fid_pri;
    unsigned char  vsel_vidx;
    unsigned short ports;
} __packed;

static struct sk_buff *rtk_put_hdr(struct sk_buff *skb, int port)
{
	struct rtk_ethhdr *eth;
    unsigned char *rtk_pos;

    // This shouldn't need to do anything, as we already reserved
    // enough header space in skb_reserve()
	if (skb_cow_head(skb, RTK_HLEN) < 0) {
		kfree_skb(skb);
		return NULL;
	}

    // first, add space for the new header to the front of the header
    // (just moves the pointer)
	skb_push(skb, RTK_HLEN);

	// Now, move the mac addresses to the beginning of the packet.
    // so we have mac_dst, mac_src, [rtk header] original_ethtype
	memmove(skb->data, skb->data + RTK_HLEN, 2 * ETH_ALEN);
	skb->mac_header -= RTK_HLEN;

    // Set out rtk header structure pointer 
    // to just after the mac addresses */
    rtk_pos = skb->data + 2*ETH_ALEN;
    eth = (struct rtk_ethhdr *)rtk_pos;

    // fiill in the realtek proprietary header
	eth->ethertype = htons(RTK_ETHPROT);
	eth->protocol  = 0x04;
    eth->reason    = 0x00;
    eth->fid_pri   = 0x00;
    eth->vsel_vidx = 0x00;
    eth->ports     = (1 << port);

	skb->protocol = htons(RTK_ETHPROT);

	return skb;
}

static void swt_send_bpdu(struct net_switch_port *p,
                          const unsigned char *data, 
                          int length)
{
	struct sk_buff *skb;

	skb = dev_alloc_skb(length+LLC_RESERVE+RTK_HLEN);
	if (!skb) {
        printk( KERN_ERR "can't alloc SKB\n");
		return;
    }

	skb->dev = p->swt->dev;
	skb->protocol = htons(ETH_P_802_2);

    /* First reserve enough for LLC and the proprietary realtek header */
	skb_reserve(skb, LLC_RESERVE + RTK_HLEN);

    /* copy the data into the data section */
	memcpy(__skb_put(skb, length), data, length);

    /* push in the llc protocol */
	llc_pdu_header_init(skb, LLC_PDU_TYPE_U, LLC_SAP_BSPAN,
			    LLC_SAP_BSPAN, LLC_PDU_CMD);
	llc_pdu_init_as_ui_cmd(skb);
	llc_mac_hdr_init(skb, p->swt->dev->dev_addr, p->swt->group_addr);

    skb = rtk_put_hdr( skb, p->port_id );

    NF_HOOK(NFPROTO_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev, dev_queue_xmit);
}

static inline void swt_set_ticks(unsigned char *dest, int j)
{
	unsigned long ticks = (STP_HZ * j)/ HZ;

	put_unaligned_be16(ticks, dest);
}

static inline int swt_get_ticks(const unsigned char *src)
{
	unsigned long ticks = get_unaligned_be16(src);

	return DIV_ROUND_UP(ticks * HZ, STP_HZ);
}

/* called under bridge lock */
void swt_send_config_bpdu(struct net_switch_port *p, struct stp_config_bpdu *bpdu)
{
	unsigned char buf[35];

    if (p->swt->stp_enabled != BR_KERNEL_STP)
		return;

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = BPDU_TYPE_CONFIG;
	buf[4] = (bpdu->topology_change ? 0x01 : 0) |
		(bpdu->topology_change_ack ? 0x80 : 0);
	buf[5] = bpdu->root.prio[0];
	buf[6] = bpdu->root.prio[1];
	buf[7] = bpdu->root.addr[0];
	buf[8] = bpdu->root.addr[1];
	buf[9] = bpdu->root.addr[2];
	buf[10] = bpdu->root.addr[3];
	buf[11] = bpdu->root.addr[4];
	buf[12] = bpdu->root.addr[5];
	buf[13] = (bpdu->root_path_cost >> 24) & 0xFF;
	buf[14] = (bpdu->root_path_cost >> 16) & 0xFF;
	buf[15] = (bpdu->root_path_cost >> 8) & 0xFF;
	buf[16] = bpdu->root_path_cost & 0xFF;
	buf[17] = bpdu->bridge_id.prio[0];
	buf[18] = bpdu->bridge_id.prio[1];
	buf[19] = bpdu->bridge_id.addr[0];
	buf[20] = bpdu->bridge_id.addr[1];
	buf[21] = bpdu->bridge_id.addr[2];
	buf[22] = bpdu->bridge_id.addr[3];
	buf[23] = bpdu->bridge_id.addr[4];
	buf[24] = bpdu->bridge_id.addr[5];
	buf[25] = (bpdu->port_id >> 8) & 0xFF;
	buf[26] = bpdu->port_id & 0xFF;

	swt_set_ticks(buf+27, bpdu->message_age);
	swt_set_ticks(buf+29, bpdu->max_age);
	swt_set_ticks(buf+31, bpdu->hello_time);
	swt_set_ticks(buf+33, bpdu->forward_delay);

	swt_send_bpdu(p, buf, 35);
}

/* called under bridge lock */
void swt_send_tcn_bpdu(struct net_switch_port *p)
{
	unsigned char buf[4];

	if (p->swt->stp_enabled != BR_KERNEL_STP)
		return;

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = BPDU_TYPE_TCN;
	swt_send_bpdu(p, buf, 4);
}

/*
 * Called from l2sd0
 *
 */
void swt_stp_rcv(struct sk_buff *skb)
{
    struct rtk_ethhdr *rtk_eth;
    unsigned char *rtk_pos;
    unsigned int port_no;

    struct net_switch_port *p;
    struct net_switch *swt;
    const unsigned char *buf;

    const unsigned char *dest = eth_hdr(skb)->h_dest;

    /* First, grab and remove the rtk header */
    rtk_pos = (unsigned char *)eth_hdr(skb) + 2*ETH_ALEN;
    rtk_eth = (struct rtk_ethhdr*)rtk_pos;
    port_no = rtk_eth->ports;

    skb_pull(skb, RTK_HLEN);
    skb_reset_network_header(skb);
    buf = skb->data;
    {
        // bridge registers with LLC - so it gets it's skb with the llc stuff already processed, 
        // and data pointer moved past it - we'll do this manually (3 bytes)
        const struct llc_pdu_un *pdu = llc_pdu_un_hdr(skb);
        
        if (pdu->ssap != LLC_SAP_BSPAN ||
            pdu->dsap != LLC_SAP_BSPAN ||
            pdu->ctrl_1 != LLC_PDU_TYPE_U)
            goto err;
    
        // Pull the LLC header - skb_data now points at the STP content
        skb_pull(skb, LLC_PDU_LEN_U);
    
        // Now we have the STP protocol 
        if (!pskb_may_pull(skb, 4)) {
            goto err;
        }
    
        /* compare of protocol id and version */
        buf = skb->data;
        if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0) {
            printk( KERN_ERR "swt_stp_rcv : bad STP header %d %d %d\n", buf[0], buf[1], buf[2] );
            goto err;
        }
    
        // ugh - global switch device...
        swt = GLOBAL_SWITCH;
        p = swt_get_port(swt, port_no);
        spin_lock(&swt->lock);
    
        if (swt->stp_enabled != BR_KERNEL_STP) {
            goto out;
        }
    
        if (!(swt->dev->flags & IFF_UP)) {
            goto out;
        }
    
        if (p->state == STP_STATE_DISABLED) {
            goto out;
        }
    
        if (compare_ether_addr(dest, swt->group_addr) != 0) {
            goto out;
        }
    
        buf = skb_pull(skb, 3);
    
        if (buf[0] == BPDU_TYPE_CONFIG) {
            struct stp_config_bpdu bpdu;
    
            if (!pskb_may_pull(skb, 32)) {
                goto out;
            }
    
            buf = skb->data;
            bpdu.topology_change = (buf[1] & 0x01) ? 1 : 0;
            bpdu.topology_change_ack = (buf[1] & 0x80) ? 1 : 0;
    
            bpdu.root.prio[0] = buf[2];
            bpdu.root.prio[1] = buf[3];
            bpdu.root.addr[0] = buf[4];
            bpdu.root.addr[1] = buf[5];
            bpdu.root.addr[2] = buf[6];
            bpdu.root.addr[3] = buf[7];
            bpdu.root.addr[4] = buf[8];
            bpdu.root.addr[5] = buf[9];
            bpdu.root_path_cost =
                (buf[10] << 24) |
                (buf[11] << 16) |
                (buf[12] << 8) |
                buf[13];
            bpdu.bridge_id.prio[0] = buf[14];
            bpdu.bridge_id.prio[1] = buf[15];
            bpdu.bridge_id.addr[0] = buf[16];
            bpdu.bridge_id.addr[1] = buf[17];
            bpdu.bridge_id.addr[2] = buf[18];
            bpdu.bridge_id.addr[3] = buf[19];
            bpdu.bridge_id.addr[4] = buf[20];
            bpdu.bridge_id.addr[5] = buf[21];
            bpdu.port_id = (buf[22] << 8) | buf[23];
    
            bpdu.message_age = swt_get_ticks(buf+24);
            bpdu.max_age = swt_get_ticks(buf+26);
            bpdu.hello_time = swt_get_ticks(buf+28);
            bpdu.forward_delay = swt_get_ticks(buf+30);
    
            swt_received_config_bpdu(p, &bpdu);
        }
    
        else if (buf[0] == BPDU_TYPE_TCN) {
            swt_received_tcn_bpdu(p);
        }
    }

 out:
    spin_unlock(&swt->lock);

 err:
    kfree_skb(skb);
}

rx_handler_result_t swt_handle_frame(struct sk_buff *skb)
{
    /* Just handle the traped packets (with RTK HEADER)
     * pass the rest on
     */
    if ( eth_hdr(skb)->h_proto == htons(RTK_ETHPROT) ) {
        /* Pass this to STP */
        swt_stp_rcv( skb );
        return RX_HANDLER_CONSUMED;
    } else {
        return RX_HANDLER_PASS;
    }
}

