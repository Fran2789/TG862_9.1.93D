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
  $Id: srvsolver.c,v 1.26 2004/06/16 17:28:45 evertonm Exp $
 */


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include <ruli.h>

#include "stdin_domains.h"
#include "stdout_srv_list.h"
#include "trivial_conf_handler.h"


const char *prog_name;

#define QBUFSZ 256

/*
 * Store query buffers
 */
typedef struct {
  /* plain text domain names */
  char txt_service[QBUFSZ];
  int  txt_service_len;
  char txt_domain[QBUFSZ];
  int  txt_domain_len;

  /* label-encoded domain names */
  char raw_service[QBUFSZ];
  int  raw_service_len;
  char raw_domain[QBUFSZ];
  int  raw_domain_len;
} srv_qbuf_t;


#ifdef SRVSOLVER_DEBUG
static void debug_dump_buf(FILE *out, const char *fmt, 
			   const char *buf, int buf_len)
{
  const int MSG_BUFSZ = 512;
  char      msg[MSG_BUFSZ];

  assert(buf_len + 1 < MSG_BUFSZ);

  memcpy(msg, buf, buf_len);
  msg[buf_len] = '\0';

  fprintf(out, "%s: debug_dump_buf(): ", prog_name);
  fprintf(out, fmt, msg, buf_len);
  fprintf(out, "\n");
}
#endif


static void release_query(ruli_srv_t *srv_qry, srv_qbuf_t *qbuf)
{
  free(qbuf);
  ruli_srv_query_delete(srv_qry);
  free(srv_qry);
}

