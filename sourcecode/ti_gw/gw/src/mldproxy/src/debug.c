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


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <time.h>
#include "defs.h"
#include "pathnames.h"
#include "debug.h"
#include "vif.h"
#include "inet6.h"
#include "mld6.h"
#include "mld6v2.h"
#include "mld6_proto.h"
#include "mld6v2_proto.h"


extern char    *progname;

struct debugname 
debugnames[20] = {
    {   "mld_proto",    DEBUG_MLD_PROTO,                5     },
    {   "mld_timer",    DEBUG_MLD_TIMER,                5     },
    {   "mld_member",   DEBUG_MLD_MEMBER,               5     },
    {   "mld",                  DEBUG_MLD,              3     },
 
    {   "timeout",              DEBUG_TIMEOUT,          2     },
    {   "callout",              DEBUG_TIMEOUT,          3     },
    {   "pkt",                  DEBUG_PKT,              2     },
    {   "packets",              DEBUG_PKT,              2     },
    {   "interfaces",           DEBUG_IF,               2     },
    {   "vif",                  DEBUG_IF,               1     },
    {   "kernel",               DEBUG_KERN,             2     },
    {   "cache",                DEBUG_MFC,              1     },
    {   "mfc",                  DEBUG_MFC,              2     },
    {   "k_cache",              DEBUG_MFC,              2     },
    {   "k_mfc",                DEBUG_MFC,              2     },
   
    {   "timers",               DEBUG_TIMER,            1     },
    {   "asserts",              DEBUG_ASSERT,           1     },
    {   "all",                  DEBUG_ALL,              2     },
    {   "3",                    0xffffffff,             1     }    /* compat. */
};

int         log_nmsgs = 0;
u_int32_t   debug = 0x00000000;	
static char dumpfilename[] = _PATH_MLDPROXYD_DUMP;

static char  statfilename[] = _PATH_MLDPROXYD_STAT;

static const char *sec2str __P((time_t));

static const char * sec2str(time_t total)
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;
	char *ep = &result[sizeof(result)];
	int n;

	days = total / 3600 / 24;
	hours = (total / 3600) % 24;
	mins = (total / 60) % 60;
	secs = total % 60;

	if (days) {
		first = 0;
		n = snprintf(p, ep - p, "%dd", days);
		if (n < 0 || n >= ep - p)
			return "?";
		p += n;
	}
	if (!first || hours) {
		first = 0;
		n = snprintf(p, ep - p, "%dh", hours);
		if (n < 0 || n >= ep - p)
			return "?";
		p += n;
	}
	if (!first || mins) {
		first = 0;
		n = snprintf(p, ep - p, "%dm", mins);
		if (n < 0 || n >= ep - p)
			return "?";
		p += n;
	}
	snprintf(p, ep - p, "%ds", secs);

	return(result);
}

const char * packet_kind(uint8_t proto, uint8_t type, uint8_t code)
{
    static char     unknown[128];

    switch (proto)
    {
      case IPPROTO_ICMPV6:
	 switch (type)
	  {
	    case MLD_LISTENER_QUERY:
		return "Multicast Listener Query    ";
	    case MLD_LISTENER_REPORT:
		return "Multicast Listener Report   ";
	    case MLD_LISTENER_DONE:
		return "Multicast Listener Done     ";
	    default:
	      snprintf(unknown, sizeof(unknown),
		    "UNKNOWN ICMPv6 message: type = 0x%02x, code = 0x%02x ",
		    type, code);
	    return unknown;
	}
      default:
	  snprintf(unknown, sizeof(unknown),
	    "UNKNOWN proto =%3d               ", proto);
	  return unknown;
    }
}


/*
 * Used for debugging particular type of messages.
 */
const int debug_kind (uint8_t proto, uint8_t type, uint8_t code )
{
    switch (proto)
    {
    case IPPROTO_ICMPV6:
	switch (type)
	{
	case MLD_LISTENER_QUERY:
	    return DEBUG_MLD;
	case MLD_LISTENER_REPORT:
	    return DEBUG_MLD;
	case MLD_LISTENER_DONE:
	    return DEBUG_MLD;
	default:
	    return DEBUG_MLD;
	}
    default:
	return 0;
    }
    return 0;
}


