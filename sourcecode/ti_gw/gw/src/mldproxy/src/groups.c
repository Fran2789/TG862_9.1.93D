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
#include "defs.h"
#include "vif.h"
#include "groups.h"

#include "mld6.h"
#include "mld6_proto.h"
#include "mld6v2_proto.h"
#include "debug.h"
#include "inet6.h"
#include "mld6v2.h"
#include "kern.h"
#include "timers.h"

struct listaddr *
find_group_in_list (struct mvif *v, struct listaddr *group)
{
  struct listaddr *g;

  /* Look for the group in our listener list. */
  for (g = v->uv_groups; g != NULL; g = g->ma_next)
    {
      if (g == group)
	break;
    }
  return g;
}

/* Make new group as responce to MLD report message */
struct listaddr *
make_new_group (mifi_t mifi, struct sockaddr_in6 *grp, uint8_t mld_version)
{

  struct listaddr *g;
  struct mvif *v = &mvifs[mifi];

  if ((!(v && grp)))
    {
      log_msg (LOG_ERR, 0, "%s sanity failure, null in params", __func__);
    }
  IF_DEBUG (DEBUG_MLD)
    log_msg (LOG_DEBUG, 0,
	     "The group %s is new, trying to add it to interface %s\n",
	     sa6_fmt (grp), v->mvif_name);

  g = (struct listaddr *) malloc (sizeof (struct listaddr));
  if (g == NULL)
    {
      log_msg (LOG_ERR, 0, "ran out of memory");	/* fatal */
      exit (15);
    }
  memset (g, 0, sizeof (*g));
  memcpy (&g->mcast_group, grp, sizeof (*grp));

  g->ma_comp_mode = mld_version;  /* mldv1 or mldv2 with no sources */
  
      
  if (g->ma_comp_mode & MLDv2)
  {
          g->ma_filter_mode = MODE_IS_EXCLUDE;	// IF NO sources for group in MLDv2 -  To_EX
  }
  
  if (mifi != upStreamVif)
    {
      create_rxmt_timer (v, g, ExpireRtrmtTimer);	/* Create timer to wait for group leave expiration */
     
      create_report_timer (v, g, ExpireReportTimer);	/* Create timer to wait for membership expiration */

      create_back_to_mldv2_timer (v, g, TimerExpire_and_move_to_mldv2_mode);	/* Create timer to wait for membership expiration */

      /* Start timer to wait for membership expiration */
      start_report_timer (v, g, MLD6_LISTENER_INTERVAL);

     
   
      
      #ifdef TODO_IMPLEMENTED_FILTER_MODE_TIMER /* we decided do not implemented sitch fro include to exclude */
       create_filterMode_timer(g); //* needed only in exclude mode */
      #endif

       if ( (v->uv_mld_version == MLDv2 ) && ( mld_version == MLDv1) )
       {
          start_back_to_mldv2_timer (v, g, MLD6_OLDER_VERSION_HOST_PRESENT);	// we are downgrading to MLDv1, but  try to return back to MLDv2
       }

  
      if ((mvifs[mifi].uv_mld_version & MLDv2) && (g->ma_comp_mode & MLDv1))
      {
         log_msg (LOG_DEBUG, 0,
	       "created a group in MLDv1 compat-mode for %s on Mif %s",
	       sa6_fmt (grp), mvifs[mifi].mvif_name);
       }
  
   }
  /* insert group fist  in the list of the groups of this interface */
  g->ma_next = v->uv_groups;
  v->uv_groups = g;
  time (&g->ma_ctime);
  return g;
}

/*
 * Checks for MLD listener: returns TRUE if there is a receiver for the group
 * on the given mvif, or returns FALSE otherwise.
 */
struct listaddr *
find_multicast_group (struct mvif *v, struct sockaddr_in6 *group)
{
  struct listaddr *g;

  /* Look for the group in our listener list. */
  for (g = v->uv_groups; g != NULL; g = g->ma_next)
    {
      if (inet6_equal (group, &g->mcast_group))
	break;
    }
  return g;
}

