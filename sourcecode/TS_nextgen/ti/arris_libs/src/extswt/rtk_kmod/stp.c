/*
 *	Spanning tree protocol; generic parts
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
#include <linux/rculist.h>
#include <linux/rtnetlink.h>
#include <linux/ti_hil.h>
#include <linux/cat_l2switch_netdev.h>

/* Realtek API for enamble / disable port, and read write STP state */
#include "rtk_api_ext.h"

#include "swt_stp.h"

/* since time values in bpdu are in jiffies and then scaled (1/256)
 * before sending, make sure that is at least one.
 */
#define MESSAGE_AGE_INCR	((HZ < 256) ? 1 : (HZ/256))

static const char *const switch_port_state_names[] = {
	[STP_STATE_DISABLED] = "disabled",
	[STP_STATE_LISTENING] = "listening",
	[STP_STATE_LEARNING] = "learning",
	[STP_STATE_FORWARDING] = "forwarding",
	[STP_STATE_BLOCKING] = "blocking",
};

void swt_log_state(const struct net_switch_port *p)
{
	swt_info( p->swt, "port %u entering %s state\n", (unsigned)p->port_no, switch_port_state_names[p->state]);
}

/* called under bridge lock */
struct net_switch_port *swt_get_port(struct net_switch *swt, u16 port_no)
{
	struct net_switch_port *p;

	list_for_each_entry_rcu(p, &swt->port_list, list) {
		if (p->port_no == port_no)
			return p;
	}

	return NULL;
}

/* called under bridge lock */
static int swt_should_become_root_port(const struct net_switch_port *p,
				      u16 root_port)
{
	struct net_switch *swt;
	struct net_switch_port *rp;
	int t;

	swt = p->swt;
	if (p->state == STP_STATE_DISABLED ||
	    swt_is_designated_port(p))
		return 0;

	if (memcmp(&swt->bridge_id, &p->designated_root, 8) <= 0)
		return 0;

	if (!root_port)
		return 1;

	rp = swt_get_port(swt, root_port);

	t = memcmp(&p->designated_root, &rp->designated_root, 8);
	if (t < 0)
		return 1;
	else if (t > 0)
		return 0;

	if (p->designated_cost + p->path_cost <
	    rp->designated_cost + rp->path_cost)
		return 1;
	else if (p->designated_cost + p->path_cost >
		 rp->designated_cost + rp->path_cost)
		return 0;

	t = memcmp(&p->designated_bridge, &rp->designated_bridge, 8);
	if (t < 0)
		return 1;
	else if (t > 0)
		return 0;

	if (p->designated_port < rp->designated_port)
		return 1;
	else if (p->designated_port > rp->designated_port)
		return 0;

	if (p->port_id < rp->port_id)
		return 1;

	return 0;
}

/* called under bridge lock */
static void swt_root_selection(struct net_switch *swt)
{
	struct net_switch_port *p;
	u16 root_port = 0;

	list_for_each_entry(p, &swt->port_list, list) {
		if (swt_should_become_root_port(p, root_port))
			root_port = p->port_no;

	}

	swt->root_port = root_port;

	if (!root_port) {
		swt->designated_root = swt->bridge_id;
		swt->root_path_cost = 0;
	} else {
		p = swt_get_port(swt, root_port);
		swt->designated_root = p->designated_root;
		swt->root_path_cost = p->designated_cost + p->path_cost;
	}
}

/* called under bridge lock */
void swt_become_root_bridge(struct net_switch *swt)
{
	swt->max_age = swt->bridge_max_age;
	swt->hello_time = swt->bridge_hello_time;
	swt->forward_delay = swt->bridge_forward_delay;
	swt_topology_change_detection(swt);
	del_timer(&swt->tcn_timer);

	if (swt->dev->flags & IFF_UP) {
		swt_config_bpdu_generation(swt);
		mod_timer(&swt->hello_timer, jiffies + swt->hello_time);
	}
}

