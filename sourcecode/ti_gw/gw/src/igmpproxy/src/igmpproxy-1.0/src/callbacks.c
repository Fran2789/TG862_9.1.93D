/*

  Copyright(c) 2012 Intel Corporation. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in
	  the documentation and/or other materials provided with the
	  distribution.
	* Neither the name of Intel Corporation nor the names of its
	  contributors may be used to endorse or promote products derived
	  from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <errno.h>
#include "defs.h"
#include "vif.h"
#include "debug.h"

#include "timers.h"
#include "groups.h"
#include "inet6.h"
#include "igmpproxy.h"

static inline int parameters_sanity_check(void *p)
{
    timer_cbk_t *params = (timer_cbk_t *) p;
    u_int64_t expCounter;
    /* Sanity */
    if ((!p) )
    {
        log_msg (LOG_ERR, 0,
                 "%s callback parameters sanity, (p) NULL", __func__, p);
	return -1;

    }
    if ( !params->g)
    {
        log_msg (LOG_ERR, 0,
                 "%s callback parameters sanity, (p=%p) group param->g = NULL", __func__, p);
	return FALSE;
    }
   
    return TRUE;
}

static inline int group_sanity_check(void *p , int fd_group_timer)
{
    timer_cbk_t *params = (timer_cbk_t *) p;
    u_int64_t expCounter;
    
    /* Timer  may happen when the group have just been deleted */
    if (!find_group_in_list (params->v, params->g))
    {
        IF_DEBUG (DEBUG_MLD_MEMBER | DEBUG_MLD_TIMER)
        log_msg (LOG_DEBUG, 0,
                 "Expire Timer may happen when the group have just been deleted, group %s does not exist at interface %s",
                 sa6_fmt (params->g->mcast_group), params->v->mvif_name);
	if (fd_group_timer ) /* will be 0 on sources timers */
	    read(fd_group_timer, &expCounter, sizeof(expCounter) );
        return FALSE;
    }
     return TRUE;
}

static inline int source_sanity_check(void *p ,  int fd_source_timer)
{ 
    timer_cbk_t *params = (timer_cbk_t *) p;
    u_int64_t expCounter;
    
    /* Timer  may happen when the group have just been deleted */
    
    if ( params->source )
    {
	if (!find_source_in_list (params->v, params->g, params->source))
	{
	    IF_DEBUG (DEBUG_MLD_MEMBER)
	    log_msg (LOG_DEBUG, 0,
                 "Expire Timer may happen when the source have just been deleted, G,S= %s.%s does not exist at interface %s",
		    sa6_fmt (params->g->mcast_group),
		    sa6_fmt (params->source->mcast_group), params->v->mvif_name);
	    if (fd_source_timer )
		read(fd_source_timer, &expCounter, sizeof(expCounter) );
	    return FALSE;
	}
    }
    
    return TRUE;
}



