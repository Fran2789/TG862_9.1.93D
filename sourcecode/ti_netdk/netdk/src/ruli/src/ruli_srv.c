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
  $Id: ruli_srv.c,v 1.86 2005/06/28 22:27:02 evertonm Exp $
  */


#include <sys/socket.h> /* FIXME: remove me [used for fprintf() debug] */
#include <netinet/in.h> /* FIXME: remove me [used for fprintf() debug] */
#include <arpa/inet.h>  /* FIXME: remove me [used for fprintf() debug] */
#include <stdio.h>      /* FIXME: remove me [used for fprintf() debug] */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <ruli_srv.h>
#include <ruli_parse.h>
#include <ruli_rand.h>
#include <ruli_txt.h>
#include <ruli_mem.h>
#include <ruli_host.h>


const char *ruli_srv_errstr(int result)
{
  return "FIXME: ruli_srv_errstr()";
}

static void addr_selection(ruli_list_t *srv_list, long options)
{
  int i;
  int srv_list_size = ruli_list_size(srv_list);

  /* scan srv entries */
  for (i = 0; i < srv_list_size; ++i) {
    ruli_srv_entry_t *entry = (ruli_srv_entry_t *) ruli_list_get(srv_list, i);

    ruli_addr_rfc3484_sort(&entry->addr_list, options);
  }
}

/*
  This is an indirection to the srv user callback
  */
void *_ruli_srv_query_done(ruli_srv_t *srv_qry, int srv_result_code)
{
  assert(srv_qry->answer_code == RULI_SRV_CODE_VOID);
  assert(srv_result_code != RULI_SRV_CODE_VOID);

  srv_qry->answer_code = srv_result_code;

  /* Sort addresses */
  if (srv_result_code == RULI_SRV_CODE_OK)
    addr_selection(&srv_qry->answer_srv_list, srv_qry->srv_options);

  /*
   * Invoke srv user callback
   */
  return srv_qry->srv_on_answer(srv_qry, srv_qry->srv_on_answer_arg);
}

static void *query_done(ruli_srv_t *srv_qry, int srv_result_code)
{
  return _ruli_srv_query_done(srv_qry, srv_result_code);
}

/*
  dispose fallback query
 */
static void f_query_done(ruli_host_t *fall_qry) {
  assert(fall_qry);
  ruli_host_query_delete(fall_qry);
  ruli_free(fall_qry);
}

/*
  finish fallback query
 */
static void *fall_query_done(ruli_srv_t *srv_qry, int srv_result_code, 
			     ruli_host_t *fall_qry)
{
#ifdef RULI_SRV_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s: %s(): %d: id=%d query_result=%s [%d]\n", 
	  __FILE__, __PRETTY_FUNCTION__, __LINE__,
	  fall_qry->host_query.query_id,
	  srv_result_code ? "FAIL" : "SUCCESS", 
	  srv_result_code);
#endif

  f_query_done(fall_qry);

  return query_done(srv_qry, srv_result_code);
}

/* walk query context */
typedef struct {
  ruli_host_t walk_query;  /* auxiliary query context for "walk" */
  ruli_srv_t  *srv_query;  /* original srv query */
} walk_t;

/*
  dispose walk query
 */
static void w_query_done(walk_t *walk)
{
  assert(walk);
  ruli_host_query_delete(&walk->walk_query);
  ruli_free(walk);
}

/*
  finish walk query
 */
static void *walk_query_done(walk_t *walk, int srv_result_code)
{
  ruli_srv_t *srv_qry = walk->srv_query;

#ifdef RULI_SRV_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s: %s(): %d: id=%d query_result=%s [%d]\n", 
	  __FILE__, __PRETTY_FUNCTION__, __LINE__,
	  walk->walk_query.host_query.query_id,
	  srv_result_code ? "FAIL" : "SUCCESS", 
	  srv_result_code);
#endif

  w_query_done(walk);

  return query_done(srv_qry, srv_result_code);
}

