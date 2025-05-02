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
  $Id: ruli_addr.c,v 1.24 2005/12/21 10:08:31 evertonm Exp $
  */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for GNU memrchr() */
#endif


#include <stdio.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <ruli_util.h>
#include <ruli_addr.h>
#include <ruli_mem.h>
#include <ruli_sock.h>
#include <ruli_srv.h>


void ruli_addr_init(ruli_addr_t *addr, int family)
{
  addr->addr_family = family;
  addr->has_src = 0; /* false */
}

ruli_addr_t *ruli_addr_inet_new(struct in_addr ia)
{
  ruli_addr_t *addr = (ruli_addr_t *) ruli_malloc(sizeof(*addr));
  if (!addr)
    return 0;

  ruli_addr_init(addr, PF_INET);
  addr->addr.ipv4 = ia;

  return addr;
}

ruli_addr_t *ruli_addr_inet6_new(struct in6_addr ia)
{
  ruli_addr_t *addr = (ruli_addr_t *) ruli_malloc(sizeof(*addr));
  if (!addr)
    return 0;

  ruli_addr_init(addr, PF_INET6);
  addr->addr.ipv6 = ia;

  return addr;
}

int ruli_addr_family(const ruli_addr_t *addr)
{
  return addr->addr_family;
}

int ruli_addr_size(const ruli_addr_t *addr)
{
  switch (addr->addr_family) {
  case PF_INET:
    return sizeof(addr->addr.ipv4);
  case PF_INET6:
    return sizeof(addr->addr.ipv6);
  }

  assert(0);

  return 0;
}

static void ruli_addr_find_source(ruli_addr_t *addr)
{
  int fd;
  union {
    struct sockaddr_in  inet;
    struct sockaddr_in6 inet6;
  } dst;
  int dst_len = sizeof(dst);
  socklen_t src_len;

  addr->has_src = 0; /* false */

  fd = socket(addr->addr_family, SOCK_DGRAM, IPPROTO_IP);
  if (fd < 0)
    return;

  ruli_sock_set_sockaddr((struct sockaddr *) &dst, &dst_len,
			 &addr->addr, addr->addr_family, 0);

  if (connect(fd, (const struct sockaddr *) &dst, dst_len)) {
    close(fd);
    return;
  }

  src_len = sizeof(addr->src_sock);
  if (getsockname(fd, (struct sockaddr *) &addr->src_sock, &src_len)) {
    close(fd);
    return;
  }

  close(fd);

  addr->src_family = addr->src_sock.sock.sin_family;

  assert(addr->src_family == addr->src_sock.sock6.sin6_family);

  addr->has_src = -1; /* true */
}

int ruli_addr_src_get_family(const ruli_addr_t *addr)
{
  assert(ruli_addr_has_source(addr));
  return addr->src_family;
}

int ruli_addr_has_source(const ruli_addr_t *addr)
{
  return addr->has_src;
}

_ruli_addr *ruli_addr_src_get_addr(const ruli_addr_t *addr)
{
  _ruli_addr *ad = NULL;

  assert(ruli_addr_has_source(addr));

  switch (ruli_addr_src_get_family(addr)) {
  case PF_INET:
    ad = (_ruli_addr *) &addr->src_sock.sock.sin_addr;
    break;
  case PF_INET6:
    ad = (_ruli_addr *) &addr->src_sock.sock6.sin6_addr;
    break;
  default:
    assert(0);
  }

  return ad;
}

struct in_addr ruli_addr_inet(const ruli_addr_t *addr)
{
  assert(ruli_addr_family(addr) == PF_INET);
  assert(ruli_addr_size(addr) == sizeof(struct in_addr));

  return addr->addr.ipv4;
}

struct in6_addr ruli_addr_inet6(const ruli_addr_t *addr)
{
  assert(ruli_addr_family(addr) == PF_INET6);
  assert(ruli_addr_size(addr) == sizeof(struct in6_addr));

  return addr->addr.ipv6;
}

#define RULI_WORD16_LEN ((int) sizeof(struct in6_addr) / 2)

