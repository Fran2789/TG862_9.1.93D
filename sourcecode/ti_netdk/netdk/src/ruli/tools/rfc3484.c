/*-GNU-GPL-BEGIN-*
RULI - Resolver User Layer Interface - Querying DNS SRV records
Copyright (C) 2005 Everton da Silva Marques

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
  $Id: rfc3484.c,v 1.8 2005/08/31 10:43:17 evertonm Exp $
 */


#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <ruli.h>


typedef struct addr_entry addr_entry;

/*
  RFC 3484

  Prefix        Precedence Label
  ::1/128               50     0
  ::/0                  40     1
  2002::/16             30     2
  ::/96                 20     3
  ::ffff:0:0/96         10     4
*/

struct addr_entry {
  const char *addr;
  int scope;
  int label;
  int precedence;
} addr_table[] = {

  /* 
     IPv4
   */

  { "255.255.255.255", RULI_ADDR_SCOPE_GLOBAL,     4, 10 },
  { "0.0.0.0",         RULI_ADDR_SCOPE_UNSPEC,     3, 20 },
  { "127.0.0.1",       RULI_ADDR_SCOPE_LINK_LOCAL, 0, 50 },
  { "127.0.0.0",       RULI_ADDR_SCOPE_LINK_LOCAL, 4, 10 },
  { "127.0.0.2",       RULI_ADDR_SCOPE_LINK_LOCAL, 4, 10 },
  { "10.0.0.0",        RULI_ADDR_SCOPE_SITE_LOCAL, 4, 10 },
  { "11.0.0.0",        RULI_ADDR_SCOPE_GLOBAL,     4, 10 },
  { "172.16.0.0",      RULI_ADDR_SCOPE_SITE_LOCAL, 4, 10 },
  { "172.31.0.0",      RULI_ADDR_SCOPE_SITE_LOCAL, 4, 10 },
  { "172.15.0.0",      RULI_ADDR_SCOPE_GLOBAL,     4, 10 },
  { "172.32.0.0",      RULI_ADDR_SCOPE_GLOBAL,     4, 10 },
  { "192.168.0.0",     RULI_ADDR_SCOPE_SITE_LOCAL, 4, 10 },
  { "192.169.0.0",     RULI_ADDR_SCOPE_GLOBAL,     4, 10 },

  /* 
     IPv6 
  */

  /* vv----< multicast, hence  */
  /*    v--< this is the scope */
  { "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF", 0xF, 1,  40 },
  { "FF00:0000:0000:0000:0000:0000:0000:0000", 0,   1,  40 },

  { "::",  RULI_ADDR_SCOPE_UNSPEC,                  3,  20 },
  { "::1", RULI_ADDR_SCOPE_LINK_LOCAL,              0,  50 },
  { "::2", RULI_ADDR_SCOPE_GLOBAL,                  3,  20 },
  { "::3", RULI_ADDR_SCOPE_GLOBAL,                  3,  20 },

  /* 0xFE8/10 is link local */
  /* 0x8 = 1000 (only first two bits are meaningful) */
  { "FE80::", RULI_ADDR_SCOPE_LINK_LOCAL,           1,  40 },
  /* 0xB = 1011 (only first two bits are meaningful) */
  { "FEB0::1", RULI_ADDR_SCOPE_LINK_LOCAL,          1,  40 },

  /* 0xFEC/10 is site local */
  /* 0xC = 1100 (only first two bits are meaningful) */
  { "FEC0::", RULI_ADDR_SCOPE_SITE_LOCAL,           1,  40 },
  /* 0xF = 1111 (only first two bits are meaningful) */
  { "FEF0::1", RULI_ADDR_SCOPE_SITE_LOCAL,          1,  40 },

  { "2000::", RULI_ADDR_SCOPE_GLOBAL,               1,  40 },

  /* 0x2002/16 is label=2, precedence=30 */
  { "2002::", RULI_ADDR_SCOPE_GLOBAL,               2,  30 },
  { "2002:8000::", RULI_ADDR_SCOPE_GLOBAL,          2,  30 },
  { "2003::", RULI_ADDR_SCOPE_GLOBAL,               1,  40 },

#if 0
  { "0.0.0.0",         RULI_ADDR_SCOPE_UNSPEC,     3, 20 },
  { "::",  RULI_ADDR_SCOPE_UNSPEC,                  3,  20 },
#endif

  /* end of table */
  { 0,     0,  0,  0  }
};


