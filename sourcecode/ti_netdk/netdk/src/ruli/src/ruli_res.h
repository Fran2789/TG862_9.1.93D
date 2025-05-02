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
  $Id: ruli_res.h,v 1.27 2005/06/28 22:27:02 evertonm Exp $
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

#ifndef RULI_RES_H
#define RULI_RES_H


#include <ruli_oop.h>
#include <ruli_list.h>
#include <ruli_msg.h>
#include <ruli_rand.h>
#include <ruli_addr.h>


enum {
  RULI_RES_QBUF_SIZE = RULI_LIMIT_MSG_HIGH + 2 /* largest query + TCP prefix */
};

enum {
  RULI_RES_OK = 0,
  RULI_RES_LIST,
  RULI_RES_SOCKET,
  RULI_RES_MSG_BUILD,
  RULI_RES_MALLOC,
  RULI_RES_NS_LIST
};

enum {
  RULI_CODE_VOID    = -1,       /* no result yet */
  RULI_CODE_OK      = 0,        /* successful, check rcode */
  RULI_CODE_TIMEOUT,            /* timed out */
  RULI_CODE_MALLOC,             /* malloc failed */
  RULI_CODE_SEND,               /* could not send */
  RULI_CODE_CONNECT,            /* could not connect */
  RULI_CODE_LAUNCH_SEARCH_LIST, /* failure restarting query with search list */
  RULI_CODE_CONCAT_SEARCH_LIST, /* failure concat'ng query with search list */
  RULI_CODE_ANSWER_OPCODE,      /* bad answer opcode */
  RULI_CODE_ANSWER_FLAG_QR,     /* bad answer flag */
  RULI_CODE_ANSWER_FLAG_RD,     /* bad answer flag */
  RULI_CODE_ANSWER_FLAG_RA,     /* bad answer flag */
  RULI_CODE_ANSWER_FLAG_Z,      /* bad answer flag */
  RULI_CODE_ANSWER_TRUNC        /* TC bit in TCP answer */
};

enum {
  RULI_RCODE_VOID           = -1, /* RULI's internals only */

  RULI_RCODE_NOERROR        = 0,  /* No error condition */
  RULI_RCODE_FORMATERROR    = 1,  /* Format error */
  RULI_RCODE_SERVERFAILURE  = 2,  /* Server failure */
  RULI_RCODE_NAMEERROR      = 3,  /* Name error */
  RULI_RCODE_NOTIMPLEMENTED = 4,  /* Not implemented */
  RULI_RCODE_REFUSED        = 5   /* Refused */
};

enum {
  RULI_RR_TYPE_A     = 1,   /* 1 a host address */
  RULI_RR_TYPE_NS    = 2,   /* 2 an authoritative name server */
  RULI_RR_TYPE_MD    = 3,   /* 3 a mail destination (Obsolete - use MX) */
  RULI_RR_TYPE_MF    = 4,   /* 4 a mail forwarder (Obsolete - use MX) */
  RULI_RR_TYPE_CNAME = 5,   /* 5 the canonical name for an alias */
  RULI_RR_TYPE_SOA   = 6,   /* 6 marks the start of a zone of authority */
  RULI_RR_TYPE_MB    = 7,   /* 7 a mailbox domain name (EXPERIMENTAL) */
  RULI_RR_TYPE_MG    = 8,   /* 8 a mail group member (EXPERIMENTAL) */
  RULI_RR_TYPE_MR    = 9,   /* 9 a mail rename domain name (EXPERIMENTAL) */
  RULI_RR_TYPE_NULL  = 10,  /* 10 a null RR (EXPERIMENTAL) */
  RULI_RR_TYPE_WKS   = 11,  /* 11 a well known service description */
  RULI_RR_TYPE_PTR   = 12,  /* 12 a domain name pointer */
  RULI_RR_TYPE_HINFO = 13,  /* 13 host information */
  RULI_RR_TYPE_MINFO = 14,  /* 14 mailbox or mail list information */
  RULI_RR_TYPE_MX    = 15,  /* 15 mail exchange */
  RULI_RR_TYPE_TXT   = 16,  /* 16 text strings */
  RULI_RR_TYPE_AAAA  = 28,  /* 28 IPv6 address (RFC 3596) */
  RULI_RR_TYPE_SRV   = 33,  /* 33 location of service */
  RULI_RR_TYPE_AXFR  = 252, /* 252 a request for a transfer of an entire zone */
  RULI_RR_TYPE_MAILB = 253, /* 253 a request for mailbox-related records (MB, MG or MR) */
  RULI_RR_TYPE_MAILA = 254, /* 254 a request for mail agent RRs (Obsolete - see MX) */
  RULI_RR_TYPE_ANY   = 255  /* 255 a request for all records */
};