static int search_forward(const char *cp, const char *past_end,
			  unsigned int *word16, int *wi,
			  const char **wildcard)
{
  const char *i;
  long w;

  *wi = 0;

  for (i = cp; i < past_end; ) {

    if (*i == ':') {
      ++i;
      if (*i == ':') {
        *wildcard = i;
	return 0;
      }
      /* can't start addr with : */
      if (*wi == 0)
        return -1;
    }

    if (*wi >= RULI_WORD16_LEN)
      return -1;

    if (!isxdigit((int) *i))
      return -1;

    errno = 0;
    w = strtol(i, 0, 16);
    if (errno)
      return -1;

    if (w < 0)
      return -1;
    if (w > 0xFFFF)
      return -1;

    word16[*wi] = w;
    ++*wi;

    i = (const char *) memchr(i, ':', past_end - i);
    if (!i)
      break;
  }

  return 0;
}

static int search_backward(const char *cp, const char *past_end,
			   unsigned int *word16, int *wj,
			   const char **wildcard)
{
  const char *i;
  long w;

  *wj = RULI_WORD16_LEN;

  for (i = past_end; i > cp; --i) {
    const char *j;

    i = (const char *) ruli_memrchr(cp, ':', i - cp);
    if (!i)
      return -1;

    if (*wj <= 0)
      return -1;

    j = i + 1;

    errno = 0;
    w = strtol(j, 0, 16);
    if (errno)
      return -1;

    if (w < 0)
      return -1;
    if (w > 0xFFFF)
      return -1;

    word16[--*wj] = w;

    if (*(i - 1) == ':') {
      *wildcard = i;
      return 0;
    }

    if (!isxdigit((int) *j))
      return -1;
  }

  return -1;
}

int ruli_inet6_aton(const char *cp, struct in6_addr *inp)
{
  unsigned int word16[RULI_WORD16_LEN];
  const char *past_end;
  const char *wildcard_fwd = 0;
  const char *wildcard_bwd;
  int wi, wj;

  /* locate end of string */
  past_end = (const char *) memchr(cp, '\0', 40);
  if (!past_end)
    return 0;

  /* search from start up to :: or end */
  if (search_forward(cp, past_end, word16, &wi, &wildcard_fwd))
    return 0;

  if (wi < RULI_WORD16_LEN) {

    /* search from end up to :: */
    if (search_backward(cp, past_end, word16, &wj, &wildcard_bwd))
      return 0;

    /* wildcard :: was hit? */
    if (wildcard_fwd) {
      /* forward found the same wildcard as backward? */
      if (wildcard_fwd != wildcard_bwd)
        return 0;
    }
    
    /* clean unused slots */
    {
      int i;
      for (i = wi; i < wj; ++i)
	word16[i] = 0;
    }
  }
  
  /* convert 2-byte host-order array to 1-byte net-order array */
  {
    char *p = (char *) inp;

    for (wi = 0; wi < RULI_WORD16_LEN; ++wi) {
      unsigned int w = word16[wi];
      *p = w >> 8;
      ++p;
      *p = w & 0xFF;
      ++p;
    }
  }

  return -1;
}

int ruli_addr_parse(const char *p, ruli_addr_t *addr)
{
  if (ruli_inet6_aton(p, &addr->addr.ipv6)) {
    ruli_addr_init(addr, PF_INET6);
    return 0; 
  }

  if (inet_aton(p, &addr->addr.ipv4)) {
    ruli_addr_init(addr, PF_INET);
    return 0; 
  }

  ruli_addr_init(addr, PF_UNSPEC);

  return -1;
}

ruli_addr_t *ruli_addr_parse_new(const char *p)
{
  struct in_addr addr;
  struct in6_addr addr6;

  if (ruli_inet6_aton(p, &addr6))
    return ruli_addr_inet6_new(addr6);

  if (inet_aton(p, &addr))
    return ruli_addr_inet_new(addr);

  return 0;
}

