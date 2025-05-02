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
  $Id: ruli_smtp.c,v 1.20 2005/06/06 22:57:02 evertonm Exp $
  */


#include <stdio.h>       /* FIXME: remove me [used for fprintf() debug] */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <ruli_smtp.h>
#include <ruli_mem.h>
#include <ruli_txt.h>
#include <ruli_host.h>


static int srv_cmp_priority(const void *srv1, const void *srv2)
{
  const ruli_srv_entry_t *e1 = *(const ruli_srv_entry_t * const *) srv1;
  const ruli_srv_entry_t *e2 = *(const ruli_srv_entry_t * const *) srv2;

  if (e1->priority < e2->priority)
    return -1;
  if (e1->priority > e2->priority)
    return 1;

  return 0;
}

static void sort_srv_by_mx_priority(ruli_srv_t *srv_qry)
{
  ruli_list_t *srv_list = &srv_qry->answer_srv_list;
  void **srv_head = srv_list->head;
  qsort(srv_head, ruli_list_size(srv_list), sizeof(void*), srv_cmp_priority);
}

static void _m_query_done(ruli_srv_t *srv_qry, ruli_res_query_t *mx_qry)
{
  assert(mx_qry);

  /* sort srv records by mx priority ? */
  if (mx_qry->answer_code == RULI_CODE_OK)
    sort_srv_by_mx_priority(srv_qry);
  
  ruli_res_query_delete(mx_qry);
  ruli_free(mx_qry);
}

static void *mx_query_done(ruli_srv_t *srv_qry, int srv_result_code,
                           ruli_res_query_t *mx_qry)
{
#ifdef RULI_SMTP_DEBUG
      fprintf(stderr, 
	      "DEBUG: %s: %s(): query_result=%s [%d]\n", 
	      __FILE__, __PRETTY_FUNCTION__,
	      srv_result_code ? "FAIL" : "SUCCESS", srv_result_code);
#endif

  _m_query_done(srv_qry, mx_qry);

  return _ruli_srv_query_done(srv_qry, srv_result_code);
}

