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
  $Id: ruli_host.c,v 1.6 2004/10/06 04:25:59 evertonm Exp $
  */


#include <assert.h>

#include <ruli_mem.h>
#include <ruli_txt.h>
#include <ruli_parse.h>
#include <ruli_host.h>


const char *ruli_host_errstr(int result)
{
  return "FIXME: ruli_host_errstr()";
}

int ruli_host_answer_code(ruli_host_t *host_qry)
{
  return host_qry->answer_code;
}

int ruli_host_rcode(ruli_host_t *host_qry)
{
  return host_qry->host_query.answer_header.rcode;
}

static int _ruli_need_query_in_a(int qtype, long options)
{
  /* can't disable all address families */
  assert( !((options & RULI_RES_OPT_SRV_NOINET) &&
            (options & RULI_RES_OPT_SRV_NOINET6)) );

  return ( (qtype == RULI_RR_TYPE_AAAA) &&
	   !(options & RULI_RES_OPT_SRV_NOINET) );
}

static int _ruli_reset_qtype(long options)
{
  /* can't disable all address families */
  assert( !((options & RULI_RES_OPT_SRV_NOINET) &&
            (options & RULI_RES_OPT_SRV_NOINET6)) );

  return (options & RULI_RES_OPT_SRV_NOINET6) ?
    RULI_RR_TYPE_A : RULI_RR_TYPE_AAAA;
}

/*
  This is an indirection to the user callback
  */
static void *query_done(ruli_host_t *host_qry, int result_code)
{
  assert(host_qry->answer_code == RULI_HOST_CODE_VOID);
  assert(result_code != RULI_HOST_CODE_VOID);

  host_qry->answer_code = result_code;

  /*
   * Invoke user callback
   */
  return host_qry->host_on_answer(host_qry, host_qry->host_on_answer_arg);
}

static void *query_resubmit(ruli_host_t *host_qry)
{
  ruli_res_query_t *qry = &host_qry->host_query;

#ifdef RULI_HOST_DEBUG
  fprintf(stderr,
          "DEBUG: %s: %s(): %d: OLD query_id=%d\n",
          __FILE__, __PRETTY_FUNCTION__, __LINE__,
          qry->query_id);
#endif /* RULI_HOST_DEBUG */

  /* reset query, but do not dispose it */
  ruli_res_query_delete(qry);

  assert(!_ruli_need_query_in_a(qry->q_type, host_qry->host_options));

  if (ruli_res_query_submit(host_qry->host_resolver, qry))
    return query_done(host_qry, RULI_HOST_CODE_QUERY);

#ifdef RULI_HOST_DEBUG
  fprintf(stderr,
          "DEBUG: %s: %s(): %d: NEW query_id=%d\n",
          __FILE__, __PRETTY_FUNCTION__, __LINE__,
          qry->query_id);
#endif /* RULI_HOST_DEBUG */

  return OOP_CONTINUE;
}

