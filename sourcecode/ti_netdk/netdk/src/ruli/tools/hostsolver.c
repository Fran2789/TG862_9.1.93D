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
  $Id: hostsolver.c,v 1.20 2005/06/07 22:17:51 evertonm Exp $
 */


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <ruli.h>

#include "stdin_domains.h"
#include "trivial_conf_handler.h"


const char *prog_name;

int qc;
int qt;

ruli_list_t output_list; /* list of null-terminated strings (char*) */


static void *on_stdout_write(oop_source *oop_src, int std_out, 
			     oop_event event, void *bogus)
{
  char *str;
  int  wr;
  int  len;

  assert(event == OOP_WRITE);
  
  str = (char *) ruli_list_pop(&output_list);

  len = strlen(str);

  wr = write(std_out, str, len);
  if (wr != len)
    fprintf(stderr, 
	    "%s: on_stdout_write(): write(): partial write: %d/%d\n", 
	    prog_name, wr, len);

  if (!ruli_list_size(&output_list))
    oop_src->cancel_fd(oop_src, std_out, OOP_WRITE);
  
  ruli_free(str);

  return OOP_CONTINUE;
}

static void enqueue_output(oop_source *oop_src, const char *str, int len, 
			   int std_out)
{
  int  output_queue_len;
  char *buf;

  ++len;

  buf = (char *) ruli_malloc(len);
  if (!buf) {
    fprintf(stderr, 
	    "%s: enqueue_output(): ruli_malloc() failed\n", 
	    prog_name);
    return;
  }
  memcpy(buf, str, len);

  output_queue_len = ruli_list_size(&output_list);

  assert(output_queue_len >= 0);

  if (!output_queue_len)
    oop_src->on_fd(oop_src, std_out, OOP_WRITE, on_stdout_write, 0);

  {
    int result = ruli_list_push(&output_list, buf);
    assert(!result);
  }
}

static void *clean_query(ruli_res_query_t *qry, char *domain)
{
  /* Finish query */
  ruli_res_query_delete(qry);
  
  /* Free buffers */
  ruli_free(domain);
  ruli_free(qry->q_domain);
  ruli_free(qry);

  return OOP_CONTINUE;
}