/* called under bridge lock */
void swt_transmit_config(struct net_switch_port *p)
{
	struct stp_config_bpdu bpdu;
	struct net_switch *swt;

	if (timer_pending(&p->hold_timer)) {
		p->config_pending = 1;
		return;
	}

	swt = p->swt;

	bpdu.topology_change = swt->topology_change;
	bpdu.topology_change_ack = p->topology_change_ack;
	bpdu.root = swt->designated_root;
	bpdu.root_path_cost = swt->root_path_cost;
	bpdu.bridge_id = swt->bridge_id;
	bpdu.port_id = p->port_id;
	if (swt_is_root_bridge(swt))
		bpdu.message_age = 0;
	else {
		struct net_switch_port *root
			= swt_get_port(swt, swt->root_port);
		bpdu.message_age = swt->max_age
			- (root->message_age_timer.expires - jiffies)
			+ MESSAGE_AGE_INCR;
	}
	bpdu.max_age = swt->max_age;
	bpdu.hello_time = swt->hello_time;
	bpdu.forward_delay = swt->forward_delay;

	if (bpdu.message_age < swt->max_age) {
		swt_send_config_bpdu(p, &bpdu);
		p->topology_change_ack = 0;
		p->config_pending = 0;
		mod_timer(&p->hold_timer, round_jiffies(jiffies + STP_HOLD_TIME));
	}
}

/* called under bridge lock */
static inline void swt_record_config_information(struct net_switch_port *p,
						const struct stp_config_bpdu *bpdu)
{
	p->designated_root = bpdu->root;
	p->designated_cost = bpdu->root_path_cost;
	p->designated_bridge = bpdu->bridge_id;
	p->designated_port = bpdu->port_id;

	mod_timer(&p->message_age_timer, jiffies
		  + (p->swt->max_age - bpdu->message_age));
}

/* called under bridge lock */
static inline void swt_record_config_timeout_values(struct net_switch *swt,
					    const struct stp_config_bpdu *bpdu)
{
	swt->max_age = bpdu->max_age;
	swt->hello_time = bpdu->hello_time;
	swt->forward_delay = bpdu->forward_delay;
	swt->topology_change = bpdu->topology_change;
}

/* called under bridge lock */
void swt_transmit_tcn(struct net_switch *swt)
{
	swt_send_tcn_bpdu(swt_get_port(swt, swt->root_port));
}

/* called under bridge lock */
static int swt_should_become_designated_port(const struct net_switch_port *p)
{
	struct net_switch *swt;
	int t;

	swt = p->swt;
	if (swt_is_designated_port(p))
		return 1;

	if (memcmp(&p->designated_root, &swt->designated_root, 8))
		return 1;

	if (swt->root_path_cost < p->designated_cost)
		return 1;
	else if (swt->root_path_cost > p->designated_cost)
		return 0;

	t = memcmp(&swt->bridge_id, &p->designated_bridge, 8);
	if (t < 0)
		return 1;
	else if (t > 0)
		return 0;

	if (p->port_id < p->designated_port)
		return 1;

	return 0;
}

/* called under bridge lock */
static void swt_designated_port_selection(struct net_switch *swt)
{
	struct net_switch_port *p;

	list_for_each_entry(p, &swt->port_list, list) {
		if (p->state != STP_STATE_DISABLED &&
		    swt_should_become_designated_port(p))
			swt_become_designated_port(p);

	}
}

/* called under bridge lock */
static int swt_supersedes_port_info(struct net_switch_port *p, struct stp_config_bpdu *bpdu)
{
	int t;

	t = memcmp(&bpdu->root, &p->designated_root, 8);
	if (t < 0)
		return 1;
	else if (t > 0)
		return 0;

	if (bpdu->root_path_cost < p->designated_cost)
		return 1;
	else if (bpdu->root_path_cost > p->designated_cost)
		return 0;

	t = memcmp(&bpdu->bridge_id, &p->designated_bridge, 8);
	if (t < 0)
		return 1;
	else if (t > 0)
		return 0;

	if (memcmp(&bpdu->bridge_id, &p->swt->bridge_id, 8))
		return 1;

	if (bpdu->port_id <= p->designated_port)
		return 1;

	return 0;
}

/* called under bridge lock */
static inline void swt_topology_change_acknowledged(struct net_switch *swt)
{
	swt->topology_change_detected = 0;
	del_timer(&swt->tcn_timer);
}

/* called under bridge lock */
void swt_topology_change_detection(struct net_switch *swt)
{
	int isroot = swt_is_root_bridge(swt);

	if (swt->stp_enabled != BR_KERNEL_STP)
		return;

	swt_info( swt, "topology change detected, %s\n", isroot ? "propagating" : "sending tcn bpdu");

	if (isroot) {
		swt->topology_change = 1;
		mod_timer(&swt->topology_change_timer, jiffies
			  + swt->bridge_forward_delay + swt->bridge_max_age);
	} else if (!swt->topology_change_detected) {
		swt_transmit_tcn(swt);
		mod_timer(&swt->tcn_timer, jiffies + swt->bridge_hello_time);
	}

	swt->topology_change_detected = 1;
}

