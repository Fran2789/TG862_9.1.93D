/*
 * Copyright (C) 1999 LSIIT Laboratory.
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
 * This program has been derived from pimd.
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
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


#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#ifdef __linux__
#include <linux/mroute6.h>
#else
#include <netinet6/ip6_mroute.h>
#endif
#include <netinet/icmp6.h>
#include <linux/mroute6.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include "defs.h"
#include "vif.h"
#include "mld6.h"
#include "mld6_proto.h"
#include "mld6v2_proto.h"
#include "debug.h"
#include "inet6.h"
#include "mld6v2.h"
#include "kern.h"
#include "timers.h"
#include "groups.h"


 /* MLDv2 implementation
  *   - MODE_IS_INCLUDE, ALLOW_NEW_SOURCES, BLOCK_OLD_SOURCES,
  *	    (S,G) is handled
  *   - MODE_IS_EXCLUDE, CHANGE_TO_EXCLUDE
  *	    just regarded as MLDv1 join
  *   - CHANGE_TO_INCLUDE:
  *	    regarded as (S,G)
  *
  * If the Multicast Interface is configured to
  *	- any(both): goes to MLDv1-compat mode if MLDv1 is received.
  *	  (default)
  *	- MLDv1 only: ignores MLDv2 messages
  *	- MLDv2 only: ignores MLDv1 messages
  */

/*
 * Forward declarations.
 */


static void accept_multicast_record(int ifindex, mifi_t mifi, struct mld_group_record_hdr *multicast_address_record, struct sockaddr_in6 *src, struct sockaddr_in6 *grp);

/*
 * Send general group membership queries on that interface if I am querier.
 */
void query_groupsV2( struct mvif * v)
{
	int ret;

	if (v->uv_flags != VIFF_DOWNSTREAM )
	{
		IF_DEBUG(DEBUG_MLD_PROTO)
		log_msg(LOG_DEBUG, 0,
				"BUG cannot send  multicast listener general query V2 on : %s, it is not downstream ",
				v->mvif_name);
		return;
	}
	if ((v->uv_flags & VIFF_QUERIER) == 0 || (v->uv_flags & VIFF_NOLISTENER))
	{
		IF_DEBUG(DEBUG_MLD_PROTO)
		log_msg(LOG_DEBUG, 0,
				"could't send a Generic Query due to a lack of querying right");
		return;
	}

	

	ret = send_mld6v2(MLD_LISTENER_QUERY, 0, &v->uv_linklocal,
					  NULL, (struct sockaddr_in6 *) NULL, v->uv_ifindex,
					  MLD6_QUERY_RESPONSE_INTERVAL, 0, TRUE, SFLAGNO,
					  v->uv_mld_robustness, v->uv_mld_query_interval,
					  FALSE);
	if (ret == TRUE)
		v->uv_out_mld_query++;
	else
		log_msg(LOG_ERR, 0,
				"Failed to send  multicast listener general query V2 on : %s ",
				v->mvif_name);
}

/*
 * Send a group-source-specific v2 query.
 * Two specific queries are built and sent:
 *  1) one with S-flag ON with every source having a timer <= LLQI
 *  2) one with S-flag OFF with every source having a timer >LLQI
 * So we call send_mldv2() twice for different set of sources.
 */
