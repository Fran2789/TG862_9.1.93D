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
  $Id: ruli_sync.c,v 1.27 2004/10/08 04:22:15 evertonm Exp $
 */


#include <stdio.h>      /* FIXME: remove me [used for fprintf() debug] */

#include <assert.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ruli_oop.h>
#include <ruli_sync.h>
#include <ruli_txt.h>
#include <ruli_mem.h>
#include <ruli_conf.h>
#include <ruli_smtp.h>
#include <ruli_http.h>


static void *on_search_answer(ruli_search_srv_t *search, void *search_arg)
{
  /* ruli_sync_t *syn_qry = search_arg; */

  return OOP_CONTINUE;
}

ruli_sync_t *ruli_sync_query(const char *txt_service, const char *txt_domain,
			     int fallback_port, long options)
{
  oop_source_sys *source_sys;
  oop_source     *source;
  ruli_sync_t    *syn_qry;

  /*
   * Create event source
   */

  source_sys = oop_sys_new();
  if (!source_sys)
    return 0;

  source = oop_sys_source(source_sys);
  if (!source) {
    oop_sys_delete(source_sys);
    return 0;
  }

#ifdef RULI_SYNC_DEBUG
    fprintf(stderr, 
	    "ruli_sync_query(): DEBUG: event source DONE\n");
#endif

  /*
   * Create sync query context
   */

  syn_qry = (ruli_sync_t *) ruli_malloc(sizeof(ruli_sync_t));
  if (!syn_qry) {
    oop_sys_delete(source_sys);
    return 0;
  }

#ifdef RULI_SYNC_DEBUG
    fprintf(stderr, 
	    "ruli_sync_query(): DEBUG: query context DONE\n");
#endif

  /*
   * Create search resolver context
   */

  syn_qry->search_res = ruli_search_res_new(source, 2, 10, NULL);
  if (!syn_qry->search_res) {
    oop_sys_delete(source_sys);
    ruli_free(syn_qry);

    return 0;
  }

  /*
   * Submit query
   */
  {
    ruli_res_t *res = ruli_search_resolver(syn_qry->search_res);

    assert(res);

    syn_qry->search = ruli_search_srv_submit(res,
                                             on_search_answer,
  					     syn_qry,
    					     options,
  					     txt_service,
  					     txt_domain,
  					     fallback_port);
    if (!syn_qry->search) {
      assert(syn_qry->search_res);

      ruli_search_res_delete(syn_qry->search_res);
      oop_sys_delete(source_sys);
      ruli_free(syn_qry);

      return 0;
    }
  }

  /*
   * Run event loop
   */
  {
    void *oop_result = oop_sys_run(source_sys);

    assert(oop_result == OOP_CONTINUE);
  }

  oop_sys_delete(source_sys); /* destroy event source */

  return syn_qry;
}

ruli_sync_t *ruli_sync_smtp_query(const char *txt_domain, long options)
{
  oop_source_sys *source_sys;
  oop_source     *source;
  ruli_sync_t    *syn_qry;

  /*
   * Create event source
   */

  source_sys = oop_sys_new();
  if (!source_sys)
    return 0;

  source = oop_sys_source(source_sys);
  if (!source) {
    oop_sys_delete(source_sys);
    return 0;
  }

  /*
   * Create sync query context
   */

  syn_qry = (ruli_sync_t *) ruli_malloc(sizeof(ruli_sync_t));
  if (!syn_qry) {
    oop_sys_delete(source_sys);
    return 0;
  }

  /*
   * Create search resolver context
   */

  syn_qry->search_res = ruli_search_res_new(source, 2, 10, NULL);
  if (!syn_qry->search_res) {
    oop_sys_delete(source_sys);
    ruli_free(syn_qry);

    return 0;
  }

  /*
   * Submit query
   */
  {
    ruli_res_t *res = ruli_search_resolver(syn_qry->search_res);

    assert(res);

    syn_qry->search = ruli_search_smtp_submit(res,
					      on_search_answer,
					      syn_qry,
					      options,
					      txt_domain);
    if (!syn_qry->search) {
      assert(syn_qry->search_res);

      ruli_search_res_delete(syn_qry->search_res);
      oop_sys_delete(source_sys);
      ruli_free(syn_qry);

      return 0;
    }
  }

  /*
   * Run event loop
   */
  {
    void *oop_result = oop_sys_run(source_sys);

    assert(oop_result == OOP_CONTINUE);
  }

  oop_sys_delete(source_sys); /* destroy event source */

  return syn_qry;
}

/*
 * This query uses fallback to address
 * records as implicit fallback mechanism.
 */
ruli_sync_t *ruli_sync_http_query(const char *txt_domain, int port, 
				  long options)
{
  oop_source_sys *source_sys;
  oop_source     *source;
  ruli_sync_t    *syn_qry;

  /*
   * Create event source
   */

  source_sys = oop_sys_new();
  if (!source_sys)
    return 0;

  source = oop_sys_source(source_sys);
  if (!source) {
    oop_sys_delete(source_sys);
    return 0;
  }

  /*
   * Create sync query context
   */

  syn_qry = (ruli_sync_t *) ruli_malloc(sizeof(ruli_sync_t));
  if (!syn_qry) {
    oop_sys_delete(source_sys);
    return 0;
  }

  /*
   * Create search resolver context
   */

  syn_qry->search_res = ruli_search_res_new(source, 2, 10, NULL);
  if (!syn_qry->search_res) {
    oop_sys_delete(source_sys);
    ruli_free(syn_qry);

    return 0;
  }

  /*
   * Submit query
   */
  {
    ruli_res_t *res = ruli_search_resolver(syn_qry->search_res);

    assert(res);

    /*
     * This query uses fallback to address
     * records as implicit fallback mechanism.
     */
    syn_qry->search = ruli_search_http_submit(res,
					      on_search_answer,
					      syn_qry,
					      port,
					      options,
					      txt_domain);
    if (!syn_qry->search) {
      assert(syn_qry->search_res);

      ruli_search_res_delete(syn_qry->search_res);
      oop_sys_delete(source_sys);
      ruli_free(syn_qry);

      return 0;
    }
  }

  /*
   * Run event loop
   */
  {
    void *oop_result = oop_sys_run(source_sys);

    assert(oop_result == OOP_CONTINUE);
  }

  oop_sys_delete(source_sys); /* destroy event source */

  return syn_qry;
}

void ruli_sync_delete(ruli_sync_t *syn_qry)
{
  assert(syn_qry);
  assert(syn_qry->search);
  assert(syn_qry->search_res);

  ruli_search_srv_delete(syn_qry->search);
  ruli_search_res_delete(syn_qry->search_res);
  ruli_free(syn_qry);
}

int ruli_sync_srv_code(const ruli_sync_t *syn_qry)
{
  assert(syn_qry);
  assert(syn_qry->search);
  return ruli_search_srv_code(syn_qry->search);
}

int ruli_sync_rcode(ruli_sync_t *syn_qry)
{
  assert(syn_qry);
  assert(syn_qry->search);
  return ruli_search_srv_rcode(syn_qry->search);
}

ruli_list_t *ruli_sync_srv_list(ruli_sync_t *syn_qry)
{
  assert(syn_qry);
  assert(syn_qry->search);
  return ruli_search_srv_answer_list(syn_qry->search);
}

