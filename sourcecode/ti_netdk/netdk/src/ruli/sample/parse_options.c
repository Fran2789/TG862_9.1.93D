/*-GNU-GPL-BEGIN-*
RULI - Resolver User Layer Interface - Querying DNS SRV records
Copyright (C) 2004 Everton da Silva Marques

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
  $Id: parse_options.c,v 1.4 2005/06/28 22:27:02 evertonm Exp $
 */

#include <string.h>
#include <stdlib.h>

#include <ruli.h>

#include "parse_options.h"


struct opt_map {
  const char *name;
  long option;
} opt_table[] = {
  { "search",   RULI_RES_OPT_SEARCH },
  { "need_ra",  RULI_RES_OPT_NEED_RA },
  { "noinet",   RULI_RES_OPT_SRV_NOINET },
  { "noinet6",  RULI_RES_OPT_SRV_NOINET6 },
  { "uri_port", RULI_RES_OPT_SRV_URI_PORT },
  { "nowalk",   RULI_RES_OPT_SRV_NOWALK },
  { "nofall",   RULI_RES_OPT_SRV_NOFALL },
  { "nosort6",  RULI_RES_OPT_SRV_NOSORT6 },
  { "rfc3484",  RULI_RES_OPT_SRV_RFC3484 },
  { "cname",    RULI_RES_OPT_SRV_CNAME },
  { 0, 0 }
};


long parse_options(int argc, const char *argv[], int first)
{
  long options = 0;
  int i;

  for (i = first; i < argc; ++i) {
    const char *arg = argv[i];
    int j;

    if (!strncmp(arg, "h", 1) || !strncmp(arg, "-h", 2)) {
      printf("%s: available resolver options:", argv[0]);
      for (j = 0;; ++j) {
	struct opt_map opt = opt_table[j];
	if (!opt.name) 
	  break;
	printf(" %s", opt.name);
      }
      printf("\n");
      exit(0);
    }

    for (j = 0;; ++j) {
      struct opt_map opt = opt_table[j];
      if (!opt.name) {
	fprintf(stderr, 
		"%s: ignoring unknown resolver option: %s\n", 
		argv[0], arg);
	break;
      }
      if (!strcmp(arg, opt.name)) {
	options |= opt.option;
	break;
      }
    }
  }

  return options;
}
