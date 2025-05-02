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
  $Id: ruli_fsm.c,v 1.27 2005/06/21 15:25:07 evertonm Exp $


  TIMEOUT NOTES:

  The following operations are protected by explicit timeouts:
  - Waiting the answer for an UDP query
  - Waiting the answer for a TCP query

  Others are not subject to timeout restrictions.
 */
/*-------------------------------------------------------------------------------------
// Copyright 2007, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
// 1. Made changes to bind the DNS requests to a specified interface.
//
// THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
// "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
// OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
// DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
// COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
//
// These changes are covered as per original license.
//-------------------------------------------------------------------------------------*/

#include <stdio.h>      /* FIXME: remove me [used for fprintf() debug] */
#include <errno.h>      /* FIXME: remove me [used for strerror() debug] */
#include <string.h>     /* FIXME: remove me [used for strerror() debug] */

#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <stdlib.h>

#include <ruli_fsm.h>
#include <ruli_sock.h>
#include <ruli_mem.h>

/*
  Forward declarations
 */
static void query_switch_status(ruli_res_query_t *qry, int status);
static void query_done_write_udp(ruli_res_query_t *qry);
static void query_want_read_udp(ruli_res_query_t *qry);
static void query_done_read_udp(ruli_res_query_t *qry);

static void query_want_connect_tcp(ruli_res_query_t *qry);
static void query_done_connect_tcp(ruli_res_query_t *qry);
static void query_want_write_tcp(ruli_res_query_t *qry);
static void query_done_write_tcp(ruli_res_query_t *qry);
static void query_want_read_tcp_head(ruli_res_query_t *qry);
static void query_done_read_tcp_head(ruli_res_query_t *qry);
static void query_want_read_tcp_body(ruli_res_query_t *qry);
static void query_done_read_tcp_body(ruli_res_query_t *qry);

static void *on_udp_write(oop_source *oop_src, int udp_sd, 
			  oop_event event, void *res_qry);
static void *on_udp_read(oop_source *oop_src, int udp_sd, 
			 oop_event event, void *res_qry);

static void *on_tcp_connect(oop_source *oop_src, int tcp_sd, 
			    oop_event event, void *ctx);
static void *on_tcp_write(oop_source *oop_src, int tcp_sd, 
			  oop_event event, void *ctx);
static void *on_tcp_read(oop_source *oop_src, int tcp_sd, 
			 oop_event event, void *ctx);

static void queries_tcp_connect2write(ruli_list_t *query_list, 
				      int tcp_sd);

static int tcp_status(int status)
{
  return (status >= RULI_QRY_TCP_STAT_MIN) && 
    (status <= RULI_QRY_TCP_STAT_MAX);
}

/*
  fsm exit point
 */
static void *query_done(ruli_res_query_t *qry)
{
#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: fsm: query_done(): "
	  "id=%d answer_code=%d, rcode=%d status=%d\n",
	  qry->query_id, qry->answer_code, qry->answer_header.rcode,
	  qry->status);
#endif

  assert(qry->status == RULI_QRY_STAT_VOID);
  assert(qry->answer_code != RULI_CODE_VOID);
  assert(qry->answer_msg_len <= qry->answer_buf_size);

  /* 
   * In case of truncated TCP response, panic
   */
  if (!qry->answer_code)
    if (qry->answer_header.flags & RULI_MSG_MASK_TC)
      qry->answer_code = RULI_CODE_ANSWER_TRUNC;

  return _ruli_fsm_query_done(qry);
}

/*
  Used to schedule query_done() immediately
 */
static void *query_done_now(oop_source *oop_src,
			    struct timeval sched_tv, 
			    void *res_qry) 
{
  return query_done((ruli_res_query_t *) res_qry);
}

static void schedule_immediate_query_done(oop_source *oop_src, 
					  ruli_res_query_t *qry)
{
  oop_src->on_time(oop_src, OOP_TIME_NOW, query_done_now, qry);
}

/*
  Returns:
  -1: switch failed: finish scheduled with result 'code'
  0:  switch succeeded
 */
static int switch_else_schedule_finish(ruli_res_query_t *qry, int code)
{
  assert(qry->status == RULI_QRY_STAT_VOID);
  assert(code != RULI_CODE_VOID);
  assert(code != RULI_CODE_OK);

  /*
   * Is it possible to switch to another server?
   */

  if (ruli_res_switch_server(qry)) {

    /*
     * No: schedule finish
     */
    assert(!qry->answer_buf);
    assert(qry->answer_code == RULI_CODE_VOID);

    qry->answer_code = code;

    schedule_immediate_query_done(qry->resolver->res_source, qry);

    return -1;
  }

  /*
   * Yes: switched
   */

  return 0;
}

static void *on_udp_read_timeout(oop_source *oop_src,
				 struct timeval sched_tv, 
				 void *res_qry) 
{
  ruli_res_query_t *qry = (ruli_res_query_t *) res_qry;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: on_udp_read_timeout(): id=%d "
	  "udp_readers=%d udp6_readers=%d\n",
	  qry->query_id, 
	  qry->resolver->udp_readers,
	  qry->resolver->udp6_readers);
#endif

  query_done_read_udp(qry);

  if (switch_else_schedule_finish(qry, RULI_CODE_TIMEOUT)) {
    /*
     * Switch failed, finish scheduled
     */
    return OOP_CONTINUE;
  }

  /*
   * Switched
   */

  _ruli_query_want_write_udp(qry);

  return OOP_CONTINUE;
}

static void *tcp_read_timeout(ruli_res_query_t *qry)
{
  /*
   * Should we close the socket which
   * failed for reading right here?
   *
   * If we close the socket, we should
   * deal with all other queries using it,
   * naaahh...
   */

  if (switch_else_schedule_finish(qry, RULI_CODE_TIMEOUT)) {
    /*
     * Switch failed, finish scheduled
     */
    return OOP_CONTINUE;
  }

  /*
   * Switched
   */

  query_want_connect_tcp(qry);

  return OOP_CONTINUE;
}

