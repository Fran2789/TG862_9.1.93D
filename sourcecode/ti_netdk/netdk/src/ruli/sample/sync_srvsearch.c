/*-GNU-GPL-BEGIN-*
RULI - Resolver User Layer Interface - Querying DNS SRV records
Copyright (C) 2003 Everton da Silva Marques

RULI is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RULI is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RULI; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
*-GNU-GPL-END-*/

/*
  $Id: sync_srvsearch.c,v 1.3 2004/06/21 00:22:37 evertonm Exp $
 */


#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

#include <ruli.h>

#include "parse_options.h"
#include "stdout_srv_list.h"


const int INBUFSZ = 1024;

const char *prog_name;


static void solve(const char *fullname, long options)
{
  int  name_len = strlen(fullname);
  char name[name_len + 1];

  char *txt_service;
  int  txt_service_len;
  char *txt_domain;
  int  txt_domain_len;

  memcpy(name, fullname, name_len + 1);

  /*
   * Split full domain name in service + domain
   * Example: _http._tcp.domain => _http._tcp + domain
   */

  {
    int  name_len  = strlen(name);
    char *past_end = name + name_len;
    char *i        = name;

    assert(name_len > 0);
    assert(name_len < INBUFSZ);

    if (*i != '_') {
      fprintf(stderr, 
	      "%s: solve(): could not match _service\n",
	      prog_name);
      
      return;
    }

    /*
     * Find domain
     */
    for (; i < past_end; ++i) {
      if (*i == '.') {
	++i;
	if (i < past_end) {
	  if (*i != '_')
	    break;
	}
      }
    }

    if (i >= past_end) {
      fprintf(stderr, 
	      "%s: solve(): could not split service/domain\n",
	      prog_name);
      
      return;
    }

    txt_service     = name;
    txt_service_len = i - name - 1;
    txt_domain      = i;
    txt_domain_len  = past_end - i;

    txt_service[txt_service_len] = '\0';
  }

  /*
   * Submit synchronous query
   */
  {
    ruli_sync_t *sync_query = ruli_sync_query(txt_service, txt_domain, 
					      -1, options);
    int         srv_code;

    /*
     * Sync query failure?
     */

    if (!sync_query) {
      printf("%s could-not-submit-query\n", fullname);
      return;
    }

    /*
     * Underlying SRV query failure?
     */

    srv_code = ruli_sync_srv_code(sync_query);

    assert(srv_code != RULI_SRV_CODE_VOID);

    /* Timeout ? */
    if (srv_code == RULI_SRV_CODE_ALARM) {
      printf("%s timeout\n", fullname);

      ruli_sync_delete(sync_query);
      return;
    }

    /* Service provided ? */
    if (srv_code == RULI_SRV_CODE_UNAVAILABLE) {
      printf("%s srv-service-not-provided\n", fullname);

      ruli_sync_delete(sync_query);
      return;
    }

    if (srv_code) {
      int rcode = ruli_sync_rcode(sync_query);

      printf("%s srv-query-failed: srv_code=%d rcode=%d\n", 
	     fullname, srv_code, rcode);

      ruli_sync_delete(sync_query);
      return;
    }

    /*
     * Show the result
     */

    show_srv_list(fullname, ruli_sync_srv_list(sync_query));

    ruli_sync_delete(sync_query);
  }
}

static void go(long options)
{
  char inbuf[INBUFSZ];

  /*
   * Scan stdin
   */
  for (;;) {
    if (!fgets(inbuf, INBUFSZ, stdin)) {
      if (feof(stdin))
	break;

      fprintf(stderr, 
	      "%s: reading from stdin: %s\n", 
	      prog_name, strerror(errno));

      continue;
    }

    /*
     * Scan tokens
     */
    {
      const char *SEP = "\r\n\t ";
      char       *ptr;
      char       *tok;

      tok = strtok_r(inbuf, SEP, &ptr);
      if (!tok)
	continue;

      for (;;) {

	/*
	 * Make SRV query for token
	 */
	solve(tok, options);

	tok = strtok_r(0, SEP, &ptr);
	if (!tok)
	  break;
      } /* for */

    } /* Scan tokens */

  }
}


int main(int argc, const char *argv[]) 
{
  long options;
  prog_name = argv[0];

  options = parse_options(argc, argv, 1);

  go(options);

  exit(0);
}

