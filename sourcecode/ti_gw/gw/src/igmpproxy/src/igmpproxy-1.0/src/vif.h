/*
 * Copyright (c) 1998-2001
 * The University of Southern California/Information Sciences Institute.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 */

/*
 * This program has been derived from pim6sd.        
 * The pim6sd program is covered by the license in the accompanying file
 * named "LICENSE.pim6sd".
 *

 * The changed portions of the program are covered by the following license 
 * in the  accompanying file named "LICENSE.mldproxy"
*/

#ifndef VIF_H
#define VIF_H

#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <net/if.h>

#include <netinet/in.h>


#include <linux/mroute.h>
#include <linux/mroute6.h>
typedef vifi_t mifi_t;
#include "defs.h"
#define NO_VIF            	((mifi_t)-1)	/* An invalid vif index */

#define VIFF_DOWN               0x0000100
#define VIFF_DISABLED           0x0000200
#define VIFF_QUERIER            0x0000400
#define VIFF_REXMIT_PRUNES      0x0004000
#define VIFF_STATIC             0x0008000
#define VIFF_NONBRS             0x0080000
#define VIFF_PIM_NBR            0x0200000
#define VIFF_POINT_TO_POINT     0x0400000
#define VIFF_NOLISTENER         0x0800000	/* no CPE listener on the link   */
#define VIFF_ENABLED            0x1000000

#define VIFF_UPSTREAM       VIFF_NOLISTENER
#define VIFF_DOWNSTREAM     VIFF_QUERIER

/* ---------------------------- Globals  Section  Start ----------------------------------------*/
extern int16_t upStreamVif;
extern int16_t upstream_idx;
extern u_int16_t numvifs;

/* ---------------------------- Globals  Section End ----------------------------------------*/

typedef struct
{
  cfunc_t callback;
  mifi_t mifi;
  struct listaddr *g;
  uint32_t mcast_group;
  struct listaddr *source;
  struct mvif *v;
  int q_time;
} timer_cbk_t;


typedef struct
{
  cfunc_t callback;
  mifi_t mifi;
  struct mvif *v;
  int q_time;
} mvif_timer_cbk_t;

/* 
 *     Multicast address  ---------------------------
 *            
*/
struct listaddr
{
  struct listaddr *ma_next;	/* link to next liist element */
  struct listaddr *sources;	/* list of sources of this group */
  uint32_t mcast_group;	        /* multicast groupc IP address */
  uint32_t transmitter;	        /* IP address of sender of the multicast */

  /* 
   *    Multicast protocol Parameters  mcast_proto.h  ---------------------------
   *
   *            
   */
  u_int8_t ma_robustness;	/* robustness */
  u_int8_t ma_llqi;		/* Last Listener Query Interval, default 1 sec */
  int8_t   ma_llqc;		/* Last Listener Query Count, set it uv_mld_llq  to  before query will be snt, decr by 1 at timer expire until 0 if mld_report will be heard */
  u_int8_t ma_filter_mode;	/* filter mode for mldv2/igmpv3 */
  u_int8_t ma_comp_mode;	/* compatibility mode igmpv1/2/3 */
  u_int8_t ma_CheckingListenerState;	/* TRUE I'm in checking listener state */
  /* 
   *   End of Multicast protocol Parameters for group, see also mvfis struct below  ---------------------------
   *
   *            
   */
  
  if_set ma_downstream_ifset;	/* populated only for upstream element of array */
  struct epoll_event group_report_timer_event;
  timer_cbk_t report_timer_callback;
  int group_report_timer;	/* FD -timer for group membership  this is timer reverenced by rfc 2710 */
  struct epoll_event group_rxmt_timer_event;
  timer_cbk_t rxmt_timer_callback;
  int group_rxmt_timer;		/* FD - timer for  retransmit group specific query - as responce for leave msg this is timer reverenced by rfc 2710 as rxmt timer */

  /* timer to return from mldv1-compatibility mode */
  struct epoll_event filterMode_timer_event;
  timer_cbk_t filterMode_timer_callback;
  int mldv2_filterMode_timer;

