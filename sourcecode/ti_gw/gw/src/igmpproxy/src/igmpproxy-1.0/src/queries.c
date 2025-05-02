/*
**  igmpproxy - IGMP proxy based multicast router
**  Copyright (C) 2005 Johnny Egeland <johnny@rlo.org>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**----------------------------------------------------------------------------
**
**  This software is derived work from the following software. The original
**  source code has been modified from it's original state by the author
**  of igmpproxy.
**
**  smcroute 0.92 - Copyright (C) 2001 Carsten Schill <carsten@cschill.de>
**  - Licensed under the GNU General Public License, version 2
**
**  mrouted 3.9-beta3 - COPYRIGHT 1989 by The Board of Trustees of
**  Leland Stanford Junior University.
**  - Original license can be found in the Stanford.txt file.
**
*/
/**
*   queries.c
*
*   Functions for building Queries.
*
*/

#include "igmpproxy.h"
#include "mcast_proto.h"
#include "debug.h"
#include "inet6.h"
#include "groups.h"
#include <linux/igmp.h>

/*
 * IGMPv1 Query: length = 8 octets AND Max Resp Code field is zero
 * IGMPv2 Query: length = 8 octets AND Max Resp Code field is non-zero
 * IGMPv3 Query: length >= 12 octets
*/
#define EXTRA_IGMPV3_QRY_HEADER_LEN 4 /* IgmpV3 header is 12 octets vs IgmpV2 header which is 8 bytes */
static int calculateResponse(const struct mvif * v)
{
    /* Max Resp Code, QQIC - maximum time allowed before sending a responding report */
    if ( v->uv_mld_version == IGMPv1 )
        return 0;
    else if ( v->uv_mld_version == IGMPv2 )
        return v->queryResponseInterval;
    else /*if ( v->uv_mld_version == IGMPv3 ) */
        return v->queryResponseInterval;
}
/*
*   Sends a general membership query on downstream VIFs
*/
int  sendGeneralMembershipQuery( struct mvif *v)
{
    int rc;
    mifi_t vifi=v->uv_mifi;
 

    {  /* Update Protocol version  compatibility status  of interface */
        struct listaddr *g;

        v->uv_older_host_present=0;
        /* Look for the group in our listener list. */
        for (g = v->uv_groups; g != NULL; g = g->ma_next)
        {
            v->uv_older_host_present |= g->ma_comp_mode;  /* summarize all groups */
        }
        if (    v->uv_older_host_present== 0 )
	    v->uv_older_host_present = v->uv_mld_version;
    }
    

    if ( ( v->uv_older_host_present & IGMPv2 ) || (v->uv_older_host_present & IGMPv1 )  )
    {
	IF_DEBUG (DEBUG_MLD_PROTO)
    
        log_msg(LOG_DEBUG, 0,
                "Sending generic membership query IGMPv%u from %s to %s. ResponseInterval in ms: %d",
                v->uv_older_host_present,
		inetFmt(v->InAdr.s_addr),
                inetFmt(allhosts_group),
                calculateResponse(v));
    
        rc=sendIgmp(vifi,v->InAdr.s_addr, allhosts_group,
                    IGMP_MEMBERSHIP_QUERY,
                    calculateResponse(v) , 0, 0);
    }
    else
    {
        if ( v->uv_older_host_present & IGMPv3 )
        {
            // ARRIS MOD START - include Router Alert option in IP header
            struct igmpv3_query *mhp= (struct igmpv3_query *) (send_buf + MIN_IP_HEADER_LEN + ROUTER_ALERT_OPTION_LEN);
            // ARRIS MOD END
            /* Fill in IGMPv3 query header */
            mhp->qrv=MCAST_ROBUSTNESS_VARIABLE;
            mhp->qqic=MLD6_QUERY_INTERVAL;
            mhp->suppress=0; /* SFLAG=1=YES */
            mhp->nsrcs=0;
            mhp->srcs[0]=0; /* zero the area */
	IF_DEBUG (DEBUG_MLD_PROTO)
    
        log_msg(LOG_DEBUG, 0,
                "Sending generic membership query IGMPv%u from %s to %s. ResponseInterval in ms: %d",
                v->uv_older_host_present,
		inetFmt(v->InAdr.s_addr),
                inetFmt(allhosts_group),
                calculateResponse(v));
            
            rc=sendIgmp(vifi,v->InAdr.s_addr, allhosts_group,
                        IGMP_MEMBERSHIP_QUERY,
                        calculateResponse(v) , 0, EXTRA_IGMPV3_QRY_HEADER_LEN);
        }
    }
    return rc;
}

