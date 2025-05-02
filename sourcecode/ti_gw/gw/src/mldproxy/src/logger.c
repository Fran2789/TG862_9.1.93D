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
**  ------------------------------------------------------------------------------
**  
**   This software is derived work from the igmproxy software.  The original
**   source code has been modified from it's original state by the author
**   of mldproxy.
**
**   Includes Intel Corporation's changes/modifications dated:
**     30-Nov-2011: changed my_log function to a wrapper 
**     30-Nov-2011: calling fprintf instead of syslog
**     30-Nov-2011: other reworks ans simplifications 
**  
**   Changed/modified portions - Copyright(C) 2011-2012, Intel Corporation.
**  
**  ------------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>


#include "defs.h"


#define bool  unsigned short
#define false 0
#define true 1

bool Log2Stderr = true;

void my_syslog(int Severity,  const char * LogMsg); 
int LogLevel = LOG_WARNING;
void log_msg( int Severity, int Errno, const char *FmtSt, ... )
{
    char LogMsg[ 128 ];

    va_list ArgPt;
    unsigned Ln;
    va_start( ArgPt, FmtSt );
    Ln = vsnprintf( LogMsg, sizeof( LogMsg ), FmtSt, ArgPt );
    if( Errno > 0 )
        snprintf( LogMsg + Ln, sizeof( LogMsg ) - Ln,
                "; Errno(%d): %s", Errno, strerror(Errno) );
    va_end( ArgPt );

    fprintf(stderr,"%s\n",  LogMsg);
    if (Severity <= LogLevel) {
        if (Log2Stderr)
            fprintf(stderr, "MLDPROXY: %s\n", LogMsg);
        else {
	    my_syslog(Severity,  LogMsg);
	}
    }
}
void my_syslog(int Severity,  const char * LogMsg)
{
  switch (Severity) 
  { 
    case LOG_DEBUG : fprintf (log_fp, "LOG_DEBUG"); break; 
    case LOG_NOTICE : fprintf (log_fp, "LOG_NOTICE"); break;
    case LOG_WARNING : fprintf (log_fp, "LOG_WARNING"); break;
    case LOG_INFO :fprintf (log_fp, "LOG_INFO"); break;
    case LOG_ERR :fprintf (log_fp, "LOG_ERR"); break;
    case LOG_ALERT :fprintf (log_fp, "LOG_ALERT"); break;
    case LOG_CRIT :fprintf (log_fp, "LOG_CRIT"); break;
    default:fprintf (log_fp, "LOG_UNKNOWN"); break;
  }
  fprintf (log_fp, " : %s\n", LogMsg);
  fflush(log_fp);
}
