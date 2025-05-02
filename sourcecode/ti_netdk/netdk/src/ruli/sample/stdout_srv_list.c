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
  $Id: stdout_srv_list.c,v 1.7 2004/05/28 22:17:03 evertonm Exp $
 */


#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "stdout_srv_list.h"


void show_srv_list(const char *fullname, const ruli_list_t *srv_list)
{
  int srv_list_size = ruli_list_size(srv_list);
  int i;

  assert(srv_list_size >= 0);

  if (srv_list_size < 1) {
    printf("%s empty\n", fullname);
    return;
  }

  /*
   * Scan list of SRV records
   */
  for (i = 0; i < srv_list_size; ++i) {
    ruli_srv_entry_t *entry         = (ruli_srv_entry_t *) ruli_list_get(srv_list, i);
    ruli_list_t      *addr_list     = &entry->addr_list;
    int              addr_list_size = ruli_list_size(addr_list);
    int              j;

    /*
     * Show original full query name
     */
    printf("%s ", fullname);

    /*
     * Show target
     */ 
    {
      char txt_dname_buf[RULI_LIMIT_DNAME_TEXT_BUFSZ];
      int  txt_dname_len;

      if (ruli_dname_decode(txt_dname_buf, RULI_LIMIT_DNAME_TEXT_BUFSZ,
			    &txt_dname_len, 
			    entry->target, entry->target_len)) {

	printf("target-decoding-failed\n");

	continue;
      }

      printf("target=%s ", txt_dname_buf);
    }

    /*
     * Show port number and addresses
     */

    printf("priority=%d weight=%d port=%d addresses=", 
	   entry->priority, entry->weight, entry->port);

    /*
     * Scan addresses
     */

    for (j = 0; j < addr_list_size; ++j) {
      ruli_addr_t *addr = (ruli_addr_t *) ruli_list_get(addr_list, j);
      switch (ruli_addr_family(addr)) {
      case PF_INET:
	printf("IPv4/");
	break;
      case PF_INET6:
	printf("IPv6/");
	break;
      default:
	printf("?/");
      }
      ruli_addr_print(stdout, addr);
      printf(" ");
    }

    printf("\n");
  }
}
