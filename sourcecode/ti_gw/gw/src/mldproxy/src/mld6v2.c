/*
 * Copyright (C) 1999 LSIIT Laboratory.
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


#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#ifdef __linux__
#include <linux/mroute6.h>
#else
#include <netinet6/ip6_mroute.h>
#endif
#include <netinet/ip6.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/timerfd.h>
#include "defs.h"
#include "vif.h"
#include "groups.h"
#include "debug.h"
#include "inet6.h"
#include "mld6.h"
#include "mld6v2.h"
#include "mld6_proto.h"
#include "mld6v2_proto.h"


#ifdef HAVE_MLDV2

#ifndef HAVE_RFC3542
extern u_int8_t raopt[IP6OPT_RTALERT_LEN];
#endif

/* uses buffer in mld6.c */
extern char *sndcmsgbuf;
extern int ctlbuflen;
extern struct iovec sndiov[2];
extern struct msghdr sndmh;

static struct sockaddr_in6 dst_sa;
static int make_mld6v2_msg (int type, int code, struct sockaddr_in6 *src,
			    struct sockaddr_in6 *dst,
			    struct sockaddr_in6 *group, int ifindex,
			    unsigned int delay, int datalen, int alert,
			    int sflag, int qrv, int qqic, int gss);

/*
 * this function build three type of messages : 
 *	- general queries
 *	- group spec. query S flag set/not set
 *	- source/group spec. query S flag set
 *	- source/group spec. queries S flag not set
 *
 * specific queries are created in the following manner:
 *	- clear S flag
 *	- look up the source list of the group 
 *	- send a specific query with all the sources satisfying either
 *	  of the two conditions
 *		* state == LESSTHANLLQI && this is a specific query
 *		* state == MORETHANLLQI 
 * 	- set S flag
 *	if there something to send, return TRUE.
 * initialisation, and reading are done in mld6.c (it's just another icmp6 filtering) 
 * only used for mldv2 query messages 
 */

