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
  $Id: ruli_res.c,v 1.36 2004/10/06 04:25:59 evertonm Exp $
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


#include <stdio.h>    /* FIXME: remove me [used for fprintf() debug] */

#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <ruli_res.h>
#include <ruli_util.h>
#include <ruli_msg.h>
#include <ruli_sock.h>
#include <ruli_fsm.h>
#include <ruli_rand.h>
#include <ruli_mem.h>
#include <ruli_txt.h>
#include <ruli_conf.h>


/*
 * From RFC 1035
 */
static const ruli_qtype_pair_t ruli_qtype_map[] =
{
  { "a",     RULI_RR_TYPE_A },
  { "ns",    RULI_RR_TYPE_NS },
  { "md",    RULI_RR_TYPE_MD },
  { "mf",    RULI_RR_TYPE_MF },
  { "cname", RULI_RR_TYPE_CNAME },
  { "soa",   RULI_RR_TYPE_SOA },
  { "mb",    RULI_RR_TYPE_MB },
  { "mg",    RULI_RR_TYPE_MG },
  { "mr",    RULI_RR_TYPE_MR },
  { "null",  RULI_RR_TYPE_NULL },
  { "wks",   RULI_RR_TYPE_WKS },
  { "ptr",   RULI_RR_TYPE_PTR },
  { "hinfo", RULI_RR_TYPE_HINFO },
  { "minfo", RULI_RR_TYPE_MINFO },
  { "mx",    RULI_RR_TYPE_MX },
  { "txt",   RULI_RR_TYPE_TXT },
  { "aaaa",  RULI_RR_TYPE_AAAA },
  { "srv",   RULI_RR_TYPE_SRV },
  { "axfr",  RULI_RR_TYPE_AXFR },
  { "mailb", RULI_RR_TYPE_MAILB },
  { "maila", RULI_RR_TYPE_MAILA },
  { "*",     RULI_RR_TYPE_ANY },
  { 0,       0 }
};

/*
 * From RFC 1035
 */
static const ruli_qclass_pair_t ruli_qclass_map[] =
{
  { "in", RULI_RR_CLASS_IN },
  { "cs", RULI_RR_CLASS_CS },
  { "ch", RULI_RR_CLASS_CH },
  { "hs", RULI_RR_CLASS_HS },
  { "*",  RULI_RR_CLASS_ANY },
  { 0,    0 }
};

int ruli_get_qtype_code(const char *qt_name) 
{
  int i;
  for (i = 0; ruli_qtype_map[i].qt_name; ++i)
    if (!strcasecmp(qt_name, ruli_qtype_map[i].qt_name))
      return ruli_qtype_map[i].qt_code;
  return 0;
}

int ruli_get_qclass_code(const char *qc_name) 
{
  int i;
  for (i = 0; ruli_qclass_map[i].qc_name; ++i)
    if (!strcasecmp(qc_name, ruli_qclass_map[i].qc_name))
      return ruli_qclass_map[i].qc_code;
  return 0;
}

const char *ruli_get_qtype_name(int qt_code) 
{
  int i;
  for (i = 0; ruli_qtype_map[i].qt_name; ++i)
    if (qt_code == ruli_qtype_map[i].qt_code)
      return ruli_qtype_map[i].qt_name;
  return 0;
}

const char *ruli_get_qclass_name(int qc_code) 
{
  int i;
  for (i = 0; ruli_qclass_map[i].qc_name; ++i)
    if (qc_code == ruli_qclass_map[i].qc_code)
      return ruli_qclass_map[i].qc_name;
  return 0;
}

int ruli_rr_type_is_address(long options, int rr_type)
{
  switch (rr_type) {
  case RULI_RR_TYPE_A:
    return !(options & RULI_RES_OPT_SRV_NOINET);
  case RULI_RR_TYPE_AAAA:
    return !(options & RULI_RES_OPT_SRV_NOINET6);
  }

  return 0;
}

const char *ruli_res_errstr(int result)
{
  return "FIXME: ruli_res_errstr()";
}

