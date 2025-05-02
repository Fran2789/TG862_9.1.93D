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
  $Id: ruli_txt.h,v 1.9 2004/06/16 17:28:44 evertonm Exp $
 */


#ifndef RULI_TXT_H
#define RULI_TXT_H


#include <ruli_util.h>


enum {
  RULI_TXT_OK  = 0,
  RULI_TXT_BAD = -1
};


int ruli_dname_encode_size(const char *dname, int dname_len);

char *ruli_dname_encode(char *buf, int buf_size, 
			const char *dname, int dname_len);

int ruli_dname_decode(char *dst_buf, int dst_buf_size, int *dst_name_len,
		      const char *src_dname, int src_dname_len);

int ruli_dname_extract(const ruli_uint8_t *msg, 
		       const ruli_uint8_t *msg_pastend,
		       ruli_uint8_t *buf,
		       const ruli_uint8_t *buf_pastend, 
		       const ruli_uint8_t *label, 
		       int *total_len);

int ruli_dname_match(const char *name1, int len1, 
		     const char *name2, int len2);

int ruli_dname_expand(const ruli_uint8_t *src_msg, 
		      const ruli_uint8_t *src_msg_pastend,
		      ruli_uint8_t       *dst_buf,
		      const ruli_uint8_t *dst_buf_pastend, 
		      const ruli_uint8_t *src_label, 
		      size_t             *expanded_len);

int ruli_dname_compare(const ruli_uint8_t *comp_name,
                       const ruli_uint8_t *comp_name_msg, size_t comp_name_msg_len,
                       const ruli_uint8_t *uncomp_name, size_t uncomp_name_len);

int ruli_dname_concat(ruli_uint8_t *dst_buf, int dst_buf_len, int *len,
                      const ruli_uint8_t *src1, int len1,
                      const ruli_uint8_t *src2, int len2);


#endif /* RULI_TXT_H */

