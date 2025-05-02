/*
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

#ifndef _STP_H
#define _STP_H

#include <linux/netdevice.h>
#include <linux/if_ether.h>
#include <linux/if.h>

// #define IFF_SWITCH_MGMT 0x10000 // temp

#define SWT_PORT_BITS   10

typedef struct bridge_id bridge_id;
typedef struct mac_addr mac_addr;
typedef __u16 port_id;

#define STP_STATE_DISABLED      0
#define STP_STATE_LISTENING     1
#define STP_STATE_LEARNING      2
#define STP_STATE_FORWARDING    3
#define STP_STATE_BLOCKING      4

#define STP_HOLD_TIME (1*HZ)

#define swt_printk(level, swt, format, args...)	\
	printk(level "%s: " format, (swt)->dev->name, ##args)

#define swt_err(__swt, format, args...)			\
	swt_printk(KERN_ERR, __swt, format, ##args)
#define swt_warn(__swt, format, args...)			\
	swt_printk(KERN_WARNING, __swt, format, ##args)
#define swt_notice(__swt, format, args...)		\
	swt_printk(KERN_NOTICE, __swt, format, ##args)
#define swt_info(__swt, format, args...)			\
	swt_printk(KERN_INFO, __swt, format, ##args)

#define swt_debug(swt, format, args...)			\
	pr_debug("%s: " format,  (swt)->dev->name, ##args)


struct bridge_id
{
	unsigned char	prio[2];
	unsigned char	addr[6];
};

struct mac_addr
{
	unsigned char	addr[6];
};

struct net_switch
{
	spinlock_t			    lock;
	struct list_head		port_list;
	struct net_device		*dev;

//	spinlock_t			    hash_lock;
//	struct hlist_head		hash[BR_HASH_SIZE];
//	u32				        feature_mask;
//	unsigned long			flags;

	/* STP */
	bridge_id			    designated_root;
	bridge_id			    bridge_id;
	u32				        root_path_cost;
	unsigned long			max_age;
	unsigned long			hello_time;
	unsigned long			forward_delay;
	unsigned long			bridge_max_age;
	unsigned long			ageing_time;
	unsigned long			bridge_hello_time;
	unsigned long			bridge_forward_delay;

	u8				        group_addr[ETH_ALEN];
	u16				        root_port;

	enum {
		BR_NO_STP, 		    /* no spanning tree */
		BR_KERNEL_STP,		/* old STP in kernel */
		BR_USER_STP,		/* new RSTP in userspace */
	} stp_enabled;

	unsigned char			topology_change;
	unsigned char			topology_change_detected;

	struct timer_list		hello_timer;
	struct timer_list		tcn_timer;
	struct timer_list		topology_change_timer;
	struct timer_list		gc_timer;
	struct kobject			*ifobj;
};

struct net_switch_port
{
    struct net_switch       *swt;
    struct list_head		list;

	/* STP */
	u8				        priority;
	u8				        state;
	u16				        port_no;
	unsigned char			topology_change_ack;
	unsigned char			config_pending;
	port_id				    port_id;
	port_id				    designated_port;
	bridge_id			    designated_root;
	bridge_id			    designated_bridge;
	u32				        path_cost;
	u32				        designated_cost;

	struct timer_list		forward_delay_timer;
	struct timer_list		hold_timer;
	struct timer_list		message_age_timer;
//	struct kobject			kobj;
//	struct rcu_head			rcu;

	unsigned long 			flags;
#define BR_HAIRPIN_MODE		0x00000001
};


#define BPDU_TYPE_CONFIG 0
#define BPDU_TYPE_TCN    0x80

struct stp_config_bpdu
{
	unsigned	topology_change:1;
	unsigned	topology_change_ack:1;
	bridge_id	root;
	int		    root_path_cost;
	bridge_id	bridge_id;
	port_id		port_id;
	int		    message_age;
	int		    max_age;
	int		    hello_time;
	int		    forward_delay;
};