static void ruli_res_dispose_servers(ruli_res_t *res_ctx)
{
  int           err     = errno;
  int           size    = ruli_list_size(&res_ctx->server_list);
  int           i;
  ruli_server_t *server;

  for (i = 0; i < size; ++i) {
    server = (ruli_server_t *) ruli_list_get(&res_ctx->server_list, i);
    if (server->tcp_sd != -1) {
      int result = close(server->tcp_sd);
      assert(!result);
    }
    ruli_free(server);
  }

  ruli_list_delete(&res_ctx->server_list);

  errno = err;
}

static void conf_load(ruli_res_t *res_ctx)
{
  assert(res_ctx);

  /*
   * Use custom conf handler?
   */
  if (res_ctx->res_conf_handler) {
    res_ctx->search_list = res_ctx->res_conf_handler->search_loader(res_ctx->res_conf_handler);
    res_ctx->ns_list     = res_ctx->res_conf_handler->ns_loader(res_ctx->res_conf_handler);

    return;
  }

  /*
   * Use default conf handler
   */
  res_ctx->search_list = ruli_conf_load_search_list(0);
  res_ctx->ns_list     = ruli_conf_load_ns_list(0);
}

static void conf_unload(ruli_res_t *res_ctx)
{
  assert(res_ctx);

  /*
   * Use custom conf handler?
   */
  if (res_ctx->res_conf_handler) {
    if (res_ctx->search_list)
      res_ctx->res_conf_handler->search_unloader(res_ctx->res_conf_handler,
                                                 res_ctx->search_list);
    if (res_ctx->ns_list)
      res_ctx->res_conf_handler->ns_unloader(res_ctx->res_conf_handler,
                                             res_ctx->ns_list);

    return;
  }

  /*
   * Use default conf handler
   */
  if (res_ctx->search_list)
    ruli_conf_unload_search_list(0, res_ctx->search_list);

  if  (res_ctx->ns_list)
    ruli_conf_unload_ns_list(0, res_ctx->ns_list);
}

int ruli_res_new(ruli_res_t *res_ctx)
{
  int           i;
  int           size;
  ruli_server_t *server;

  /*
    FIXME: We need a better method to check this
   */
  assert(sizeof(ruli_uint8_t) == 1);
  assert(sizeof(ruli_uint16_t) == 2);
  assert(sizeof(ruli_uint32_t) == 4);

  /*
   * Load search and ns list
   */
  res_ctx->search_list = 0;
  res_ctx->ns_list     = 0;

  conf_load(res_ctx);

  if (!res_ctx->ns_list) 
    return RULI_RES_NS_LIST;

  assert(res_ctx->ns_list);
  assert(ruli_list_size(res_ctx->ns_list));

  /*
   * Allocate query list
   */
  if (ruli_list_new_cap(&res_ctx->query_list, 2)) {
    conf_unload(res_ctx);
    return RULI_RES_LIST;
  }

  res_ctx->udp_sd        = -1;
  res_ctx->udp_writers   = 0;
  res_ctx->udp_readers   = 0;
  res_ctx->udp6_sd       = -1;
  res_ctx->udp6_writers  = 0;
  res_ctx->udp6_readers  = 0;
  res_ctx->next_server   = 0;
  res_ctx->next_query_id = 0;

  /*
   * Allocate space for server information
   * Includes TCP socket descriptors
   */
  size = ruli_list_size(res_ctx->ns_list);

  assert(size > 0);

  if (ruli_list_new_cap(&res_ctx->server_list, size)) {
    conf_unload(res_ctx);
    ruli_list_delete(&res_ctx->query_list);
    return RULI_RES_LIST;
  }

  for (i = 0; i < size; ++i) {
    server = (ruli_server_t *) ruli_malloc(sizeof(ruli_server_t));
    if (!server) {
      conf_unload(res_ctx);
      ruli_list_delete(&res_ctx->query_list);
      ruli_res_dispose_servers(res_ctx);
      return RULI_RES_MALLOC;
    }
    if (ruli_list_push(&res_ctx->server_list, server)) {
      conf_unload(res_ctx);
      ruli_list_delete(&res_ctx->query_list);
      ruli_res_dispose_servers(res_ctx);
      return RULI_RES_MALLOC;
    }

    server->tcp_sd         = -1;
    server->tcp_connecters = 0;
    server->tcp_writers    = 0;
    server->tcp_readers    = 0;
    server->port           = 53; /* default DNS server port in host-order */
    server->tcp_head_len   = -1;
  }

  /* Be sure we considered all servers */
  assert(ruli_list_size(&res_ctx->server_list) == size);

  ruli_rand_init(&res_ctx->rand_ctx);

  return RULI_RES_OK;
}

