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
  $Id: ruli_addr.h,v 1.16 2005/06/17 10:08:17 evertonm Exp $
  */


#ifndef RULI_ADDR_H
#define RULI_ADDR_H


#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <ruli_list.h>
#include <ruli_util.h>


typedef enum {
  RULI_ADDR_SCOPE_INTERFACE_LOCAL    = 0x1,
  RULI_ADDR_SCOPE_LINK_LOCAL         = 0x2,
  RULI_ADDR_SCOPE_SUBNET_LOCAL       = 0x3,
  RULI_ADDR_SCOPE_ADMIN_LOCAL        = 0x4,
  RULI_ADDR_SCOPE_SITE_LOCAL         = 0x5,
  RULI_ADDR_SCOPE_ORGANIZATION_LOCAL = 0x8,
  RULI_ADDR_SCOPE_GLOBAL             = 0xE,
  RULI_ADDR_SCOPE_UNSPEC             = 0x10
} ruli_addr_scope_t;


typedef union {
  struct in_addr ipv4;
  struct in6_addr ipv6;
} _ruli_addr;


typedef struct {
  int addr_family; /* PF_INET, PF_INET6 */
  _ruli_addr addr;

  /*
    The section below holds the source address
    for RFC3484 destination address selection.
   */
  int has_src; /* boolean */
  int src_family; /* PF_INET, PF_INET6 */
  union {
    struct sockaddr_in sock;
    struct sockaddr_in6 sock6;
  } src_sock;
} ruli_addr_t;


void ruli_addr_init(ruli_addr_t *addr, int family);

/*
 * constructors
 */
ruli_addr_t *ruli_addr_inet_new(struct in_addr ia);
ruli_addr_t *ruli_addr_inet6_new(struct in6_addr ia);

/*
 * access methods
 */
int ruli_addr_family(const ruli_addr_t *addr);
int ruli_addr_size(const ruli_addr_t *addr);
int ruli_addr_has_source(const ruli_addr_t *addr);
int ruli_addr_src_get_family(const ruli_addr_t *addr);
_ruli_addr *ruli_addr_src_get_addr(const ruli_addr_t *addr);
struct in_addr ruli_addr_inet(const ruli_addr_t *addr);
struct in6_addr ruli_addr_inet6(const ruli_addr_t *addr);

/*
 * misc utilities
 */
int ruli_inet6_aton(const char *cp, struct in6_addr *inp);
int ruli_addr_parse(const char *p, ruli_addr_t *addr);
ruli_addr_t *ruli_addr_parse_new(const char *p);
int ruli_inet6_printf(FILE *out, const char *fmt, const struct in6_addr *ia);
int ruli_inet6_print(FILE *out, const struct in6_addr *ia);
int ruli_in_print(FILE *out, const _ruli_addr *addr, int family);
int ruli_addr_print(FILE *out, const ruli_addr_t *addr);
int ruli_inet6_snprintf(char *buf, size_t size, const char *fmt, const struct in6_addr *ia);
int ruli_inet6_snprint(char *buf, size_t size, const struct in6_addr *ia);
int ruli_in_snprint(char *buf, size_t size, const _ruli_addr *addr, int family);
int ruli_addr_snprint(char *buf, size_t size, const ruli_addr_t *addr);
int ruli_addr_get_scope(const _ruli_addr *ad, int family);
int ruli_addr_get_label(const _ruli_addr *ad, int family);
int ruli_addr_get_precedence(const _ruli_addr *ad, int family);
int ruli_addr_get_common_prefix_bitlen(const ruli_uint8_t *a1,
				       const ruli_uint8_t *a2,
				       int byte_len);
void ruli_addr_rfc3484_sort(ruli_list_t *addr_list, long options);

#endif /* RULI_ADDR_H */