void
delete_group (mifi_t mifi, struct listaddr *group)
{
  struct listaddr *current, *prev, *next, *head_of_list;
  struct mvif *v = &mvifs[mifi];

  // Sanity
  if (mifi < 0 || group == NULL)
    {
      log_msg (LOG_ALERT, 0, "%s BUG, null group parameters");
    }
  IF_DEBUG (DEBUG_MLD_MEMBER)
    log_msg (LOG_DEBUG, 0,
	     "Entering delete_group() G=%s for interface %s\n",
	     sa6_fmt (&group->mcast_group), mvifs[mifi].mvif_name);

  if (mifi != upStreamVif)
    {
      delete_report_timer (v, group);
      delete_rxmt_timer (v, group);
    }

  head_of_list = current = prev = next = mvifs[mifi].uv_groups;	// list of multicast groups/addresses  - interface stores head of list
  while (current != NULL)
    {
      if (current == group)	// we are looking for a group by its memory addresses
	{
	  // Sanity 
	  if (memcmp (&current->mcast_group, &group->mcast_group, sizeof (group->mcast_group)) != 0)	// IPV6 multicast address in mcast_group field must match too
	    {
	      log_msg (LOG_ALERT, 0,
		       "BUG delete_group():Group MCAST addresss G=%s do not match stored address=%s from interface %s\n",
		       sa6_fmt (&current->mcast_group),
		       sa6_fmt (&group->mcast_group), mvifs[mifi].mvif_name);
	    }
	  IF_DEBUG (DEBUG_MLD_MEMBER)
	    log_msg (LOG_DEBUG, 0,
		     "delete_group() found group G=%s  in the  interface %s group list\n",
		     sa6_fmt (&current->mcast_group), mvifs[mifi].mvif_name);

	  if (prev == head_of_list && current == head_of_list)	// head of the list 
	    {


	      IF_DEBUG (DEBUG_MLD_MEMBER)
		log_msg (LOG_DEBUG, 0,
			 "delete_group() removing  HEAD of the list group G=%s  from  interface %s  %p\n",
			 sa6_fmt (&current->mcast_group),
			 mvifs[mifi].mvif_name, mvifs[mifi].uv_groups);
	      mvifs[mifi].uv_groups = current->ma_next;	// make next element to be head of the list instead of current
	      if (current->sources)
	          delete_group_sources (mifi, group);
	      free ((char *) current);
	      break;

	    }
	  else
	    {
	      prev->ma_next = current->ma_next;	// chain next list element instead of current 

	      IF_DEBUG (DEBUG_MLD_MEMBER)
		log_msg (LOG_DEBUG, 0,
			 "delete_group() removing group G=%s  from  interface %s group list %p, p=%p p->next=%p\n",
			 sa6_fmt (&current->mcast_group),
			 mvifs[mifi].mvif_name, mvifs[mifi].uv_groups, prev,
			 prev->ma_next);
	    }
	  if (current->sources)
	    {
	      IF_DEBUG (DEBUG_MLD_MEMBER)
		log_msg (LOG_DEBUG, 0,
			 "delete_group() found group G,S=%s  in the  interface %s group list, removing\n",
			 sa6_fmt (&current->mcast_group),
			 sa6_fmt (&current->sources->mcast_group),
			 mvifs[mifi].mvif_name);
	      delete_group_sources (mifi, group);
	    }
	  free ((char *) current);
	  break;
	}
      else
	{
	  prev = current;

	  current = next = current->ma_next;
	}
    }
  if (mifi != upStreamVif)
    delete_group_upstream (mifi, &group->mcast_group);	// Synchronize with Proxy interface
}

/*
 delete_group_upstream - removes group from  WAN database
  * It is not obliged to delete the whole group, it might be called to update the status of Upstream/WAN interface source list
*/

void
delete_group_upstream (mifi_t mifi,
		       struct sockaddr_in6 *group_multicast_address)
{
  short intfce = mvifs[mifi].uv_ifindex;
  register struct mvif *v = &mvifs[upStreamVif];
  register struct listaddr *wan_group;
  int rc=0;

  if_set zero;


  IF_ZERO (&zero);

  IF_DEBUG (DEBUG_MLD_MEMBER)
    log_msg (LOG_DEBUG, 0,
	     "Entering %s G=%s\n",
	     __func__,
	     sa6_fmt (group_multicast_address));

