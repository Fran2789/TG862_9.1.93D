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
  $Id: ruli_host.h,v 1.3 2004/06/28 19:19:29 evertonm Exp $
  */


#ifndef RULI_HOST_H
#define RULI_HOST_H


#include <ruli_res.h>


/* Return codes for ruli_host_query_submit() */
enum {
  RULI_HOST_SUBMIT_OK = 0, /* successful */
  RULI_HOST_SUBMIT_OTHER   /* could not sumit query */
};

/* Answer codes for ruli_host_t.answer_code */
enum {
  RULI_HOST_CODE_VOID = -1, /* no result yet */
  RULI_HOST_CODE_OK   = 0,  /* good answer */
  RULI_HOST_CODE_ALARM,     /* underlying query timeout */
  RULI_HOST_CODE_QUERY,     /* underlying query unspecified error */
  RULI_HOST_CODE_PARSE,     /* underlying query parsing failed */
  RULI_HOST_CODE_EMPTY,     /* underlying query rcode==NOERROR, no records */
  RULI_HOST_CODE_RCODE,     /* underlying query rcode!=NOERROR, check rcode */
  RULI_HOST_CODE_CNAME,     /* underlying query cname chain too deep */
  RULI_HOST_CODE_OTHER      /* unspecified error, resources exhausted */
};


struct ruli_host_t;
typedef struct ruli_host_t ruli_host_t;


/* User callback function type */
typedef void *(*ruli_host_call_t)(ruli_host_t *host_qry, void *host_qry_arg);


/* Query opaque context */
struct ruli_host_t {
  /*
   * private members
   */
  ruli_res_query_t host_query;  /* underlying query context */

  /*
   * public members
   */

  /* input */
  ruli_res_t       *host_resolver;       /* resolver context */
  ruli_host_call_t host_on_answer;       /* user callback function */
  void             *host_on_answer_arg;  /* user callback argument */
  char             *host_domain;         /* encoded domain name */
  int              host_domain_len;      /* length of above */
  long             host_options;         /* query options */
  int              host_max_cname_depth; /* max cname chain depth */

  /* output */
  int              answer_code;          /* result code */
  ruli_list_t      answer_addr_list;     /* list of ruli_addr_t* */
};


int ruli_host_query_submit(ruli_host_t *host_qry);
void ruli_host_query_detach(ruli_host_t *host_qry);
void ruli_host_query_delete(ruli_host_t *host_qry);
const char *ruli_host_errstr(int result);
int ruli_host_answer_code(ruli_host_t *host_qry);
int ruli_host_rcode(ruli_host_t *host_qry);


#endif /* RULI_HOST_H */

