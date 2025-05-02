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
  $Id: smtpsearch.c,v 1.3 2004/06/21 00:22:37 evertonm Exp $
 */


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>

#include <ruli.h>

#include "parse_options.h"
#include "stdout_srv_list.h"


const char *prog_name;

#define QBUFSZ RULI_LIMIT_DNAME_TEXT_BUFSZ

/*
 * Store query buffers
 */
typedef struct {
  char txt_domain[QBUFSZ];
  int  txt_domain_len;
} srv_qbuf_t;

const int QBUF_SIZE = sizeof(srv_qbuf_t);


static void release_query(ruli_search_srv_t *search, srv_qbuf_t *qbuf)
{
  ruli_search_srv_delete(search);
  ruli_free(qbuf);
}

static void *on_search_answer(ruli_search_srv_t *search, void *search_arg)
{
  srv_qbuf_t *qbuf = (srv_qbuf_t *) search_arg;
  int        code;

  assert(search);

  code = ruli_search_srv_code(search);

  assert(code != RULI_SRV_CODE_VOID);

  /*
   * Timeout?
   */ 
  if (code == RULI_SRV_CODE_ALARM) {

    printf("%s timeout\n", 
	   qbuf->txt_domain);

    release_query(search, qbuf);

    return OOP_CONTINUE;
  }

  /*
   * Service is not provided by that domain?
   */ 
  if (code == RULI_SRV_CODE_UNAVAILABLE) {

    printf("%s smtp-service-not-provided\n", 
	   qbuf->txt_domain);

    release_query(search, qbuf);

    return OOP_CONTINUE;
  }

  /*
   * Other error?
   */ 
  if (code) {
    int rcode = ruli_search_srv_rcode(search);

    printf("%s srv-failure code=%d rcode=%d\n", 
	   qbuf->txt_domain, code, rcode);

    release_query(search, qbuf);

    return OOP_CONTINUE;
  }

  /*
   * Query successful
   */

  show_srv_list(qbuf->txt_domain, ruli_search_srv_answer_list(search));

  release_query(search, qbuf);

  return OOP_CONTINUE;
}

static void create_oop_source(oop_source_sys **source_sys, oop_source **source)
{
  /* Create the system event source */
  *source_sys = oop_sys_new();
  if (!*source_sys) {
    fprintf(stderr, 
	    "%s: can't create system event source: oop_sys_new() failed\n", 
	    prog_name);
    exit(1);
  }

  /* Get the system event registration interface */
  *source = oop_sys_source(*source_sys);
  if (!*source) {
    fprintf(stderr, 
	    "%s: can't get registration interface: oop_sys_source() failed\n",
	    prog_name);
    exit(1);
  }
}

static void *run_event_loop(oop_source_sys *source_sys)
{
  void *oop_result = oop_sys_run(source_sys);
  
  if (oop_result == OOP_ERROR)
    fprintf(stderr, 
	    "%s: oop system source returned error\n", prog_name);
  else if (oop_result == OOP_CONTINUE) {
    
    /*
     * Normal termination
     */
    
#ifdef SRVSEARCH_DEBUG
    fprintf(stderr, 
	    "%s: oop system source had no event registered\n", prog_name);
#endif
    
  }
  else if (oop_result == OOP_HALT)
    fprintf(stderr,
	    "%s: some sink requested oop system halt\n", prog_name);
  else
    fprintf(stderr,
	    "%s: unexpected oop system source result (!)\n", prog_name);

  return oop_result;
}

static void do_query(ruli_res_t *res_ctx, const char *domain, long options)
{
  srv_qbuf_t *qbuf;

  /* Allocate qbuf */
  qbuf = (srv_qbuf_t *) ruli_malloc(QBUF_SIZE);
  if (!qbuf) {
    fprintf(stderr, 
	    "%s: do_query(): could not allocate srv_qbuf_t: ruli_malloc(%d) failed\n",
	    prog_name, QBUF_SIZE);
    
    return;
  }

  qbuf->txt_domain_len = strlen(domain);
  assert(qbuf->txt_domain_len < QBUFSZ);
  strcpy(qbuf->txt_domain, domain);
  
  /*
   * Send query
   */

  {
    ruli_search_srv_t *search = ruli_search_smtp_submit(res_ctx,
							on_search_answer,
							qbuf,
							options,
							qbuf->txt_domain);
    if (!search) {
      fprintf(stderr, 
	      "%s: do_query(): could not send SRV query\n",
	      prog_name);
    
      ruli_free(qbuf);

      return;
    }
  }

  /*
   * Submitted
   */
}

static void go(long options)
{
  oop_source_sys    *source_sys;  /* System event source */
  oop_source        *source;      /* Event registration interface */
  ruli_search_res_t *search_res;
  ruli_res_t        *res;
  int               retry   = 2;
  int               timeout = 10; /* seconds */

  /*
   * Create event source
   */
  create_oop_source(&source_sys, &source);

  /*
   * Initialize resolver
   */

  search_res = ruli_search_res_new(source, retry, timeout, NULL);
  if (!search_res) {
    fprintf(stderr,
            "%s: can't create ruli resolver\n",
            prog_name);
    exit(1);
  }

  assert(search_res);

  res = ruli_search_resolver(search_res);

  assert(res);
  
  for (;;) {
    const int INBUFSZ = 1024;
    char      inbuf[INBUFSZ];

    /*
     * Read stdin
     */
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
	 * Submit query for token
	 */
	do_query(res, tok, options);
	
	tok = strtok_r(0, SEP, &ptr);
	if (!tok)
	  break;
      } /* for */

    } /* Scan tokens */

    /*
     * Run event loop
     */
    {
      void *oop_result = run_event_loop(source_sys);
      if (oop_result != OOP_CONTINUE)
	break;
    }
  } /* for */

  /*
   * Destroy resolver
   */
  ruli_search_res_delete(search_res);

  /*
   * Destroy event source
   */
  oop_sys_delete(source_sys);
}

int main(int argc, const char **argv) 
{
  long options;
  prog_name = argv[0];

  options = parse_options(argc, argv, 1);

  go(options);

  exit(0);
}

