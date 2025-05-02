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
  $Id: stdin_domains.c,v 1.6 2004/11/10 15:29:39 evertonm Exp $
 */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "stdin_domains.h"


char in_buf[IN_BUF_SIZE];
int  in_buf_len = 0;


static int is_blank(int c)
{
  return isspace(c);
}

static void shift_in_buffer_from(char *i)
{
  int len = in_buf + in_buf_len - i;
  memcpy(in_buf, i, len);
  in_buf_len = len;
}

void reset_stdin_buf()
{
  in_buf_len = 0;
}

int read_stdin(int std_in)
{
  int  rd;

  if (in_buf_len >= IN_BUF_SIZE) {
    fprintf(stderr, 
	    "read_stdin(): buffer overflow reading from stdin\n");

    return STDIN_READ_OVERFLOW;
  }

  rd = read(std_in, in_buf + in_buf_len, IN_BUF_SIZE - in_buf_len);

  /* EOF ? */
  if (!rd)
    return STDIN_READ_EOF;

  /* Error ? */
  if (rd < 0) {
    if (errno == EAGAIN)
      return STDIN_READ_BLOCK;

    fprintf(stderr, 
	    "read_stdin(): can't read stdin: %s\n",
	    strerror(errno));

    return STDIN_READ_ERROR;
  }

  assert(rd <= (IN_BUF_SIZE - in_buf_len));

  in_buf_len += rd;

  return STDIN_READ_OK;
}

int get_next_domain(char *domain_buf, 
		    int domain_buf_size, 
		    int *domain_len)
{
  char *i;
  char *j;
  char *past_end;

  /* 
   * Find domain-name begin 
   */

  i        = in_buf;
  past_end = in_buf + in_buf_len;

  for (; i < past_end; ++i)
    if (!is_blank(*i))
      break;

  if (i >= past_end) {
    
    /* optimized shift_in_buffer_from(past_end) */
    in_buf_len = 0; 

    return PARSE_DOMAIN_NONE;
  }

  /* 
   * Find domain-name end 
   */
  j = i + 1;
  for (; j < past_end; ++j)
    if (is_blank(*j))
      break;

  if (j >= past_end) {
    fprintf(stderr, "get_next_domain(): can't find end of hostname\n");

    assert(j == past_end);

    /* strip off leading blank spaces */
    shift_in_buffer_from(i);

    return PARSE_DOMAIN_NONE;
  }

  /*
   * So we have at least one domain name
   */

  {
    int len = j - i;

    *domain_len = len;

    if (domain_buf_size < len) {
      fprintf(stderr, 
	      "get_next_domain(): won't fit: domain_buf_size=%d < domain_len=%d\n",
	      domain_buf_size, len);
      
      return PARSE_DOMAIN_OVERFLOW;
    }

    memcpy(domain_buf, i, len);
  }

  shift_in_buffer_from(j);

  return PARSE_DOMAIN_OK;
}

void set_non_blocking(int fd)
{
  int result;
  long flags;

  flags = fcntl(fd, F_GETFL, 0);
  assert(flags != -1);

  result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  assert(result != -1);
}