int ruli_inet6_printf(FILE *out, const char *fmt, const struct in6_addr *ia)
{
  const unsigned char *begin = (const unsigned char *) ia;
  const unsigned char *past_end = begin + sizeof(*ia);
  int wr = 0;
  const unsigned char *i;
  unsigned int sum = 0;
  
  for (i = begin; i < past_end; ++i) {
    int j = i - begin;
    int w;

    assert(wr >= 0);

    sum <<= 8;
    sum += *i;

    assert(sum >= 0);
    assert(sum <= 0xFFFF);

    if (j) {
      if (!(j % 2)) {
	w = fprintf(out, ":");
	if (w < 0)
	  return w;
	wr += w;
      }

      if (!((j + 1) % 2)) {
	w = fprintf(out, fmt, sum);
	if (w < 0)
	  return w;
	wr += w;

	sum = 0;
      }
    }
  }
  
  return wr;
}

int ruli_inet6_print(FILE *out, const struct in6_addr *ia)
{
  return ruli_inet6_printf(out, "%x", ia);
}

int ruli_in_print(FILE *out, const _ruli_addr *addr, int family)
{
  switch (family) {
  case PF_INET:
    return fprintf(out, inet_ntoa(addr->ipv4));

  case PF_INET6:
    return ruli_inet6_print(out, &addr->ipv6);

  default:
    assert(0);
  }

  return -1;
}

int ruli_addr_print(FILE *out, const ruli_addr_t *addr)
{
  return ruli_in_print(out, &addr->addr, addr->addr_family);
}

int ruli_inet6_snprintf(char *buf, size_t size, const char *fmt, const struct in6_addr *ia)
{
  const unsigned char *begin = (const unsigned char *) ia;
  const unsigned char *past_end = begin + sizeof(*ia);
  int wr = 0;
  const unsigned char *i;
  unsigned int sum = 0;
  
  for (i = begin; i < past_end; ++i) {
    int j = i - begin;
    int w;

    assert(wr >= 0);
    assert((size_t) wr < size);

    sum <<= 8;
    sum += *i;

    assert(sum >= 0);
    assert(sum <= 0xFFFF);

    if (j) {
      if (!(j % 2)) {
	w = snprintf(buf + wr, size - wr, ":");
	if (w < 0)
	  return w;
	wr += w;
	if ((size_t) wr >= size)
	  return wr;
      }

      if (!((j + 1) % 2)) {
	w = snprintf(buf + wr, size - wr, fmt, sum);
	if (w < 0)
	  return w;
	wr += w;
	if ((size_t) wr >= size)
	  return wr;

	sum = 0;
      }
    }
  }
  
  return wr;
}

int ruli_inet6_snprint(char *buf, size_t size, const struct in6_addr *ia)
{
  return ruli_inet6_snprintf(buf, size, "%x", ia);
}

int ruli_in_snprint(char *buf, size_t size, const _ruli_addr *addr, int family)
{
  switch (family) {
  case PF_INET:
    return snprintf(buf, size, inet_ntoa(addr->ipv4));

  case PF_INET6:
    return ruli_inet6_snprint(buf, size, &addr->ipv6);    

  default:
    assert(0);
  }

  return -1;
}

int ruli_addr_snprint(char *buf, size_t size, const ruli_addr_t *addr)
{
  return ruli_in_snprint(buf, size, &addr->addr, addr->addr_family);
}

