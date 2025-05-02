/*
 * Copyright (C) 1998 WIDE Project.
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


#include <time.h>
//#include <linux/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <error.h>
//#include <sys/time.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <sys/timerfd.h>

#include <sys/epoll.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#ifdef __linux__
#include <linux/mroute6.h>
#else
#include <netinet6/ip6_mroute.h>
#endif
#include <netinet/icmp6.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "timers.h"
#include "mld6.h"
#include "vif.h"
#include "mld6_proto.h"
#include "mld6v2.h"
#include "mld6v2_proto.h"
#include "debug.h"
#include "inet6.h"
#include "kern.h"
#include "groups.h"


#include "timers.h"
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC             1  //usr/include/bits/time.h
#define CLOCK_REALTIME              0
#endif

/*
 * Forward declarations.
 */


/*
 * Send group membership queries on that interface if I am querier.
 */
void
query_groups(struct mvif *v)
{
	{
		int ret;

		if (v->uv_startupQueryCount)
		{	
		   start_genQuery_timer ( v, MLD6_STARTUP_QUERY_INTERVAL  ); /* start Qeneric quries on interface query timer */
		   
		   v->uv_startupQueryCount--;
		}
		else
		{
		    start_genQuery_timer ( v, MLD6_QUERY_INTERVAL  ); /* start Qeneric quries on interface query timer */
		}   
		    
		ret = send_mld6(MLD_LISTENER_QUERY, 0,
			&v->uv_linklocal, NULL,
			(struct in6_addr *)&in6addr_any, v->uv_ifindex,
			MLD6_QUERY_RESPONSE_INTERVAL, 0, 1);
		if (ret == TRUE)
			v->uv_out_mld_query++;
	}
}


/*
 * Process an incoming MLDv1 group membership report.
 */
void accept_listener_report( short ifindex,
	struct sockaddr_in6 *src,
	struct in6_addr *dst,struct in6_addr  *group)
{
	short mifi;
	struct mvif *v = NULL;
	struct sockaddr_in6 group_sa;
        log_msg(LOG_DEBUG,0,  // TODO Lev 
			    "accept_listener_report: group(%s) \n"
			    , inet6_fmt(group));
	
	if (IN6_IS_ADDR_MC_LINKLOCAL(group)) {
		IF_DEBUG(DEBUG_MLD)
		      //log_msg(LOG_DEBUG,0,
	               log_msg(LOG_DEBUG,0,  // TODO Lev 
			    "accept_listener_report: group(%s) has the "
			    "link-local scope. discard", inet6_fmt(group));
		return;
	}
	
        // just find mifi for physical interface =ifindex
	if ( (mifi = find_vif_by_ifindex(ifindex)) <0 ||
               (mifi >= MAXMVIFS )) /* ARRIS MOD for index check */
               {
		IF_DEBUG(DEBUG_MLD)
			log_msg(LOG_INFO, 0,
			    "accept_listener_report: Message coming from interface %d, dcan't find a vif",
                            ifindex);
		return;
	}

	v = &mvifs[mifi];
	init_sin6(&group_sa);
	group_sa.sin6_addr = *group;
	group_sa.sin6_scope_id = inet6_mvif2scopeid(&group_sa, v);

	IF_DEBUG(DEBUG_MLD)
	          log_msg(LOG_DEBUG,0,
		    "accepting multicast listener report: "
		    "src %s,dst %s, grp %s\n",
		    sa6_fmt(src),inet6_fmt(dst), inet6_fmt(group));

	v->uv_in_mld_report++;
	recv_listener_report(ifindex, mifi, src, &group_sa, MLDv1);
	
}
/*
 * Process Multicast Group membership report and update LAN membership database group list
 * ifundex - input LAN interface 
 * src     - sender of report 
 * mcast - Multicast Group
 */
/* shared with MLDv1-compat mode in mld6v2_proto.c */
struct listaddr * recv_listener_report(short ifindex , mifi_t mifi, struct sockaddr_in6 *  src, struct sockaddr_in6 *mcast, u_int8_t mld_version)
{
	struct mvif *v = &mvifs[mifi];
	register struct listaddr *g;
	
	/*
	 * Look for the group in our group list; if found,
	 *  1) if necessary, shift to MLDv1-compat-mode
	 *  2) just reset MLD-related timers (nothing special is necessary
	 *     regarding compat-mode, since an MLDv2 TO_EX{NULL} message
	 *     is also handled in here in the same manner as MLDv1 report).
	 */
	g = find_multicast_group(v, mcast);
	
