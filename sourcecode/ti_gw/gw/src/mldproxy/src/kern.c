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
 * This program has been derived from pim6sd.        
 * The pim6sd program is covered by the license in the accompanying file
 * named "LICENSE.pim6sd".
 *

 * The changed portions of the program are covered by the following license 
 * in the  accompanying file named "LICENSE.mldproxy"
*/
#include <errno.h>
#include <string.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <net/if.h>

#include <net/route.h>
#include <netinet/in.h>

#include <linux/mroute6.h>


#include "defs.h"
#include "vif.h"
#include "groups.h"
#include "mld6.h"
#include "inet6.h"
#include "debug.h"
#include "kern.h"


static int process_cache_miss(struct mrt6msg * im)
{
	static struct sockaddr_in6 mcast_sender;
	static struct sockaddr_in6 mcastgrp;
	struct listaddr * g_wan;
	mifi_t          iif, mifi, vifi;
	register struct mvif *v;
	int rc;

	if_set zero;


	IF_ZERO (&zero);

	init_sin6(&mcastgrp);

	init_sin6(&mcast_sender);


	/*
	 * When there is a cache miss, we check only the header of the packet
	 * (and only it should be sent up by the kernel.)
	 */
	mcastgrp.sin6_addr = im->im6_dst;
	mcastgrp.sin6_scope_id = inet6_mvif2scopeid(&mcastgrp, &mvifs[im->im6_mif]);
	mcast_sender.sin6_addr = im->im6_src;
	mcast_sender.sin6_scope_id = inet6_mvif2scopeid(&mcast_sender, &mvifs[im->im6_mif]);
	iif = im->im6_mif; // /proc/net/ip6_mr_vif


	if (IN6_IS_ADDR_MC_NODELOCAL(&mcastgrp.sin6_addr) ||/* sanity? */
		IN6_IS_ADDR_MC_LINKLOCAL(&mcastgrp.sin6_addr))
	{
		log_msg(LOG_DEBUG, 0, "Error : process_cache_miss IN6_IS_ADDR_MC_NODELOCAL || IN6_IS_ADDR_MC_LINKLOCAL ");
		return -1;

	}
	IF_DEBUG(DEBUG_KERN)
	log_msg(LOG_DEBUG, 0, "processing cache miss for mcastgrp (G)=%s from  multicast Sender=%s",
			sa6_fmt(&mcastgrp),
			sa6_fmt(&mcast_sender));
	if ( iif == upStreamVif )
	{
		log_msg(LOG_DEBUG, 0, "Cache miss from  upStreamVif interface %d ", iif);
	} else
	{
		IF_DEBUG(DEBUG_KERN)
		log_msg(LOG_DEBUG, 0, "Cache miss from : process_cache_miss from non-upsream interface ");
		return -1; //I got cache miss from rndbr1 when only link-local waas cobfigured
	}
	mvifs[upStreamVif].uv_cache_miss++;
	//find mcastgrp in the upstream interface mcastgrp list
	g_wan = find_multicast_group(&mvifs[upStreamVif], &mcastgrp);
	if (g_wan == NULL )
	{
		IF_DEBUG(DEBUG_KERN)
		log_msg(LOG_DEBUG, 0, "Unsolicit multicast received :"
				"cache_miss  but group is not found in upstream  interface %s list, exiting ", 
				mvifs[iif].mvif_name);
		return -1;
	}



	/* XXX. if there are too many cache miss for the same (S,G), install
	* negative cache entry in the kernel (oif==NULL) to prevent too many
	* upcalls. - seems not the case on DOCSIS-3.0 where multicast are filtered by dcid
	*/
	{ /*
	  * Do not create MFC entry for non-registered by SSM join sender 
	  * i.e Ensure Group is  exclusively in SSM mode 
		*/

		if ((g_wan->sources != NULL) &&
			(memcmp (&g_wan->ma_downstream_ifset, &zero, sizeof (if_set)) == 0 ))  /* ASM will set bits in g_wan->ma_downstream_ifset*/
		{

			return -1; /* Do nothing - MFC cache entry for SSM join  was set up in advance */
		}
	}   
	/* Install MFC cache entry if group is in mixed SSM/ASM mode */
	memcpy( &g_wan->transmitter, &mcast_sender, sizeof(mcast_sender));
	IF_DEBUG(DEBUG_KERN)
	log_msg(LOG_DEBUG, 0, "Adding MFC entry for group (G)=%s from  multicast Sender=%s",
			sa6_fmt(&mcastgrp),
			sa6_fmt(&mcast_sender));

	rc=k_add_to_mfc6(mld6_socket, &mcast_sender, /* sender of multicast*/ 
					 &mcastgrp,	 /* multicast address - the destination */
					 upStreamVif,  /* from where it come - upstream */
					 &(g_wan->ma_downstream_ifset) /*set of the DS interfaces to route to */
					);
	return rc;
}