static void *on_mx_answer(ruli_res_query_t *mx_qry, void *qry_arg)
{
  ruli_srv_t *srv_qry           = (ruli_srv_t *) qry_arg;
  int        prev_srv_list_size = ruli_list_size(&srv_qry->answer_srv_list);

#ifdef RULI_SMTP_DEBUG
  {
    char txt_dname_buf[RULI_LIMIT_DNAME_TEXT_BUFSZ];
    int  txt_dname_len;
    int  result;

    result = ruli_dname_decode(txt_dname_buf, RULI_LIMIT_DNAME_TEXT_BUFSZ,
			       &txt_dname_len, 
			       (const char *) mx_qry->full_dname, 
			       mx_qry->full_dname_len);
    assert(!result);

    fprintf(stderr, 
	    "DEBUG: %s: %s(): domain=%s domain_len=%d\n",
	    __FILE__, __PRETTY_FUNCTION__,
	    txt_dname_buf, txt_dname_len);

    fprintf(stderr, 
	    "DEBUG: %s: %s(): id=%d answer_code=%d rcode=%d\n",
	    __FILE__, __PRETTY_FUNCTION__,
	    mx_qry->query_id, mx_qry->answer_code, mx_qry->answer_header.rcode);
  }
#endif

  assert(mx_qry->answer_code != RULI_CODE_VOID);
  assert(!prev_srv_list_size);

  /*
   * Query failed?
   */
  if (mx_qry->answer_code) {
    srv_qry->last_rcode = RULI_RCODE_VOID;

    if (mx_qry->answer_code == RULI_CODE_TIMEOUT)
      return mx_query_done(srv_qry, RULI_SRV_CODE_FALL_ALARM, mx_qry);

    return mx_query_done(srv_qry, RULI_SRV_CODE_FALL_QUERY, mx_qry);
  }

  /*
   * Bad RCODE ?
   */
  {
    ruli_uint16_t rcode = mx_qry->answer_header.rcode;

    srv_qry->last_rcode = rcode;

    if (rcode != RULI_RCODE_NOERROR)
      return mx_query_done(srv_qry, RULI_SRV_CODE_FALL_RCODE, mx_qry);
  }

  /*
   * Parse answer
   */
  {
    ruli_uint8_t *wanted_owner;
    int          wanted_owner_len;

    ruli_parse_t parse;

    if (ruli_parse_new(&parse))
      return mx_query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER, mx_qry);

    {
      int result;

      assert(sizeof(ruli_uint8_t) == sizeof(char));

      result = ruli_parse_message(&parse, &mx_qry->answer_header,
				  (ruli_uint8_t *) mx_qry->answer_buf,
                                  mx_qry->answer_msg_len);
      if (result) {
	ruli_parse_delete(&parse);
	return mx_query_done(srv_qry, RULI_SRV_CODE_FALL_PARSE, mx_qry);
      }
    }

    assert(sizeof(ruli_uint8_t) == sizeof(char));

    wanted_owner     = (ruli_uint8_t *) mx_qry->full_dname;
    wanted_owner_len = mx_qry->full_dname_len;

    /*
     * Search for MX records in answer section
     */
    {
      ruli_list_t      *an_list     = &parse.answer_list;
      int              an_list_size = ruli_list_size(an_list);
      int              i;

#ifdef RULI_SMTP_DEBUG
      {
	char wanted_txt[RULI_LIMIT_DNAME_TEXT_BUFSZ];
	int  wanted_txt_len;
	int  result;
	
	assert(sizeof(ruli_uint8_t) == sizeof(char));
	
	result = ruli_dname_decode(wanted_txt, RULI_LIMIT_DNAME_TEXT_BUFSZ, 
				   &wanted_txt_len, 
				   (const char *) wanted_owner, 
				   wanted_owner_len);
	assert(!result);
	
	fprintf(stderr, 
		"DEBUG: %s: %s(): "
		"wanted owner=(%d)%s\n", 
		__FILE__, __PRETTY_FUNCTION__,
		wanted_txt_len, wanted_txt);
      }
#endif

      /*
       * Scan answer section for IN MX records
       */
      for (i = 0; i < an_list_size; ++i) {
	ruli_rr_t *rr = (ruli_rr_t *) ruli_list_get(an_list, i);

	if (rr->qclass != RULI_RR_CLASS_IN)
	  continue;
	if (rr->type != RULI_RR_TYPE_MX)
	  continue;

	/*
	 * Owner matches?
	 */

        if (ruli_dname_compare(rr->owner,
                               (ruli_uint8_t *) mx_qry->answer_buf,
                               mx_qry->answer_msg_len,
                               wanted_owner,
                               wanted_owner_len))
          continue;

	/*
	 * Find MX target
	 */

	{
	  int              j;
	  ruli_mx_rdata_t  mx_rdata;
	  ruli_list_t      *ad_list     = &parse.additional_list;
	  int              ad_list_size = ruli_list_size(ad_list);
	  ruli_srv_entry_t *srv_entry;

	  if (ruli_parse_rr_mx(&mx_rdata, rr->rdata, rr->rdlength,
			       (ruli_uint8_t *) mx_qry->answer_buf,
			       mx_qry->answer_msg_len)) {
	    ruli_parse_delete(&parse);
	    return mx_query_done(srv_qry, RULI_SRV_CODE_FALL_PARSE, mx_qry);
	  }

	  /* 
	   * Create SRV record and append it to list 
	   */
	  srv_entry =_ruli_srv_list_new_entry(&srv_qry->answer_srv_list,
					      (const char *) \
					      mx_rdata.target,
					      mx_rdata.target_len,
					      mx_rdata.preference,
					      -1,
					      srv_qry->srv_fallback_port,
                          rr->ttl,
                          rr->qclass,
                          rr->type);
	  if (!srv_entry) {
	    ruli_parse_delete(&parse);
	    return mx_query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER, mx_qry);
	  }

	  /*
	   * Search MX target addresses in additional section
	   */
	  
	  for (j = 0; j < ad_list_size; ++j) {
	    ruli_rr_t   *ad_rr = (ruli_rr_t *) ruli_list_get(ad_list, j);
	    ruli_addr_t *addr;

	    if (ad_rr->qclass != RULI_RR_CLASS_IN)
	      continue;
	    if (!ruli_rr_type_is_address(srv_qry->srv_options, ad_rr->type))
	      continue;

	    /*
	     * MX owner matches?
	     */

	    if (ruli_dname_compare(ad_rr->owner,
				   (ruli_uint8_t *) mx_qry->answer_buf,
				   mx_qry->answer_msg_len,
				   mx_rdata.target,
				   mx_rdata.target_len))
	      continue;

	    /*
	     * Save MX target address
	     */

	    /* Allocate space */
	    addr = (ruli_addr_t *) ruli_malloc(sizeof(*addr));
	    if (!addr) {
	      ruli_parse_delete(&parse);
	      return mx_query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER, mx_qry);
	    }
	    
	    /* Save space */
	    if (ruli_list_push(&srv_entry->addr_list, addr)) {
	      ruli_free(addr); 
	      ruli_parse_delete(&parse);
	      return mx_query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER, mx_qry);
	    }
	    
	    /* Write address into space */
	    ruli_parse_addr_rr(addr, ad_rr, srv_qry->srv_options);

	  } /* Save MX target addresses */
	  
	} /* Search MX target in additional section */
	
      } /* for: IN MX scan of answer section */
      
      /* 
       * Done if at least one SRV record has been built 
       */
      {
	int curr_srv_list_size = ruli_list_size(&srv_qry->answer_srv_list);
	
	assert(curr_srv_list_size >= prev_srv_list_size);
	
	if (curr_srv_list_size > prev_srv_list_size) {
#ifdef RULI_SMTP_DEBUG
	  {
	    int ii;
	    ruli_list_t *list = &srv_qry->answer_srv_list;
	    
	    fflush(stdout);
	    for (ii = 0; ii < ruli_list_size(list); ++ii) {
	      ruli_srv_entry_t *srv_entry = \
		(ruli_srv_entry_t *) ruli_list_get(list, ii);
	      
	      fprintf(stderr,
		      "DEBUG: %s: %s(): answer SRV RR: "
		      "priority=%d weight=%d port=%d\n",
		      __FILE__, __PRETTY_FUNCTION__,
		      srv_entry->priority, srv_entry->weight, srv_entry->port);
	    }
	    fflush(stderr);
	  }
#endif

	  ruli_parse_delete(&parse);

	  /* Dispose current IN MX fallback query */
	  _m_query_done(srv_qry, mx_qry);

	  /*
	   * Launch queries to fill missing addresses, if any
	   */

	  assert(srv_qry->under.walk_index == -1);
	  srv_qry->under.walk_index = 0;
	  
	  return _ruli_srv_answer_walk(srv_qry);
	}
      }

