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
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include "defs.h"
#include "vif.h"
#include "kern.h"
#include "igmpproxy.h"
#include "mcast_proto.h"
//#include "igmpv2_proto.h"
#include "igmpv3_proto.h"
#include "debug.h"
#include "inet6.h"

#include "groups.h"
#include "timers.h"
/*
for some unknown reason -t mangle chain forward ???
for some unknown reason -i rndbr1 -o rndbr1 in magle table ?
*/
// define IP6T_CMD "ip6tables -%s FORWARD -i %s -o %s -p udp --dst %s --src %s -j DROP"
#define IPT_INSERT_CMD "iptables -t filter -I FORWARD 1 -i %s -o %s -p udp --dst %s --src %s -j DROP"
#define IPT_DEL_CMD "iptables -t filter -D FORWARD  -i %s -o %s -p udp --dst %s --src %s -j DROP"
static int
make_iptables_lan_filter (mifi_t mifi, struct listaddr *group,
                          struct listaddr *source)
{
    char cmd[128];

    if (source->ma_filter_mode == 0)  //NON_INITIALIZED - mode change triggers iptables command;
    {
        return 1;
    } else if (source->ma_filter_mode == MODE_IS_INCLUDE)	// no iptables command;
    {
        return 1;
    } else if (source->ma_filter_mode == MODE_IS_EXCLUDE)
    {
        int rc;
        sprintf (cmd, IPT_INSERT_CMD,  // add new rule - Drop packets in exlude if appeared on LAN intweface
                 //for some unknown reason -i rndbr1 -o rndbr1 in magle table ?
                  mvifs[upStreamVif].mvif_name,
                 mvifs[mifi].mvif_name,
                 inetFmt(group->mcast_group), inetFmt(source->mcast_group));
        IF_DEBUG (DEBUG_MLD_MEMBER)
        log_msg (LOG_DEBUG, 0, "Apply iptables filter %s", cmd);
        rc = system (cmd);
        if (rc < 0)
        {
            log_msg (LOG_WARNING, errno, "Failed  %s", cmd);
        }
        return rc;
    } else
    {
        log_msg (LOG_ERR, 0,
                 "BUG, invalid filter_mode=%d of S=%s",
                 source->ma_filter_mode, inetFmt(source->mcast_group));
        return -1;
    }
}

