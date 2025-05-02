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
  $Id: ruli_msg.h,v 1.5 2003/02/10 02:50:18 evertonm Exp $
  */


#ifndef RULI_MSG_H
#define RULI_MSG_H


#include <ruli_util.h>
#include <ruli_limits.h>


enum {
  RULI_MSG_OK          = 0,
  RULI_MSG_SHORT_BUF,
  RULI_MSG_PARSE_SHORT
};

enum {
  RULI_MSG_OFFSET_QR     = 15, /* 1 bit  */
  RULI_MSG_OFFSET_OPCODE = 11, /* 4 bits */
  RULI_MSG_OFFSET_AA     = 10, /* 1 bit  */
  RULI_MSG_OFFSET_TC     = 9,  /* 1 bit  */
  RULI_MSG_OFFSET_RD     = 8,  /* 1 bit  */
  RULI_MSG_OFFSET_RA     = 7,  /* 1 bit  */
  RULI_MSG_OFFSET_Z      = 4,  /* 1 bit  */
  RULI_MSG_OFFSET_RCODE  = 0   /* 4 bits */
};

enum {
  RULI_MSG_OFFMASK_QR     = 1, /* 1 bit  */
  RULI_MSG_OFFMASK_OPCODE = 7, /* 4 bits */
  RULI_MSG_OFFMASK_AA     = 1, /* 1 bit  */
  RULI_MSG_OFFMASK_TC     = 1, /* 1 bit  */
  RULI_MSG_OFFMASK_RD     = 1, /* 1 bit  */
  RULI_MSG_OFFMASK_RA     = 1, /* 1 bit  */
  RULI_MSG_OFFMASK_Z      = 1, /* 1 bit  */
  RULI_MSG_OFFMASK_RCODE  = 7  /* 4 bits */
};

enum {
  RULI_MSG_MASK_QR     = RULI_MSG_OFFMASK_QR     << RULI_MSG_OFFSET_QR,
  RULI_MSG_MASK_OPCODE = RULI_MSG_OFFMASK_OPCODE << RULI_MSG_OFFSET_OPCODE,
  RULI_MSG_MASK_AA     = RULI_MSG_OFFMASK_AA     << RULI_MSG_OFFSET_AA,
  RULI_MSG_MASK_TC     = RULI_MSG_OFFMASK_TC     << RULI_MSG_OFFSET_TC,
  RULI_MSG_MASK_RD     = RULI_MSG_OFFMASK_RD     << RULI_MSG_OFFSET_RD,
  RULI_MSG_MASK_RA     = RULI_MSG_OFFMASK_RA     << RULI_MSG_OFFSET_RA,
  RULI_MSG_MASK_Z      = RULI_MSG_OFFMASK_Z      << RULI_MSG_OFFSET_Z,
  RULI_MSG_MASK_RCODE  = RULI_MSG_OFFMASK_RCODE  << RULI_MSG_OFFSET_RCODE
};

enum {
  RULI_OPCODE_QUERY  = 0, /* standard query */
  RULI_OPCODE_IQUERY = 1, /* obsolete */
  RULI_OPCODE_STATUS = 2  /* server status request */
};

typedef struct {
  ruli_uint16_t id;
  ruli_uint16_t flags;
  ruli_uint8_t  rcode;
  ruli_uint16_t qdcount;
  ruli_uint16_t ancount;
  ruli_uint16_t nscount;
  ruli_uint16_t arcount;
} ruli_msg_header_t;


int ruli_msg_build(ruli_uint8_t *buf, int buf_size,
		   int *msg_len, ruli_uint16_t query_id, 
		   const ruli_uint8_t *dname, int dname_len,
		   ruli_uint16_t qclass, ruli_uint16_t qtype);

int ruli_msg_parse_header(ruli_msg_header_t *msg_hdr, 
			  const ruli_uint8_t *msg, size_t msg_len);


#endif /* RULI_MSG_H */