static void *on_tcp_read_head_timeout(oop_source *oop_src,
				      struct timeval sched_tv, 
				      void *res_qry) 
{
  ruli_res_query_t *qry = (ruli_res_query_t *) res_qry;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "on_tcp_read_timeout(): id=%d\n",
	  qry->query_id);
#endif

  query_done_read_tcp_head(qry);

  return tcp_read_timeout(qry);
}

static void *on_tcp_read_body_timeout(oop_source *oop_src,
				      struct timeval sched_tv, 
				      void *res_qry) 
{
  ruli_res_query_t *qry = (ruli_res_query_t *) res_qry;

  query_done_read_tcp_body(qry);

  return tcp_read_timeout(qry);
}

static void query_switch_status(ruli_res_query_t *qry, int status)
{
#ifdef RULI_RES_DEBUG
  fprintf(stderr, "DEBUG: query_switch_status(id=%d index=%d): %d=>%d"
	  " udp_writers=%d udp_readers=%d"
	  " udp6_writers=%d udp6_readers=%d\n", 
	  qry->query_id, qry->resolver_index, qry->status, status,
	  qry->resolver->udp_writers, qry->resolver->udp_readers,
	  qry->resolver->udp6_writers, qry->resolver->udp6_readers);
#endif

  qry->status = status;
}

void _ruli_query_want_write_udp(ruli_res_query_t *qry)
{
  ruli_res_t *res_ctx = qry->resolver;
  oop_source *oop_src = res_ctx->res_source;
  ruli_addr_t *addr = ruli_res_get_curr_serv_addr(qry);
  
  assert(qry->status == RULI_QRY_STAT_VOID);

  /*
   * For each address family socket:
   *
   * Only one query registers the udp sd for event monitoring.
   * Many queries may later be waiting for write permission.
   * Thus we pass the resolver context to the on_udp_write event sink.
   * So any UDP_WANT_SEND-query can be selected in on_udp_write time.
   */

  switch (addr->addr_family) {
  case PF_INET:
    assert(res_ctx->udp_writers >= 0);

    if (!res_ctx->udp_writers)
      oop_src->on_fd(oop_src, res_ctx->udp_sd, OOP_WRITE, on_udp_write,
		     res_ctx);
  
    ++res_ctx->udp_writers;
    break;

  case PF_INET6:
    assert(res_ctx->udp6_writers >= 0);

    if (!res_ctx->udp6_writers)
      oop_src->on_fd(oop_src, res_ctx->udp6_sd, OOP_WRITE, on_udp_write,
		     res_ctx);
  
    ++res_ctx->udp6_writers;
    break;

  default:
    assert(0);
  }
  
  query_switch_status(qry, RULI_QRY_STAT_UDP_WANT_SEND);
}

static void query_done_write_udp(ruli_res_query_t *qry)
{
  ruli_res_t *res_ctx = qry->resolver;
  oop_source *oop_src = res_ctx->res_source;
  ruli_addr_t *addr = ruli_res_get_curr_serv_addr(qry);

  assert(qry->status == RULI_QRY_STAT_UDP_WANT_SEND);

  switch (addr->addr_family) {
  case PF_INET:

    assert(res_ctx->udp_writers > 0);

    --res_ctx->udp_writers;
    if (!res_ctx->udp_writers)
      oop_src->cancel_fd(oop_src, res_ctx->udp_sd, OOP_WRITE);

    break;

  case PF_INET6:

    assert(res_ctx->udp6_writers > 0);

    --res_ctx->udp6_writers;
    if (!res_ctx->udp6_writers)
      oop_src->cancel_fd(oop_src, res_ctx->udp6_sd, OOP_WRITE);

    break;

  default:
    assert(0);
  }

  query_switch_status(qry, RULI_QRY_STAT_VOID);
}

static void schedule_timeout(ruli_res_query_t *qry, 
			     oop_call_time *call)
{
  ruli_res_t *res_ctx = qry->resolver;
  oop_source *oop_src = res_ctx->res_source;

  {
    int result = gettimeofday(&qry->tv, 0);
    assert(!result);
  }

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: schedule_timeout(): query_id=%d now_is=%ld/%ld ", 
	  qry->query_id, qry->tv.tv_sec, qry->tv.tv_usec);
#endif

  qry->tv.tv_sec += res_ctx->res_timeout;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, "scheduling_for=%ld/%ld\n", 
	  qry->tv.tv_sec, qry->tv.tv_usec);
#endif

  oop_src->on_time(oop_src, qry->tv, call, qry);
}

static void query_want_read_udp(ruli_res_query_t *qry)
{
  ruli_res_t *res_ctx = qry->resolver;
  oop_source *oop_src = res_ctx->res_source;
  ruli_addr_t *addr = ruli_res_get_curr_serv_addr(qry);

  assert(qry->status == RULI_QRY_STAT_VOID);

  /*
   * For each socket address family:
   *
   * Only one query registers the sd for event monitoring.
   * Many queries may later be waiting for write permission.
   * Thus we pass the resolver context to the on_udp_write event sink.
   * So any UDP_WANT_SEND-query can be selected in on_udp_write time.
   */

  switch (addr->addr_family) {
  case PF_INET:

    assert(res_ctx->udp_readers >= 0);

    if (!res_ctx->udp_readers)
      oop_src->on_fd(oop_src, res_ctx->udp_sd, OOP_READ, on_udp_read, 
		     res_ctx);
    ++res_ctx->udp_readers;

    break;

  case PF_INET6:

    assert(res_ctx->udp6_readers >= 0);

    if (!res_ctx->udp6_readers)
      oop_src->on_fd(oop_src, res_ctx->udp6_sd, OOP_READ, on_udp_read, 
		     res_ctx);
    ++res_ctx->udp6_readers;

    break;

  default:
    assert(0);
  }

  /*
   * Set a timeout specific for this query.
   * Each query has its own timeout event registered.
   * The timeval is stored in the query context for later removal.
   */
  schedule_timeout(qry, on_udp_read_timeout);
  
  query_switch_status(qry, RULI_QRY_STAT_UDP_WANT_RECV);
}