void  Send_GSS_QueryV2(struct mvif *v, struct listaddr *g , struct listaddr *s )
{
	int ret;

	if ((v->uv_flags & VIFF_QUERIER) == 0 || (v->uv_flags & VIFF_NOLISTENER))
	{
		log_msg(LOG_DEBUG, 0,
				"don't send a GSS Query due to a lack of querying right");
		return;
	}
	if ( s == NULL || g == NULL)
	{
		log_msg(LOG_ALERT, 0,
				"BUG:Send_GSS_QueryV2 does  have emtpy (G,S) parameters (NULL)");
		return;
	}

	IF_DEBUG(DEBUG_MLD_PROTO)
	log_msg(LOG_DEBUG, 0,
			"sending multicast listener SSM_V2 query for (G,S)= (%s,%s) on : %s", 
			sa6_fmt(&g->mcast_group), 
			sa6_fmt(&s->mcast_group), 
			v->mvif_name);

	ret = send_mld6v2(MLD_LISTENER_QUERY, 0, &v->uv_linklocal,
					  NULL, &g->mcast_group, v->uv_ifindex,
					  MLD6_QUERY_RESPONSE_INTERVAL, 0, TRUE, SFLAGNO,
					  v->uv_mld_robustness, v->uv_mld_query_interval, TRUE);
	if (ret == TRUE)
		v->uv_out_mld_query++;
#if 0  // SFLAG meaningfull for routers, why send twice? 
	ret = send_mld6v2(MLD_LISTENER_QUERY, 0, &v->uv_linklocal,
					  NULL, &g->mcast_group, v->uv_ifindex,
					  MLD6_QUERY_RESPONSE_INTERVAL, 0, TRUE, SFLAGYES,
					  v->uv_mld_robustness, v->uv_mld_query_interval, TRUE); 
#endif
	if (ret == TRUE)
		v->uv_out_mld_query++;

}

/*
 * Send a group-specific v2 query.
 */
void Send_GS_QueryV2 (struct mvif *v, struct listaddr *g)
{

	int sflag = SFLAGNO;
	int ret;

	if (v == NULL || g == NULL)	//Sanity
	{
		log_msg(LOG_ERR, 0, "%s: BUG NULL prameters", __func__);
		return ;
	}
	if ((v->uv_flags & VIFF_QUERIER) == 0 || (v->uv_flags & VIFF_NOLISTENER))
	{
		log_msg(LOG_ALERT, 0,
				"Can't send a GS Query due to a lack of querying right on interface %s", v->mvif_name);
		return;
	}
	IF_DEBUG(DEBUG_MLD_PROTO)
	log_msg(LOG_DEBUG, 0,
			"sending multicast listener V2 query for (G,*)= (%s,) on : %s", sa6_fmt(&g->mcast_group),  v->mvif_name);
	if (g->ma_time > MLD6_LAST_LISTENER_QUERY_TIMER &&
		g->ma_comp_mode == MLDv2)
		sflag = SFLAGYES;

	sflag = SFLAGNO; // TODO no answer
	ret = send_mld6v2(MLD_LISTENER_QUERY, 0, &v->uv_linklocal,
					  NULL, &g->mcast_group, v->uv_ifindex,
					  MLD6_QUERY_RESPONSE_INTERVAL, 0, TRUE, sflag,
					  v->uv_mld_robustness, v->uv_mld_query_interval, FALSE);
	if (ret == TRUE)
		v->uv_out_mld_query++;

}

/*
 * Process an incoming group membership report. Note : this can be possible
 * only if The Router Alert Option have been set and I'm configured as a
 * router (net.inet6.ip6.forwarding=1) because report are sent to the
 * multicast group. processed in QUERIER and Non-QUERIER State
 * actually there is a timer per group/source pair. It's the easiest solution
 * but not really efficient...
 */