static int
make_mld6v2_msg (int type, int code, struct sockaddr_in6 *src,
		 struct sockaddr_in6 *dst, struct sockaddr_in6 *group,
		 int ifindex,
		 unsigned int delay, int datalen, int alert, int sflag,
		 int qrv, int qqic, int gss)
{
  struct mldv2_hdr *mhp = (struct mldv2_hdr *) mld6_send_buf;
  int ctllen, hbhlen = 0;
  int nbsrc = 0;
  struct listaddr *lstsrc = NULL;
  u_int8_t misc = 0;		/*Resv+S flag + QRV */
  unsigned int realnbr;
  mifi_t vifi;
  struct mvif *v;
  struct listaddr *g = NULL;
  struct cmsghdr *cmsgp;

  init_sin6 (&dst_sa);
  dst_sa.sin6_addr = allnodes_group.sin6_addr;
  sndmh.msg_name = (caddr_t) & dst_sa;

  vifi = find_vif_by_ifindex (ifindex);
  if (vifi  < 0 || vifi >= numvifs )
    {
      IF_DEBUG (DEBUG_MLD_PROTO)
	log_msg (LOG_INFO, 0, "make_mld6v2_msg: can't find a vif");
      return FALSE;
    }
  v = &mvifs[vifi];
  IF_DEBUG (DEBUG_MLD_PROTO) 
        log_msg (LOG_DEBUG, 0, "Preparing make_mld6v2_msg(): of type %d for G=%s interface %s",
                                type, sa6_fmt(group),v->mvif_name
                  );

  if (group != NULL)
    {
      g = find_multicast_group (v, group);

      if (g == NULL)
	{
	  IF_DEBUG (DEBUG_MLD_PROTO)
	    log_msg (LOG_ERR, 0,
		     "%s:BUG :trying to build group specific query but group %s is unknown on interface %s",
                     __func__,
		     sa6_fmt (group),
		     v->mvif_name);
	  return FALSE;
	}
      dst_sa.sin6_addr = group->sin6_addr;
    }

  /* scan the source-list only in case of GSS query */
  if (gss)
    {
      struct itimerspec timeleft;
      lstsrc = g->sources;

      while (lstsrc)
	{


	  mhp->mld_src[nbsrc] = lstsrc->mcast_group.sin6_addr;
	  IF_DEBUG (DEBUG_MLD_PROTO)
	    log_msg (LOG_DEBUG, 0,
		     "%s :BUILDING GSM list -  appending source (G,S)=(%s,%s)  interface %s",
                     __func__,
		     sa6_fmt (group),
		     sa6_fmt (&lstsrc->mcast_group),
		     v->mvif_name);
	  nbsrc++;
#if 0				// TODO
	  /*
	   * Section 6.6.1 draft-vida-mld-v2-00.txt : When a router send 
	   * a query with clear router Side Processing flag,
	   * it must update its timer to reflect the correct timeout 
	   * values: source timer for sources are lowered to LLQI
	   */
	  //if (sflag == SFLAGNO)
	  //
	  timerfd_gettime (lstsrc->group_report_timer, &timeleft);
	  if (timeleft.it_value.tv_sec >
	      MLD6_LAST_LISTENER_QUERY_INTERVAL )
	    {
	      IF_DEBUG (DEBUG_TIMER)
		log_msg (LOG_WARNING, 0,
			 "Sending GSS query() : source timer for source S=%s are lowered to LLQI =%d",
			 sa6_fmt (&lstsrc->mcast_group),
			 MLD6_LAST_LISTENER_QUERY_INTERVAL
			 );
	      start_report_timer (lstsrc,
				  MLD6_LAST_LISTENER_QUERY_INTERVAL 
				  );

	    }
#endif // TODO
	nextsrc:
	  lstsrc = lstsrc->ma_next;
	}
      if (nbsrc == 0)
	{
	  log_msg (LOG_WARNING, 0, "BUG: No source found for this GSS Query, perhaps group /source were deleted"
		   "(%s S-flag %s)",
		   sa6_fmt (group), sflag == SFLAGYES ? "ON" : "OFF");
	  return FALSE;
	}
    }

  IF_DEBUG (DEBUG_MLD_PROTO)
  {
          log_msg (LOG_DEBUG, 0, "==>(%s) Going to Send Query With S flag %s",
	            sa6_fmt (&dst_sa), sflag == SFLAGYES ? "ON" : "OFF");
  }

  /* fill the misc field */
  misc |= sflag;
  if (qrv <= 7)
    misc |= qrv;

  /* XXX : hard-coding, 28 is the minimal size of the mldv2 query header */
  datalen = 28 + nbsrc * sizeof (struct in6_addr);
  mhp->mld_icmp6hdr.icmp6_type = type;
  mhp->mld_icmp6hdr.icmp6_code = code;
  mhp->mld_icmp6hdr.icmp6_maxdelay =
    htons (codafloat (delay, &realnbr, 3, 12));
  mhp->mld_icmp6hdr.icmp6_maxdelay =
    htons (codafloat (delay, &realnbr, 3, 12));
  if (group != NULL)
    {
      IF_DEBUG (DEBUG_MLD_PROTO)
	log_msg (LOG_DEBUG, 0, "building Group Specific query");
      mhp->mld_addr = group->sin6_addr;	// building Group Specific query
    }
  else
    {
      IF_DEBUG (DEBUG_MLD_PROTO)
	log_msg (LOG_DEBUG, 0, "building Generic query");
      mhp->mld_addr = in6addr_any;	// building ASM query
    }
  mhp->mld_rtval = misc;
  mhp->mld_qqi = codafloat (qqic, &realnbr, 3, 4);
  mhp->mld_numsrc = htons (nbsrc);

  sndiov[0].iov_len = datalen;

  /* estimate total ancillary data length */
  ctllen = 0;
  if (ifindex != -1 || src)
    {
      ctllen = CMSG_SPACE (sizeof (struct in6_pktinfo));
      IF_DEBUG (DEBUG_MLD_PROTO)
	log_msg (LOG_DEBUG, 0,
		 "Reserved Anciallary data len for  in6_pktinfo  %d", ctllen);
    }
  if (alert)
    {
#ifdef HAVE_RFC3542
      if ((hbhlen = inet6_opt_init (NULL, 0)) == -1)
	log_msg (LOG_ERR, 0, "inet6_opt_init(0) failed");
      if ((hbhlen =
	   inet6_opt_append (NULL, 0, hbhlen, IP6OPT_ROUTER_ALERT, 2, 2,
			     NULL)) == -1)
	log_msg (LOG_ERR, 0, "inet6_opt_append(0) failed");
      if ((hbhlen = inet6_opt_finish (NULL, 0, hbhlen)) == -1)
	{
	  log_msg (LOG_ERR, 0, "inet6_opt_finish(0) failed");
	  //log_msg(LOG_DEBUG, 0, "Anciallary data len for IP6OPT_ROUTER_ALERT is %d", CMSG_SPACE(hbhlen)); 
	}
      ctllen += CMSG_SPACE (hbhlen);
#else
#ifdef HAVE_RFC2292		// Old API, RFC3542 is newer

      hbhlen = inet6_option_space (sizeof (raopt));
      ctllen += hbhlen;
#endif
#endif
    }
  if (sndcmsgbuf)
    {
      free (sndcmsgbuf);
    }
  /* allocate ancillary data space (if necessary) */

  if ((sndcmsgbuf = malloc (ctllen)) == NULL)
    {
      log_msg (LOG_ERR, 0, "make_mld6_msg: malloc failed");
    }
  ctlbuflen = ctllen;
  /* store ancillary data */
  //log_msg(LOG_DEBUG, 0, "Reserved Anciallary data len for is %d", ctllen); 
  if (ctllen <= 0)
    {
      sndmh.msg_control = NULL;	/* clear for safety */
      return TRUE;
    }
  // man says - must be a sum sndmh.msg_controllen = ctllen;
  memset (sndcmsgbuf, 0, sizeof (ctllen));
  sndmh.msg_controllen = ctllen;
  sndmh.msg_control = sndcmsgbuf;	/* place to store ancillary data */
  cmsgp = CMSG_FIRSTHDR (&sndmh);

  if (cmsgp == NULL)
    log_msg (LOG_DEBUG, 0,
	     "No space in ancillary options field of ICMPV6 message for IPV6_PKTINFO ");
  if ((ifindex != -1 || src) && cmsgp)
    {
      struct in6_pktinfo *pktinfo;

      cmsgp->cmsg_len = CMSG_LEN (sizeof (struct in6_pktinfo));
      cmsgp->cmsg_level = IPPROTO_IPV6;
      cmsgp->cmsg_type = IPV6_PKTINFO;
      pktinfo = (struct in6_pktinfo *) CMSG_DATA (cmsgp);
      memset ((caddr_t) pktinfo, 0, sizeof (*pktinfo));
      if (ifindex != -1)
	pktinfo->ipi6_ifindex = ifindex;
      if (src)
	pktinfo->ipi6_addr = src->sin6_addr;
      //log_msg(LOG_DEBUG, 0, "Placed   ICMPV6 message for IPV6_PKTINFO at address %p len is %d ", cmsgp, cmsgp->cmsg_len );
      // TODO cmsgp = CMSG_NXTHDR(&sndmh, cmsgp);
      cmsgp = (void *) sndcmsgbuf + CMSG_SPACE (sizeof (struct in6_pktinfo));
      IF_DEBUG (DEBUG_MLD_PROTO)
      	       log_msg (LOG_DEBUG, 0,
      		                   "%s:Placed   ICMPV6  Next header  at address %p", __func__, cmsgp);
      sndmh.msg_controllen = ctllen;
    }
  if (cmsgp == NULL)
    {
      log_msg (LOG_WARNING, 0,
	       "No space in ancillary options field of ICMPV6 message for ROUTER_ALERT");
    }
  if (alert && cmsgp)
    {
#ifdef HAVE_RFC3542
      int currentlen;
      void *hbhbuf, *optp = NULL;
      u_int16_t rtalert_code;

      rtalert_code = htons (IP6OPT_RTALERT_MLD);

      cmsgp->cmsg_len = CMSG_LEN (hbhlen);
      cmsgp->cmsg_level = IPPROTO_IPV6;
      cmsgp->cmsg_type = IPV6_HOPOPTS;
      hbhbuf = CMSG_DATA (cmsgp);

      if ((currentlen = inet6_opt_init (hbhbuf, hbhlen)) == -1)
	log_msg (LOG_ERR, 0, "inet6_opt_init(len = %d) failed", hbhlen);
      if ((currentlen = inet6_opt_append (hbhbuf, hbhlen, currentlen,
					  IP6OPT_ROUTER_ALERT, 2,
					  2, &optp)) == -1)
	log_msg (LOG_ERR, 0,
		 "inet6_opt_append(len = %d/%d) failed", currentlen, hbhlen);
      (void) inet6_opt_set_val (optp, 0, &rtalert_code,
				sizeof (rtalert_code));
      if (inet6_opt_finish (hbhbuf, hbhlen, currentlen) == -1)
	log_msg (LOG_ERR, 0, "inet6_opt_finish(buf) failed");
#else /* old API */
#ifdef HAVE_RFC2292

      if (inet6_option_init ((void *) cmsgp, &cmsgp,
#ifdef IPV6_2292HOPOPTS
			     IPV6_2292HOPOPTS
#else
			     IPV6_HOPOPTS
#endif
	  ))
	log_msg (LOG_ERR, 0,	/* assert */
		 "make_mld6_msg: inet6_option_init failed");
      if (inet6_option_append (cmsgp, raopt, 4, 0))
	log_msg (LOG_ERR, 0,	/* assert */
		 "make_mld6_msg: inet6_option_append failed");

#endif //old API
#endif
      cmsgp = CMSG_NXTHDR (&sndmh, cmsgp);	// TODO no neead to advnce, we are near function end
    }

  return TRUE;
}