/*
 * Rtrmt timer for  muticast address or a source(s) time outed
*/
void
ExpireRtrmtTimer (void *p)
{
    int rc;
    u_int64_t expCounter;
    timer_cbk_t *params = (timer_cbk_t *) p;
    register struct mvif *v = &mvifs[params->mifi];

    if ( ! parameters_sanity_check(p ) )   return;
    
    if ( ! group_sanity_check(p, params->g->group_rxmt_timer ) )   return;
    
    rc=read(params->g->group_rxmt_timer, &expCounter, sizeof(expCounter) );

    IF_DEBUG (DEBUG_TIMER)
    log_msg (LOG_DEBUG, 0, "ExpireRtrmtTimer for G=%s  timeout at %lu",
             sa6_fmt ((params->g->mcast_group)),
             time (NULL) - params->g->ma_time);

    params->g->ma_CheckingListenerState = TRUE;	// Guard repetetive reports
    params->g->ma_llqc--;
    /*
     * Multicast-Address-Specific Queries sent in response to
     Done messages ,
     */
    if ( v->uv_fastleave || ((params->g->ma_llqc) <= 0)
            || params->g->ma_llqc  > MLD6_ROBUSTNESS_VARIABLE )
    {
        IF_DEBUG (DEBUG_TIMER)
        log_msg (LOG_DEBUG, 0,
                 "ExpireRtrmtTimer, group %s  at interface %s  Rtrtm counter is %d and timer must be  stopped",
                 sa6_fmt (params->g->mcast_group),
                 mvifs[params->mifi].mvif_name, params->g->ma_llqc);
        delete_group (params->mifi, params->g);
        return;
    }

    /*
     * "Send Q(MA,X)"
     * When a table action "Send Q(MA,X)" is encountered by the Querier in
     the table in section 7.4.2, the following actions must be performed
     for each of the sources in X that send to multicast address MA, with
     source timer larger than LLQT:

     o  Lower source timer to LLQT; // TODO  start_report_timer (LLQT)  will cause on first lost retransmit to delete group

     o  Add the sources to the Retransmission List;

     o  Set the Source Retransmission Counter for each source to [Last
     Listener Query Count].
     *
     */
    else

    {
        if ( (params->g->ma_comp_mode == IGMPv1) ||(params->g->ma_comp_mode == IGMPv2)
                || (params->g->ma_comp_mode == IGMPv3 ) )
        {
            sendGroupSpecificMemberQuery ( params->v, params->g);
        }
        else
        {
            log_msg (LOG_WARNING, 0,
                     "%s Invalid group %s at %s compatability, neither v1 nor v2 nor v3", __func__ ,
                     sa6_fmt (params->g->mcast_group),
                     mvifs[params->mifi].mvif_name);
        }
    }
    IF_DEBUG (DEBUG_TIMER)
    log_msg (LOG_DEBUG, 0,
             "ExpireRtrmtTimer, group %s  at interface %s  Rtrtm counter is %d , starting report_timer\n",
             sa6_fmt (params->g->mcast_group), mvifs[params->mifi].mvif_name,
             params->g->ma_llqc);
    if (params->source != NULL)
    {
        start_report_timer (v, params->source, MLD6_LISTENER_INTERVAL);	  // Try to clear up the source in the group if no responces are received
    }

    else

    {
        start_report_timer (v, params->g, MLD6_LISTENER_INTERVAL);	  // Try to clear up the group if no responces are received
    }
}
void
IgmpV1_TimerExpire_and_back_to_Higher_Igmp (void *p)
{
    int rc;
    u_int64_t expCounter;
    timer_cbk_t *params = (timer_cbk_t *) p;
    struct mvif * v= params->v;
    
    
    if ( ! parameters_sanity_check(p) )   return;
    
    /* RFC 3810  -cancel all pending responces and retransmittions timers */

    
    if ( params->g->ma_comp_mode != IGMPv1)
    {
        log_msg (LOG_DEBUG, 0,
                 "%s:BUG Wrong IGMP compatibility mode=%d for G=%s on %s, group should be  in IGMPv2" , __func__,
                 sa6_fmt ((params->g->mcast_group)), params->v->mvif_name
                );

    }
    IF_DEBUG (DEBUG_TIMER)
    log_msg (LOG_DEBUG, 0,
             "Expired back2Igmpv2_ Timer for G=%s on %s,  timeout in %lu secs ",
             sa6_fmt ((params->g->mcast_group)), params->v->mvif_name,
             time (NULL) - params->g->ma_time);

    rc = read(params->g->group_back2Igmpv2_timer, &expCounter, sizeof(expCounter) );

    stop_back2Igmpv2_timer (params->v, params->g); /* Destroy  timer */

    if  (params->g->group_back2Igmpv3_timer &&  // if IGMPv2 to IGMPv3  timer was created - it has fd > 0
            params->g->ma_comp_mode != IGMPv3)
    {
        /* IGMPv2 Host Present running */
        IF_DEBUG (DEBUG_TIMER)
        log_msg (LOG_DEBUG, 0,
                 "Expired back2Igmpv2 Timer for G=%s on %s, group was in IGMPv1, SWITCHING to IGMPv2, stopping the timer ",
                 sa6_fmt ((params->g->mcast_group)), params->v->mvif_name
                );

        params->g->ma_comp_mode = IGMPv2; /* RFC says - just swithch to IGMPv2 */

        // if IGMPv2 to IGMPv3  timer was created - restart  it

        start_back2Igmpv3_timer (params->v, params->g, MLD6_OLDER_VERSION_HOST_PRESENT );
    }
    else
    {       /* No timer for IGMPv2 to v3 - Return to V3  */
        IF_DEBUG (DEBUG_TIMER)
        log_msg (LOG_DEBUG, 0,
                 "Expired Back_To_IGMPv3_ Timer for G=%s on %s, group was in IGMPv1, SWITCHING to IGMPv3",
                 sa6_fmt ((params->g->mcast_group)), params->v->mvif_name
                );


        if  (params->g->group_back2Igmpv3_timer)
            stop_back2Igmpv3_timer (params->v, params->g);
        params->g->ma_comp_mode = IGMPv3;
	start_all_sources_timers(params->v, params->g, MLD6_LAST_LISTENER_QUERY_TIMER);
    }
    
    {  /* make summar IGMP compatibility status  of interface */
        struct listaddr *g;

        v->uv_older_host_present=0;
        /* Look for the group in our listener list. */
        for (g = v->uv_groups; g != NULL; g = g->ma_next)
        {

            v->uv_older_host_present |= g->ma_comp_mode;

        }
        
	if (v->uv_older_host_present == 0 ) 
	    v->uv_older_host_present= v->uv_mld_version;
    }
    sendGeneralMembershipQuery(params->v);
}

