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
  $Id: ruli_getaddrinfo.h,v 1.4 2005/08/30 13:21:10 evertonm Exp $
  */


#ifndef RULI_GETADDRINFO
#define RULI_GETADDRINFO


#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

long ruli_getaddrinfo_getoptions(void);
void ruli_getaddrinfo_setoptions(long options);

int ruli_getaddrinfo(const char *node, const char *service,
		     const struct addrinfo *hints, struct addrinfo **_res);
void ruli_freeaddrinfo(struct addrinfo *res);


#endif

