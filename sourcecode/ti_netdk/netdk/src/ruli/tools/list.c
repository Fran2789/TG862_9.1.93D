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
  $Id: list.c,v 1.3 2004/05/21 18:23:50 evertonm Exp $
  */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "ruli.h"

static void static_push(ruli_list_t *list, int i)
{
  int result = ruli_list_push(list, (void *) i);
  assert(!result);

  printf("static pushed=%d size=%d capacity=%d\n", i, ruli_list_size(list), ruli_list_capacity(list));
}

static void static_pop(ruli_list_t *list)
{
  int i = (int) ruli_list_pop(list);

  printf("static popped=%d size=%d capacity=%d\n", i, ruli_list_size(list), ruli_list_capacity(list));
}

static void static_show_item(ruli_list_t *list, int i)
{
  printf("static item=%d value=%d\n", i, (int) ruli_list_get(list, i));
}

static void dynamic_push(ruli_list_t *list, int i)
{
  int *j = (int *) malloc(sizeof(int));
  if (!j) {
    fprintf(stderr, "dynamic_push(): malloc() failed: %s", strerror(errno));
    exit(1);
  }
  *j = i;

  {
    int result = ruli_list_push(list, j);
    assert(!result);
  }

  printf("dynamic pushed=%d size=%d capacity=%d\n", i, ruli_list_size(list), ruli_list_capacity(list));
}

static void dynamic_show_item(ruli_list_t *list, int i)
{
  int *j = (int *) ruli_list_get(list, i);

  printf("dynamic item=%d value=%d\n", i, *j);
}

static void static_test()
{
  const int MAX = 10;
  int i;
  ruli_list_t list;

  {
    int result = ruli_list_new_cap(&list, 2);
    assert(!result);
  }

  for (i = 0; i < MAX; ++i)
    static_push(&list, i);

  for (i = 0; i < ruli_list_size(&list); ++i)
    static_show_item(&list, i);

  while (ruli_list_size(&list) > 0)
    static_pop(&list); 

  ruli_list_delete(&list);
}

static void int_disposer(void *ptr)
{
  int *i = (int *) ptr;

  printf("int_disposer: %d\n", *i);

  free(i);
}

static void dynamic_test()
{
  const int MAX = 10;
  int i;
  ruli_list_t list;

  {
    int result = ruli_list_new_cap(&list, 2);
    assert(!result);
  }

  for (i = 0; i < MAX; ++i)
    dynamic_push(&list, i);

  for (i = 0; i < ruli_list_size(&list); ++i)
    dynamic_show_item(&list, i);

  ruli_list_dispose(&list, int_disposer);
}

int main()
{
  static_test();

  dynamic_test();

  exit(0);
}