static int
clean_iptables (mifi_t mifi, struct listaddr *group, struct listaddr *source)
{
    char cmd[128];

    if (source->ma_filter_mode == 0)  //NON_INITIALIZED - mode change triggers iptables command;
    {
        return 1;
    } else if (source->ma_filter_mode == MODE_IS_INCLUDE)	// no iptables command;
    {
        return 1;
    } else if (source->ma_filter_mode == MODE_IS_EXCLUDE)
    {
        int rc;
        sprintf (cmd, IPT_DEL_CMD,  // delete
                 //for some unknown reason -i rndbr1 -o rndbr1 in mangle table ?
                 mvifs[upStreamVif].mvif_name,
                 mvifs[mifi].mvif_name,
                 inetFmt(group->mcast_group), inetFmt(source->mcast_group));
        IF_DEBUG (DEBUG_MLD_MEMBER)
        log_msg (LOG_DEBUG, 0, "Removing iptables block %s", cmd);
        rc = system (cmd);
        if (rc < 0)
        {
            log_msg (LOG_WARNING, errno, "Failed  %s", cmd);
        }
        return rc;
    } else
    {
        log_msg (LOG_ERR, 0,
                 "BUG, invalid filter_mode=%d of S=%s",
                 source->ma_filter_mode, inetFmt(source->mcast_group));
        return -1;
    }
}
struct listaddr *
            make_new_source (mifi_t mifi, struct listaddr *group,
                             IP_ADDRESS mcast_group,
                             IP_ADDRESS required_source_address,
                             uint8_t filter_mode)
{

    register struct listaddr *source;
    register struct mvif *v = &mvifs[mifi];

    IF_DEBUG (DEBUG_MLD_MEMBER)
    log_msg (LOG_DEBUG, 0,
             "The ((G,S)=(%s,%s) doesn't exist on %s, trying to add it\n",
             inetFmt (mcast_group), inetFmt (required_source_address),
             mvifs[mifi].mvif_name);

    source = (struct listaddr *) malloc (sizeof (struct listaddr));
    if (source == NULL)
    {
        log_msg (LOG_ERR, 0, "ran out of memory");	  /* fatal */
        exit (15);
    }
    memset (source, 0, sizeof (*source));
    source->mcast_group = required_source_address; // store source address
    source->sources = group;
    source->ma_llqc = MLD6_ROBUSTNESS_VARIABLE;	  // Prepare retransmt count of Source Specific leave
    /* Set up filter mode,
     *filter mode EXCLUDE on LAN mif will apply iptables block per interface
     *filter mode EXCLUDE on WAN mifs[upStreamVif]  will compose and send MLDv2 Report upstream
     */
    switch (filter_mode)
    {
    case 0:
        break;			  // allow init state
    case MODE_IS_INCLUDE:
    case MODE_IS_EXCLUDE:
        source->ma_filter_mode = filter_mode;
        break;
    case CHANGE_TO_EXCLUDE_MODE:
        source->ma_filter_mode = MODE_IS_EXCLUDE;
        break;			  //filter mode state change on LAN  will apply iptables block
    case CHANGE_TO_INCLUDE_MODE:
        source->ma_filter_mode = MODE_IS_INCLUDE;
        break;
    default:
        log_msg (LOG_ERR, 0,
                 "***  new SSM (Group,Source) = (%s,%s) on interface %s in invalid mode %d *** : ",
                 inetFmt(group->mcast_group), inetFmt(source->mcast_group),
                 v->mvif_name, filter_mode);
    }

    if (mifi != upStreamVif)
    {
        /* Set a rxmt timer to delete the record of a source/group membership on a vif.
         * typically used when report with record type
         * -BLOCK_OLD_SOURCES,MODE_IS_INCLUDE,ALLOW_NEW_SOURCES  or  m-a-s source/group is received
         */

        /* Create timer to wait for source leave expiration */
        create_rxmt_timer (v, source, ExpireSourceRtrmtTimer);

        source->rxmt_timer_callback.source = source;
        source->rxmt_timer_callback.g = group;
        /*
         * Set a timer to non-active delete the record of a source/group membership on a vif.
         * typically used if no keep-alive message was received during MLD6_LISTENER_INTERVAL
         */

        create_report_timer (v, source, ExpireSourceTimer);	  /* Create timer to wait for source expiration */

        source->report_timer_callback.source = source;
        source->report_timer_callback.g = group;
        source->ma_CheckingListenerState = FALSE;
        /* Start timer to wait for membership expiration */
        start_report_timer (v, source, MLD6_LISTENER_INTERVAL);
        start_report_timer (v, group, MLD6_LISTENER_INTERVAL);
        source->ma_comp_mode = MLDv2;
        source->ma_comp_mode |= MCAST_SSM;

        // filter_mode=NON_INITIALIZED=0 due to memset

        //source->ma_filter_mode=MODE_IS_EXCLUDE; // XXXX LEV debug
        //make_iptables_lan_filter(mifi, group, source ); // XXXX LEV debug

        /* RFC 5790
         * It is  generally unnecessary to support the filtering function that blocks sources.
         if ( filter_mode == EXCLUDE )
         {
         start_filterMode_timer(g);
         }
         */
    }

    /* insert this new source as  fist  in s the list of the its group f this interface */

    source->ma_next = group->sources; // set  a linked list previousely head to be on my right,  next of s
    group->sources = source;  // make me (s) a head of the list
    time (&group->ma_ctime);
    IF_DEBUG (DEBUG_MLD)
    log_msg (LOG_DEBUG, 0,
             "*** Created new SSM (Group,Source) = (%s,%s) on interface %s in mode(%d) %s *** : ",
             inetFmt(group->mcast_group), inetFmt(source->mcast_group),
             v->mvif_name,
             source->ma_filter_mode,
             (source->ma_filter_mode == MODE_IS_INCLUDE) ? "INCLUDE" :
             ((source->ma_filter_mode ==
               MODE_IS_EXCLUDE) ? "EXCLUDE" : "INVALID"));
    return source;
}