static void *on_srv_answer(ruli_srv_t *srv_qry, void *srv_qry_arg)
{
  srv_qbuf_t *qbuf = (srv_qbuf_t *) srv_qry_arg;

  assert(srv_qry->answer_code != RULI_SRV_CODE_VOID);

  /*
   * Timeout?
   */ 
  if (srv_qry->answer_code == RULI_SRV_CODE_ALARM) {

    printf("%s.%s timeout\n", 
	   qbuf->txt_service, qbuf->txt_domain);

    release_query(srv_qry, qbuf);

    return OOP_CONTINUE;
  }

  /*
   * Service is not provided by that domain?
   */ 
  if (srv_qry->answer_code == RULI_SRV_CODE_UNAVAILABLE) {

    printf("%s.%s service-not-provided\n", 
	   qbuf->txt_service, qbuf->txt_domain);

    release_query(srv_qry, qbuf);

    return OOP_CONTINUE;
  }

  /*
   * RCODE error?
   */ 
  if (srv_qry->answer_code) {

    printf("%s.%s srv-query-failed=%d ", 
	   qbuf->txt_service, qbuf->txt_domain, srv_qry->answer_code);

    switch (ruli_srv_rcode_kind(srv_qry)) {
    case RULI_SRV_RCODE_NONE:
      printf("non-rcode-failure\n");
      break;

    case RULI_SRV_RCODE_WALK:
      printf("target-walk-query-rcode=%d\n", ruli_srv_rcode(srv_qry));
      break;

    case RULI_SRV_RCODE_FALL:
      printf("fallback-query-rcode=%d\n", ruli_srv_rcode(srv_qry));
      break;

    case RULI_SRV_RCODE_SRV:
      printf("underlying-query-rcode=%d\n", ruli_srv_rcode(srv_qry));
      break;

    default:
      assert(0);
    }

    release_query(srv_qry, qbuf);

    return OOP_CONTINUE;
  }

  /*
   * Query successful
   */

  {
    char fullname[RULI_LIMIT_DNAME_TEXT_BUFSZ];

    snprintf(fullname, RULI_LIMIT_DNAME_TEXT_BUFSZ, "%s.%s", 
	     qbuf->txt_service, qbuf->txt_domain);

    show_srv_list(fullname, &srv_qry->answer_srv_list);
  }

  release_query(srv_qry, qbuf);

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


static int encode_srv_qbuf(srv_qbuf_t *qbuf)
{
  char *i;
  int  len;

  /*
   * Encode service
   */

  len = ruli_dname_encode_size(qbuf->txt_service, qbuf->txt_service_len);
  if (len > QBUFSZ)
    return -1;

  i = ruli_dname_encode(qbuf->raw_service, QBUFSZ, 
			qbuf->txt_service, qbuf->txt_service_len);
  if (!i)
    return -1;

  qbuf->raw_service_len = i - qbuf->raw_service;

  assert(len == qbuf->raw_service_len);

  /*
   * Encode domain
   */

  len = ruli_dname_encode_size(qbuf->txt_domain, qbuf->txt_domain_len);
  if (len > QBUFSZ)
    return -1;

  i = ruli_dname_encode(qbuf->raw_domain, QBUFSZ, 
			qbuf->txt_domain, qbuf->txt_domain_len);
  if (!i)
    return -1;

  qbuf->raw_domain_len = i - qbuf->raw_domain;

  assert(len == qbuf->raw_domain_len);

  return 0;
}

static ruli_srv_t *submit_query(ruli_res_t *res_ctx, srv_qbuf_t *qbuf)
{
  int        result;
  ruli_srv_t *srv_qry;

  /*
   * Allocate space for query
   */
  srv_qry = (ruli_srv_t *) malloc(sizeof(ruli_srv_t));
  if (!srv_qry)
    return 0;
  
  /*
   * Send query
   */

#ifdef SRVSOLVER_DEBUG
  fprintf(stderr, 
	  "%s: DEBUG: submit_query(): resolver=%u srv_qry=%u\n", 
	  prog_name, (unsigned int) res_ctx, (unsigned int) srv_qry);
#endif

  srv_qry->srv_resolver      = res_ctx;
  srv_qry->srv_on_answer     = on_srv_answer;
  srv_qry->srv_on_answer_arg = qbuf; 
  srv_qry->srv_service       = qbuf->raw_service;
  srv_qry->srv_service_len   = qbuf->raw_service_len;
  srv_qry->srv_domain        = qbuf->raw_domain;
  srv_qry->srv_domain_len    = qbuf->raw_domain_len;
  srv_qry->srv_fallback_port = -1;
  srv_qry->srv_options       = RULI_RES_OPT_VOID;

  result = ruli_srv_query_submit(srv_qry);
  if (result) {
    fprintf(stderr, 
	    "%s: ruli_srv_query_submit() failed: %s [%d]\n", 
	    prog_name, ruli_srv_errstr(result), result);
    free(srv_qry);

    return 0;
  }

  return srv_qry;
}

static void *on_stdin_read(oop_source *oop_src, int std_in, 
			   oop_event event, void *ctx)
{
  ruli_res_t *res_ctx = (ruli_res_t *) ctx;
  int        result;
  const int  QBUF_SIZE = sizeof(srv_qbuf_t);

  assert(std_in == 0);
  assert(event == OOP_READ);

  /*
   * Read stdin
   */

  result = read_stdin(std_in);

  switch (result) {
  case STDIN_READ_OK:
    break;

  case STDIN_READ_BLOCK:
    return OOP_CONTINUE;

  case STDIN_READ_EOF:
    /* Stop monitoring stdin */
    oop_src->cancel_fd(oop_src, std_in, OOP_READ);
    return OOP_CONTINUE;

  case STDIN_READ_ERROR:
    return OOP_HALT;

  case STDIN_READ_OVERFLOW:
    fprintf(stderr, 
	    "%s: on_stdin_read(): stdin read buffer overflow\n", 
	    prog_name);
    reset_stdin_buf();
    return OOP_CONTINUE;

  default:
    assert(0); /* NOT REACHED */
  }

  /*
   * Scan possible hostnames from stdin
   */
  for (;;) {

    srv_qbuf_t *qbuf;

    /* plain domain */
    const int domain_buf_size = IN_BUF_SIZE;
    char      domain[domain_buf_size];
    int       domain_len;
    
    /*
     * Parse hostname
     */

    result = get_next_domain(domain, domain_buf_size, &domain_len);
    
    /* If no domain found yet, keep waiting for one */
    if (result == PARSE_DOMAIN_NONE)
      return OOP_CONTINUE;
    
    /* Do not expect other errors, not even overflow */
    assert(!result);

    /*
     * Now we have the full domain name, submit a query for it
     */

    /*
     * Break full domain name in service + domain into qbuf
     */

    {
      char *past_end = domain + domain_len;
      char *i = domain;

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
		"%s: on_stdin_read(): could not split service/domain\n",
		prog_name);

	return OOP_CONTINUE;
      }

      /* Allocate qbuf */
      qbuf = (srv_qbuf_t *) malloc(QBUF_SIZE);
      if (!qbuf) {
	fprintf(stderr, 
		"%s: on_stdin_read(): could not allocate srv_qbuf_t: malloc(%d) failed\n",
		prog_name, QBUF_SIZE);
	
	return OOP_CONTINUE;
      }

      qbuf->txt_service_len = i - domain - 1;
      assert(qbuf->txt_service_len < QBUFSZ);
      memcpy(qbuf->txt_service, domain, qbuf->txt_service_len);
      qbuf->txt_service[qbuf->txt_service_len] = '\0';
      
#ifdef SRVSOLVER_DEBUG
      debug_dump_buf(stderr, 
		     "on_stdin_read(): txt_service=%s txt_service_len=%d", 
		     qbuf->txt_service, qbuf->txt_service_len);
#endif

      qbuf->txt_domain_len = past_end - i;
      assert(qbuf->txt_domain_len < QBUFSZ);
      memcpy(qbuf->txt_domain, i, qbuf->txt_domain_len);
      qbuf->txt_domain[qbuf->txt_domain_len] = '\0';

#ifdef SRVSOLVER_DEBUG
      debug_dump_buf(stderr, 
		     "on_stdin_read(): txt_domain=%s txt_domain_len=%d", 
		     qbuf->txt_domain, qbuf->txt_domain_len);
#endif
    }

    /*
     * Encode buffers in qbuf (txt => raw)
     */

    if (encode_srv_qbuf(qbuf)) {
      fprintf(stderr, 
	      "%s: on_stdin_read(): could not encode domain in srv_qbuf_t\n",
	      prog_name);

      free(qbuf);

      return OOP_CONTINUE;
    }
        
    /*
     * Send query
     */
    
    {
      ruli_srv_t *srv_qry = submit_query(res_ctx, qbuf);
      if (!srv_qry) {
	fprintf(stderr, 
		"%s: on_stdin_read(): could not send SRV query\n",
		prog_name);

	free(qbuf);
	
	return OOP_CONTINUE;
      }
    }
    
  } /* for loop */

  assert(0); /* NOT REACHED */

  return OOP_CONTINUE;
}