void
IgmpV2_TimerExpire_and_back_to_Higher_Igmp (void *p)
{
    int rc;
    u_int64_t expCounter;
    timer_cbk_t *params = (timer_cbk_t *) p;
    struct mvif * v= params->v;
    
    if ( ! parameters_sanity_check(p) )   return;
    
    /* RFC 3810  -cancel all pending responces and retransmittions timers */

    

    IF_DEBUG (DEBUG_TIMER)
    log_msg (LOG_DEBUG, 0,
             "Expired back2Igmpv3_ Timer for G=%s on %s,  timeout in %lu secs ",
             sa6_fmt ((params->g->mcast_group)), params->v->mvif_name,
             time (NULL) - params->g->ma_time);

    rc = read(params->g->group_back2Igmpv3_timer, &expCounter, sizeof(expCounter) );

    stop_back2Igmpv3_timer (params->v, params->g); /* Destroy  timer */
    if (params->g->ma_comp_mode == IGMPv2 )
    {
        IF_DEBUG (DEBUG_TIMER)
        log_msg (LOG_DEBUG, 0,
                 "Expired Back_To_IGMPv3_ Timer for G=%s on %s, group was in IGMPv2, SWITCHING to IGMPv3",
                 sa6_fmt ((params->g->mcast_group)), params->v->mvif_name
                );
        params->g->ma_comp_mode = IGMPv3;
    }
    else
    {
        IF_DEBUG (DEBUG_TIMER)
        log_msg (LOG_DEBUG, 0,
                 "%s:BUG Wrong IGMP compatibility mode=%d for G=%s on %s, group should be  in IGMPv2,SWITCHING to IGMPv3 " , __func__,
                 sa6_fmt ((params->g->mcast_group)), params->v->mvif_name
                );
        params->g->ma_comp_mode = IGMPv3;
	
    }
    
    start_all_sources_timers(params->v, params->g, MLD6_LAST_LISTENER_QUERY_TIMER);
    
    {  /* Detect Protocol version compatibility status  of interface */
        struct listaddr *g;

        v->uv_older_host_present=0;
        /* Look for the group in our listener list. */
        for (g = v->uv_groups; g != NULL; g = g->ma_next)
        {

            v->uv_older_host_present |= g->ma_comp_mode; /* summarize all groups */

        }
    }
    
    if (v->uv_older_host_present == 0 ) 
	v->uv_older_host_present= v->uv_mld_version;
    sendGeneralMembershipQuery(params->v);
    start_report_timer(params->v, params->g,MLD6_LAST_LISTENER_QUERY_TIMER);
    sendGroupSpecificMemberQuery(params->v, params->g);

}



/*
 * timer for  muticast address or a source(s) time has been expired
*/
void
ExpireReportTimer (void *p)
{
    int rc;
    u_int64_t expCounter;
    timer_cbk_t *params = (timer_cbk_t *) p;
    struct mvif *v = &mvifs[params->mifi];

    /* Group membership had expired */
    /* if no reports was heard, send query - No */
    /* RFC says
       If an address's timer expires, it is
       assumed that there are no longer any listeners for that address
       present on the link, so it is deleted from the list and its
       disappearance is made known to the multicast routing component.
     */

    if ( ! parameters_sanity_check(p ) )   return;
   
    if ( ! group_sanity_check(p, params->g->group_report_timer ) )   return;

    rc=read(params->g->group_report_timer, &expCounter, sizeof(expCounter) );

    IF_DEBUG (DEBUG_TIMER)
    log_msg (LOG_DEBUG, 0,
             "Expired Report Timer for G=%s on %s,  timeout in %lu secs ",
             sa6_fmt ((params->g->mcast_group)), v->mvif_name,
             time (NULL) - params->g->ma_time);


    if (  params->v->uv_fastleave)
    {
        delete_group (params->mifi, params->g);
    } else if (params->g->ma_CheckingListenerState == TRUE  )
    {
        /* sometimes report expire may happen during keep-alive retransmittions  */
        return;
    } else
    {
        params->g->ma_llqc = v->uv_mld_llqc;
        params->g->ma_CheckingListenerState = TRUE;	// Guard repetetive reports
        start_rxmt_timer (params->v, params->g, v->uv_mld_llqi );
        start_report_timer (params->v, params->g, v->uv_mld_llqi *v->uv_mld_robustness);
        sendGroupSpecificMemberQuery (params->v, params->g);
    }
}


