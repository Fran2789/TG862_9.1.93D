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
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "vif.h"
#include "groups.h"
#include "mcast_proto.h"

#include "inet6.h"
#include "kern.h"
#include "timers.h"
#include "groups.h"



/* Make new group as responce to MLD report message */
struct listaddr *
            make_new_group (mifi_t mifi, IP_ADDRESS group_multicast_address, u_int8_t mld_version)
{

    struct listaddr *g;
    struct mvif *v = &mvifs[mifi];

    if ((!(v && group_multicast_address)))
    {
        log_msg (LOG_ERR, 0, "%s sanity failure, null in params", __func__);
    }
    IF_DEBUG (DEBUG_MLD)
    log_msg (LOG_DEBUG, 0,
             "The group %s is new, trying to add it to interface %s\n",
             inetFmt (group_multicast_address), v->mvif_name);

    g = (struct listaddr *) malloc (sizeof (struct listaddr));
    if (g == NULL)
    {
        log_msg (LOG_ERR, 0, "ran out of memory");	/* fatal */
        exit (15);
    }
    memset (g, 0, sizeof (*g));
    g->mcast_group=group_multicast_address;

    g->ma_comp_mode = mld_version;  /* mldv1 or mldv2 with no sources */


    if (g->ma_comp_mode & MLDv2)
    {
        g->ma_filter_mode = MODE_IS_EXCLUDE;	// IF NO sources for group in MLDv2 -  To_EX
    }

    if (mifi != upStreamVif)
    {
        create_rxmt_timer (v, g, ExpireRtrmtTimer);	/* Create timer to wait for group leave expiration */

        create_report_timer (v, g, ExpireReportTimer);	/* Create timer to wait for membership expiration */

        /* Start timer to wait for membership expiration */
        start_report_timer (v, g, MLD6_LISTENER_INTERVAL);

        create_back2Igmpv3_timer (v, g, IgmpV2_TimerExpire_and_back_to_Higher_Igmp); /* Create timer to return  to IGMPv3 */
        create_back2Igmpv2_timer (v, g, IgmpV1_TimerExpire_and_back_to_Higher_Igmp);

#ifdef IMPLEMENTED_FILTER_MODE_TIMER /* we decided do not implemented sitch fro include to exclude */
        create_filterMode_timer(g); //* needed only in exclude mode */
#endif


    }
    /* insert group fist  in the list of the groups of this interface */
    g->ma_next = v->uv_groups;
    v->uv_groups = g;
    time (&g->ma_ctime);
    return g;
}


