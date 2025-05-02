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
  $Id: ruli_conf.c,v 1.10 2005/06/29 01:34:22 evertonm Exp $
 */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for GNU strtok_r(), inet_aton() */
#endif


#include <stdio.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ruli_conf.h>
#include <ruli_mem.h>
#include <ruli_txt.h>


#define LOAD_SEARCH_LIST_INBUFSZ 1024
#define LOAD_NS_LIST_INBUFSZ     1024


const char *const RESOLV_CONF = "/etc/resolv.conf";
const char *const SEARCH      = "search";
const char *const NAMESERVER  = "nameserver";


static int conf_load_search_list(ruli_list_t *search_list)
{
  int list_size;

  list_size = ruli_list_size(search_list);

  assert(list_size == 0);

  /*
   * Parse resolv.conf
   */
  {
    char inbuf[LOAD_SEARCH_LIST_INBUFSZ];
    FILE *in;

    in = fopen(RESOLV_CONF, "r");
    if (!in)
      return -1;

    /*
     * Scan file for 'search'
     */
    for (;;) {
      if (!fgets(inbuf, LOAD_SEARCH_LIST_INBUFSZ, in)) {
	/* If EOF, exit nicely */
	if (feof(in))
	  break;

	/* If error, exit nicely too */
	break;
      }

      /* Make sure it's a null-terminated string */
      if (!memchr(inbuf, '\0', LOAD_SEARCH_LIST_INBUFSZ))
        continue;

      /*
       * Scan tokens
       */
      {
	const char *SEP = "\r\n\t ";
	char       *ptr;
	char       *tok;
	
	tok = strtok_r(inbuf, SEP, &ptr);
	if (!tok)
	  continue;

	if (strcmp(tok, SEARCH))
	  continue;

	/*
	 * Parse domains
	 */
	for (;;) {
	  ruli_domain_t *dom;
	  char          *i;

	  /* Find domain name */
	  tok = strtok_r(0, SEP, &ptr);
	  if (!tok)
	    break;

	  /* Allocate space for domain name */
	  dom = (ruli_domain_t *) ruli_malloc(sizeof(ruli_domain_t));
	  if (!dom)
	    break;

	  assert(sizeof(char) == sizeof(ruli_uint8_t));

	  /* Encode domain name into space */
	  i = ruli_dname_encode((char *) dom->domain_name, 
				RULI_LIMIT_DNAME_ENCODED,
				tok, strlen(tok));
	  if (!i) {
	    ruli_free(dom);
	    break;
	  }
	  dom->domain_len = (ruli_uint8_t *) i - dom->domain_name;

	  assert(dom->domain_len > 0);
	  assert(dom->domain_len <= RULI_LIMIT_DNAME_ENCODED);

	  /* Save space into search_list */
	  if (ruli_list_push(search_list, dom))
	    ruli_free(dom);
	}
      }

    } /* for */

    fclose(in);
  }

  list_size = ruli_list_size(search_list);

#ifdef RULI_CONF_DEBUG
  {
    int i;

    fprintf(stderr, 
	    "DEBUG: %s: %s(): loaded search list:",
	    __FILE__, __PRETTY_FUNCTION__);

    for (i = 0; i < list_size; ++i) {
      ruli_domain_t *dom = ruli_list_get(search_list, i);
      char txt_domain[RULI_LIMIT_DNAME_TEXT_BUFSZ];
      int  txt_len;

      assert(sizeof(char) == sizeof(ruli_uint8_t));

      {
	int result = ruli_decode_dname(txt_domain, RULI_LIMIT_DNAME_TEXT_BUFSZ,
				       &txt_len, (char *) dom->domain_name,
				       dom->domain_len);
	assert(!result);
      }

      fprintf(stderr, " %s", txt_domain);
    }

    fprintf(stderr, "\n");
  }
#endif

  if (list_size < 1)
    return -1;

  return 0;
}