int
send_mld6v2 (uint8_t mld_packet_type, uint8_t code,	/* for trace packets only */
	     struct sockaddr_in6 *src, struct sockaddr_in6 *dst,	/* may be NULL */
	     struct sockaddr_in6 *group,
	     uint8_t index, uint16_t delay,
	     uint16_t datalen, uint8_t alert,
	     uint8_t sflag, uint8_t qrv, uint8_t qqic, uint8_t gss)
{
  struct sockaddr_in6 *dstp;

  if (make_mld6v2_msg (mld_packet_type, code, src, dst, group, index, delay *MLD6_TIMER_SCALE,
		       datalen, alert, sflag, qrv, qqic, gss) == FALSE)
    {
      log_msg (LOG_ERR, 0, "Failed to make_mld6v2_msg of type %d",
	       mld_packet_type);
      return FALSE;
    }

  dstp = (struct sockaddr_in6 *) sndmh.msg_name;

  if (IN6_IS_ADDR_LINKLOCAL (&dstp->sin6_addr) ||
      IN6_IS_ADDR_MC_LINKLOCAL (&dstp->sin6_addr))
    dstp->sin6_scope_id = index;

  if (sendmsg (mld6_socket, &sndmh, 0) < 0)
    {
      if (errno == ENETDOWN)
	{
	  log_msg (LOG_ERR, errno,
		   " Critical - interface  %s is down, sendmsg to %s with src %s",
		   ifindex2str (index), sa6_fmt (dstp), sa6_fmt (src));
	}
      else
	{
	  log_msg (log_level (IPPROTO_ICMPV6, mld_packet_type, 0), errno,
		   "Failed in sendmsg to %s with src %s on %s",
		   sa6_fmt (dstp), sa6_fmt (src), ifindex2str (index));
	}
      return FALSE;
    }

  IF_DEBUG (DEBUG_PKT)
  {
    log_msg (LOG_DEBUG, 0, "SENT %s from %-15s to %s",
	     packet_kind (IPPROTO_ICMPV6, mld_packet_type, 0),
	     src ? sa6_fmt (src) : "unspec", sa6_fmt (dstp));
  }
  return TRUE;
}


