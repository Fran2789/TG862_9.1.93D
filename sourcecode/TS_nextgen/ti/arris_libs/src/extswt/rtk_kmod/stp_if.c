/*
 *	Spanning tree protocol; interface code
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
/* 
 * Includes Intel Corporation's changes/modifications dated: [11/07/2011].
* Changed/modified portions - Copyright © [2011], Intel Corporation.
*/

#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/ti_hil.h>
#include <net/stp.h>

#include "swt_stp.h"


/* Port id is composed of priority and port number.
 * NB: least significant bits of priority are dropped to
 *     make room for more ports.
 *     FOR SWITCH - just have port ID
 */
static inline port_id swt_make_port_id(__u8 priority, __u16 port_no)
{
	return ((u16)priority << SWT_PORT_BITS)
		| (port_no & ((1<<SWT_PORT_BITS)-1));
}

/* called under bridge lock */
void swt_init_port(struct net_switch_port *p)
{
	p->port_id = swt_make_port_id(p->priority, p->port_no);
	swt_become_designated_port(p);
	p->state = STP_STATE_BLOCKING;
	p->topology_change_ack = 0;
	p->config_pending = 0;
}

void swt_stp_enable_bridge(struct net_switch *swt)
{
	struct net_switch_port *p;

    spin_lock_bh(&swt->lock);

	mod_timer(&swt->hello_timer, jiffies + swt->hello_time);
	//mod_timer(&swt->gc_timer, jiffies + HZ/10);

    swt_config_bpdu_generation(swt);

	list_for_each_entry(p, &swt->port_list, list) {
        swt_stp_enable_port(p);
	}
	spin_unlock_bh(&swt->lock);
}

/* NO locks held */
void swt_stp_disable_bridge(struct net_switch *swt)
{
	struct net_switch_port *p;

	spin_lock_bh(&swt->lock);
	list_for_each_entry(p, &swt->port_list, list) {
		if (p->state != STP_STATE_DISABLED)
			swt_stp_disable_port(p);
	}

	swt->topology_change = 0;
	swt->topology_change_detected = 0;
	spin_unlock_bh(&swt->lock);

	del_timer_sync(&swt->hello_timer);
	del_timer_sync(&swt->topology_change_timer);
	del_timer_sync(&swt->tcn_timer);
	del_timer_sync(&swt->gc_timer);
}

/* called under bridge lock */
void swt_stp_enable_port(struct net_switch_port *p)
{
	swt_init_port(p);
	swt_port_state_selection(p->swt);
	swt_log_state(p);
}

/* called under bridge lock */
void swt_stp_disable_port(struct net_switch_port *p)
{
	struct net_switch *swt = p->swt;
	int wasroot;

	swt_log_state(p);

	wasroot = swt_is_root_bridge(swt);
	swt_become_designated_port(p);
	p->state = STP_STATE_DISABLED;
	p->topology_change_ack = 0;
	p->config_pending = 0;

	del_timer(&p->message_age_timer);
	del_timer(&p->forward_delay_timer);
	del_timer(&p->hold_timer);

    // no forwarding database here...
	// swt_fdb_delete_by_port(swt, p, 0);
	// swt_multicast_disable_port(p);

	swt_configuration_update(swt);

	swt_port_state_selection(swt);

#if 0
#ifdef CONFIG_TI_PACKET_PROCESSOR
    /* Generate the event indicating that the port has been disabled. */
    ti_hil_pp_event(TI_BRIDGE_PORT_DISABLED, (void *)p->dev);
#endif
#endif

    if (swt_is_root_bridge(swt) && !wasroot)
		swt_become_root_bridge(swt);
}

static void swt_stp_start(struct net_switch *swt)
{
	swt->stp_enabled = BR_KERNEL_STP;
	swt_debug(swt, "using kernel STP\n");

	/* To start timers on any ports left in blocking */
	spin_lock_bh(&swt->lock);
	swt_port_state_selection(swt);
	spin_unlock_bh(&swt->lock);
}

static void swt_stp_stop(struct net_switch *swt)
{
	swt->stp_enabled = BR_NO_STP;
}

void swt_stp_set_enabled(struct net_switch *swt, unsigned long val)
{
	ASSERT_RTNL();

	if (val) {
		if (swt->stp_enabled == BR_NO_STP)
			swt_stp_start(swt);
	} else {
		if (swt->stp_enabled != BR_NO_STP)
			swt_stp_stop(swt);
	}
}

