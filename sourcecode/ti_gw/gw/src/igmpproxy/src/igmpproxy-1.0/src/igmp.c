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
*   igmp.h - Recieves IGMP requests, and handle them
*            appropriately...
*/
#include <stdlib.h>

#include "defs.h"
#include "vif.h"
#include "debug.h"
#include "kern.h"
#include "mcast_proto.h"
#include "inet6.h"
#include "igmpproxy.h"
#include "os-linux.h"
#include <linux/igmp.h>
//#include "igmpv2_proro.h"
#include "igmpv3_proto.h"
#include "timers.h"
#include "groups.h"
/* Globals                  */
uint32_t     allhosts_group;          /* All hosts addr in net order */
uint32_t     allrouters_group;          /* All hosts addr in net order */

int IgmpSocket;
int IgmpProxySocket;



//static uint16_t inetChksum(uint16_t *addr,  uint16_t len);
static uint16_t inetChecksum(uint16_t *addr, int count);
/* local variables. */

static struct msghdr sndmh, rcvmh;
static struct iovec sndiov[2];
static struct iovec rcviov[2];

static struct sockaddr_in from;
static struct sockaddr_in dst_sa;

static u_char *rcvcmsgbuf = NULL;
static int rcvcmsglen;





char *sndcmsgbuf = NULL;
char * recv_buf;
char * send_buf;
int ctlbuflen = 0;
static u_int16_t rtalert_code;

// ARRIS ADD START
// IGMPv2 message types defined in RFC 2236
// 0x16  - Version 2 Membership report
// 0x17  - Leave Group
#define IGMP_IS_V2_MSG(type)\
    (((type) == IGMP_V2_MEMBERSHIP_REPORT) \
    || ((type) == IGMP_V2_LEAVE_GROUP))

#define IGMP_INCLUDE_ROUTER_ALERT(msg_type, extra_len, max_resp_code)\
    (IGMP_IS_V2_MSG(msg_type) || ((extra_len) >= 4) || (max_resp_code))
// ARRIS ADD END


/* local functions */

static void acceptIgmpMsg(u_int32_t recvlen);
static int enableMRouter(void);
static void disableMRouter(void);
static int openProxySocket(void);

static int openProxySocket(void)
{

    if ( (IgmpProxySocket   = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP)) < 0 )
        log_msg( LOG_ERR, errno, "IGMP socket open" );
    if (setsockopt (IgmpProxySocket ,
                    SOL_SOCKET, SO_BINDTODEVICE,
                    mvifs[upStreamVif].mvif_name, IF_NAMESIZE) < 0)
        log_msg (LOG_ERR, errno, "IGMPproxy socket -BIND TO UPSTREAM");
    return IgmpProxySocket ;

}
/*
** Initialises the mrouted API and locks it by this exclusively.
**
** returns: - 0 if the functions succeeds
**          - the errno value for non-fatal failure condition
*/
static int enableMRouter(void)
{
    int Va = 1;

    if ( (IgmpSocket  = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP)) < 0 )
        log_msg( LOG_ERR, errno, "IGMP socket open" );

    if ( setsockopt( IgmpSocket, IPPROTO_IP, MRT_INIT,
                     (void *)&Va, sizeof( Va ) ) )
        return errno;


    return 0;
}

/*
** Diables the mrouted API and relases by this the lock.
**
*/
static void disableMRouter(void)
{
    if ( setsockopt( IgmpSocket, IPPROTO_IP, MRT_DONE, NULL, 0 )
            || close( IgmpSocket )
       ) {
        IgmpSocket = 0;
        log_msg( LOG_ERR, errno, "MRT_DONE/close" );
    }

    IgmpSocket = -1;
}

/*
 * Open and initialize the igmp socket, and fill in the non-changing
 * IP header fields in the output packet buffer.
 */