static void *on_host_answer(ruli_res_query_t *qry, void *qry_arg)
{
  ruli_host_t *host_qry = (ruli_host_t *) qry_arg;
  int prev_addr_list_size = ruli_list_size(&host_qry->answer_addr_list);

#ifdef RULI_HOST_DEBUG
  {
    char txt_dname_buf[RULI_LIMIT_DNAME_TEXT_BUFSZ];
    int  txt_dname_len;
    int  result;

    result = ruli_dname_decode(txt_dname_buf, RULI_LIMIT_DNAME_TEXT_BUFSZ,
			       &txt_dname_len, 
			       (const char *) qry->full_dname, 
			       qry->full_dname_len);
    assert(!result);

    fprintf(stderr, 
	    "DEBUG: %s: %s(): %d: domain=%s len=%d\n",
	    __FILE__, __PRETTY_FUNCTION__, __LINE__,
	    txt_dname_buf, txt_dname_len);

    fprintf(stderr, 
	    "DEBUG: %s: %s(): %d: "
	    "query_id=%d qtype=%d answer_code=%d rcode=%d\n",
	    __FILE__, __PRETTY_FUNCTION__, __LINE__,
	    qry->query_id, qry->q_type, qry->answer_code, 
	    qry->answer_header.rcode);
  }
#endif

  assert(qry->answer_code != RULI_CODE_VOID);

  /*
   * Query failed?
   */
  if (qry->answer_code) {
    if (qry->answer_code == RULI_CODE_TIMEOUT)
      return query_done(host_qry, RULI_HOST_CODE_ALARM);

    return query_done(host_qry, RULI_HOST_CODE_QUERY);
  }

  /*
   * Bad RCODE ?
   */
  {
    ruli_uint16_t rcode = qry->answer_header.rcode;

    if (rcode != RULI_RCODE_NOERROR) {

      if (_ruli_need_query_in_a(qry->q_type, host_qry->host_options)) {
	qry->q_type = RULI_RR_TYPE_A;
	return query_resubmit(host_qry);
      }

      return query_done(host_qry, RULI_HOST_CODE_RCODE);
    }
  }

  /*
   * Parse answer
   */
  {
    ruli_uint8_t       *wanted_owner;
    int                wanted_owner_len;
    ruli_cname_rdata_t cname_rdata;
    int                cname_found; /* boolean */
    int                remaining_cname_depth = host_qry->host_max_cname_depth;

    ruli_parse_t parse;

    if (ruli_parse_new(&parse))
      return query_done(host_qry, RULI_HOST_CODE_OTHER);

    {
      int result;

      assert(sizeof(ruli_uint8_t) == sizeof(char));

      result = ruli_parse_message(&parse, &qry->answer_header,
				  (ruli_uint8_t *) qry->answer_buf,
                                  qry->answer_msg_len);
      if (result) {
	ruli_parse_delete(&parse);
	return query_done(host_qry, RULI_HOST_CODE_PARSE);
      }
    }

    assert(sizeof(ruli_uint8_t) == sizeof(char));

    wanted_owner     = (ruli_uint8_t *) qry->full_dname;
    wanted_owner_len = qry->full_dname_len;

    /*
     * Search for Address records
     *
     * The following algorithm scans the answer section
     * for CNAME records:
     *
     * 1/3: We look for address records for the wanted_owner
     * 2/3: If at least one address record is found, we're done
     * 3/3: We search IN CNAME records for the wanted_owner
     *      If a CNAME record is found, we make
     *      wanted_owner <- cname_target and go to 1/3
     *      Otherwise, if no CNAME found, we quit
     */
    for (;;) {
      ruli_list_t *an_list     = &parse.answer_list;
      int         an_list_size = ruli_list_size(an_list);
      int         i;

#ifdef RULI_HOST_DEBUG
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
		"DEBUG: %s: %s(): %d: "
		"wanted owner=(%d)%s answer_list_len=%d\n", 
		__FILE__, __PRETTY_FUNCTION__, __LINE__,
		wanted_txt_len, wanted_txt, an_list_size);
      }
#endif

      /*
       * 1/3: Scan answer section for address records
       */
      for (i = 0; i < an_list_size; ++i) {
	ruli_rr_t *rr = (ruli_rr_t *) ruli_list_get(an_list, i);

	if (rr->qclass != RULI_RR_CLASS_IN)
	  continue;
	if (!ruli_rr_type_is_address(host_qry->host_options, rr->type))
	  continue;

	/*
	 * Owner matches?
	 */

        if (ruli_dname_compare(rr->owner,
                               (ruli_uint8_t *) qry->answer_buf,
                               qry->answer_msg_len,
                               wanted_owner,
                               wanted_owner_len))
          continue;

	/*
	 * Save address
	 */
	{
	  ruli_addr_t *addr;

	  /* Allocate space */
	  addr = (ruli_addr_t *) ruli_malloc(sizeof(*addr));
	  if (!addr) {
	    ruli_parse_delete(&parse);
	    return query_done(host_qry, RULI_HOST_CODE_OTHER);
	  }

	  /* Save space */
	  if (ruli_list_push(&host_qry->answer_addr_list, addr)) {
	    ruli_free(addr); 
	    ruli_parse_delete(&parse);
	    return query_done(host_qry, RULI_HOST_CODE_OTHER);
	  }

	  /* Write address into space */
	  ruli_parse_addr_rr(addr, rr, host_qry->host_options);

#ifdef RULI_HOST_DEBUG
          fprintf(stderr, "DEBUG: %s: %s(): %d: "
                  "query_id=%d q_type=%d addr_pushed=",
                  __FILE__, __PRETTY_FUNCTION__, __LINE__,
                  qry->query_id, qry->q_type);
          ruli_addr_print(stderr, addr);
          fprintf(stderr, "\n");
#endif
	
	} /* save address */

      } /* for: scan answer section for addresses */

      /* 
       * 2/3: Done if at least one address record has been added 
       */
      {
	int addr_list_size = ruli_list_size(&host_qry->answer_addr_list);

#ifdef RULI_HOST_DEBUG
        fprintf(stderr, "DEBUG: %s: %s(): %d: "
                "query_id=%d q_type=%d addr_list_size=%d done=%d\n",
                __FILE__, __PRETTY_FUNCTION__, __LINE__,
                qry->query_id, qry->q_type, addr_list_size,
                addr_list_size > prev_addr_list_size);
#endif
	
	if (addr_list_size > prev_addr_list_size) {
	  ruli_parse_delete(&parse);

	  if (_ruli_need_query_in_a(qry->q_type, host_qry->host_options)) {
	    qry->q_type = RULI_RR_TYPE_A;
	    return query_resubmit(host_qry);
	  }

	  return query_done(host_qry, RULI_HOST_CODE_OK);
	}
      }

#ifdef RULI_HOST_DEBUG
      fprintf(stderr, 
	      "DEBUG: %s: %s(): %d: "
              "query_id=%d qtype=%d answer_code=%d rcode=%d: "
	      "BUT: no matching address record\n",
	      __FILE__, __PRETTY_FUNCTION__, __LINE__,
	      qry->query_id, qry->q_type, qry->answer_code,
              qry->answer_header.rcode);
#endif

      /*
       * 3/3: Scan answer section for IN CNAME records
       */

      cname_found = 0; /* false */

      for (i = 0; i < an_list_size; ++i) {
	ruli_rr_t *rr = (ruli_rr_t *) ruli_list_get(an_list, i);

	if (rr->qclass != RULI_RR_CLASS_IN)
	  continue;
	if (rr->type != RULI_RR_TYPE_CNAME)
	  continue;

	/*
	 * Owner matches?
	 */

        if (ruli_dname_compare(rr->owner,
                               (ruli_uint8_t *) qry->answer_buf,
                               qry->answer_msg_len,
                               wanted_owner,
                               wanted_owner_len))
	  continue;
	
	/* Get new owner from CNAME target */
	
	assert(sizeof(ruli_uint8_t) == sizeof(char));
	
	if (ruli_parse_rr_cname(&cname_rdata, rr->rdata, rr->rdlength,
				(ruli_uint8_t *) qry->answer_buf,
				qry->answer_msg_len)) {
	  ruli_parse_delete(&parse);
	  return query_done(host_qry, RULI_HOST_CODE_PARSE);
	}
	
#ifdef RULI_HOST_DEBUG
	{
	  ruli_uint8_t cname_owner_txt[RULI_LIMIT_DNAME_TEXT_BUFSZ];
	  int          cname_owner_txt_len;
	  char         cname_trg_txt[RULI_LIMIT_DNAME_TEXT_BUFSZ];
	  int          cname_trg_txt_len;
	  int          result;
	  
	  assert(sizeof(ruli_uint8_t) == sizeof(char));
	  
	  result = ruli_dname_extract((const ruli_uint8_t *) qry->answer_buf,
				     (const ruli_uint8_t *) qry->answer_buf + qry->answer_msg_len,
				     cname_owner_txt, 
				     cname_owner_txt + RULI_LIMIT_DNAME_TEXT_BUFSZ,
				     rr->owner, &cname_owner_txt_len); 
	  assert(!result);
	  
	  result = ruli_dname_decode(cname_trg_txt, RULI_LIMIT_DNAME_TEXT_BUFSZ, 
				     &cname_trg_txt_len, 
				     (const char *) cname_rdata.cname, 
				     cname_rdata.cname_len);
	  assert(!result);
	  
	  fprintf(stderr, 
		  "DEBUG: %s: %s(): %d: "
		  "CNAME owner=(%d)%s target=(%d)%s remaining=%d\n", 
		  __FILE__, __PRETTY_FUNCTION__, __LINE__,
		  cname_owner_txt_len, cname_owner_txt,
		  cname_trg_txt_len, cname_trg_txt,
		  remaining_cname_depth);
	}
#endif
	
	wanted_owner     = cname_rdata.cname;
	wanted_owner_len = cname_rdata.cname_len;
	
	cname_found = 1; /* true */
	
	break;

      } /* for: IN CNAME scan of answer section */

      /* If no CNAME found, give up */
      if (!cname_found)
	break;

      /* If CNAME found, check depth */
      if (--remaining_cname_depth < 0) {
	ruli_parse_delete(&parse);
	return query_done(host_qry, RULI_HOST_CODE_CNAME);
      }

      /* Loop to find address for CNAME target */

    } /* for: Address search */    

    ruli_parse_delete(&parse);
    
  } /* parsed answer context */
  
  if (_ruli_need_query_in_a(qry->q_type, host_qry->host_options)) {
    qry->q_type = RULI_RR_TYPE_A;
    return query_resubmit(host_qry);
  }

  if (ruli_list_size(&host_qry->answer_addr_list))
    return query_done(host_qry, RULI_HOST_CODE_OK);

  return query_done(host_qry, RULI_HOST_CODE_EMPTY);
}