static void delete_group_timers(struct mvif *v ,struct listaddr *group);
static void delete_group_timers(struct mvif *v ,struct listaddr *group)
{
    delete_report_timer(v, group);
    delete_rxmt_timer(v, group);
    delete_back2Igmpv2_timer(v,group);
    delete_back2Igmpv3_timer(v,group);
#ifdef IMPLEMENTED_FILTER_MODE_TIMER
    delete_filterMode_timer(v,group);
#endif
}
void
delete_group (mifi_t mifi, struct listaddr *group)
{
    struct listaddr *current, *prev, *next, *head_of_list;
    struct mvif *v = &mvifs[mifi];

    /*
     * Sanity
     */
    if ( mifi > MAXMIFS || group == NULL)
    {
        log_msg (LOG_ALERT, 0, "BUG in delete_group , invalid/ null group parameters ");
        return;
    }
    IF_DEBUG (DEBUG_MLD_MEMBER)
    log_msg (LOG_DEBUG, 0,
             "Entering delete_group() G=%s for interface %s\n",
             inetFmt (group->mcast_group), mvifs[mifi].mvif_name);

    if (mifi != upStreamVif)
    {
        delete_group_timers (v, group);

    }

    head_of_list = current = prev = next = mvifs[mifi].uv_groups;	/* list of multicast groups/addresses  - interface stores head of list */
    while (current != NULL)
    {
        if (current == group)	/* we are looking for a group by its memory addresses */
        {
            /* Sanity  */
            if (current->mcast_group != group->mcast_group)	/* IPV6 multicast address in mcast_group field must match too*/
            {
                log_msg (LOG_ALERT, 0,
                         "BUG delete_group():Group MCAST addresss G=%s do not match stored address=%s from interface %s",
                         inetFmt (current->mcast_group),
                         inetFmt (group->mcast_group), v->mvif_name);
            }
            IF_DEBUG (DEBUG_MLD_MEMBER)
            log_msg (LOG_DEBUG, 0,
                     "delete_group() found group G=%s  in the  interface %s group list\n",
                     inetFmt (current->mcast_group), v->mvif_name);

            if (prev == head_of_list && current == head_of_list)	  /*head of the list */
            {


                IF_DEBUG (DEBUG_MLD_MEMBER)
                log_msg (LOG_DEBUG, 0,
                         "delete_group() removing  HEAD of the list group G=%s  from  interface %s  %p\n",
                         inetFmt (current->mcast_group),
                         mvifs[mifi].mvif_name, mvifs[mifi].uv_groups);
                mvifs[mifi].uv_groups = current->ma_next;	/* make next element to be head of the list instead of current */
                if (current->sources)
                    delete_group_sources (mifi, group);

                free ((char *) current);
		v->uv_older_host_present=v->uv_mld_version; /* Update IGMP compatibility status  of interface */
                break;

            }
            else
            {
                prev->ma_next = current->ma_next;	/* chain next list element instead of current  */

                IF_DEBUG (DEBUG_MLD_MEMBER)
                log_msg (LOG_DEBUG, 0,
                         "delete_group() removing group G=%s  from  interface %s group list %p, p=%p p->next=%p\n",
                         inetFmt (current->mcast_group),
                         mvifs[mifi].mvif_name, mvifs[mifi].uv_groups, prev,
                         prev->ma_next);
            }
            if (current->sources)
            {
                IF_DEBUG (DEBUG_MLD_MEMBER)
                log_msg (LOG_DEBUG, 0,
                         "delete_group() found group G,S=%s  in the  interface %s group list, removing\n",
                         inetFmt (current->mcast_group),
                         inetFmt (current->sources->mcast_group),
                         mvifs[mifi].mvif_name);
                delete_group_sources (mifi, group);
            }

            free ((char *) current);
	    {  /* Update IGMP compatibility status  of interface */
                    struct listaddr *g;

                    v->uv_older_host_present=0;
                    /* Look for the group in our listener list. */
                    for (g = v->uv_groups; g != NULL; g = g->ma_next)
                    {

                        v->uv_older_host_present |= g->ma_comp_mode;

                    }
		   if (v->uv_older_host_present == 0 ) 
		       v->uv_older_host_present=v->uv_mld_version;
                }
	    
            break;
        }
        else
        {
            prev = current;

            current = next = current->ma_next;
        }
    } /* end while */


    if (mifi != upStreamVif)
        delete_group_upstream (mifi, group->mcast_group);	/* Synchronize with Proxy interface */
}