static void query_done_read_udp(ruli_res_query_t *qry)
{
  ruli_res_t *res_ctx = qry->resolver;
  oop_source *oop_src = res_ctx->res_source;
  ruli_addr_t *addr = ruli_res_get_curr_serv_addr(qry);

  assert(qry->status == RULI_QRY_STAT_UDP_WANT_RECV);

  switch (addr->addr_family) {
  case PF_INET:

    assert(res_ctx->udp_readers > 0);

    --res_ctx->udp_readers;

    oop_src->cancel_time(oop_src, qry->tv, on_udp_read_timeout, qry);
    
    if (!res_ctx->udp_readers)
      oop_src->cancel_fd(oop_src, res_ctx->udp_sd, OOP_READ);
    
    break;

  case PF_INET6:

    assert(res_ctx->udp6_readers > 0);

    --res_ctx->udp6_readers;

    oop_src->cancel_time(oop_src, qry->tv, on_udp_read_timeout, qry);
    
    if (!res_ctx->udp6_readers)
      oop_src->cancel_fd(oop_src, res_ctx->udp6_sd, OOP_READ);
    
    break;
    
  default:
    assert(0);
  }

  query_switch_status(qry, RULI_QRY_STAT_VOID);
}

static void query_want_write_tcp(ruli_res_query_t *qry)
{
  ruli_res_t    *res_ctx = qry->resolver;
  oop_source    *oop_src = res_ctx->res_source;
  ruli_server_t *server  = ruli_res_get_curr_server(qry);

  assert(qry->status == RULI_QRY_STAT_VOID);
  assert(server->tcp_writers >= 0);

  /*
   * Only one query registers the sd for event monitoring.
   * Many queries may later be waiting for connection.
   * Thus we pass the resolver context to the on_tcp_write event sink.
   * So any TCP_WANT_SEND-query can be selected in on_tcp_write time.
   */
  if (!server->tcp_writers)
    oop_src->on_fd(oop_src, server->tcp_sd, OOP_WRITE, on_tcp_write, res_ctx);
    
  ++server->tcp_writers;
    
  query_switch_status(qry, RULI_QRY_STAT_TCP_WANT_SEND);
}

static void query_done_write_tcp(ruli_res_query_t *qry)
{
  ruli_res_t    *res_ctx = qry->resolver;
  oop_source    *oop_src = res_ctx->res_source;
  ruli_server_t *server  = ruli_res_get_curr_server(qry);

  assert(qry->status == RULI_QRY_STAT_TCP_WANT_SEND);
  assert(server->tcp_writers > 0);

  --server->tcp_writers;

  if (!server->tcp_writers)
    oop_src->cancel_fd(oop_src, server->tcp_sd, OOP_WRITE);

  query_switch_status(qry, RULI_QRY_STAT_VOID);
}

static void query_want_read_tcp_head(ruli_res_query_t *qry)
{
  ruli_res_t    *res_ctx = qry->resolver;
  oop_source    *oop_src = res_ctx->res_source;
  ruli_server_t *server  = ruli_res_get_curr_server(qry);

  assert(qry->status == RULI_QRY_STAT_VOID);
  assert(server->tcp_readers >= 0);

  /*
   * Only one query registers the sd for event monitoring.
   * Many queries may later be waiting for tcp read opportunity.
   * Thus we pass the resolver context to the on_tcp_read event sink.
   * So any TCP_WANT_READ-query can be selected in on_tcp_read time.
   */
  if (!server->tcp_readers)
    oop_src->on_fd(oop_src, server->tcp_sd, OOP_READ, on_tcp_read, res_ctx);
    
  ++server->tcp_readers;

  /* Reset received length of TCP header */
  server->tcp_head_len = 0;

  /*
   * Set a timeout specific for this query.
   * Each query has its own timeout event registered.
   * The timeval is stored in the query context for later removal.
   */
  schedule_timeout(qry, on_tcp_read_head_timeout);
    
  query_switch_status(qry, RULI_QRY_STAT_TCP_WANT_RECV_HEAD);
}

static void query_done_read_tcp_head(ruli_res_query_t *qry)
{
  ruli_res_t    *res_ctx = qry->resolver;
  oop_source    *oop_src = res_ctx->res_source;
  ruli_server_t *server  = ruli_res_get_curr_server(qry);

  assert(qry->status == RULI_QRY_STAT_TCP_WANT_RECV_HEAD);
  assert(server->tcp_readers > 0);

  /*
   * Disable timeout
   */
  oop_src->cancel_time(oop_src, qry->tv, on_tcp_read_head_timeout, qry);

  --server->tcp_readers;

  if (!server->tcp_readers)
    oop_src->cancel_fd(oop_src, server->tcp_sd, OOP_READ);

  query_switch_status(qry, RULI_QRY_STAT_VOID);
}

static void query_want_read_tcp_body(ruli_res_query_t *qry)
{
  ruli_res_t    *res_ctx = qry->resolver;
  oop_source    *oop_src = res_ctx->res_source;
  ruli_server_t *server  = ruli_res_get_curr_server(qry);

  assert(qry->status == RULI_QRY_STAT_VOID);
  assert(server->tcp_readers >= 0);
  assert(server->tcp_head_len == RULI_LIMIT_TCP_HEADER);

  /*
   * Only one query registers the sd for event monitoring.
   * Many queries may later be waiting for tcp read opportunity.
   * Thus we pass the resolver context to the on_tcp_read event sink.
   * So any TCP_WANT_READ-query can be selected in on_tcp_read time.
   */
  if (!server->tcp_readers)
    oop_src->on_fd(oop_src, server->tcp_sd, OOP_READ, on_tcp_read, res_ctx);
    
  ++server->tcp_readers;

  /*
   * Set a timeout specific for this query.
   * Each query has its own timeout event registered.
   * The timeval is stored in the query context for later removal.
   */
  schedule_timeout(qry, on_tcp_read_body_timeout);
    
  query_switch_status(qry, RULI_QRY_STAT_TCP_WANT_RECV_BODY);
}

