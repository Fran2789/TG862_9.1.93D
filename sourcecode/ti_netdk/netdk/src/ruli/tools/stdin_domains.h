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
  $Id: stdin_domains.h,v 1.5 2003/01/24 14:39:05 evertonm Exp $
 */


#ifndef STDIN_DOMAINS_H
#define STDIN_DOMAINS_H


#define IN_BUF_SIZE 512


extern char in_buf[];
extern int  in_buf_len;


#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif


/*
  0: ok, proceed
  -1: stdin would block, force continue
  -2: stdin eof, please finish reading from stdin
  -3: stdin error, please halt reading from stdin
  -4: internal buffer overflow
 */
#define STDIN_READ_OK       0
#define STDIN_READ_BLOCK    -1
#define STDIN_READ_EOF      -2
#define STDIN_READ_ERROR    -3
#define STDIN_READ_OVERFLOW -4


/*
  0:  ok, domain found
  -1: no domain found yet
  -2: overflow: domain won't fit on buffer provided
 */
#define PARSE_DOMAIN_OK       0
#define PARSE_DOMAIN_NONE     -1
#define PARSE_DOMAIN_OVERFLOW -2


void reset_stdin_buf();
int read_stdin(int std_in);
int get_next_domain(char *domain_buf, 
		    int domain_buf_size, 
		    int *domain_len);
void set_non_blocking(int fd);


#endif /* STDIN_DOMAINS_H */
