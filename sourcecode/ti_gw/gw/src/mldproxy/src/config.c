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
**   mldproxy - MLD proxy based multicast router 
**   This software is derived work from the igmproxy software. The original
**   source code has been modified from it's original state by the author
**   of mldproxy.
**
**   Includes Intel Corporation's changes/modifications dated:
**     30-Nov-2011: Provided includes and function prototypes, 
**     reworked configuration parameters, structures and part of logics
**     removed obsolete parts non-relevant for IPv6
**  
**   Changed/modified portions - Copyright(C) 2011-2012, Intel Corporation.
**  
**  ------------------------------------------------------------------------------
*/

#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include "defs.h"
#include "debug.h"
#include "config.h"
#include "vif.h"
#include "mld6_proto.h"
/**
*   config.c - Contains functions to load and parse config
*              file, and functions to configure the daemon.              
*/
// Keeps common configuration settings 

struct vifconfig
{
  struct vifconfig *next;	// Next config in list...
  char *name;
  short state;
  int ratelimit;
  int threshold;


  unsigned int protocolV;
  unsigned int robustnessValue;
  unsigned int queryInterval;
  unsigned int queryResponseInterval;
  // Used on startup..
  unsigned int startupQueryInterval;
  unsigned int startupQueryCount;
  // Last member probe...
  unsigned int lastMemberQueryInterval;
  unsigned int lastMemberQueryCount;
  // Set if upstream leave messages should be sent instantly..
  unsigned short fastUpstreamLeave;
};


// Structure to keep vif configuration
struct vifconfig *vifconf;

// Keeps common settings...
static struct vifconfig commonConfig;

// Prototypes..
int parsePhyintToken (int index, struct mvif *v);
void applydefaults (struct mvif *v);

// from confread.c
char *getCurrentConfigToken (void);
char *nextConfigToken (void);





char *nextConfigToken (void);

/**
*   Loads the configuration from file, and stores the config in 
*   respective holders...
*/
int
loadConfig (char *configFile)
{
    char *token;
    int countInfces = 0;
    /* ARRIS ADD START */
    int max_vif_count = sizeof(mvifs)/sizeof(mvifs[0]);
    /* ARRIS ADD END */
  upstream_idx = upStreamVif = -1;

  // Test config file reader...
  if (!openConfigFile (configFile))
    {
      log_msg (LOG_ERR, 0, "MLDPROXY:Unable to open config file  %s",
	       configFile);
    }

  // Get first token...
  token = nextConfigToken ();
  if (token == NULL)
    {
      log_msg (LOG_ERR, 0, "MLDPROXY:Config file was empty.");
    }

  // Loop until all configuration is read.
    /* ARRIS MOD START */
    while (token != NULL && countInfces < max_vif_count)
    /* ARRIS MOD END */    
    {
      register struct mvif *v = &mvifs[countInfces];
      int rc;
      // Check token...
      if (strcasecmp ("phyint", token) == 0)
	{
	  // Got a phyint token... - configuration line for interface
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0, "Config: Got a phyint token.");
	  rc = parsePhyintToken (countInfces, v);	//Call  parser
	  if (rc == 0)
	    {
	      // Unparsable token... Exit...
	      closeConfigFile ();
	      log_msg (LOG_ERR, 0,
		       "MLDPROXY:Unknown token '%s' in configfile", token);
	      return 0;
	    }
	  else
	    {

	      // RESULTS OF PARSER
	      IF_DEBUG (DEBUG_IF)
	      {
		log_msg (LOG_DEBUG, 0, "IF name : %s", v->mvif_name);

		log_msg (LOG_DEBUG, 0, "MLD Version : %d", v->uv_mld_version);
		log_msg (LOG_DEBUG, 0, "Robustness : %d",
			 v->uv_mld_robustness);
		log_msg (LOG_DEBUG, 0, "StartupQueryCount : %d",
			 v->uv_startupQueryCount);
		log_msg (LOG_DEBUG, 0, "StartupQueryInterval : %d",
			 v->uv_startupQueryInterval);
		log_msg (LOG_DEBUG, 0, "LLQI : %d", v->uv_mld_llqi);
	      }

	      applydefaults (v);	// complent configuration values by defaults
	      IF_DEBUG (DEBUG_IF)
	      {
		log_msg (LOG_DEBUG, 0, "IF name : %s", v->mvif_name);

		log_msg (LOG_DEBUG, 0, "MLD Version : %d", v->uv_mld_version);
		log_msg (LOG_DEBUG, 0, "Robustness : %d",
			 v->uv_mld_robustness);
		log_msg (LOG_DEBUG, 0, "StartupQueryCount : %d",
			 v->uv_startupQueryCount);
		log_msg (LOG_DEBUG, 0, "StartupQueryInterval : %d",
			 v->uv_startupQueryInterval);
		log_msg (LOG_DEBUG, 0, "LLQI : %d", v->uv_mld_llqi);
	      }
	      v->uv_ifindex = if_nametoindex (v->mvif_name);
	      if (v->uv_ifindex < 0)
		{
		  log_msg (LOG_ERR, errno,
			   "MLDPROXY:Fatal: Kernel can not make index from ifname =%s",
			   v->mvif_name);
		  return -1;
		}
	      countInfces++;
	    }
	}
      else if (strcasecmp ("fastleave", token) == 0)
	{
	  // Got fastleave a  token....
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "MLDPROXY:Config: Fast leave mode enabled.");
	  commonConfig.fastUpstreamLeave = 1;

	  // Read next token...
	  token = nextConfigToken ();
	  continue;
	}
      else
	{
	  // Unparsable token... Exit...
	  closeConfigFile ();
	  log_msg (LOG_WARNING, 0,
		   "MLDPROXY:Unknown token '%s' in configfile", token);
	  return 0;
	}
      // Get token that was not recognized by phyint parser.
      token = getCurrentConfigToken ();
    }
    if (upstream_idx < 0 )
    {
	  log_msg (LOG_ERR, 0,
			   "MLDPROXY:No upstream (WAN SIDE) interface found in config file ");
            exit(15);
     }
  // Close the configfile...
  closeConfigFile ();

  return countInfces;
}

