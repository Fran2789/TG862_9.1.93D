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
   $Id: ruli_mem.h,v 1.8 2003/05/03 04:28:17 evertonm Exp $
   */


#ifndef RULI_MEM_H
#define RULI_MEM_H


#include <sys/types.h>


void *ruli_mem_malloc(size_t len);
void *ruli_mem_realloc(void *ptr, size_t len);
void ruli_mem_free(void *ptr);

extern void *(*ruli_malloc)(size_t len);
extern void *(*ruli_realloc)(void *ptr, size_t len);
extern void (*ruli_free)(void *ptr);


#endif /* RULI_MEM_H */