static int get_scope(const _ruli_addr *ad, int family)
{
  switch (family) {
  case PF_INET:
    {
      const ruli_uint8_t *addr = (const ruli_uint8_t *) &ad->ipv4;

      /* 
	 169.254/16, 127/8 are link-local
      */
      if ((addr[0] == 169 && addr[1] == 254) || addr[0] == 127)
        return RULI_ADDR_SCOPE_LINK_LOCAL;

      /*
	10/8, 192.168/16, 172.16/12 are site-local
       */
      if (addr[0] == 10 || 
	  (addr[0] == 192 && addr[1] == 168) ||
	  (addr[0] == 172 && addr[1] >= 16 && addr[1] <= 31))
        return RULI_ADDR_SCOPE_SITE_LOCAL;

      /* unspecified? */
      if (ad->ipv4.s_addr == INADDR_ANY)
	return RULI_ADDR_SCOPE_UNSPEC; /* try to ignore */
    }
    break;
  case PF_INET6:
    {
      const struct in6_addr *addr = (const struct in6_addr *) &ad->ipv6;

      if (IN6_IS_ADDR_MULTICAST(addr))
        return addr->s6_addr[1] & 0xf;

      if (IN6_IS_ADDR_LINKLOCAL(addr))
	return RULI_ADDR_SCOPE_LINK_LOCAL;

      if (IN6_IS_ADDR_LOOPBACK(addr))
	return RULI_ADDR_SCOPE_LINK_LOCAL;
      
      if (IN6_IS_ADDR_SITELOCAL(addr))
	return RULI_ADDR_SCOPE_SITE_LOCAL;

      if (IN6_IS_ADDR_UNSPECIFIED(addr))
	return RULI_ADDR_SCOPE_UNSPEC; /* try to ignore */
    }
    break;
  default:
    assert(0);
  }

  return RULI_ADDR_SCOPE_GLOBAL;
}

int ruli_addr_get_scope(const _ruli_addr *ad, int family)
{
  int scope = get_scope(ad, family);

#ifdef RULI_RFC3484_DEBUG
  fprintf(stderr,
	  "DEBUG %s %s addr=",
	  __FILE__, __PRETTY_FUNCTION__);
    
  ruli_in_print(stderr, ad, family);
    
  fprintf(stderr, " scope=%d\n", scope);
#endif /* RULI_RFC3484_DEBUG */

  return scope;
}

/*
  RFC 3484

  Prefix        Precedence Label
  ::1/128               50     0
  ::/0                  40     1
  2002::/16             30     2
  ::/96                 20     3
  ::ffff:0:0/96         10     4
*/

/* 
static const struct prefixlist {
  struct in6_addr prefix;
  unsigned int bits;
  int val;
} default_label[] = {
  { IN6ADDR_LOOPBACK_INIT,                                128, 0 },
  { { { { 0x20,0x02,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 } } }, 16,  2 },
  { IN6ADDR_ANY_INIT,                                     96,  3 },
  { { { { 0,0,0,0, 0,0,0,0, 0,0,0xff,0xff, 0,0,0,0 } } }, 96,  4 },
  { IN6ADDR_ANY_INIT,                                     0,   1 }
};

static const struct prefixlist default_precedence[] = {
  { IN6ADDR_LOOPBACK_INIT,                                128, 50 },
  { { { { 0x20,0x02,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 } } }, 16,  30 },
  { IN6ADDR_ANY_INIT,                                     96,  20 },
  { { { { 0,0,0,0, 0,0,0,0, 0,0,0xff,0xff, 0,0,0,0 } } }, 96,  10 },
  { IN6ADDR_ANY_INIT,                                     0,   40 }
};
*/

static const struct prefixlist {
  struct in6_addr prefix;
  unsigned int bits;
  int val;
} default_label[] = {
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,1 }, 128, 0 },
  { { 0x20,0x02,0,0, 0,0,0,0, 0,0,0,0,       0,0,0,0 }, 16,  2 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,0 }, 96,  3 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0xff,0xff, 0,0,0,0 }, 96,  4 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,0 }, 0,   1 }
};

static const struct prefixlist default_precedence[] = {
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,1 }, 128, 50 },
  { { 0x20,0x02,0,0, 0,0,0,0, 0,0,0,0,       0,0,0,0 }, 16,  30 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,0 }, 96,  20 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0xff,0xff, 0,0,0,0 }, 96,  10 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,0 }, 0,   40 }
};

/*
  does ADDRESS addr/addr_len
  belong to NETWORK net/prefix_len ?
 */
