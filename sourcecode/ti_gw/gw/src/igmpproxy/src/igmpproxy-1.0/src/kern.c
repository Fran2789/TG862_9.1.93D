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

#ifdef HAVE_CONFIG_H
#include <../include/config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>

#include <errno.h>

#include <string.h>
#include "defs.h"

#include "vif.h"

#include "inet6.h"
#include "debug.h"
#include "kern.h"
#include "groups.h"
#include "mcast_proto.h"

struct ip_mreqn_proxy {
    struct in_addr  imr_multiaddr;          /* IP multicast address of group */
    struct in_addr  imr_address;            /* local IP address of interface */
    int             imr_ifindex;            /* Interface index */
};
#define ip_mreqn ip_mreqn_proxy
struct vifctl_proxy {
    vifi_t	vifc_vifi;		/* Index of VIF */
    unsigned char vifc_flags;	/* VIFF_ flags */
    unsigned char vifc_threshold;	/* ttl limit */
    unsigned int vifc_rate_limit;	/* Rate limiter values (NI) */
    union {
        struct in_addr vifc_lcl_addr;     /* Local interface address */
        int            vifc_lcl_ifindex;  /* Local interface index   */
    };
    struct in_addr vifc_rmt_addr;	/* IPIP tunnel addr */
};
#define vifctl vifctl_proxy
int process_cache_miss(  u_int16_t ifindex, mifi_t mifi, uint32_t source, uint32_t  group)
{
    struct listaddr * g_wan, *listsrc;

    if_set zero;


    IF_ZERO (&zero);



    /*
     * When there is a cache miss, we check only the header of the packet
     * (and only it should be sent up by the kernel.)
     *
     */
    /*iif = im->im_mif;
     * /proc/net/ip_mr_vif
     */




    if ( mifi == upStreamVif )
    {
        IF_DEBUG(DEBUG_KERN)
        log_msg(LOG_DEBUG, 0, "Cache miss from  upStreamVif interface #%dv%d:%s  for (G,S)=(%s,%s)",
                ifindex, mifi, mvifs[mifi].mvif_name,
                inetFmt(group),
                inetFmt(source));
    }
    else
    {
        IF_DEBUG(DEBUG_KERN)
        log_msg(LOG_DEBUG, 0, "Cache miss  from non-upstream interface, ignoring ");
        return -1;
    }

    /*
     * find group in the upstream interface group list
     */
    g_wan = find_multicast_group(&mvifs[upStreamVif], group);

    if (g_wan == NULL )
    {
        IF_DEBUG(DEBUG_KERN)
        log_msg(LOG_DEBUG, 0, "Unsolicit multicast received : cache_miss  but group is not found in upstream  interface %s list, exiting ",
                mvifs[mifi].mvif_name);
        return -1;
    }

    //  if ( ! ( g_wan->transmitter ) )
    memcpy(&g_wan->transmitter, &source, sizeof( source ));

    /* XXX. if there are too many cache miss for the same (S,G), install
    * negative cache entry in the kernel (oif==NULL) to prevent too many
    * upcalls. - seems not the case on DOCSIS-3.0 where multicast are filtered by dcid
    */


    /* Install MFC cache entry if group is in mixed SSM/ASM mode */


    if (memcmp (&g_wan->ma_downstream_ifset, &zero, sizeof (if_set)) != 0)
    {
			
       if (g_wan->ma_filter_mode == MODE_IS_EXCLUDE || g_wan->ma_filter_mode == CHANGE_TO_EXCLUDE_MODE)
        {
        k_add_to_mfc( source, /* sender of multicast*/
                      group,  /* multicast address - the destination */
                      upStreamVif,  /* from where it come - upstream */
                      &(g_wan->ma_downstream_ifset) /*set of the DS interfaces to route to */
                    );
         }
        IF_DEBUG(DEBUG_MFC)
        log_msg(LOG_DEBUG, 0, "Adding MFC entry for group (G)=%s from  multicast Sender=%s",
                inetFmt(group),
                inetFmt(source));
    }

    /* Reinstall SSM entries */
    listsrc =g_wan->sources;
    while (listsrc)
    {
        if (memcmp (&listsrc->ma_downstream_ifset, &zero, sizeof (if_set)) != 0)
        {
            if ( source != g_wan->transmitter )
            {
                k_add_to_mfc( source, /* sender of multicast*/
                              group,  /* multicast address - the destination */
                              upStreamVif,  /* from where it come - upstream */
                              &(listsrc->ma_downstream_ifset) /*set of the DS interfaces to route to */
                            );
                IF_DEBUG(DEBUG_MFC)
                log_msg(LOG_DEBUG, 0, "Adding MFC for Sources of Group %s Source=%s from  multicast Sender=%s",
			inetFmt(group),
                        inetFmt(listsrc->mcast_group),
                        inetFmt(source));
            }
        }

        if (listsrc->ma_next == NULL)
            break;
        listsrc=listsrc->ma_next;
    }

    return 0;
}



