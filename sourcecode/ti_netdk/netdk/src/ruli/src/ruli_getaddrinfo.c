/* libruliwrap - wrapper for getaddrinfo using libruli
 * Copyright (C) 2004 Göran Weinholt <goran@weinholt.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 */

/*
  $Id: ruli_getaddrinfo.c,v 1.15 2005/08/31 10:43:17 evertonm Exp $
*/


#ifndef __USE_GNU
#define __USE_GNU
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include <ruli_getaddrinfo.h>
#include <ruli_sync.h>
#include <ruli_txt.h>
#include <ruli_mem.h>


typedef struct {
  struct addrinfo ai;
  union {
    struct sockaddr_in sa_inet;
    struct sockaddr_in6 sa_inet6;
  } sa;
} _ruli_ai_addrbag;


static long _ruli_getaddrinfo_options = RULI_RES_OPT_SEARCH | RULI_RES_OPT_SRV_RFC3484;


long ruli_getaddrinfo_getoptions(void)
{
  return _ruli_getaddrinfo_options;
}

void ruli_getaddrinfo_setoptions(long options)
{
  _ruli_getaddrinfo_options = options;
}

int ruli_getaddrinfo(const char *node, const char *service,
		     const struct addrinfo *hints, struct addrinfo **_res)
{
  ruli_sync_t *query;
  const ruli_list_t *srv_list;
  struct addrinfo *res = NULL, *res0 = NULL, *res1 = NULL;
  char full_service[RULI_LIMIT_DNAME_TEXT_BUFSZ];
  int srv_code, srv_list_size, i;
  long options = ruli_getaddrinfo_getoptions();

  if (!node || (node && !*node) ||
      !service || (service && !*service) || !hints || !_res)
    return getaddrinfo(node, service, hints, _res);

  /* Can't SRV records for numeric hosts. */
  if (hints->ai_flags & AI_NUMERICHOST)
    return getaddrinfo(node, service, hints, _res);
	
  /* No numeric services either. */
  if (atoi(service) > 0 || *service == '0')
    return getaddrinfo(node, service, hints, _res);
	
  if (hints->ai_socktype == SOCK_STREAM)
    snprintf(full_service, RULI_LIMIT_DNAME_TEXT_BUFSZ, "_%s._tcp", service);
  else if (hints->ai_socktype == SOCK_DGRAM)
    snprintf(full_service, RULI_LIMIT_DNAME_TEXT_BUFSZ, "_%s._udp", service);
  else
    return getaddrinfo(node, service, hints, _res);

  if (hints->ai_family == AF_INET)
    options |= RULI_RES_OPT_SRV_NOINET6;
  else if (hints->ai_family == AF_INET6)
    options |= RULI_RES_OPT_SRV_NOINET;
	
  query = ruli_sync_query(full_service, node, -1, options);
  if (!query)
    return getaddrinfo(node, service, hints, _res);

  srv_code = ruli_sync_srv_code(query);
  if (srv_code) {
    ruli_sync_delete(query);
    return getaddrinfo(node, service, hints, _res);
  }

  srv_list = ruli_sync_srv_list(query);
  srv_list_size = ruli_list_size(srv_list);
  if (srv_list_size < 1) {
    ruli_sync_delete(query);
    return getaddrinfo(node, service, hints, _res);
  }

  /* scan srv list */
  for (i = 0; i < srv_list_size; i++) {
    ruli_srv_entry_t *entry = (ruli_srv_entry_t *) ruli_list_get(srv_list, i);
    ruli_list_t *addr_list = &entry->addr_list;
    int addr_list_size = ruli_list_size(addr_list);
    int j;
    char canonname[RULI_LIMIT_DNAME_TEXT_BUFSZ];
    int canonlen;

    if (ruli_dname_decode(canonname, RULI_LIMIT_DNAME_TEXT_BUFSZ,
			  &canonlen, entry->target, entry->target_len))
      continue;

    if (entry->port == -1) {
      struct servent *serv = 0;

      if (hints->ai_socktype == SOCK_STREAM)
	serv = getservbyname(service, "tcp");
      else if (hints->ai_socktype == SOCK_DGRAM)
	serv = getservbyname(service, "udp");
			
      if (!serv)
	continue;

      entry->port = ntohs(serv->s_port);
    }

    /* scan addr list */
    for (j = 0; j < addr_list_size; ++j) {
      ruli_addr_t *addr = (ruli_addr_t *) ruli_list_get(addr_list, j);
      int sockaddr_size;

      switch (ruli_addr_family(addr)) {
      case PF_INET:
	sockaddr_size = sizeof(struct sockaddr_in);
	break;
      case PF_INET6:
	sockaddr_size = sizeof(struct sockaddr_in6);
	break;
      default:
	ruli_sync_delete(query);
	if (res0) {
	  ruli_freeaddrinfo(res0);
	  res0 = 0;
	}
	return EAI_FAMILY;
      }
      
      {
	const int SZ = sizeof(_ruli_ai_addrbag) + canonlen + 1;
	res1 = (struct addrinfo *) ruli_malloc(SZ);
	if (!res1) {
	  ruli_sync_delete(query);
	  return EAI_MEMORY;
	}
	memset(res1, 0, SZ);
      }

      if (!res0)
	res0 = res1;
      else
	res->ai_next = res1;
      res = res1;

      res->ai_family = ruli_addr_family(addr);
      res->ai_socktype = hints->ai_socktype;

      switch (hints->ai_socktype) {
      case SOCK_STREAM:
	res->ai_protocol = IPPROTO_TCP;
	break;
      case SOCK_DGRAM:
	res->ai_protocol = IPPROTO_UDP;
	break;
      default:
	ruli_sync_delete(query);
	if (res0) {
	  ruli_freeaddrinfo(res0);
	  res0 = 0;
	}
	return EAI_SOCKTYPE;
      }

      switch (ruli_addr_family(addr)) {
      case PF_INET: 
	{
	  struct sockaddr_in *ai_addr;

	  ai_addr = &((_ruli_ai_addrbag *) res)->sa.sa_inet;
	  ai_addr->sin_port = htons(entry->port);
	  ai_addr->sin_addr = ruli_addr_inet(addr);
	  ai_addr->sin_family = ruli_addr_family(addr);

	  res->ai_addr = (struct sockaddr *) ai_addr;
	}
	break;
      case PF_INET6:
	{
	  struct sockaddr_in6 *ai_addr;

	  ai_addr = &((_ruli_ai_addrbag *) res)->sa.sa_inet6;
	  ai_addr->sin6_port = htons(entry->port);
	  ai_addr->sin6_addr = ruli_addr_inet6(addr);
	  ai_addr->sin6_family = ruli_addr_family(addr);

	  res->ai_addr = (struct sockaddr *) ai_addr;
	}
	break;
      default:
	ruli_sync_delete(query);
	if (res0) {
	  ruli_freeaddrinfo(res0);
	  res0 = 0;
	}
	return EAI_FAMILY;
      }

      res->ai_addrlen = sockaddr_size;

      if (hints->ai_flags & AI_CANONNAME) {
	res->ai_canonname = (char *) (((_ruli_ai_addrbag *) res) + 1);
	memcpy(res->ai_canonname, canonname, canonlen + 1);
      }

    } /* for addr list */

  } /* for srv list */
	
  ruli_sync_delete(query);

  /* Something went wrong and res0 isn't populated. */
  if (!res0)
    return getaddrinfo(node, service, hints, _res);

  *_res = res0;
  return 0;
}

void ruli_freeaddrinfo(struct addrinfo *res)
{
  struct addrinfo *prev;

  while (res) {
    prev = res;
    res = res->ai_next;
    ruli_free(prev);
  }
}
