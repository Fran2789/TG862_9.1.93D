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

#ifndef MCAST_PROTO_H
#define MCAST_PROTO_H
#include <netinet/in.h>
#include <linux/mroute.h>

#define queryResponseInterval uv_mld_query_rsp_interval
#define lastMemberQueryInterval uv_mld_llqi

#include "vif.h"
/* structure used to send multicast group/source specific queries */

/*
 * Constans for Multicast Listener Discovery protocol for IPv6.
*/




#define	MLD6_DEFAULT_VERSION	MLDv2
#define MLD6_DEFAULT_ROBUSTNESS       2
#define MLD6_DEFAULT_QUERY_INTERVAL 125 /* in seconds */
#define MLD6_DEFAULT_QUERY_RESPONSE_INTERVAL 10 /* in a units of 1/10 seconds i. e 1 sec */
#define MLD6_DEFAULT_LAST_LISTENER_QUERY_INTERVAL 1 /* in seconds */
#define MLD6_STARTUP_QUERY_INTERVAL 30	/* in seconds */
#define MLD6_STARTUP_QUERY_COUNT	MLD6_ROBUSTNESS_VARIABLE

#define MLD6_ROBUSTNESS_VARIABLE		v->uv_mld_robustness
#define MLD6_QUERY_INTERVAL			v->uv_mld_query_interval
#define MLD6_QUERY_RESPONSE_INTERVAL		v->uv_mld_query_rsp_interval
#define MLD6_LAST_LISTENER_QUERY_INTERVAL	v->uv_mld_llqi
#ifndef MLD6_TIMER_SCALE
#define MLD6_TIMER_SCALE 10
#endif




typedef enum v3_mcast_report_subtypes {
 CACHE_MISS=0,
 MODE_IS_INCLUDE=1,
 MODE_IS_EXCLUDE=2,
 CHANGE_TO_INCLUDE_MODE=3,
 CHANGE_TO_EXCLUDE_MODE=4,
 ALLOW_NEW_SOURCES=5,
 BLOCK_OLD_SOURCES=6
} mcast_report_subtypes;



/*
 * MCAST_LISTENER_INTERVAL corresponds to the following same value.
 *  - Multicast Listener Interval (MLDv1, RFC2710 sec 7.4)
 *  - Multicast Address Listening Interval (MLDv2, RFC3810 sec 9.4)
 */
#define MLD6_LISTENER_INTERVAL (MLD6_ROBUSTNESS_VARIABLE * \
		MLD6_QUERY_INTERVAL \
		 )
#define MLD6_LAST_LISTENER_QUERY_COUNT     ( MLD6_ROBUSTNESS_VARIABLE -1 )
#define MLD6_LAST_LISTENER_QUERY_TIMER \
	(MLD6_LAST_LISTENER_QUERY_COUNT * \
	 MLD6_LAST_LISTENER_QUERY_INTERVAL )

#define MLD6_OTHER_QUERIER_PRESENT_INTERVAL (MLD6_ROBUSTNESS_VARIABLE * \
		MLD6_QUERY_INTERVAL + \
		MLD6_QUERY_RESPONSE_INTERVAL/10)
#define MLD6_OLDER_VERSION_HOST_PRESENT (MLD6_ROBUSTNESS_VARIABLE * \
		MLD6_QUERY_INTERVAL  + \
		MLD6_QUERY_RESPONSE_INTERVAL/ 10 )



extern struct listaddr * recv_listener_report(short ifindex , mifi_t mifi, uint32_t  src, uint32_t mcast, u_int8_t mld_version);

extern void     recv_listener_done      __P((short ifindex, mifi_t,
					     uint32_t src,
					     uint32_t group));

#define IGMPPROXY_IPV4
#ifdef IGMPPROXY_IPV4
/* Multicast group modes */
#define IGMPv3	0x04
#define IGMPv2	0x02
#define IGMPv1	0x01
/* Multicast group modes */
#define MLDv1	         IGMPv2
#define MLDv2	         IGMPv3
#define MCAST_SSM	 0x08  /*  Source specific multicast */

#define	MCAST_DEFAULT_VERSION	                    IGMPv3
#define MCAST_DEFAULT_ROBUSTNESS                    MLD6_DEFAULT_ROBUSTNESS
#define MCAST_DEFAULT_QUERY_INTERVAL                MLD6_DEFAULT_QUERY_INTERVAL
#define MCAST_DEFAULT_QUERY_RESPONSE_INTERVAL       MLD6_DEFAULT_QUERY_RESPONSE_INTERVAL
#define MCAST_DEFAULT_LAST_LISTENER_QUERY_INTERVAL  MLD6_DEFAULT_LAST_LISTENER_QUERY_INTERVAL
#define MCAST_STARTUP_QUERY_INTERVAL                MLD6_STARTUP_QUERY_INTERVAL 
#define MCAST_QUERY_RESPONSE_INTERVAL               MLD6_QUERY_RESPONSE_INTERVAL
#define MCAST_STARTUP_QUERY_COUNT	            MLD6_STARTUP_QUERY_COUNT
#define MCAST_LAST_LISTENER_QUERY_COUNT             MLD6_LAST_LISTENER_QUERY_COUNT
#define MCAST_ROBUSTNESS_VARIABLE                   MLD6_ROBUSTNESS_VARIABLE
#define MCAST_LISTENER_INTERVAL                     MLD6_LISTENER_INTERVAL
#define MCAST_LAST_LISTENER_QUERY_INTERVAL          MLD6_LAST_LISTENER_QUERY_INTERVAL
#define MCAST_TIMER_SCALE                           MLD6_TIMER_SCALE   
#define MCAST_LAST_LISTENER_QUERY_TIMER             MLD6_LAST_LISTENER_QUERY_TIMER
#endif
/* V3 * from linux/igmp.h */
#ifndef IGMPV3_HOST_MEMBERSHIP_REPORT
#define IGMPV3_HOST_MEMBERSHIP_REPORT	0x22	/* V3 version of 0x12 */
#define IGMPV3_ALL_MCR	 	htonl(0xE0000016L)
#define IGMP_V3_MEMBERSHIP_REPORT IGMPV3_HOST_MEMBERSHIP_REPORT
#endif
#endif
