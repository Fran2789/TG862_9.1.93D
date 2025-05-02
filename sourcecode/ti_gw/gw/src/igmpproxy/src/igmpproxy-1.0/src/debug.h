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


#ifndef __MCAST_DEBUG_H_
#define  __MCAST_DEBUG_H_

#include "defs.h"
extern u_int32	debug_mask;
extern int log_nmsgs;

#define IF_DEBUG(l)	if (debug_mask &  (l))

#define LOG_MAX_MSGS	20	/* if > 20/minute then shut up for a while */
#define LOG_SHUT_UP	600	/* shut up for 10 minutes */


/* Debug values definition */


/* MLD related */

#define DEBUG_MLD_PROTO      0x00000010
#define DEBUG_MLD_TIMER      0x00000020
#define DEBUG_MLD_MEMBER     0x00000040
#define DEBUG_GEN_QUERIES_TIMER 0x00000080
#define DEBUG_MEMBER          DEBUG_MLD_MEMBER
#define DEBUG_MLD             ( DEBUG_MLD_PROTO | DEBUG_MLD_TIMER | \
				DEBUG_MLD_MEMBER )

/* Misc */


#define DEBUG_TIMEOUT         0x00000100
#define DEBUG_PKT             0x00000200


/* Kernel related */

#define DEBUG_IF              0x00000400
#define DEBUG_KERN            0x00000800
#define DEBUG_MFC             0x00001000
#define DEBUG_EXPIRE_TIMER   0x00002000
#define DEBUG_MLD_ASSERT      0x00008000


#define DEBUG_TIMER           ( DEBUG_MLD_TIMER | DEBUG_GEN_QUERIES_TIMER | \
				DEBUG_EXPIRE_TIMER )
#define DEBUG_ASSERT          ( DEBUG_MLD_ASSERT )

/* CONFIG related */
#define DEBUG_CONF 0x01000000

#define DEBUG_ALL             0xffffffff
#define DEBUG_SWITCH  		  0x80000000

#define DEBUG_DEFAULT   0xffffffff/*  default if "-d" given without value */



struct debugname {
    const char           *name;
    int             level;
    int             nchars;
};

extern struct debugname debugnames[20];



extern const char *packet_kind   (uint8_t proto, uint8_t type, uint8_t code);    
extern int  debug_kind    (uint8_t proto, uint8_t type, uint8_t code);
/*
extern void log_msg         __P((int, int, char *, ...))
	__attribute__((__format__(__printf__, 3, 4)));
*/
extern int  log_level      (uint8_t proto, uint8_t type, uint8_t code);
extern void dump            __P((int i));
extern void fdump           __P((int i));

extern void dump_vifs       __P((FILE *fp));

extern void dump_mldgroups	__P((FILE *fp));
extern void dump_stat __P((void));

#endif

