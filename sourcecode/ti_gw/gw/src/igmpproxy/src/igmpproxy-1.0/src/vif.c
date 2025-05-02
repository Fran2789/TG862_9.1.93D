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
#include <time.h>


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <ifaddrs.h>
#include <linux/igmp.h>
#include "defs.h"
#include "vif.h"
#include "mcast_proto.h"
#include "groups.h"
#include "timers.h"
#include "config.h"
#include "inet6.h"
#include "kern.h"
#include "debug.h"
#include "igmpproxy.h"

struct mvif	mvifs[MAXMVIFS];	/*the list of virtual interfaces */
mifi_t numvifs;				/*total number of interface */
int16_t upStreamVif;
int16_t upstream_idx;

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

	
	
	
	IF_DEBUG(DEBUG_IF)
		log_msg(LOG_DEBUG, 0, "Getting vif configuration from  file %s", configfilename);
		
	/*
	 * Configure the vifs based on the interface configuration of
	 * the kernel and the contents of the configuration file.
	 * Copmpliment config file by default values of protocol
	 */

        numvifs=read_config(); /* returns N of interfaces in config file N DS +1 UP */
	
	if ( numvifs < 2)  /* counting from 1, */
	{
		log_msg(LOG_ERR, 0, "can't forward: %s",
		    numvifs == 0 ? "no enabled vifs" :
		     "only one enabled vif");
		return;
	}
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
#if 0
int get_interface_index(const char *interface_name)
{
      struct ifreq ifr;
      int fd;
 
      memset(&ifr, 0, sizeof(ifr));
 
      // setup ifr for ioctl 
      strncpy (ifr.ifr_name, interfaceName, sizeof(ifr.ifr_name) - 1);
      ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';
 
      // create socket
      fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
      if ( fd == -1) {
             fprintf(stderr," Could not create raw socket:  %s \n", strerror(errno));
             return -1;
      }
 
      // get index 
      if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1)
      {
              close(fd);
              return (-1);
      }
      // close socket if created locally
      close(fd);
 
      return ifr.ifr_ifindex;
}
#endif
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

	k_add_vif(IgmpSocket,vifi,v );
	IF_DEBUG(DEBUG_IF)
		log_msg(LOG_DEBUG,0,"%s comes up ,vif #%u now in service",v->mvif_name,vifi);

	if ((v->uv_flags & VIFF_DOWNSTREAM ))
	{  /* not upstream */
	    /*
	     * Join the ALL-ROUTERS multicast group on the interface.
	     *  to allow receiving of mld6 messages (of mldv1).
	     */
	     
	    k_join(IgmpSocket, allrouters_group, v->uv_ifindex);
	     IF_DEBUG(DEBUG_IF)
		log_msg(LOG_DEBUG,0,"%s set up vif #%u for allrouters_group",v->mvif_name,vifi);
	    
	     /* Set up interface to accept IGMPv3  */
	    
            if ( v->uv_mld_version & IGMPv3)
	    {
	        if (k_join(IgmpSocket, IGMPV3_ALL_MCR, v->uv_ifindex) <0)
                {
		    log_msg(LOG_ERR,errno,"Failed to  set up vif %s for ssmrouters_group 224.0.0.22",v->mvif_name);
                }
	        IF_DEBUG(DEBUG_IF)
		     log_msg(LOG_DEBUG,0,"%s set up vif #%u for ssmrouters_group 224.0.0.22",v->mvif_name,vifi);
	    }
	    /*
	     * Until neighbors are discovered, assume responsibility for sending
	     * periodic group membership queries to the subnet.  Send the first
	     * query.
	     */
	    


	    create_genQuery_timer (vifi, v , ExpireGenericQueryTimer);
	    start_genQuery_timer ( v , v->uv_startupQueryInterval);
	 
	     IF_DEBUG(DEBUG_MLD_PROTO)
			log_msg(LOG_DEBUG,0,"Starting to send Generic queries on interface %s with a period of %d ",
                               v->mvif_name, v->uv_startupQueryInterval);
			
            sendGeneralMembershipQuery( v);
	    
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
		k_leave(IgmpSocket, allrouters_group,
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
			delete_group (vifi,  a);  /* frees all the related sources as well*/
			delete_group_upstream (vifi, a->mcast_group); /* synchronize delete with upstream */
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
	k_del_vif( IgmpSocket, vifi);
	v->uv_flags =  VIFF_DISABLED;

        
	

	vifs_down = TRUE;

	IF_DEBUG(DEBUG_IF)
		log_msg(LOG_DEBUG, 0, "%s goes down, vif #%u out of service",
			v->mvif_name, vifi);
}

int config_vif_from_kernel( struct mvif *v)
{

	short rc=0;
	struct sockaddr_in addr;
	struct in_addr mask;
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
		 * Ignore any address family other than IPv4.
		 */
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_INET)
		{
			/* Eventually may have IPv6 address  */
			total_interfaces++;
			continue;
		}
		
        /* ARRIS MOD START for PROD00209636*/
        /* Only compare ifname here will fetch l2sd0.4xxx's ip address for interface l2sd0.4 */
		if (strcmp(v->mvif_name, ifa->ifa_name) != 0) 
			continue;
        /* ARRIS MOD END for PROD00209636*/

		memcpy(&addr, ifa->ifa_addr, sizeof(struct sockaddr_in));
#if 0
        IF_DEBUG(DEBUG_IF)
        {
	        printf ("IPv4 address %s of interface %s\n",ifa->ifa_name,sa4_fmt(&addr), v->mvif_name );
        }
#endif
		flags = ifa->ifa_flags;

		/*
		 * Get netmask of the address.
		 */
		memcpy(&mask,
		       &((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr,
		       sizeof(mask));


		
			memcpy(&(v->InAdr ), &addr.sin_addr,  sizeof(struct in_addr));
			IF_DEBUG(DEBUG_IF) 
            {
			    printf(
			    "IGMPROXY: Interface %s -> Link local address  %s "
			    "on phys interface n #%d\n",
			     v->mvif_name, 
			     inet_ntoa(v->InAdr),
			     v->uv_ifindex);
			     rc=1;
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
   return NOVIF;
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