void
applydefaults (struct mvif *v)
{
  v->uv_mld_query_interval =
    (v->uv_mld_query_interval >
     0) ? v->uv_mld_query_interval : MLD6_DEFAULT_QUERY_INTERVAL;
  v->uv_mld_robustness =
    (v->uv_mld_robustness >
     0) ? v->uv_mld_robustness : MLD6_DEFAULT_ROBUSTNESS;
  v->uv_mld_version =
    (v->uv_mld_version > 0) ? v->uv_mld_version : MLD6_DEFAULT_VERSION;
  v->uv_mld_llqi = (v->uv_mld_llqi > 0) ? v->uv_mld_llqi : MLD6_DEFAULT_LAST_LISTENER_QUERY_INTERVAL ;
  v->uv_fastleave = (v->uv_fastleave > 0) ? v->uv_fastleave : 0;
  v->uv_flags = (v->uv_flags > 0) ? v->uv_flags : VIFF_DOWNSTREAM;
  v->uv_startupQueryCount =
    (v->uv_startupQueryCount >
     0) ? v->uv_startupQueryCount : MLD6_DEFAULT_ROBUSTNESS;
  v->uv_startupQueryInterval =
    (v->uv_startupQueryInterval >
     0) ? v->uv_startupQueryInterval : MLD6_DEFAULT_QUERY_INTERVAL;
}