static void go(int retry, int timeout, ruli_list_t *server_list)
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
   * Monitor stdin for read
   */

  {
    int std_in = 0;

    set_non_blocking(std_in);

    source->on_fd(source, std_in, OOP_READ, on_stdin_read, &res_ctx);
  }

  /*
   * Run event loop
   */

  {
    void *oop_result = oop_sys_run(source_sys);

    if (oop_result == OOP_ERROR)
      fprintf(stderr, 
	      "%s: oop system source returned error\n", 
	      prog_name);
    else if (oop_result == OOP_CONTINUE) {

      /*
       * Normal termination
       */

#ifdef SRVSOLVER_DEBUG
      fprintf(stderr, 
	      "%s: oop system source had no event registered\n", 
	      prog_name);
#endif

    }
    else if (oop_result == OOP_HALT)
      fprintf(stderr,
	      "%s: some sink requested oop system halt\n", 
	      prog_name);
    else
      fprintf(stderr,
	      "%s: unexpected oop system source result (!)\n", 
	      prog_name);
  }

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
      free(addr);
      exit(1);
    }

#ifdef SRVSOLVER_DEBUG
    fprintf(stderr, "%s: saving server: ", prog_name);
    ruli_addr_print(stderr, addr);
    fprintf(stderr, "\n");
#endif

    {
      int result = ruli_list_push(server_list, addr);
      assert(!result);
    }
  }
}


int main(int argc, const char **argv) 
{
  int         retry;
  int         timeout;
  int         serverc;
  const char  **serverv;
  ruli_list_t server_list; /* list of ruli_addr_t* */

  prog_name = argv[0];

  if (argc < 4) {
    fprintf(stderr, 
	    "usage: %s <retry> <timeout> <server1> [ ... <serverN> ]\n", 
	    prog_name);
    exit(1);
  }

  retry   = atoi(argv[1]);
  timeout = atoi(argv[2]);
  serverc = argc - 3;
  serverv = argv + 3;

  if (retry < 0) {
    fprintf(stderr, "%s: bad retry: %d\n", prog_name, retry);
    exit(1);
  }

  if (timeout < 0) {
    fprintf(stderr, "%s: bad timeout: %d\n", prog_name, timeout);
    exit(1);
  }

  {
    int result = ruli_list_new(&server_list);
    assert(!result);
  }

  parse_servers(&server_list, serverc, serverv);

  go(retry, timeout, &server_list);

  ruli_list_dispose_trivial(&server_list);

  exit(0);
}

