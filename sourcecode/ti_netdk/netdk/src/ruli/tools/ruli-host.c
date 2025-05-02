/*-GNU-GPL-BEGIN-*
RULI - Resolver User Layer Interface - Querying DNS SRV records
Copyright (C) 2004 Everton da Silva Marques

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
  $Id: ruli-host.c,v 1.4 2004/11/10 15:29:39 evertonm Exp $
 */


#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

#include <ruli.h>

#include "trivial_conf_handler.h"


#undef RULI_HOST_DEBUG


const char *prog_name;


static void *clean_query(ruli_host_t *qry, char *domain)
{
  ruli_free(domain);
  ruli_host_query_delete(qry);
  ruli_free(qry);

  return OOP_CONTINUE;
}

static void *on_answer(ruli_host_t *qry, void *arg)
{
  char      *domain  = (char *) arg;
  const int BUFSZ    = 1024;
  char      buf[BUFSZ];
  int       str_len;
  int       answer_code = ruli_host_answer_code(qry);

  assert(answer_code != RULI_HOST_CODE_VOID);

  if (answer_code == RULI_HOST_CODE_ALARM) {
    str_len = snprintf(buf, BUFSZ, "%s query-timeout\n", domain);

    assert(str_len < BUFSZ);

    printf(buf);
    
    return clean_query(qry, domain);
  }

  if (answer_code) {
    int rcode = ruli_host_rcode(qry);
    if (rcode != RULI_RCODE_NOERROR) {
      printf("%s bad-server-rcode: %d\n", domain, rcode);
      return clean_query(qry, domain);
    }

    str_len = snprintf(buf, BUFSZ, "%s query-failure: %d\n", 
		       domain, answer_code);
    assert(str_len < BUFSZ);
    printf(buf);

    return clean_query(qry, domain);
  }

#ifdef RULI_HOST_DEBUG
  {
    ruli_msg_header_t msg_hdr;

    /* debug only, DON'T do this in real programs >:] */
    msg_hdr = qry->host_query.answer_header;

    str_len = snprintf(buf, BUFSZ,
		       "%s: query succeded: domain=%s id=%d "
		       "rcode=%d qd=%d an=%d ns=%d ar=%d "
		       "answer_buf_size=%d answer_msg_len=%d\n", 
		       prog_name, domain, msg_hdr.id, msg_hdr.rcode, 
		       msg_hdr.qdcount, msg_hdr.ancount, 
		       msg_hdr.nscount, msg_hdr.arcount,
		       qry->host_query.answer_buf_size,
                       qry->host_query.answer_msg_len);

    assert(str_len < BUFSZ);

    printf(buf);
  }
#endif
  
  {
    ruli_list_t *addr_list = &qry->answer_addr_list;
    int addr_list_size = ruli_list_size(addr_list);
    int i;

    printf("%s", domain);
    for (i = 0; i < addr_list_size; ++i) {
      ruli_addr_t *addr = ruli_list_get(addr_list, i);
      printf(" ");
      ruli_addr_print(stdout, addr);
    }
    printf("\n");
  }

  return clean_query(qry, domain);
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

static ruli_host_t *submit_query(ruli_res_t *res_ctx,
                                 char *dname_buf, int dname_len,
                                 const char *domain, int domain_len,
                                 int cname)
{
  int         result;
  ruli_host_t *qry;
  char        *dom_str;

  /*
   * Allocate space for query
   */
  qry = (ruli_host_t *) ruli_malloc(sizeof(*qry));
  if (!qry)
    return 0;
  
  /*
   * Save domain string
   */
  dom_str = (char *) ruli_malloc(domain_len + 1);
  if (!dom_str) {
    ruli_free(qry);
    return 0;
  }

  memcpy(dom_str, domain, domain_len);
  dom_str[domain_len] = '\0';

  /*
   * Send query
   */

#ifdef RULI_HOST_DEBUG
  fprintf(stderr, 
	  "%s: DEBUG: submit_query(): domain=%s resolver=%u "
	  "dname_buf=%u qry=%u dname_len=%d\n", 
	  prog_name, dom_str, (unsigned int) res_ctx,
	  (unsigned int) dname_buf, (unsigned int) qry,
	  dname_len);
#endif

  qry->host_resolver        = res_ctx;
  qry->host_on_answer       = on_answer;
  qry->host_on_answer_arg   = dom_str;
  qry->host_domain          = dname_buf;
  qry->host_domain_len      = dname_len;
  qry->host_options         = RULI_RES_OPT_VOID;
  qry->host_max_cname_depth = cname;

  result = ruli_host_query_submit(qry);
  if (result) {
    fprintf(stderr, 
	    "%s: ruli_host_query_submit() failed: %s [%d]\n", 
	    prog_name, ruli_host_errstr(result), result);
    ruli_free(dom_str);
    ruli_free(qry);
    return 0;
  }

  return qry;
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
    
#ifdef RULI_HOST_DEBUG
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

static void do_query(ruli_res_t *res_ctx, const char *name, int cname)
{
  int  name_len = strlen(name);
  char *dname_buf;
  int  dname_buf_len;
  int  dname_len;
  char *i;

  dname_buf_len = ruli_dname_encode_size(name, name_len);

  /* Allocate buffer */
  dname_buf = (char *) ruli_malloc(dname_buf_len);
  if (!dname_buf) {
    fprintf(stderr, 
	    "%s: do_query(): ruli_malloc(%d) failed: %s\n",
	    prog_name, dname_buf_len, strerror(errno));
    
    return;
  }
  
  i = ruli_dname_encode(dname_buf, dname_buf_len, name, name_len);
  if (!i) {
    const int DOM_BUFSZ = 256;
    char      dom_buf[DOM_BUFSZ];
    int       dom_len = RULI_MIN(name_len, DOM_BUFSZ - 1);
    
    memcpy(dom_buf, name, dom_len);
    dom_buf[dom_len] = '\0';
    
    fprintf(stderr, 
	    "%s: do_query(): can't encode domain: (total_len=%d displaying=%d) %s\n", 
	    prog_name, name_len, dom_len, dom_buf);
    
    return;
  }
  dname_len = i - dname_buf;

  /*
   * Send query
   */
  {
    ruli_host_t *qry = submit_query(res_ctx, dname_buf, dname_len,
				    name, name_len, cname);
    if (!qry) {
      fprintf(stderr, 
	      "%s: do_query(): could not send query\n",
	      prog_name);
      
      return;
    }
  }

}

static void solve(int retry, int timeout, int cname, const ruli_list_t *server_list)
{
  oop_source_sys      *source_sys; /* System event source */
  oop_source          *source;     /* Event registration interface */
  ruli_res_t          res_ctx;
  int                 result;
  ruli_conf_handler_t handler;

  /*
   * Create event source
   */
  create_oop_source(&source_sys, &source);

  /*
   * Initialize resolver
   */

  handler.opaque          = server_list;
  handler.search_loader   = load_search_list;
  handler.search_unloader = unload_search_list;
  handler.ns_loader       = load_ns_list;
  handler.ns_unloader     = unload_ns_list;

  res_ctx.res_conf_handler = &handler;
  res_ctx.res_source       = source;
  res_ctx.res_retry        = retry;
  res_ctx.res_timeout      = timeout;

  result = ruli_res_new(&res_ctx);
  if (result) {
    fprintf(stderr, 
	    "%s: can't create ruli resolver: %s [%d]\n", 
	    prog_name, ruli_res_errstr(result), result);
    exit(1);
  }

  /*
   * main solve loop
   */
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
	do_query(&res_ctx, tok, cname);
	
	tok = strtok_r(0, SEP, &ptr);
	if (!tok)
	  break;
      } /* for */

    } /* Scan tokens */

    /*
     * Run event loop (send queries, receive answers)
     */
    {
      void *oop_result = run_event_loop(source_sys);
      if (oop_result != OOP_CONTINUE)
	break;
    }

  } /* main solve loop */

  /*
   * Destroy resolver
   */
  ruli_res_delete(&res_ctx);

  /*
   * Destroy event source
   */
  oop_sys_delete(source_sys);
}

