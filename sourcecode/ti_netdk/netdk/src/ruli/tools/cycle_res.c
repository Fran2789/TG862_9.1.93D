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
  $Id: cycle_res.c,v 1.3 2004/11/10 15:29:39 evertonm Exp $
 */


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>

#include <ruli.h>

#include "trivial_conf_handler.h"


static void go()
{
  ruli_conf_handler_t handler;
  ruli_list_t         bogus_list;
  int                 result;

  result = ruli_list_new(&bogus_list);
  assert(!result);

  result = ruli_list_push(&bogus_list, 0);
  assert(!result);

  handler.opaque          = &bogus_list;
  handler.search_loader   = load_search_list;
  handler.search_unloader = unload_search_list;
  handler.ns_loader       = load_ns_list;
  handler.ns_unloader     = unload_ns_list;

  for (;;) {
    ruli_res_t          res_ctx;

    res_ctx.res_conf_handler = &handler;

    result = ruli_res_new(&res_ctx);
    assert(!result);

    ruli_res_delete(&res_ctx);
  }

  ruli_list_delete(&bogus_list);
}

int main(int argc, const char **argv) 
{
  go();

  exit(0);
}

