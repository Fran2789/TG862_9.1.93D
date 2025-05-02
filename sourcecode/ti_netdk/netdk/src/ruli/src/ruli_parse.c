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
  $Id: ruli_parse.c,v 1.20 2005/06/07 22:17:51 evertonm Exp $
  */


#include <stdio.h>      /* FIXME: remove me [fprintf() debug] */

#include <assert.h>
#include <string.h>
#include <sys/socket.h>

#include <ruli_parse.h>
#include <ruli_mem.h>
#include <ruli_txt.h>
#include <ruli_res.h>


int ruli_parse_new(ruli_parse_t *parse)
{
  if (ruli_list_new(&parse->question_list))
    return RULI_PARSE_LIST;

  if (ruli_list_new(&parse->answer_list)) {
    ruli_list_delete(&parse->question_list);
    return RULI_PARSE_LIST;
  }

  if (ruli_list_new(&parse->authority_list)) {
    ruli_list_delete(&parse->question_list);
    ruli_list_delete(&parse->answer_list);
    return RULI_PARSE_LIST;
  }

  if (ruli_list_new(&parse->additional_list)) {
    ruli_list_delete(&parse->question_list);
    ruli_list_delete(&parse->answer_list);
    ruli_list_delete(&parse->authority_list);
    return RULI_PARSE_LIST;
  }

  return RULI_PARSE_OK;
}

static const ruli_uint8_t *skip_rr_owner(const ruli_uint8_t *msg, 
	         		         const ruli_uint8_t *past_end)
{
  int len;

  /*
   * Iterate over labels
   */
  for (;;) {
    if (msg >= past_end)
      return 0;

    len = *msg;

    /* Last label? */
    if (!len) {
      const ruli_uint8_t *next = msg + 1;

      if (next > past_end)
	return 0;

      return next;
    }

    /* Name compression? */
    if ((len & 0xC0) == 0xC0) {
      const ruli_uint8_t *next = msg + 2;

      if (next > past_end)
	return 0;

      return next;
    }

    msg += len + 1;
  }

  /*
   * NOT REACHED
   */
  assert(0);

  return 0;
}

typedef const ruli_uint8_t *(*rr_parser_t)(ruli_rr_t *rr,
				           const ruli_uint8_t *msg,
				           const ruli_uint8_t *past_end);

static const ruli_uint8_t *parse_question(ruli_rr_t *rr,
				          const ruli_uint8_t *msg,
				          const ruli_uint8_t *past_end)
{
  const ruli_uint8_t *m;

  assert(msg <= past_end);

  /*
   * Skip owner
   */

  m = skip_rr_owner(msg, past_end);
  if (!m)    
    return 0;

  assert(msg < m);
  assert(m < past_end);

  /* 
     offset data     size
     ------------------------
     0      type     2
     2      class    2

     4      next
   */

  {
    const ruli_uint8_t *next = m + 4;
    if (next > past_end)
      return 0;
    
    rr->owner     = msg;
    rr->owner_len = m - msg;
    rr->type      = ruli_pack2(m);
    rr->qclass    = ruli_pack2(m + 2);
    
    rr->ttl       = 0;
    rr->rdlength  = 0;
    rr->rdata     = 0;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: parse_question(): owner_len=%d type=%d class=%d\n",
	  rr->owner_len, rr->type, rr->qclass);
#endif
    
    return next;
  }
}

static const ruli_uint8_t *parse_rr(ruli_rr_t *rr,
	    		            const ruli_uint8_t *msg,
			            const ruli_uint8_t *past_end)
{
  const ruli_uint8_t  *m;
  const ruli_uint8_t  *rdata;
  const ruli_uint8_t  *next;
  ruli_uint16_t       rdlength;

  assert(msg <= past_end);

  /*
   * Skip owner
   */

  m = skip_rr_owner(msg, past_end);
  if (!m)    
    return 0;

  assert(msg < m);
  assert(m < past_end);

  /* 
     offset data     size
     ------------------------
     0      type     2
     2      class    2
     4      ttl      4
     8      rdlength 2
     10     rdata    rdlength

     10 + rdlength
   */

  rdlength = ruli_pack2(m + 8);
  rdata    = m + 10;

  /* RR data must fit */
  next = rdata + rdlength;
  if (next > past_end)
    return 0;

  rr->owner     = msg;
  rr->owner_len = m - msg;
  rr->type      = ruli_pack2(m);
  rr->qclass    = ruli_pack2(m + 2);
  rr->ttl       = ruli_pack4(m + 4);
  rr->rdlength  = rdlength;
  rr->rdata     = rdata;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: parse_rr(): owner_len=%d type=%d class=%d ttl=%u "
	  "rdlength=%d\n",
	  rr->owner_len, rr->type, rr->qclass, rr->ttl, rdlength);