static void *on_walk_answer(ruli_host_t *host_qry, void *qry_arg)
{
  walk_t           *walk_qry = (walk_t *) qry_arg;
  ruli_srv_t       *srv_qry = walk_qry->srv_query;
  ruli_srv_entry_t *srv_entry;
  int              prev_addr_list_size;

  assert(ruli_host_answer_code(host_qry) != RULI_SRV_CODE_VOID);

  srv_entry = (ruli_srv_entry_t *) ruli_list_get(&srv_qry->answer_srv_list, 
                                                 srv_qry->under.walk_index);
  prev_addr_list_size = ruli_list_size(&srv_entry->addr_list);

#ifdef RULI_SRV_DEBUG
  {
    char target_txt[RULI_LIMIT_DNAME_TEXT_BUFSZ];
    int  target_txt_len;
    int  result;
    
    result = ruli_dname_decode(target_txt, RULI_LIMIT_DNAME_TEXT_BUFSZ, 
			       &target_txt_len, 
			       (const char *) walk_qry->walk_query.host_query.full_dname, 
			       walk_qry->walk_query.host_query.full_dname_len);
    assert(!result);
    
    fprintf(stderr, 
	    "DEBUG: on_walk_answer(): query target=%s target_len=%d\n", 
	    target_txt, target_txt_len);
  }
#endif

  /*
   * Query failed?
   */
  {
    int answer_code = ruli_host_answer_code(host_qry);
    if (answer_code) {
      srv_qry->last_rcode = RULI_RCODE_VOID;

      switch(answer_code) {
      case RULI_HOST_CODE_ALARM:
        return walk_query_done(walk_qry, RULI_SRV_CODE_WALK_ALARM);
      case RULI_HOST_CODE_EMPTY:
        return walk_query_done(walk_qry, RULI_SRV_CODE_WALK_EMPTY);
      case RULI_HOST_CODE_RCODE:
	assert(ruli_host_rcode(host_qry) != RULI_RCODE_VOID);
	assert(ruli_host_rcode(host_qry) != RULI_RCODE_NOERROR);
	srv_qry->last_rcode = ruli_host_rcode(host_qry);
	return walk_query_done(walk_qry, RULI_SRV_CODE_WALK_RCODE);
      default:
        return walk_query_done(walk_qry, RULI_SRV_CODE_WALK_QUERY);
      }

      assert(0);
    }
  }

  assert(ruli_host_rcode(host_qry) == RULI_RCODE_NOERROR);
  srv_qry->last_rcode = RULI_RCODE_NOERROR;

  /*
   * Move addresses into SRV record
   */
  {
    ruli_list_t *addr_list = &host_qry->answer_addr_list;
    int addr_list_size = ruli_list_size(addr_list);
    int i;

    /* Move addresses from addr query to SRV entry */
    for (i = 0; i < addr_list_size; ++i) {
      ruli_addr_t *addr = (ruli_addr_t *) ruli_list_get(addr_list, i);
      if (ruli_list_push(&srv_entry->addr_list, addr)) {
        ruli_list_prune(&srv_entry->addr_list, 0);
        return walk_query_done(walk_qry, RULI_SRV_CODE_FALL_OTHER);
      }
    }
  }

  /*
   * Detaches addresses from addr-query ruli_list_t container
   * So that next ruli_host_query_delete does not dispose them
   */
  ruli_host_query_detach(host_qry);

  /* Dispose auxiliary query as it will be re-created
     by next walk query */
  w_query_done(walk_qry);

  /* In future walk query, consider next answer target */
  ++srv_qry->under.walk_index;

  return _ruli_srv_answer_walk(srv_qry);
}

static int find_addr(const ruli_list_t *addr_list, long options)
{
  int list_size = ruli_list_size(addr_list);
  int i;

  for (i = 0; i < list_size; ++i) {
    ruli_addr_t *addr = (ruli_addr_t *) ruli_list_get(addr_list, i);
    switch (ruli_addr_family(addr)) {
      case PF_INET6:
        if (!(options & RULI_RES_OPT_SRV_NOINET6))
          return -1;
        break;
      case PF_INET:
        if (!(options & RULI_RES_OPT_SRV_NOINET))
          return -1;
        break;
    }
  }

  return 0;
}

/*
  Search missing addresses for targets.

  We make serialized queries so we don't need
  to keep more than one query reference (walk_query).
 */
void *_ruli_srv_answer_walk(ruli_srv_t *srv_qry)
{
  ruli_list_t *srv_list     = &srv_qry->answer_srv_list;
  int         srv_list_size = ruli_list_size(srv_list);

  /* Have the user disabled walk query? */
  if (srv_qry->srv_options & RULI_RES_OPT_SRV_NOWALK)
    return query_done(srv_qry, RULI_SRV_CODE_OK);

  /*
   * Scan SRV answer targets, considering address lists
   */
  for (; srv_qry->under.walk_index < srv_list_size; 
       ++srv_qry->under.walk_index) {
    ruli_srv_entry_t *entry = \
      (ruli_srv_entry_t *) ruli_list_get(srv_list, 
					 srv_qry->under.walk_index);
    ruli_list_t *addr_list = &entry->addr_list;
    walk_t *walk_qry;

    /* If this target already has address(es), skip it */
    if (find_addr(addr_list, srv_qry->srv_options))
      continue;

#ifdef RULI_SRV_DEBUG
    {
      char target_txt[RULI_LIMIT_DNAME_TEXT_BUFSZ];
      int  target_txt_len;
      int  result;
      
      result = ruli_dname_decode(target_txt, RULI_LIMIT_DNAME_TEXT_BUFSZ, 
				 &target_txt_len, entry->target, 
				 entry->target_len);
      assert(!result);
      
      fprintf(stderr, 
	      "DEBUG: _ruli_srv_answer_walk(): "
              "missing target=%s walk_index=%d\n", 
	      target_txt, srv_qry->under.walk_index);
    }
#endif
    
    /*
     * Allocate space for auxiliary walk query
     */
    walk_qry = \
      (walk_t *) ruli_malloc(sizeof(*walk_qry));
    if (!walk_qry)
      return query_done(srv_qry, RULI_SRV_CODE_WALK_OTHER);
    walk_qry->srv_query = srv_qry;

    /*
     * Initialize walk query arguments
     */
    walk_qry->walk_query.host_resolver        = srv_qry->srv_resolver;
    walk_qry->walk_query.host_on_answer       = on_walk_answer;
    walk_qry->walk_query.host_on_answer_arg   = walk_qry;
    walk_qry->walk_query.host_domain          = entry->target;
    walk_qry->walk_query.host_domain_len      = entry->target_len;
    walk_qry->walk_query.host_options         = srv_qry->srv_options;
    /* RFC 2782 states CNAME aren't valid SRV targets */
    walk_qry->walk_query.host_max_cname_depth =
	(srv_qry->srv_options & RULI_RES_OPT_SRV_CNAME) ?
	RULI_LIMIT_CNAME_DEPTH : 0;

    /*
     * Submit walk query
     */
    if (ruli_host_query_submit(&walk_qry->walk_query)) {
      ruli_free(walk_qry);
      return query_done(srv_qry, RULI_SRV_CODE_WALK_QUERY);
    }

    /* Wait answer */
    return OOP_CONTINUE;

  } /* for */

  /*
   * All targets scanned, we're done
   */

  return query_done(srv_qry, RULI_SRV_CODE_OK);
}