/*
 * Finds if there is registered SSM with the given source, group parameters
 * Return  - source record
 */
struct listaddr *
            find_requested_mcast_transmitter (struct mvif * v,
                                              IP_ADDRESS  multicast_group,
                                              struct listaddr * wan_group,
                                              IP_ADDRESS  mcast_sender )
{
    struct listaddr *g, *s;


    /* sanity check */
    if (v == NULL ||multicast_group == 0 || wan_group == NULL || mcast_sender == 0 )
    {
        IF_DEBUG (DEBUG_MLD_MEMBER)
        log_msg (LOG_DEBUG, 0,
                 "%s  sanity check for %s on interface %p - NULL params ", __func__,
                 multicast_group,
                 v
                );
        return NULL;
    }

    if (multicast_group != wan_group->mcast_group)
    {
        IF_DEBUG (DEBUG_MLD_MEMBER)
        log_msg (LOG_DEBUG, 0,
                 "% sanity check - no Multicast %s in  (G,,%s) from interface %s\n", __func__,
                 multicast_group,
                 inetFmt(wan_group->mcast_group),
                 v->mvif_name
                );
        return NULL;		/* invalid group is given */
    }
    /*
    * group scan: if v->uv_group is given from the argument,
    * it's skipped to prevent unnecessary duplicated scanning
    */

    g = find_multicast_group (v, multicast_group);
    if (g == NULL)
    {
        return NULL;		/* multicast_group not found, sanity failed */
    }


    /* find  requred multicast source in the list of sources */

    for (s = wan_group->sources; s != NULL; s = s->ma_next)
    {
        if ( mcast_sender == s->mcast_group)  // assumed that mcast_group hold source IPV6 address,  and not multicast group
            break;
    }
    return s;		  /* multicast_group found, SSM entries searched and found */
}


int
delete_source (mifi_t mifi, struct listaddr *group, struct listaddr *source)
{
    struct mvif *v = &mvifs[mifi];
    struct listaddr *current, *next, *prev, *head_of_list;


    if (mifi != upStreamVif)
    {
        delete_report_timer (v, source);
        delete_rxmt_timer (v, source);
    }

    head_of_list = prev = next = current = group->sources;	  // list of MLDv2 multicast sources address  - each multicast group points to the  head of list
    while (current != NULL)
    {
        if (current->mcast_group == source->mcast_group ) // IP address of SOURCE is kept in mcast_group field
        {
            IF_DEBUG (DEBUG_MLD_MEMBER)
            log_msg (LOG_DEBUG, 0,
                     "deleting source (G,S)=(%s,%s) from interface %s\n",
                     inetFmt(group->mcast_group),
                     inetFmt(current->mcast_group), mvifs[mifi].mvif_name);
            if (prev == group->sources && current == group->sources)  // head of the list
            {
                IF_DEBUG (DEBUG_MLD_MEMBER)
                log_msg (LOG_DEBUG, 0,
                         "deleting source HEAD (G,S)=(%s,%s) from interface %s\n",
                         inetFmt(group->mcast_group),
                         inetFmt(current->mcast_group),
                         mvifs[mifi].mvif_name);
                clean_iptables (mifi, group, current);	  /* Remove iptables rule TODO -detect rule presence */
                group->sources = current->ma_next;	  // chain next list element to be list head
                free ((char *) current);
                break;

            } else
            {
                prev->ma_next = current->ma_next; // chain next list element instead of current
                next = prev->ma_next;
                if (next)
                {
                    IF_DEBUG (DEBUG_MLD_MEMBER)
                    log_msg (LOG_DEBUG, 0,
                             "chaining source (PREV,NEXT)=(%s,%s) from interface %s\n",
                             inetFmt(prev->mcast_group),
                             inetFmt(next->mcast_group),
                             mvifs[mifi].mvif_name);
                }
                clean_iptables (mifi, group, current);	  /* Remove iptables rule TODO -detect rule presence */
                free ((char *) current);
                break;
            }
        } else
        {
            prev = current;
            current = next = current->ma_next;	  // take the next list element
        }
    }
    if (mifi != upStreamVif)
    {

        delete_source_upstream (mifi, group->mcast_group, source->mcast_group); //  Synchronize with Wan interface
    }

    return 1;
}