void accept_listenerV2_report(  int ifindex, /* network interface message was received from */
								struct sockaddr_in6 *src,
								struct in6_addr *dst,
								char  *report_message,
								int             datalen 
							 )
{

	short  mifi;
	register struct mvif *v;
	struct mld_report_hdr *report;
	struct mld_group_record_hdr *multicast_address_record;
	int             i, records_in_the_report, numsrc, totsrc;
	struct sockaddr_in6 group_sa;
	char *p;
	char buff[IFNAMSIZ+4];

	init_sin6(&group_sa);

	if ((mifi = find_vif_by_ifindex(ifindex)) < 0 || mifi >=numvifs )
	{
		IF_DEBUG(DEBUG_IF)
		log_msg(LOG_WARNING, 0, "%s : ERROR vif index %d is out of range for msg coming from ifindex=%d",
				__func__, mifi, ifindex);
		return;
	}

	v = &mvifs[mifi];

	if ((v->uv_mld_version & MLDv2) == 0)
	{
		IF_DEBUG(DEBUG_IF)
		log_msg(LOG_WARNING, 0,
				"Mif %s configured in MLDv1 received MLDv2 report,ignored",
				v->mvif_name);
		return;
	}

	IF_DEBUG(DEBUG_MLD_PROTO)
	log_msg(LOG_DEBUG, 0,
			"accepting multicast listener V2 report from phys.interface id=%d %s: "
			"src %s,dst %s", 
			ifindex, v->mvif_name,
			sa6_fmt(src), inet6_fmt(dst));

	report = (struct mld_report_hdr *) report_message;
	records_in_the_report = ntohs(report->mld_grpnum);

	v->uv_in_mld_report++;

	/*
	 * loop through each multicast record
	 */

	totsrc = 0;
	for (i = 0; i < records_in_the_report; i++)
	{
		struct mld_group_record_hdr *multicast_address_record0 = (struct mld_group_record_hdr *)(report + 1);
		p = (char *)(multicast_address_record0 + i) - sizeof(struct in6_addr) * i
			+ totsrc * sizeof(struct in6_addr);
		multicast_address_record= (struct mld_group_record_hdr *) p;
		numsrc = ntohs(multicast_address_record->numsrc);
		totsrc += numsrc;

		group_sa.sin6_addr = multicast_address_record->group;
		group_sa.sin6_scope_id = inet6_mvif2scopeid(&group_sa, v);

		if (IN6_IS_ADDR_MC_LINKLOCAL(&group_sa.sin6_addr))
		{
			/* too noisy */
			IF_DEBUG(DEBUG_PKT)
			log_msg(LOG_DEBUG, 0,
					"accept_listenerV2_report: group(%s) has the "
					"link-local scope, discarding",
					sa6_fmt(&group_sa));
			continue;
		}

		if (IN6_IS_ADDR_MC_SITELOCAL(&group_sa.sin6_addr))
		{
			/* too noisy */
			IF_DEBUG(DEBUG_PKT)
			log_msg(LOG_DEBUG, 0,
					"accept_listenerV2_report: group(%s) has the "
					"site-local scope, discarding\n",
					sa6_fmt(&group_sa));
			continue;
		}
		accept_multicast_record( ifindex, mifi, multicast_address_record, src, &group_sa);
	}
}
/*
* accept_multicast_record()       
*                          -parses MLDv2 multicast record :
*                           for every part of multipart record processes ASM or SSM message content
*
*
*/

static void accept_multicast_record(int ifindex, mifi_t mifi, struct mld_group_record_hdr *multicast_address_record,
									struct sockaddr_in6 *multicast_subscriber_address, 
									struct sockaddr_in6 *required_multicast_group)
{
	struct mvif *v = &mvifs[mifi];
	int numsrc = ntohs(multicast_address_record->numsrc);

	int j;
	struct sockaddr_in6 source_sa;
	struct listaddr *s = NULL;
	struct listaddr *g = NULL;

	init_sin6(&source_sa);

	/* sanity check */
	if ( mifi == upStreamVif || (v->uv_flags & VIFF_UPSTREAM ) )
	{
		log_msg(LOG_WARNING, 0,
				"Sanity failed :Got listener report on non-listener phys. interface #=%d, mvif=%d",  ifindex, mifi );
		return;
	}

	if (required_multicast_group == NULL) // Sanity Check
	{
		log_msg(LOG_ERR, 0,
				"BUG Sanity failed :Got listener report with empty required_multicast_group"  );
		return;
	}
	/* just locate group */
	g = find_multicast_group(v, required_multicast_group); // returns g -: pointer to group record if any