/*
 delete_group_upstream - removes group from  WAN database
  * It is not obliged to delete the whole group, it might be called to update the status of Upstream/WAN interface source list
*/
void
delete_group_upstream (mifi_t mifi,
                       uint32_t group_multicast_address)
{
    short intfce = mvifs[mifi].uv_ifindex;
    register struct mvif *v = &mvifs[upStreamVif];
    register struct listaddr *wan_group;
    int rc=0;

    if_set zero;


    IF_ZERO (&zero);

    IF_DEBUG (DEBUG_MLD_MEMBER)
    log_msg (LOG_DEBUG, 0,
             "Entering %s G=%s", __func__,
             inetFmt (group_multicast_address));

    wan_group = find_multicast_group (v, group_multicast_address);

    if (wan_group == NULL)
    {
        IF_DEBUG (DEBUG_MLD_MEMBER)
        log_msg (LOG_DEBUG, 0,
                 "%s: BUG -Cannot find multicast group  G=%s  on  interface %s, wan group not exist", __func__,
                 inetFmt (group_multicast_address), v->mvif_name);;
        return;
    }

    if (wan_group->sources)
    {
        struct listaddr * listsrc;
        rc = delete_group_sources_upstream (mifi, wan_group);	/* Delete all remaining (S,)  of group G */

        if (rc >0 )  /* there are still active Sources,  */
        {
            if (wan_group->sources == NULL )
                IF_DEBUG (DEBUG_MLD_MEMBER)
            {
                log_msg (LOG_ERR, 0,
                         "BUG group  G=%s still contains N=%d active  SSM, but sources list is empty",
                         inetFmt (wan_group->mcast_group),rc);
            }
            IF_DEBUG (DEBUG_MLD_MEMBER)
            log_msg (LOG_DEBUG, 0,
                     "upstream group  G=%s still contains N=%d active  SSM",
                     inetFmt (wan_group->mcast_group), rc);

            IF_DEBUG (DEBUG_MFC)
            log_msg (LOG_DEBUG, 0,
                     "Cleared MFC cache entry delete_group_upstream G=%s\n",
                     inetFmt (wan_group->mcast_group));
            listsrc =wan_group->sources;
            while (listsrc)
            {
                /* Delete mcast entries for SSM in kernel MFC cachein order to reinstall on cache miss  */
                k_del_from_mfc( listsrc->mcast_group, /* sender of multicast*/
                                wan_group->mcast_group,  /* multicast address - the destination */
                                mifi,
                                &(listsrc->ma_downstream_ifset) /*set of the DS interfaces to route to */
                              );

                if (listsrc->ma_next == NULL)
                    break;
                listsrc=listsrc->ma_next;
            }
        }
        else
        {
            wan_group->ma_comp_mode = IGMPv3; /* All sources deleted, NO SSM in group */
        }
    }
    /* Delete GROUP and Sources from MFC cache in order to reinstall on cache miss */
    k_del_from_mfc ( wan_group->transmitter, wan_group->mcast_group,
                     mifi,  &wan_group->ma_downstream_ifset); /* Delete mcast entries in kernel MFC cache */
    IF_DEBUG (DEBUG_MFC)
    log_msg (LOG_DEBUG, 0,
             "Cleared MFC cache entry delete_group_upstream G=%s\n",
             inetFmt (wan_group->mcast_group));

    IF_DEBUG (DEBUG_MFC)
    log_msg (LOG_DEBUG, 0,
             "%s:Clearing bitmask for %s   G=%s", __func__,
             mvifs[mifi].mvif_name, inetFmt (wan_group->mcast_group));
    IF_CLR (mifi, &(wan_group->ma_downstream_ifset));


    if (memcmp (&wan_group->ma_downstream_ifset, &zero, sizeof (if_set)) == 0)
    {
        /* No more subscribers */
        IF_DEBUG (DEBUG_MLD_MEMBER)
        log_msg (LOG_DEBUG, 0,
                 "No more subscribers for G=%s on LAN interfaces ",
                 inetFmt (wan_group->mcast_group));

        k_leave (IgmpProxySocket , wan_group->mcast_group, mvifs[upStreamVif].uv_ifindex);	/* Send DONE|Leave  MLD message */

        delete_group (upStreamVif, wan_group);

        /* Delete GROUP from MFC cache */
        /* TODO -check the case ASM group with several transmitters  */
    }

}