	if (g != NULL) 
	{
	      IF_DEBUG(DEBUG_MLD_PROTO)
		    log_msg(LOG_DEBUG, 0, " Report for exiting group G=%s on %s",sa6_fmt(mcast) , v->mvif_name);
		/* the group  FOUND */
		/* stop retransmit  timers */
		stop_rxmt_timer(v, g);
		
		start_report_timer( v, g, MLD6_LISTENER_INTERVAL );
	        
		// Prepare next retransmit when (if) leave|done message will be heard
		g->ma_llqc =(v->uv_fastleave ) ? 0 :  v->uv_mld_llqc;  // Restore retry count of Group Specific queries
		if (v->uv_mld_version == MLDv2 && mld_version == MLDv1 )
		{
		    // Switching mode from V2 to V1  RFC3810 8.3.2.  In the Presence of MLDv1 Multicast Address Listeners
		     IF_DEBUG(DEBUG_MLD_PROTO)
			  log_msg(LOG_DEBUG, 0, "Switching mode from V2 to V1 for group G=%s on %s",sa6_fmt(mcast) , v->mvif_name);
    start_back_to_mldv2_timer(v, g, MLD6_OLDER_VERSION_HOST_PRESENT);
    stop_all_sources_timers(v,g);
		}
		return g;
	}
        
	/* MAKE NEW GROUP , and add it to the list and update kernel cache. */
	
	
	if ( (g = make_new_group(  mifi, mcast, mld_version)) == NULL) 
	{
	        log_msg(LOG_ERR, 0, "make_new_group G=%s on interface %s failed", sa6_fmt(mcast) ,v->mvif_name );
		return;
	}
	/* Set LAN group mcast mode */
	g->ma_comp_mode |= MCAST_ASM ;    /* set Any source multicast mode */
	g->ma_comp_mode |= mld_version ;  /* set version if group is in mixed mode  */
	// Syncronize with WAN/upstream database
	mld_merge_with_upstream ( mifi , mcast, mld_version, /* pass mcast to upstream database as a search parameter */
                                 NULL, /* no SSM */
                                 0);   /* no filter_mode for SSM */

	return g;
}



void accept_listener_done(
        short ifindex,
	struct sockaddr_in6 *src,
	struct in6_addr *dst, struct in6_addr* multicast_address)
{
	mifi_t mifi;
	struct mvif *v = NULL;
	struct sockaddr_in6 multicast_socket;
        if (ifindex == upstream_idx )
	    return;
	/* Don't create routing entries for the LAN scoped addresses */
	/* sanity? */
	if (IN6_IS_ADDR_MC_NODELOCAL(multicast_address)) {
		IF_DEBUG(DEBUG_MLD)
			log_msg(LOG_DEBUG,0,
			    "accept_listener_done: address multicast node "
			    " local(%s), ignore it...", inet6_fmt(multicast_address));
		return;
	}

	if (IN6_IS_ADDR_MC_LINKLOCAL(multicast_address)) {
		IF_DEBUG(DEBUG_MLD)
			log_msg(LOG_DEBUG,0,
			    "accept_listener_done: address multicast "
			    "link local(%s), ignore it ...", inet6_fmt(multicast_address));
		return;
	}

	mifi = find_vif_by_ifindex(ifindex);
	if (mifi  < 0 || mifi >= numvifs)
        {
		log_msg(LOG_ALERT, 0,
			    "accept_listener_done: can't find a mif");
		return;
	}

	v = &mvifs[mifi];
	if ( v->uv_flags & VIFF_QUERIER == 0 ) // Sanity
	{
	  // RFC 2710 - done significant only on querier interfaces 
	  IF_DEBUG(DEBUG_MLD)
			log_msg(LOG_INFO, 0, "Got a Done on non querier interface %s", v->mvif_name );
	  return;
	}
	if ( v->uv_groups == NULL ) // Sanity
	{
	  // RFC 2710 - done significant only when inerface is in listening state
	  // if it have registeres groups - it is listening  
	  IF_DEBUG(DEBUG_MLD)
			log_msg(LOG_INFO, 0, "Gat a Done on non listening interface %s", v->mvif_name );
	  return;
	}
	init_sin6(&multicast_socket);
	multicast_socket.sin6_addr = *multicast_address;
	multicast_socket.sin6_scope_id = inet6_mvif2scopeid(&multicast_socket, v);

	/*
	 * MLD done does not affect mld-compatibility;
	 * draft-vida-mld-v2-05.txt section 7.3.2 says:
	 *  The Multicast Address Compatibility Mode variable is based
	 *  on whether an older version report was heard in the last
	 *  Older Version Host Present Timeout seconds.
	 */
	

	IF_DEBUG(DEBUG_MLD)
		log_msg(LOG_INFO, 0,
		    "accepting listener done message: src %s, dst %s, grp %s\n",
		    sa6_fmt(src), inet6_fmt(dst), inet6_fmt(multicast_address));
	v->uv_in_mld_done++;
        
	recv_listener_done(ifindex, mifi, src, &multicast_socket);
	
}