void ruli_res_delete(ruli_res_t *res_ctx)
{
#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s: %s(): %d: "
	  "query_list_size=%d\n", 
	  __FILE__, __PRETTY_FUNCTION__, __LINE__,
	  ruli_list_size(&res_ctx->query_list));
#endif /* RULI_RES_DEBUG */

  /*
   * Clean UDP socket
   */
  if (res_ctx->udp_sd != -1) {
    int result = close(res_ctx->udp_sd);
    assert(!result);
    res_ctx->udp_sd = -1;
  }

  /*
   * Clean UDP6 socket
   */
  if (res_ctx->udp6_sd != -1) {
    int result = close(res_ctx->udp6_sd);
    assert(!result);
    res_ctx->udp6_sd = -1;
  }

  /*
   * Dispose servers information
   * Including TCP sockets
   */
  ruli_res_dispose_servers(res_ctx);

  /* Please don't finish without releasing queries */
  assert(!ruli_list_size(&res_ctx->query_list));

  /* Release query list */
  ruli_list_delete(&res_ctx->query_list);

  /* Release dynamic config */
  conf_unload(res_ctx);

  /* Free res_interface string */
  if(res_ctx->res_interface) {
    ruli_free(res_ctx->res_interface);
    res_ctx->res_interface = NULL;
  }
}

/*
  Many queries can use the same UDP socket,
  for a given address family.
  This function encapsulates this.
 */
static int get_udp_socket(ruli_res_query_t *res_qry)
{
  ruli_addr_t *addr = ruli_res_get_curr_serv_addr(res_qry);
  ruli_res_t *res_ctx = res_qry->resolver;
  int sd = -1; /* picky compilers */

  switch (addr->addr_family) {
  case PF_INET:
    sd = res_ctx->udp_sd;
    if (sd != -1)
      return sd;
    sd = ruli_sock_create_udp(PF_INET, res_ctx->res_interface);
    if (sd == -1)
      return sd;
    res_ctx->udp_sd = sd;
    break;

  case PF_INET6:
    sd = res_ctx->udp6_sd;
    if (sd != -1)
      return sd;
    sd = ruli_sock_create_udp(PF_INET6, res_ctx->res_interface);
    if (sd == -1)
      return sd;
    res_ctx->udp6_sd = sd;
    break;

  default:
    assert(0);
  }

  return sd;
}

static int start_query(ruli_res_query_t *res_qry)
{
  int result;

  /* 
   * Get UDP socket
   */

  int udp_sd = get_udp_socket(res_qry);
  if (udp_sd == -1)
    return RULI_RES_SOCKET;

  /* 
   * Build query
   */

  result = ruli_msg_build(ruli_qry_udp_buf(res_qry), res_qry->query_buf_size,
			  &res_qry->query_msg_len, res_qry->query_id, 
			  res_qry->full_dname, res_qry->full_dname_len,
			  res_qry->q_class, res_qry->q_type);
#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: start_query(): query_id=%d result=%d query_msg_len=%d\n", 
	  res_qry->query_id, result, res_qry->query_msg_len);
#endif
  if (result)
    return RULI_RES_MSG_BUILD;
  
  assert(res_qry->query_msg_len <= res_qry->query_buf_size);

  /*
   * Encode TCP message length
   */
  {
    ruli_uint8_t *i = ruli_unpack2(ruli_qry_tcp_buf(res_qry), 
			           res_qry->query_msg_len);
    assert(i == ruli_qry_tcp_buf(res_qry) + 2);
  }
  
  /*
   * Set handler for events on socket
   *
   * fsm will return in _ruli_fsm_query_done()
   */

  _ruli_query_want_write_udp(res_qry);

  return RULI_RES_OK;
}