static int addr_in_net(const ruli_uint8_t *net, int prefix_len, 
		       const ruli_uint8_t *addr, int addr_len)
{
  const ruli_uint8_t *net_pastend;
  char byte_len, left;

  /*
   * Compares all bytes, but the last one.
   */
  byte_len = prefix_len >> 3; /* prefix_len / 8 */
  net_pastend = net + byte_len;
  for (; net < net_pastend; ++net, ++addr)
    if (*net != *addr)
      return 0;

  /*
   * left = prefix_len % 8 = number of bits 1 at left
   */
  left = prefix_len & 7;
  if (!left)
    return -1;

  /*
   * Compares last byte.
   *
   * 8 - ()       => number of bits 0 at right
   * 0xFF << ()   => insert bits 0 at right
   * *ip_a & ()   => ignore bits 0
   */
  return *net == (*addr & (char) (0xFF << (8 - left)));
}

static int match_prefix(const struct prefixlist *list,
			const _ruli_addr *ad, int family)
{
  int i;
  struct in6_addr in6_mem;
  const struct in6_addr *in6 = NULL;

  switch (family) {
  case PF_INET6:
    in6 = (const struct in6_addr *) &ad->ipv6;
    break;
  case PF_INET:
    {
      const struct in_addr *in = (const struct in_addr *) &ad->ipv4;

      /* 
	 Convert to IPv6 address
       */

      if (in->s_addr == htonl(0)) {
	struct in6_addr tmp = IN6ADDR_ANY_INIT;
	memcpy(&in6_mem, &tmp, sizeof(tmp));
      }
      else if (in->s_addr == htonl(0x7f000001)) {
	struct in6_addr tmp = IN6ADDR_LOOPBACK_INIT;
	memcpy(&in6_mem, &tmp, sizeof(tmp));
      }
      else {
	/* Construct a V4-to-6 mapped address.  */
	memset(&in6_mem, '\0', sizeof(in6_mem));

	/* in6_mem.s6_addr16[5] = 0xffff; */
	((unsigned char *) &in6_mem)[10] = 0xff;
	((unsigned char *) &in6_mem)[11] = 0xff;

	/* in6_mem.s6_addr32[3] = in->s_addr; */
	memcpy(((char *) &in6_mem) + 12, &in->s_addr, 4);
      }

      in6 = &in6_mem;
    }
    break;
  default:
    assert(0);
  }

  for (i = 0; ; ++i) {
    unsigned int bits = list[i].bits;
    ruli_uint8_t *mask = list[i].prefix.s6_addr;

#ifdef RULI_RFC3484_DEBUG
    fprintf(stderr,
	    "DEBUG %s %s ad=",
	    __FILE__, __PRETTY_FUNCTION__);
    ruli_in_print(stderr, ad, family);
    fprintf(stderr, " v6=");
    ruli_in_print(stderr, (const _ruli_addr *) in6, PF_INET6);
    fprintf(stderr, " prefix[%d]=", i);
    ruli_in_print(stderr, (const _ruli_addr *) mask, PF_INET6);
    fprintf(stderr, "/%d\n", bits);
#endif /* RULI_RFC3484_DEBUG */
    
    if (addr_in_net(mask, bits, (const ruli_uint8_t *) in6, sizeof(*in6)))
      return list[i].val;
  }

  assert(0);

  return -1;
}

int ruli_addr_get_label(const _ruli_addr *ad, int family)
{
  int label = match_prefix(default_label, ad, family);

#ifdef RULI_RFC3484_DEBUG
  fprintf(stderr,
	  "DEBUG %s %s addr=",
	  __FILE__, __PRETTY_FUNCTION__);
    
  ruli_in_print(stderr, ad, family);
    
  fprintf(stderr, " label=%d\n", label);
#endif /* RULI_RFC3484_DEBUG */

  return label;
}

int ruli_addr_get_precedence(const _ruli_addr *ad, int family)
{
  int precedence = match_prefix(default_precedence, ad, family);

#ifdef RULI_RFC3484_DEBUG
  fprintf(stderr,
	  "DEBUG %s %s addr=",
	  __FILE__, __PRETTY_FUNCTION__);
    
  ruli_in_print(stderr, ad, family);
    
  fprintf(stderr, " precedence=%d\n", precedence);
#endif /* RULI_RFC3484_DEBUG */

  return precedence;
}