/*
 * From RFC 1035
 */
enum {
  RULI_RR_CLASS_IN  = 1,  /* 1 the Internet */
  RULI_RR_CLASS_CS  = 2,  /* 2 the CSNET class (Obsolete) */
  RULI_RR_CLASS_CH  = 3,  /* 3 the CHAOS class */
  RULI_RR_CLASS_HS  = 4,  /* 4 Hesiod (Dyer 87) */
  RULI_RR_CLASS_ANY = 255 /* 255 any class */
};


/*
 * Query options
 */
enum {
  RULI_RES_OPT_VOID         = 0,
  RULI_RES_OPT_SEARCH       = 1 << 0, /* Use search list */
  RULI_RES_OPT_NEED_RA      = 1 << 1, /* Require RA bit in answer */
  RULI_RES_OPT_SRV_NOINET   = 1 << 2, /* Don't search IPv4 addresses */
  RULI_RES_OPT_SRV_NOINET6  = 1 << 3, /* Don't search IPv6 addresses */
  RULI_RES_OPT_SRV_URI_PORT = 1 << 4, /* Assume port was forced in URI */
  RULI_RES_OPT_SRV_NOWALK   = 1 << 5, /* Don't search missing addresses */
  RULI_RES_OPT_SRV_NOFALL   = 1 << 6, /* Don't launch fallback queries */
  RULI_RES_OPT_SRV_NOSORT6  = 1 << 7, /* Don't sort favorably to v6 addrs */
  RULI_RES_OPT_SRV_RFC3484  = 1 << 8, /* Apply RFC3484 destination address
                                         selection rules */
  RULI_RES_OPT_SRV_CNAME    = 1 << 9,  /* Allow CNAME in SRV targets */
  RULI_RES_OPT_IGNORE_RD    = 1 << 10  /* Ignore MTA DNS RD validation flag. ARRIS ADD */
};

typedef struct {
  const char *qt_name;
  int         qt_code;
} ruli_qtype_pair_t;

typedef struct {
  const char *qc_name;
  int         qc_code;
} ruli_qclass_pair_t ;

typedef struct {
  int  tcp_sd;           /* TCP socket descriptor */
  int  tcp_connecters;   /* # of TCP queries waiting for connection */
  int  tcp_writers;      /* # of TCP queries waiting to send */
  int  tcp_readers;      /* # of TCP queries waiting to receive */
  int  port;             /* server DNS port */

  /* 14-byte buffer for header */
  ruli_uint8_t tcp_head_buf[RULI_LIMIT_TCP_HEADER];
  int          tcp_head_len;                        /* received length */
} ruli_server_t;

/*
 * This type is used for the search list
 */
typedef struct {
  /* encoded, uncompressed */
  ruli_uint8_t domain_name[RULI_LIMIT_DNAME_ENCODED]; 
  size_t       domain_len;
} ruli_domain_t;


struct ruli_conf_handler_t;
typedef struct ruli_conf_handler_t ruli_conf_handler_t;

struct ruli_conf_handler_t {
  void *opaque;

  /*
   * load/unload search list
   */
  ruli_list_t *(*search_loader)(ruli_conf_handler_t *self);
  void (*search_unloader)(ruli_conf_handler_t *self, ruli_list_t *search_list);

  /*
   * load/unload ns list
   */
  ruli_list_t *(*ns_loader)(ruli_conf_handler_t *self);
  void (*ns_unloader)(ruli_conf_handler_t *self, ruli_list_t *ns_list);
};


