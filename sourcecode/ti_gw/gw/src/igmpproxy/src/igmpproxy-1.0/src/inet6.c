/*
 * Copyright (C) 1998 WIDE Project.
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
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.static int      ip6round = 0;
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
 * This program has been derived from pim6dd.        
 * The pim6sd program is covered by the license in the accompanying file
 * named "LICENSE.pim6sd".
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
#include <sys/socket.h>
#include <sys/param.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#ifdef __linux__
#include <linux/mroute6.h>
#else
#include <netinet6/ip6_mroute.h>
#endif
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include "defs.h"
#include "vif.h"
#include "inet6.h"
#include <arpa/inet.h>

/* flag if address to hostname resolution should be perfomed */
int numerichost = TRUE;

char     ip6buf[8][INET6_ADDRSTRLEN];

int      ip6round = 0;
uint8_t  ip6bufindex = 0;
char     ifname[IFNAMSIZ];

const char *inetFmt (uint32_t addr)
{
   const char           *cp;
   
  if ( addr == 0)
      return "0.0.0.0";
    ip6bufindex++;
    ip6bufindex = ip6bufindex %7;
    
    memset(ip6buf[ip6bufindex], 0, sizeof(ip6buf[ip6bufindex] ));
  
    cp= inet_ntop(AF_INET, (void *) &addr,
                           ip6buf[ip6bufindex], sizeof(ip6buf[ip6bufindex] ));
    if (cp == NULL)
    {
      perror("inetFmt critical error");
    }
    return cp;
  
   
}
  
  
const char *
inet4_fmt(struct in_addr *sa4)
{
    int flags = 0;
    const char           *cp;

    if (sa4 == NULL)
      return "0.0.0.0";
    ip6bufindex++;
    ip6bufindex = ip6bufindex %7;
    
    memset(ip6buf[ip6bufindex], 0, sizeof(ip6buf[ip6bufindex] ));
    cp= inet_ntop(AF_INET, (void *) sa4,
                           ip6buf[ip6bufindex], sizeof(ip6buf[ip6bufindex] ));
    if (cp == NULL)
    {
      perror("sa4_fmt critical error");
    }
    return cp;
  
   
}
const char *
sa4_fmt(struct sockaddr_in *sa4)
{
    int flags = 0;
    const char           *cp;

    if (sa4 == NULL)
      return "0.0.0.0";
    ip6bufindex++;
    ip6bufindex = ip6bufindex %7;
    
    memset(ip6buf[ip6bufindex], 0, sizeof(ip6buf[ip6bufindex] ));
    cp= inet_ntop(AF_INET, (void *) &sa4->sin_addr,
                           ip6buf[ip6bufindex], sizeof(ip6buf[ip6bufindex] ));
    if (cp == NULL)
    {
      perror("sa4_fmt critical error");
    }
    return cp;
  
   
}


char           *
ifindex2str(int ifindex)
{

    return (if_indextoname(ifindex, ifname));
}


void
init_sin(struct sockaddr_in *sin)
{
	memset(sin, 0, sizeof(*sin));
	sin->sin_family = AF_INET;
#ifdef HAVE_SA_LEN
	sin->sin6_len = sizeof(*sin);
#endif
}

socklen_t
get_sa_len(struct sockaddr *addr)
{
#ifdef HAVE_SA_LEN
	return addr->sa_len;
#else
	switch (addr->sa_family) {
	case AF_INET:
		return sizeof(struct sockaddr_in);
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
	default:
		return sizeof(struct sockaddr);
	}
#endif
}
