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
  $Id: cycle_res2.c,v 1.6 2004/11/10 15:37:32 evertonm Exp $
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


static void *on_answer(ruli_res_query_t *qry, void *arg)
{
  assert(0);

  return OOP_CONTINUE;
}

static void go()
{
  char                *dname_buf = "";
  int                 dname_len = 1;
  ruli_addr_t         server_addr;
  oop_source_sys      *source_sys;
  oop_source          *source;
  ruli_res_query_t    qry;
  ruli_list_t         server_list;
  ruli_res_t          res_ctx;
  int                 result;
  int		      i;
  ruli_conf_handler_t handler;

  result = ruli_addr_parse("127.0.0.1", &server_addr);
  assert(!result);

  source_sys = oop_sys_new();
  assert(source_sys);

  source = oop_sys_source(source_sys);
  assert(source);

  qry.q_on_answer     = on_answer;
  qry.q_on_answer_arg = 0;
  qry.q_domain        = dname_buf;
  qry.q_domain_len    = dname_len;
  qry.q_class         = RULI_RR_CLASS_IN;
  qry.q_type          = RULI_RR_TYPE_A;
  qry.q_options       = RULI_RES_OPT_VOID;

  ruli_list_new(&server_list);

  ruli_list_push(&server_list, &server_addr);

  handler.opaque          = &server_list;
  handler.search_loader   = load_search_list;
  handler.search_unloader = unload_search_list;
  handler.ns_loader       = load_ns_list;
  handler.ns_unloader     = unload_ns_list;

  for (i = 0; /*i <*/ 1000; ++i) {
    res_ctx.res_conf_handler = &handler;
    res_ctx.res_source       = source;
    res_ctx.res_retry        = 1;
    res_ctx.res_timeout      = 1;

    result = ruli_res_new(&res_ctx);
    assert(!result);

    assert(!ruli_list_size(&res_ctx.query_list));

    result = ruli_res_query_submit(&res_ctx, &qry);
    assert(!result);

    assert(ruli_list_size(&res_ctx.query_list) == 1);

    ruli_res_query_delete(&qry);

    assert(!ruli_list_size(&res_ctx.query_list));

    ruli_res_delete(&res_ctx);
  }

  ruli_list_delete(&server_list);
}

int main(int argc, const char **argv) 
{
  go();

  exit(0);
}