#endif

  return next;
}

static const ruli_uint8_t *parse_section(rr_parser_t rr_parser,
	     			         ruli_list_t *rr_list,
				         const ruli_uint8_t *msg, 
				         const ruli_uint8_t *past_end,
				         int rr_count)
{
  int                i;
  const ruli_uint8_t *m;

  assert(!ruli_list_size(rr_list));
  assert(msg <= past_end);

  m = msg;

  /*
   * Scan msg for resource records
   */
  for (i = 0; i < rr_count; ++i) {
    const ruli_uint8_t *p;
    ruli_rr_t          *rr;

    /* Allocate space for RR */
    rr = (ruli_rr_t *) ruli_malloc(sizeof(ruli_rr_t));
    if (!rr)
      return 0;

    /* Effectively parse RR */
    p = rr_parser(rr, m, past_end);
    if (!p) {
      ruli_free(rr);
      return 0;
    }

    assert(m < p);         /* We MUST have found at least one RR */
    assert(p <= past_end);

    /* Save reference for RR */
    if (ruli_list_push(rr_list, rr)) {
      ruli_free(rr);
      return 0;
    }

    m = p;
  }

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: parse_section(): scanned_octets=%d RRs_found=%d\n",
	  m - msg, ruli_list_size(rr_list));
#endif

  return m;
}

int ruli_parse_message(ruli_parse_t *parse, ruli_msg_header_t *msg_hdr, 
		       const ruli_uint8_t *msg, int msg_len)
{
  const ruli_uint8_t *i;
  const ruli_uint8_t *j;
  const ruli_uint8_t *past_end;

  assert(!ruli_list_size(&parse->question_list));
  assert(!ruli_list_size(&parse->answer_list));
  assert(!ruli_list_size(&parse->authority_list));
  assert(!ruli_list_size(&parse->additional_list));

  parse->qdcount = msg_hdr->qdcount;
  parse->ancount = msg_hdr->ancount;
  parse->nscount = msg_hdr->nscount;
  parse->arcount = msg_hdr->arcount;

  /* Message too short? */
  if (msg_len < RULI_LIMIT_MSG_HEADER)
    return RULI_PARSE_SHORT_MSG;

  /* Skip message header */
  i = msg + RULI_LIMIT_MSG_HEADER;

  past_end = msg + msg_len;

  /*
   * Parse question section
   */
  j = parse_section(parse_question, 
		    &parse->question_list, i, past_end, parse->qdcount);
  if (!j)
    return RULI_PARSE_QUESTION;

  assert(i <= j);
  assert(j <= past_end);
  assert(ruli_list_size(&parse->question_list) == parse->qdcount);

  /*
   * Parse answer section
   */
  i = parse_section(parse_rr,
		    &parse->answer_list, j, past_end, parse->ancount);
  if (!i)
    return RULI_PARSE_ANSWER;

  assert(j <= i);
  assert(i <= past_end);
  assert(ruli_list_size(&parse->answer_list) == parse->ancount);

  /*
   * Parse authority section
   */
  j = parse_section(parse_rr,
		    &parse->authority_list, i, past_end, parse->nscount);
  if (!j)
    return RULI_PARSE_AUTHORITY;

  assert(i <= j);
  assert(j <= past_end);
  assert(ruli_list_size(&parse->authority_list) == parse->nscount);

  /*
   * Parse additional section
   */
  i = parse_section(parse_rr,
		    &parse->additional_list, j, past_end, parse->arcount);
  if (!i)
    return RULI_PARSE_ADDITIONAL;

  assert(j <= i);
  assert(i <= past_end);
  assert(ruli_list_size(&parse->additional_list) == parse->arcount);

  if (i < past_end)
    return RULI_PARSE_LONG_MSG;

  return RULI_PARSE_OK;
}

void ruli_parse_delete(ruli_parse_t *parse)
{
  /*
   * Free all ruli_rr_t structs, if any.
   */

  ruli_list_dispose_trivial(&parse->question_list);
  ruli_list_dispose_trivial(&parse->answer_list);
  ruli_list_dispose_trivial(&parse->authority_list);
  ruli_list_dispose_trivial(&parse->additional_list);
}

int ruli_parse_rr_a(struct in_addr *addr,
		    const ruli_uint8_t *rdata, ruli_uint16_t rdlength)
{
  int in_addr_size = sizeof(*addr);

  if (rdlength != in_addr_size)
    return RULI_PARSE_RR_FAIL;

  memcpy(addr, rdata, rdlength);

  return RULI_PARSE_RR_OK;
}