#ifdef RULI_SMTP_DEBUG
      fprintf(stderr, 
	      "DEBUG: %s: %s(): id=%d answer_code=%d, rcode=%d: "
	      "BUT: no matching IN MX record\n",
	      __FILE__, __PRETTY_FUNCTION__,
	      mx_qry->query_id, mx_qry->answer_code, 
	      mx_qry->answer_header.rcode);
#endif

    } /* MX scanning loop */    

    ruli_parse_delete(&parse);
    
  } /* parsed answer context */
  
  /* Dispose failed, current IN MX fallback query,
   that is replaced by IN A fallback query below */
  _m_query_done(srv_qry, mx_qry);

  /* Try default IN A fallback query */
  return _ruli_srv_answer_fallback_addr(srv_qry);
}

/*
  Fallback to 'smtp'
 */
static void *srv_answer_fallback_smtp(ruli_srv_t *srv_qry)
{
  ruli_res_query_t *mx_qry;

#ifdef RULI_SMTP_DEBUG
  {
    char txt_dname_buf[RULI_LIMIT_DNAME_TEXT_BUFSZ];
    int  txt_dname_len;
    int  result;

    result = ruli_dname_decode(txt_dname_buf, RULI_LIMIT_DNAME_TEXT_BUFSZ,
			       &txt_dname_len, 
			       srv_qry->srv_domain, srv_qry->srv_domain_len);
    assert(!result);

    fprintf(stderr, 
	    "DEBUG: %s: %s(): domain=%s len=%d\n",
	    __FILE__, __PRETTY_FUNCTION__,
	    txt_dname_buf, txt_dname_len);
  }
#endif

  assert(!ruli_list_size(&srv_qry->answer_srv_list));

  /*
   * Allocate space for fallback query
   */
  mx_qry = (ruli_res_query_t *) ruli_malloc(sizeof(*mx_qry));
  if (!mx_qry)
    return _ruli_srv_query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER);

  /*
   * Initialize fallback query arguments
   */
  mx_qry->q_on_answer     = on_mx_answer;
  mx_qry->q_on_answer_arg = srv_qry;
  mx_qry->q_domain        = srv_qry->srv_domain;
  mx_qry->q_domain_len    = srv_qry->srv_domain_len;
  mx_qry->q_class         = RULI_RR_CLASS_IN;
  mx_qry->q_type          = RULI_RR_TYPE_MX;
  mx_qry->q_options       = srv_qry->srv_options;

  /*
   * Submit fallback query
   */
  if (ruli_res_query_submit(srv_qry->srv_resolver, mx_qry)) {
    ruli_free(mx_qry);
    return _ruli_srv_query_done(srv_qry, RULI_SRV_CODE_FALL_OTHER);
  }

  /* Wait query answer */
  return OOP_CONTINUE;
}

ruli_search_srv_t *ruli_search_smtp_submit(ruli_res_t *resolver, 
					   void *(*call)(ruli_search_srv_t *search, void *arg),
					   void *call_arg,
					   long options,
					   const char *txt_domain)
{
  return _ruli_search_srv_submit(srv_answer_fallback_smtp,
				 resolver,
				 call,
				 call_arg,
				 options,
				 "_smtp._tcp",
				 txt_domain,
				 25);
}