/*
 * A multicast packet has been received on wrong iif by the kernel. Check for
 * a matching entry. If there is (S,G) with reset SPTbit and the packet was
 * received on the iif toward the source, this completes the switch to the
 * shortest path and triggers (S,G) prune toward the RP (unless I am the RP).
 * Otherwise, if the packet's iif is in the oiflist of the routing entry,
 * trigger an Assert.
 */

static int process_wrong_iif(struct mrt6msg * im)
{

	short iif = im->im6_mif;

	log_msg(LOG_DEBUG, 0, "Error : process_wrong_iif   interface=%d  - not upStream   interface upstream_idx= %d  ", iif, upstream_idx);

	return -1;
}

int process_kernel_call(void )
{
	register struct mrt6msg *im;	/* igmpmsg control struct */

	im = (struct mrt6msg *) mld6_recv_buf;

	switch (im->im6_msgtype)
	{
	case MRT6MSG_NOCACHE:
		return process_cache_miss(im);
		break;
	case MRT6MSG_WRONGMIF:
		IF_DEBUG(DEBUG_KERN)
		log_msg(LOG_DEBUG, 0, " cache miss MRT6MSG_WRONGMIF: calling process_wrong_iif ");
		return process_wrong_iif(im);
		break;
	default:
		IF_DEBUG(DEBUG_KERN)
		log_msg(LOG_DEBUG, 0, "Unknown kernel_call code");
		return -1;
		break;
	}
	return -1;
}


/* 
 * Set the socket receiving buffer. `bufsize` is the preferred size,
 * `minsize` is the smallest acceptable size.
 */ 

int k_set_rcvbuf(int socket, int bufsize, int minsize)
{
	int             delta = bufsize / 2;
	int             iter = 0;

	/*
	 * Set the socket buffer.  If we can't set it as large as we
	 * want, search around to try to find the highest acceptable
	 * value.  The highest acceptable value being smaller than
	 * minsize is a fatal error. 
	 */


	if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) < 0)
	{
		bufsize -= delta;
		while (1)
		{
			iter++;
			if (delta > 1)
				delta /= 2;
			if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) < 0)
				bufsize -= delta;
			else
			{
				if (delta < 1024)
					break;
				bufsize += delta;
			}
		}
		if (bufsize < minsize)
			log_msg(LOG_ERR, 0, "OS-allowed buffer size %u < app min %u",
					bufsize, minsize);
		return -1; /*NOTREACHED*/

	}
	IF_DEBUG(DEBUG_KERN)
	log_msg(LOG_DEBUG,0,"Buffer reception size for socket %d : %d in %d iterations",socket, bufsize, iter);
	return 1;
}

/*  
 * Set the default Hop Limit for the multicast packets outgoing from this
 * socket.
 */ 