static void *on_fallback_answer(ruli_host_t *qry, void *qry_arg)
{
  ruli_srv_t *srv_qry           = (ruli_srv_t *) qry_arg;
  int        prev_srv_list_size = ruli_list_size(&srv_qry->answer_srv_list);
  int        answer_code;

  assert(prev_srv_list_size == 0);

  assert(qry->answer_code != RULI_SRV_CODE_VOID);

  /*
   * Query failed?
   */
  answer_code = ruli_host_answer_code(qry);
  if (answer_code) {
    srv_qry->last_rcode = RULI_RCODE_VOID;

    switch(answer_code) {
    case RULI_HOST_CODE_ALARM:
      return fall_query_done(srv_qry, RULI_SRV_CODE_FALL_ALARM, qry);
    case RULI_HOST_CODE_EMPTY:
      return fall_query_done(srv_qry, RULI_SRV_CODE_FALL_EMPTY, qry);
    case RULI_HOST_CODE_RCODE:
      assert(ruli_host_rcode(qry) != RULI_RCODE_VOID);
      assert(ruli_host_rcode(qry) != RULI_RCODE_NOERROR);
      srv_qry->last_rcode = ruli_host_rcode(qry);
      return fall_query_done(srv_qry, RULI_SRV_CODE_FALL_RCODE, qry);
    default: 
      return fall_query_done(srv_qry, RULI_SRV_CODE_FALL_QUERY, qry);
    }

    assert(0);
  }

  assert(ruli_host_rcode(qry) == RULI_RCODE_NOERROR);
  srv_qry->last_rcode = RULI_RCODE_NOERROR;

  /*
   * Move addresses into SRV record
   */
  {
    ruli_srv_entry_t *srv_entry;
    ruli_list_t *addr_list = &qry->answer_addr_list;
    int addr_list_size = ruli_list_size(addr_list);
    int i;

    /* Create SRV entry and append it to list */
    srv_entry =_ruli_srv_list_new_entry(&srv_qry->answer_srv_list,
                                        srv_qry->srv_domain,
                                        srv_qry->srv_domain_len,
                                        -1,
                                        -1,
                                        srv_qry->srv_fallback_port,
                                        -1,
                                        -1,
                                        -1);
    if (!srv_entry)
      return fall_query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER, qry);

    /* Move addresses from addr query to SRV entry */
    for (i = 0; i < addr_list_size; ++i) {
      ruli_addr_t *addr = (ruli_addr_t *) ruli_list_get(addr_list, i);
      if (ruli_list_push(&srv_entry->addr_list, addr)) {
        ruli_list_prune(&srv_entry->addr_list, 0);
        return fall_query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER, qry);
      }
    }
  }

  /* 
   * Detaches addresses from addr-query ruli_list_t container
   * So that next ruli_host_query_delete does not dispose them
   */
  ruli_host_query_detach(qry);

  return fall_query_done(srv_qry, RULI_SRV_CODE_OK, qry);
}

/*
  Fallback to 'addr'
 */
void *_ruli_srv_answer_fallback_addr(ruli_srv_t *srv_qry)
{
  ruli_host_t *fall_qry;

#ifdef RULI_SRV_DEBUG
  {
    char txt_dname_buf[RULI_LIMIT_DNAME_TEXT_BUFSZ];
    int  txt_dname_len;
    int  result;

    result = ruli_dname_decode(txt_dname_buf, RULI_LIMIT_DNAME_TEXT_BUFSZ,
			       &txt_dname_len, 
			       srv_qry->srv_domain, srv_qry->srv_domain_len);
    assert(!result);

    fprintf(stderr, 
	    "DEBUG: %s: %s(): %d: address query: "
	    "domain=%s domain_len=%d fallback=%d\n",
	    __FILE__, __PRETTY_FUNCTION__, __LINE__,
	    txt_dname_buf, txt_dname_len, 
            !!(srv_qry->srv_options & RULI_RES_OPT_SRV_NOFALL));
  }
#endif

  /* Has the user disabled fallback query? */
  if (srv_qry->srv_options & RULI_RES_OPT_SRV_NOFALL)
    return query_done(srv_qry, RULI_SRV_CODE_OK);

  /*
   * Allocate space for fallback query
   */
  fall_qry = (ruli_host_t *) \
    ruli_malloc(sizeof(*fall_qry));
  if (!fall_qry)
    return query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER);

  /*
   * Initialize fallback query arguments
   */
  fall_qry->host_resolver        = srv_qry->srv_resolver;
  fall_qry->host_on_answer       = on_fallback_answer;
  fall_qry->host_on_answer_arg   = srv_qry;
  fall_qry->host_domain          = srv_qry->srv_domain;
  fall_qry->host_domain_len      = srv_qry->srv_domain_len;
  fall_qry->host_options         = srv_qry->srv_options;
  fall_qry->host_max_cname_depth = RULI_LIMIT_CNAME_DEPTH;

  /*
   * Submit fallback query
   */
  if (ruli_host_query_submit(fall_qry)) {
    ruli_free(fall_qry);
    return query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER);
  }

  /* Wait query answer */
  return OOP_CONTINUE;
}

#ifdef RULI_SRV_DEBUG
static void show_dname(const char *label, const char *dname, int dname_len)
{
  char buf[RULI_LIMIT_DNAME_TEXT_BUFSZ];
  int  txt_len;
  int  result;

  fprintf(stderr, 
          "DEBUG: ruli_srv: show_dname(): dname=%u/%d", 
          (unsigned int) dname, dname_len);

  result = ruli_dname_decode(buf, RULI_LIMIT_DNAME_TEXT_BUFSZ, &txt_len,
                             dname, dname_len);
  assert(!result);

  fprintf(stderr,
          ": %s=%s (encoded_len=%d txt_len=%d)\n",
          label, buf, dname_len, txt_len);
}
#endif

