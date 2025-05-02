/*-GNU-GPL-BEGIN-*
RULI - Resolver User Layer Interface - Querying DNS SRV records
Copyright (C) 2005 Everton da Silva Marques

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

/* $Id: run_getaddrinfo.c,v 1.1 2005/08/31 10:43:17 evertonm Exp $ */

#define __USE_GNU /* RTLD_LAZY */

#ifdef DEBUG_DLOPEN
# include <stdio.h> /* fprintf() debug */
#endif

#include <dlfcn.h>
#include <assert.h>

#include "run_getaddrinfo.h"

#ifndef RTLD_LAZY
# define RTLD_LAZY 0x00001
#endif

typedef int (*getaddrinfo_run_t)(const char *node, const char *service,
				  const struct addrinfo *hints,
				  struct addrinfo **res);

typedef void (*freeaddrinfo_run_t)(struct addrinfo *res);

static void *libruli = 0;

int run_getaddrinfo(const char *node, const char *service,
		     const struct addrinfo *hints, struct addrinfo **res)
{
  getaddrinfo_run_t ruli_getaddrinfo;

  if (!libruli) {
    libruli = dlopen("libruli.so", RTLD_LAZY);
    if (!libruli)
      return getaddrinfo(node, service, hints, res);
  }

#ifdef DEBUG_DLOPEN
  fprintf(stderr, "DEBUG: run_getaddrinfo(): dlopen(): libruli.so found\n");
#endif

  ruli_getaddrinfo = (getaddrinfo_run_t)
    dlsym(libruli, "ruli_getaddrinfo");

  if (ruli_getaddrinfo) {
#ifdef DEBUG_DLOPEN
    fprintf(stderr, "DEBUG: run_getaddrinfo(): dlsym(): ruli_getaddrinfo() found\n");
#endif
    return ruli_getaddrinfo(node, service, hints, res);
  }

  return getaddrinfo(node, service, hints, res);
}

void run_freeaddrinfo(struct addrinfo *res)
{
  freeaddrinfo_run_t ruli_freeaddrinfo;

  if (!libruli) {
    libruli = dlopen("libruli.so", RTLD_LAZY);
    if (!libruli)
      return freeaddrinfo(res);
  }

#ifdef DEBUG_DLOPEN
  fprintf(stderr, "DEBUG: run_freeaddrinfo(): dlopen(): libruli.so found\n");
#endif

  ruli_freeaddrinfo = (freeaddrinfo_run_t)
    dlsym(libruli, "ruli_freeaddrinfo");

  if (ruli_freeaddrinfo) {
#ifdef DEBUG_DLOPEN
    fprintf(stderr, "DEBUG: run_getaddrinfo(): dlsym(): ruli_freeaddrinfo() found\n");
#endif
    goto close_libruli;
  }

  freeaddrinfo(res);

 close_libruli:
  {
    int result;
    assert(libruli);
    result = dlclose(libruli);
    assert(!result);
    libruli = 0;
  }
}