int 
k_set_hlim(int socket,  u_int8_t hop_limit )
{
	int             hlim = hop_limit;

	if (setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &hlim, sizeof(hlim)) < 0)
	{
		log_msg(LOG_ERR,errno,"k_set_hlim");
		return -1;
	}

	return 1;
}

/*
 * Set/reset the IPV6_MULTICAST_LOOP. Set/reset is specified by "flag".
 */


int 
k_set_loop(int socket, int flag)
{
	u_int           loop;

	loop = flag;
	if (setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (char *) &loop, sizeof(loop)) < 0)
	{
		log_msg(LOG_ERR,errno,"k_set_loop");
		return -1;
	}
	return 1;
}

/*
 * Set the IPV6_MULTICAST_IF option on local interface which has the
 * specified index.
 */  


int 
k_set_if(int socket, u_int16_t ifindex)
{
	if (setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_IF,
				   (char *) &ifindex, sizeof(ifindex)) < 0)
	{
		log_msg(LOG_ERR, errno, "setsockopt IPV6_MULTICAST_IF for %s",
				ifindex2str(ifindex));
		return -1;
	}

	return 1;
}
/*
 * Join a multicast grp group on upstream interface ifa.
 */  

int 
k_join_src(int socket, struct in6_addr * grp, struct in6_addr * m_source, u_int16_t ifindex)
{  
	struct group_source_req gsreq;
	IF_DEBUG(DEBUG_IF)
	log_msg(LOG_DEBUG, 0, "Going to join using (S,G)=(%s,%s) on interface %s",
			inet6_fmt(grp),inet6_fmt(m_source), ifindex2str(ifindex));

	memset(&gsreq, 0, sizeof(gsreq));
	gsreq.gsr_interface = ifindex;

	gsreq.gsr_group.ss_family=PF_INET6;
	gsreq.gsr_group.__ss_align =0;
	memcpy(&gsreq.gsr_group.__ss_padding, grp->__in6_u.__u6_addr8, 16 );


	gsreq.gsr_source.ss_family=PF_INET6;
	gsreq.gsr_source.__ss_align =0;
	memcpy(&gsreq.gsr_source.__ss_padding, m_source->__in6_u.__u6_addr8, 16);

	if (setsockopt(socket, IPPROTO_IPV6, MCAST_JOIN_SOURCE_GROUP,
				   (char *) &gsreq, sizeof(gsreq)) < 0)
	{
		log_msg(LOG_ALERT, errno, "Cannot join using (S,G)=(%s,%s) on interface %s",
				inet6_fmt(grp),inet6_fmt(m_source), ifindex2str(ifindex));
		return -1;
	}
	return 1;
}
/*
 * Block a  sender with address m_source multicast grp group on upstream interface ifa. in Exlude mode
 */  

int 
k_block_src(int socket, struct in6_addr * grp, struct in6_addr * m_source, u_int16_t ifindex)
{  
	struct group_source_req gsreq;
	IF_DEBUG(DEBUG_IF)
	log_msg(LOG_DEBUG, 0, "Going to join using (S,G)=(%s,%s) on interface %s",
			inet6_fmt(grp),inet6_fmt(m_source), ifindex2str(ifindex));

	memset(&gsreq, 0, sizeof(gsreq));
	gsreq.gsr_interface = ifindex;

	gsreq.gsr_group.ss_family=PF_INET6;
	gsreq.gsr_group.__ss_align =0;
	memcpy(&gsreq.gsr_group.__ss_padding, grp->__in6_u.__u6_addr8, 16 );


	gsreq.gsr_source.ss_family=PF_INET6;
	gsreq.gsr_source.__ss_align =0;
	memcpy(&gsreq.gsr_source.__ss_padding, m_source->__in6_u.__u6_addr8, 16);

	if (setsockopt(socket, IPPROTO_IPV6, MCAST_BLOCK_SOURCE,
				   (char *) &gsreq, sizeof(gsreq)) < 0)
	{
		log_msg(LOG_ALERT, errno, "Cannot BLOCK_SOURCE using ( exclude S,G)=(%s,%s) on interface %s",
				inet6_fmt(grp),inet6_fmt(m_source), ifindex2str(ifindex));
		return -1;
	}
	return 1;
}
/*
 * Cancel BLOCK mode of a souece multicast grp group on upstream interface ifa
 */  