static void query_done_read_tcp_body(ruli_res_query_t *qry)
{
  ruli_res_t    *res_ctx = qry->resolver;
  oop_source    *oop_src = res_ctx->res_source;
  ruli_server_t *server  = ruli_res_get_curr_server(qry);

  assert(qry->status == RULI_QRY_STAT_TCP_WANT_RECV_BODY);
  assert(server->tcp_readers > 0);

  /*
   * Disable timeout
   */
  oop_src->cancel_time(oop_src, qry->tv, on_tcp_read_body_timeout, qry);

  --server->tcp_readers;

  if (!server->tcp_readers)
    oop_src->cancel_fd(oop_src, server->tcp_sd, OOP_READ);

  query_switch_status(qry, RULI_QRY_STAT_VOID);
}

static void query_want_connect_tcp(ruli_res_query_t *qry)
{
  ruli_res_t    *res_ctx = qry->resolver;
  oop_source    *oop_src = res_ctx->res_source;
  ruli_server_t *server;

  /*
   * Scan available servers
   */

  for (;;) {
    ruli_addr_t *server_addr = ruli_res_get_curr_serv_addr(qry);
    int         server_port  = ruli_res_get_curr_serv_port(qry);
    int         result;

    server = ruli_res_get_curr_server(qry);

#ifdef RULI_RES_DEBUG
    fprintf(stderr, "DEBUG: query_want_connect_tcp(): trying ");
    ruli_addr_print(stderr, server_addr);
    fprintf(stderr, ":%d\n", server_port);
#endif

    assert(qry->status == RULI_QRY_STAT_VOID);

    /*
     * Server already connected?
     */
    if (server->tcp_sd != -1) {
      query_want_write_tcp(qry);
      break;
    }

    /*
     * Create socket
     */
    /*
     * Added a new third parameter which holds the interface to use to send out the 
     * DNS requests.
     */
    server->tcp_sd = ruli_sock_create_tcp(server_addr->addr_family, res_ctx->res_interface);
    if (server->tcp_sd == -1) {

      /*
       * Try to switch to next server.
       * If switched successfully, then loop.
       *
       * I'm not sure there is benefit in
       * switching server in this case. I
       * believe we could just schedule
       * query_done() immediately with
       * a failed result.
       */
      if (switch_else_schedule_finish(qry, RULI_CODE_CONNECT)) {
	/*
	 * Switch failed, finish scheduled: exit loop
	 */
	break; /* for */
      }

      /*
       * Switched: loop to try new server
       */
      continue; /* for */
    }

    /*
     * Try to connect to server
     */
    result = ruli_sock_connect(server->tcp_sd, server_addr, server_port);

    /*
     * If connected, exit loop
     */
    if (!result) {
      /*
       * We got an immediate connection and
       * must wait opportunity to write
       *
       * Does this really happens on any OS?
       */
      
      query_want_write_tcp(qry);
      
      /*
       * Switch OTHER wanna-connect queries on same socket
       * to wanna-write 
       *
       * Otherwise they would miss the "connected" event,
       * which occurs only once per socket descriptor
       */
      queries_tcp_connect2write(&res_ctx->query_list, server->tcp_sd);
      
      break;
    }

    /*
     * If connection is in progress, we'll wait
     * in wanna-connect mode
     */
    if (result == RULI_SOCK_WOULD_BLOCK) {

#ifdef RULI_RES_DEBUG
      fprintf(stderr, 
	    "DEBUG: query_want_connect_tcp(): would block\n");
#endif

      assert(server->tcp_connecters >= 0);

      /*
       * Only one query registers the sd for event monitoring.
       * Many queries may later be waiting for connection.
       * Thus we pass the resolver context to the on_tcp_connect event sink.
       * So any TCP_WANT_CONNECT-query can be selected in on_tcp_connect time.
       */
      if (!server->tcp_connecters)
	oop_src->on_fd(oop_src, server->tcp_sd, OOP_WRITE, on_tcp_connect, 
		       res_ctx);
      
      ++server->tcp_connecters;
    
      query_switch_status(qry, RULI_QRY_STAT_TCP_WANT_CONNECT);
    
      break;
    }

#ifdef RULI_RES_DEBUG
    fprintf(stderr, 
	    "DEBUG: query_want_connect_tcp(): connect failed\n");
#endif

    /*
     * Any other error, try to switch the server
     *
     * If switch fails, give up exiting the for-loop
     */
    if (switch_else_schedule_finish(qry, RULI_CODE_CONNECT)) {
      /*
       * Switch failed, finish scheduled
       */
      break;
    }

    /*
     * Switched: loop to try next server
     */

  } /* for */

}

static void query_done_connect_tcp(ruli_res_query_t *qry)
{
  ruli_res_t    *res_ctx = qry->resolver;
  oop_source    *oop_src = res_ctx->res_source;
  ruli_server_t *server  = ruli_res_get_curr_server(qry);

  assert(qry->status == RULI_QRY_STAT_TCP_WANT_CONNECT);
  assert(server->tcp_connecters > 0);

  --server->tcp_connecters;

  if (!server->tcp_connecters)
    oop_src->cancel_fd(oop_src, server->tcp_sd, OOP_WRITE);

  query_switch_status(qry, RULI_QRY_STAT_VOID);
}

/*
 * Note: As there is no timeout for the wanna-write
 *       condition, we don't need to disable the
 *       timeout here.
 *
 * udp_sd family is either PF_INET or PF_INET6.
 */
static void *on_udp_write(oop_source *oop_src, int udp_sd, 
			  oop_event event, void *ctx)
{
  ruli_res_t       *res_ctx = (ruli_res_t *) ctx;

  ruli_addr_t      *curr_server_addr;
  int              curr_server_port;
  int              result;

  ruli_res_query_t *qry = 0; /* picky compilers */
  int              i;
  int              queries;

  assert(event == OOP_WRITE);

  /*
   * Now we can write to the socket.
   *
   * As many queries may be waiting to write,
   * we need to pick (any) one of them.
   *
   * Note: Other possible approach is to try all
   * udp-wanna-write queries, as long as they
   * don't block. We currently don't do this.
   */

  queries = ruli_list_size(&res_ctx->query_list);

  /* There must exist at least one query */
  assert(queries > 0);

  for (i = 0; i < queries; ++i) {
    qry = (ruli_res_query_t *) ruli_list_get(&res_ctx->query_list, i);
    if (qry->status == RULI_QRY_STAT_UDP_WANT_SEND)
      break;
  }

  /* At least one UDP_WANT_SEND query MUST have been found */
  assert(i < queries); 

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: on_udp_write(): id=%d index=%d resolver=%u query=%u\n", 
	  qry->query_id, qry->resolver_index, 
	  (unsigned int) ctx, (unsigned int) qry);