static void *on_srv_answer(ruli_res_query_t *qry, void *arg)
{
  ruli_srv_t *srv_qry = (ruli_srv_t *) arg;
  int        result;

  assert(qry->answer_code != RULI_SRV_CODE_VOID);

  /*
   * Underlying query failed?
   */
  if (qry->answer_code == RULI_CODE_TIMEOUT)
    return query_done(srv_qry, RULI_SRV_CODE_ALARM);

  if (qry->answer_code)
    return query_done(srv_qry, RULI_SRV_CODE_QUERY_FAILED);

#ifdef RULI_SRV_DEBUG
  {
    ruli_msg_header_t msg_hdr;

    msg_hdr = qry->answer_header;

    fprintf(stderr, 
	    "DEBUG: on_srv_answer(): underlying query succeded: "
	    "id=%d rcode=%d qd=%d an=%d ns=%d ar=%d "
	    "answer_buf_size=%d answer_msg_len=%d\n", 
	    msg_hdr.id, msg_hdr.rcode, 
	    msg_hdr.qdcount, msg_hdr.ancount, 
	    msg_hdr.nscount, msg_hdr.arcount,
	    qry->answer_buf_size, qry->answer_msg_len);
  }
#endif

  /*
   * Parse answer for SRV records
   */

  assert(sizeof(ruli_uint8_t) == sizeof(char));

  result = ruli_parse_message(&srv_qry->parse, &qry->answer_header, 
			      (ruli_uint8_t *) qry->answer_buf,
                              qry->answer_msg_len);
  if (result)
    return query_done(srv_qry, RULI_SRV_CODE_PARSE_FAILED);

  /*
   * Check reply code and answer count
   */

  if ((qry->answer_header.rcode != RULI_RCODE_NOERROR) ||
      (qry->answer_header.ancount < 1)) {

#ifdef RULI_SRV_DEBUG
    fprintf(stderr, 
	    "DEBUG: on_srv_answer(): SRV query failed\n");
#endif

    /* Fallback query */
    return srv_qry->fallback(srv_qry);
  }

  /*
   * NOERROR && (ancount > 0) 
   */
  assert(qry->answer_header.rcode == RULI_RCODE_NOERROR);
  assert(qry->answer_header.ancount > 0);

  /* 
   * Processing of SRV answer:
   *
   * Step 1/6: One SRV RR with target == '.' ?
   * Step 2/6: Parse rdata portion of all SRV RRs
   * Step 3/6: Sort SRV RRs by priority
   * Step 4/6: Select SRV RRs by random weighted order
   * Step 5/6: Build list of srv answers by inspecting additional section
   * Step 6/6: Launch queries to fill missing addresses, if any
   */

  /* 
   * Step 1/6: One SRV RR with target == '.' ?
   */
  
  if (qry->answer_header.ancount == 1) {

    ruli_list_t *an_list = &srv_qry->parse.answer_list;

    if (ruli_list_size(an_list) == 1) {

      ruli_rr_t *rr = (ruli_rr_t *) ruli_list_top(an_list);

      if (rr->qclass == RULI_RR_CLASS_IN) {

	if (rr->type == RULI_RR_TYPE_SRV) {

	  ruli_srv_rdata_t srv_rdata;

	  if (ruli_parse_rr_srv(&srv_rdata, rr->rdata, rr->rdlength))
	    return query_done(srv_qry, RULI_SRV_CODE_PARSE_FAILED);

	  /* target == '.' ? */
	  if (*srv_rdata.target == '\0') {

	    /* Sanity test */
	    if (srv_rdata.target_len != 1)
	      return query_done(srv_qry, RULI_SRV_CODE_PARSE_FAILED);

            /*
             * ruli_srv.c: target=='.': Owner match?
             */

            assert(sizeof(ruli_uint8_t) == sizeof(char));

#ifdef RULI_SRV_DEBUG
            show_dname("on_srv_answer(): target=='.': qdomain",
                       (const char *) qry->full_dname, qry->full_dname_len);
#endif

            if (ruli_dname_compare(rr->owner,
                                   (ruli_uint8_t *) qry->answer_buf,
                                   qry->answer_msg_len,
                                   (ruli_uint8_t *) qry->full_dname,
                                   qry->full_dname_len))
              return query_done(srv_qry, RULI_SRV_CODE_PARSE_FAILED);

	      
	    return query_done(srv_qry, RULI_SRV_CODE_UNAVAILABLE);
	  }
	}
      }
    }
  } /* One SRV RR with target == '.' ? */

  /*
   * Step 2/6: Parse rdata portion of all SRV RRs
   */
  {
    ruli_list_t *an_list     = &srv_qry->parse.answer_list;
    int         an_list_size = ruli_list_size(an_list);
    int         i;

    for (i = 0; i < an_list_size; ++i) {
      ruli_rr_t        *rr = (ruli_rr_t *) ruli_list_get(an_list, i);
      ti_srv_rr_t      *ti_srv_rr;

      if (rr->qclass != RULI_RR_CLASS_IN)
	continue;

      if (rr->type != RULI_RR_TYPE_SRV)
	continue;

#ifdef RULI_SRV_DEBUG
      fprintf(stderr,
	      "DEBUG: on_srv_answer(): considering SRV-RR owner: %d of %d\n",
      (i + 1), an_list_size);
#endif

      if (ruli_dname_compare(rr->owner,
                             (ruli_uint8_t *) qry->answer_buf,
                             qry->answer_msg_len,
                             (ruli_uint8_t *) qry->full_dname,
                             qry->full_dname_len))
        return query_done(srv_qry, RULI_SRV_CODE_PARSE_FAILED);

#ifdef RULI_SRV_DEBUG
      fprintf(stderr,
	      "DEBUG: on_srv_answer(): SRV-RR owner OK: %d of %d\n",
      (i + 1), an_list_size);
#endif

      ti_srv_rr = (ti_srv_rr_t *) ruli_malloc(sizeof(ti_srv_rr_t));
      if (!ti_srv_rr)
	    return query_done(srv_qry, RULI_SRV_CODE_MALLOC);

      if (ruli_list_push(&srv_qry->rr_srv_list, ti_srv_rr)) {
	    ruli_free(ti_srv_rr);
	    return query_done(srv_qry, RULI_SRV_CODE_LIST);
      }

      if (ruli_parse_rr_srv(&ti_srv_rr->srv_rdata, rr->rdata, rr->rdlength))
	    return query_done(srv_qry, RULI_SRV_CODE_PARSE_FAILED);
    
      ti_srv_rr->ttl = rr->ttl;
      ti_srv_rr->qclass = rr->qclass;
      ti_srv_rr->type = rr->type;
    }

  }

#ifdef RULI_SRV_DEBUG
  {
    int i;
    ruli_list_t *list = &srv_qry->rr_srv_list;

    fflush(stdout);
    for (i = 0; i < ruli_list_size(list); ++i) {
      ti_srv_rr_t *ti_srv_rr_temp = \
	(ti_srv_rr_t *) ruli_list_get(list, i);
      ruli_srv_rdata_t *srv_rdata = &ti_srv_rr_temp->srv_rdata;

      fflush(stderr);
      fprintf(stderr,
	      "DEBUG: on_srv_answer(): SRV RR: "
	      "priority=%d weight=%d port=%d ttl=%d\n",
	      srv_rdata->priority, srv_rdata->weight, srv_rdata->port,ti_srv_rr_temp->ttl);
      fflush(stderr);
    }
  }
#endif

  /*
   * Step 3/6: Sort SRV RRs by priority
   */
  {
    ruli_list_t *src_list     = &srv_qry->rr_srv_list;
    int         src_list_size = ruli_list_size(src_list);
    int         j;

    /*
     * Handle every RR based on priority (higher priority first)
     */
    for (j = 0; j < src_list_size; ++j) {
      ti_srv_rr_t *ti_srv_rr    = \
	(ti_srv_rr_t *) ruli_list_get(src_list, j);
      ruli_list_t      *dst_list     = &srv_qry->pri_srv_list;
      int              dst_list_size = ruli_list_size(dst_list);
      int              i;

      assert(ti_srv_rr);
      ruli_srv_rdata_t *srv_rdata = &ti_srv_rr->srv_rdata;
      assert(srv_rdata);

      /*
       * Find a lower-or-equal priority
       */
      for (i = 0; i < dst_list_size; ++i) {
	ti_srv_rr_t *srv_rr_d = (ti_srv_rr_t *) ruli_list_get(dst_list, i);
	ruli_srv_rdata_t *rd = &srv_rr_d->srv_rdata;

	if (srv_rdata->priority < rd->priority)
	  continue;

	/*
	 * For this priority, put 0-weight-elements at tail
	 */
	if (srv_rdata->weight == 0) {

	  /*
	   * Find begin of next priority and insert there
	   */
	  for (; i < dst_list_size; ++i) {
	    ti_srv_rr_t *s_srv_rr_d = (ti_srv_rr_t *) ruli_list_get(dst_list, i);
	    ruli_srv_rdata_t *s_rd = &s_srv_rr_d->srv_rdata;

	    if (srv_rdata->priority != s_rd->priority)
	      break;
	  } /* for */
	  if (i == dst_list_size)
	    break; /* Insert at tail (of this priority) */

	}

	if (ruli_list_insert_at(dst_list, i, ti_srv_rr))
	  return query_done(srv_qry, RULI_SRV_CODE_LIST);

	srv_rdata = 0; /* mark as handled */
	  
	break;
      } /* for */

      /* If not handled yet, insert at tail */
      if (ti_srv_rr)
	if (ruli_list_push(dst_list, ti_srv_rr))
	  return query_done(srv_qry, RULI_SRV_CODE_LIST);

    } /* while */
  }

#ifdef RULI_SRV_DEBUG
  {
    int i;
    ruli_list_t *list = &srv_qry->pri_srv_list;

    fflush(stdout);
    for (i = 0; i < ruli_list_size(list); ++i) {
      ti_srv_rr_t *ti_srv_rr_temp = \
	(ti_srv_rr_t *) ruli_list_get(list, i);
      ruli_srv_rdata_t *srv_rdata = &ti_srv_rr_temp->srv_rdata;

      fflush(stderr);
      fprintf(stderr,
	      "DEBUG: on_srv_answer(): priority SRV RR: "
	      "priority=%d weight=%d port=%d ttl=%d\n",
	      srv_rdata->priority, srv_rdata->weight, srv_rdata->port,ti_srv_rr_temp->ttl);
      fflush(stderr);
    }
  }
#endif

  /*
   * Step 4/6: Select SRV RRs by random weighted order
   */
  {
    ruli_list_t *src_list = &srv_qry->pri_srv_list;
    ruli_list_t *dst_list = &srv_qry->wei_srv_list;

    /*
     * Iterate over every priority
     */
    for (;;) {
      int              src_list_size        = ruli_list_size(src_list);
      ruli_uint16_t    curr_priority;
      int              priority_weight_sum;
      int              curr;
      int              i;
      int              rnd;
      int              run_sum;
      ti_srv_rr_t      *srv_rr_d;
      ruli_srv_rdata_t *srv_rd;

      if (src_list_size < 1)
	break;
	
      /*
       * Get current priority
       */
      curr          = src_list_size - 1;
      srv_rr_d      = (ti_srv_rr_t *) ruli_list_get(src_list, curr);
      srv_rd        = &srv_rr_d->srv_rdata;
      curr_priority = srv_rd->priority;

      /*
       * Accumulate weight sum for priority
       */
      priority_weight_sum = 0;
      for (i = curr; i >= 0; --i) {
      ti_srv_rr_t *srv_rr_rd      = (ti_srv_rr_t *) ruli_list_get(src_list, i);
	  ruli_srv_rdata_t *rd = &srv_rr_rd->srv_rdata;
	  
	if (curr_priority != rd->priority)
	  break;
	  
	priority_weight_sum += rd->weight;
      } /* for */

      /*
       * Pick random number: 0..priority_weight_sum
       */
      rnd = ruli_rand_next(&srv_qry->srv_resolver->rand_ctx, 
			   0, priority_weight_sum);

      /*
       * Select least running sum
       */
      run_sum = 0;
      for (i = curr; ; --i) {
	ruli_srv_rdata_t *rd;

	assert(i >= 0);

    ti_srv_rr_t *srv_rr_rd      = (ti_srv_rr_t *) ruli_list_get(src_list, i);
	rd = (ruli_srv_rdata_t *)&srv_rr_rd->srv_rdata;
	run_sum += rd->weight;

	if (run_sum < rnd)
	  continue;
	  
	/*
	 * Move from src_list to dst_list
	 * (Both lists are only referential)
	 */
	ruli_list_shift_at(src_list, i);
	if (ruli_list_push(dst_list, srv_rr_rd))
	  return query_done(srv_qry, RULI_SRV_CODE_LIST);
	  
	break;
	  
      } /* for */

    } /* for */
  }

#ifdef RULI_SRV_DEBUG
  {
    int i;
    ruli_list_t *list = &srv_qry->wei_srv_list;

    fflush(stdout);
    for (i = 0; i < ruli_list_size(list); ++i) {
      ti_srv_rr_t *ti_srv_rr_temp = \
	(ti_srv_rr_t *) ruli_list_get(list, i);
      ruli_srv_rdata_t *srv_rdata = &ti_srv_rr_temp->srv_rdata;

      fflush(stderr);
      fprintf(stderr,
	      "DEBUG: on_srv_answer(): weight SRV RR: "
	      "priority=%d weight=%d port=%d ttl=%d\n",
	      srv_rdata->priority, srv_rdata->weight, srv_rdata->port, ti_srv_rr_temp->ttl);
      fflush(stderr);
    }
  }
#endif

  /*
   * Step 5/6: Build list of srv answers by inspecting additional section
   */
  {
    ruli_list_t *src_list     = &srv_qry->wei_srv_list;
    ruli_list_t *dst_list     = &srv_qry->answer_srv_list;
    int         src_list_size = ruli_list_size(src_list);
    int         i;

    assert(ruli_list_size(dst_list) == 0);

#ifdef RULI_SRV_DEBUG
    {
      fflush(stdout);
      fprintf(stderr,
	      "DEBUG: %s: %s(): %d: "
	      "BEFORE addit inspection: "
	      "srv_rr_list_size=%d srv_answer_list_size=%d\n",
	      __FILE__, __PRETTY_FUNCTION__, __LINE__,
	      src_list_size, ruli_list_size(&srv_qry->answer_srv_list));
      fflush(stderr);
    }
#endif

    /*
     * Build answer records inspecting additional section
     */

    /* Scan all targets */
    for (i = 0; i < src_list_size; ++i) {
      ti_srv_rr_t *srv_rr_rd = (ti_srv_rr_t *) ruli_list_get(src_list, i);
      ruli_srv_rdata_t *rd = (ruli_srv_rdata_t *)&srv_rr_rd->srv_rdata;
      ruli_srv_entry_t *srv_entry;

      /* Create SRV entry and append it to list */
      srv_entry =_ruli_srv_list_new_entry(dst_list,
					  (const char *) rd->target,
					  rd->target_len,
					  rd->priority,
					  rd->weight,
					  rd->port,
                      srv_rr_rd->ttl,
                      srv_rr_rd->qclass,
                      srv_rr_rd->type);
      if (!srv_entry)
	return query_done(srv_qry, RULI_SRV_CODE_MALLOC);

      /*
       * Look up target address(es) in additional section
       */
      {
	ruli_list_t *ad_list     = &srv_qry->parse.additional_list;
	int         ad_list_size = ruli_list_size(ad_list);
	int         j;

	/* Scan additional section */
	for (j = 0; j < ad_list_size; ++j) {
	  ruli_rr_t *rr = (ruli_rr_t *) ruli_list_get(ad_list, j);
	  ruli_addr_t *addr;

	  if (rr->qclass != RULI_RR_CLASS_IN)
	    continue;

	  if (!ruli_rr_type_is_address(srv_qry->srv_options, rr->type))
	    continue;

	  /* Compare SRV target against additional record owner */
	  if (ruli_dname_compare(rr->owner,
				 (ruli_uint8_t *) qry->answer_buf,
				 qry->answer_msg_len,
				 (ruli_uint8_t *) srv_entry->target,
				 srv_entry->target_len))
	    continue;

	  /* Allocate space for address */
	  addr = (ruli_addr_t *) ruli_malloc(sizeof(*addr));
	  if (!addr)
	    return query_done(srv_qry, RULI_SRV_CODE_MALLOC);

	  /* Save space */
	  if (ruli_list_push(&srv_entry->addr_list, addr)) {
	    ruli_free(addr); 
	    return query_done(srv_qry, RULI_SRV_CODE_LIST);
	  }

	  /* Write address into space */
	  ruli_parse_addr_rr(addr, rr, srv_qry->srv_options);

	} /* for */
      }

    } /* for */

#ifdef RULI_SRV_DEBUG
    {
      fflush(stdout);
      fprintf(stderr,
	      "DEBUG: %s: %s(): %d: "
	      "AFTER addit inspection: "
	      "srv_rr_list_size=%d srv_answer_list_size=%d\n",
	      __FILE__, __PRETTY_FUNCTION__, __LINE__,
	      src_list_size, ruli_list_size(&srv_qry->answer_srv_list));
      fflush(stderr);
    }
#endif

    assert(ruli_list_size(dst_list) == src_list_size);

  } /* Build list of srv answers by inspecting additional section */

#ifdef RULI_SRV_DEBUG
  {
    int i;
    ruli_list_t *list = &srv_qry->answer_srv_list;

    fflush(stdout);
    for (i = 0; i < ruli_list_size(list); ++i) {
      ruli_srv_entry_t *srv_entry = \
	(ruli_srv_entry_t *) ruli_list_get(list, i);

      fprintf(stderr,
	      "DEBUG: on_srv_answer(): answer SRV RR: "
	      "priority=%d weight=%d port=%d ttl=%d\n",
	      srv_entry->priority, srv_entry->weight, srv_entry->port, srv_entry->ttl);
    }
    fflush(stderr);
  }
#endif

  /*
   * Step 6/6: Launch queries to fill missing addresses, if any
   */

  assert(srv_qry->under.walk_index == -1);
  srv_qry->under.walk_index = 0;

  return _ruli_srv_answer_walk(srv_qry);
}