  wan_group = find_multicast_group (v, group_multicast_address);

  if (wan_group == NULL)
    log_msg (LOG_ERR, 0,
	     "cannot find multicast group  G=%s  on  interface %s, wan group not exist\n",
	     sa6_fmt (group_multicast_address), v->mvif_name);;

  if (wan_group->sources)
  {
      rc = delete_group_sources_upstream (mifi, wan_group);	// Delete all remaining (S,)  of group G

      if (rc >0 )  // there are still active Sources, 
      {
        if (wan_group->sources == NULL )
	IF_DEBUG (DEBUG_MLD_MEMBER)
        {
		log_msg (LOG_ERR, 0,
			     "BUG group  G=%s still contains N=%d active  SSM, but sources list is empty \n", 
			     sa6_fmt (&wan_group->mcast_group),rc);
        }
	IF_DEBUG (DEBUG_MLD_MEMBER)
		log_msg (LOG_DEBUG, 0,
			     "upstream group  G=%s still contains N=%d active  SSM \n",
			     sa6_fmt (&wan_group->mcast_group), rc);
         return ;
      }
      wan_group->ma_comp_mode &= (~MCAST_SSM); /* All sources deleted, NO SSM in group */
  }
  IF_CLR (mifi, &(wan_group->ma_downstream_ifset));
 
 
  IF_DEBUG (DEBUG_MLD_MEMBER)
    log_msg (LOG_DEBUG, 0,
	     "Clearing bitmask for %s  delete_group_upstream G=%s\n",
	     mvifs[mifi].mvif_name, sa6_fmt (&wan_group->mcast_group));

  // Test if  other DS interface still have active subscribers for this group
  if (memcmp (&wan_group->ma_downstream_ifset, &zero, sizeof (if_set)) == 0)
    {
      // No more subscribers
      IF_DEBUG (DEBUG_MLD_MEMBER)
        	log_msg (LOG_DEBUG, 0,
	            	 "No more subscribers for G=%s on LAN interfaces \n",
	          	 sa6_fmt (&wan_group->mcast_group));
       k_leave (mld6_proxy_socket, &wan_group->mcast_group.sin6_addr, upStreamVif);	// Send DONE|Leave  MLD message

      k_del_from_mfc6 (mld6_socket, &wan_group->transmitter, &wan_group->mcast_group);	// Delete mcast entries in kernel MFC cache
      IF_DEBUG (DEBUG_MFC)
	log_msg (LOG_DEBUG, 0,
		 "Cleared MFC cache entry delete_group_upstream G=%s\n",
		 sa6_fmt (&wan_group->mcast_group));
      delete_group (upStreamVif, wan_group);

    }

}