void initIgmp()
{
    struct ip *ip;

    openProxySocket();
    enableMRouter();

    recv_buf = malloc(RECV_BUF_SIZE);
    send_buf = malloc(RECV_BUF_SIZE);

    if ( (recv_buf==NULL) || send_buf==NULL )
    {
        log_msg (LOG_ERR, errno, "malloc send/rcv buf failed");
    }
    /* specify to tell receiving interface */



    rcvcmsglen = CMSG_SPACE (sizeof (struct in_pktinfo)) +
                 CMSG_SPACE (sizeof (int));
    if (rcvcmsgbuf == NULL && (rcvcmsgbuf = malloc (rcvcmsglen)) == NULL)
        log_msg (LOG_ERR, 0, "malloc failed");

    /* initialize msghdr for receiving packets */

    rcviov[0].iov_base = (caddr_t) recv_buf;
    rcviov[0].iov_len = RECV_BUF_SIZE;
    rcvmh.msg_name = (caddr_t) & from;
    rcvmh.msg_namelen = sizeof (from);
    rcvmh.msg_iov = rcviov;
    rcvmh.msg_iovlen = 1;
    rcvmh.msg_control = (caddr_t) rcvcmsgbuf;
    rcvmh.msg_controllen = rcvcmsglen;

    /* initialize msghdr for sending packets */
    sndiov[0].iov_base = (caddr_t) send_buf;
    sndmh.msg_namelen = sizeof (struct sockaddr_in);
    sndmh.msg_iov = sndiov;
    sndmh.msg_iovlen = 1;


    if (rcvcmsgbuf == NULL && (rcvcmsgbuf = malloc (rcvcmsglen)) == NULL)
        log_msg (LOG_ERR, 0, "malloc failed");



    k_set_rcvbuf(SO_RECV_BUF_SIZE_MAX, SO_RECV_BUF_SIZE_MIN); /* lots of input buffering        */
    k_set_ttl(1);       /* restrict multicasts to one hop */
    k_set_loop( false);      /* disable multicast loopback     */
    k_set_packetinfo();     /* specify to tell receiving interface */
    k_hdr_include(true);    /* include IP header when sending */

    // Prepare send buffer
    ip         = (struct ip *)send_buf;
    memset(ip, 0, sizeof(struct ip));
    /*
     * Fields zeroed that aren't filled in later:
     * - IP ID (let the kernel fill it in)
     * - Offset (we don't send fragments)
     * - Checksum (let the kernel fill it in)
     */
    ip->ip_v   = IPVERSION;
    //ip->ip_hl  = sizeof(struct ip) >> 2;
	ip->ip_hl  = (sizeof(struct ip)+IP_RAOPT_LEN) >> 2;
    ip->ip_tos = 0xc0;      /* Internet Control */
    ip->ip_ttl = MAXTTL;    /* applies to unicasts only */
    ip->ip_p   = IPPROTO_IGMP;



    allhosts_group   = htonl(INADDR_ALLHOSTS_GROUP);
    allrouters_group = htonl(INADDR_ALLRTRS_GROUP);
}

/*
*   Finds the textual name of the supplied IGMP request.
*/
static const char *igmpPacketKind(u_int8_t type, u_int8_t code) {
    static char unknown[20];

    switch (type) {
    case IGMP_MEMBERSHIP_QUERY:
        return  "Membership query  ";
    case IGMP_V1_MEMBERSHIP_REPORT:
        return "V1 member report  ";
    case IGMP_V2_MEMBERSHIP_REPORT:
        return "V2 member report  ";
    case IGMP_V3_MEMBERSHIP_REPORT:
        return "V3 member report  ";
    case IGMP_V2_LEAVE_GROUP:
        return "Leave message     ";

    default:
        sprintf(unknown, "unk: 0x%02x/0x%02x    ", type, code);
        return unknown;
    }
}


/* Read an IGMP message */
void
msg_read (int socket_fd)
{
    register int mld6_recvlen;

    mld6_recvlen = recvmsg (socket_fd, &rcvmh, 0); /* get  ancillary data - packet src. dst, len */

    if (mld6_recvlen < 0)
    {
        if (errno != EINTR)
            log_msg (LOG_ERR, errno, "MLD6 recvmsg");
        return;
    }

    acceptIgmpMsg (mld6_recvlen);   /* TBD make it as a thread in the future releases */
}


/**
 * Process a newly received IGMP packet that is sitting in the input
 * packet buffer rcvmh.
 */
