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
  $Id: ruli_rand.h,v 1.6 2003/05/07 15:40:18 evertonm Exp $
*/


#ifndef RULI_RAND_H
#define RULI_RAND_H


#include "ruli_isaac.h"


typedef struct {
  isaac_randctx isaac_ctx;
} ruli_rand_t;


void ruli_rand_init(ruli_rand_t *rand_ctx);
int ruli_rand_next(ruli_rand_t *rand_ctx, int min, int max);


#endif /* RULI_RAND_H */