/* called under bridge lock */
static inline int swt_is_designated_port(const struct net_switch_port *p)
{
	return !memcmp(&p->designated_bridge, &p->swt->bridge_id, 8) &&
		(p->designated_port == p->port_id);
}

static inline int swt_is_root_bridge(const struct net_switch *swt)
{
	return !memcmp(&swt->bridge_id, &swt->designated_root, 8);
}

extern void stp_init( void );
extern void stp_stop( void );

/* stp.c */
extern void swt_become_root_bridge(struct net_switch *br);
extern void swt_config_bpdu_generation(struct net_switch *);
extern void swt_configuration_update(struct net_switch *);
extern void swt_port_state_selection(struct net_switch *);
extern void swt_received_config_bpdu(struct net_switch_port *p, struct stp_config_bpdu *bpdu);
extern void swt_received_tcn_bpdu(struct net_switch_port *p);
extern void swt_transmit_config(struct net_switch_port *p);
extern void swt_transmit_tcn(struct net_switch *br);
extern void swt_topology_change_detection(struct net_switch *br);

/* stp_bpdu.c */
extern void swt_send_config_bpdu(struct net_switch_port *, struct stp_config_bpdu *);
extern void swt_send_tcn_bpdu(struct net_switch_port *);

/* br_stp.c */
extern void swt_log_state(const struct net_switch_port *p);
extern struct net_switch_port *swt_get_port(struct net_switch *br,
					   u16 port_no);
extern void swt_init_port(struct net_switch_port *p);
extern void swt_become_designated_port(struct net_switch_port *p);

/* br_stp_if.c */
extern void swt_stp_enable_bridge(struct net_switch *br);
extern void swt_stp_disable_bridge(struct net_switch *br);
extern void swt_stp_set_enabled(struct net_switch *br, unsigned long val);
extern void swt_stp_enable_port(struct net_switch_port *p);
extern void swt_stp_disable_port(struct net_switch_port *p);
extern bool swt_stp_recalculate_bridge_id(struct net_switch *br);
extern void swt_stp_change_bridge_id(struct net_switch *br, const unsigned char *a);
extern void swt_stp_set_bridge_priority(struct net_switch *br,
				       u16 newprio);
extern void swt_stp_set_port_priority(struct net_switch_port *p,
				     u8 newprio);
extern void swt_stp_set_path_cost(struct net_switch_port *p,
				 u32 path_cost);
extern ssize_t swt_show_bridge_id(char *buf, const struct bridge_id *id);
extern rx_handler_result_t swt_handle_frame(struct sk_buff *skb);

/* br_stp_bpdu.c */
struct stp_proto;
extern void swt_stp_rcv(struct sk_buff *skb);

/* br_stp_timer.c */
extern void swt_stp_timer_init(struct net_switch *br);
extern void swt_stp_port_timer_init(struct net_switch_port *p);
extern unsigned long swt_timer_value(const struct timer_list *timer);

// #define swt_is_management_dev(dev) (dev->priv_flags & IFF_SWITCH_MGMT)
static inline struct net_switch_port *swt_port_get_rcu(const struct net_device *dev)
{
	struct net_switch_port *port = rcu_dereference(dev->rx_handler_data);
//	return swt_is_management_dev(dev) ? port : NULL;
    return port;
}

static inline unsigned long swt_round_jiffies( unsigned long j )
{
    int rem;
    unsigned long original = j;

    rem = j % HZ;

    /*
     * If the target jiffie is just after a whole second (which can happen
     * due to delays of the timer irq, long irq off times etc etc) then
     * we should round down to the whole second, not up. Use 1/4th second
     * as cutoff for this rounding as an extreme upper bound for this.
     * But never round down if @force_up is set.
     */
    if (rem < HZ/4) /* round down */
        j = j - rem;
    else /* round up */
        j = j - rem + HZ;

    if (j <= jiffies) /* rounding ate our timeout entirely; */
        return original;

    return j;
}

// UGH
extern struct net_switch *GLOBAL_SWITCH;
#endif