int 
k_unblock_src(int socket, struct in6_addr * grp, struct in6_addr * m_source, u_int16_t ifindex)
{  
	struct group_source_req gsreq;
	IF_DEBUG(DEBUG_IF)
	log_msg(LOG_DEBUG, 0, "Going to join using (S,G)=(%s,%s) on interface %s",
			inet6_fmt(grp),inet6_fmt(m_source), ifindex2str(ifindex));

	memset(&gsreq, 0, sizeof(gsreq));
	gsreq.gsr_interface = ifindex;

	gsreq.gsr_group.ss_family=PF_INET6;
	gsreq.gsr_group.__ss_align =0;
	memcpy(&gsreq.gsr_group.__ss_padding, grp->__in6_u.__u6_addr8, 16 );


	gsreq.gsr_source.ss_family=PF_INET6;
	gsreq.gsr_source.__ss_align =0;
	memcpy(&gsreq.gsr_source.__ss_padding, m_source->__in6_u.__u6_addr8, 16);

	if (setsockopt(socket, IPPROTO_IPV6, MCAST_UNBLOCK_SOURCE,
				   (char *) &gsreq, sizeof(gsreq)) < 0)
	{
		log_msg(LOG_ALERT, errno, "Cannot UNBLOCK_SOURCE using ( exclude S,G)=(%s,%s) on interface %s",
				inet6_fmt(grp),inet6_fmt(m_source), ifindex2str(ifindex));
		return -1;
	}
	return 1;
}

/*
 * Leave a multicast  group on local interface ifa.
 */  

int 
k_leave_src(int socket, struct in6_addr * grp, struct in6_addr * m_source, u_int16_t ifindex)
{

	struct group_source_req gsreq;
	IF_DEBUG(DEBUG_IF)
	log_msg(LOG_DEBUG, 0, "Going to remove join of  (S,G)=(%s,%s) on interface %s",
			inet6_fmt(grp),inet6_fmt(m_source), ifindex2str(ifindex));

	memset(&gsreq, 0, sizeof(gsreq));
	gsreq.gsr_interface = ifindex;

	gsreq.gsr_group.ss_family=PF_INET6;
	gsreq.gsr_group.__ss_align =0;
	memcpy(&gsreq.gsr_group.__ss_padding, grp->__in6_u.__u6_addr8, 16 );


	gsreq.gsr_source.ss_family=PF_INET6;
	gsreq.gsr_source.__ss_align =0;
	memcpy(&gsreq.gsr_source.__ss_padding, m_source->__in6_u.__u6_addr8, 16);

	if (setsockopt(socket, IPPROTO_IPV6, MCAST_LEAVE_SOURCE_GROUP,
				   (char *) &gsreq, sizeof(gsreq)) < 0)
	{
		log_msg(LOG_WARNING,errno,  "Cannot leave using (S,G)=(%s,%s) on interface %s",
				inet6_fmt(grp), inet6_fmt(m_source), ifindex2str(ifindex)) ;
		return -1;
	}
	return 1;
}


/*
 * Join a multicast grp group on local interface ifa.
 */  

int 
k_join(int socket, struct in6_addr * grp, u_int16_t ifindex)
{
	struct ipv6_mreq mreq6;

	mreq6.ipv6mr_multiaddr = *grp;

	mreq6.ipv6mr_interface = ifindex;

	if (setsockopt(socket, IPPROTO_IPV6, IPV6_JOIN_GROUP,
				   (char *) &mreq6, sizeof(mreq6)) < 0)
	{
		log_msg(LOG_ERR, errno, "Cannot join group %s on interface %s",
				inet6_fmt(grp), ifindex2str(ifindex));
		return -1;
	}
	return 1;
}