int
mld_merge_with_upstream (mifi_t mifi,
			 struct sockaddr_in6 *mcast_group_address,
			 uint8_t mld_version, struct sockaddr_in6 *source,
                         uint8_t source_filter_mode)
{
  register struct mvif *v = &mvifs[upStreamVif];
  register struct listaddr *g_wan ;
  short new_group = 0;

  /*
   * Look for the group in the interface's  group list; if found,
   *  .
   */

  g_wan = find_multicast_group (v, mcast_group_address);

  if (g_wan == NULL)
    {

      IF_DEBUG (DEBUG_IF)
	log_msg (LOG_DEBUG, 0,
		 "The group %s doesn't exist on Upstream interface %s, trying to add it\n",
		 sa6_fmt (mcast_group_address), v->mvif_name);

      g_wan = make_new_group (upStreamVif, mcast_group_address, mld_version);	// Create a group
      if (g_wan == NULL)
	{
	  log_msg (LOG_ERR, 0,
		   "%s: G=%s on Upstream interface %s  failed", __func__ ,
		   sa6_fmt (mcast_group_address), v->mvif_name);
	  return;
	}
      new_group = 1;
    }
   
   
   { 
   g_wan->ma_comp_mode |= mld_version; /* add  version if group is in mixed mode  */
   /* Extend the mode - ASM or SSM or both */
   if ( mld_version == MLDv1)
       g_wan->ma_comp_mode |= MCAST_ASM;
   else if (source == NULL && mld_version == MLDv2)
      g_wan->ma_comp_mode |= MCAST_ASM;

   else if (source != NULL && mld_version == MLDv2)
     
     g_wan->ma_comp_mode |= MCAST_SSM;
   }
   
   
   
   
  if (source != NULL && mld_version == MLDv2)
    {
      struct listaddr *s ;
      // Find whether multicast source  is already exust in wan group (registered)  -SSM exists
   
       s = find_requested_mcast_transmitter (v, mcast_group_address, g_wan, source);
					                        

      if (s != NULL)  // needed  merge routing to other LAN DS interfaces
	{
            if ( IF_ISSET (mifi, &s->ma_downstream_ifset) )
	          return;		// no merge required, source is present in DS SET
	    IF_DEBUG (DEBUG_IF)
	        log_msg (LOG_DEBUG, 0,
		     " (G,S) = (%s ,%s)  already exist in upstream interface database\n",
		     sa6_fmt (mcast_group_address), sa6_fmt (source));

	     if (s->ma_filter_mode == MODE_IS_INCLUDE)
	     { 
	      /* IF_SET (mifi, &g_wan->ma_downstream_ifset); */  // Keep record of subscribed LANS to group G
	      IF_SET (mifi, &s->ma_downstream_ifset);   // Keep record of subscribed LANS to source S
	        IF_DEBUG (DEBUG_IF)
	                  log_msg (LOG_DEBUG, 0,
		                   "SSM routing  to LAN interface %s added  for G,S= (%s ,%s) to Downstream bitmap",
		             v->mvif_name,
		             sa6_fmt (mcast_group_address), sa6_fmt (source)
                             );
	      k_add_to_mfc6 (mld6_socket, source,	/* sender of multicast */
			     mcast_group_address,	/* multicast address - the destination */
			     upStreamVif,	/* from where it will come - upstream virtual vif */
			     &s->ma_downstream_ifset	/*set of DS intefaces to send mcasts to */
		);
	        IF_DEBUG (DEBUG_MFC)
	                  log_msg (LOG_DEBUG, 0,
		                   "SSM routing  to LAN interface %s added  for G,S= (%s ,%s) to MFC cache",
		             v->mvif_name,
		             sa6_fmt (mcast_group_address), sa6_fmt (source)
                             );
             }
             else
             {  // MODE_IS_EXCLUDE -negative  // TODO - test MIX of negative and positibe
	        IF_DEBUG (DEBUG_MFC)
	                  log_msg (LOG_DEBUG, 0,
		                   "SSM Making negativ routing  to LAN interface %s added  for G,S= (%s ,%s) to MFC cache",
		             v->mvif_name,
		             sa6_fmt (mcast_group_address), sa6_fmt (source)
                             );
	         k_add_to_mfc6 (mld6_socket, source, mcast_group_address,
			     upStreamVif, NULL);  // creates  negative mfc cache - it works, i.e traffic is stopped
             }
	     return;		// no merge required, source is present in upStreamVif
	}
      else
	{
	  s = make_new_source (upStreamVif, g_wan, mcast_group_address, source,source_filter_mode);
	  if (s == NULL)
	    {
	      log_msg (LOG_ERR, 0,
		       "make_new_(group, source )= (%s,%s) on Upstream interface %s  failed\n",
		       sa6_fmt (mcast_group_address), sa6_fmt (source),
		       v->mvif_name);
	      return;
	    }
	}
      if (s != NULL)
	{
	  // Only new source going to proxy join

	  k_join_src (mld6_proxy_socket, &mcast_group_address->sin6_addr, &source->sin6_addr, upstream_idx);	// Add new  SSM to upstream interface,  (will exit on error)
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "SSM  (G,S) join request sent = (%s ,%s) on interface  %s\n",
		     sa6_fmt (mcast_group_address), sa6_fmt (source),
		     v->mvif_name);
	  /*
	   * we can create mfc cache entry in advance since we now the required source address, 
	   * i.e from where mcast will come
	   */
	  if (s->ma_filter_mode == MODE_IS_INCLUDE)
	  { 
	      /*IF_SET (mifi, &g_wan->ma_downstream_ifset); */  // Keep record of subscribed LANS to group G
	      IF_SET (mifi, &s->ma_downstream_ifset);   // Keep record of subscribed LANS to source S

	      IF_DEBUG (DEBUG_IF)
	               log_msg (LOG_DEBUG, 0,
		     "SSM  (G,S) = (%s ,%s) IN MODE_IS_INCLUDE on interface  %s\n",
		     sa6_fmt (mcast_group_address), sa6_fmt (source),
		     v->mvif_name);
	      k_add_to_mfc6 (mld6_socket, source,	/* sender of multicast */
			     mcast_group_address,	/* multicast address - the destination */
			     upStreamVif,	/* from where it will come - upstream virtual vif */
			     &s->ma_downstream_ifset	/*set of DS intefaces to send mcasts to */
		);
	  }
	  else if (s->ma_filter_mode == MODE_IS_EXCLUDE )
	  {
	      // else -> s->ma_filter_mode == MODE_IS_EXCLUDE
	      /*
	       * create negative cache entry for the given source
	       *  by setting   oif=NULL or ttl=0 or ttl=255
	       */
	       IF_DEBUG (DEBUG_IF)
	               log_msg (LOG_DEBUG, 0,
		                             "SSM  (G,S) = (%s ,%s) IN MODE_IS_EXCLUDE on interface  %s\n",
		                            sa6_fmt (mcast_group_address), sa6_fmt (source),
	                        	     v->mvif_name);
	      //  make kernel to produc MLD report and pass exclude to CMTS
	      k_block_src (mld6_proxy_socket, &mcast_group_address->sin6_addr, &source->sin6_addr, upstream_idx);	// Add new  SSM to upstream interface,  (will exit on error)
	      //  since we passed exclude to CMTS
	      //  CMTS should stop mcast tranmissions  if we are a the only subscriber
	      //  But if thre are several subscribers, CMTS will transmit
	      k_add_to_mfc6 (mld6_socket, source, mcast_group_address, // creates  negative mfc cache 
	                     upStreamVif, NULL);  // - it works, i.e traffic is stopped
	  }
          else
	  {
	              log_msg (LOG_ERR, 0,
		     "SSM  (G,S) = (%s ,%s) IN invalid filter mode %d on interface  %s\n",
		     sa6_fmt (mcast_group_address), sa6_fmt (source),
                     s->ma_filter_mode,
		     v->mvif_name);
	  }
	}
        else
        { // TODO not implemented or source_process_mode() in sourcese
        }
    }
  else  /* ASM or MLDv1, no sources */
  {
      if (new_group)
	{
	  /* Make a proxy action - join the group */
	  k_join (mld6_proxy_socket, &mcast_group_address->sin6_addr, upstream_idx);	//will exit on error
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     " (G,*) join request sent = (%s ) on physical interface#%d %s\n",
		     sa6_fmt (mcast_group_address), upstream_idx,
		     v->mvif_name);
	}
    

  // Prepare set of downstream interfaces when the cache miss will occur
  /*
  *  the closed multicast session leaves mfc cache in this  invalid state for around ~4 sec time, then  the entry is finally removed
  # cat /proc/net/ip6_mr_cache 
  Group                            Origin                           Iif      Pkts  Bytes     Wrong  Oifs
  ff1e:0000:0000:0000:0000:0000:0000:0100 fe80:0000:0000:0000:0250:f1ff:fe08:b663 -1         0        0        0
  */


  // MFC :if  (! IF_ISSET(mvifs[mifi].uv_ifindex,  &(g->downstream_ifset)) )
    if (!IF_ISSET (mifi, &(g_wan->ma_downstream_ifset)))	// assuming mifi is the same number in /proc/net/ip6_mfc
    {
      //IF_SET(mvifs[mifi].uv_ifindex,& (g->downstream_ifset));
      // TODO -Wrong iif = 3 IF_SET(mifi ,&(g->downstream_ifset) );
      IF_SET (mifi, &(g_wan->ma_downstream_ifset));
      IF_DEBUG (DEBUG_IF)
	log_msg (LOG_DEBUG, 0,
		 " interface %s  added to downstream ifset of (G,*)= %s\n",
		 mvifs[mifi].mvif_name, sa6_fmt (mcast_group_address));

    }
  }
  return 0;
}