/**
*   Internal function to parse phyint config
*/
int
parsePhyintToken (int countInfces, struct mvif *v)
{

  char *token;
  short parseError = 0;

  // First token should be the interface name....
  token = nextConfigToken ();

  // Sanity check the name...
  if (token == NULL)
    return 0;
  if (strlen (token) >= IF_NAMESIZE)
    {
      log_msg (LOG_ERR, 0, "MLDPROXY:Error: IF name too long  interface %s.",
	       token);
      return 0;
    }
  IF_DEBUG (DEBUG_IF)
    log_msg (LOG_DEBUG, 0, "MLDPROXY:Config: IF: Config for interface %s.",
	     token);




  // Set default values...


  v->uv_flags = VIFF_DOWNSTREAM;
  v->uv_fastleave = 0;

  // Make a copy of the token to store the IF name


  strncpy (v->mvif_name, token, sizeof (v->mvif_name));

  // Parse the rest of the config..
  token = nextConfigToken ();
  while (token != NULL)
    {
      if (strcasecmp ("upstream", token) == 0)
	{
	  // Upstream
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0, "Config: IF %s: Got upstream token.",
		     v->mvif_name);
	  v->uv_flags = VIFF_UPSTREAM;
	  upstream_idx = if_nametoindex (v->mvif_name);
	  upStreamVif = countInfces;
	}
      else if (strcasecmp ("downstream", token) == 0)
	{
	  // Downstream
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0, "Config: IF %s: Got downstream token.",
		     v->mvif_name);
	  v->uv_flags = VIFF_DOWNSTREAM;
	}
      else if (strcasecmp ("disabled", token) == 0)
	{
	  // Disabled
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0, "Config: IF %s: Got disabled token.",
		     v->mvif_name);
	  v->uv_flags = VIFF_DISABLED;
	}
      else if (strcasecmp ("protocolV", token) == 0)
	{
	  // protocolV...
	  int n;
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "Config: IF %s: Got protocolV token '%s'.",
		     v->mvif_name, token);
	  token = nextConfigToken ();
	  n = atoi (token);
	  if ((n > 2) || (n < 1))
	    {
	      log_msg (LOG_WARNING, 0,
		       "MLDPROXY:protocolV must 1 or 2 , not  %d", n);
	      parseError = 1;
	      break;
	    }
	  v->uv_mld_version = n;
	}
      else if (strcasecmp ("fastleave", token) == 0)
	{
	  //  queryResponce Interval
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "MLDPROXY:Config: Fast leave mode enabled.");
	  v->uv_fastleave = 1 ;

	}
      else if (strcasecmp ("llqi", token) == 0)
	{
	  // Last Listener queryInterval
	  int n=0;
	  token = nextConfigToken ();
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "MLDPROXY:Config: IF %s: Got llqi token '%s'.",
		     v->mvif_name, token);
	  n = atoi (token);
	  if (n < 1)
	    {
	      log_msg (LOG_WARNING, 0,
		       "MLDPROXY:llqi must be 1 sec or more and not %d",
		       n);
	      parseError = 1;
	      break;
	    }
	  v->uv_mld_llqi = n;

	}
      else if (strcasecmp ("llqc", token) == 0)
	{
	  // Last Listener query Count
	  int n;
	  token = nextConfigToken ();
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "MLDPROXY:Config: IF %s: Got llqc token '%s'.",
		     v->mvif_name, token);
	  n = atoi (token);


	  if (n < 0)
	    {
	      log_msg (LOG_WARNING, 0,
		       "MLDPROXY:llqc must be  greater (or eq) then 1 not %d",
		       n);
	      parseError = 1;
	      break;
	    }
	  v->uv_mld_llqc = n -1; /* RFC Says llgc - 1 attepts to retransmit */
	  v->uv_mld_robustness = n;
	}
      else if (strcasecmp ("startupQueryCount", token) == 0)
	{
	  // startupQueryCount
	  int n;

	  token = nextConfigToken ();
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "Config: IF %s: Got startupQueryCount token '%s'.",
		     v->mvif_name, token);
	  n = atoi (token);
	  if (n < 0)
	    {
	      log_msg (LOG_WARNING, 0,
		       "MLDPROXY:startupQueryCount must be  greater (or eq) then 1 not %d",
		       n);
	      parseError = 1;
	      break;
	    }
	  v->uv_startupQueryCount = n;
	}
      else if (strcasecmp ("queryResponseInterval", token) == 0)
	{
	  // queryResponseInterval
	  int n;
	  token = nextConfigToken ();
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "MLDPROXY:Config: IF %s: Got queryResponseInterval token '%s'.",
		     v->mvif_name, token);
	  n = atoi (token);

	  if (n < 0 )
	    {
	      log_msg (LOG_WARNING, 0,
		       "queryResponseInterval must be positive");
	      parseError = 1;
	      break;
	    }
	  v->uv_mld_query_rsp_interval = n; 
	}
      else if (strcasecmp ("startupQueryInterval", token) == 0)
	{
	  // startupQueryInterval
	  int n;
	  token = nextConfigToken ();
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "Config: IF %s: Got startupQueryInterval token '%s'.",
		     token);
	  n = atoi (token);
	  if (n < 0)
	    {
	      log_msg (LOG_WARNING, 0,
		       "startupQueryInterval must be  greater (or eq) then 1 not %d",
		       n);
	      parseError = 1;
	      break;
	    }
	  v->uv_startupQueryInterval = n;
	}
      else if (strcasecmp ("QueryInterval", token) == 0)
	{
	  // QueryInterval
	  int n;
	  token = nextConfigToken ();
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0,
		     "Config: IF %s: Got QueryInterval token '%s'.",
		     token);
	  n = atoi (token);
	  if (n < 0)
	    {
	      log_msg (LOG_WARNING, 0,
		       "QueryInterval must be  greater (or eq) then 1 not %d",
		       n);
	      parseError = 1;
	      break;
	    }
	  v->uv_mld_query_interval = n;
	}
      else if (strcasecmp ("robustness", token) == 0)
	{
	  // Threshold
	  int n;

	  token = nextConfigToken ();
	  IF_DEBUG (DEBUG_IF)
	    log_msg (LOG_DEBUG, 0, "Config: IF: Got robustness token '%s'.",
		     token);
	  n = atoi (token);

	  if (n <= 0 || n > 255)
	    {
	      log_msg (LOG_WARNING, 0,
		       "robustness must be between 1 and 255.");
	      parseError = 1;
	      break;
	    }
	  v->uv_mld_llqc = n -1;
	  v->uv_mld_robustness = n;
	}
      else
	{
	  // Unknown token. Break...
	  break;
	}
      token = nextConfigToken ();
    }

  // Clean up after a parseerror...
  if (parseError)
    {
      return 0;
    }

  return 1;
}
