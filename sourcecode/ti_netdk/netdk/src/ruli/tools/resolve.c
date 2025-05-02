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
  This test program can redo one query N times.
  One query is resent only after the previous instance is finished.

  $Id: resolve.c,v 1.10 2004/06/16 17:28:45 evertonm Exp $
 */


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <ruli.h>

#include "trivial_conf_handler.h"


const char *prog_name;

int redo;


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

static void *perform_redo(ruli_res_t *res_ctx, ruli_res_query_t *qry)
{
  assert(redo >= 0);

  ruli_res_query_delete(qry);

  if (--redo >= 0)
    ruli_res_query_submit(res_ctx, qry);
  
  return OOP_CONTINUE;
}

static void *on_answer(ruli_res_query_t *res_qry, void *arg)
{
  ruli_res_t        *res_ctx = res_qry->resolver;
  ruli_msg_header_t msg_hdr;

  /* Just to check we received the same arg we passed */
  assert(res_qry == arg);

  if (res_qry->answer_code == RULI_CODE_TIMEOUT) {
      printf("%s: query timed out\n", prog_name);
      
      return perform_redo(res_ctx, res_qry);
  }

  if (res_qry->answer_code) {
    printf("%s: query failed with code=%d\n", 
	   prog_name, res_qry->answer_code);

    return perform_redo(res_ctx, res_qry);
  }

  msg_hdr = res_qry->answer_header;

  printf("%s: query succeded: id=%d rcode=%d qd=%d an=%d ns=%d ar=%d "
	 "answer_buf_size=%d answer_msg_len=%d\n", 
	 prog_name, msg_hdr.id, msg_hdr.rcode, msg_hdr.qdcount, 
	 msg_hdr.ancount, msg_hdr.nscount, msg_hdr.arcount,
	 res_qry->answer_buf_size, res_qry->answer_msg_len);

  return perform_redo(res_ctx, res_qry);
}

static void submit_query(ruli_res_t *res_ctx, ruli_res_query_t *res_qry,
			 const char *domain, int domain_len,
			 char *dname_buf, int dname_buf_len,
			 const char *qclass, const char *type)
{
  int  qc, qt;
  int  result;
  char *i;

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

  i = ruli_dname_encode(dname_buf, dname_buf_len, domain, domain_len);
  if (!i) {
    fprintf(stderr, "%s: can't encode domain: %s\n", prog_name, domain);
    exit(1);
  }

  res_qry->q_on_answer      = on_answer;
  res_qry->q_on_answer_arg  = res_qry;    /* bogus arg */
  res_qry->q_domain         = dname_buf;
  res_qry->q_domain_len     = i - dname_buf;
  res_qry->q_class          = qc;
  res_qry->q_type           = qt;
  res_qry->q_options        = RULI_RES_OPT_VOID;

  result = ruli_res_query_submit(res_ctx, res_qry);
  if (result) {
    fprintf(stderr, 
	    "%s: ruli_res_query_submit() failed: %s [%d]\n", 
	    prog_name, ruli_res_errstr(result), result);
    exit(1);
  }
}

static void perform_query(const char *domain, const char *qclass, 
			  const char *type, int retry, int timeout,
			  ruli_list_t *server_list)
{
  oop_source_sys      *source_sys; /* System event source */
  oop_source          *source;     /* Event registration interface */
  ruli_res_t          res_ctx;
  int                 result;
  ruli_conf_handler_t handler;

  int                 domain_len    = strlen(domain);
  int                 dname_buf_len = ruli_dname_encode_size(domain, domain_len);
  char                dname_buf[dname_buf_len];

  ruli_res_query_t res_qry;
  /* ruli_res_query_t res_qry2; */

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
   * Submit query
   */
  submit_query(&res_ctx, &res_qry, domain, domain_len,
	       dname_buf, dname_buf_len,
	       qclass, type);

  /*
   * Replay
   */
  /*
  submit_query(&res_ctx, &res_qry2, domain, domain_len,
	       dname_buf, dname_buf_len,
	       qclass, type);
  */

  /*
   * Run event loop
   */

  {
    void *oop_result = oop_sys_run(source_sys);

    if (oop_result == OOP_ERROR)
      printf("%s: oop system source returned error\n", prog_name);
    else if (oop_result == OOP_CONTINUE)
      printf("%s: oop system source had no event registered\n", prog_name);
    else if (oop_result == OOP_HALT)
      printf("%s: some sink requested oop system halt\n", prog_name);
    else
      printf("%s: unexpected oop system source result (!)\n", prog_name);
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
    ruli_addr_t *addr;

    addr = ruli_addr_parse_new(serverv[i]);
    if (!addr) {
      fprintf(stderr, "%s: can't create address: %s\n", prog_name, serverv[i]);
      exit(1);
    }

    printf("%s: saving server: ", prog_name);
    ruli_addr_print(stdout, addr);
    printf("\n");

    {
      int result = ruli_list_push(server_list, addr);
      assert(!result);
    }
    
  }
}

int main(int argc, const char **argv) 
{
  const char  *domain;
  const char  *qclass;
  const char  *type;
  int         retry;
  int         timeout;
  int         serverc;
  const char  **serverv;
  ruli_list_t server_list; /* list of ruli_addr_t* */

  prog_name = argv[0];

  if (argc < 8) {
    fprintf(stderr, 
	    "usage: %s <REDO> <domain> <class> <type> <retry> <timeout> <server1> [ ... <serverN> ]\n", 
	    prog_name);
    exit(1);
  }

  redo    = atoi(argv[1]);
  domain  = argv[2];
  qclass   = argv[3];
  type    = argv[4];
  retry   = atoi(argv[5]);
  timeout = atoi(argv[6]);
  serverc = argc - 7;
  serverv = argv + 7;

  if (redo < 0) {
    fprintf(stderr, "%s: bad REDO: %d\n", prog_name, redo);
    exit(1);
  }

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

  perform_query(domain, qclass, type, retry, timeout, &server_list);

  ruli_list_dispose_trivial(&server_list);

  fprintf(stderr, "%s: done\n", prog_name);

  exit(0);
}

