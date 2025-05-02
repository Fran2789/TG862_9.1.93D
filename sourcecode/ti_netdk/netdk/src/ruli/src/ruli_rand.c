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
  $Id: ruli_rand.c,v 1.7 2004/05/21 18:23:50 evertonm Exp $
 */


#include <assert.h>

#include <ruli_rand.h>


void ruli_rand_init(ruli_rand_t *rand_ctx)
{
  int i;

  rand_ctx->isaac_ctx.randa = 0;
  rand_ctx->isaac_ctx.randb = rand_ctx->isaac_ctx.randa;
  rand_ctx->isaac_ctx.randc = rand_ctx->isaac_ctx.randc;

  for (i = 0; i < 256; ++i)
    rand_ctx->isaac_ctx.randrsl[i] = 0;

  isaac_randinit(&rand_ctx->isaac_ctx, 1);
}

int ruli_rand_next(ruli_rand_t *rand_ctx, int min, int max)
{
  int          r;
  unsigned int u;

  assert(min <= max);

  u = isaac_rand(&rand_ctx->isaac_ctx);

  r = (int) ((float) (1.0 + max - min) *  u / ISAAC_UB4MAXVAL + min);

  assert(min <= r);
  assert(r <= max);

  return r;  
}