/*
 * Filter mode timer for  muticast address  time outed
 */
void
ExpireFilterModeTimer (void *p)
{
    int rc;
    u_int64_t expCounter;
    timer_cbk_t *params = (timer_cbk_t *) p;
    register struct mvif * v= params->v;

    /* On expiration of A Filter mode timer we try to return from EXCLUDE mode to INCLUDE */

    if ( ! parameters_sanity_check(p ) )   return;

    rc=read(params->g->mldv2_filterMode_timer, &expCounter, sizeof(expCounter) );
    IF_DEBUG (DEBUG_TIMER)
    log_msg (LOG_DEBUG, 0, "Expired FilterMode Timer for G=%s on %s\n",
             sa6_fmt ((params->g->mcast_group)), v->mvif_name);

    // 7.5.  Switching Router Filter Modes
    switch (params->g->ma_filter_mode)
    {
    case MODE_IS_INCLUDE:

    {			  /* Sources from the Requested List are moved in the Include List,
	 while sources from the Exclude List are deleted. */
        start_all_sources_timers (params->v, params->g, MLD6_LISTENER_INTERVAL);
    }
    break;
    case MODE_IS_EXCLUDE:
        params->g->ma_filter_mode = MODE_IS_INCLUDE;
        start_all_sources_timers (v, params->g, MLD6_LISTENER_INTERVAL);
        break;
    default:
        log_msg (LOG_WARNING, 0,
                 "FilterMode Timer:BUG:Multicast group %s filter mode is invalid, neither EX nor IN",
                 sa6_fmt (params->g->mcast_group));
        params->g->ma_filter_mode = MODE_IS_INCLUDE;
        break;
    }
    if ( (!( params->g->ma_comp_mode & MLDv1 )) || (!( params->g->ma_comp_mode & MLDv2 )))
    {
        log_msg (LOG_WARNING, 0,
                 "FilterMode Timer: Multicast group %s  MLD compatabilty mode is invalid, neither MLDv1 nor MLDv2",
                 sa6_fmt (params->g->mcast_group));
    } else
    {
        sendGeneralMembershipQuery( v);
    }
}
void
ExpireSourceTimer (void *p)
{
    int rc;
    u_int64_t expCounter;
    timer_cbk_t *params = (timer_cbk_t *) p;
    register struct mvif *v = &mvifs[params->mifi];

    /* Source membership had expired */
    /* if no reports was heard, send query - No */
    /* RFC says
       If an address's timer expires, it is
       assumed that there are no longer any listeners for that address
       present on the link, so it is deleted from the list and its
       disappearance is made known to the multicast routing component.
     */
    /* Sanity */
    if ( ! parameters_sanity_check(p ) )   return;
    if ( ! source_sanity_check(p, params->source->group_report_timer ) )   return;

    rc=read(params->source->group_report_timer, &expCounter, sizeof(expCounter) );
    IF_DEBUG (DEBUG_TIMER)
    log_msg (LOG_DEBUG, 0, "Expired Source Timer for (G,S)= (%s,%s on %s",
             sa6_fmt ((params->g->mcast_group)),
             sa6_fmt ((params->source->mcast_group)), v->mvif_name);

    if (params->v->uv_fastleave)
    {
        /*  RFC3810
         * If the timer of a source from the Include List expires, the source is deleted from the Include List *
         */
        IF_DEBUG (DEBUG_TIMER)
        log_msg (LOG_DEBUG, 0,
                 "Expired Source Report Timer at fast leave mode of (G,S) %s  at interface %s, deleting source",
                 sa6_fmt (params->g->mcast_group),
                 sa6_fmt (params->source->mcast_group),
                 mvifs[params->mifi].mvif_name);
        delete_source (params->mifi, params->g, params->source);
        return;
    } else if (params->source->ma_CheckingListenerState == TRUE  )
    {
        /* sometimes report expire may happen during keep-alive retransmittions  */
        return;
    } else
    {
        params->source->ma_CheckingListenerState = TRUE; // Guard repetetive reports
        if ( params->g->sources )
        {
            sendSSMSpecificMemberQuery ( v, params->g->mcast_group, params->g, params->source);
            start_rxmt_timer (v, params->source,
                              MLD6_LAST_LISTENER_QUERY_INTERVAL);	// will retry ma_llqc times until decide to delete group
            //To  delete group with empty source list
            start_report_timer (v, params->g,  MLD6_LAST_LISTENER_QUERY_TIMER);				  // Lower report timer
        }
        return;
    }
}