/*
*   Sends a group specific membership  query until the
*   group times out...
*/
int sendGroupSpecificMemberQuery(  struct mvif *v,  struct listaddr * g_lan)
{
    int rc;
    struct listaddr  *group_sources;
    uint32_t mcast_group_address = g_lan->mcast_group;
    mifi_t vifi=v->uv_mifi;

    IF_DEBUG(DEBUG_MLD_PROTO)
    log_msg(LOG_DEBUG, 0, "%s :Group Specific membership query mode V%x mode interface V%x for group G=%s on LAN=%s in older v%x", __func__,
            g_lan->ma_comp_mode, v->uv_mld_version,
            inetFmt(g_lan->mcast_group) , v->mvif_name,v->uv_older_host_present
           );


    if (g_lan->ma_comp_mode == IGMPv1 || g_lan->ma_comp_mode == IGMPv2)
    {
        IF_DEBUG (DEBUG_MLD_PROTO)
        {
            log_msg(LOG_DEBUG, 0, "Sending IGMpv2 Group Specific membership query from %s to %s. Delay: %d",
                    inetFmt(v->InAdr.s_addr), inetFmt(mcast_group_address),
                    calculateResponse(v));
        }
        rc=sendIgmp(vifi, v->InAdr.s_addr, mcast_group_address,
                    IGMP_MEMBERSHIP_QUERY,
                    v->lastMemberQueryInterval*MLD6_TIMER_SCALE,
                    mcast_group_address, 0);
    }
    else
    {
        // ARRIS MOD START - inlude Router Alert option in IP header
        struct igmpv3_query *mhp= (struct igmpv3_query *) (send_buf + MIN_IP_HEADER_LEN + ROUTER_ALERT_OPTION_LEN);
        // ARRIS MOD END
        /* Fill in Extras in IGMPv3 query header */
        mhp->qrv=MCAST_ROBUSTNESS_VARIABLE;
        mhp->qqic=MLD6_QUERY_INTERVAL;
        mhp->suppress=0; /* SFLAG=1=YES */
        mhp->nsrcs=0;
        mhp->srcs[0]=0; /* zero the area */

        log_msg(LOG_DEBUG, 0, "Sending IGMpv3 Group Specific membership query from %s to %s. Delay: %d",
                inetFmt(v->InAdr.s_addr), inetFmt(mcast_group_address),
                calculateResponse(v));

        rc=sendIgmp(vifi, v->InAdr.s_addr, mcast_group_address,
                    IGMP_MEMBERSHIP_QUERY,
                    v->lastMemberQueryInterval*MLD6_TIMER_SCALE,/* the multiply is because the send_igmp will devide this value in 10 */
                    mcast_group_address,EXTRA_IGMPV3_QRY_HEADER_LEN );
        v->uv_out_mld_query++;

        /*
         * If present, send also SSM query
         */

        if (g_lan->ma_comp_mode == IGMPv3 )
        {
            group_sources=g_lan->sources;
            if (group_sources)
                rc=sendSSMSpecificMemberQuery(v, mcast_group_address, g_lan, group_sources);
        }
        return rc;

    }
}
/*
      *   Sends a SSM membership  query until the
      *   SSM times out...
*/
/*
 * Send a group-source-specific query.
 * Two specific queries are built and sent:
 *  1) one with S-flag ON with every source having a timer <= LLQI
 *  2) one with S-flag OFF with every source having a timer >LLQI
 * So we call send_mldv2() twice for different set of sources.
 */
