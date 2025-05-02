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
/*
  $Id: ruli_search.c,v 1.9 2004/10/08 04:22:15 evertonm Exp $
 */


#include <stdio.h>       /* FIXME: remove me [used for fprintf() debug] */

#include <string.h>
#include <assert.h>

#include <ruli_search.h>
#include <ruli_txt.h>
#include <ruli_mem.h>
#include <ruli_srv.h>
#include <ruli_conf.h>


static void *on_srv_answer(ruli_srv_t *srv_qry, void *srv_qry_arg)
{
  ruli_search_srv_t *search = (ruli_search_srv_t *) srv_qry_arg;

  return search->search_call(search, search->search_call_arg);
}

/*
  If fallback call is 0, use implicit 
  '_ruli_srv_answer_fallback_addr' as fallback call
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
					   int fallback_port)
{
  ruli_search_srv_t *search;

  assert(memchr(txt_service, '\0', RULI_LIMIT_DNAME_TEXT_BUFSZ));
  assert(memchr(txt_domain, '\0', RULI_LIMIT_DNAME_TEXT_BUFSZ));

  search = (ruli_search_srv_t *) ruli_malloc(sizeof(ruli_search_srv_t));
  if (!search)
    return 0;

  /*
   * Encode query arguments
   */
  {
    int txt_service_len = strlen(txt_service);
    int txt_domain_len  = strlen(txt_domain);
    char *i;

    assert(txt_service_len >= 0);
    assert(txt_domain_len  >= 0);
    assert(txt_service_len <= RULI_LIMIT_DNAME_TEXT);
    assert(txt_domain_len  <= RULI_LIMIT_DNAME_TEXT);
    
    /* Service: as in "_http._tcp" */
    
#ifdef RULI_SEARCH_DEBUG
    fprintf(stderr, 
	    "DEBUG: %s: %s: encoding service: (%d) %s\n",
	    __FILE__, __PRETTY_FUNCTION__,
	    txt_service_len, txt_service);
#endif

    i = ruli_dname_encode(search->search_encoded_service,
			  RULI_LIMIT_DNAME_ENCODED,
			  txt_service, txt_service_len);
    if (!i) {
      ruli_free(search);
      return 0;
    }

    search->search_encoded_service_len = i - search->search_encoded_service;

    assert(search->search_encoded_service_len > 0);
    assert(search->search_encoded_service_len <= RULI_LIMIT_DNAME_ENCODED);

    /* Domain: example.com */

#ifdef RULI_SEARCH_DEBUG
    fprintf(stderr, 
	    "DEBUG: %s(): %s: encoding domain: (%d) %s\n",
	    __FILE__, __PRETTY_FUNCTION__,
	    txt_domain_len, txt_domain);
#endif

    i = ruli_dname_encode(search->search_encoded_domain, 
			  RULI_LIMIT_DNAME_ENCODED,
			  txt_domain, txt_domain_len);
    if (!i) {
      ruli_free(search);
      return 0;
    }

    search->search_encoded_domain_len = i - search->search_encoded_domain;

    assert(search->search_encoded_domain_len <= RULI_LIMIT_DNAME_ENCODED);
  }

#ifdef RULI_SEARCH_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s(): %s: encoding DONE\n",
	  __FILE__, __PRETTY_FUNCTION__);
#endif

  search->search_call     = call;
  search->search_call_arg = call_arg;

  search->srv_query.srv_resolver      = resolver;
  search->srv_query.srv_on_answer     = on_srv_answer;
  search->srv_query.srv_on_answer_arg = search;
  search->srv_query.srv_service       = search->search_encoded_service;
  search->srv_query.srv_service_len   = search->search_encoded_service_len;
  search->srv_query.srv_domain        = search->search_encoded_domain;
  search->srv_query.srv_domain_len    = search->search_encoded_domain_len;
  search->srv_query.srv_fallback_port = fallback_port;
  search->srv_query.srv_options       = options;

  /* Force explicit fallback call */
  if (fallback_call) {

    assert(fallback_call);

    if (_ruli_srv_query_submit(&search->srv_query, fallback_call)) {
      ruli_free(search);
      return 0;
    }
    
    return search;
  }

  /* Use implicit '_ruli_srv_answer_fallback_addr' as fallback call' */
  if (ruli_srv_query_submit(&search->srv_query)) {
    ruli_free(search);
    return 0;
  }

  return search;
}

/*
  This function just invokes _ruli_search_srv_submit with
  0 as fallback call, what means to use the implicit 
  '_ruli_srv_answer_fallback_addr' as fallback call
*/
ruli_search_srv_t *ruli_search_srv_submit(ruli_res_t *resolver, 
					  void *(*call)
					  (ruli_search_srv_t *search, 
					   void *arg),
					  void *call_arg,
					  long options,
					  const char *txt_service,
					  const char *txt_domain,
					  int fallback_port)
{
  return _ruli_search_srv_submit(0, resolver, call, call_arg, options,
				 txt_service, txt_domain, fallback_port);
}

void ruli_search_srv_delete(ruli_search_srv_t *search)
{
  assert(search);
  ruli_srv_query_delete(&search->srv_query);
  ruli_free(search);
}

int ruli_search_srv_code(const ruli_search_srv_t *search)
{
  assert(search);
  return search->srv_query.answer_code;
}

int ruli_search_srv_rcode(ruli_search_srv_t *search)
{
  assert(search);
  return ruli_srv_rcode(&search->srv_query);
}

ruli_list_t *ruli_search_srv_answer_list(ruli_search_srv_t *search)
{
  assert(search);
  return &(search->srv_query.answer_srv_list);
}

/*
 * Added a new fourth parameter which holds the interface to use to send out the 
 * DNS requests.
 */
ruli_search_res_t *ruli_search_res_new(oop_source *source, int retry, 
				       int timeout, char* interface)
{
  ruli_search_res_t *search_res;

  search_res = (ruli_search_res_t *) ruli_malloc(sizeof(ruli_search_res_t));
  if (!search_res)
    return 0;

  search_res->resolver.res_conf_handler = 0; /* default */
  search_res->resolver.res_source       = source;
  search_res->resolver.res_retry        = retry;
  search_res->resolver.res_timeout      = timeout;

  /* Copy the interface name to the resolver context */
  if(interface && strlen(interface) > 0) {
    if((search_res->resolver.res_interface = (char *) ruli_malloc(strlen(interface))) == NULL) {
        ruli_free(search_res);
        printf("ruli_res_new failed: interface malloc failed\n");
        return 0;
    }
    strcpy(search_res->resolver.res_interface, interface);
  }

  if (ruli_res_new(&search_res->resolver)) {
    ruli_free(search_res);
    printf("ruli_res_new failed\n");
    return 0;
  }

  return search_res;
}

void ruli_search_res_delete(ruli_search_res_t *search_res)
{
  assert(search_res);
  ruli_res_delete(&search_res->resolver);
  ruli_free(search_res);
}

ruli_res_t *ruli_search_resolver(ruli_search_res_t *search_res)
{
  assert(search_res);
  return &search_res->resolver;
}