  /* Older  Version -IGMPv2 Timeout   - per MA address */
  struct epoll_event group_back2Igmpv3_time_event;
  timer_cbk_t group_back2Igmpv3_timer_callback;
  int group_back2Igmpv3_timer;
  /* Older  Version IGMPv1  Timeout   - per MA address */
  struct epoll_event group_back2Igmpv2_time_event;
  timer_cbk_t group_back2Igmpv2_timer_callback;
  int group_back2Igmpv2_timer;
  
  time_t report_time;		/* time of the listener report */
  time_t ma_time;		/* time of the last timer */
  time_t ma_ctime;		/* entry creation time */

  /*
   *  The control of these timers is managed by routines the correspondent  
   *  timers/callback routines , except
   *  Generic Query transmission which is interface specific
   */
};

/*
 * Model of Multicast Virtual Interface  
 *
 * A "virtual interface structure" is a model of a physical, multicast-capable interface
 * (called a "phyint"), 
 * Linux kernel also has acting mcast vifs in /proc/net/ip6_mcast_mif
 * 
 */
 #ifndef IFNAMSIZ
 #define IFNAMSIZ 16
 #endif

struct mvif
{
  struct listaddr *uv_groups;	/* list of MA groups   */
  char mvif_name[IFNAMSIZ];	/* interface name */
  struct in_addr InAdr;	/* link-local address of this vif */
  
  u_int16_t uv_ifindex;		/*  if_nametoindex -  index of the real physical interface */
  u_int16_t uv_mifi;		/*  index of the vitual interface */
  u_int32_t uv_flags;		/* VIFF_DOWNSTREAM/VIFF_UPSTREAM, VIFF_QUERIER */

  /* 
   *   CONFIGURATION PARAMETER START  ---------------------------
   *
   *             MLD protocol Parameters 
   */
  u_int8_t  uv_fastleave;	/* 1 means fastleave, 0 - send llqc quieris */
  u_int8_t  uv_startupQueryCount;	/* Startup Query Count */
  u_int8_t  uv_mld_version;	/* mld version of this mif */
  u_int8_t  uv_mld_robustness;	/* robustness variable of this vif (mld6 protocol) */
  u_int8_t  uv_older_host_present;	/* older IGMP protocol host present in network */
  u_int16_t uv_startupQueryInterval;
  u_int16_t uv_mld_query_interval;	/* query interval of this vif (mld6 protocol) */
  u_int16_t uv_mld_query_rsp_interval;	/* query response interval of this vif (mld6 protocol) */
  u_int16_t uv_mld_llqi;	/* last listener query interval */
  u_int8_t  uv_mld_llqc;		/* last listener query count */

  /* 
   *   CONFIGURATION PARAMETER END  ---------------------------
   */

  /* Statistics  : */
  /*incoming MLD packets on this interface */
  uint32_t uv_in_mld_query;
  uint32_t uv_in_mld_report;
  uint32_t uv_in_mld_done;

  /* outgoing MLD packets on this interface */
  uint32_t uv_out_mld_query;
  uint32_t uv_out_mld_report;
  uint32_t uv_out_mld_done;

  /* statistics about the forwarding cache in kernel */
  uint32_t uv_cache_miss;
  uint32_t uv_cache_notcreated;

#define mvif_timer gen_query_timer
  struct epoll_event uv_genQuery_timer_event;
  mvif_timer_cbk_t uv_genQuery_timer_callback;
  int uv_genQuery_timer;	/* timer for MLD protocol Generic Queries  */
  time_t uv_q_time;		/* time of the last timer event */
};

/* ARRIS MOD START */
/* For TS8.0 we have 8 subnet interfaces, plus one upstream interface, the total number is 9 */
#define MAXMVIFS 9		/* Size of Multicast Virtual Interfaces Array */
/* ARRIS MOD END */
#define NOVIF MAXMIFS+1
extern struct mvif mvifs[MAXMVIFS];
extern int   config_vif_from_kernel (struct mvif *v);
extern void start_all_vifs (void);
extern int   activate_back2Igmpv3_timer (mifi_t mifi);
extern void  create_mcastVifs __P ((void));
extern void  stop_all_vifs __P ((void));
extern mifi_t find_vif_by_ifindex (uint8_t ifindex_of_real_nic);


#endif /*  */