int ruli_addr_get_common_prefix_bitlen(const ruli_uint8_t *a1, 
				       const ruli_uint8_t *a2,
				       int byte_len)
{
  const ruli_uint8_t *a1_pastend;
  int bitlen;

  assert(byte_len >= 0);
  assert(byte_len <= 32);
  
  bitlen = 0;

  for (a1_pastend = a1 + byte_len; ; ++a1, ++a2, bitlen += 8) {
    if (a1 >= a1_pastend)
      break;
    if (*a1 != *a2) {
      int mask;
      int match_bits = 8;
      for (mask = *a1 ^ *a2; mask; mask >>= 1)
	--match_bits;
      bitlen += match_bits;
      break;
    }
  }

  assert(bitlen >= 0);
  assert(bitlen <= 128);

  return bitlen;
}

static int rfc3484_cmp(const void *addr1, const void *addr2)
{
  const ruli_addr_t *a1 = *(const ruli_addr_t * const *) addr1;
  const ruli_addr_t *a2 = *(const ruli_addr_t * const *) addr2;
  int a1_dst_scope;
  int a2_dst_scope;
  int a1_family;
  int a2_family;

  /* 
     Rule 1:  Avoid unusable destinations. 
   */
  if (a1->has_src && !a2->has_src)
    return -1;
  if (!a1->has_src && a2->has_src)
    return 1;

  /* 
     Rule 2:  Prefer matching scope. 
  */
  a1_family = ruli_addr_family(a1);
  a2_family = ruli_addr_family(a2);

  a1_dst_scope = ruli_addr_get_scope(&a1->addr, a1_family);
  a2_dst_scope = ruli_addr_get_scope(&a2->addr, a2_family);

  if (a1->has_src && a2->has_src) {
    _ruli_addr *a1_src = ruli_addr_src_get_addr(a1);
    _ruli_addr *a2_src = ruli_addr_src_get_addr(a2);
    
    int a1_src_family = ruli_addr_src_get_family(a1);
    int a2_src_family = ruli_addr_src_get_family(a2);
    
    int a1_src_scope = ruli_addr_get_scope(a1_src, a1_src_family);
    int a2_src_scope = ruli_addr_get_scope(a2_src, a2_src_family);
    
    if ((a1_dst_scope == a1_src_scope) && (a2_dst_scope != a2_src_scope))
      return -1;
    if ((a1_dst_scope != a1_src_scope) && (a2_dst_scope == a2_src_scope))
      return 1;
  }
  
  /* 
     Rule 3:  Avoid deprecated addresses. How?? FIXME
  */

  /*
    Rule 4:  Prefer home addresses. How?? FIXME
  */

  /*
    Rule 5:  Prefer matching label.
  */

  if (a1->has_src && a2->has_src) {
    int a1_dst_label = ruli_addr_get_label(&a1->addr, a1_family);
    int a2_dst_label = ruli_addr_get_label(&a2->addr, a2_family);
    
    _ruli_addr *a1_src = ruli_addr_src_get_addr(a1);
    _ruli_addr *a2_src = ruli_addr_src_get_addr(a2);
    
    int a1_src_family = ruli_addr_src_get_family(a1);
    int a2_src_family = ruli_addr_src_get_family(a2);
    
    int a1_src_label = ruli_addr_get_label(a1_src, a1_src_family);
    int a2_src_label = ruli_addr_get_label(a2_src, a2_src_family);
    
    if ((a1_dst_label == a1_src_label) && (a2_dst_label != a2_src_label))
      return -1;
    if ((a1_dst_label != a1_src_label) && (a2_dst_label == a2_src_label))
      return 1;
  }

  /*
    Rule 6:  Prefer higher precedence.
  */
  {
    int a1_prec = ruli_addr_get_precedence(&a1->addr, a1_family);
    int a2_prec = ruli_addr_get_precedence(&a2->addr, a2_family);

    if (a1_prec > a2_prec)
      return -1;
    if (a1_prec < a2_prec)
      return 1;
  }

  /*
    Rule 7:  Prefer native transport. How?? FIXME
  */

  /*
    Rule 8:  Prefer smaller scope.
  */
  if (a1_dst_scope < a2_dst_scope)
    return -1;
  if (a1_dst_scope > a2_dst_scope)
    return 1;

  /*
    Rule 9:  Use longest matching prefix.
  */

  if (a1_family == a2_family)
    if (a1->has_src && a2->has_src) {
      _ruli_addr *a1_src = ruli_addr_src_get_addr(a1);
      _ruli_addr *a2_src = ruli_addr_src_get_addr(a2);
      int addr_size = ruli_addr_size(a1);
      int len1;
      int len2;

      assert(addr_size == ruli_addr_size(a2));

      len1 = ruli_addr_get_common_prefix_bitlen((const ruli_uint8_t *) 
						&a1->addr, 
						(const ruli_uint8_t *) 
						a1_src, 
						addr_size);
      len2 = ruli_addr_get_common_prefix_bitlen((const ruli_uint8_t *) 
						&a2->addr, 
						(const ruli_uint8_t *) 
						a2_src, 
						addr_size);

      if (len1 > len2)
	return -1;
      if (len1 < len2)
	return 1;
    }

  /*
    Rule 10:  Otherwise, leave the order unchanged.
  */

  return 0;
}

