/*  Copyright 2014, ARRIS Enterprises, Inc., All rights reserved                                  */

// This file is intended to hold Arris-created symbols/variables in the kernel that our kernel modules
// need access to.  This way the symbols are in the kernel and aren't declared in a kernel module
// creating unnecessary dependency chains.

#include <linux/kernel.h>
#include <linux/module.h>
#include <arris/arris_mod_api.h>
#include <linux/skbuff.h>
#include <br_private.h>

ARRISMOD_DEFINE( int, arris_loop_detected_hook, unsigned char * );

ARRISMOD_DEFINE( int, arris_skb_rx_filter_hook, struct sk_buff* );

ARRISMOD_DEFINE( int, arris_bridge_forward_filter_hook, const struct sk_buff*, const struct net_bridge_port * );
ARRISMOD_DEFINE( void, arris_bridge_free_dev_hook, struct net_device * );
