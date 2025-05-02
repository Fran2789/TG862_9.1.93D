#ifndef ___IGMPROXY_HDR_
#define ___IGMPROXY_HDR_

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
*   igmpproxy.h - Header file for common includes.
*/

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/igmp.h>
#include <arpa/inet.h>
#include "vif.h"




/*
 * Limit on length of route data
 */
#define MAX_IP_PACKET_LEN	576
#define MIN_IP_HEADER_LEN	20
#define MAX_IP_HEADER_LEN	60

// ARRIS ADD START
#define ROUTER_ALERT_OPTION_LEN         (4)
// ARRIS ADD END
#define IP_RAOPT_LEN        4  // router option length in bytes
#define IP_HEADER_RAOPT_LEN ((MIN_IP_HEADER_LEN) + (IP_RAOPT_LEN)) 


/* 
 * Bit manipulation macros...
*/
#define BIT_ZERO(X)      ((X) = 0)
#define BIT_SET(X,n)     ((X) |= 1 << (n))
#define BIT_CLR(X,n)     ((X) &= ~(1 << (n)))
#define BIT_TST(X,n)     ((X) & 1 << (n))


/*#################################################################################
*  Globals
#################################################################################*/

/*
 * External declarations for global variables and functions.
 */


/*
extern char     s1[];
extern char     s2[];
extern char		s3[];
extern char		s4[];
*/


/* IGMP socket as interface for the mrouted API
 - receives the IGMP messages
*/

extern int IgmpSocket;
extern int IgmpProxySocket;

/* Program sources compatability to mld6 */
#define mld6_proxy_socket IgmpProxySocket
#define mld6_socket IgmpSocket
typedef u_int32_t IP_ADDRESS;


#define RECV_BUF_SIZE	        4*1024
#define SO_RECV_BUF_SIZE_MAX	16*1024
#define SO_RECV_BUF_SIZE_MIN	1*1024

/* igmp.c
*/
extern char * send_buf;
extern uint32_t allhosts_group;
extern uint32_t allrouters_group;
extern void initIgmp(void);
extern void acceptIgmp(int);
extern int sendIgmp(uint16_t mifi ,uint32_t src, uint32_t dst, int type, int code, uint32_t group, uint16_t datalen);
/* 
 * lib.c

extern char   *fmtInAdr( char *St, struct in_addr InAdr );
extern char   *inetFmts(uint32_t addr, uint32_t mask, char *s);
*/


/*
* queries.c Prototypes...
 */

extern int sendGeneralMembershipQuery(struct mvif *v) ;
extern int sendGroupSpecificMemberQuery(  struct mvif *v,  struct listaddr * group_db_record);
extern int sendSSMSpecificMemberQuery( struct mvif *v, uint32_t  group,
                               struct listaddr * group_db_record,
                               struct listaddr * source_db_record
                              );

//#include "igmpv2_proto.h"
//#include "igmpv3_proto.h"

#endif

