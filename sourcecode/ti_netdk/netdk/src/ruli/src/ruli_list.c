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
  $Id: ruli_list.c,v 1.17 2004/05/21 18:23:50 evertonm Exp $
  */


#include <stdio.h>     /* FIXME: remove me [used for fprintf() debug] */

#include <assert.h>

#include <ruli_list.h>
#include <ruli_mem.h>


void *(*ruli_list_malloc)(size_t)         = ruli_mem_malloc;
void *(*ruli_list_realloc)(void *,size_t) = ruli_mem_realloc;
void (*ruli_list_free)(void *)            = ruli_mem_free;


const char *ruli_list_errstr(int result)
{
  return "FIXME: ruli_list_errstr()";
}

int ruli_list_new_cap(ruli_list_t *list, int cap)
{
  void **head;

  assert(list);
  assert(cap > 0);

  head = (void **) ruli_list_malloc(cap * sizeof(void *));
  if (!head)
    return RULI_LIST_MALLOC;

  list->head     = head;
  list->capacity = cap;
  list->size     = 0;

  return RULI_LIST_OK;
}

int ruli_list_new(ruli_list_t *list)
{
  assert(list);
  return ruli_list_new_cap(list, 10);
}

void ruli_list_delete(ruli_list_t *list)
{
  assert(list);
  assert(list->head);

  ruli_list_free(list->head);
  list->head = 0;

  assert(!list->head);
}

void ruli_list_clean(ruli_list_t *list, ruli_list_disposer_t cleaner)
{
  assert(list);
  assert(list->head);

  {
    void **i        = list->head;
    void **past_end = list->head + list->size;
    
    for (; i < past_end; ++i)
      cleaner(*i);
  }

  list->size = 0;

  assert(!list->size);
  assert(list->head);
}

void ruli_list_clean_trivial(ruli_list_t *list)
{
  assert(list);
  assert(list->head);

  ruli_list_clean(list, ruli_list_free);

  assert(!list->size);
}

void ruli_list_dispose(ruli_list_t *list, ruli_list_disposer_t disposer)
{
  assert(list);
  assert(list->head);
  assert(disposer);

  {
    void **i        = list->head;
    void **past_end = list->head + list->size;
    
    for (; i < past_end; ++i)
      disposer(*i);
  }

  ruli_list_delete(list);

  assert(!list->head);
}

void ruli_list_dispose_trivial(ruli_list_t *list)
{
  assert(list);
  assert(list->head);

  ruli_list_dispose(list, ruli_list_free);

  assert(!list->head);
}

int ruli_list_size(const ruli_list_t *list)
{
  assert(list);
  assert(list->head);
  assert(list->size >= 0);

  return list->size;
}

int ruli_list_capacity(const ruli_list_t *list)
{
  assert(list->head);
  return list->capacity;
}

static int list_grow(ruli_list_t *list)
{
  int  new_cap;
  void **new_head;

  assert(list);
  assert(list->head);

  new_cap = list->capacity << 1;
  assert(new_cap > 0);
  
  new_head = (void **) ruli_list_realloc(list->head, new_cap * sizeof(void *));
  if (!new_head)
    return RULI_LIST_MALLOC;

  list->head     = new_head;
  list->capacity = new_cap;

  return RULI_LIST_OK;
}

int ruli_list_push(ruli_list_t *list, void *item)
{
  assert(list);
  assert(list->head);

  if (list->size >= list->capacity) {
    int result = list_grow(list);
    if (result)
      return result;
  }

  list->head[list->size] = item;
  ++list->size;

  return RULI_LIST_OK;
}

int ruli_list_insert_at(ruli_list_t *list, int idx, void *item)
{
  assert(list);
  assert(list->head);
  assert(idx >= 0);
  assert(idx < list->size);

  if (list->size >= list->capacity) {
    int result = list_grow(list);
    if (result)
      return result;
  }

  {
    void **begin = list->head + idx;
    void **to    = list->head + list->size;
    void **from  = to - 1;

    for (; from >= begin; --from, --to)
      *to = *from;

    *begin = item;
  }

  ++list->size;

  return RULI_LIST_OK;
}

void *ruli_list_shift_at(ruli_list_t *list, int idx)
{
  void *item;

  assert(list);
  assert(list->head);
  assert(idx >= 0);
  assert(idx < list->size);
  assert(list->size > 0);

  {
    void **past_end = list->head + list->size;
    void **to       = list->head + idx;
    void **from     = to + 1;

    item = *to;

    for (; from < past_end; ++from, ++to)
      *to = *from;
  }

  --list->size;

  return item;
}

void *ruli_list_top(const ruli_list_t *list)
{
  assert(list);
  assert(list->head);
  assert(list->size > 0);

  return list->head[list->size - 1];
}

void *ruli_list_pop(ruli_list_t *list)
{
  assert(list);
  assert(list->head);
  assert(list->size > 0);

  --list->size;

  return list->head[list->size];
}

void ruli_list_drop(ruli_list_t *list, int idx)
{
  assert(list);
  assert(list->head);
  assert(idx >= 0);
  assert(idx < list->size);

  --list->size;

  if (idx == list->size)
    return;

  list->head[idx] = list->head[list->size];
}

void *ruli_list_get(const ruli_list_t *list, int idx)
{
  assert(list);
  assert(list->head);
  assert(idx >= 0);
  assert(idx < list->size);

  return list->head[idx];
}

void ruli_list_set(ruli_list_t *list, int idx, void *item)
{
  assert(list);
  assert(list->head);
  assert(idx >= 0);
  assert(idx < list->size);

  list->head[idx] = item;
}

void ruli_list_prune(ruli_list_t *list, int size)
{
  assert(list);
  assert(list->head);
  assert(size >= 0);
  assert(size <= list->size);

  list->size = size;
}