int k_hdr_include(int hdrincl)
{
    int rc=setsockopt(IgmpSocket, IPPROTO_IP, IP_HDRINCL,
                      (char *)&hdrincl, sizeof(hdrincl));
    if (rc  < 0)
        log_msg(LOG_ERR, errno, "setsockopt IP_HDRINCL %u", hdrincl);
    return rc;
}
int curttl=0; /* TODO buildIgmp ? */

int k_set_ttl(int t) {

    int ttl;
    int rc;

    curttl=ttl = t;
    rc=setsockopt(IgmpSocket, IPPROTO_IP, IP_MULTICAST_TTL,
                  (char *)&ttl, sizeof(ttl));
    if (rc < 0)
    {
        log_msg(LOG_ERR, errno, "setsockopt IP_MULTICAST_TTL %u", ttl);
    }
    return rc;
}


int k_set_loop(int l)
{
    int loop;
    int rc;
    loop = l;

    rc=setsockopt(IgmpSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
                  (char *)&loop, sizeof(loop));
    if (rc < 0)
        log_msg (LOG_ERR, errno, "setsockopt IP_MULTICAST_LOOP %u", loop);
    return rc;
}
/*
 * Set the socket receiving buffer. `bufsize` is the preferred size,
 * `minsize` is the smallest acceptable size.
 */

