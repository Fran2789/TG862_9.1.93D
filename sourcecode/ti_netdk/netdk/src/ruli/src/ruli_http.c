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
  $Id: ruli_http.c,v 1.3 2004/10/13 20:11:27 evertonm Exp $
  */


#include <stdio.h>       /* FIXME: remove me [used for fprintf() debug] */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <ruli_http.h>
#include <ruli_mem.h>
#include <ruli_txt.h>
#include <ruli_host.h>


ruli_search_srv_t *ruli_search_http_submit(ruli_res_t *resolver, 
					   void *(*call)(ruli_search_srv_t 
							 *search, void *arg),
					   void *call_arg,
					   int port,
					   long options,
					   const char *txt_domain)
{
  /*
   * If the user passed a valid port number,
   * also assume that port number was forced in
   * the URI. Otherwise, use 80 as default
   * fallback port.
   */
  if (port > 0)
    options |= RULI_RES_OPT_SRV_URI_PORT;
  else
    port = 80;

  /*
   * This query uses fallback to address
   * records as implicit fallback mechanism.
   */
  return ruli_search_srv_submit(resolver,
				call,
				call_arg,
				options,
				"_http._tcp",
				txt_domain,
				port);
}

