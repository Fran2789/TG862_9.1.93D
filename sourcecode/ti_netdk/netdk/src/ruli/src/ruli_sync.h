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
  $Id: ruli_sync.h,v 1.13 2004/10/08 04:22:15 evertonm Exp $
  */


#ifndef RULI_SYNC_H
#define RULI_SYNC_H


#include <ruli_srv.h>
#include <ruli_search.h>


/* Opaque query context */
typedef struct {
  /*
   * ruli_sync_t private members
   */
  ruli_search_res_t   *search_res;
  ruli_search_srv_t   *search;
  ruli_conf_handler_t conf_handler;
} ruli_sync_t;


/*
 * ruli_sync_t public members
 */

ruli_sync_t *ruli_sync_query(const char *txt_service, const char *txt_domain,
			     int fallback_port, long options);
ruli_sync_t *ruli_sync_smtp_query(const char *txt_domain, long options);
ruli_sync_t *ruli_sync_http_query(const char *txt_domain, int port, 
				  long options);
void ruli_sync_delete(ruli_sync_t *syn_qry);

int ruli_sync_srv_code(const ruli_sync_t *syn_qry);
int ruli_sync_rcode(ruli_sync_t *syn_qry);
ruli_list_t *ruli_sync_srv_list(ruli_sync_t *syn_qry);


#endif /* RULI_SYNC_H */