/*
  delete_group_sources() - Delete all sources in LAN interface group
*/
int
delete_group_sources (mifi_t mifi, struct listaddr *group)
{
    register struct listaddr *source = group->sources;
    register struct listaddr *next = source;
    int count = 0;
    while (next)
    {
        next = source->ma_next;
        delete_source (mifi, group, source);
        source = next;
        count++;
    }
    group->sources = NULL;
    group->ma_comp_mode &= (~MCAST_SSM);
    return count;
}



void
start_all_sources_timers (struct mvif *v, struct listaddr *group, u_int16 timeout)
{
    struct listaddr *prev = group->sources;
    struct listaddr *current = prev;


    while (current)
    {
        IF_DEBUG(DEBUG_TIMER)
        log_msg (LOG_DEBUG, 0,
                 "%s: source S=%s mifi=%s ", __func__,
                 sa6_fmt(current->mcast_group), v->mvif_name) ;
        start_report_timer (v, current, timeout);
        current = prev->ma_next;
        prev=current;
    }
}

void
stop_all_sources_timers (struct mvif *v, struct listaddr *group)
{
    register struct listaddr *source = group->sources;
    register struct listaddr *next = source;


    while (next)
    {
        next = source->ma_next;
        stop_report_timer (v, source);
        source = next;
    }
}

/*
  delete_group_sources_upstream() - Try to synchronize group delete  with source status in WAN interface group
		  try to delete because
		  it may have subscribers on other LAN interfaces
  Returns - Number of sunscribers after
*/
int
delete_group_sources_upstream (mifi_t lan_mifi, struct listaddr *lan_group)
{
    register struct listaddr *source;
    register struct listaddr *next;
    int count_active = 0;
    int count_errors = 0;
    int rc = 0;

    next = source = lan_group->sources;
    while (next)
    {
        next = source->ma_next;

        if (delete_source_upstream (lan_mifi, lan_group->mcast_group,	  /* try to delete single source */
                                    source->mcast_group) <= 0)
        {			/* source  still active on some LAN interface */
            count_active++;
        }
        source = next;
    }

    return count_active;
}

/*
  delete__source_upstream() - Try to synchronize source delete with source status in WAN interface group
		  try to delete because
		  it may have subscribers on other LAN interfaces
  Return value 1 -source deleted
			   0 other subscribers exists
*/