void acceptIgmpMsg(u_int32_t recvlen)
{
    uint32_t src, dst, group;
    struct ip *ip;
    struct igmp *igmp;
    u_int16_t ipdatalen, iphdrlen;
    int16_t igmpdatalen;
    int16_t mifi=-1;
    int16_t ifindex=-1;
    struct cmsghdr *cm = NULL;
    struct in_pktinfo *pi = NULL;
    struct mvif * vif, *v;

    struct sockaddr_in *src_sa = (struct sockaddr_in *) rcvmh.msg_name;

    if (recvlen < sizeof(struct ip))
    {
        log_msg(LOG_WARNING, 0,
                "%s:received packet too short (%u bytes) even for IP header", __func__, recvlen);
        return;
    }

    //  log_msg(LOG_WARNING, 0,
    //       "received packet with from src address  (%s) for IP header", sa4_fmt(src_sa));


    //ip        = (struct ip *)recv_buf;
    ip        = (struct ip *)rcvmh.msg_iov[0].iov_base;
    src       = ip->ip_src.s_addr;
    dst       = ip->ip_dst.s_addr;

    /* extract optional/ancillary  information via Advanced API */
    cm = (struct cmsghdr *) CMSG_FIRSTHDR (&rcvmh);
    while (cm )
    {
        if ( (cm->cmsg_level == IPPROTO_IP )
                && (cm->cmsg_type == IP_PKTINFO)
                && (cm->cmsg_len == CMSG_LEN (sizeof (struct in_pktinfo)))
           )
        {
            struct in_addr * dst_adr;
            pi = (struct in_pktinfo *) (CMSG_DATA (cm));
            ifindex = pi->ipi_ifindex;
            dst_adr = &pi->ipi_addr;
            IF_DEBUG(DEBUG_IF)
            log_msg(LOG_WARNING, 0,
                    "Interface: #%d : : received packet from %s -> %s for IP header",
                    ifindex,
                    sa4_fmt(src_sa), inet4_fmt(dst_adr)
                   );
        }
        cm = (struct cmsghdr *) CMSG_NXTHDR (&rcvmh, cm);
    }

    mifi = find_vif_by_ifindex( ifindex );

    if ( (mifi  < 0) || (mifi >= MAXMVIFS )) /* ARRIS MOD for arrays index check */
    {
        IF_DEBUG(DEBUG_IF)
        log_msg(LOG_INFO, 0,
                "accept_listener_report: Message coming from interface %d, can't find a vif",
                ifindex);
        return;
    }

    v=vif=&mvifs[mifi];

    if  (vif == NULL ) /* Sanity */
    {
        log_msg(LOG_ERR, 0,
                "BUG :accept_listener_report: Message coming from interface %d, can't find a vif",
                ifindex);
	    return;
    }
    /*
     * this is most likely a message from the kernel indicating that   - MFC cache_miss
     * a new src grp pair message has arrived and so, it would be
     * necessary to install a route into the kernel for this.
     */
    /*
    * Packets sent up from kernel to daemon have ip_p  0.
    */
    if ( ip->ip_p == 0 )
    {
        if (src == 0 || dst == 0)
        {
            log_msg(LOG_WARNING, 0, "kernel request not accurate");
            return;
        }

        IF_DEBUG (DEBUG_KERN)
        {
            log_msg (LOG_DEBUG, 0,
                     "received packet type 0 - CACHE MISS (%u bytes) for interface #%d:%s header",
                     rcvcmsglen, ifindex, ifindex2str(ifindex)
                    );
        }
        /* Activate the route. */
        if ( ifindex == mvifs[upStreamVif].uv_ifindex )
        {
            process_cache_miss (ifindex, upStreamVif, src, dst);
        }
        return;

    }


    iphdrlen  = ip->ip_hl << 2;
    ipdatalen = ip_data_len(ip);

    if (iphdrlen + ipdatalen != recvlen)
    {
        IF_DEBUG (DEBUG_PKT)
        log_msg(LOG_WARNING, 0,
                "Invalid packet received  from %s it is shorter (%u bytes) than hdr+data length (%u+%u)",
                inetFmt(src), recvlen, iphdrlen, ipdatalen);
        return;
    }

    igmp        = (struct igmp *)(recv_buf + iphdrlen);
    group       = dst;
    memcpy(&group, &igmp->igmp_group.s_addr, sizeof (group));
    igmpdatalen = ipdatalen - IGMP_MINLEN;
    if (igmpdatalen < 0)
    {
        IF_DEBUG (DEBUG_PKT)
        log_msg(LOG_WARNING, 0,
                "received IP data field too short (%u bytes) for IGMP, from %s",
                ipdatalen, inetFmt(src));
        return;
    }
    IF_DEBUG (DEBUG_MLD_PROTO)
    log_msg(LOG_DEBUG, 0, "RECV %s  from  src %-15s to dst  %s",
            igmpPacketKind(igmp->igmp_type, igmp->igmp_code),
            inetFmt(src), inetFmt(dst));




    switch (igmp->igmp_type) {
    case IGMP_V1_MEMBERSHIP_REPORT:
	acceptGroupReport(ifindex, mifi, src, group, IGMPv1);
	return;
    case IGMP_V2_MEMBERSHIP_REPORT:
	 log_msg(LOG_WARNING, 0,
                "%s:received   IGMP_V2_MEMBERSHIP_REPORT (%u bytes) even for IP header", __func__, recvlen);
	acceptGroupReport(ifindex, mifi, src, group, IGMPv2);
	return;
   
    case IGMP_V3_MEMBERSHIP_REPORT:
    {
	struct listaddr * lan_group;
	
        acceptV3GroupReport(ifindex, mifi, src, dst, (struct igmpv3_report *) ((void *)(igmp)));
	lan_group=find_multicast_group(vif, dst );
        if (! lan_group)
        {
	    IF_DEBUG(DEBUG_MLD_MEMBER)
            log_msg (LOG_DEBUG, 0,
                     "%s: Got IGMP_V3_MEMBERSHIP_REPORT msg, but group %s does not exist at interface %s", __func__,
                     inetFmt (group), vif->mvif_name);
	    return;
        }
        lan_group->ma_comp_mode =vif->uv_mld_version; /* Provide downgrade if interface was configure for older version */
        return;
    }
    case IGMP_V2_LEAVE_GROUP:
    {
	struct listaddr * lan_group;
	 
	lan_group=find_multicast_group(vif, group );
        if (! lan_group)
        {
	    IF_DEBUG(DEBUG_MLD_MEMBER)
            log_msg (LOG_DEBUG, 0,
                     "%s: Got IGMP_V2_LEAVE_GROUP msg, but group %s does not exist at interface %s", __func__,
                     inetFmt (group), vif->mvif_name);
	    return;
        }
        if (  lan_group->ma_comp_mode == IGMPv1 )
	{
	    IF_DEBUG(DEBUG_MLD_PROTO)
            log_msg (LOG_DEBUG, 0,
                     "%s: IGMPv1 mode -> Ignoring IGMP_V2_LEAVE_GROUP msg for Group %s at interface %s", __func__,
                     inetFmt (group), vif->mvif_name);
            return; /* Ignore Leave for IGPMv1 as RFC requires */
	}
	
        acceptLeaveMessage(ifindex, mifi, src, group);

        return;
    }
    case IGMP_MEMBERSHIP_QUERY:
        return;

    default:
        IF_DEBUG (DEBUG_MLD_PROTO)
        log_msg(LOG_INFO, 0,
                "Ignoring unknown IGMP message type %x from src %s to dst %s",
                igmp->igmp_type, inetFmt(src),
                inetFmt(dst));
        return;
    }
}


