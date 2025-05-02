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
  $Id: ruli_util.c,v 1.4 2004/06/16 21:53:30 evertonm Exp $
  */


#include <ruli_util.h>


ruli_uint8_t *ruli_unpack2(ruli_uint8_t *i, ruli_uint16_t val)
{
  *i = val >> 8;
  *++i = val & 0xFF;

  return ++i;
}

ruli_uint16_t ruli_pack2(const ruli_uint8_t *i)
{
  ruli_uint16_t a, b;

  a = (*i) << 8;
  b = *++i;

  return a | b;
}

ruli_uint32_t ruli_pack4(const ruli_uint8_t *i)
{
  ruli_uint32_t a, b, c, d;

  a = (*i)   << 24;
  b = (*++i) << 16;
  c = (*++i) << 8;
  d = *++i;

  return a | b | c | d;
}

void *ruli_memrchr(const void *buf, int c, size_t size)
{
  unsigned char wanted = c;
  const unsigned char *begin = (const unsigned char *) buf;
  const unsigned char *i;

  for (i = begin + size - 1; i >= begin; --i)
    if (*i == wanted)
      return (void *) i;

  return 0;
}
