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



#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/epoll.h>
#include <bits/time.h>

#include <sys/timerfd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#ifdef __linux__
#include <linux/mroute6.h>
#else
#include <netinet6/ip6_mroute.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <ifaddrs.h>
#include "defs.h"
#include "vif.h"
#include "groups.h"
#include "timers.h"
#include "mld6.h"
#include "mld6v2.h"
#include "config.h"
#include "inet6.h"
#include "kern.h"
#include "mld6_proto.h"
#include "mld6v2_proto.h"
#include "debug.h"


struct mvif	mvifs[MAXMVIFS];	/*the list of virtual interfaces */
mifi_t numvifs;				/*total number of interface */
u_int16_t upStreamVif;
u_int16_t upstream_idx;

int vifs_down;

int default_vif_status;


int total_interfaces;

void start_all_vifs __P((void));
void start_vif __P((mifi_t vifi));
void stop_vif __P((mifi_t vivi));


static int read_config(void);

static int read_config(void)
{
    int rc;
    /*Created vif[] array from the configuration file */
    if( (rc=loadConfig( configfilename )) <=0 ) 
    {
            log_msg(LOG_ERR, 0, "Unable to load config file...");
    }
    return  rc; 
   
}

void create_mcastVifs()
{
	mifi_t vifi;
	struct mvif *v;
	int enabled_vifs;

	numvifs = 0;
	memset(&mvifs[0], 0,  sizeof(mvifs) );

	/*
	 * Configure the vifs based on the interface configuration of
	 * the kernel and the contents of the configuration file.
	 * (Open a UDP socket for ioctl use in the config procedures if
	 * the kernel can't handle IOCTL's on the MLD socket.)
	 */
	
	for (vifi = 0, v = mvifs; vifi < MAXMVIFS; ++vifi, ++v) {
		memset(v, 0, sizeof(*v));
		// first initialize to defaults
		v->uv_mld_version = MLD6_DEFAULT_VERSION;
		v->uv_mld_robustness = MLD6_DEFAULT_ROBUSTNESS;
		v->uv_mld_query_interval = MLD6_DEFAULT_QUERY_INTERVAL;
		v->uv_mld_query_rsp_interval = MLD6_DEFAULT_QUERY_RESPONSE_INTERVAL;
		v->uv_mld_llqi = MLD6_DEFAULT_LAST_LISTENER_QUERY_INTERVAL ; 
		v->uv_mld_llqc = MLD6_DEFAULT_ROBUSTNESS -1;  /* RFC saya llqc -a attepmts */
	}
	IF_DEBUG(DEBUG_IF)
		log_msg(LOG_DEBUG, 0, "Interfaces world initialized...");
	IF_DEBUG(DEBUG_IF)
		log_msg(LOG_DEBUG, 0, "Getting vif configuratin from %s", configfilename);
        numvifs=read_config(); // returns N of interfaces in config file N DS +1 UP
	
	if ( numvifs < 2)  /* counting from 1, */
		log_msg(LOG_ERR, 0, "can't forward: %s",
		    enabled_vifs == 0 ? "no enabled vifs" :
		     "only one enabled vif");
	IF_DEBUG(DEBUG_IF)
	    log_msg(LOG_DEBUG, 0,  "Found %d interfaces in the config file %s \n", numvifs,configfilename);
	for (vifi = 0, v = mvifs; vifi < numvifs; ++vifi, ++v)
        {
	   config_vif_from_kernel(v);
        }
        IF_DEBUG(DEBUG_IF)
	     dump_vifs(log_fp);

}


void start_all_vifs(void)
{
	mifi_t vifi;
	struct mvif *v;

	
	for (vifi = 0, v = mvifs; vifi < numvifs; vifi++, v++) {
			

			if (v->uv_flags & VIFF_DISABLED) {
				IF_DEBUG(DEBUG_IF)
					log_msg(LOG_DEBUG, 0,
					    "%s is %s; vif #%u out of service",
					    v->mvif_name,
					    v->uv_flags & VIFF_DISABLED ? "DISABLED" : "DOWN",
					    vifi); 
				continue;
			}
			start_vif(vifi);
	}
}

/*
 * Initialize the vif and add to the kernel. The vif can be either
 * physical, register or tunnel (tunnels will be used in the future
 * when this code becomes PIM multicast boarder router.
 */

void start_vif (mifi_t vifi)
{
	struct mvif *v = &mvifs[vifi];

	
	/* Sanity */
	if ( ! (v->uv_ifindex) )
	{
	    log_msg(LOG_ERR, 0 , "start_vif() Sanity check failed for vifi=%d, total N of vifs=%d",vifi,
            numvifs);
	    return; /* wrong interface */
	}

	/* Tell kernel to add, i.e. start this vif */

	k_add_vif(mld6_socket,vifi,v );
	IF_DEBUG(DEBUG_IF)
		log_msg(LOG_DEBUG,0,"%s comes up ,vif #%u now in service",v->mvif_name,vifi);

	if ((v->uv_flags != VIFF_UPSTREAM ))
	{  /* not upstream */
	    /*
	     * Join the ALL-ROUTERS multicast group on the interface.
	     * This allows mtrace requests to loop back if they are run
	     * on the multicast router.this allow receiving mld6 messages too.
	     */
	    k_join(mld6_socket, &allrouters_group.sin6_addr, v->uv_ifindex);
	     IF_DEBUG(DEBUG_IF)
		log_msg(LOG_DEBUG,0,"%s set up vif #%u for allrouters_group",v->mvif_name,vifi);
        /* Set up interface to acceptMLDv2  */
         if ( v->uv_mld_version & MLDv2 )
	    {
	        if (k_join(mld6_socket, &ssm_routers_group.sin6_addr, v->uv_ifindex) <0)
            {
		        log_msg(LOG_ERR,errno,"Failed to  set up vif %s for ssmrouters_group FF02::16",v->mvif_name);
             }
	        IF_DEBUG(DEBUG_IF)
		     log_msg(LOG_DEBUG,0,"%s set up vif #%u for ssmrouters_group FF02::16",v->mvif_name,vifi);
        }
	    /*
	     * Until neighbors are discovered, assume responsibility for sending
	     * periodic group membership queries to the subnet.  Send the first
	     * query.
	     */
	    
	   
	    if (v->uv_startupQueryCount <=0 )
	        v->uv_startupQueryCount = MLD6_ROBUSTNESS_VARIABLE ;
	    
	    create_genQuery_timer (vifi, v ,ExpireGenericQueryTimer);
            start_genQuery_timer(v, v->uv_startupQueryInterval);
            sendGeneralMembershipQuery(v);
	}
	else
        /* upstream */
	{
                upstream_idx=v->uv_ifindex;
        }
}

