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
  $Id: ruli_srv.h,v 1.38 2004/06/25 23:19:11 evertonm Exp $
  */


#ifndef RULI_SRV_H
#define RULI_SRV_H


#include <netinet/in.h>

#include <ruli_res.h>
#include <ruli_parse.h>
#include <ruli_host.h>


/* Return codes for ruli_srv_query_submit() */
enum {
  RULI_SRV_OK         = 0,
  RULI_SRV_QRY_SUBMIT,
  RULI_SRV_MALLOC,
  RULI_SRV_CONCAT
};

/* Values for ruli_srv_t.answer_code */
enum {
  RULI_SRV_CODE_VOID         = -1, /* no result yet */
  RULI_SRV_CODE_OK           = 0,  /* successful */
  RULI_SRV_CODE_UNAVAILABLE,       /* service surely not avaible in domain */
  RULI_SRV_CODE_ALARM,             /* timeout */
  RULI_SRV_CODE_QUERY_FAILED,      /* query failed */
  RULI_SRV_CODE_PARSE_FAILED,      /* parse error */
  RULI_SRV_CODE_MALLOC,            /* resources */
  RULI_SRV_CODE_LIST,              /* resources */

  RULI_SRV_CODE_WALK_QUERY, /* walk query failed */
  RULI_SRV_CODE_WALK_PARSE, /* walk parse failed */
  RULI_SRV_CODE_WALK_ALARM, /* walk timeout */
  RULI_SRV_CODE_WALK_EMPTY, /* walk rcode == NOERROR, but no records */
  RULI_SRV_CODE_WALK_RCODE, /* walk rcode != NOERROR, check rcode */
  RULI_SRV_CODE_WALK_OTHER, /* other error, probably resources exhausted */

  RULI_SRV_CODE_FALL_QUERY, /* fall query failed */
  RULI_SRV_CODE_FALL_PARSE, /* fall parse failed */
  RULI_SRV_CODE_FALL_ALARM, /* fall timeout */
  RULI_SRV_CODE_FALL_EMPTY, /* fall rcode == NOERROR, but no records */
  RULI_SRV_CODE_FALL_RCODE, /* fall rcode != NOERROR, check rcode */
  RULI_SRV_CODE_FALL_CNAME, /* cname chain too deep */
  RULI_SRV_CODE_FALL_OTHER  /* other error, probably resources exhausted */
};

typedef enum {
  RULI_SRV_RCODE_NONE, /* no RCODE available */
  RULI_SRV_RCODE_SRV,  /* RCODE returned for SRV query */
  RULI_SRV_RCODE_WALK, /* RCODE returned for SRV target query */
  RULI_SRV_RCODE_FALL  /* RCODE returned for IN A fallback query */
} ruli_srv_rcode_kind_t;


typedef struct {
  char        target[RULI_LIMIT_DNAME_ENCODED]; /* encoded, uncompressed */
  int         target_len;
  int         priority;
  int         weight;
  int         port;
  ruli_list_t addr_list; /* list of *ruli_addr_t */
  ruli_uint16_t      type;    /* Query type - A/SRV */
  ruli_uint16_t      qclass;  /* Query class - IN */
  ruli_uint32_t      ttl;     /* Time to live */
} ruli_srv_entry_t;


struct ruli_srv_t;
typedef struct ruli_srv_t ruli_srv_t;


/* User callback function type */
typedef void *(*ruli_srv_call_t)(ruli_srv_t *srv_qry, void *srv_qry_arg);


/* Query opaque context */
struct ruli_srv_t {
  /*
   * private members
   */
  ruli_res_query_t query;          /* underlying query context */
  ruli_parse_t     parse;          /* store result of answer parsing */

  char             *qdomain;       /* srv_service + srv_domain */
  int              qdomain_len;    /* length of qdomain */

  ruli_list_t      rr_srv_list;    /* list of *ruli_srv_rdata_t */
  ruli_list_t      pri_srv_list;   /* priority ordered list of 
                                      *ruli_srv_rdata_t (only references) */
  ruli_list_t      wei_srv_list;   /* random weight ordered list of
				      *ruli_srv_rdata_t (only references) */

  /* auxiliary context for underlying queries */
  union {
    int            walk_index;     /* index for answer_srv_list */
  } under;

  int              last_rcode;     /* saved rcode for underlying query */

  void *(*fallback)(ruli_srv_t *self); /* fallback function */

  

  /*
   * public members
   */

  /* input */
  ruli_res_t       *srv_resolver;      /* resolver context */

  ruli_srv_call_t  srv_on_answer;      /* user callback function */
  void             *srv_on_answer_arg; /* user callback argument */

  char             *srv_service;       /* encoded _service._protocol part */
  int              srv_service_len;    /* length of above */

  char             *srv_domain;        /* encoded domain name */
  int              srv_domain_len;     /* length of above */

  int              srv_fallback_port;  /* fallback port */
  long             srv_options;        /* query options */

  /* output */
  int              answer_code;
  ruli_list_t      answer_srv_list;    /* list of *ruli_srv_entry_t */
};


const char *ruli_srv_errstr(int result);

int ruli_srv_query_submit(ruli_srv_t *srv_qry);
void ruli_srv_query_delete(ruli_srv_t *srv_qry);

ruli_srv_rcode_kind_t ruli_srv_rcode_kind(ruli_srv_t *srv_qry);
int ruli_srv_rcode(ruli_srv_t *srv_qry);


void *_ruli_srv_query_done(ruli_srv_t *srv_qry, int srv_result_code);
int _ruli_srv_query_submit(ruli_srv_t *srv_qry, 
			   void *(*fallback_call)(ruli_srv_t *));
void *_ruli_srv_answer_fallback_addr(ruli_srv_t *srv_qry);
void *_ruli_srv_answer_walk(ruli_srv_t *srv_qry);
ruli_srv_entry_t *_ruli_srv_list_new_entry(ruli_list_t *srv_list,
					   const char *target,
					   int target_len,
					   int priority,
					   int weight,
					   int port,
                       int ttl,
                       int qclass,
                       int type);


#endif /* RULI_SRV_H */
