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
  $Id: ruli_sock.h,v 1.10 2005/06/08 22:38:28 evertonm Exp $
  */


#ifndef RULI_SOCKET_H
#define RULI_SOCKET_H


#include <netinet/in.h>
#include <sys/socket.h>

#include <ruli_addr.h>
#include <ruli_util.h>


enum {
  RULI_SOCK_OK          = 0,
  RULI_SOCK_WOULD_BLOCK,
  RULI_SOCK_SEND_FAIL,
  RULI_SOCK_RECV_FAIL,
  RULI_SOCK_CONNECT_FAIL,
  RULI_SOCK_CLOSED
};


/** ARRIS ADD: BEGIN                      **/
#ifdef INCLUDE_ARRIS_GW_TR69
int ruli_sock_create(int family, int type, const char *proto_name, char* interface);
#else
int ruli_sock_create(int family, int type, const char *proto_name);
#endif
/** ARRIS ADD: END                      **/
int ruli_sock_create_udp(int family, char* interface);
int ruli_sock_create_tcp(int family, char* interface);
int ruli_sock_has_connected(int tcp_sd);
void ruli_sock_set_sockaddr(struct sockaddr *sa, int *sa_len, 
			    const _ruli_addr *inet, int family,
			    int port);
int ruli_sock_connect(int sd, const ruli_addr_t *remote_addr, int rem_port);
int ruli_sock_sendto(int sd, const ruli_addr_t *rem_addr, int rem_port,
		     const ruli_uint8_t *buf, int msg_len);
int ruli_sock_send(int sd, const ruli_uint8_t *buf, int msg_len);
int ruli_sock_recvfrom(int sd, ruli_uint8_t *buf, int buf_size, int *msg_len, 
		       struct sockaddr *sa, socklen_t *sa_len);
int ruli_sock_recv(int sd, ruli_uint8_t *buf, int buf_size, int *msg_len);


#endif