int sendSSMSpecificMemberQuery( struct mvif *v, uint32_t  group,
                                struct listaddr * group_db_record,
                                struct listaddr * source_db_record
                              )
{
    int rc;
    int nbsrc = 0;
    struct listaddr *lstsrc;
    mifi_t vifi=v->uv_mifi;

    if (v->uv_flags != VIFF_DOWNSTREAM )
    {
        IF_DEBUG(DEBUG_MLD_PROTO)
        log_msg(LOG_DEBUG, 0,
                "%s BUG: cannot send group-source-specific qyery on : %s, it is not downstream ", __func__ ,
                v->mvif_name);
        return;
    }
    if ((v->uv_flags & VIFF_QUERIER) == 0 || (v->uv_flags & VIFF_NOLISTENER))
    {
        IF_DEBUG(DEBUG_MLD_PROTO)
        log_msg(LOG_DEBUG, 0,
                "%s BUG: could't group-source-specific query due to a lack of querying rights,", __func__);
        return;
    }
    IF_DEBUG(DEBUG_MLD_PROTO)
    log_msg(LOG_DEBUG, 0,
            "sending multicast listener SSM query for (G,S)= (%s,%s) on : %s",
            sa6_fmt(group_db_record->mcast_group),
            sa6_fmt(source_db_record->mcast_group),
            v->mvif_name);
    IF_DEBUG (DEBUG_PKT)
    {
        log_msg(LOG_DEBUG, 0, "Sending membership query from %s to %s. Delay: %d",
                inetFmt(v->InAdr.s_addr), inetFmt(group),
                v->lastMemberQueryInterval);
    }

    /* Fill *send_buf */
    /* scan the source-list only in case of GSS query */


    /* struct igmpv3_query <linux/igmp.h> */
    // ARRIS MOD START - inlude Router Alert option in IP header
    struct igmpv3_query *mhp= (struct igmpv3_query *) (send_buf + MIN_IP_HEADER_LEN + ROUTER_ALERT_OPTION_LEN);
    // ARRIS MOD END
    /* Fill in IGMPv3 query header */
    mhp->qrv=MCAST_ROBUSTNESS_VARIABLE;
    mhp->qqic=MLD6_QUERY_RESPONSE_INTERVAL;
    mhp->suppress=1; /* SFLAG=YES */


    lstsrc = group_db_record->sources;

    while ( lstsrc  )
    {


        memcpy(&mhp->srcs[nbsrc] ,&lstsrc->mcast_group, sizeof (IP_ADDRESS));
        IF_DEBUG (DEBUG_MLD_PROTO)
        log_msg (LOG_DEBUG, 0,
                 "%s :BUILDING GSM list -  appending source[%d] (G,S)=(%s,%s)  interface %s", __func__,
                 nbsrc,
                 sa6_fmt (group),
                 sa6_fmt (lstsrc->mcast_group),
                 v->mvif_name
                );
#if 0
        log_msg (LOG_DEBUG, 0,
                 "BUILDING GSM list Address mhp->srcs=%p,&mhp->srcs=%p mhp->srcs[0]=%p &mhp->srcs[nbsrc]=%p",
                 mhp->srcs, &mhp->srcs,  &mhp->srcs[0],  &mhp->srcs[nbsrc]
                );
#endif
        nbsrc++;
        mhp->srcs[nbsrc]=0;
        if  (lstsrc->ma_next)
            lstsrc=lstsrc->ma_next;
        else
            break;
    }


    mhp->nsrcs=htons(nbsrc);


    rc=sendIgmp(vifi, v->InAdr.s_addr, group,
                IGMP_MEMBERSHIP_QUERY,
                v->lastMemberQueryInterval*MLD6_TIMER_SCALE, /* the multiply is because the send_igmp will devide this value in 10 */
                group,
                nbsrc* sizeof(IP_ADDRESS) +EXTRA_IGMPV3_QRY_HEADER_LEN    /* calculate checksum for the IGMP_MINLEN + nbsrc* sizeof(IP_ADDRESS) +4  */
               );

    v->uv_out_mld_query++;

    return rc;

}