/*
 * given a number, an exp. size in bits and a mantisse size in bits, return
 * the coded number value according to the code described in 
 * draft-vida-mld-v2-08.txt 
 * used to compute the Maximum Response Code (exp=3bit, mant=12bit) 
 * and the Querier Query interval Code (exp=3bit, mant=4 bit)
 * format  : |1|...exp...|...mant...|
 * if the number isn't representable there is a difference between realnbr 
 * and nbr if the number is too big return the max code value with a warning 
 */
unsigned int
codafloat (unsigned int nbr, unsigned int *realnbr, unsigned int sizeexp,
	   unsigned int sizemant)
{
  unsigned int mask = 0x1;
  unsigned int max = 0x0;
  unsigned int exp = 1;		/*exp value */
  unsigned int tmax;		/*max code value */
  unsigned int mantmask = 1;	/*mantisse mask */
  unsigned int onebit = 1;
  unsigned int mant;
  u_int16_t code = 1;		/* code */
  int i;

  /* compute maximal exp value */
  for (i = 1; i < sizeexp; i++)
    exp = (exp << 1) | 1;

  /* maximum size of this number in bits (after decoding) */
  tmax = exp + 3 + sizemant + 1;

  /* minimum value of this number */
  code <<= sizeexp + sizemant;
  mask <<= tmax - 1;

  /* maximum value of this number + a mantisse masque */
  for (i = 0; i <= sizemant; i++)
    max = max | mask >> i;
  for (i = 0; i < sizemant; i++)
    mantmask = mantmask | (onebit << i);

  /* not in coded number, so just return the given number as it is */
  if (nbr < code)
    {
      code = *realnbr = nbr;
      return code;
    }

  /* overflowed, so just return the possible max value */
  if (nbr > max)
    {
      *realnbr = max;
      return codafloat (max, realnbr, sizeexp, sizemant);
    }

  /* calculate the float number */
  while (!(nbr & mask))
    {
      mask >>= 1;
      tmax--;
    }
  exp = tmax - (sizemant + 1);
  mant = nbr >> exp;
  exp -= 3;

  /* build code */
  mant &= mantmask;
  code |= mant;
  code |= exp << sizemant;

  /* compute effective value (draft-vida-mld-v2-08.txt p.11) */
  onebit <<= sizemant;
  *realnbr = (mant | onebit) << (exp + 3);
  return code;
}

unsigned int
decodeafloat (unsigned int nbr, unsigned int sizeexp, unsigned int sizemant)
{
  unsigned int onebit = 1;
  unsigned int mantmask = 0;
  unsigned int mant = 0;
  unsigned int exp = 0;
  int i;

  for (i = 0; i < sizemant; i++)
    mantmask = mantmask | (onebit << i);
  mant = nbr & mantmask;
  exp = (nbr & ~(onebit << (sizemant + sizeexp))) >> sizemant;
  onebit <<= sizemant;
  return (mant | onebit) << (exp + 3);
}
#endif
