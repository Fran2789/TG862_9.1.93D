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
  $Id: ruli_parse.h,v 1.13 2004/06/06 04:27:48 evertonm Exp $
  */


#ifndef RULI_PARSE_H
#define RULI_PARSE_H


#include <netinet/in.h>

#include <ruli_util.h>
#include <ruli_list.h>
#include <ruli_msg.h>
#include <ruli_addr.h>


enum {
  RULI_PARSE_OK        = 0,
  RULI_PARSE_SHORT_MSG,
  RULI_PARSE_LONG_MSG,
  RULI_PARSE_QUESTION,
  RULI_PARSE_ANSWER,
  RULI_PARSE_AUTHORITY,
  RULI_PARSE_ADDITIONAL,
  RULI_PARSE_LIST
};

enum {
  RULI_PARSE_RR_OK   = 0,
  RULI_PARSE_RR_FAIL
};


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

typedef struct {
  /* 
   * References owner in compressed format 
   */
  const ruli_uint8_t *owner;
  int                owner_len;

  ruli_uint16_t      type;
  ruli_uint16_t      qclass; /* class is a C++ keyword */
  ruli_uint32_t      ttl;
  ruli_uint32_t      rdlength;
  const ruli_uint8_t *rdata;
} ruli_rr_t;

typedef struct {
  ruli_uint16_t      priority;
  ruli_uint16_t      weight;
  ruli_uint16_t      port;
  const ruli_uint8_t *target;
  int                target_len;
} ruli_srv_rdata_t;

typedef struct {
  ruli_uint8_t cname[RULI_LIMIT_DNAME_ENCODED]; /* encoded, uncompressed */
  int          cname_len;
} ruli_cname_rdata_t;

typedef struct {
  ruli_uint16_t preference;
  ruli_uint8_t  target[RULI_LIMIT_DNAME_ENCODED]; /* encoded, uncompressed */
  int           target_len;
} ruli_mx_rdata_t;

typedef struct {
  ruli_srv_rdata_t   srv_rdata;
  ruli_uint16_t      type;
  ruli_uint16_t      qclass; /* class is a C++ keyword */
  ruli_uint32_t      ttl;
} ti_srv_rr_t;

typedef struct {
  /*
   * input
   */
  ruli_uint16_t qdcount;
  ruli_uint16_t ancount;
  ruli_uint16_t nscount;
  ruli_uint16_t arcount;

  /*
   * output: lists of ruli_rr_t*
   */
  ruli_list_t question_list;
  ruli_list_t answer_list;
  ruli_list_t authority_list;
  ruli_list_t additional_list;
} ruli_parse_t;


int ruli_parse_new(ruli_parse_t *parse);
int ruli_parse_message(ruli_parse_t *parse, ruli_msg_header_t *msg_hdr,
		       const ruli_uint8_t *msg, int msg_len);
void ruli_parse_delete(ruli_parse_t *parse);

int ruli_parse_rr_a(struct in_addr *addr,
		    const ruli_uint8_t *rdata, ruli_uint16_t rdlength);

int ruli_parse_rr_aaaa(struct in6_addr *addr,
		       const ruli_uint8_t *rdata, ruli_uint16_t rdlength);

int ruli_parse_rr_srv(ruli_srv_rdata_t *srv_rdata,
		      const ruli_uint8_t *rdata, ruli_uint16_t rdlength);

int ruli_parse_rr_cname(ruli_cname_rdata_t *cname_rdata,
			const ruli_uint8_t *rdata, ruli_uint16_t rdlength,
			const ruli_uint8_t *msg, size_t msg_len);

int ruli_parse_rr_mx(ruli_mx_rdata_t *mx_rdata,
		     const ruli_uint8_t *rdata, ruli_uint16_t rdlength,
		     const ruli_uint8_t *msg, size_t msg_len);

void ruli_parse_addr_rr(ruli_addr_t *addr, const ruli_rr_t *rr, long options);

#endif /* RULI_PARSE_H */