int ruli_host_query_submit(ruli_host_t *host_qry)
{
  ruli_res_query_t *query = &host_qry->host_query;

  /*
   * Initialize query
   */
  if (ruli_list_new(&host_qry->answer_addr_list))
    return RULI_HOST_SUBMIT_OTHER;
  host_qry->answer_code = RULI_HOST_CODE_VOID;

  /*
   * Define callback parameters
   */
  query->q_on_answer     = on_host_answer;
  query->q_on_answer_arg = host_qry;

  /*
   * Pass on arguments to underlying query
   */
  query->q_domain     = host_qry->host_domain;
  query->q_domain_len = host_qry->host_domain_len;
  query->q_class      = RULI_RR_CLASS_IN;
  query->q_type       = _ruli_reset_qtype(host_qry->host_options);
  query->q_options    = host_qry->host_options;

  assert(host_qry->host_max_cname_depth >= 0);
  assert(host_qry->host_max_cname_depth <= RULI_LIMIT_CNAME_DEPTH);

  /*
   * Submit underlying query
   */
  if (ruli_res_query_submit(host_qry->host_resolver, query)) {
    ruli_list_delete(&host_qry->answer_addr_list);
    return RULI_HOST_SUBMIT_OTHER;
  }

  return RULI_HOST_SUBMIT_OK;
}

/*
  This is an ugly hack: it detaches the addresses
  from the ruli_list_t container, so that the
  later call to ruli_host_query_delete won't
  dispose those addresses.
 */
void ruli_host_query_detach(ruli_host_t *host_qry)
{
  ruli_list_prune(&host_qry->answer_addr_list, 0);
}

void ruli_host_query_delete(ruli_host_t *host_qry)
{
  ruli_list_dispose_trivial(&host_qry->answer_addr_list);
  ruli_res_query_delete(&host_qry->host_query);
}


