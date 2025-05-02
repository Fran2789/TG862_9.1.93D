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
  $Id: ruli_limits.h,v 1.7 2003/02/12 06:10:52 evertonm Exp $
 */


#ifndef RULI_LIMITS_H
#define RULI_LIMITS_H


/*
  message sizing

  section size
  ---------------
  header  12
  qname   1..255  [the ENCODED dname must fit into 255]
  qtype   2
  qclass  2
  ---------------
  total   17..271
*/

enum {
  RULI_LIMIT_MSG_HEADER        = 12,
  RULI_LIMIT_TCP_HEADER        = RULI_LIMIT_MSG_HEADER + 2, /* 14 */

  RULI_LIMIT_MSG_LOW           = RULI_LIMIT_MSG_HEADER + 5,  /* 17 */
  RULI_LIMIT_MSG_HIGH          = 271,

  RULI_LIMIT_DNAME_ENCODED     = 255, /* bufsz can have same length */
  RULI_LIMIT_DNAME_TEXT        = RULI_LIMIT_DNAME_ENCODED - 2, /* 253 */
  RULI_LIMIT_DNAME_TEXT_BUFSZ  = RULI_LIMIT_DNAME_TEXT + 1,    /* 253 + EOS */

  RULI_LIMIT_DATAGRAM_HIGH     = 512, 

  RULI_LIMIT_LABEL_LOW         = 1,
  RULI_LIMIT_LABEL_HIGH        = 63,

  RULI_LIMIT_COMPRESSION_DEPTH = 20,  /* arbitrary */

  RULI_LIMIT_CNAME_DEPTH       = 5    /* arbitrary */
};


#endif /* RULI_LIMITS_H */