static void parse_servers(ruli_list_t *server_list, int serverc, 
			  const char **serverv)
{
  int i;
  assert(serverc >= 1);

  for (i = 0; i < serverc; ++i) {
    ruli_addr_t *addr = ruli_addr_parse_new(serverv[i]);
    if (!addr) {
      fprintf(stderr, "%s: can't save address: %s\n", prog_name, serverv[i]);
      exit(1);
    }

#ifdef RULI_HOST_DEBUG
    fprintf(stderr, "%s: saving server: ", prog_name);
    ruli_addr_print(stderr, addr);
    fprintf(stderr, "\n");
#endif

    {
      int result = ruli_list_push(server_list, addr);
      assert(!result);
    }

  } /* for */

}

int main(int argc, const char **argv) 
{
  int         retry;
  int         timeout;
  int         cname;
  int         serverc;
  const char  **serverv;
  ruli_list_t server_list; /* list of ruli_addr_t* */

  prog_name = argv[0];

  if (argc < 4) {
    fprintf(stderr, 
	    "usage: %s <retry> <timeout> <cname> <server1> [ ... <serverN> ]\n", 
	    prog_name);
    exit(1);
  }

  retry   = atoi(argv[1]);
  timeout = atoi(argv[2]);
  cname   = atoi(argv[3]);
  serverc = argc - 4;
  serverv = argv + 4;

  if (retry < 0) {
    fprintf(stderr, "%s: bad retry: %d\n", prog_name, retry);
    exit(1);
  }

  if (timeout < 0) {
    fprintf(stderr, "%s: bad timeout: %d\n", prog_name, timeout);
    exit(1);
  }

  if (cname < 0) {
    fprintf(stderr, "%s: bad cname: %d\n", prog_name, cname);
    exit(1);
  }

  {
    int result = ruli_list_new(&server_list);
    assert(!result);
  }

  parse_servers(&server_list, serverc, serverv);

  solve(retry, timeout, cname, &server_list);

  ruli_list_dispose_trivial(&server_list);

#ifdef RULI_HOST_DEBUG
  fprintf(stderr, "%s: done\n", prog_name);
#endif

  exit(0);
}