#endif

  /*
   * Proceed to write to socket descriptor
   */

  curr_server_addr = ruli_res_get_curr_serv_addr(qry);
  curr_server_port = ruli_res_get_curr_serv_port(qry);
      
  result = ruli_sock_sendto(udp_sd, curr_server_addr, curr_server_port,
			    ruli_qry_udp_buf(qry), ruli_qry_udp_msg_len(qry));
      
  if (result == RULI_SOCK_WOULD_BLOCK)
    return OOP_CONTINUE;
      
  if (result == RULI_SOCK_SEND_FAIL) {

    query_done_write_udp(qry); 

    if (switch_else_schedule_finish(qry, RULI_CODE_SEND)) {
      /*
       * Switch failed, finish scheduled
       */
      return OOP_CONTINUE;
    }

    /*
     * Switched
     */

    _ruli_query_want_write_udp(qry); 

    return OOP_CONTINUE;
  }
    
  /* We don't expect other failures */
  assert(!result);

  /*
   * Change this query mode to read
   */
  query_done_write_udp(qry); 
  query_want_read_udp(qry);

  return OOP_CONTINUE;
}

static void *start_tcp_query(ruli_res_query_t *qry)
{
  int tcp_sd = _ruli_get_curr_tcp_socket(qry);

  /*
   * If socket disconnected: Set wanna-connect mode
   */
  if (tcp_sd == -1) {
    query_want_connect_tcp(qry);
    return OOP_CONTINUE;
  }

  /*
   * If socket connected: Set wanna-send mode
   */
  query_want_write_tcp(qry);

  return OOP_CONTINUE;
}

static int check_answer_flags(ruli_uint16_t flags, long options)
{
  /* We expect a query answer */
  if (!(flags & RULI_MSG_MASK_QR))
    return RULI_CODE_ANSWER_FLAG_QR;

  /* We expect an opcode for a standard query */
  if ((flags & RULI_MSG_MASK_OPCODE) != RULI_OPCODE_QUERY)
    return RULI_CODE_ANSWER_OPCODE;

  /* We expect the server returns RD as we previously sent */
  // ARRIS MOD   
  if(!(options & RULI_RES_OPT_IGNORE_RD))
  {
	  if (!(flags & RULI_MSG_MASK_RD))
		  return RULI_CODE_ANSWER_FLAG_RD;
  }
  // ARRIS MOD END
  
  /* Require recursive service? */
  if (options & RULI_RES_OPT_NEED_RA) 
    if (!(flags & RULI_MSG_MASK_RA))
      return RULI_CODE_ANSWER_FLAG_RA;
  
  /* RFC1035 states Z must be zero, so be it */
  if (flags & RULI_MSG_MASK_Z)
    return RULI_CODE_ANSWER_FLAG_Z;
  
  return RULI_CODE_OK;
}

#define UDP_READ_BUFSZ 2048

/*
  udp_sd family is either PF_INET or PF_INET6.
*/

static void *on_udp_read(oop_source *oop_src, int udp_sd, 
			 oop_event event, void *ctx)
{
  ruli_res_t        *res_ctx = (ruli_res_t *) ctx;
  ruli_uint8_t      buf[UDP_READ_BUFSZ];
  union {
    struct sockaddr_in inet;
    struct sockaddr_in6 inet6;
  } sa;
  socklen_t         sa_len = sizeof(sa);
  int               rd;
  int               result;
  ruli_res_query_t  *qry;
  ruli_msg_header_t msg_hdr;

  assert(event == OOP_READ);

  /* Ensure enough work space */
  assert(UDP_READ_BUFSZ >= RULI_LIMIT_DATAGRAM_HIGH);

  /*
   * Receive message
   */
  result = ruli_sock_recvfrom(udp_sd, buf, UDP_READ_BUFSZ, &rd, 
			      (struct sockaddr *) &sa, &sa_len);
  if (result == RULI_SOCK_WOULD_BLOCK)
    return OOP_CONTINUE;

  /* If we can't receive the message, we can't identify the query */
  if (result == RULI_SOCK_RECV_FAIL)
    return OOP_CONTINUE;

  /* We don't expect other failures */
  assert(!result);

  /* Ignore message too long for DNS protocol */
  if (rd > RULI_LIMIT_DATAGRAM_HIGH)
    return OOP_CONTINUE;

  /* If we can't parse the header, ignore the message */
  if (ruli_msg_parse_header(&msg_hdr, buf, rd))
    return OOP_CONTINUE;

  /* If we can't find the query, ignore the message */
  qry = ruli_res_find_query_by_id(&res_ctx->query_list, msg_hdr.id);
  if (!qry)
    return OOP_CONTINUE;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: on_udp_read(): "
	  "id=%d index=%d resolver=%u qry=%u status=%d\n", 
	  qry->query_id, qry->resolver_index, (unsigned int) qry->resolver,
	  (unsigned int) qry, qry->status);
#endif

  /* 
   * If the found query is not waiting for the answer, ignore the message.
   * One reason for this is: we may have switched to another server due
   * to timeout, then the previous server answer arrives.
   */
  if (qry->status != RULI_QRY_STAT_UDP_WANT_RECV)
    return OOP_CONTINUE;

  /* Query must be attached to a resolver */
  assert(qry->resolver);

  /* Query must be attached to right resolver */
  assert(qry->resolver == res_ctx);