/*
 * Leave a multicats grp group on local interface ifa.
 */  

int 
k_leave(int socket, struct in6_addr * grp, u_int16_t ifindex)
{
	struct ipv6_mreq mreq6;

	mreq6.ipv6mr_multiaddr = *grp;

	mreq6.ipv6mr_interface = ifindex;

	if (setsockopt(socket, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
				   (char *) &mreq6, sizeof(mreq6)) < 0)
	{
		log_msg(LOG_INFO, errno, "Warning - Cannot perform leave group %s on interface N%d=%s"
				"Perhaps SSM leave was done before group leave",  // Possible cause -  leave_src was done already so the
				inet6_fmt(grp), ifindex, mvifs[ifindex].mvif_name);
		return -1;
	}
	return 1;
}

/* 
 * Add a virtual interface in the kernel.
 */

int 
k_add_vif(int socket, mifi_t vifi, struct mvif * v)
{
	struct mif6ctl  mc;
	memset(&mc, 0, sizeof(mc));
	mc.mif6c_mifi = vifi;
	mc.mif6c_flags = v->uv_flags;
	mc.mif6c_pifi = v->uv_ifindex;

	// first delete interface as a precaution for a repetetive runs without propeper cleanup
	setsockopt(socket, IPPROTO_IPV6, MRT6_DEL_MIF,
			   (char *) &vifi, sizeof(vifi));


	if (setsockopt(socket, IPPROTO_IPV6, MRT6_ADD_MIF,
				   (char *) &mc, sizeof(mc)) < 0)
	{
		log_msg(LOG_ERR, errno, "setsockopt MRT6_ADD_MIF on mif %d", vifi);
		return -1;
	}
	return 1;
}

/*
 * Delete a virtual interface in the kernel.
 */

int 
k_del_vif(int socket, mifi_t vifi)
{
	if (setsockopt(socket, IPPROTO_IPV6, MRT6_DEL_MIF,
				   (char *) &vifi, sizeof(vifi)) < 0)
	{
		log_msg(LOG_ERR, errno, "setsockopt MRT6_DEL_MIF on mif %d", vifi);
		return -1;
	}
	return 1;
}

/*
 * Delete all MFC entries for particular routing entry from the kernel.
 */  

/*
 *  See /procc/net/ip6_mr_vif, /proc/net/ip6_mr_cache

 * sometims repetetive run of CPE to the same address did nor produce traffic because MFC cache need some time to
 clean, the /proc/net/ip6_mr_cache shows for 4-5 sec, then it clears and the mcastread succeeds 
 ff1e:0000:0000:0000:0000:0000:0000:0100 fe80:0000:0000:0000:0250:f1ff:fe08:b663 -1  0        0        0
 */

/* Delete one of the routes for the group, i.e remove route to one of downstream interfaces */

