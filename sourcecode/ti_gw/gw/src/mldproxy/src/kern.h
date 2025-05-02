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


#ifndef __MLDPROXY_KERN_H 
#define __MLDPROXY_KERN_H
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>	
#include <netinet/in.h>
#include <linux/mroute6.h>
#include "vif.h"

extern int     k_set_rcvbuf    __P((int socket, int bufsize, int minsize));
extern int     k_set_hlim       __P((int socket, u_int8_t hop_limit));
extern int     k_set_loop      __P((int socket, int l));
extern int     k_set_if        __P((int socket, u_int16_t ifindex));
extern int     k_join          __P((int socket, struct in6_addr *grp,
                     u_int16_t ifindex));
extern int     k_leave         __P((int socket, struct in6_addr *grp,
                     u_int16_t ifindex));
extern int     k_join_src          __P((int socket, struct in6_addr *grp, struct in6_addr *m_source,
                     u_int16_t ifindex));
extern int     k_leave_src         __P((int socket, struct in6_addr *grp, struct in6_addr *m_source,
                     u_int16_t ifindex));
extern int     k_block_src          __P((int socket, struct in6_addr *grp, struct in6_addr *m_source,
                     u_int16_t ifindex));
extern int     k_unblock_src          __P((int socket, struct in6_addr *grp, struct in6_addr *m_source,
                     u_int16_t ifindex));
extern int      k_add_to_mfc6       __P((int socket, struct sockaddr_in6 *source,
				      struct sockaddr_in6 *group, mifi_t iif, 
				      if_set *oifs));
extern int      k_del_from_mfc6       __P((int socket,  struct sockaddr_in6 *source,
					    struct sockaddr_in6 *group));

extern int     k_add_vif       __P((int socket, mifi_t vifi, struct mvif *v));
extern int     k_del_vif       __P((int socket, mifi_t vifi));
extern int      process_kernel_call __P((void ));
extern int k_get_sg_cnt(int socket, struct sockaddr_in6 * source, struct sockaddr_in6 * group );


#endif