#ifndef NDEBUG
  /* Make sure the query was waiting on the proper socket */
  {
    ruli_addr_t *addr = ruli_res_get_curr_serv_addr(qry);
    
    switch (addr->addr_family) {
    case PF_INET:
      assert(udp_sd == qry->resolver->udp_sd);
      break;
    case PF_INET6:
      assert(udp_sd == qry->resolver->udp6_sd);
      break;
    default:
      assert(0);
    }
  }
#endif /* assert() NDEBUG */

  /*
   * Stop monitoring for reading and disable timeout
   */
  query_done_read_udp(qry);

  /*
   * Check answer flags
   */
  {
    int code = check_answer_flags(msg_hdr.flags, qry->q_options);

    if (code) {
      qry->answer_code = code;
      return query_done(qry);
    }
  }

  /* 
   * In case of truncated UDP response, switch to TCP query 
   */
  if (msg_hdr.flags & RULI_MSG_MASK_TC)
    return start_tcp_query(qry);
  
  /*
   * Save the answer for user
   */

  assert(!qry->answer_buf);
  assert(qry->answer_code == RULI_CODE_VOID);

  qry->answer_buf = (char *) ruli_malloc(rd);
  if (!qry->answer_buf) {
    qry->answer_code = RULI_CODE_MALLOC;
    return query_done(qry);
  }

  qry->answer_buf_size = rd;
  qry->answer_msg_len  = rd;
  qry->answer_header   = msg_hdr;
  qry->answer_code     = RULI_CODE_OK;

  memcpy(qry->answer_buf, buf, rd);

  return query_done(qry);
}

/*
  If we don't want to use the failed_tcp_sd
  anymore, but still have work to do in TCP
  mode, we call this function to close the
  socket and switch to next server.

  Behavior:

  The failed tcp socket is closed here.
  Try to switch TCP queries to next server.
  Finish query if the server switch fails.

  For every query from query_list:
  If query is monitoring the failed_tcp_sd,
  Try to switch such query to the next server.
 */
static void queries_connect_next(oop_source *oop_src, 
				 ruli_list_t *query_list, 
				 int failed_tcp_sd)
{
  ruli_server_t *failed_server = 0;
  int           list_size;
  int           i;
  ruli_list_t   affected_list; /* list of ruli_res_query_t* */

  /* There must exist at least one query */
  list_size = ruli_list_size(query_list);
  assert(list_size > 0);

  /* 
   * Before closing socket, we're going to unregister wanna-connect event
   */
  {
    int result = ruli_list_new_cap(&affected_list, list_size);
    assert(!result);
  }

  /*
   * Scan queries, saving the TCP ones in affected_list
   */
  for (i = 0; i < list_size; ++i) {
    ruli_res_query_t *qry = (ruli_res_query_t *) ruli_list_get(query_list, i);

    /*
     * Analyze only queries in TCP mode
     */
    if (tcp_status(qry->status)) {
        ruli_server_t *server = ruli_res_get_curr_server(qry);

	assert(server);

	if (server->tcp_sd == failed_tcp_sd) {
	  /*
	   * Save the affected queries for processing below
	   */
	  int result = ruli_list_push(&affected_list, qry);
	  assert(!result);
	}
    }
  }

  /* 
   * There must exist at least one affected query 
   *
   * Otherwise, which query was using the failed socket?
   */
  list_size = ruli_list_size(&affected_list);
  assert(list_size > 0);

  /* 
   * Close the socket 
   */
  for (i = 0; i < list_size; ++i) {
    ruli_res_query_t *qry    = (ruli_res_query_t *) ruli_list_get(&affected_list, i);
    ruli_server_t    *server = ruli_res_get_curr_server(qry);
    
    if (server->tcp_sd != -1) {
      /* Close socket */
      int result = close(server->tcp_sd);
      assert(!result);
      server->tcp_sd = -1;
      
      /* Remember server of failed connection */
      assert(!failed_server);
      failed_server = server;
      
      break;
    }
  }
  
  /*
   * Try to switch affected queries to next server
   */

  for (i = 0; i < list_size; ++i) {
    ruli_res_query_t *qry    = (ruli_res_query_t *) ruli_list_get(&affected_list, i);
    ruli_server_t    *server = ruli_res_get_curr_server(qry);

    /*
     * Switch off TCP mode
     */

    assert(tcp_status(qry->status));

    switch(qry->status) {
    case RULI_QRY_STAT_TCP_WANT_CONNECT:
      query_done_connect_tcp(qry);
      break;
    case RULI_QRY_STAT_TCP_WANT_SEND:
      query_done_write_tcp(qry);
      break;
    case RULI_QRY_STAT_TCP_WANT_RECV_HEAD:
      query_done_read_tcp_head(qry);
      break;
    case RULI_QRY_STAT_TCP_WANT_RECV_BODY:
      query_done_read_tcp_body(qry);
      break;
    default:
      assert(0);
    }

    assert(qry->status == RULI_QRY_STAT_VOID);
    assert(server);
    assert(failed_server);
    assert(server == failed_server);
   
    if (switch_else_schedule_finish(qry, RULI_CODE_CONNECT)) {
      /*
       * Switch failed, finish scheduled
       */
      continue;
    }

    /*
     * Switched successfully, wait for TCP connection
     */
    query_want_connect_tcp(qry);
  }

  ruli_list_delete(&affected_list);
}

static void queries_tcp_connect2write(ruli_list_t *query_list, int tcp_sd)
{
  int list_size = ruli_list_size(query_list);
  int i;

  assert(tcp_sd != -1);

  /*
   * Scan the given query list
   * Wanna-connect queries monitoring tcp_sd are switched to wanna-write
   */

  for (i = 0; i < list_size; ++i) {
    ruli_res_query_t *qry = (ruli_res_query_t *) ruli_list_get(query_list, i);
      
    if (qry->status == RULI_QRY_STAT_TCP_WANT_CONNECT) {
        ruli_server_t *server = ruli_res_get_curr_server(qry);

	assert(server);
	assert(server->tcp_sd != -1);

	if (server->tcp_sd == tcp_sd) {
	  query_done_connect_tcp(qry);
	  query_want_write_tcp(qry);
	}
    }
  }

}