/* Opaque resolver context */
typedef struct {
  /*
   * private members
   */
  ruli_list_t   server_list;   /* list of ruli_server_t* */
  ruli_list_t   query_list;    /* list of ruli_res_query_t* */
  int           udp_sd;        /* descriptor for UDP socket */
  int           udp_writers;   /* # of UDP queries waiting to send */
  int           udp_readers;   /* # of UDP queries waiting to receive */
  int           udp6_sd;       /* inet6: descriptor for UDP socket */
  int           udp6_writers;  /* inet6: # of UDP queries waiting to send */
  int           udp6_readers;  /* inet6: # of UDP queries waiting to receive */
  int           next_server;   /* server given when a query asks for one */
  ruli_uint16_t next_query_id; /* query id serial generator */
  ruli_rand_t   rand_ctx;      /* random generator */
  ruli_list_t   *search_list;  /* dynamic conf: list of ruli_domain_t* */
  ruli_list_t   *ns_list;      /* dynamic conf: list of ruli_addr_t* */

  /*
   * public members
   */
  ruli_conf_handler_t *res_conf_handler; /* dynamic config handler */
  oop_source          *res_source;       /* event source */
  int                 res_retry;         /* retry limit for each server */
  int                 res_timeout;       /* query timeout for each server */

  char*             res_interface;      /* interface to use to send this query */
} ruli_res_t;


struct ruli_res_query_t;
typedef struct ruli_res_query_t ruli_res_query_t;

typedef void *(*ruli_res_query_call_t)(ruli_res_query_t *qry,
				       void *arg);

/* Opaque query context */
struct ruli_res_query_t {
  /* 
   * private members
   */
  ruli_res_t     *resolver;      /* the resolver */
  int            resolver_index; /* index of this query in the resolver list */
  ruli_uint16_t  query_id;       /* the query unique id */
  int            status;         /* query status */
  int            first_server;   /* server suggested by resolver */
  int            curr_server;    /* server the query is using */
  ruli_uint8_t   query_buf[RULI_RES_QBUF_SIZE]; /* buffer for query message */
  int            query_buf_size;                /* size of buffer */
  int            query_msg_len;                 /* length of message */
  int            remaining_retries; /* # of remaining retries */
  struct timeval tv;                /* saved for timeout de-scheduling */
  int            search_index;   /* index in the resolver search list */
  ruli_uint8_t   full_dname[RULI_LIMIT_DNAME_ENCODED]; /* encoded, uncomp. */
  int            full_dname_len;                       /* length of above */

  /* 
   * public members
   */

  /* input */
  ruli_res_query_call_t q_on_answer;       /* query callback function */
  void                  *q_on_answer_arg;  /* query callback opaque argument */
  char                  *q_domain;         /* query domain */
  int                   q_domain_len;      /* domain length */
  int                   q_class;           /* query class */
  int                   q_type;            /* query type */
  long                  q_options;         /* query options */

  /* output */
  char              *answer_buf;
  int               answer_buf_size;
  int               answer_msg_len;
  int               answer_code;
  ruli_msg_header_t answer_header;
};

int ruli_get_qtype_code(const char *qt_name);
int ruli_get_qclass_code(const char *qc_name);
const char *ruli_get_qtype_name(int qt_code);
const char *ruli_get_qclass_name(int qc_code);
int ruli_rr_type_is_address(long options, int rr_type);

const char *ruli_res_errstr(int result);

int ruli_res_new(ruli_res_t *res_ctx);
void ruli_res_delete(ruli_res_t *res_ctx);

int ruli_res_query_submit(ruli_res_t *res_ctx, ruli_res_query_t *res_qry);
void ruli_res_query_delete(ruli_res_query_t *res_qry);

int ruli_res_get_curr_server_index(ruli_res_query_t *res_qry);
ruli_server_t *ruli_res_get_curr_server(ruli_res_query_t *res_qry);
ruli_addr_t *ruli_res_get_curr_serv_addr(ruli_res_query_t *res_qry);
int ruli_res_get_curr_serv_port(ruli_res_query_t *res_qry);
int _ruli_get_curr_tcp_socket(ruli_res_query_t *qry);

/*
 * Exported to ruli_fsm
 */
int ruli_res_switch_server(ruli_res_query_t *res_qry);
ruli_res_query_t *ruli_res_find_query_by_id(ruli_list_t *query_list, 
					    ruli_uint16_t query_id);
ruli_server_t *ruli_res_find_server_by_sd(ruli_list_t *server_list, 
					  int sd);
ruli_uint8_t *ruli_qry_tcp_buf(ruli_res_query_t *qry);
ruli_uint8_t *ruli_qry_udp_buf(ruli_res_query_t *qry);
int ruli_qry_tcp_msg_len(ruli_res_query_t *qry);
int ruli_qry_udp_msg_len(ruli_res_query_t *qry);

void *_ruli_fsm_query_done(ruli_res_query_t *qry);

#endif /* RULI_RES_H */