int
delete_source_upstream (mifi_t lan_mifi, IP_ADDRESS mcast_group,
                        IP_ADDRESS source_address)
{
    int rc = 0;
    register struct mvif *v = &mvifs[upStreamVif];
    short intfce = mvifs[lan_mifi].uv_ifindex;
    struct listaddr *source, *group;
    if_set zero;

    // Sanity
    if (source_address == 0 || mcast_group == 0 )
    {
        log_msg (LOG_ERR, 0,
                 "%s: BUG parameters NULL mcast_group/ source lan_mifi=%d ", __func__,
                 lan_mifi) ;
        return -1;
    }


    IF_ZERO (&zero);


    IF_DEBUG (DEBUG_MLD_MEMBER)
    log_msg (LOG_DEBUG, 0,
             "delete_source_upstream (G,S)=%s,%s on %s",
             inetFmt (mcast_group), inetFmt (source_address), v->mvif_name);
    /* find the group in upstream DB */
    group = find_multicast_group (v, mcast_group);
    /* find the source record in upstream DB */
    

	k_del_from_mfc( source_address,	  /* sender of multicast */
                    mcast_group,	  /* multicast address - the destination */
                    lan_mifi,	  /* from where it will come - upstream virtual vif */
                    &group->ma_downstream_ifset	  /*set of DS intefaces to send mcasts to */
                  );
    source = find_requested_mcast_transmitter (v, group->mcast_group, group, source_address);
    if ( source == NULL )
    {
		return;
    }
    k_del_from_mfc( source_address,	  /* sender of multicast */
                    mcast_group,	  /* multicast address - the destination */
                    lan_mifi,	  /* from where it will come - upstream virtual vif */
                    &source->ma_downstream_ifset	  /*set of DS intefaces to send mcasts to */
                  );
    if (IF_ISSET(lan_mifi, &(source->ma_downstream_ifset) ) )
    {
        IF_CLR (lan_mifi, &(source->ma_downstream_ifset));
        IF_DEBUG (DEBUG_MFC)
        log_msg (LOG_DEBUG, 0,
                 "%s : Cleared bitmask for lan interface %s of source S=%s\n",
                 __func__, mvifs[lan_mifi].mvif_name,
                 inetFmt(source->mcast_group));
    }
#ifdef k_add_to_mfc_instead_delete_modifies_MFC
    if (memcmp (&source->ma_downstream_ifset, &zero, sizeof (if_set)) != 0)
    {
        IF_DEBUG (DEBUG_MFC)
        log_msg (LOG_DEBUG, 0,
                 "Removing routing for interface %s from MFC,  keep other LANs subscribers for source %s in MFC",
                 mvifs[lan_mifi].mvif_name, inetFmt(source->mcast_group));

        k_del_from_mfc( source_address,	  /* sender of multicast */
                        mcast_group,	  /* multicast address - the destination */
                        lan_mifi,	  /* from where it will come - upstream virtual vif */
                        &source->ma_downstream_ifset	  /*set of DS intefaces to send mcasts to */
                      );


        k_add_to_mfc( source_address,	  /* sender of multicast */
                      mcast_group,	  /* multicast address - the destination */
                      upStreamVif,	  /* from where it will come - upstream virtual vif */
                      &source->ma_downstream_ifset	  /*set of DS intefaces to send mcasts to */
                    );
        // Remove LAN interface for this S from downstream set bitmask
        IF_CLR (lan_mifi, &(source->ma_downstream_ifset));
        IF_DEBUG (DEBUG_MFC)
        log_msg (LOG_DEBUG, 0,
                 "%s : Cleared bitmask for lan interface %s of source S=%s\n",
                 __func__, mvifs[lan_mifi].mvif_name,
                 inetFmt(source->mcast_group));
        return 0;
    } else
#endif
        if (memcmp (&source->ma_downstream_ifset, &zero, sizeof (if_set)) == 0)
        {				// No more subscribers for this source,  we've just deleted S of the last LAN client
            // After del_from_mfc6() MFC cache entry becames invalid and is kept for another 5 sec before it is cleared
            // kernel does not keep reference count for mfc cache, several ADD_MFCs may be cleared with one DEL_MFC
            IF_DEBUG (DEBUG_MFC)
            log_msg (LOG_DEBUG, 0,
                     "Removing interface %s from MFC routing cache for source %s",
                     mvifs[lan_mifi].mvif_name, inetFmt(source->mcast_group));
            k_del_from_mfc( source_address, mcast_group, lan_mifi, &source->ma_downstream_ifset);

            if (source->ma_filter_mode == MODE_IS_INCLUDE)
            {
                IF_DEBUG (DEBUG_IF)
                log_msg (LOG_DEBUG, 0,
                         " k_leave_src (G,S)=%s,%s on %s",
                         inetFmt (mcast_group),
                         inetFmt (source_address), v->mvif_name);

                k_leave_src (mld6_proxy_socket, group->mcast_group ,
                             source->mcast_group , upstream_idx);
            }

            else if (source->ma_filter_mode == MODE_IS_EXCLUDE)
            {
                IF_DEBUG (DEBUG_IF)
                log_msg (LOG_DEBUG, 0,
                         " k_unblock_src (G,S)=%s,%s on %s",
                         inetFmt (mcast_group),
                         inetFmt (source_address), v->mvif_name);

                k_unblock_src (mld6_proxy_socket, group->mcast_group ,
                               source->mcast_group , upstream_idx);
            } else
            {
                log_msg (LOG_WARNING, 0,
                         "BUG:invalid  Source FILTER_MODE %d on upstream intfce for S=%s",
                         source->ma_filter_mode, inetFmt(source->mcast_group));
            }

            delete_source (upStreamVif, group, source);	  /* finally remove S from WAN group status record */

            return 1;		  /* source succesfully removed */
        }
    return 0;
}