/* called under bridge lock */
void swt_stp_change_bridge_id(struct net_switch *swt, const unsigned char *addr)
{
	/* should be aligned on 2 bytes for compare_ether_addr() */
	unsigned short oldaddr_aligned[ETH_ALEN >> 1];
	unsigned char *oldaddr = (unsigned char *)oldaddr_aligned;
	struct net_switch_port *p;
	int wasroot;

	wasroot = swt_is_root_bridge(swt);

	memcpy(oldaddr, swt->bridge_id.addr, ETH_ALEN);
	memcpy(swt->bridge_id.addr, addr, ETH_ALEN);
	memcpy(swt->dev->dev_addr, addr, ETH_ALEN);

	list_for_each_entry(p, &swt->port_list, list) {
		if (!compare_ether_addr(p->designated_bridge.addr, oldaddr))
			memcpy(p->designated_bridge.addr, addr, ETH_ALEN);

		if (!compare_ether_addr(p->designated_root.addr, oldaddr))
			memcpy(p->designated_root.addr, addr, ETH_ALEN);

	}

	swt_configuration_update(swt);
	swt_port_state_selection(swt);
	if (swt_is_root_bridge(swt) && !wasroot)
		swt_become_root_bridge(swt);
}

/* should be aligned on 2 bytes for compare_ether_addr() */
static unsigned short swt_mac_dummy_aligned[ETH_ALEN >> 1];

/* called under bridge lock */
bool swt_stp_recalculate_bridge_id(struct net_switch *swt)
{
    unsigned char *swt_mac_dummy = (unsigned char *)swt_mac_dummy_aligned;
    unsigned char *addr = swt_mac_dummy;
    swt_mac_dummy[0] = 0x00;
    swt_mac_dummy[1] = 0x01;
    swt_mac_dummy[2] = 0x02;
    swt_mac_dummy[3] = 0x03;
    swt_mac_dummy[4] = 0x04;
    swt_mac_dummy[5] = 0x05;

    /* mac address is address of the l2sw0 device */
	if (compare_ether_addr(swt->bridge_id.addr, addr) == 0)
		return false;	/* no change */

	swt_stp_change_bridge_id(swt, addr);
	return true;
}

/* called under bridge lock */
void swt_stp_set_bridge_priority(struct net_switch *swt, u16 newprio)
{
	struct net_switch_port *p;
	int wasroot;

	wasroot = swt_is_root_bridge(swt);

	list_for_each_entry(p, &swt->port_list, list) {
		if (p->state != STP_STATE_DISABLED &&
		    swt_is_designated_port(p)) {
			p->designated_bridge.prio[0] = (newprio >> 8) & 0xFF;
			p->designated_bridge.prio[1] = newprio & 0xFF;
		}
	}

	swt->bridge_id.prio[0] = (newprio >> 8) & 0xFF;
	swt->bridge_id.prio[1] = newprio & 0xFF;
	swt_configuration_update(swt);
	swt_port_state_selection(swt);
	if (swt_is_root_bridge(swt) && !wasroot)
		swt_become_root_bridge(swt);
}

/* called under bridge lock */
void swt_stp_set_port_priority(struct net_switch_port *p, u8 newprio)
{
	port_id new_port_id = swt_make_port_id(newprio, p->port_no);

	if (swt_is_designated_port(p))
		p->designated_port = new_port_id;

	p->port_id = new_port_id;
	p->priority = newprio;
	if (!memcmp(&p->swt->bridge_id, &p->designated_bridge, 8) &&
	    p->port_id < p->designated_port) {
		swt_become_designated_port(p);
		swt_port_state_selection(p->swt);
	}
}

/* called under bridge lock */
void swt_stp_set_path_cost(struct net_switch_port *p, u32 path_cost)
{
	p->path_cost = path_cost;
	swt_configuration_update(p->swt);
	swt_port_state_selection(p->swt);
}

ssize_t swt_show_bridge_id(char *buf, const struct bridge_id *id)
{
	return sprintf(buf, "%.2x%.2x.%.2x%.2x%.2x%.2x%.2x%.2x\n",
	       id->prio[0], id->prio[1],
	       id->addr[0], id->addr[1], id->addr[2],
	       id->addr[3], id->addr[4], id->addr[5]);
}