int _ruli_srv_query_submit(ruli_srv_t *srv_qry, 
			   void *(*fallback_call)(ruli_srv_t *))
{
  ruli_res_query_t *qry = &srv_qry->query;
  int              result;
  char             *qdomain;
  int              qdomain_len;

  assert(srv_qry);
  assert(fallback_call);

  /* can't disable all address families */
  assert( !((srv_qry->srv_options & RULI_RES_OPT_SRV_NOINET) &&
	    (srv_qry->srv_options & RULI_RES_OPT_SRV_NOINET6)) );

  srv_qry->fallback = fallback_call;

  /*
   * Concat srv_service + srv_domain into qdomain
   */

  {
    int service_len = srv_qry->srv_service_len;
    int concat_len;

    --service_len;

    assert(srv_qry->srv_service[service_len] == '\0');

    qdomain_len = service_len + srv_qry->srv_domain_len;

    qdomain = (char *) ruli_malloc(qdomain_len);
    if (!qdomain)
      return RULI_SRV_MALLOC;

    if (ruli_dname_concat((ruli_uint8_t *) qdomain, qdomain_len,
                          &concat_len,
                          (ruli_uint8_t *) srv_qry->srv_service,
                          srv_qry->srv_service_len,
			  (ruli_uint8_t *) srv_qry->srv_domain,
                          srv_qry->srv_domain_len)) {
      ruli_free(qdomain);
      return RULI_SRV_CONCAT;
    }
   
    assert(qdomain_len == concat_len);

    srv_qry->qdomain     = qdomain;
    srv_qry->qdomain_len = qdomain_len;

#ifdef RULI_SRV_DEBUG
    {
      show_dname("ruli_srv_query_submit(): service",
                 srv_qry->srv_service, srv_qry->srv_service_len);
      show_dname("ruli_srv_query_submit(): domain",
                 srv_qry->srv_domain, srv_qry->srv_domain_len);
      show_dname("ruli_srv_query_submit(): qdomain",
                 qdomain, qdomain_len);
    }
#endif
  }
    
  /*
   * Initialize members
   */

  /* stores data */
  if (ruli_list_new(&srv_qry->rr_srv_list)) {
    ruli_free(srv_qry->qdomain);
    return RULI_SRV_CODE_LIST;
  }

  /* stores only references */
  if (ruli_list_new(&srv_qry->pri_srv_list)) {
      ruli_free(srv_qry->qdomain);
      ruli_list_delete(&srv_qry->rr_srv_list);
      return RULI_SRV_CODE_LIST;
  }

  /* stores only references */
  if (ruli_list_new(&srv_qry->wei_srv_list)) {
    ruli_free(srv_qry->qdomain);
    ruli_list_delete(&srv_qry->rr_srv_list);
    ruli_list_delete(&srv_qry->pri_srv_list);
    return RULI_SRV_CODE_LIST;
  }

  if (ruli_list_new(&srv_qry->answer_srv_list)) {
    ruli_free(srv_qry->qdomain);
    ruli_list_delete(&srv_qry->rr_srv_list);
    ruli_list_delete(&srv_qry->pri_srv_list);
    ruli_list_delete(&srv_qry->wei_srv_list);
    return RULI_SRV_CODE_LIST;
  }

  if (ruli_parse_new(&srv_qry->parse)) {
    ruli_free(srv_qry->qdomain);
    ruli_list_delete(&srv_qry->rr_srv_list);
    ruli_list_delete(&srv_qry->pri_srv_list);
    ruli_list_delete(&srv_qry->wei_srv_list);
    ruli_list_delete(&srv_qry->answer_srv_list);
    return RULI_SRV_CODE_LIST;
  }

  srv_qry->answer_code = RULI_SRV_CODE_VOID;
  srv_qry->last_rcode = RULI_RCODE_VOID;

  srv_qry->under.walk_index = -1;

  /*
   * Define callback parameters
   */
  qry->q_on_answer     = on_srv_answer;
  qry->q_on_answer_arg = srv_qry;

  /*
   * Pass on query parameters
   */
  qry->q_domain     = qdomain;
  qry->q_domain_len = qdomain_len;
  qry->q_class      = RULI_RR_CLASS_IN;
  qry->q_type       = RULI_RR_TYPE_SRV;
  qry->q_options    = srv_qry->srv_options;

  /*
   * If the RULI port is explicitely defined in the URI,
   * the user wants only address records.
   *
   * Skip SRV query and fetch addresses instead.
   */
  if (srv_qry->srv_options & RULI_RES_OPT_SRV_URI_PORT) {
    srv_qry->query.resolver = 0;
    {
      void *oop_result = _ruli_srv_answer_fallback_addr(srv_qry);
      if (oop_result != OOP_CONTINUE)
	return RULI_SRV_CODE_QUERY_FAILED;
    }
    return RULI_SRV_OK;
  }

  /*
   * Submit plain query to underlying resolver
   */
  result = ruli_res_query_submit(srv_qry->srv_resolver, qry);
  if (result) {
    ruli_srv_query_delete(srv_qry);
    return RULI_SRV_QRY_SUBMIT;
  }

  return RULI_SRV_OK;
}