int
mld_merge_with_upstream (mifi_t mifi,
                         uint32_t  mcast_group_address,
                         uint8_t mld_version, uint32_t source,
                         uint8_t source_filter_mode)
{
    struct mvif *v = &mvifs[upStreamVif];
    struct listaddr *g_wan ;
    u_int8_t new_wan_group = 0;

    /*
     * Look for the group in the interface's  group list; if found,
     *  .
     */
    // ARRIS ADD START
	// NO need to merge this group with upstream
    if(strcmp(inetFmt (mcast_group_address),"239.255.255.250")==0)
    {
		
        return;
    }
    // ARRIS ADD END
    g_wan = find_multicast_group (v, mcast_group_address);

    if (g_wan == NULL)
    {

        IF_DEBUG (DEBUG_IF)
        log_msg (LOG_DEBUG, 0,
                 "The group %s doesn't exist on Upstream interface %s, trying to add it\n",
                 inetFmt (mcast_group_address), v->mvif_name);

        g_wan = make_new_group (upStreamVif, mcast_group_address, mld_version);	  /* Create a group */
        if (g_wan == NULL)
        {
            log_msg (LOG_ERR, 0,
                     "make_new_group G=%s on Upstream interface %s  failed\n",
                     inetFmt (mcast_group_address), v->mvif_name);
            return;
        }
        

               new_wan_group = 1;



        g_wan->ma_comp_mode = mld_version; /* set mode  */
		g_wan->ma_filter_mode = MODE_IS_EXCLUDE;
        /* Add SSM flag */

        if (source != 0 && mld_version == MLDv2)
        {
            g_wan ->ma_filter_mode = source_filter_mode;
            
            /* ARRIS MOD START */
            /*********************************************************************
               Code inspection shows that MCAST_SSM saved in g_wan->ma_comp_mode 
               hasn't been ever used for upstream interface(erouter0),
               but there are many "if (g_wan->ma_comp_mode == MLDv1/2)" checkings
               which break IGMPv3.
               So remove following statement to make sure IGMP proxy works fine.
             *********************************************************************/
            //g_wan->ma_comp_mode = MCAST_SSM;
            /* ARRIS MOD END */
        }
    }


    if ( source == 0 || g_wan->ma_comp_mode == IGMPv2 || (g_wan->ma_comp_mode == IGMPv1) )   /* No source, we are at ASM mode */
    {

        if (g_wan->ma_comp_mode > mld_version)
        {
            g_wan->ma_comp_mode = mld_version;
        }

        if (new_wan_group ) /* new_group on lan interface and new_group on WAN */
        {
            /* Make a proxy action - join the group */
            k_join (IgmpProxySocket , mcast_group_address, upstream_idx); /*will exit on error */
            IF_DEBUG (DEBUG_IF)
            log_msg (LOG_DEBUG, 0,
                     " (G,*) join request sent = (%s ) on physical interface#%d %s\n",
                     inetFmt (mcast_group_address), upstream_idx,
                     v->mvif_name);
        }

        /* Prepare set of downstream interfaces when the cache miss will occur

        *  the closed multicast session leaves mfc cache in this  invalid state for around ~4 sec time, then  the entry is finally removed
        # cat /proc/net/ip6_mr_cache
        Group                            Origin                           Iif      Pkts  Bytes     Wrong  Oifs
        ff1e:0000:0000:0000:0000:0000:0000:0100 fe80:0000:0000:0000:0250:f1ff:fe08:b663 -1         0        0        0




        /*
                * In order to extend MFC record, we actually delete it in a hope that
                * CACHE_MISS will reinstall extended MFC  record
                */
        if ( g_wan->transmitter )
        {
            k_del_from_mfc (g_wan->transmitter, /* sender of multicast */
                            mcast_group_address,	  /* multicast address - the destination */
                            upStreamVif,	  /* from where it will come - upstream virtual vif */
                            &g_wan->ma_downstream_ifset /*set of DS intefaces to send mcasts to */
                           );
            IF_DEBUG (DEBUG_MFC)
            log_msg (LOG_DEBUG, 0,
                     "%s MFC cache of G,S= (%s,%s)  have been purgued", __func__,
                     inetFmt (mcast_group_address), inetFmt (source));
        }

        if  (! IF_ISSET(mvifs[mifi].uv_ifindex,  &(g_wan->ma_downstream_ifset)) )
        {
            IF_SET (mifi, &(g_wan->ma_downstream_ifset));
            IF_DEBUG (DEBUG_MFC)
            log_msg (LOG_DEBUG, 0,
                     "%s interface %s  added to downstream ifset of (G,*)= %s", __func__,
                     mvifs[mifi].mvif_name, inetFmt (mcast_group_address));
        }
        return; /* We had finished syncing this group with WAN inteface in ASM mode */
    }
    if (source != 0  ) /* i.e IGMPV3 */
    {
        struct listaddr *s ;
        /*
         * Find whether multicast source address is already registered on upstream interface
         * */

        s = find_requested_mcast_transmitter (&mvifs[upStreamVif],
                                              mcast_group_address, g_wan, source);
        /* fill to success in delete group MFC  in SSM mod */
        if (g_wan->transmitter == 0 )
            g_wan->transmitter = source;


        if (s != NULL)
            /* needed  source merge routing to other LAN DS interfaces*/
        {
            if ( IF_ISSET (mifi, &s->ma_downstream_ifset) )
            {

                IF_DEBUG (DEBUG_IF)
                log_msg (LOG_DEBUG, 0,
                         " (G,S) = (%s ,%s) is already present in upstream interface database",
                         inetFmt (mcast_group_address), inetFmt (source));
                /* may reqiuere exclude fi (G,S) return;			/* no merge required, source is present in DS SET */
            }
            /* IF_SET (mifi, &g_wan->ma_downstream_ifset); */ /* Keep record of subscribed LANS to group G */
            IF_SET (mifi, &s->ma_downstream_ifset);	  /* Keep record of subscribed LANS to source S */
            IF_DEBUG (DEBUG_IF)
            log_msg (LOG_DEBUG, 0,
                     "SSM routing  to LAN interface %s added  for G,S= (%s ,%s) to Downstream bitmap",
                     mvifs[mifi].mvif_name,
                     inetFmt (mcast_group_address), inetFmt (source)
                    );
            /* TODO  or handle mode change - see set_source_mode */
            if (s->ma_filter_mode == MODE_IS_INCLUDE && g_wan ->ma_comp_mode == MLDv2)
            {
                k_add_to_mfc (
                    source,	/* sender of multicast */
                    mcast_group_address,	/* multicast address - the destination */
                    upStreamVif,	/* from where it will come - upstream virtual vif */
                    &s->ma_downstream_ifset	/*set of DS intefaces to send mcasts to */
                );
                IF_DEBUG (DEBUG_MFC)
                log_msg (LOG_DEBUG, 0,
                         "SSM routing  to LAN interface %s added  for G,S= (%s ,%s) to MFC cache",
                         v->mvif_name,
                         inetFmt (mcast_group_address), inetFmt (source)
                        );
            } else if (s->ma_filter_mode == MODE_IS_EXCLUDE && g_wan ->ma_comp_mode == MLDv2)
            {  /* MODE_IS_EXCLUDE -negative  . TODO - test MIX of negative and positibe */
                IF_DEBUG (DEBUG_MFC)
                log_msg (LOG_DEBUG, 0,
                         "SSM Making negativ routing  to LAN interface %s added  for G,S= (%s ,%s) to MFC cache",
                         v->mvif_name,
                         inetFmt (mcast_group_address), inetFmt (source)
                        );
                k_make_negative_mfc ( source, mcast_group_address,
                                      mifi, &s->ma_downstream_ifset);  /* creates  negative mfc cache - it works, i.e traffic is stopped */
            }
            return;		   /* no merge required, source is present in upStreamVif */
        } else
        {
            s = make_new_source (upStreamVif, g_wan, mcast_group_address, source, source_filter_mode);
            if (s == NULL)
            {
                log_msg (LOG_ERR, 0,
                         "make_new_(group, source )= (%s,%s) on Upstream interface %s  failed\n",
                         inetFmt (mcast_group_address), inetFmt (source),
                         v->mvif_name);
                return;
            }
        }
        if (s != NULL)
        {
            /* Only new source going to proxy join */

           
       
           

            IF_DEBUG (DEBUG_IF)
            log_msg (LOG_DEBUG, 0,
                     "SSM  (G,S) join request sent = (%s ,%s) on interface  %s\n",
                     inetFmt (mcast_group_address), inetFmt (source),
                     v->mvif_name);
            /*
             * we can create mfc cache entry in advance since we now the required source address,
             * i.e from where mcast will come
             */
            if (s->ma_filter_mode == MODE_IS_INCLUDE)
            {
                if (g_wan ->ma_filter_mode == MODE_IS_INCLUDE && g_wan ->ma_comp_mode == MLDv2)
                {
                    k_join_src (IgmpProxySocket , mcast_group_address, source, upstream_idx); 
                }
                if (g_wan ->ma_filter_mode == MODE_IS_EXCLUDE && g_wan ->ma_comp_mode == MLDv2)
                {
                    k_unblock_src (IgmpProxySocket , mcast_group_address, source, upstream_idx); 
                }
                IF_SET (mifi, &g_wan->ma_downstream_ifset);	  /* Keep record of subscribed LANS to group G */
                IF_DEBUG (DEBUG_MFC)
                log_msg (LOG_DEBUG, 0,
                         "%s interface %s  added to downstream ifset of (G)= %s", __func__,
                         mvifs[mifi].mvif_name, inetFmt (mcast_group_address));

                IF_SET (mifi, &s->ma_downstream_ifset);	  /* Keep record of subscribed LANS to source S */
                IF_DEBUG (DEBUG_MFC)
                log_msg (LOG_DEBUG, 0,
                         "%s interface %s  added to downstream ifset of (S)= %s", __func__,
                         mvifs[mifi].mvif_name, inetFmt (source));

                IF_DEBUG (DEBUG_IF)
                log_msg (LOG_DEBUG, 0,
                         "SSM  (G,S) = (%s ,%s) IN MODE_IS_INCLUDE on interface  %s\n",
                         inetFmt (mcast_group_address), inetFmt (source),
                         v->mvif_name);
                k_add_to_mfc(  source, /* sender of multicast */
                               mcast_group_address,	  /* multicast address - the destination */
                               upStreamVif,	  /* from where it will come - upstream virtual vif */
                               &s->ma_downstream_ifset	  /*set of DS intefaces to send mcasts to */
                            );
            } else if (s->ma_filter_mode == MODE_IS_EXCLUDE )
            {
                /* else -> s->ma_filter_mode == MODE_IS_EXCLUDE */
                /*
                 * create negative cache entry for the given source
                 *  by setting   oif=NULL or ttl=0 or ttl=255, IPV6- null if_set
                 */
                IF_DEBUG (DEBUG_MFC)
                log_msg (LOG_DEBUG, 0,
                         "SSM Adding negativ routing to LAN interface %s for (G,S) = (%s ,%s) to MFC cache IN MODE_IS_EXCLUDE",
                         v->mvif_name,
                         inetFmt (mcast_group_address), inetFmt (source)
                        );
                /*
                 *  make kernel to produce MLD report and pass exclude to CMTS
                 */
                if (new_wan_group == 1)
                {
                    k_join (IgmpProxySocket , mcast_group_address, upstream_idx); /*will exit on error */
                }
                 if (g_wan ->ma_filter_mode == MODE_IS_INCLUDE && g_wan ->ma_comp_mode == MLDv2)
                 {

                     k_leave_src (IgmpProxySocket  , mcast_group_address, source, upstream_idx);
                 }
                 if (g_wan ->ma_filter_mode == MODE_IS_EXCLUDE && g_wan ->ma_comp_mode == MLDv2)
                 {
                     k_block_src (IgmpProxySocket  , mcast_group_address, source, upstream_idx);	
                 }
                /* since we passed exclude to CMTS
                *  CMTS should stop mcast tranmissions  if we are a the only subscriber
                *  But if thre are several subscribers, CMTS will transmit
                  */



                IF_SET (mifi, &g_wan->ma_downstream_ifset);	  /* Keep record of subscribed LANS to group G */
                IF_DEBUG (DEBUG_MFC)
                log_msg (LOG_DEBUG, 0,
                         "%s interface %s  added to downstream ifset of (G)= %s", __func__,
                         mvifs[mifi].mvif_name, inetFmt (mcast_group_address));

                IF_SET (mifi, &s->ma_downstream_ifset);	  /* Keep record of subscribed LANS to source S */
                IF_DEBUG (DEBUG_MFC)
                log_msg (LOG_DEBUG, 0,
                         "%s interface %s  added to downstream ifset of (S)= %s", __func__,
                         mvifs[mifi].mvif_name, inetFmt (source));



                k_make_negative_mfc ( source, mcast_group_address,	/* creates  negative mfc cache. i.e ttl=0 for ifset[mifi]  */
                                      mifi ,&s->ma_downstream_ifset );	 /* - it works, i.e traffic is stopped */

            } else
            {
                log_msg (LOG_ERR, 0,
                         "SSM  (G,S) = (%s ,%s) IN invalid filter mode %d on interface  %s\n",
                         inetFmt (mcast_group_address), inetFmt (source),
                         s->ma_filter_mode,
                         v->mvif_name);
            }
        } else
        { /*
		  TODO not implemented or source_process_mode() in sourcese
		  */
        }
    }
    return 0;
}