int k_add_to_mfc6(int socket, struct sockaddr_in6 *origin, /* sender of multicast*/ 
	 				struct sockaddr_in6 *mcastgrp,  /* multicast address - the destination */
	 				mifi_t in,  /* from where it come - upstream , interface N in /proc/net/ip6_mr_vif */
         			struct if_set *out /*set of inteface to route to */
					)
{
	struct mf6cctl mf6c;
	int rc;

	if (socket < 0)
	{
		log_msg(LOG_ERR, errno, " multicast routing is not enabled ");
	}
	memset(&mf6c, 0, sizeof(mf6c));
	memcpy(&mf6c.mf6cc_origin, origin, sizeof(mf6c.mf6cc_origin)); // ??? will IPv6 any address
	memcpy( &mf6c.mf6cc_mcastgrp, mcastgrp, sizeof(mf6c.mf6cc_mcastgrp));
	mf6c.mf6cc_parent = in;

	if ( out != NULL)
	{
		mf6c.mf6cc_ifset =*out;
		//memcpy( &mf6c.mf6cc_ifset , out,  sizeof (*out));
	} else
	{
		IF_DEBUG(DEBUG_MFC)
		log_msg(LOG_DEBUG, 0, "%s(): to set NEGATIVE MFC cache routing for (G,S)=(%s,%s) ",
				__func__, 
				sa6_fmt(mcastgrp),
				sa6_fmt(origin));
	}
	IF_DEBUG(DEBUG_MFC)
	{
		log_msg(LOG_DEBUG, 0, "Asking kernel setsockopt to add MFC  MFC  (G,S)=(%s,%s) ",
				sa6_fmt(&mf6c.mf6cc_mcastgrp),
				sa6_fmt(&mf6c.mf6cc_origin));
		}
        // kernel does not keep reference count for mfc cache, several ADD_MFCs may be cleared with one DEL_MFC
        if ( (rc=setsockopt(socket, IPPROTO_IPV6, MRT6_ADD_MFC, &mf6c, sizeof(mf6c)))<  0)
		{
                log_msg(LOG_ERR, errno, " error adding MFC entry for  MFC  (G,S)=(%s,%s) ",
		     						   sa6_fmt(mcastgrp),
										sa6_fmt(origin));
		
        }
        return rc;
}


/*
 * Delete MFC entry for particular routing entry from the kernel., i.e
 *      from sender to group G to all DS interfaces
 */ 

k_del_from_mfc6(int socket, struct sockaddr_in6 *origin,                /* sender of multicast*/ 
				 struct sockaddr_in6 *mcastgrp            /* multicast address - the destination */
       			  )
{
	struct mf6cctl mf6c;
	int  rc;


	if (socket < 0)	// Sanity
	{
		log_msg(LOG_ERR, errno, " multicast routing is not enabled/initialized ");
	}

	memset(&mf6c, 0, sizeof(mf6c));
	memcpy(&mf6c.mf6cc_origin, origin, sizeof(mf6c.mf6cc_origin)); // ??? will IPv6 any address
	memcpy( &mf6c.mf6cc_mcastgrp, mcastgrp, sizeof(mf6c.mf6cc_mcastgrp));
	mf6c.mf6cc_parent = upStreamVif;

	IF_DEBUG(DEBUG_KERN)
	{    
		log_msg(LOG_DEBUG, 0, "Asking kernel setsockopt to del MFC caache entry  (G,S)=(%s,%s)",
				sa6_fmt(&mf6c.mf6cc_mcastgrp),
				sa6_fmt(&mf6c.mf6cc_origin));
	}    
	if ( (rc=setsockopt(socket, IPPROTO_IPV6, MRT6_DEL_MFC, &mf6c, sizeof(mf6c))) < 0)
	// Perhaps SSM leave/MFC clean  was done before group leave",  
	{
		log_msg(LOG_DEBUG, errno, " error deleting MFC enrty  (G,S)=(%s,%s)",sa6_fmt(mcastgrp),	// may be caused if no MFC cache was created before
				sa6_fmt(origin));
	}
	return rc;
}

/*
* k_get_sg_cnt()  -find out if MFC cache entry exists
 * Gets the number of packets, bytes, and number of packets arrived on wrong
 * if in the kernel for particular (S,G) entry.
 */

int
k_get_sg_cnt(int socket, struct sockaddr_in6 * source, struct sockaddr_in6 * group )
{
	struct sioc_sg_req6 sgreq;

	sgreq.src = *source;
	sgreq.grp = *group;
	if (ioctl(socket, SIOCGETSGCNT_IN6, (char *) &sgreq) < 0)
	{
		log_msg(LOG_WARNING, errno, "SIOCGETSGCNT_IN6 on (%s %s)",
				sa6_fmt(source), sa6_fmt(group));
		return(0);
	}
	return(1);	//if no error - G,S exixts
}
