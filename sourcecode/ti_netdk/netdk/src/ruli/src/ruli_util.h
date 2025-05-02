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
  $Id: ruli_util.h,v 1.6 2005/12/07 21:16:18 evertonm Exp $
  */


#ifndef RULI_UTIL_H
#define RULI_UTIL_H


#include <sys/types.h>


#ifndef RULI_MAX
#define RULI_MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef RULI_MIN
#define RULI_MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if defined(_RULI_SOLARIS_) && !defined(__cplusplus)
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
extern int snprintf(char *, size_t, const char *, ...);
#endif


typedef unsigned char  ruli_uint8_t;
typedef unsigned short ruli_uint16_t;
typedef unsigned int   ruli_uint32_t;

ruli_uint8_t *ruli_unpack2(ruli_uint8_t *i, ruli_uint16_t val);
ruli_uint16_t ruli_pack2(const ruli_uint8_t *i);
ruli_uint32_t ruli_pack4(const ruli_uint8_t *i);
void *ruli_memrchr(const void *buf, int c, size_t size);


#endif /* RULI_UTIL_H */

