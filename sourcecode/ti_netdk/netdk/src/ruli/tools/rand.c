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
  $Id: rand.c,v 1.2 2004/02/24 20:38:22 evertonm Exp $
  */


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "ruli_rand.h"


static void go()
{
  const int N = 100000;
  const int M = 10;

  int         x[M];
  ruli_rand_t rand_ctx;
  int         i;

  for (i = 0; i < M; ++i)
    x[i] = 0;

  ruli_rand_init(&rand_ctx);

  for (i = 0; i < N; ++i) {
    int j = ruli_rand_next(&rand_ctx, 0, M - 1);
    assert(j >= 0);
    assert(j < M);
    ++x[j];
  }

  for (i = 0; i < M; ++i)
    printf("%2d: %d\n", i, x[i]);
}


int main()
{
  go();

  exit(0);
}