static void *on_tcp_connect(oop_source *oop_src, int tcp_sd, 
			    oop_event event, void *ctx)
{
  ruli_res_t *res_ctx = (ruli_res_t *) ctx;

  assert(event == OOP_WRITE);

  /*
   * If connection succeded, switch all wanna-connect queries 
   * monitoring this tcp_sd socket to wanna-write mode.
   */
  if (ruli_sock_has_connected(tcp_sd)) {

    /* There must exist at least one query */
    assert(ruli_list_size(&res_ctx->query_list) > 0);

    queries_tcp_connect2write(&res_ctx->query_list, tcp_sd);
    
    return OOP_CONTINUE;
  }
  
  /*
   * Connection failed: switch queries on tcp_sd to next server.
   */
  queries_connect_next(oop_src, &res_ctx->query_list, tcp_sd);

  return OOP_CONTINUE;
}

/*
 * Note: As there is no timeout for the wanna-write
 *       condition, we don't need to disable the
 *       timeout here.
 */
static void *on_tcp_write(oop_source *oop_src, int tcp_sd, 
			  oop_event event, void *ctx)
{
  ruli_res_t       *res_ctx = (ruli_res_t *) ctx;
  int              result;
  ruli_res_query_t *qry = 0; /* picky compilers */
  int              i;
  int              queries;

  assert(event == OOP_WRITE);

  /*
   * Now we can write to the socket.
   *
   * As many queries may be waiting to write,
   * we need to find one of them.
   */

  queries = ruli_list_size(&res_ctx->query_list);

  /* There must exist at least one query */
  assert(queries > 0);

  for (i = 0; i < queries; ++i) {
    qry = (ruli_res_query_t *) ruli_list_get(&res_ctx->query_list, i);
    if (qry->status == RULI_QRY_STAT_TCP_WANT_SEND)
      break;
  }

  /* At least one TCP_WANT_SEND query MUST have been found */
  assert(i < queries); 

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: on_tcp_write(): id=%d index=%d\n", 
	  qry->query_id, qry->resolver_index);
#endif

  /*
   * Proceed to write to socket descriptor
   */

  result = ruli_sock_send(tcp_sd,
			  ruli_qry_tcp_buf(qry), 
			  ruli_qry_tcp_msg_len(qry));
      
  if (result == RULI_SOCK_WOULD_BLOCK)
    return OOP_CONTINUE;

  if (result == RULI_SOCK_SEND_FAIL) {
    /*
     * Connection failed: switch queries on tcp_sd to next server
     *
     * FIXME:
     *
     * I believe it's not needed to switch other
     * wanna-send queries. Switching the current one 
     * should be sufficient. Other queries would detect
     * the problem in their send event. But the
     * server-switching code is handy, so we use it.
     *
     */
    queries_connect_next(oop_src, &res_ctx->query_list, tcp_sd);
    
    return OOP_CONTINUE;
  }
    
  /* We don't expect other failures */
  assert(!result);

  /*
   * Change this query mode to read
   */
  query_done_write_tcp(qry); 
  query_want_read_tcp_head(qry);

  return OOP_CONTINUE;
}

static void *recv_tcp_head(ruli_res_t *ctx, int tcp_sd)
{
  ruli_server_t     *server;
  int               rd;
  int               result;
  int               head_len;
  ruli_uint16_t     msg_len;
  ruli_msg_header_t msg_hdr;
  ruli_res_query_t  *qry;

  /*
   * Find the server
   */
  server = ruli_res_find_server_by_sd(&ctx->server_list, tcp_sd);

  /* The server must have been found */
  assert(server);

  head_len = server->tcp_head_len;

  /* Sanity of received part of header */
  assert(head_len >= 0);

  /* We must be waiting some part of the header */
  assert(head_len < RULI_LIMIT_TCP_HEADER);

  /*
   * Receive message
   */
  result = ruli_sock_recv(tcp_sd, 
			  server->tcp_head_buf + head_len, 
			  RULI_LIMIT_TCP_HEADER - head_len, &rd);

  /* We don't block, so keep waiting */
  if (result == RULI_SOCK_WOULD_BLOCK)
    return OOP_CONTINUE;

  /* Server closed connection */
  if (result == RULI_SOCK_CLOSED) {
    queries_connect_next(ctx->res_source, &ctx->query_list, tcp_sd);
    return OOP_CONTINUE;
  }

  /* If we can't read the TCP socket, assume it's broken, and
     switch to next server */
  if (result == RULI_SOCK_RECV_FAIL) {
    queries_connect_next(ctx->res_source, &ctx->query_list, tcp_sd);
    return OOP_CONTINUE;
  }

  /* We don't expect other failures */
  assert(!result);
  
  /*
   * If we haven't filled a full header yet, just keep waiting.
   */
  server->tcp_head_len += rd;
  if (server->tcp_head_len < RULI_LIMIT_TCP_HEADER)
    return OOP_CONTINUE;

  /* Get encoded message size */
  msg_len = ruli_pack2(server->tcp_head_buf);

#ifdef RULI_RES_DEBUG
  fprintf(stderr, "DEBUG: recv_tcp_head(): encoded_msg_len=%d\n", msg_len);
#endif

  /*
   * If the encoded message size is too short,
   * surely there is some error.
   *
   * As we won't be able to delimit messages anymore, 
   * the TCP socket is unusable. Give up on that socket.
   */
  if (msg_len < RULI_LIMIT_MSG_LOW) {
    queries_connect_next(ctx->res_source, &ctx->query_list, tcp_sd);

    return OOP_CONTINUE;
  }

  /* Parse the message header */
  result = ruli_msg_parse_header(&msg_hdr, 
				 server->tcp_head_buf + 2, 
				 server->tcp_head_len - 2);
  /* In this case, the header parsing can't fail */
  assert(!result);

  /* 
   * If we can't find the query, there must be some error.
   * Then TCP socket is unusable, drop it.
   */
  qry = ruli_res_find_query_by_id(&ctx->query_list, msg_hdr.id);
  if (!qry) {
    queries_connect_next(ctx->res_source, &ctx->query_list, tcp_sd);

    return OOP_CONTINUE;
  }

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: recv_tcp_head(): id=%d index=%d\n", 
	  qry->query_id, qry->resolver_index);
