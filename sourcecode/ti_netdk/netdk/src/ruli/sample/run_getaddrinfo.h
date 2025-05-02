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

/* $Id: run_getaddrinfo.h,v 1.1 2005/08/31 10:43:17 evertonm Exp $ */

#ifndef RUN_GETADDRINFO_H
#define RUN_GETADDRINFO_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int run_getaddrinfo(const char *node, const char *service,
		     const struct addrinfo *hints, struct addrinfo **res);
void run_freeaddrinfo(struct addrinfo *res);

#endif /* RUN_GETADDRINFO_H */