/*
* recv_listener_done() - process group LEAVE message
*1  - find group on LAN interface
*2  -send a group specific query to ensure no more subscribers exist 
*
* shared with MLDv1-compat mode in mld6v2_proto.c
*/
void
recv_listener_done(
	short ifindex,
	mifi_t mifi,
	struct sockaddr_in6 *src,struct sockaddr_in6  *group_multicast_socket )
{
	struct mvif *v = &mvifs[mifi];
	register struct listaddr *g;
	int ret = FALSE;


	/*
	 * Look for the group in our group list in order to set up a
	 * short-timeout query.
	 */
	 g = find_multicast_group(v,group_multicast_socket);

		if ( g == NULL) {
		    log_msg(LOG_ERR, 0, "[accept_done_message] for G=%s not exist on interface %s\n",
			       sa6_fmt(group_multicast_socket), v->mvif_name);;
                               return;
		}
		IF_DEBUG(DEBUG_MLD)
			log_msg(LOG_DEBUG,0, "[accept_done_message] for G=%s\n",
			       sa6_fmt( &g->mcast_group) );
			       
		if ( v->uv_fastleave)
		{
		    IF_DEBUG(DEBUG_MLD_PROTO)
			log_msg(LOG_DEBUG,0, 
			     "Received  done_message for G=%s on FastLeave track \n",
			    sa6_fmt( &g->mcast_group) );

		    delete_group(mifi ,g); // Deletes on LAN, and if required on Upstream WAN too
		    return;
		}
		
		/* still waiting for a reply to a query, ignore the done */
		if  (  g->ma_CheckingListenerState == TRUE  ) 
        {
		        IF_DEBUG(DEBUG_MLD)
			          log_msg(LOG_DEBUG,0, "Ignoring repeated done_message for G=%s\n",
			                               sa6_fmt(&(g->mcast_group)));
			     return;
		}
        g->ma_llqc=MLD6_LAST_LISTENER_QUERY_COUNT;
		
		sendGroupSpecificMemberQuery( v , &g->mcast_group);
		v->uv_out_mld_query++;
		
        start_report_timer (v, g,  MLD6_LAST_LISTENER_QUERY_TIMER);               // Lower report timer

	    start_rxmt_timer ( v, g, v->uv_mld_llqi );
			          
}


	
/*
 * Send a group-specific query.  This function shouldn't be called when
 * the interface is configured with MLDv2, to prevent MLDv2 hosts from
 * shifting to MLDv1-compatible mode unnecessarily.
 * (now it's called only from SetQueryTimer() when the interface is
 *  configured in MLDv1, so the above condition is satisfied)
 */
void sendGroupSpecificMemberQuery(struct  mvif * v, struct sockaddr_in6* mcast_group)
{
	int8_t ret=0;
	struct listaddr *group ;

  /*
   * Look for the group in the interface's  group list; if found,
   *  .
   */

 group = find_multicast_group (v, mcast_group);
 if (!group) {
     return;
 }
	/*
		 * if an interface is configure in MLDv2, query is done
		 * by MLDv2, regardless of compat-mode.
		 * (draft-vida-mld-v2-05.txt section 7.3.2 page 39)
		 *
		 * if an interface is configured only with MLDv1, query
		 * is done by MLDv1.
		 */
	#ifdef HAVE_MLDV2
        // TODO check group v2 mode instead
	if (group->ma_comp_mode & MLDv2 ) {
			IF_DEBUG(DEBUG_MLD_PROTO)
			    log_msg(LOG_DEBUG, 0,
			  " sendGroupSpecificMemberQuery() group specific for G=%s in V2 on : %s ", sa6_fmt(mcast_group),  v->mvif_name);
                        // TODO Send_GS_QueryV2(v,g)
			ret = send_mld6v2(MLD_LISTENER_QUERY, 0,
					  &v->uv_linklocal, NULL,
					  mcast_group, v->uv_ifindex,
					  MLD6_QUERY_RESPONSE_INTERVAL, 0, TRUE,
					  SFLAGNO, v->uv_mld_robustness,
					  v->uv_mld_query_interval, FALSE);
	}
	else
	#endif
	    if (group->ma_comp_mode & MLDv1)
	    {
	       IF_DEBUG(DEBUG_MLD_PROTO)
			    log_msg(LOG_DEBUG, 0,
			  " sendGroupSpecificMemberQuery() group specific for G=%s in V1 on : %s ", sa6_fmt(mcast_group),  v->mvif_name);
			 
		ret = send_mld6(MLD_LISTENER_QUERY, 0,
				&v->uv_linklocal, NULL,
				&mcast_group->sin6_addr, v->uv_ifindex,
				 MLD6_QUERY_RESPONSE_INTERVAL, 0, 1);
	  }
	
	if (ret == TRUE)
		v->uv_out_mld_query++;
	
	
}


