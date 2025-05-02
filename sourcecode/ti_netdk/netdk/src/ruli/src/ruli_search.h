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
  $Id: ruli_search.h,v 1.7 2004/10/04 06:52:05 evertonm Exp $
*/


#ifndef RULI_SEARCH_H
#define RULI_SEARCH_H


#include <ruli_srv.h>

struct ruli_search_srv_t;
typedef struct ruli_search_srv_t ruli_search_srv_t;

/*
 * Query context
 */
struct ruli_search_srv_t {
  ruli_srv_t srv_query;
  void       *(*search_call)(ruli_search_srv_t *search, void *arg);
  void       *search_call_arg;
  char       search_encoded_service[RULI_LIMIT_DNAME_ENCODED];
  int        search_encoded_service_len;
  char       search_encoded_domain[RULI_LIMIT_DNAME_ENCODED];
  int        search_encoded_domain_len;
};

/*
 * Resolver context
 */
typedef struct {
  ruli_res_t resolver;
} ruli_search_res_t;


/*
 * Query functions
 */
ruli_search_srv_t *_ruli_search_srv_submit(void *(*fallback_call)
					   (ruli_srv_t *srv_query),
					   ruli_res_t *resolver, 
					   void *(*call)
					   (ruli_search_srv_t *search,
					    void *arg),
					   void *call_arg,
					   long options,
					   const char *txt_service,
					   const char *txt_domain,
					   int fallback_port);
ruli_search_srv_t *ruli_search_srv_submit(ruli_res_t *resolver, 
					  void *(*call)
					  (ruli_search_srv_t *search, 
					   void *arg),
					  void *call_arg,
					  long options,
					  const char *txt_service,
					  const char *txt_domain,
					  int fallback_port);

void ruli_search_srv_delete(ruli_search_srv_t *search);

int ruli_search_srv_code(const ruli_search_srv_t *search);
int ruli_search_srv_rcode(ruli_search_srv_t *search);
ruli_list_t *ruli_search_srv_answer_list(ruli_search_srv_t *search);

/*
 * Resolver functions
 */
ruli_search_res_t *ruli_search_res_new(oop_source *source, int retry, int timeout, char* interface);
void ruli_search_res_delete(ruli_search_res_t *res);
ruli_res_t *ruli_search_resolver(ruli_search_res_t *search_res);


#endif /* RULI_SEARCH_H */