int
set_sources_mode (mifi_t mifi, struct listaddr *group, uint16_t numsrc,
                  LIST_OF_SOURCES_IN_REPORT srcs, uint8_t v2_report_record_type)
{
    int j;
    register struct mvif *v = &mvifs[mifi];
    uint8_t filter_mode;
    IP_ADDRESS source_sa;

    switch (v2_report_record_type)
    {
    case CHANGE_TO_INCLUDE_MODE:
    case MODE_IS_INCLUDE:
    case ALLOW_NEW_SOURCES:
        filter_mode = MODE_IS_INCLUDE;
        break;
    case CHANGE_TO_EXCLUDE_MODE:
    case MODE_IS_EXCLUDE:
        filter_mode = MODE_IS_EXCLUDE;
        break;
    default:
    {
        log_msg (LOG_ERR, 0,
                 "BUG, invalid report_record_type=%d of S=%s",
                 v2_report_record_type, inetFmt(group->mcast_group));
        return -1;
    }
    }

    for (j = 0; j < numsrc; j++)
    {
        source_sa= *((*srcs)+j);
        struct listaddr *s_lan, *s_wan, *g_wan;

        IF_DEBUG (DEBUG_MLD_PROTO)
        log_msg (LOG_DEBUG, 0,
                 "Processing source S=%s in group G=%s  on lan intfce %s ",
                 inetFmt(source_sa),
                 inetFmt(group->mcast_group), v->mvif_name);
        /* ARRIS MOD START */
        /****************************************************************************
           Entry of requested multicast group on upstream interface(erouter0) might 
           not exist at the time, move following code after requested multicast
           group has been merged with upstream interface(erouter0).
        *****************************************************************************/
#if 0
        g_wan = find_multicast_group (&mvifs[upStreamVif], group->mcast_group);
        if (g_wan == NULL)
        {
            log_msg (LOG_ERR, 0,
                     "BUG:Can't find a group  on upstream intfce for S=%s",
                     inetFmt(source_sa));
            return -1;
        }
#endif
        /* ARRIS MOD END */
        s_lan = find_requested_mcast_transmitter (v, group->mcast_group,
                group, source_sa);
        if (s_lan == NULL)
        {
            if(v2_report_record_type == ALLOW_NEW_SOURCES)
            {
                if ((group->ma_filter_mode == MODE_IS_EXCLUDE))
                    continue;
            }
            
            s_lan = make_new_source (mifi, group, group->mcast_group, source_sa, 0);	  // create init state for filter_mode
            // XXX set_filter_mode(mifi, group,filter_mode);
            // XXX sync_source_upstream (mifi, group->mcast_group, MLDv2,source_sa, filter_mode);
            mld_merge_with_upstream (mifi, group->mcast_group, MLDv2,
                                     source_sa, filter_mode);
        } else
        {
            /*we received a report for this source so stop rxmt timer and mark listener state false*/
            stop_rxmt_timer (v, s_lan);
            s_lan->ma_CheckingListenerState = FALSE;
            start_report_timer (v, s_lan, MLD6_LISTENER_INTERVAL);
        }

        /* ARRIS MOD START */
        /**************************************************************************
           The checking of requested multicast group entry on upstream interface is
           moved here to make sure it won't break normal IGMPv3.
        ***************************************************************************/
        g_wan = find_multicast_group (&mvifs[upStreamVif], group->mcast_group);
        if (g_wan == NULL)
        {
            log_msg (LOG_ERR, 0,
                     "BUG:Can't find a group  on upstream intfce for S=%s",
                     inetFmt(source_sa));
            return -1;
        }
        /* ARRIS MOD END */

		if (g_wan->ma_comp_mode == MLDv2)
		{
        s_wan =
            find_requested_mcast_transmitter (&mvifs[upStreamVif],
                                              group->mcast_group, g_wan,
                                              source_sa);
        if (s_wan == NULL)
        {
            log_msg (LOG_ERR, 0,
                     "%s :BUG:Can't find a source S=%s on upstream intfce in group G=%s", __func__,
                     inetFmt(source_sa), inetFmt(g_wan->mcast_group));
            return -1;
        }
		}
        // todo :on state swicck lan include req exlude, put state to 0 and ask CPE to  report
        // start rxmt, callback - if on expire  any CPE reports in include, stay in include
        switch (filter_mode)
        {
        case CHANGE_TO_INCLUDE_MODE:
        case MODE_IS_INCLUDE:
        {
            switch (s_lan->ma_filter_mode)
            {
            case MODE_IS_INCLUDE:
                break;		// nothing to do
            case 0:
                //even if one CPE will ask EXCLDE, it will remain  IN INCLUDE
                s_lan->ma_filter_mode = MODE_IS_INCLUDE;
                break;
            case MODE_IS_EXCLUDE:
                // state change
                start_rxmt_timer (v, s_lan, v->uv_mld_llqi);	//deletes source on llqc=0
                s_lan->ma_filter_mode = MODE_IS_INCLUDE;	//include event always override exclude
                clean_iptables (mifi, group, s_lan);
                if (s_wan->ma_filter_mode == MODE_IS_EXCLUDE)
                {
                    /* WAN state change */
                     s_wan->ma_filter_mode = MODE_IS_INCLUDE;
                     mld_merge_with_upstream (mifi, group->mcast_group, MLDv2,
                                     source_sa, filter_mode);
                  //  k_unblock_src(mld6_proxy_socket,
                            //      g_wan->mcast_group ,
                            //      s_lan->mcast_group ,
                           //       upstream_idx);
                   
                
                }

                break;
            default:
            {
                log_msg (LOG_ERR, 0,
                         "BUG, invalid filter_mode=%d of S=%s",
                         s_lan->ma_filter_mode,
                         inetFmt(s_lan->mcast_group));
                return -1;
            }
            }
            break;
        }
        case CHANGE_TO_EXCLUDE_MODE:
        case MODE_IS_EXCLUDE:
        {
            switch (s_lan->ma_filter_mode)
            {
            case MODE_IS_EXCLUDE:
				if (g_wan->ma_comp_mode != MLDv2) 
				{ /*means group in upstream in exclude mode*/
					make_iptables_lan_filter (mifi, group, s_lan);
				}
				
                break;		// nothing to do
            case MODE_IS_INCLUDE:
                // one CPE asked TO_EXCLUDE, but it
                break;		// can not override others CPE who asked INCLUDE
            case 0:
                s_lan->ma_filter_mode = MODE_IS_EXCLUDE;
				if (g_wan->ma_comp_mode != MLDv2) 
				{ /*means group in upstream in exclude mode*/
					make_iptables_lan_filter (mifi, group, s_lan);
					 break;
				}
                if (s_wan->ma_filter_mode == MODE_IS_INCLUDE)
                {
                    //if wan is in EXCL MFC cache is negative, i.e
                    // trafic is not forwarded to LAN
                    make_iptables_lan_filter (mifi, group, s_lan);
                } else if (s_wan->ma_filter_mode == 0)
                {
                    // Following must happen in mld_merge_with_upstream()
                    // TODO compose and send MLD6v2 MODE_IS_EXCLUDE report
                    // TODO  make negative MFC cache
                    s_wan->ma_filter_mode = MODE_IS_EXCLUDE;
                }
                break;
            default:
            {

                log_msg (LOG_ERR, 0,
                         "BUG, invalid filter_mode=%d of S=%s",
                         s_lan->ma_filter_mode,
                         inetFmt(s_lan->mcast_group));
                return -1;
            }
            }
            break;
        }
        default:
        {
            log_msg (LOG_ERR, 0,
                     "BUG, invalid filter_mode=%d of S=%s",
                     s_lan->ma_filter_mode, inetFmt(s_lan->mcast_group));
            return -1;
        }

        }
    }
    return 1;
}
