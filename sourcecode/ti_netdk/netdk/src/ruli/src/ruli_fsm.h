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
  $Id: ruli_fsm.h,v 1.4 2004/06/15 14:42:10 evertonm Exp $
 */


#ifndef RULI_FSM_H
#define RULI_FSM_H


#include <ruli_res.h>


/*
 * Query status
 */
enum {
  RULI_QRY_STAT_VOID               = 0,
  RULI_QRY_STAT_UDP_WANT_SEND      = 1,
  RULI_QRY_STAT_UDP_WANT_RECV      = 2,
  RULI_QRY_STAT_TCP_WANT_CONNECT   = 3,
  RULI_QRY_STAT_TCP_WANT_SEND      = 4,
  RULI_QRY_STAT_TCP_WANT_RECV_HEAD = 5,
  RULI_QRY_STAT_TCP_WANT_RECV_BODY = 6
};

enum {
  RULI_QRY_TCP_STAT_MIN = RULI_QRY_STAT_TCP_WANT_CONNECT,
  RULI_QRY_TCP_STAT_MAX = RULI_QRY_STAT_TCP_WANT_RECV_BODY
};


void _ruli_query_want_write_udp(ruli_res_query_t *qry);
void _ruli_query_status_done(ruli_res_query_t *qry);


#endif /* RULI_FSM_H */