static void eval_addresses(ruli_list_t *addr_list)
{
  char buf[40];
  int result;
  addr_entry *entry;

  result = ruli_list_new(addr_list);
  assert(!result);

  for (entry = &addr_table[0]; entry->addr; ++entry) {
    ruli_addr_t *addr;
    int scope_must, label_must, prec_must;
    int scope_found, label_found, prec_found;

    addr = ruli_addr_parse_new(entry->addr);
    if (!addr) {
      fprintf(stderr, "parsing failed: %s\n", entry->addr);
      continue;
    }
    if (ruli_list_push(addr_list, addr)) {
      fprintf(stderr, "storing failed: %s\n", entry->addr);
      continue;
    }

    scope_must = entry->scope;
    scope_found = ruli_addr_get_scope(&addr->addr, ruli_addr_family(addr));

    label_must = entry->label;
    label_found = ruli_addr_get_label(&addr->addr, ruli_addr_family(addr));

    prec_must = entry->precedence;
    prec_found = ruli_addr_get_precedence(&addr->addr, ruli_addr_family(addr));

    ruli_addr_snprint(buf, sizeof(buf), addr);

    fprintf(stderr, "%39s s_m=%2d s=%2d %4s",
	    buf, scope_must, scope_found,
	    (scope_must == scope_found) ? "sOK" : "sBAD");

    fprintf(stderr, " l_m=%d l=%d %4s",
	    label_must, label_found,
	    (label_must == label_found) ? "lOK" : "lBAD");

    fprintf(stderr, " p_m=%2d p=%2d %4s",
	    prec_must, prec_found,
	    (prec_must == prec_found) ? "pOK" : "pBAD");

    fprintf(stderr, "\n");
  }
}

static void show_addresses(ruli_list_t *addr_list)
{
  int addr_list_size = ruli_list_size(addr_list);
  char buf[40];
  int i;

  for (i = 0; i < addr_list_size; ++i) {
    ruli_addr_t *addr = ruli_list_get(addr_list, i);
    int scope;
    int label;
    int prec;

    scope = ruli_addr_get_scope(&addr->addr, ruli_addr_family(addr));
    label = ruli_addr_get_label(&addr->addr, ruli_addr_family(addr));
    prec  = ruli_addr_get_precedence(&addr->addr, ruli_addr_family(addr));

    ruli_addr_snprint(buf, sizeof(buf), addr);

    fprintf(stderr, 
	    "%39s S%2d L%d P%2d", 
	    buf, scope, label, prec);

    if (ruli_addr_has_source(addr)) {
      char src[40];
      int src_scope;
      int src_label;
      int src_prec;
      int bitlen;
      const _ruli_addr *src_ad = ruli_addr_src_get_addr(addr);
      int src_family = ruli_addr_src_get_family(addr);

      src_scope = ruli_addr_get_scope(src_ad, src_family);
      src_label = ruli_addr_get_label(src_ad, src_family);
      src_prec  = ruli_addr_get_precedence(src_ad, src_family);

      if (ruli_addr_family(addr) == src_family)
	bitlen = ruli_addr_get_common_prefix_bitlen((const ruli_uint8_t *)
						    &addr->addr, 
						    (const ruli_uint8_t *)
						    src_ad,
						    ruli_addr_size(addr));
      else
	bitlen = -1;

      ruli_in_snprint(src, sizeof(src), src_ad, src_family);

      fprintf(stderr, 
	      " %s S%2d L%d P%2d C%3d", 
	      src, src_scope, src_label, src_prec, bitlen);
    }
    else {
      fprintf(stderr, " UNREACHABLE");
    }

    fprintf(stderr, "\n");
  }
}

static void go()
{
  ruli_list_t addr_list;

  eval_addresses(&addr_list);

  ruli_addr_rfc3484_sort(&addr_list, RULI_RES_OPT_SRV_RFC3484);

  fprintf(stderr, "-- after RFC3484 --\n");

  show_addresses(&addr_list);

  ruli_list_dispose_trivial(&addr_list);
}

int main()
{
  go();
  exit(0);
}
