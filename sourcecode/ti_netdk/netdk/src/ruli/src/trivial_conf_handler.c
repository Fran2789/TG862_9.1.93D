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
  $Id: trivial_conf_handler.c,v 1.2 2004/06/16 17:28:45 evertonm Exp $
 */


#include "trivial_conf_handler.h"


ruli_list_t *load_search_list(ruli_conf_handler_t *handler)
{
  return 0;
}

void unload_search_list(ruli_conf_handler_t *handler, ruli_list_t *search_list)
{
  /* do nothing */
}

ruli_list_t *load_ns_list(ruli_conf_handler_t *handler)
{
  return (ruli_list_t *) handler->opaque;
}

void unload_ns_list(ruli_conf_handler_t *handler, ruli_list_t *ns_list)
{
  /* do nothing */
}