int ruli_srv_query_submit(ruli_srv_t *srv_qry)
{
  assert(srv_qry);

  return _ruli_srv_query_submit(srv_qry, _ruli_srv_answer_fallback_addr);
}

void ruli_srv_query_delete(ruli_srv_t *srv_qry)
{
#ifdef RULI_SRV_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s: %s(): %d: query_id=%d\n", 
	  __FILE__, __PRETTY_FUNCTION__, __LINE__,
	  srv_qry->query.query_id);
#endif /* RULI_SRV_DEBUG */

  /*
   * Dispose list of srv answers
   */
  {
    ruli_list_t *list     = &srv_qry->answer_srv_list;
    int         list_size = ruli_list_size(list);
    int         i;

    /*
     * For each entry, release list of *ruli_addr_t
     */
    for (i = 0; i < list_size; ++i) {
      ruli_srv_entry_t *srv_entry = (ruli_srv_entry_t *) \
	ruli_list_get(list, i);
      ruli_list_dispose_trivial(&srv_entry->addr_list);
    }
  }
  ruli_list_dispose_trivial(&srv_qry->answer_srv_list);

  ruli_parse_delete(&srv_qry->parse);
  ruli_list_dispose_trivial(&srv_qry->rr_srv_list);
  ruli_list_delete(&srv_qry->pri_srv_list);
  ruli_list_delete(&srv_qry->wei_srv_list);
  ruli_free(srv_qry->qdomain);
  ruli_res_query_delete(&srv_qry->query);
}