/* called under bridge lock */
void swt_config_bpdu_generation(struct net_switch *swt)
{
	struct net_switch_port *p;

	list_for_each_entry(p, &swt->port_list, list) {
		if (p->state != STP_STATE_DISABLED && swt_is_designated_port(p) ) {
			swt_transmit_config(p);
        }
	}
}

/* called under bridge lock */
static inline void swt_reply(struct net_switch_port *p)
{
	swt_transmit_config(p);
}

/* called under bridge lock */
void swt_configuration_update(struct net_switch *swt)
{
	swt_root_selection(swt);
	swt_designated_port_selection(swt);
}

/* called under bridge lock */
void swt_become_designated_port(struct net_switch_port *p)
{
	struct net_switch *swt;

	swt = p->swt;
	p->designated_root = swt->designated_root;
	p->designated_cost = swt->root_path_cost;
	p->designated_bridge = swt->bridge_id;
	p->designated_port = p->port_id;
}


/* called under bridge lock */
static void swt_make_blocking(struct net_switch_port *p)
{
	if (p->state != STP_STATE_DISABLED &&
	    p->state != STP_STATE_BLOCKING) {
		if (p->state == STP_STATE_FORWARDING ||
		    p->state == STP_STATE_LEARNING)
			swt_topology_change_detection(p->swt);

		p->state = STP_STATE_BLOCKING;
		swt_log_state(p);
		del_timer(&p->forward_delay_timer);
	}
}

/* called under bridge lock */
static void swt_make_forwarding(struct net_switch_port *p)
{
	struct net_switch *swt = p->swt;

	if (p->state != STP_STATE_BLOCKING)
		return;

	if (swt->stp_enabled == BR_NO_STP || swt->forward_delay == 0) {
		p->state = STP_STATE_FORWARDING;
		swt_topology_change_detection(swt);
		del_timer(&p->forward_delay_timer);
//#ifdef CONFIG_TI_PACKET_PROCESSOR	
//		ti_hil_pp_event(TI_BRIDGE_PORT_FORWARD, (void *)p->dev);
//#endif //CONFIG_TI_PACKET_PROCESSOR
	}
	else if (swt->stp_enabled == BR_KERNEL_STP)
		p->state = STP_STATE_LISTENING;
	else
		p->state = STP_STATE_LEARNING;

//	swt_multicast_enable_port(p);

	swt_log_state(p);

	if (swt->forward_delay != 0)
		mod_timer(&p->forward_delay_timer, jiffies + swt->forward_delay);
}


/* called under bridge lock */
void swt_port_state_selection(struct net_switch *swt)
{
	struct net_switch_port *p;
	unsigned int liveports = 0;

	/* Don't change port states if userspace is handling STP */
	if (swt->stp_enabled == BR_USER_STP)
		return;

	list_for_each_entry(p, &swt->port_list, list) {
		if (p->state == STP_STATE_DISABLED)
			continue;

		if (p->port_no == swt->root_port) {
			p->config_pending = 0;
			p->topology_change_ack = 0;
			swt_make_forwarding(p);
		} else if (swt_is_designated_port(p)) {
			del_timer(&p->message_age_timer);
			swt_make_forwarding(p);
		} else {
			p->config_pending = 0;
			p->topology_change_ack = 0;
			swt_make_blocking(p);
		}

		if (p->state == STP_STATE_FORWARDING)
			++liveports;
	}

	if (liveports == 0)
		netif_carrier_off(swt->dev);
	else
		netif_carrier_on(swt->dev);
}

/* called under bridge lock */
static inline void swt_topology_change_acknowledge(struct net_switch_port *p)
{
	p->topology_change_ack = 1;
	swt_transmit_config(p);
}

/* called under bridge lock */
void swt_received_config_bpdu(struct net_switch_port *p, struct stp_config_bpdu *bpdu)
{
	struct net_switch *swt;
	int was_root;

	swt = p->swt;
	was_root = swt_is_root_bridge(swt);

	if (swt_supersedes_port_info(p, bpdu)) {
        swt_record_config_information(p, bpdu);
		swt_configuration_update(swt);
		swt_port_state_selection(swt);

		if (!swt_is_root_bridge(swt) && was_root) {
            del_timer(&swt->hello_timer);
			if (swt->topology_change_detected) {
                del_timer(&swt->topology_change_timer);
				swt_transmit_tcn(swt);

				mod_timer(&swt->tcn_timer,
					  jiffies + swt->bridge_hello_time);
			}
		}

		if (p->port_no == swt->root_port) {
			swt_record_config_timeout_values(swt, bpdu);
			swt_config_bpdu_generation(swt);
			if (bpdu->topology_change_ack) {
				swt_topology_change_acknowledged(swt);
            }
		}
	} else if (swt_is_designated_port(p)) {
        swt_reply(p);
	}
}