int ruli_parse_rr_aaaa(struct in6_addr *addr,
		       const ruli_uint8_t *rdata, ruli_uint16_t rdlength)
{
  int in_addr_size = sizeof(*addr);

  if (rdlength != in_addr_size)
    return RULI_PARSE_RR_FAIL;

  memcpy(addr, rdata, rdlength);

  return RULI_PARSE_RR_OK;
}

int ruli_parse_rr_srv(ruli_srv_rdata_t *srv_rdata,
		      const ruli_uint8_t *rdata, ruli_uint16_t rdlength)
{
  const ruli_uint8_t *i;
  const ruli_uint8_t *past_end = rdata + rdlength;

  /* 
     offset data     size
     ------------------------
     0      priority 2
     2      weight   2
     4      port     2
     6      target   1..255
  */

  if (rdlength < 7)
    return RULI_PARSE_RR_FAIL;

  if (rdlength > 261)
    return RULI_PARSE_RR_FAIL;

  srv_rdata->priority = ruli_pack2(rdata);
  srv_rdata->weight   = ruli_pack2(rdata + 2);
  srv_rdata->port     = ruli_pack2(rdata + 4);

  {
    const ruli_uint8_t *trg = rdata + 6;

    i = skip_rr_owner(trg, past_end);
    if (i != past_end)
      return RULI_PARSE_RR_FAIL;
    
    srv_rdata->target     = trg;
    srv_rdata->target_len = past_end - trg;
  }

  assert(srv_rdata->target_len <= RULI_LIMIT_DNAME_ENCODED);

  return RULI_PARSE_RR_OK;
}

int ruli_parse_rr_cname(ruli_cname_rdata_t *cname_rdata,
			const ruli_uint8_t *rdata, ruli_uint16_t rdlength,
			const ruli_uint8_t *msg, size_t msg_len)
{
  size_t len;

  if (ruli_dname_expand(msg, msg + msg_len, cname_rdata->cname, 
			cname_rdata->cname + RULI_LIMIT_DNAME_ENCODED,
			rdata, &len))
    return RULI_PARSE_RR_FAIL;

  assert(len > 0);
  assert(len <= RULI_LIMIT_DNAME_ENCODED);

  cname_rdata->cname_len = len;

  return RULI_PARSE_RR_OK;
}

int ruli_parse_rr_mx(ruli_mx_rdata_t *mx_rdata,
		     const ruli_uint8_t *rdata, ruli_uint16_t rdlength,
		     const ruli_uint8_t *msg, size_t msg_len)
{
  size_t len;

  /* 
     offset data       size
     ------------------------
     0      preference 2
     2      target     1..255
  */

  if (rdlength < 3)
    return RULI_PARSE_RR_FAIL;

  if (rdlength > 257)
    return RULI_PARSE_RR_FAIL;

  mx_rdata->preference = ruli_pack2(rdata);

  if (ruli_dname_expand(msg, msg + msg_len, mx_rdata->target, 
			mx_rdata->target + RULI_LIMIT_DNAME_ENCODED,
			rdata + 2, &len))
    return RULI_PARSE_RR_FAIL;

  assert(len > 0);
  assert(len <= RULI_LIMIT_DNAME_ENCODED);

  mx_rdata->target_len = len;

  return RULI_PARSE_RR_OK;
}
		     
void ruli_parse_addr_rr(ruli_addr_t *addr, const ruli_rr_t *rr, long options)
{
  assert(ruli_rr_type_is_address(options, rr->type));

  switch (rr->type) {
  case RULI_RR_TYPE_A:
    {
      int result = ruli_parse_rr_a(&addr->addr.ipv4, 
				   rr->rdata, rr->rdlength);
      assert(!result); /* IN A parsing can't fail */
    }
    ruli_addr_init(addr, PF_INET);
    break;
    
  case RULI_RR_TYPE_AAAA:
    {
      int result = ruli_parse_rr_aaaa(&addr->addr.ipv6, 
				      rr->rdata, rr->rdlength);
      assert(!result); /* IN AAAA parsing can't fail */
    }
    ruli_addr_init(addr, PF_INET6);
    break;
    
  default:
    /* Previous ruli_rr_type_is_address() call ensures
     * RR type is either IN_A or IN_AAAA, thus we can
     * panic otherwise.
     */
    assert(0);
  }

#ifdef RULI_RES_DEBUG
  fprintf(stderr, "DEBUG: ruli_parse_addr_rr(): addr=");
  ruli_addr_print(stderr, addr);
  fprintf(stderr, "\n");
#endif
}