ruli_list_t *ruli_conf_load_search_list(ruli_conf_handler_t *bogus)
{
  ruli_list_t *search_list = (ruli_list_t *) ruli_malloc(sizeof(ruli_list_t));
  if (!search_list)
    return 0;

  if (ruli_list_new(search_list)) {
    ruli_free(search_list);
    return 0;
  }

  if (conf_load_search_list(search_list)) {
    assert(!ruli_list_size(search_list));
    ruli_list_delete(search_list);
    ruli_free(search_list);
    return 0;
  }

  return search_list;
}

void ruli_conf_unload_search_list(ruli_conf_handler_t *bogus, 
				  ruli_list_t *search_list)
{
  assert(search_list);

  ruli_list_dispose_trivial(search_list);

  ruli_free(search_list);
}

static int load_ns_list(ruli_list_t *ns_list)
{
  int list_size;

  list_size = ruli_list_size(ns_list);

  assert(!list_size);

  /*
   * Parse resolv.conf
   */
  {
    char inbuf[LOAD_NS_LIST_INBUFSZ];
    FILE *in;

    in = fopen(RESOLV_CONF, "r");
    if (!in)
      return -1;

    /*
     * Scan file for 'nameserver'
     */
    for (;;) {
      if (!fgets(inbuf, LOAD_NS_LIST_INBUFSZ, in)) {
	/* If EOF, exit nicely */
	if (feof(in))
	  break;

	/* If error, exit nicely too */
	break;
      }

      /* Make sure it's a null-terminated string */
      if (!memchr(inbuf, '\0', LOAD_NS_LIST_INBUFSZ))
        continue;


      /*
       * Scan tokens
       */
      {
	const char *SEP = "\r\n\t ";
	char       *ptr;
	char       *tok;

	tok = strtok_r(inbuf, SEP, &ptr);
	if (!tok)
	  continue;

	if (strcmp(tok, NAMESERVER))
	  continue;

	/*
	 * Parse addresses
	 */
	for (;;) {
	  struct in_addr addr;
	  struct in6_addr addr6;
	  ruli_addr_t *ad = 0;
	  
	  tok = strtok_r(0, SEP, &ptr);
	  if (!tok)
	    break;

	  /* IPv4 address? */
	  if (inet_aton(tok, &addr))
	    ad = ruli_addr_inet_new(addr);

	  /* IPv6 address? */
	  if (!ad)
	    if (ruli_inet6_aton(tok, &addr6))
	      ad = ruli_addr_inet6_new(addr6);

	  if (!ad)
	    break;

	  if (ruli_list_push(ns_list, ad))
	    ruli_free(ad);
	}
      }

    } /* for */

    fclose(in);
  }

  list_size = ruli_list_size(ns_list);

#ifdef RULI_CONF_DEBUG
  {
    int i;

    fprintf(stderr, 
	    "DEBUG: %s: %s(): loaded ns list:"
            __FILE__, __PRETTY_FUNCTION__);

    for (i = 0; i < list_size; ++i) {
      ruli_addr_t *addr = ruli_list_get(ns_list, i);
      fprintf(stderr, " ");
      ruli_addr_print(stderr, addr);
    }

    fprintf(stderr, "\n");
  }
#endif

  if (list_size < 1)
    return -1;

  return 0;
}

ruli_list_t *ruli_conf_load_ns_list(ruli_conf_handler_t *bogus)
{
  ruli_list_t *ns_list = (ruli_list_t *) ruli_malloc(sizeof(ruli_list_t));
  if (!ns_list)
    return 0;

  if (ruli_list_new(ns_list)) {
    ruli_free(ns_list);
    return 0;
  }

  if (load_ns_list(ns_list)) {
    assert(!ruli_list_size(ns_list));
    ruli_list_delete(ns_list);
    ruli_free(ns_list);
    return 0;
  }

  return ns_list;
}

void ruli_conf_unload_ns_list(ruli_conf_handler_t *bogus,
                              ruli_list_t *ns_list)
{
  assert(ns_list);

  ruli_list_dispose_trivial(ns_list);

  ruli_free(ns_list);
}