static long addr_cmp_options;

static int addr_cmp_preference(const void *addr1, const void *addr2)
{
  const ruli_addr_t *a1 = *(const ruli_addr_t * const *) addr1;
  const ruli_addr_t *a2 = *(const ruli_addr_t * const *) addr2;
  int result;

  /* Apply RFC3484 destination address selection rules? */
  if (addr_cmp_options & RULI_RES_OPT_SRV_RFC3484) {
    result = rfc3484_cmp(addr1, addr2);
    if (result)
      return result;
  }

  /* Give preference to IPv6 addresses over IPv4? */
  if (!(addr_cmp_options & RULI_RES_OPT_SRV_NOSORT6)) {
    int f1 = ruli_addr_family(a1);
    int f2 = ruli_addr_family(a2);

    if ((f1 == PF_INET6) && (f2 == PF_INET))
      return -1;

    if ((f1 == PF_INET) && (f2 == PF_INET6))
      return 1;
  }

  /* Perform stable sorting */
  if (addr1 < addr2)
    return -1;
  if (addr1 > addr2)
    return 1;

  return 0;
}

static void find_addr_src(ruli_list_t *addr_list)
{
  int addr_list_size = ruli_list_size(addr_list);
  int j;

  for (j = 0; j < addr_list_size; ++j) {
    ruli_addr_t *addr = ruli_list_get(addr_list, j);
    ruli_addr_find_source(addr);
    
#ifdef RULI_RFC3484_DEBUG
    fprintf(stderr,
	    "DEBUG %s %s dst=",
	    __FILE__, __PRETTY_FUNCTION__);
    
    ruli_addr_print(stderr, addr);
    
    fprintf(stderr, 
	    " has_src=%d src_family=%d src=",
	    addr->has_src, addr->src_family);
    
    switch (addr->src_family) {
    case PF_INET:
      fprintf(stderr, inet_ntoa(addr->src_sock.sock.sin_addr));
      break;
    case PF_INET6:
      ruli_inet6_print(stderr, &addr->src_sock.sock6.sin6_addr);
      break;
    default:
      fprintf(stderr, "?");
    }
    
    fprintf(stderr, "\n");
#endif /* RULI_RFC3484_DEBUG */
  }
}

void ruli_addr_rfc3484_sort(ruli_list_t *addr_list, long options)
{
  /* 
     solve source addresses for later RFC3484 
     destination address selection rules?
  */
  if (options & RULI_RES_OPT_SRV_RFC3484)
    find_addr_src(addr_list);

  /* argument for addr_cmp_preference functor */
  addr_cmp_options = options;

  /* sort address list */
  qsort(addr_list->head, ruli_list_size(addr_list),
	sizeof(void*), addr_cmp_preference);
}