void
ExpireSourceRtrmtTimer (void *p)
{
    int rc;
    u_int64_t expCounter;
    timer_cbk_t *params = (timer_cbk_t *) p;
    struct mvif *v = &mvifs[params->mifi];

    

    if ( ! parameters_sanity_check(p ) )   return;
    if ( ! source_sanity_check(p, params->source->group_rxmt_timer ) )   return;
    
    rc=read(params->source->group_rxmt_timer, &expCounter, sizeof(expCounter) );

    IF_DEBUG (DEBUG_TIMER)
    log_msg (LOG_DEBUG, 0,
             "Expired Source Retransmission Timer for (G,S)= (%s,%s) on %s\n",
             sa6_fmt ((params->g->mcast_group)),
             sa6_fmt ((params->source->mcast_group)), v->mvif_name);

    /*
     *  RFC 3810 defines fast leave as N query  restransmit attemts ???

     */
    params->source->ma_llqc--;
    params->source->ma_CheckingListenerState = TRUE; // Guard repetetive reports
    if (v->uv_fastleave || (params->source->ma_llqc) <= 0
            || params->source->ma_llqc  > MLD6_ROBUSTNESS_VARIABLE )
    {
        IF_DEBUG (DEBUG_MLD_MEMBER)
        log_msg (LOG_DEBUG, 0,
                 "%s: (G,S)=%s,%s  at interface %s   Rtrtsmit is done (llgc=%d) and source must be removed", __func__,
                 sa6_fmt (params->g->mcast_group),
                 sa6_fmt (params->source->mcast_group), v->mvif_name,
                 params->source->ma_llqc);
        delete_source (params->mifi, params->g, params->source);

        if (params->g->sources == NULL && v->uv_fastleave)
        {
            delete_group(params->mifi, params->g );	/* deletes group on LAN and synchronizes with WAN */
            return;
        }

        /* else- soft leave -Send query to detect whether group has subscribers, otherwise provide group deletion */
        if ( params->g->sources == NULL)
        {
            params->g->ma_llqc=(v->uv_fastleave)? v->uv_mld_llqc :0 ;
            sendGroupSpecificMemberQuery( v, params->g);
            start_rxmt_timer (v, params->g, v->uv_mld_llqi );
            start_report_timer (v, params->g, (v->uv_mld_llqi * v->uv_mld_robustness ) );	  // Lower report timer
        }

        if ( params->g->sources)
        {
            /* if Group is not empty, i.e contains sources, try to detect them */
            sendGroupSpecificMemberQuery( v, params->g);
            start_all_sources_timers(v, params->g, MLD6_LAST_LISTENER_QUERY_TIMER ); // Lower sources timer


        }
    }

    else
    {
        IF_DEBUG (DEBUG_TIMER)
        log_msg (LOG_DEBUG, 0,
                 "Expired Source Retransmission Timer for (G,S)= (%s,%s), sending GSS Query\n",
                 sa6_fmt ((params->g->mcast_group)),
                 sa6_fmt ((params->source->mcast_group)));
        sendSSMSpecificMemberQuery ( v, params->g->mcast_group, params->g, params->source);
    }
}


/*
	Generic timer for the interface MVIF[mifi]  time outed
*/
void
ExpireGenericQueryTimer (void *p)
{
    int rc;
    u_int64_t expCounter;

    /* rearm the timer ? */
    mvif_timer_cbk_t *params = (mvif_timer_cbk_t *) p;
    struct mvif *v = &mvifs[params->mifi];

    /* Sanity */
    if ((!p) )
    {
        log_msg (LOG_ERR, 0,
                 "%s callback parameters sanity, (p) NULL", __func__, p);

    }
    rc=read(v->uv_genQuery_timer, &expCounter, sizeof(expCounter) );
    IF_DEBUG (DEBUG_TIMER)
    log_msg (LOG_DEBUG, 0, "ExpireGenericQueryTimer on interface %s",
             v->mvif_name);
    v->uv_q_time = time (NULL);	  /* Store last expiration time */
    /* Are we at startup  mode ? */
    if ( v->uv_startupQueryCount-- <=0)
    {
        /* Change timer interval on Transit from the startup to regular mode */
        stop_genQuery_timer (v);
        start_genQuery_timer (v, v->uv_mld_query_interval);
    }

    sendGeneralMembershipQuery( params->v);

}
