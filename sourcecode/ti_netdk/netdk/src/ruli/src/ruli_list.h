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
  $Id: ruli_list.h,v 1.13 2003/05/03 19:52:49 evertonm Exp $
  */


#ifndef RULI_LIST_H
#define RULI_LIST_H


enum {
  RULI_LIST_OK = 0,
  RULI_LIST_MALLOC  /* malloc failed */
};

typedef struct {
  int  size;
  int  capacity;
  void **head;
} ruli_list_t;

typedef void (*ruli_list_disposer_t)(void *);


extern void *(*ruli_list_malloc)(size_t len);
extern void *(*ruli_list_realloc)(void *ptr, size_t len);
extern void (*ruli_list_free)(void *ptr);


const char *ruli_list_errstr(int result);

int ruli_list_new_cap(ruli_list_t *list, int cap);
int ruli_list_new(ruli_list_t *list);
void ruli_list_delete(ruli_list_t *list); /* destroy list, keep contents */

/* clean list contents */
void ruli_list_clean(ruli_list_t *list, ruli_list_disposer_t cleaner);
void ruli_list_clean_trivial(ruli_list_t *list);

/* destroy list, apply disposer on contents */
void ruli_list_dispose(ruli_list_t *list, ruli_list_disposer_t disposer);

/* destroy list, apply free() on contents */
void ruli_list_dispose_trivial(ruli_list_t *list);

int ruli_list_size(const ruli_list_t *list);
int ruli_list_capacity(const ruli_list_t *list);
int ruli_list_push(ruli_list_t *list, void *item);
int ruli_list_insert_at(ruli_list_t *list, int idx, void *item);
void *ruli_list_shift_at(ruli_list_t *list, int idx);
void *ruli_list_top(const ruli_list_t *list);
void *ruli_list_pop(ruli_list_t *list);
void ruli_list_drop(ruli_list_t *list, int idx);
void *ruli_list_get(const ruli_list_t *list, int idx);
void ruli_list_set(ruli_list_t *list, int idx, void *item);
void ruli_list_prune(ruli_list_t *list, int size);


#endif /* RULI_LIST_H */