static int get_next_server(ruli_res_t *res_ctx)
{
  int next;
  int servers;

  assert(res_ctx);
  assert(res_ctx->ns_list);

  servers = ruli_list_size(res_ctx->ns_list);

  assert(servers > 0);

  next = res_ctx->next_server++;

  res_ctx->next_server %= servers;

  assert(res_ctx->next_server >= 0);
  assert(res_ctx->next_server < servers);

  return next;
}

ruli_uint8_t *ruli_qry_tcp_buf(ruli_res_query_t *qry)
{
  return qry->query_buf;
}

int ruli_qry_tcp_msg_len(ruli_res_query_t *qry)
{
  return qry->query_msg_len + 2;
}

ruli_uint8_t *ruli_qry_udp_buf(ruli_res_query_t *qry)
{
  return qry->query_buf + 2;
}

int ruli_qry_udp_msg_len(ruli_res_query_t *qry)
{
  return qry->query_msg_len;
}

int ruli_res_query_submit(ruli_res_t *res_ctx, ruli_res_query_t *res_qry)
{
  int result;

  /* Initially use the domain name without search list */
  assert(res_qry->q_domain_len <= RULI_LIMIT_DNAME_ENCODED);
  memcpy(res_qry->full_dname, res_qry->q_domain, res_qry->q_domain_len);
  res_qry->full_dname_len = res_qry->q_domain_len;

  /*
   * Append query into resolver context
   */
  res_qry->resolver_index = ruli_list_size(&res_ctx->query_list);
  result = ruli_list_push(&res_ctx->query_list, res_qry);
  assert(!result);

  assert(RULI_RES_QBUF_SIZE == RULI_LIMIT_MSG_HIGH + 2);

  /*
   * Attach to resolver
   */
  res_qry->resolver          = res_ctx;

  res_qry->query_id          = res_ctx->next_query_id++;
  res_qry->status            = RULI_QRY_STAT_VOID;
  res_qry->first_server      = get_next_server(res_ctx);
  res_qry->curr_server       = res_qry->first_server;
  res_qry->query_buf_size    = RULI_LIMIT_MSG_HIGH;
  res_qry->query_msg_len     = -1;
  res_qry->answer_buf        = 0;
  res_qry->answer_buf_size   = -1;
  res_qry->answer_msg_len    = -1;
  res_qry->answer_code       = RULI_CODE_VOID;
  res_qry->remaining_retries = res_ctx->res_retry;
  res_qry->search_index      = -1;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s: %s(): %d: query_id=%d\n", 
	  __FILE__, __PRETTY_FUNCTION__, __LINE__,
	  res_qry->query_id);
#endif /* RULI_RES_DEBUG */

  return start_query(res_qry);
}

void ruli_res_query_delete(ruli_res_query_t *res_qry)
{
  ruli_res_t  *res     = res_qry->resolver; /* query resolver */
  ruli_list_t *q_list;                      /* resolver query list */
  int         q_ind;                        /* query index in resolver */

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s: %s(): %d: BEFORE delete: query_id=%d attached_res=%d\n", 
	  __FILE__, __PRETTY_FUNCTION__, __LINE__,
	  res_qry->query_id, res != 0);
#endif /* RULI_RES_DEBUG */
  
  q_list = &res->query_list;
  q_ind  = res_qry->resolver_index;

  /*
   * Perform state-specific clean-up
   */
  _ruli_query_status_done(res_qry);
   
  /*
   * If we have an answer buffer allocated, release it.
   */
  if (res_qry->answer_buf) {
    ruli_free(res_qry->answer_buf);
    res_qry->answer_buf      = 0;
    res_qry->answer_buf_size = -1;
    res_qry->answer_msg_len  = -1;
    res_qry->answer_code     = RULI_CODE_VOID;
  }

  /*
   * If query is not attached to a resolver, it can't be deleted.
   */
  if (!res)
    return;

  /*
   * Erase query from the resolver list.
   */
  ruli_list_drop(q_list, q_ind);

  /*
   * Re-index the query which replaced this, if any.
   */
  if (q_ind != ruli_list_size(q_list)) {
    ruli_res_query_t *qry = (ruli_res_query_t *) ruli_list_get(q_list, q_ind);
    qry->resolver_index = q_ind;
  }

  /*
   * Mark as detached from resolver.
   */
  res_qry->resolver = 0;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s: %s(): %d: AFTER delete: query_list_size=%d\n", 
	  __FILE__, __PRETTY_FUNCTION__, __LINE__,
	  ruli_list_size(q_list));