static void *on_answer(ruli_res_query_t *qry, void *arg)
{
  oop_source *oop_src = qry->resolver->res_source;
  int        std_out  = 1;
  char       *domain  = (char *) arg;
  const int  BUFSZ    = 1024;
  char       buf[BUFSZ];
  int        str_len;

  assert(qry->answer_code != RULI_CODE_VOID);

  if (qry->answer_code == RULI_CODE_TIMEOUT) {
    str_len = snprintf(buf, BUFSZ, "%s query-timeout\n", domain);

    assert(str_len < BUFSZ);

    enqueue_output(oop_src, buf, str_len, std_out);
    
    return clean_query(qry, domain);
  }

  if (qry->answer_code) {
    str_len = snprintf(buf, BUFSZ, "%s query-failure\n", domain);

    assert(str_len < BUFSZ);

    enqueue_output(oop_src, buf, str_len, std_out);

    return clean_query(qry, domain);
  }

#ifdef HOSTSOLVER_DEBUG
  {
    ruli_msg_header_t msg_hdr;

    msg_hdr = qry->answer_header;

    str_len = snprintf(buf, BUFSZ,
		       "%s: query succeded: domain=%s id=%d "
		       "rcode=%d qd=%d an=%d ns=%d ar=%d "
		       "answer_buf_size=%d answer_msg_len=%d\n", 
		       prog_name, domain, msg_hdr.id, msg_hdr.rcode, 
		       msg_hdr.qdcount, msg_hdr.ancount, 
		       msg_hdr.nscount, msg_hdr.arcount,
		       qry->answer_buf_size, qry->answer_msg_len);

    assert(str_len < BUFSZ);

    enqueue_output(oop_src, buf, str_len, std_out);
  }
#endif
  
  {
    ruli_parse_t parse;
    int          result;
    int          i;
    int          size;
    int          addr_count = 0;

    ruli_parse_new(&parse);

    result = ruli_parse_message(&parse, &qry->answer_header, 
				(ruli_uint8_t *) qry->answer_buf,
				qry->answer_buf_size);
    if (result) {
      str_len = snprintf(buf, BUFSZ, "%s answer-unparseable\n", domain);
      
      assert(str_len < BUFSZ);
      
      enqueue_output(oop_src, buf, str_len, std_out);
    }

    size = ruli_list_size(&parse.answer_list);
    for (i = 0; i < size; ++i) {
      ruli_addr_t addr;
      ruli_rr_t   *rr;

      rr = (ruli_rr_t *) ruli_list_get(&parse.answer_list, i);

      if (rr->qclass != RULI_RR_CLASS_IN)
	continue;

      switch (rr->type) {
      case RULI_RR_TYPE_A:

        result = ruli_parse_rr_a(&addr.addr.ipv4, rr->rdata, rr->rdlength);
        if (result)
          continue;

	ruli_addr_init(&addr, PF_INET);

        break;

      case RULI_RR_TYPE_AAAA:

        result = ruli_parse_rr_aaaa(&addr.addr.ipv6, rr->rdata, rr->rdlength);
        if (result)
          continue;

	ruli_addr_init(&addr, PF_INET6);

        break;

      default:
        continue;
      }

      {
	const int OWNER_BUFSZ = 256;
	char      owner_buf[OWNER_BUFSZ];
	int       owner_len;

	result = ruli_dname_extract((ruli_uint8_t *) qry->answer_buf, 
				    (ruli_uint8_t *) qry->answer_buf + qry->answer_buf_size,
				    (ruli_uint8_t *) owner_buf, 
				    (ruli_uint8_t *) owner_buf + OWNER_BUFSZ,
				    rr->owner,
				    &owner_len);
	assert(!result);

#ifdef HOSTSOLVER_DEBUG
	fprintf(stderr, 
		"DEBUG: on_answer(): domain=%s txt_owner=%s "
		"txt_owner_len=%d\n", 
		domain, owner_buf, owner_len);
#endif

	if (!ruli_dname_match(domain, strlen(domain), owner_buf, owner_len))
	  continue;
      }

      str_len = snprintf(buf, BUFSZ, "%s ", domain);
      assert(str_len > 0);
      assert(str_len < BUFSZ);

      {
	int len = ruli_addr_snprint(buf + str_len , BUFSZ - str_len, &addr);
	assert(len > 0);
	assert(len < (BUFSZ - str_len));
	str_len += len;
      }

      {
	char *dst = strncat(buf, "\n", BUFSZ);
	assert(dst);
      }

      enqueue_output(oop_src, buf, str_len, std_out);

      ++addr_count;
    }
    
    ruli_parse_delete(&parse);

    if (!addr_count) {
      str_len = snprintf(buf, BUFSZ, "%s answer-missing-address\n", domain);
      assert(str_len < BUFSZ);
      enqueue_output(oop_src, buf, str_len, std_out);
    }
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

static ruli_res_query_t *submit_query(ruli_res_t *res_ctx,
				      char *dname_buf, int dname_len,
				      const char *domain, int domain_len)
{
  int              result;
  ruli_res_query_t *qry;
  char             *dom_str;

  /*
   * Allocate space for query
   */
  qry = (ruli_res_query_t *) ruli_malloc(sizeof(ruli_res_query_t));
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

#ifdef HOSTSOLVER_DEBUG
  fprintf(stderr, 
	  "%s: DEBUG: submit_query(): domain=%s resolver=%u "
	  "dname_buf=%u qry=%u dname_len=%d class=%d type=%d\n", 
	  prog_name, dom_str, (unsigned int) res_ctx,
	  (unsigned int) dname_buf, (unsigned int) qry,
	  dname_len, qc, qt);
#endif

  qry->q_on_answer     = on_answer;
  qry->q_on_answer_arg = dom_str;
  qry->q_domain        = dname_buf;
  qry->q_domain_len    = dname_len;
  qry->q_class         = qc;
  qry->q_type          = qt;
  qry->q_options       = RULI_RES_OPT_VOID;

  result = ruli_res_query_submit(res_ctx, qry);
  if (result) {
    fprintf(stderr, 
	    "%s: ruli_res_query_submit() failed: %s [%d]\n", 
	    prog_name, ruli_res_errstr(result), result);
    ruli_free(dom_str);
    ruli_free(qry);

    return 0;
  }

  return qry;
}

static void *on_stdin_read(oop_source *oop_src, int std_in, 
			   oop_event event, void *ctx)
{
  ruli_res_t *res_ctx = (ruli_res_t *) ctx;
  int        result;

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
    assert(0);
  }

  /*
    Scan possible hostnames from stdin
   */
  for (;;) {

    char        *i;

    /* plain domain */
    const int        domain_buf_size = IN_BUF_SIZE;
    char             domain[domain_buf_size];
    int              domain_len;
    
    /* encoded domain */
    char             *dname_buf;
    int              dname_buf_len;
    int              dname_len;

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
     * Now we have a hostname, submit a query for it
     */

    /*
     * Allocate space for encoded domain in dname_buf;
     */

    /* Find required size */
    dname_buf_len = ruli_dname_encode_size(domain, domain_len);

    /* Allocate buffer */
    dname_buf = (char *) ruli_malloc(dname_buf_len);
    if (!dname_buf) {
      fprintf(stderr, 
	      "%s: on_stdin_read(): ruli_malloc(%d) failed: %s\n",
	      prog_name, dname_buf_len, strerror(errno));
      
      return OOP_CONTINUE;
    }

#ifdef HOSTSOLVER_DEBUG
    {
      /*
	Debug
      */
      const int DEBUG_BUFSZ = 256;
      char      debug_buf[DEBUG_BUFSZ];
      
      assert(domain_len < DEBUG_BUFSZ);
      
      memcpy(debug_buf, domain, domain_len);
      debug_buf[domain_len] = '\0';
      
      fprintf(stderr, 
	      "%s: DEBUG: on_stdin_read(): dname_buf=%u domain=%s domain_len=%d\n",
	      prog_name, (unsigned int) dname_buf, debug_buf, domain_len);
    }
#endif
    
    /*
     * Encode domain
     */
    
    i = ruli_dname_encode(dname_buf, dname_buf_len, domain, domain_len);
    if (!i) {
      const int DOM_BUFSZ = 256;
      char      dom_buf[DOM_BUFSZ];
      int       dom_len = RULI_MIN(domain_len, DOM_BUFSZ - 1);
      
      memcpy(dom_buf, domain, dom_len);
      dom_buf[dom_len] = '\0';

      fprintf(stderr, 
	      "%s: on_stdin_read(): can't encode domain: (total_len=%d displaying=%d) %s\n", 
	      prog_name, domain_len, dom_len, dom_buf);

      continue;
    }
    dname_len = i - dname_buf;
    
    /*
     * Send query
     */
    
    {
      ruli_res_query_t *qry = submit_query(res_ctx, dname_buf, dname_len,
					   domain, domain_len);
      if (!qry) {
	fprintf(stderr, 
		"%s: on_stdin_read(): could not send query\n",
		prog_name);
	
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
	      "%s: oop system source returned error\n", prog_name);
    else if (oop_result == OOP_CONTINUE) {

      /*
       * Normal termination
       */

#ifdef HOSTSOLVER_DEBUG
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
      ruli_free(addr);
      exit(1);
    }

#ifdef HOSTSOLVER_DEBUG
    fprintf(stderr, "%s: saving server: ", prog_name);
    ruli_addr_printf(stderr, addr);
    fprintf(stderr, "\n");
#endif

    {
      int result = ruli_list_push(server_list, addr);
      assert(!result);
    }

  } /* server for() loop */
}

int main(int argc, const char **argv) 
{
  const char *qclass = "in";
  const char *type  = "a";

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

  qc = ruli_get_qclass_code(qclass);
  if (!qc) {
    fprintf(stderr, "%s: can't find query class: %s\n", prog_name, qclass);
    exit(1);
  }

  qt = ruli_get_qtype_code(type);
  if (!qt) {
    fprintf(stderr, "%s: can't find query type: %s\n", prog_name, type);
    exit(1);
  }

  {
    int result = ruli_list_new(&output_list);
    assert(!result);
  }

  {
    int std_out = 1;
    set_non_blocking(std_out);
  }

  go(retry, timeout, &server_list);

  assert(!ruli_list_size(&output_list));

  ruli_list_delete(&output_list);

  ruli_list_dispose_trivial(&server_list);

#ifdef HOSTSOLVER_DEBUG
  fprintf(stderr, "%s: done\n", prog_name);
#endif

  exit(0);
}