/* called under bridge lock */
void swt_received_tcn_bpdu(struct net_switch_port *p)
{
	if (swt_is_designated_port(p)) {
		swt_topology_change_detection(p->swt);
		swt_topology_change_acknowledge(p);
	}
}


const u8 swt_group_address[ETH_ALEN] = { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };
struct net_switch *GLOBAL_SWITCH = NULL;

static int port_cost(unsigned int port)
{
    rtk_port_linkStatus_t LinkStatus;
    rtk_port_speed_t      Speed;
    rtk_port_duplex_t     Duplex;

    if ( rtk_port_phyStatus_get( port, &LinkStatus, &Speed, &Duplex ) )
    {
        switch (Speed) 
        {
            case PORT_SPEED_1000M:
				return 4;
			case PORT_SPEED_100M:
				return 19;
			case PORT_SPEED_10M:
				return 100;
            default:
                return 100;
		}
	}

	return 100;	/* assume old 10Mbps */
}

static struct net_switch_port *new_nsp(struct net_switch *swt, unsigned int port)
{
	struct net_switch_port *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return ERR_PTR(-ENOMEM);

	p->swt = swt;
	p->path_cost = port_cost(port);
	p->priority = 0x8000 >> SWT_PORT_BITS;
	p->port_no = port;
	p->flags = 0;
	swt_init_port(p);
	p->state = STP_STATE_DISABLED;
	swt_stp_port_timer_init(p);
	// br_multicast_add_port(p);

	return p;
}

extern rx_handler_result_t(*rtk_skb_rx_hook)(struct sk_buff *skb);

static struct net_switch *new_switch(struct net *net, const char *name)
{
	struct net_switch *swt;
	struct net_device *man_dev;

    man_dev = dev_get_by_name(net, L2SW_NETDEV_DATA0);
    swt = kmalloc( sizeof( struct net_switch ), GFP_KERNEL );

    if (swt) 
    {
        swt->dev = man_dev;

        spin_lock_init(&swt->lock);
        INIT_LIST_HEAD(&swt->port_list);
        // spin_lock_init(&swt->hash_lock);

        swt->bridge_id.prio[0] = 0x80;
        swt->bridge_id.prio[1] = 0x00;

        memcpy(swt->group_addr, swt_group_address, ETH_ALEN);

        // swt->feature_mask = dev->features;
        swt->stp_enabled = BR_NO_STP;
        swt->designated_root = swt->bridge_id;
        swt->root_path_cost = 0;
        swt->root_port = 0;
        swt->bridge_max_age = swt->max_age = 20 * HZ;
        swt->bridge_hello_time = swt->hello_time = 2 * HZ;
        swt->bridge_forward_delay = swt->forward_delay = 15 * HZ;
        swt->topology_change = 0;
        swt->topology_change_detected = 0;
        swt->ageing_time = 300 * HZ;

        swt_stp_timer_init(swt);
    }

    /* Setup the network device acting as the switch management device */
    /* NOTE: should this be more like the bridge code, where the switch
       is its own netdevice? For now, I don't think so.
     */
    rtk_skb_rx_hook = swt_handle_frame;
    // man_dev->priv_flags |= IFF_SWITCH_MGMT;

    /* should be new function */
    {
        struct net_switch_port *p;
        int    err = 0;
        bool   changed_addr;
        rtk_port_t              port;

        for (port = 0; port <= RTK_PHY_ID_MAX; port++)
        {
            p = new_nsp(swt, port);
            list_add_rcu(&p->list, &swt->port_list);

            spin_lock_bh(&swt->lock);
            changed_addr = swt_stp_recalculate_bridge_id(swt);
            // swt_features_recompute(swt);

            if ((man_dev->flags & IFF_UP) && netif_carrier_ok(man_dev))
                swt_stp_enable_port(p);
            spin_unlock_bh(&swt->lock);
        }
    }

    return swt;
}

void stp_init( void )
{
    // Setup STP, much like is done in br_if.c
    GLOBAL_SWITCH = new_switch( &init_net, "test" );

    swt_stp_enable_bridge(GLOBAL_SWITCH);
    swt_stp_set_enabled(GLOBAL_SWITCH, 1);
}

void stp_stop( void )
{
    swt_stp_set_enabled(GLOBAL_SWITCH, 0);
    swt_stp_disable_bridge(GLOBAL_SWITCH);

    kfree( GLOBAL_SWITCH );
    GLOBAL_SWITCH = NULL;
}