#endif /* RULI_RES_DEBUG */
}

int ruli_res_get_curr_server_index(ruli_res_query_t *qry)
{
  assert(qry->curr_server >= 0);
  assert(qry->curr_server < ruli_list_size(&qry->resolver->server_list));

  return qry->curr_server;
}

ruli_server_t *ruli_res_get_curr_server(ruli_res_query_t *qry)
{
  return (ruli_server_t *) ruli_list_get(&qry->resolver->server_list, 
		                         ruli_res_get_curr_server_index(qry));
}

ruli_addr_t *ruli_res_get_curr_serv_addr(ruli_res_query_t *res_qry)
{
  ruli_res_t *res_ctx = res_qry->resolver;

  assert(res_ctx);
  assert(res_ctx->ns_list);
  assert(ruli_list_size(res_ctx->ns_list) > 0);

  assert(res_qry->curr_server >= 0);
  assert(res_qry->curr_server < ruli_list_size(res_ctx->ns_list));

  return (ruli_addr_t *) ruli_list_get(res_ctx->ns_list, res_qry->curr_server);
}

int ruli_res_get_curr_serv_port(ruli_res_query_t *res_qry)
{
  return ruli_res_get_curr_server(res_qry)->port;
}

int _ruli_get_curr_tcp_socket(ruli_res_query_t *qry) 
{
  ruli_res_t    *res_ctx = qry->resolver;
  int           i        = ruli_res_get_curr_server_index(qry);
  ruli_server_t *server  = (ruli_server_t *) ruli_list_get(&res_ctx->server_list, i);
  int           tcp_sd   = server->tcp_sd;

  return tcp_sd;
}

int ruli_res_switch_server(ruli_res_query_t *res_qry)
{
  int servers = ruli_list_size(res_qry->resolver->ns_list);

  assert(servers > 0);

  ++res_qry->curr_server;
  res_qry->curr_server %= servers;

  assert(res_qry->curr_server >= 0);
  assert(res_qry->curr_server < servers);

  if (res_qry->curr_server == res_qry->first_server) {

    if (res_qry->remaining_retries < 1)
      return -1;

    --res_qry->remaining_retries;
  }

  return 0;
}

ruli_res_query_t *ruli_res_find_query_by_id(ruli_list_t *query_list, 
					    ruli_uint16_t query_id)
{
  ruli_res_query_t *qry;
  int              query_list_size;
  int              i;

  query_list_size = ruli_list_size(query_list);

  for (i = 0; i < query_list_size; ++i) {
    qry = (ruli_res_query_t *) ruli_list_get(query_list, i);
    if (qry->query_id == query_id)
      return qry;
  }

  return 0;
}

ruli_server_t *ruli_res_find_server_by_sd(ruli_list_t *server_list, 
					  int sd)
{
  ruli_server_t *server;
  int           list_size;
  int           i;

  /* We should not look for an invalid fd */
  assert(sd >= 0);

  list_size = ruli_list_size(server_list);

  for (i = 0; i < list_size; ++i) {
    server = (ruli_server_t *) ruli_list_get(server_list, i);
    if (server->tcp_sd == sd)
      return server;
  }

  return 0;
}

static const ruli_domain_t *next_search_suffix(ruli_res_query_t *qry)
{
  ruli_res_t    *res_ctx         = qry->resolver;
  int           search_list_size;
  ruli_domain_t *suffix;

  if (!res_ctx->search_list)
    return 0;

  assert(res_ctx->search_list);

  search_list_size = ruli_list_size(res_ctx->search_list);

  assert(qry->search_index >= -1);
  assert(qry->search_index < search_list_size);

  if (qry->search_index == (search_list_size - 1))
    return 0;

  assert(qry->search_index < (search_list_size - 1));

  ++qry->search_index; 

  assert(qry->search_index >= 0);
  assert(qry->search_index < search_list_size);

  suffix = (ruli_domain_t *) ruli_list_get(res_ctx->search_list, 
					   qry->search_index);

  return suffix;
}