#endif

  /* Query must be attached to a resolver */
  assert(qry->resolver);

  /* Query must be attached to right resolver */
  assert(qry->resolver == ctx);

  /*
   * Stop monitoring for reading and disable timeout
   */
  query_done_read_tcp_head(qry);

  /*
   * Check answer flags
   */
  {
    int code = check_answer_flags(msg_hdr.flags, qry->q_options);

    if (code) {
      qry->answer_code = code;
      return query_done(qry);
    }
  }

  /*
   * Allocate buffer for whole answer
   */
  assert(!qry->answer_buf);
  assert(qry->answer_code == RULI_CODE_VOID);

  qry->answer_buf = (char *) ruli_malloc(msg_len);
  if (!qry->answer_buf) {
    qry->answer_code = RULI_CODE_MALLOC;

    return query_done(qry);
  }

  /*
   * Save already-received header
   */
  {
    int len = server->tcp_head_len - 2;

    assert(len == RULI_LIMIT_MSG_HEADER);

    qry->answer_buf_size = msg_len;
    qry->answer_msg_len  = len;
    qry->answer_header   = msg_hdr;
    
    memcpy(qry->answer_buf, server->tcp_head_buf + 2, len);
  }

  /*
   * Wait answer body
   */

  query_want_read_tcp_body(qry);

  return OOP_CONTINUE;
}

static void *recv_tcp_body(ruli_res_query_t *qry, int tcp_sd)
{
  int result;
  int rd;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: recv_tcp_body(): id=%d index=%d\n", 
	  qry->query_id, qry->resolver_index);
#endif

  assert(_ruli_get_curr_tcp_socket(qry) == tcp_sd);
  assert(qry->answer_buf);
  assert(qry->answer_buf_size >= RULI_LIMIT_MSG_LOW);
  assert(qry->answer_msg_len >= RULI_LIMIT_MSG_HEADER);
  
  /* We are here because the body has not been finished */
  assert(qry->answer_msg_len < qry->answer_buf_size);

  /*
   * Receive message
   */

  assert(sizeof(ruli_uint8_t) == sizeof(char));

  result = ruli_sock_recv(tcp_sd, 
			  (ruli_uint8_t *) (qry->answer_buf + qry->answer_msg_len), 
			  qry->answer_buf_size - qry->answer_msg_len,
			  &rd);

  /* We never block, so keep waiting */
  if (result == RULI_SOCK_WOULD_BLOCK)
    return OOP_CONTINUE;

  /* If we can't read the TCP socket, assume it's broken, and
     switch to next server */
  if (result == RULI_SOCK_RECV_FAIL) {
    queries_connect_next(qry->resolver->res_source, 
			 &qry->resolver->query_list, tcp_sd);
    
    return OOP_CONTINUE;
  }

  /* We don't expect other failures */
  assert(!result);

  assert(rd >= 0);
  assert(rd <= (qry->answer_buf_size - qry->answer_msg_len));

  /* Update message received length */
  qry->answer_msg_len += rd;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: recv_tcp_body(): id=%d index=%d answer_msg_len=%d\n", 
	  qry->query_id, qry->resolver_index, qry->answer_msg_len);
#endif

  /*
   * Received whole TCP message?
   */

  if (qry->answer_msg_len == qry->answer_buf_size) {
    /*
     * Yes, we received the whole TCP message
     */

    query_done_read_tcp_body(qry);

    assert(qry->answer_buf_size == qry->answer_msg_len);
    assert(qry->answer_code == RULI_CODE_VOID);

    qry->answer_code = RULI_CODE_OK;

    return query_done(qry);
  }

  /*
   * No, keep waiting for more
   */

  return OOP_CONTINUE;
}

static void *on_tcp_read(oop_source *oop_src, int tcp_sd, 
			 oop_event event, void *ctx)
{
  ruli_res_t       *res_ctx    = (ruli_res_t *) ctx;
  ruli_list_t      *query_list = &res_ctx->query_list;
  int              list_size   = ruli_list_size(query_list);
  ruli_res_query_t *qry = NULL;
  int              i;

  assert(event == OOP_READ);

  /* At least one query must exist */
  assert(list_size > 0);
  
  /*
   * Is there a query waiting the body of a
   * TCP answer on tcp_sd socket?
   *
   * Only one TCP query can exist in the
   * READ_BODY state for a given socket.
   * Other TCP queries on the socket remain
   * in READ_HEAD until the first is complete.
   */

  for (i = 0; i < list_size; ++i) {
    qry = (ruli_res_query_t *) ruli_list_get(query_list, i);

    if (_ruli_get_curr_tcp_socket(qry) == tcp_sd)
      if (qry->status == RULI_QRY_STAT_TCP_WANT_RECV_BODY)
	break;
  }

  /*
   * Yes: Let's receive the remaining of the answer body.
   */
  if (i < list_size)
    return recv_tcp_body(qry, tcp_sd);

  /*
   * No: Receive a new TCP answer.
   */
  return recv_tcp_head(res_ctx, tcp_sd);
}

void _ruli_query_status_done(ruli_res_query_t *qry)
{
  switch (qry->status) {
  case RULI_QRY_STAT_VOID:
    break;
  case RULI_QRY_STAT_UDP_WANT_SEND:
    query_done_write_udp(qry);
    break;
  case RULI_QRY_STAT_UDP_WANT_RECV:
    query_done_read_udp(qry);
    break;
  case RULI_QRY_STAT_TCP_WANT_CONNECT:
    query_done_connect_tcp(qry);
    break;
  case RULI_QRY_STAT_TCP_WANT_SEND:
    query_done_read_udp(qry);
    break;
  case RULI_QRY_STAT_TCP_WANT_RECV_HEAD:
    query_done_read_tcp_head(qry);
    break;
  case RULI_QRY_STAT_TCP_WANT_RECV_BODY:
    query_done_read_tcp_body(qry);
    break;
  default:
    assert(0);
  }

  assert(qry->status == RULI_QRY_STAT_VOID);
}