/*
 * Construct an IGMP message in the output packet buffer.  The caller may
 * have already placed data in that buffer, of length 'datalen'.
 */
void buildIgmp(uint32_t src, uint32_t dst, int type, int code, uint32_t group, uint16_t datalen) {
    struct ip *ip;
    struct igmp *igmp;
    extern int curttl;
    // ARRIS ADD START
    char *p;
    int iplen = MIN_IP_HEADER_LEN;
    // ARRIS ADD END

    ip                      = (struct ip *)send_buf;
    ip->ip_src.s_addr       = src;
    ip->ip_dst.s_addr       = dst;

    // ARRIS ADD START
    // for IGMPv2/IGMPv3 messages, inlude Router Alert option in IP header
    if (IGMP_INCLUDE_ROUTER_ALERT(type, datalen, code))
    {
        /**************************************************
         Router Alert option, RFC 2113
         +--------+--------+--------+--------+
         |10010100|00000100| 2 octet value(0)|
         +--------+--------+--------+--------+        
        **************************************************/
        p = ((char*)send_buf + sizeof(struct ip));
        *p++ = 0x94;
        *p++ = 0x04;
        *p++ = 0x00;
        *p++ = 0x00;
        iplen += ROUTER_ALERT_OPTION_LEN;
        // make ip header length, includes Router Alert option
        ip->ip_hl = (sizeof (struct ip) + ROUTER_ALERT_OPTION_LEN) >> 2;
    }
    else
    {
        ip->ip_hl = (sizeof(struct ip)) >> 2;
    }
    // ARRIS ADD END
    
    ip_set_len(ip, iplen + IGMP_MINLEN + datalen); // ARRIS MOD, inlucde Router Alert option

    if (IN_MULTICAST(ntohl(dst))) {
        ip->ip_ttl = curttl;
    } else {
        ip->ip_ttl = MAXTTL;
    }

    igmp                    = (struct igmp *)(send_buf + iplen); // ARRIS MOD
    igmp->igmp_type         = type;
    igmp->igmp_code         = code;
    igmp->igmp_group.s_addr = group;
    igmp->igmp_cksum        = 0;
    igmp->igmp_cksum        = inetChecksum((u_int16_t *)igmp, IGMP_MINLEN + datalen);
}