/*
 * Some messages are more important than others.  This routine determines the
 * logging level at which to log a send error (often "No route to host").
 * This is important when there is asymmetric reachability and someone is
 * trying to, i.e., mrinfo me periodically.
 */
int log_level(uint8_t proto, uint8_t type, uint8_t code)
{
    switch (proto)
    {
    case IPPROTO_ICMPV6:
	switch (type)
	{
    	default:
	    return LOG_WARNING;
    	}

    case IPPROTO_PIM:
    /* PIM v2 */
    	switch (type)
    	{
    	default:
	    return LOG_INFO;
    	}
    default:
    	return LOG_WARNING;
    }

    return LOG_WARNING;
}


/*
 * Dump internal data structures to stderr.
 */
/*
 * TODO: currently not used void dump(int i) { dump_vifs(stderr);
 * 
 */

/*
 * Dump internal data structures to a file.
 */
void fdump(int i)
{
    FILE           *fp;
    fp = fopen(dumpfilename, "w");
    if (fp != NULL)
    {
	dump_vifs(fp);
	
	// TODO dump_mldqueriers(fp);
	dump_mldgroups(fp);
	
	
	(void) fclose(fp);
    }
}


void
dump_stat(void)
{
	FILE *fp;
	mifi_t vifi;
	register struct mvif *v;

	fp = fopen(statfilename, "w");
	if (fp == NULL) {
		log_msg(LOG_WARNING, errno, "dump_stat: can't open file(%s)",
		    statfilename);
		return;
	}

	fprintf(fp, "mldproxy per-interface statistics\n");
	for (vifi = 0, v = mvifs; vifi < numvifs; vifi++, v++) {
#if 0				/* is it better to skip them? */
		if ((v->uv_flags & (VIFF_DISABLED|VIFF_DOWN)) != 0)
			continue;
#endif
		fprintf(fp, " Mif=%d, PhyIF=%s\n", vifi, v->mvif_name);
		
		

		fprintf(fp, "\t%qu MLD query received\n",
			(unsigned long long)v->uv_in_mld_query);
		fprintf(fp, "\t%qu MLD report received\n",
			(unsigned long long)v->uv_in_mld_report);
		fprintf(fp, "\t%qu MLD done received\n",
			(unsigned long long)v->uv_in_mld_done);

		fprintf(fp, "\t%qu MLD query sent\n",
			(unsigned long long)v->uv_out_mld_query);
		fprintf(fp, "\t%qu MLD report sent\n",
			(unsigned long long)v->uv_out_mld_report);
		fprintf(fp, "\t%qu MLD done sent\n",
			(unsigned long long)v->uv_out_mld_done);
	}

	
	fclose(fp);
}

void dump_vifs(FILE *fp)
{
    mifi_t          vifi;
    register struct mvif *v;
    

    fprintf(fp, "\nMulticast Interface Table\n %-4s %-6s %-43s %5s %-8s %-14s\n",
	    "Mif", " PhyIF", "Local-Address/Prefixlen","Scope", "GenID", "Flags");

    for (vifi = 0, v = mvifs; vifi < numvifs; ++vifi, ++v)
    {
	
	
	    
	    fprintf(fp, "  %-3u %6s %-43s", vifi,
		    v->mvif_name,
		    sa6_fmt(&v->uv_linklocal));
			    
	   
	   

	    
	    if (v->uv_flags & VIFF_DISABLED)
		fprintf(fp, " DOWN");

	    if (v->uv_flags & VIFF_QUERIER)
		fprintf(fp, " QRY");
	    if ((v->uv_flags & VIFF_ENABLED) &&
	        (v->uv_flags & VIFF_DOWNSTREAM))
		fprintf(fp, "DS UP");

	    fprintf(fp, "\n");
	}

	fprintf(fp, "  %3s %6s ", "", "");
	fprintf(fp, "Timers:  MLD query timer = %ld:%02ld\n",
		v->uv_q_time / 60, v->uv_q_time % 60);

	fprintf(fp, "  %3s %6s ", "", "");
	fprintf(fp, "possible MLD version = %s%s\n",
		v->uv_mld_version & MLDv1 ? "1 " : "",
		v->uv_mld_version & MLDv2 ? "2 " : "");
    
    fprintf(fp, "\n");
}



