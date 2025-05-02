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
#include "igmpproxy.h"
#include "mcast_proto.h"
//#include "igmpv2_proto.h"
#include "igmpv3_proto.h"
#include "debug.h"
#include "inet6.h"

#include "kern.h"
#include "timers.h"
#include "groups.h"
#include <linux/igmp.h>


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


static void accept_multicast_record(int ifindex, mifi_t mifi,  struct igmpv3_grec *multicast_address_record, IP_ADDRESS src, IP_ADDRESS grp);



#define SWITCHV1V2(comp_mode) \
log_msg(LOG_DEBUG, 0, "Switching request  to IGMPv1/v2 -compat-mode");\
            if ( (comp_mode) == IGMPv2 ) \
            {\
            numsrc=0; \
                switch (multicast_address_record->record_type) \
                {   /* Ignore */ \
                case BLOCK_OLD_SOURCES: \
                case ALLOW_NEW_SOURCES: \
                    return; \
                } \
            } \
            if ( (comp_mode) == IGMPv1 ) \
            {\
            numsrc=0; \
                switch (multicast_address_record->record_type) \
                {\
                case BLOCK_OLD_SOURCES: \
                case  ALLOW_NEW_SOURCES: \
                case CHANGE_TO_INCLUDE_MODE: /* Ignore To _IN */ \
                    return; \
                }\
	    }
/*
 * Process an incoming group membership report. Note : this can be possible
 * only if The Router Alert Option have been set and I'm configured as a
 * router (net.inet6.ip6.forwarding=1) because report are sent to the
 * multicast group. processed in QUERIER and Non-QUERIER State
 * actually there is a timer per group/source pair. It's the easiest solution
 * but not really efficient...
 */

