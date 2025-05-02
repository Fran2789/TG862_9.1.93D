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
  $Id: ruli_conf.h,v 1.2 2003/02/13 01:59:26 evertonm Exp $
*/


#ifndef RULI_CONF_H
#define RULI_CONF_H


#include <ruli_res.h>
#include <ruli_list.h>


/*
 * Search list
 */
ruli_list_t *ruli_conf_load_search_list(ruli_conf_handler_t *bogus);
void ruli_conf_unload_search_list(ruli_conf_handler_t *bogus, 
				  ruli_list_t *search_list);

/*
 * Nameserver list
 */
ruli_list_t *ruli_conf_load_ns_list(ruli_conf_handler_t *bogus);
void ruli_conf_unload_ns_list(ruli_conf_handler_t *bogus,
                              ruli_list_t *ns_list);


#endif /* RULI_CONF_H */