	switch (multicast_address_record->record_type)
	{
	case CHANGE_TO_INCLUDE_MODE:  // filter_mode changes to include, RFC  ref TO_IN(X), wheere X - source to include, i.e to join
		{
			IF_DEBUG(DEBUG_MLD_PROTO)
			log_msg(LOG_DEBUG, 0, "MLDV2 CHANGE_TO_INCLUDE_MODE  processing (G,S)=%s,*",
					sa6_fmt(required_multicast_group));
			if (numsrc == 0)
			{
				// MLDv2 in ASM mode  eq. GROUP or LEAVE of  MLDv1

				if (g == NULL)
				{

					return;
				}
				IF_DEBUG(DEBUG_MLD_PROTO)
				log_msg(LOG_DEBUG, 0, "ASM of MLDV2 CHANGE_TO_INCLUDE_MODE=GROUP_LEAVE "
						"for (G)=%s on interface %s",
						sa6_fmt(required_multicast_group),
						v->mvif_name);

				if (g->ma_filter_mode ==  MODE_IS_INCLUDE)
					break;
				if (g->ma_filter_mode ==  MODE_IS_EXCLUDE)
				{
					IF_DEBUG(DEBUG_MLD_PROTO)
					log_msg(LOG_DEBUG, 0, " filter_mode MODE_IS_EXCLUDE ASM of MLDV2 CHANGE_TO_INCLUDE_MODE=GROUP_LEAVE " // XXX LEV debug
							"for (G)=%s on interface %s",
							sa6_fmt(required_multicast_group),
							v->mvif_name);
					// When working in Any Source Multicast, CHANGE_TO_INCLUDE_MODE is sent to leave a group
					//g->ma_CheckingListenerState = TRUE; // Guard repetetive reports
					g->ma_filter_mode = MODE_IS_INCLUDE; // Guard repetetive reports
					recv_listener_done(ifindex,mifi,multicast_subscriber_address,required_multicast_group);
					break;
				}

				g->ma_filter_mode = MODE_IS_INCLUDE; // Guard repetetive reports
				break;

			} else	// numsrc  > 0
			{
				
				IF_DEBUG(DEBUG_MLD_PROTO)
				log_msg(LOG_DEBUG, 0, "MLDV2 CHANGE_TO_INCLUDE_MODE  processing N=%d source of  (G)=%s",
						numsrc,
						sa6_fmt(required_multicast_group));
				set_sources_mode(mifi, g, numsrc, &multicast_address_record->src, CHANGE_TO_INCLUDE_MODE);
			}

			break;

		}
	case MODE_IS_INCLUDE:
		{
		        if (numsrc  == 0 )
			{
				IF_DEBUG(DEBUG_MLD_PROTO)
				log_msg(LOG_DEBUG, 0, "MLDV2 state report  MODE_IS_INCLUDE  ASM mode - no sources");
				if ( g == NULL )
			        {
				IF_DEBUG(DEBUG_MLD_MEMBER)
				log_msg(LOG_DEBUG, 0,
						"The group (G)=%s does not exist , trying to add it", sa6_fmt(required_multicast_group));
				}
				g=recv_listener_report(ifindex, mifi, multicast_subscriber_address, required_multicast_group, MLDv2);
			
			      
				g->ma_filter_mode = MODE_IS_INCLUDE; // TODO RFC 3810 if no sources, created in EXCLUDE         
				

			}
			
			else	// (numsrc  > 0 )
			{
				IF_DEBUG(DEBUG_MLD_PROTO)
				log_msg(LOG_DEBUG, 0, "MLDV2 MODE_IS_INCLUDE:  processing N=%d source of  (G)=%s",
						numsrc,
						sa6_fmt(required_multicast_group));
				if ( g == NULL )
			       {
				   IF_DEBUG(DEBUG_MLD_MEMBER)
				   log_msg(LOG_DEBUG, 0,
						"The group (G)=%s does not exist , trying to add it", sa6_fmt(required_multicast_group));
				   g = make_new_group( mifi , required_multicast_group , MLDv2); //TODO check multicast_subscriber_address ? sender or  SSM
				        
				    //mld_merge_with_upstream(mifi,  required_multicast_group,  MLDv2 |MCAST_SSM ,NULL ,MODE_IS_INCLUDE); // try to merge  with upstream interface database in SSM mode

			        }
				set_sources_mode(mifi, g, numsrc, &multicast_address_record->src, MODE_IS_INCLUDE);
			}
			break;
		}
	case ALLOW_NEW_SOURCES:	 // RFC 3810 7.4.1.  Reception of Current State Records
		{
			IF_DEBUG(DEBUG_MLD_PROTO)
			log_msg(LOG_DEBUG, 0, "MLDV2 ALLOW_NEW_SOURCES  processing N=%d source of  (G)=%s",
					numsrc,
					sa6_fmt(required_multicast_group));
			if (numsrc  == 0 )
			{
				log_msg(LOG_ALERT, 0, "BUG ALLOW_NEW_SOURCES: mode - no sources");
				recv_listener_report(ifindex, mifi, multicast_subscriber_address, required_multicast_group, MLDv2);
				break;
			} 
			if ( g == NULL )
			{
				IF_DEBUG(DEBUG_MLD_MEMBER)
				log_msg(LOG_DEBUG, 0,
						"The group (G)=%s does not exist , trying to add it", sa6_fmt(required_multicast_group));
				g = make_new_group( mifi , required_multicast_group , MLDv2); //TODO check multicast_subscriber_address ? sender or  SSM
				g->ma_filter_mode = MODE_IS_INCLUDE; // TODO RFC 3810 if no sources, created in EXCLUDE         
				//mld_merge_with_upstream(mifi,  required_multicast_group,  MLDv2 |MCAST_SSM ,NULL ,MODE_IS_INCLUDE); // try to merge  with upstream interface database in SSM mode

			}
			
			set_sources_mode(mifi, g, numsrc, &multicast_address_record->src, MODE_IS_INCLUDE);
			

			break;
		}
	case BLOCK_OLD_SOURCES:
		{
			IF_DEBUG(DEBUG_MLD_PROTO)
			log_msg(LOG_DEBUG, 0,"MLDV2 BLOCK_OLD_SOURCES processing (G,S)=%s,*", sa6_fmt(required_multicast_group));
			if (g == NULL)
			{
				log_msg(LOG_WARNING, 0,
						"Group %s does not exist, ignoring the  BLOCK_OLD_SOURCES report", sa6_fmt(required_multicast_group));
				return;
			}
			if (g->ma_comp_mode == MLDv1)
			{
				log_msg(LOG_DEBUG, 0, "ignores BLOCK msg in MLDv1-compat-mode");
				return;
			}
			/*
			 * Unlike RFC2710 section 4 p.7 (Routers in Non-Querier state
			 * MUST ignore Done messages), MLDv2 non-querier should
			 * accept BLOCK_OLD_SOURCES message to support fast-leave
			 * (although it's not explcitly mentioned).
			 */
			for (j = 0; j < numsrc; j++)
			{
				/*
				 * Look for the multicast_subscriber_address/group
				 * in our multicast_subscriber_address/group list; in order to set up a short-timeout
				 * group/source specific query.
				 */

				source_sa.sin6_addr = multicast_address_record->src[j];
				source_sa.sin6_scope_id = inet6_mvif2scopeid(&source_sa, v);

				s = find_requested_mcast_transmitter(v, required_multicast_group, g, &source_sa);
				if (s == NULL)
				{
					log_msg(LOG_WARNING, 0,
							"Cannot accept BLOCK_OLD_SOURCE record"
							"for non-existent source (G,S)=(%s,%s)", sa6_fmt(required_multicast_group), sa6_fmt( &source_sa));
					continue;
				}
				/*
				 * the source exist , so according to the spec, we will always
				 * send a source specific query here : A*B is true here
				 */

				/* scheduling MLD6_ROBUSTNESS_VAR specific queries to send */
				/* => send a m-a-s	*/
				/* start rxmt timer */
				if (v->uv_fastleave)
				{
					ExpireSourceRtrmtTimer ((void *) &s->rxmt_timer_callback);
				} else
				{
				        if ( s->ma_CheckingListenerState == TRUE ) // guard repetetive reports
                                              break;
					s->ma_llqc=MLD6_ROBUSTNESS_VARIABLE -1; // arm retransmit counter, decreases every timer expiratn
					start_rxmt_timer(v, s, v->uv_mld_llqi);	 //deletes source on llqc=0
					s->ma_CheckingListenerState = TRUE;
					Send_GSS_QueryV2 (v, g, s);
				}
			}
			break;

		}
	case MODE_IS_EXCLUDE:
		{
			/* 
			 * Periodically Confirms filter_mode state
			 */ 
			if (numsrc  == 0 )
			{
				struct listaddr * new_group;
				IF_DEBUG(DEBUG_MLD_PROTO)
				log_msg(LOG_DEBUG, 0, "MLDV2 filter state  MODE_IS_EXCLUDE processing (G,S)=%s,*", sa6_fmt(required_multicast_group));

				/* just regard as ASM (G,*) but not shift to mldv1-compat-mode */
				new_group =recv_listener_report(ifindex, mifi, multicast_subscriber_address, required_multicast_group, MLDv2);
				new_group->ma_filter_mode=MODE_IS_EXCLUDE;
				break;
			} else //  (numsrc  > 0 )
			{
				set_sources_mode(mifi, g, numsrc, &multicast_address_record->src, MODE_IS_EXCLUDE);
				break;
			}
			break;
			// TODO if numsrc >0 -> need to update sorces list  RFC 3810 says it can contain source addresses

		}
	case CHANGE_TO_EXCLUDE_MODE:  // filter state change
		{
			struct listaddr * new_group;
			/*
			 * RFC3810 8.3.2 says "MLDv2 BLOCK messages are ignored, as are
			 * source-lists in TO_EX() messages".
			 * But mldproxy figures out if source state changes as result of this operatin,
			 * and applies iptables to block transmission
			 *  e.
			 */
			IF_DEBUG(DEBUG_MLD_PROTO)
			log_msg(LOG_DEBUG, 0, "MLDV2 filter state change CHANGE_TO_EXCLUDE_MODE processing (G,S)=%s,*", sa6_fmt(required_multicast_group));
			if (g && g->ma_comp_mode & MLDv1)
			{
				log_msg(LOG_WARNING, 0,
						"ignores TO_EX source list in MLDv1-compat-mode");
			}
			if (numsrc  == 0 )
			{
				/* just regard as ASM (G, *) but not shift to mldv1-compat-mode */

				new_group=recv_listener_report(ifindex, mifi, multicast_subscriber_address, required_multicast_group, MLDv2);

				if (new_group == NULL)
				{
					log_msg(LOG_ALERT, 0,
							"Fail to create group G=(%s) in CHANGE_TO_EXCLUDE_MODE, numsrc=0", sa6_fmt(required_multicast_group));
                                        return;
				}
				new_group->ma_filter_mode=MODE_IS_EXCLUDE;
				break;
			}
			if (numsrc  > 0 )
			{
				if (g == NULL)
				{
                                   IF_DEBUG(DEBUG_MLD_PROTO)				
				   log_msg(LOG_NOTICE, 0,
							"CHANGE_TO_EXCLUDE_MODE received for invalid/empty G=%s, numsrc=%d ", sa6_fmt(required_multicast_group), numsrc);
				   break;
				}
				set_sources_mode(mifi, g, numsrc, &multicast_address_record->src, CHANGE_TO_EXCLUDE_MODE);
				break;
			}
			break;
		}
	default:
	        IF_DEBUG(DEBUG_PKT)
		log_msg(LOG_NOTICE, 0,
				"wrong multicast report type : %d", multicast_address_record->record_type);
		break;
	}
}