void acceptV3GroupReport(uint16_t ifindex, mifi_t mifi, uint32_t src, uint32_t group,  struct igmpv3_report *report_message )
{

    struct  igmpv3_grec *multicast_address_record;
    int             i, records_in_the_report, numsrc, totsrc;

    char *p;
    char buff[IFNAMSIZ+4];
    struct mvif  *sourceVif= &mvifs[mifi];
    // Sanitycheck the group adress...
    if (!IN_MULTICAST( ntohl(group) )) {
        log_msg(LOG_WARNING, 0, "The group address %s is not a valid Multicast group.",
                inetFmt(group));
        return;
    }


    if (sourceVif->InAdr.s_addr == src) {
#if 0
        log_msg(LOG_NOTICE, 0, "The IGMP message was from myself. Ignoring.");
#endif
        return;
    }

    // We have a IF so check that it's an downstream IF.
    if (sourceVif->uv_flags & VIFF_DOWNSTREAM)
    {

#if 0
        IF_DEBUG(DEBUG_MLD_PROTO)

        log_msg( LOG_DEBUG, 0,
                 "acceptV3GroupReport: "
                 "interface #%dv%d: %s src %s, grp %s\n",
                 ifindex, mifi,
                 sourceVif->mvif_name, inetFmt(src), inetFmt(group));
#endif

        // The membership report was OK... Insert it into the route table..
        sourceVif->uv_in_mld_report++;
    } else {
        // Log the state of the interface the report was recieved on.
        IF_DEBUG(DEBUG_MLD_PROTO)
        log_msg(LOG_INFO, 0, "Membership report was recieved on the upstream interface. Ignoring.");
        return;

    }



    if (sourceVif->uv_mld_version != IGMPv3) /* Cannot be because Report type 0x22 of IGMPv3  brings  us here */
    {
        IF_DEBUG(DEBUG_IF)
        log_msg(LOG_WARNING, 0,
                "BUG :Mif %s configured in older IGMP version received IGMPv3 report,ignored",
                sourceVif->mvif_name);
        return;
    }
#if 0
    IF_DEBUG(DEBUG_MLD_PROTO)
    log_msg(LOG_DEBUG, 0,
            "accepting multicast listener report from phys.interface id=%d %s: "
            "src %s,dst %s",
            ifindex, sourceVif->mvif_name,
            sa6_fmt(src), sa6_fmt(group));
#endif
    multicast_address_record= (struct igmpv3_grec *) report_message->grec;
    records_in_the_report = ntohs(report_message->ngrec);

    sourceVif->uv_in_mld_report++;

    /*
     * loop through each multicast record
     */

    totsrc = 0;
    for (i = 0; i < records_in_the_report; i++)
    {
        uint32_t mcast_group=multicast_address_record->grec_mca;
        uint16_t nsrcs=ntohs(multicast_address_record->grec_nsrcs);
#if 0
        IF_DEBUG(DEBUG_MLD_PROTO)
        log_msg(LOG_DEBUG, 0,
                "Interface:%s Processing multicast listener record #%d (addr %x,) from total of %d in a report of Group %s: ",
                sourceVif->mvif_name,i, multicast_address_record, records_in_the_report,
                sa6_fmt(mcast_group));

#endif
        accept_multicast_record( ifindex, mifi, multicast_address_record, src, mcast_group);
        /* nexr group record */
        multicast_address_record=(struct igmpv3_grec *)
                                 ( (void*) multicast_address_record + sizeof(*multicast_address_record)+
                                   (nsrcs)*
                                   (sizeof( multicast_address_record->grec_src[0]) ) );

    }
}
/*
* accept_multicast_record()
*                          -parses MLDv2 multicast record :
*                           for every part of multipart record processes ASM or SSM message content
*
*
*/
#ifndef record_type  /* make declaration to uniform to mldproxy */
#define record_type grec_type
#endif
static void accept_multicast_record(int ifindex, mifi_t mifi, struct igmpv3_grec * multicast_address_record,
                                    IP_ADDRESS multicast_subscriber_address,
                                    IP_ADDRESS required_multicast_group)
{
    struct mvif *v = &mvifs[mifi];
    int numsrc = ntohs(multicast_address_record->grec_nsrcs);

    int j;
    IP_ADDRESS mcast_source_ip_address;
    struct listaddr *s = NULL;
    struct listaddr *g = NULL;



    /* sanity check */
    if ( mifi == upStreamVif || (v->uv_flags & VIFF_UPSTREAM ) )
    {
        log_msg(LOG_WARNING, 0,
                "Sanity failed :Got listener report on non-listener phys. interface #=%d, mvif=%d",  ifindex, mifi );
        return;
    }

    if (required_multicast_group <= 0 ) // Sanity Check
    {
        log_msg(LOG_ERR, 0,
                "BUG Sanity failed :Got listener report with empty required_multicast_group"  );
        return;
    }
    /* just locate group */
    g = find_multicast_group(v, required_multicast_group); // returns g -: pointer to group record if any
    if ( v->uv_mld_version == IGMPv1 || v->uv_mld_version ==  IGMPv2 )
    {
        SWITCHV1V2 (v->uv_mld_version); /* Switching request  to IGMPv1/v2 -compat-mode */
    }
    if (g)
    {
        /* the group  FOUND */
        /* include/change_to_include with number of sources = 0 is the igmpv3 equivalent for leave message . block_old_sources
           might also indicate a leave message so in those cases we wouldnt want to stop the rxmt timer . in all other cases the igmpv3 report we just received indicated that at least 1 client is still interested
           in receiving multicast traffic from this group so the rxmt timer should be stopped.*/
       if(!((((multicast_address_record->record_type == MODE_IS_INCLUDE) || 
	    (multicast_address_record->record_type == CHANGE_TO_INCLUDE_MODE)) && (numsrc == 0))
	    || (multicast_address_record->record_type == BLOCK_OLD_SOURCES)))
       {
        stop_rxmt_timer(v, g);
        start_report_timer( v, g, MCAST_LISTENER_INTERVAL );

        // Prepare next retransmit when (if) leave|done message will be heard
        g->ma_llqc =(v->uv_fastleave ) ? 0 :  v->uv_mld_llqc;  // Prepare retrt count of Group Specific queries
        g->ma_CheckingListenerState = FALSE;
       /* SWITCHV1V2(g->ma_comp_mode); /* Switching request  to IGMPv1/v2 -compat-mode */
    }
    }

   
    switch (multicast_address_record->record_type)
    {
    case CHANGE_TO_INCLUDE_MODE:  // filter_mode changes to include, RFC  ref TO_IN(X), wheere X - source to include, i.e to join
    {
        IF_DEBUG(DEBUG_MLD_PROTO)
        log_msg(LOG_DEBUG, 0, "MLDV2 CHANGE_TO_INCLUDE_MODE  processing (G,S)=%s,*",
                sa6_fmt(required_multicast_group));
        if (numsrc == 0)
        {
            // filter mode is include 0 sources same as leave group
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
            {
                recv_listener_done(ifindex,mifi,multicast_subscriber_address,required_multicast_group); //UNIHAN ADD for PROD00219446
                break;
            }
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

        }
        else
        {   // numsrc  > 0
              if (g == NULL)
            {

                IF_DEBUG(DEBUG_MLD_PROTO)
                log_msg(LOG_NOTICE, 0,
                        "CHANGE_TO_INCLUDE_MODE received for invalid/empty G=%s, numsrc=%d ", sa6_fmt(required_multicast_group), numsrc);
                break;
            }
            if (g->ma_comp_mode == IGMPv1 ||g->ma_comp_mode == IGMPv2 )
            {
                IF_DEBUG(DEBUG_MLD_PROTO)
                log_msg(LOG_DEBUG, 0, "ignores CHANGE_TO_INCLUDE_MODE src list  in IGMPv1/v2 -compat-mode");
                return;
            }

            IF_DEBUG(DEBUG_MLD_PROTO)
            log_msg(LOG_DEBUG, 0, "MLDV2 CHANGE_TO_INCLUDE_MODE  processing N=%d source of  (G)=%s",
                    numsrc,
                    sa6_fmt(required_multicast_group));
            set_sources_mode(mifi, g, numsrc, &multicast_address_record->grec_src, CHANGE_TO_INCLUDE_MODE);
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
                return;
            }
            recv_listener_done(ifindex,mifi,multicast_subscriber_address,required_multicast_group);
            g->ma_filter_mode = MODE_IS_INCLUDE; // TODO RFC 3810 if no sources, created in EXCLUDE


        }
        else
        {   // (numsrc  > 0 )
            IF_DEBUG(DEBUG_MLD_PROTO)
            log_msg(LOG_DEBUG, 0, "MLDV2 MODE_IS_INCLUDE:  processing N=%d source of  (G)=%s",
                    numsrc,
                    sa6_fmt(required_multicast_group));
            if ( g == NULL )
            {
                IF_DEBUG(DEBUG_MLD_MEMBER)
                log_msg(LOG_DEBUG, 0,
                        "The group (G)=%s does not exist , trying to add it", sa6_fmt(required_multicast_group));
                g = make_new_group( mifi , required_multicast_group , MLDv2);
                mld_merge_with_upstream(mifi,  required_multicast_group,MLDv2,multicast_address_record->grec_src[0],MODE_IS_INCLUDE);
                
            }
             if (g->ma_comp_mode == IGMPv1 ||g->ma_comp_mode == IGMPv2 )
            {
                IF_DEBUG(DEBUG_MLD_PROTO)
                log_msg(LOG_DEBUG, 0, "ignores sources msg in IGMPv1/v2-compat-mode");
                return;
            }
            set_sources_mode(mifi, g, numsrc, &multicast_address_record->grec_src, MODE_IS_INCLUDE);
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
            log_msg(LOG_WARNING, 0, "BUG ALLOW_NEW_SOURCES: mode - no sources");
            return;
            //g=recv_listener_report(ifindex, mifi, multicast_subscriber_address, required_multicast_group, MLDv2);
            //break;
        }
	    // ARRIS MOD START
        // if the group does not exist create a group and try to merge with upstream
        if ( g == NULL )
        {
         
		    IF_DEBUG(DEBUG_MLD_MEMBER)
            log_msg(LOG_DEBUG, 0,
                    "The group (G)=%s does not exist , trying to add it", sa6_fmt(required_multicast_group));
            g = make_new_group( mifi , required_multicast_group , MLDv2); //TODO check multicast_subscriber_address ? sender or  SSM
            g->ma_filter_mode = MODE_IS_INCLUDE;            

        }
        if (g->ma_comp_mode == IGMPv1 ||g->ma_comp_mode == IGMPv2 )
        {

            IF_DEBUG(DEBUG_MLD_PROTO)
            log_msg(LOG_DEBUG, 0, "ignores ALLOW_NEW_SOURCE msg in IGMPv1/v2 -compat-mode");
            return;
        }
        set_sources_mode(mifi, g, numsrc, &multicast_address_record->grec_src, ALLOW_NEW_SOURCES);
        // ARRIS MOD END

        break;
    }
    case BLOCK_OLD_SOURCES:
    {

        if (g == NULL)
        {
            log_msg(LOG_WARNING, 0,
                    "Group %s does not exist, ignoring the  BLOCK_OLD_SOURCES report", sa6_fmt(required_multicast_group));
            return;
        }
        if (g->ma_comp_mode == IGMPv1 ||g->ma_comp_mode == IGMPv2 )
        {
            IF_DEBUG(DEBUG_MLD_PROTO)
            log_msg(LOG_DEBUG, 0, "ignores BLOCK msg in MLDv1-compat-mode");
            return;
        }

        IF_DEBUG(DEBUG_MLD_PROTO)
        log_msg(LOG_DEBUG, 0,"MLDV2 BLOCK_OLD_SOURCES processing (G,S)=%s,*", sa6_fmt(required_multicast_group));
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

            mcast_source_ip_address = multicast_address_record->grec_src[j];


            s = find_requested_mcast_transmitter(v, required_multicast_group, g, mcast_source_ip_address);
            if (s == NULL)
            {
                log_msg(LOG_WARNING, 0,
                        "Cannot accept BLOCK_OLD_SOURCE record"
                        "for non-existent source (G,S)=(%s,%s)", sa6_fmt(required_multicast_group), sa6_fmt( mcast_source_ip_address));
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
                sendSSMSpecificMemberQuery( v, g->mcast_group, g, s );

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
        }
        else
        {    //  (numsrc  > 0 )
              if ( g == NULL )
            {
                IF_DEBUG(DEBUG_MLD_MEMBER)
                log_msg(LOG_DEBUG, 0,
                        "The group (G)=%s does not exist , trying to add it", sa6_fmt(required_multicast_group));
                g = make_new_group( mifi , required_multicast_group , MLDv2);
                //mld_merge_with_upstream(mifi,  required_multicast_group,  MLDv2); // try to merge  with upstream interface database in SSM mode
            }
              mld_merge_with_upstream(mifi,  required_multicast_group,  MLDv2,0,MODE_IS_EXCLUDE); // try to merge  with upstream interface database in SSM mode
            set_sources_mode(mifi, g, numsrc, &multicast_address_record->grec_src, MODE_IS_EXCLUDE);
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
        log_msg(LOG_DEBUG, 0, "Multicast filter state change CHANGE_TO_EXCLUDE_MODE processing (G,S)=%s,*", sa6_fmt(required_multicast_group));

        
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

                  IF_DEBUG(DEBUG_MLD_MEMBER)
                log_msg(LOG_DEBUG, 0,
                        "The group (G)=%s does not exist , trying to add it", sa6_fmt(required_multicast_group));
                g = make_new_group( mifi , required_multicast_group , MLDv2);
               
            }
          //  if (g->ma_comp_mode == IGMPv1 ||g->ma_comp_mode == IGMPv2 )
           // {
           //    IF_DEBUG(DEBUG_MLD_PROTO)
           //     log_msg(LOG_DEBUG, 0, "ignores sources msg in IGMPv1/v2 -compat-mode");
            //    return;
           // }
            mld_merge_with_upstream(mifi,  required_multicast_group,  MLDv2,0,MODE_IS_EXCLUDE); // try to merge  with upstream interface database in SSM mode
            set_sources_mode(mifi, g, numsrc, &multicast_address_record->grec_src, CHANGE_TO_EXCLUDE_MODE);

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