void
dump_mldgroups(FILE *fp)
{
	struct mvif *v;
	mifi_t vifi;
	struct listaddr *grp, *src;

	fprintf(fp, "Reported MLD Group\n");
	fprintf(fp, " %-3s %6s %s\n", "Mif", "PhyIF",
		"Group(Group-Timer,MLD-ver(Filter-Mode,Compat-Timer))/"
		"Source(TimerID)");

	for (vifi = 0, v = mvifs; vifi < numvifs; ++vifi, ++v) {
		for (grp = v->uv_groups; grp; grp = grp->ma_next) {
#ifdef HAVE_MLDV2
			fprintf(fp, "#interface[%u]:%s (G=mcast_group=%s compat-mode=%s filter-mode=%s))\n", vifi,
			     v->mvif_name,
			     sa6_fmt(&grp->mcast_group), 
			    grp->ma_comp_mode & MLDv2 ? "v2" : "v1",
			    grp->ma_filter_mode == MODE_IS_INCLUDE ? "IN" : "EX"
			    );
#else
			fprintf(fp, " %-3u %6s %s (#%u (%s))\n", vifi,
			     v->mvif_name,
			    sa6_fmt(&grp->mcast_group), grp->group_report_timer,
			    "v1");
#endif

			src = grp->sources;
			if (src == NULL) {
				fprintf(fp, " %-3s %6s   %s (-)\n", "", "",
					"(any source)");
				continue;
			}
			for ( ; src; src = src->ma_next) {
				fprintf(fp, " %-3s %6s   %s (#%u)\n", "", "",
					sa6_fmt(&src->mcast_group),
					src->group_report_timer);
			}
		}
	}
	fprintf(fp, "\n");
} 

/*
 * Log errors and other messages to the system log daemon and to stderr,
 * according to the severity of the message and the current debug level. For
 * errors of severity LOG_ERR or worse, terminate the program.
 */
#ifdef __STDC__
void
debug_log_msg(int severity, int syserr, char *format, ...)
{
    va_list         ap;
    static char     fmt[211] = "warning - ";
    char           *msg;
    struct timeval  now;
    struct tm      *thyme;

    va_start(ap, format);
#else
/* VARARGS3 */
void
debug_log_msg(severity, syserr, format, va_alist)
    int             severity,
                    syserr;
    char           *format;
va_dcl
{
    va_list         ap;
    static char     fmt[311] = "warning - ";
    char           *msg;
    char            tbuf[20];
    struct timeval  now;
    struct tm      *thyme;

    va_start(ap);
#endif
    vsnprintf(&fmt[10], sizeof(fmt) - 10, format, ap);
    va_end(ap);
    msg = (severity == LOG_WARNING) ? fmt : &fmt[10];

    /*
     * Log to stderr if we haven't forked yet and it's a warning or worse, or
     * if we're debugging.
     */
    if (debug || severity <= LOG_WARNING)
    {
	time_t t;
	FILE *fp = log_fp ? log_fp : stderr;

	gettimeofday(&now, NULL);
	t = (time_t)now.tv_sec;
	thyme = localtime(&t);
	if (!debug)
	    fprintf(fp, "%s: ", progname);
	fprintf(fp, "%02d:%02d:%02d.%03ld %s", thyme->tm_hour,
		thyme->tm_min, thyme->tm_sec, (long int)now.tv_usec / 1000,
		msg);
	if (syserr == 0)
	    fprintf(fp, "\n");
	else {
	    fprintf(fp, ": %s\n", strerror(syserr));

	}
    }

    /*
     * Always log things that are worse than warnings, no matter what the
     * log_nmsgs rate limiter says. Only count things worse than debugging in
     * the rate limiter (since if you put daemon.debug in syslog.conf you
     * probably actually want to log the debugging messages so they shouldn't
     * be rate-limited)
     */
    if ((severity < LOG_WARNING) || (log_nmsgs < LOG_MAX_MSGS))
    {
	if (severity < LOG_DEBUG)
	    log_nmsgs++;
	if (syserr != 0)
	{
	    errno = syserr;
	    syslog(severity, "%s: %m", msg);
	}
	else
	    syslog(severity, "%s", msg);
    }

    if (severity <= LOG_ERR)
	exit(-1);
}