/*
 * Call build_igmp() to build an IGMP message in the output packet buffer.
 * Then send the message from the interface with IP address 'src' to
 * destination 'dst'.
 */
int sendIgmp(uint16_t mifi ,uint32_t src, uint32_t dst, int type, int code, uint32_t group, uint16_t datalen) {
    struct sockaddr_in sdst;
    int rc, setloop = 0, setigmpsource = 0;
    // ARRIS ADD START
    // recalculate ip header length after including Router Alert option
    int iplen;
    iplen = MIN_IP_HEADER_LEN;
    if (IGMP_INCLUDE_ROUTER_ALERT(type, datalen, code))
    {
        iplen += ROUTER_ALERT_OPTION_LEN;
    }
    // ARRIS ADD END
    
    buildIgmp(src, dst, type, code, group, datalen);

    if (IN_MULTICAST(ntohl(dst))) {
        /* In order too provide message outgoes to specific interface, bind k_set_if before sending  */
        k_set_if( IgmpSocket, mifi);
        setigmpsource = 1;
        if (type != IGMP_DVMRP || dst == allhosts_group) {
            setloop = 1;
            k_set_loop(false);
        }
    }

    memset(&sdst, 0, sizeof(sdst));
    sdst.sin_family = AF_INET;
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sdst.sin_len = sizeof(sdst);
#endif
    sdst.sin_addr.s_addr = dst;
    rc=sendto(IgmpSocket, send_buf,
              iplen + IGMP_MINLEN + datalen, 0, // ARRIS MOD
              (struct sockaddr *)&sdst, sizeof(sdst));

    if ( rc < 0) {
        if (errno == ENETDOWN)
        {
            log_msg(LOG_ERR, errno, "Sender VIF was down.");
        }
        else
        {
            IF_DEBUG (DEBUG_PKT)
            log_msg(LOG_INFO, errno,
                    "sendto to %s on %s",
                    inetFmt(dst), inetFmt(src));
        }
    }


    IF_DEBUG (DEBUG_MLD_PROTO)
    log_msg(LOG_DEBUG, 0, "SENT %s from %-15s to %s",
            igmpPacketKind(type, code), src == INADDR_ANY ? "INADDR_ANY" :
            inetFmt(src), inetFmt(dst));
    k_unset_if( IgmpSocket, mifi);

    return rc;
}
/*
 * inet_cksum extracted from:
 *                      P I N G . C
 *
 * Author -
 *      Mike Muuss
 *      U. S. Army Ballistic Research Laboratory
 *      December, 1983
 * Modified at Uc Berkeley
 *
 * (ping.c) Status -
 *      Public Domain.  Distribution Unlimited.
 *
 *                      I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
#if 0
uint16_t inetChksum(uint16_t *addr,  uint16_t len)
{
    register uint16_t nleft = len;
    register uint16_t *w = addr;
    uint16_t answer = 0;
    register uint32_t sum = 0;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1) {
        *(uint8_t *) (&answer) = *(uint8_t *)w ;
        sum += answer;
    }

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return(answer);
}
#endif

/* Taken from http://tools.ietf.org/html/rfc1071 */
uint16_t inetChecksum(uint16_t *addr, int count)
{
    uint32_t sum = 0;
    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }

    if ( count > 0 )
        sum += * (uint8_t *) addr;

    while (sum>>16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}