ruli_srv_rcode_kind_t ruli_srv_rcode_kind(ruli_srv_t *srv_qry)
{
  int code = srv_qry->answer_code;

  if ((code >= RULI_SRV_CODE_OK) &&
      (code <= RULI_SRV_CODE_UNAVAILABLE))
    return RULI_SRV_RCODE_SRV;

  if ((code >= RULI_SRV_CODE_WALK_EMPTY) &&
      (code <= RULI_SRV_CODE_WALK_RCODE))
    return RULI_SRV_RCODE_WALK;
  
  if ((code >= RULI_SRV_CODE_FALL_EMPTY) &&
      (code <= RULI_SRV_CODE_FALL_RCODE))
    return RULI_SRV_RCODE_FALL;

  return RULI_SRV_RCODE_NONE;
}

int ruli_srv_rcode(ruli_srv_t *srv_qry)
{
  ruli_srv_rcode_kind_t kind = ruli_srv_rcode_kind(srv_qry);

  int rcode = -1; /* picky compilers */

  switch (kind) {
  case RULI_SRV_RCODE_NONE:
    rcode = RULI_RCODE_VOID;
    break;

  case RULI_SRV_RCODE_SRV:
    rcode = srv_qry->query.answer_header.rcode;
    break;

  case RULI_SRV_RCODE_WALK:
  case RULI_SRV_RCODE_FALL:
    rcode = srv_qry->last_rcode;
    break;

  default:
    assert(0);
  }

  return rcode;
}

ruli_srv_entry_t *_ruli_srv_list_new_entry(ruli_list_t *srv_list,
					   const char *target,
					   int target_len,
					   int priority,
					   int weight,
					   int port,
                       int ttl,
                       int qclass,
                       int type)
{
  ruli_srv_entry_t *srv_entry;

  /* Create SRV record */
  srv_entry = (ruli_srv_entry_t *) ruli_malloc(sizeof(*srv_entry));
  if (!srv_entry)
    return 0;

  /* Create record addr list */
  if (ruli_list_new(&srv_entry->addr_list)) {
    ruli_free(srv_entry);
    return 0;
  }

  /* Save record */
  if (ruli_list_push(srv_list, srv_entry)) {
    ruli_list_delete(&srv_entry->addr_list);
    ruli_free(srv_entry);
    return 0;
  }

  /*
   * Init record
   */
  srv_entry->priority = priority;
  srv_entry->weight   = weight;
  srv_entry->port     = port;

  assert(target_len <= RULI_LIMIT_DNAME_ENCODED);

  srv_entry->target_len = target_len;
  memcpy(srv_entry->target, target, target_len);

  srv_entry->ttl = ttl;
  srv_entry->qclass = qclass;
  srv_entry->type = type;

  return srv_entry;
}