int k_set_rcvbuf( int bufsize, int minsize)
{
    int             delta = bufsize / 2;
    int             iter = 0;

    /*
     * Set the socket buffer.  If we can't set it as large as we
     * want, search around to try to find the highest acceptable
     * value.  The highest acceptable value being smaller than
     * minsize is a fatal error.
     */


    if (setsockopt(IgmpSocket, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) < 0)
    {
        bufsize -= delta;
        while (1)
        {
            iter++;
            if (delta > 1)
                delta /= 2;
            if (setsockopt(IgmpSocket, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) < 0)
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
    log_msg(LOG_DEBUG,0,"Buffer reception size for socket %d : %d in %d iterations",IgmpSocket, bufsize, iter);
    return 1;
}

/*
 * Set the default Hop Limit for the multicast packets outgoing from this
 * socket.
 */

int
k_set_hlim(  u_int8_t hop_limit )
{
    int             hlim = hop_limit;
    int rc;

    rc=setsockopt(IgmpSocket, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &hlim, sizeof(hlim));
    if (rc < 0)
    {
        log_msg(LOG_ERR,errno,"k_set_hlim");
        return rc;
    }

    return rc;
}


/*
 * Set the IP_MULTICAST_IF option on local interface which has the
 * specified index.
 */

int
k_set_if(int the_socket, u_int16_t mifi)
{
    struct mvif * v= &mvifs[mifi];
    struct ip_mreqn mreq;

    memset(&mreq, 0 , sizeof(mreq));
    mreq.imr_address=v->InAdr;
    mreq.imr_ifindex=v->uv_ifindex;


    int rc=setsockopt(the_socket, IPPROTO_IP, IP_MULTICAST_IF,
                      (void *) &mreq, sizeof(mreq));
    if (rc < 0)
    {
        log_msg(LOG_WARNING, errno, "setsockopt IP_MULTICAST_IF for interface #%d : %s",
                v->uv_ifindex , v->mvif_name);
        return rc;
    }

    return rc;
}

int
k_unset_if(int the_socket, u_int16_t mifi)
{
    struct mvif * v= &mvifs[mifi];
    struct ip_mreqn mreq;
    struct in_addr any;

    any.s_addr=htonl(INADDR_ANY);

    memset(&mreq, 0 , sizeof(mreq));
    mreq.imr_address=any;
    mreq.imr_ifindex=v->uv_ifindex;


    int rc=setsockopt(the_socket, IPPROTO_IP, IP_MULTICAST_IF,
                      (void *) &mreq, sizeof(mreq));
    if (rc < 0)
    {
        log_msg(LOG_WARNING, errno, "setsockopt UNSET IP_MULTICAST_IF for interface #%d : %s",
                v->uv_ifindex , v->mvif_name);
        return rc;
    }

    return rc;
}

#include <linux/igmp.h>
/*
/*
 * Join a multicast grp group on upstream interface ifa.
 */
int
k_join_src(int the_socket, uint32_t grp,uint32_t   m_source, u_int16_t ifindex)
{
    struct ip_mreq_source mreqs;

    IF_DEBUG(DEBUG_IF)
    log_msg(LOG_DEBUG, 0, "Going to join using (S,G)=(%s,%s) on interface %s",
            inetFmt(grp),inetFmt(m_source), ifindex2str(ifindex));

    memset(&mreqs, 0, sizeof(mreqs));

    mreqs.imr_multiaddr.s_addr = grp;
    mreqs.imr_sourceaddr.s_addr = m_source;
    mreqs.imr_interface = mvifs[upStreamVif].InAdr;



    if (setsockopt(the_socket, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
                   (void *) &mreqs, sizeof(mreqs)) < 0)
    {
       log_msg(LOG_WARNING, errno, "Cannot join ");
        return -1;
    }
    return 1;
}
/*
 * Block a  sender with address m_source multicast grp group on upstream interface ifa. in Exlude mode
 */

int
k_block_src(int the_socket, uint32_t grp,uint32_t   m_source, u_int16_t ifindex)
{
    struct ip_mreq_source mreqs;
    IF_DEBUG(DEBUG_IF)
    log_msg(LOG_DEBUG, 0, "Going to join using (S,G)=(%s,%s) on interface %s",
            inetFmt(grp),inetFmt(m_source), ifindex2str(ifindex));

    memset(&mreqs, 0, sizeof(mreqs));
    mreqs.imr_multiaddr.s_addr = grp;
    mreqs.imr_sourceaddr.s_addr = m_source;
    mreqs.imr_interface = mvifs[upStreamVif].InAdr;
    if (setsockopt(the_socket, IPPROTO_IP, IP_BLOCK_SOURCE ,
                   (void *) &mreqs, sizeof(mreqs)) < 0)
    {
       log_msg(LOG_WARNING, errno, "Cannot BLOCK_SOURCE on interface ");

        return -1;
    }
    return 1;
}
/*
 * Cancel BLOCK mode of a souece multicast grp group on upstream interface ifa
 */

int
k_unblock_src(int the_socket, uint32_t grp, uint32_t m_source, u_int16_t ifindex)
{
    struct ip_mreq_source mreqs;
    IF_DEBUG(DEBUG_IF)
    log_msg(LOG_DEBUG, 0, "Going to join using (S,G)=(%s,%s) on interface %s",
            inetFmt(grp),inetFmt(m_source), ifindex2str(ifindex));


    memset(&mreqs, 0, sizeof(mreqs));
    mreqs.imr_multiaddr.s_addr = grp;
    mreqs.imr_sourceaddr.s_addr = m_source;
    mreqs.imr_interface  = mvifs[upStreamVif].InAdr;
    if (setsockopt(the_socket, IPPROTO_IP, IP_UNBLOCK_SOURCE ,
                   (void *) &mreqs, sizeof(mreqs)) < 0)
    {
        log_msg(LOG_WARNING, errno, "Cannot UNBLOCK_SOURCE  on interface");

        return -1;
    }
    return 1;
}

/*
 * Leave a multicast  group on local interface ifa.
 */

int
k_leave_src(int the_socket, uint32_t grp,uint32_t   m_source, u_int16_t ifindex)
{
    struct ip_mreq_source mreqs;

    IF_DEBUG(DEBUG_IF)
    log_msg(LOG_DEBUG, 0, "Going to remove join of  (S,G)=(%s,%s) on interface %s",
            inetFmt(grp),inetFmt(m_source), ifindex2str(ifindex));

    memset(&mreqs, 0, sizeof(mreqs));
    mreqs.imr_multiaddr.s_addr = grp;
    mreqs.imr_sourceaddr.s_addr = m_source;
    mreqs.imr_interface = mvifs[upStreamVif].InAdr;
    if (setsockopt(the_socket, IPPROTO_IP,  IP_DROP_SOURCE_MEMBERSHIP ,
                   (void *) &mreqs, sizeof(mreqs)) < 0)

    {
        log_msg(LOG_WARNING,errno,  "Cannot leave  on interface");
        return -1;
    }
    return 1;
}



void
k_add_to_mfc( uint32_t src, uint32_t  dst, mifi_t in,
              struct if_set *out)
{
    struct mfcctl mfc;
    int i;

    memset( &mfc, 0, sizeof(mfc));
    memcpy( &mfc.mfcc_origin, &src, sizeof(src));
    memcpy( &mfc.mfcc_mcastgrp, &dst, sizeof(dst));
    mfc.mfcc_parent = in;
    for (i = 0; i < MAXVIFS; i++) {
        if (IF_ISSET(i, out))
            mfc.mfcc_ttls[i] = 1;
        else
            mfc.mfcc_ttls[i] = 0;
    }
    if (setsockopt(IgmpSocket, IPPROTO_IP, MRT_ADD_MFC, &mfc, sizeof(mfc)) < 0) {
        log_msg(LOG_ERR, errno, "MRT_ADD_MFC (G, S)=(%s, %s)", inetFmt(dst),
                inetFmt(src));
    }
}

void
k_make_negative_mfc( uint32_t src, uint32_t  dst, mifi_t in,
                     struct if_set *out)
{
    struct mfcctl mfc;
    int i;

    memset( &mfc, 0, sizeof(mfc));
    memcpy( &mfc.mfcc_origin, &src, sizeof(src));
    memcpy( &mfc.mfcc_mcastgrp, &dst, sizeof(dst));
    mfc.mfcc_parent = in;
    for (i = 0; i < MAXVIFS; i++) {
        if (IF_ISSET(i, out))
            mfc.mfcc_ttls[i] = 1;
        else
            mfc.mfcc_ttls[i] = 0;
        if ( i == in )
            mfc.mfcc_ttls[i] = 0;
    }
    if (setsockopt(IgmpSocket, IPPROTO_IP, MRT_ADD_MFC, &mfc, sizeof(mfc)) < 0) {
        log_msg(LOG_ERR, errno, "k_make_negative_mfc MRT_ADD_MFC (G, S)=(%s, %s)", inetFmt(dst),
                inetFmt(src));
    }
}
void
k_del_from_mfc(uint32_t  src, uint32_t  dst, mifi_t ds_lan,
               struct if_set *out)
{
    struct mfcctl mfc;
    int i;
    struct if_set zero;

    IF_ZERO(&zero);

    IF_DEBUG(DEBUG_MFC)
    log_msg(LOG_DEBUG, 0, "k_del_from_mfc  (G, Sender)=(%s, %s)", inetFmt(dst),
            inetFmt(src));

    if (memcmp (out, &zero, sizeof (if_set)) == 0)
    {
        return;
    }
    if (src == 0 )
    {
        IF_DEBUG(DEBUG_MFC)
        log_msg(LOG_DEBUG, 0, "BUG k_del_from_mfc (Sender=src=0) (G, S)=(%s, %s)", inetFmt(dst),
                inetFmt(src));
        return;
    }
    if (src == 0 )
    {
        IF_DEBUG(DEBUG_MFC)
        log_msg(LOG_DEBUG, 0, "BUG k_del_from_mfc (G=0) (G, S)=(%s, %s)", inetFmt(dst),
                inetFmt(src));
        return;
    }
    memset( &mfc, 0, sizeof(mfc));
    memcpy( &mfc.mfcc_origin, &src, sizeof(src));
    memcpy( &mfc.mfcc_mcastgrp, &dst, sizeof(dst));
    mfc.mfcc_parent = upStreamVif;
    for (i = 0; i < MAXVIFS; i++) {
        if (IF_ISSET(i, out) ) // XXX ??? && ( i !=  ds_lan))
            mfc.mfcc_ttls[i] = 1;
        else
            mfc.mfcc_ttls[i] = 0;
    }
    if (setsockopt(IgmpSocket, IPPROTO_IP, MRT_DEL_MFC, &mfc, sizeof(mfc)) < 0) {
        log_msg(LOG_DEBUG, errno, "MRT_DEL_MFC (G, S)=(%s, %s)", inetFmt(dst),
                inetFmt(src));
    }
}


#include <linux/mroute.h>

int     k_add_vif     (int the_socket, mifi_t vifi, struct mvif *v)
{

    struct vifctl  CtlReq;
    int rc;

    memset(&CtlReq, 0, sizeof(CtlReq));
    CtlReq.vifc_vifi = vifi;
    CtlReq.vifc_flags |=VIFF_USE_IFINDEX;
    CtlReq.vifc_lcl_ifindex=v->uv_ifindex;


    rc = setsockopt( IgmpSocket, IPPROTO_IP, MRT_ADD_VIF,
                     (void *)&CtlReq, sizeof( CtlReq ) );
    if (rc <0 )
        log_msg( LOG_WARNING, errno, "MRT_ADD_VIF" );

    return rc;

}
int k_set_packetinfo(void )
{
    int on=1;
    int rc;

    rc=setsockopt (IgmpSocket, IPPROTO_IP, IP_PKTINFO, &on, /* allows to figure out input interface index, ... */
                   sizeof (on));
    if (rc < 0)
        log_msg (LOG_ERR, errno, "setsockopt(IPV6_PKTINFO)");

    return rc;
}


int     k_del_vif       (int the_socket, mifi_t vifi)
{

    struct vifctl  CtlReq;
    int rc;

    memset(&CtlReq, 0, sizeof(CtlReq));
    CtlReq.vifc_vifi = vifi;
    CtlReq.vifc_flags |=VIFF_USE_IFINDEX;
    CtlReq.vifc_lcl_ifindex =mvifs[vifi].uv_ifindex;


    rc = setsockopt( IgmpSocket, IPPROTO_IP, MRT_DEL_MFC,
                     (void *)&CtlReq, sizeof( CtlReq ) );
    if (rc < 0 )
        log_msg( LOG_WARNING, errno, "MRT_DEL_MFC" );

    return rc;

}
/*
 * Join a multicast grp group on local interface ifa.
 */
int
k_join(int the_socket, uint32_t   mcastaddr, u_int16_t ifindex)
{
    struct ip_mreqn_proxy CtlReq;
    int rc;

    if (  find_vif_by_ifindex(ifindex) == NOVIF )   /* Sanity */
    {
        /* proxy also joins all_routers group on LAN interfaces */
        log_msg(LOG_ERR, errno, "Group %s : Invalid interface index for JOIN %d",
                inetFmt(mcastaddr), ifindex);
        return -1;
    }

    memset(&CtlReq, 0, sizeof(CtlReq));
    CtlReq.imr_multiaddr.s_addr = mcastaddr;
    /*CtlReq.imr_address. = mvifs[mifi].InAdr; */
    CtlReq.imr_ifindex = ifindex;  /* physical interface */


    rc=setsockopt(the_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                  (void *)&CtlReq, sizeof( CtlReq ) );

    if (rc < 0)
    {
        log_msg(LOG_WARNING, errno, "Cannot join group %s on interface ",inetFmt(mcastaddr));
   
    }
    return rc;
}

/*
 * Leave a multicats grp group on local interface ifa.
 */

int k_leave(int the_socket,uint32_t   mcastaddr, u_int16_t ifindex)
{
    struct ip_mreqn_proxy CtlReq;
    int rc;

    if (  find_vif_by_ifindex(ifindex) == NOVIF )   /* Sanity */
    {
        log_msg(LOG_ERR, errno, "Group %s : Invalid interface index for LEAVE %d",
                inetFmt(mcastaddr), ifindex);
        return -1;
    }
    memset(&CtlReq, 0, sizeof(CtlReq));
    CtlReq.imr_multiaddr.s_addr = mcastaddr;
    /*CtlReq.imr_address. = mvifs[mifi].InAdr; */
    CtlReq.imr_ifindex = ifindex;


    rc=setsockopt(the_socket, IPPROTO_IP,IP_DROP_MEMBERSHIP ,
                  (void *)&CtlReq, sizeof( CtlReq ));

    if (rc < 0 )
    {
        log_msg(LOG_WARNING, errno, "Cannot leave group  on interface");
      
    }
    return rc;
}