/*
 * Stop a vif (either physical interface, tunnel or
 * register.) If we are running only PIM we don't have tunnels.
 */ 


void
stop_vif(mifi_t vifi)
{
	struct mvif *v;
	struct listaddr *a;
	
	v = &mvifs[vifi];
	if (vifi !=upStreamVif )
	{ /* not upstream */
		stop_genQuery_timer ( v); /* stop Generic Queries timer */
		k_leave(mld6_socket, &allrouters_group.sin6_addr,
			v->uv_ifindex);

		/*
		 * Discard all group addresses.  (No need to tell kernel;
		 * the k_del_vif() call will clean up kernel state.)
		 */
		while (v->uv_groups != NULL)
		{
			a = v->uv_groups;
			v->uv_groups = a->ma_next;
			stop_genQuery_timer(v);
			
			/* discard the group */
			delete_group (vifi,  a);  // frees all the related sources as well
			delete_group_upstream (vifi, &a->mcast_group); // synchronize delete with upstream
			free((char *)a);
		}
		v->uv_groups = NULL;    
	} /* not upstream */
	/*
	 * Delete the interface from the kernel's vif structure.
	 */
	if (vifi !=upStreamVif )
	{
	    delete_genQuery_timer (v);
	}
	k_del_vif( mld6_socket, vifi);
	v->uv_flags =  VIFF_DISABLED;

        
	

	vifs_down = TRUE;

	IF_DEBUG(DEBUG_IF)
		log_msg(LOG_DEBUG, 0, "%s goes down, vif #%u out of service",
			v->mvif_name, vifi);
}

int config_vif_from_kernel( struct mvif *v)
{

	short rc=0;
	struct sockaddr_in6 addr;
	struct in6_addr mask;
	short flags;
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap))
		log_msg(LOG_ERR, errno, "getifaddrs");

	/*
	 * Loop through all interface' addresses.
	 */
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) 
	{

		/*
		 * Ignore any address family other than IPv6.
		 */
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_INET6) {
			/* Eventually may have IPv6 address later */
			total_interfaces++;
			continue;
		}
		if (strncmp(v->mvif_name, ifa->ifa_name, sizeof(IFNAMSIZ)) != 0) 
			continue;

		memcpy(&addr, ifa->ifa_addr, sizeof(struct sockaddr_in6));
	        //printf ("IPv6 addresses %s of interface %s",ifa->ifa_name,sa6_fmt(&addr) );
		flags = ifa->ifa_flags;

		/*
		 * Get netmask of the address.
		 */
		memcpy(&mask,
		       &((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr,
		       sizeof(mask));


		/*
		 * Get IPv6 specific flags, and ignore an anycast address.
		 * XXX: how about a deprecated, tentative, duplicated or
		 * detached address?
		 */

		if (IN6_IS_ADDR_LINKLOCAL(&addr.sin6_addr))
		{
			addr.sin6_scope_id = if_nametoindex(ifa->ifa_name);
			memcpy(&(v->uv_linklocal ), &addr,  sizeof(struct sockaddr_in6));
			    //"Link local address %s (%s on subnet %s) ,"
			    //net6name(&v->uv_prefix.sin6_addr,&mask),
			printf(
			    "MLDPROXY: Interface %s -> Link local address  %s "
			    "on phys interface n #%d\n",
			     v->mvif_name, 
			     sa6_fmt(&v->uv_linklocal),
			     v->uv_ifindex);
			     rc=1;
			     break;
		}
	}

	freeifaddrs(ifap);
	return rc;
}








/*  
 * If the source is directly connected, or is local address,
 * find the vif number for the corresponding physical interface
 * (tunnels excluded).
 * Return the vif number or NO_VIF if not found.
 */ 

mifi_t find_vif_by_ifindex(u_int8 ifindex)
{
   register struct mvif *v;
   register int i;
   for (i = 0, v = mvifs; i < numvifs; i++) {
        if ( v->uv_ifindex == ifindex )
	  return i;
        v++;
   }
   return -1;
}
/*  
 * If the source is directly connected, or is local address,
 * find the vif number for the corresponding physical interface
 * (tunnels excluded).
 * Return the vif number or NO_VIF if not found.
 */ 



/*  
 * stop all vifs
 */ 
void
stop_all_vifs()
{
    mifi_t vifi;
    struct mvif *v;
 
    for (vifi = 0, v=mvifs; vifi < numvifs; ++vifi, ++v) {
	if (v->uv_flags &  VIFF_DISABLED)
		continue;
	stop_vif(vifi);
    }
}

