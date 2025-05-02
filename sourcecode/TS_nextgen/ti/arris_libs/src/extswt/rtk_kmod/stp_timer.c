/*
 *	Spanning tree protocol; timer-related code
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
#include <linux/times.h>
#include <linux/ti_hil.h>

#include "swt_stp.h"

/* called under bridge lock */
static int swt_is_designated_for_some_port(const struct net_switch *swt)
{
	struct net_switch_port *p;

	list_for_each_entry(p, &swt->port_list, list) {
		if (p->state != STP_STATE_DISABLED &&
		    !memcmp(&p->designated_bridge, &swt->bridge_id, 8))
			return 1;
	}

	return 0;
}

static void swt_hello_timer_expired(unsigned long arg)
{
	struct net_switch *swt = (struct net_switch *)arg;

	swt_info(swt, "hello timer expired\n");

    spin_lock(&swt->lock);
	if (swt->dev->flags & IFF_UP) {
		swt_config_bpdu_generation(swt);

		mod_timer(&swt->hello_timer, round_jiffies(jiffies + swt->hello_time));
	}
	spin_unlock(&swt->lock);
}

static void swt_message_age_timer_expired(unsigned long arg)
{
	struct net_switch_port *p = (struct net_switch_port *) arg;
	struct net_switch *swt = p->swt;
	const bridge_id *id = &p->designated_bridge;
	int was_root;

	if (p->state == STP_STATE_DISABLED)
		return;

    swt_err(swt, "port %u neighbor %.2x%.2x.%pM lost\n",
        (unsigned) p->port_no,
        id->prio[0], id->prio[1], &id->addr);

	/*
	 * According to the spec, the message age timer cannot be
	 * running when we are the root bridge. So..  this was_root
	 * check is redundant. I'm leaving it in for now, though.
	 */
	spin_lock(&swt->lock);
	if (p->state == STP_STATE_DISABLED)
		goto unlock;
	was_root = swt_is_root_bridge(swt);

	swt_become_designated_port(p);
	swt_configuration_update(swt);
	swt_port_state_selection(swt);
	if (swt_is_root_bridge(swt) && !was_root)
		swt_become_root_bridge(swt);
 unlock:
	spin_unlock(&swt->lock);
}

static void swt_forward_delay_timer_expired(unsigned long arg)
{
	struct net_switch_port *p = (struct net_switch_port *) arg;
	struct net_switch *swt = p->swt;  

	swt_err(swt, "port %u forward delay timer\n",
		 (unsigned) p->port_no);

    spin_lock(&swt->lock);
	if (p->state == STP_STATE_LISTENING) {
		p->state = STP_STATE_LEARNING;
		mod_timer(&p->forward_delay_timer,
			  jiffies + swt->forward_delay);
	} else if (p->state == STP_STATE_LEARNING) {
		p->state = STP_STATE_FORWARDING;
		if (swt_is_designated_for_some_port(swt))
			swt_topology_change_detection(swt);
		netif_carrier_on(swt->dev);

//#ifdef CONFIG_TI_PACKET_PROCESSOR	
//		ti_hil_pp_event(TI_BRIDGE_PORT_FORWARD, (void *)p->dev);
//#endif //CONFIG_TI_PACKET_PROCESSOR
	}

    swt_log_state(p);
	spin_unlock(&swt->lock);
}

static void swt_tcn_timer_expired(unsigned long arg)
{
	struct net_switch *swt = (struct net_switch *) arg;

	swt_err(swt, "tcn timer expired\n");

    spin_lock(&swt->lock);
	if (swt->dev->flags & IFF_UP) {
		swt_transmit_tcn(swt);

		mod_timer(&swt->tcn_timer,jiffies + swt->bridge_hello_time);
	}
	spin_unlock(&swt->lock);
}

static void swt_topology_change_timer_expired(unsigned long arg)
{
	struct net_switch *swt = (struct net_switch *) arg;

	swt_err(swt, "topo change timer expired\n");
	spin_lock(&swt->lock);
	swt->topology_change_detected = 0;
	swt->topology_change = 0;
	spin_unlock(&swt->lock);
}

static void swt_hold_timer_expired(unsigned long arg)
{
	struct net_switch_port *p = (struct net_switch_port *) arg;

    swt_err(p->swt, "port %u hold timer expired\n", (unsigned) p->port_no);

	spin_lock(&p->swt->lock);
	if (p->config_pending)
		swt_transmit_config(p);
	spin_unlock(&p->swt->lock);
}

void swt_stp_timer_init(struct net_switch *swt)
{
	setup_timer(&swt->hello_timer, swt_hello_timer_expired,
		      (unsigned long)swt);

	setup_timer(&swt->tcn_timer, swt_tcn_timer_expired,
		      (unsigned long)swt);

	setup_timer(&swt->topology_change_timer,
		      swt_topology_change_timer_expired,
		      (unsigned long)swt);

//	setup_timer(&swt->gc_timer, swt_fdb_cleanup, (unsigned long)swt);
}

void swt_stp_port_timer_init(struct net_switch_port *p)
{
    setup_timer(&p->message_age_timer, swt_message_age_timer_expired,
		      (unsigned long)p);

	setup_timer(&p->forward_delay_timer, swt_forward_delay_timer_expired,
		      (unsigned long)p);

	setup_timer(&p->hold_timer, swt_hold_timer_expired,
		      (unsigned long)p);
}

/* Report ticks left (in USER_HZ) used for API */
unsigned long swt_timer_value(const struct timer_list *timer)
{
	return timer_pending(timer)
		? jiffies_to_clock_t(timer->expires - jiffies) : 0;
}