static void *do_user_callback(ruli_res_query_t *qry)
{
  return qry->q_on_answer(qry, qry->q_on_answer_arg);
}

/*
  start_query() has passed control to fsm through query_want_write_udp()

  fsm returns here
 */
void *_ruli_fsm_query_done(ruli_res_query_t *qry)
{
#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s: %s()\n",
	  __FILE__, __PRETTY_FUNCTION__);
#endif

  /*
   * Use the search list?
   */
  if (qry->q_options & RULI_RES_OPT_SEARCH)

    /*
     * If we received a server answer...
     */
    if (!qry->answer_code) {
      ruli_uint8_t rcode = qry->answer_header.rcode;
    
      /*
       * And that answer misses the query name...
       */
      if ((rcode == RULI_RCODE_SERVERFAILURE) ||
	  (rcode == RULI_RCODE_NAMEERROR)) {

	/*
	 * ... Then yes, we try the search list
	 */
        const ruli_domain_t *suffix = next_search_suffix(qry);
        if (suffix) {

#ifdef RULI_RES_DEBUG
          {
            char txt[RULI_LIMIT_DNAME_TEXT_BUFSZ];
            int  txt_len;
            int  result;

            result = ruli_dname_decode(txt, RULI_LIMIT_DNAME_TEXT_BUFSZ,
                                       &txt_len,
                                       (const char *) suffix->domain_name,
                                       suffix->domain_len);
            assert(!result);

            fprintf(stderr,
                    "DEBUG: %s: %s(): search list suffix name=%s len=%d\n",
                    __FILE__, __PRETTY_FUNCTION__,
                    txt, txt_len);
          }
#endif /* RULI_RES_DEBUG */

	  /*
	   * Build new query name with current search list item
	   */

          if (ruli_dname_concat(qry->full_dname, RULI_LIMIT_DNAME_ENCODED,
                                &qry->full_dname_len,
                                (ruli_uint8_t *) qry->q_domain,
                                qry->q_domain_len,
                                suffix->domain_name, suffix->domain_len)) {
            qry->answer_code = RULI_CODE_CONCAT_SEARCH_LIST;
            return do_user_callback(qry);
          }

#ifdef RULI_RES_DEBUG
          {
            char txt[RULI_LIMIT_DNAME_TEXT_BUFSZ];
            int  txt_len;
            int  result;

            result = ruli_dname_decode(txt, RULI_LIMIT_DNAME_TEXT_BUFSZ,
                                       &txt_len,
                                       (char *) qry->full_dname,
                                       qry->full_dname_len);
            assert(!result);

            fprintf(stderr,
                    "DEBUG: %s: %s(): search list full domain name=%s len=%d\n",
                    __FILE__, __PRETTY_FUNCTION__,
                    txt, txt_len);
          }
#endif /* RULI_RES_DEBUG */

          /*
           * Tweak query state
           */

          assert(qry->answer_buf);
          assert(qry->answer_buf_size > 0);
          assert(qry->answer_buf_size <= 65535);
          assert(qry->answer_msg_len > 0);
          assert(qry->answer_msg_len <= 65535);
          assert(qry->answer_msg_len == qry->answer_buf_size);
          assert(!qry->answer_code);

          ruli_free(qry->answer_buf);
          qry->answer_buf        = 0;
          qry->answer_buf_size   = -1;
          qry->answer_msg_len    = -1;
          qry->answer_code       = RULI_CODE_VOID;
          qry->remaining_retries = qry->resolver->res_retry;
          qry->first_server      = qry->curr_server;

	  /*
	   * Re-start query
	   */

	  if (!start_query(qry))
	    return OOP_CONTINUE;

	  /* Oh well, tell the user we failed miserably */
	  qry->answer_code = RULI_CODE_LAUNCH_SEARCH_LIST;
        }
      }
    }

  /*
   * Finally invoke user callback
   */
  return do_user_callback(qry);
}

