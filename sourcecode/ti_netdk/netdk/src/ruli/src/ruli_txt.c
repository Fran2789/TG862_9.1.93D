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
  $Id: ruli_txt.c,v 1.17 2004/08/25 19:04:23 evertonm Exp $
  */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for GNU strncasecmp() */
#endif


#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <ruli_txt.h>
#include <ruli_limits.h>


static int build_label(ruli_uint8_t *buf, int buf_len, 
		       const ruli_uint8_t *label, int label_len)
{
  if (label_len < RULI_LIMIT_LABEL_LOW)
    return RULI_TXT_BAD;

  if (label_len > RULI_LIMIT_LABEL_HIGH)
    return RULI_TXT_BAD;

  if (label_len + 1 > buf_len)
    return RULI_TXT_BAD;

  *buf = label_len;
  ++buf;

  memcpy(buf, label, label_len);

  return RULI_TXT_OK;
}

int ruli_dname_encode_size(const char *dname, int dname_len)
{
  return dname[dname_len - 1] == '.' ? dname_len + 1 : dname_len + 2;
}

char *ruli_dname_encode(char *buf, int buf_size, 
			const char *dname, int dname_len)
{
  const ruli_uint8_t *src;
  const ruli_uint8_t *src_pastend;
  ruli_uint8_t       *dst;
  ruli_uint8_t       *dst_pastend;

  /*
   * We ignore the trailing dot
   * because we always generate
   * the FQDN domain name.
   */
  if (dname[dname_len - 1] == '.')
    --dname_len;

  /*
   * Ensure we have enough buffer space.
   */
  if (buf_size < dname_len + 2)
    return 0;

  /*
   * Ensure given dname is not too long.
   *
   * ENCODED dname must fit into 255
   *
   * When ignoring the trailing dot,
   * TEXTUAL dname must fit into 253
   */
  if (dname_len > RULI_LIMIT_DNAME_ENCODED)
    return 0;

  assert(sizeof(ruli_uint8_t) == sizeof(char));

  src         = (const ruli_uint8_t *) dname;
  src_pastend = (const ruli_uint8_t *) src + dname_len;
  dst         = (ruli_uint8_t *) buf;
  dst_pastend = (ruli_uint8_t *) buf + buf_size;

  /*
   * Scan every label
   */
  for (;;) {   
    /*
     * Last label?
     */
    if (src >= src_pastend) {

      assert(dst >= (ruli_uint8_t *) buf);
      assert(dst < dst_pastend);

      *dst = 0;                           /* mark as last, empty label */
      ++dst;                              /* next position */

      assert(dst > (ruli_uint8_t *) buf);
      assert(dst <= dst_pastend);

      return (char *) dst; 
    }

    /*
     * Encode label
     */
    {
      const ruli_uint8_t *i;
      int                label_len;

      /*
       * Find label end
       */
      for (i = src; i < src_pastend; ++i)
	if (*i == '.')
	  break;

      label_len = i - src;

      /*
       * Build label
       */
      if (build_label(dst, dst_pastend - dst, src, label_len))
	return 0;

      /*
       * Skip label
       */
      {
	int len = label_len + 1;
	src += len;
	dst += len;
      }
    }
  }

}

static int dname_decode(char *dst_buf, int dst_buf_size, 
			int *dst_name_len, 
			const char *src_dname, int src_dname_len)
{
  char       *dst         = dst_buf;
  const char *dst_pastend = dst_buf + dst_buf_size;
  const char *src         = src_dname;
  const char *src_pastend = src_dname + src_dname_len;

  int len, len_plus_1;

  assert(dst <= dst_pastend);
  assert(src <= src_pastend);

  /*
   * Iterate over labels
   */
  for (;;) {

    /* Is the label still inside the source? */
    if (src >= src_pastend)
      return RULI_TXT_BAD;

    len = *src;

    /* Last label? */
    if (!len) {

      if (dst >= dst_pastend)
	return RULI_TXT_BAD;

      /* end of string */
      *dst = '\0';

      return RULI_TXT_OK;
    }

    /* Name compression disallowed */
    if ((len & 0xC0) == 0xC0)
      return RULI_TXT_BAD;

    len_plus_1 = len + 1;

    /* Will name fit into buffer? */
    if (dst + len_plus_1 >= dst_pastend)
      return RULI_TXT_BAD;

    *dst_name_len += len_plus_1;
    
    ++src; /* skip label length */

    /* Copy label into buffer */
    memcpy(dst, src, len);
    dst += len;
    *dst = '.';
    ++dst;

    src += len;
  }

  /* NOT REACHED */
  assert(0);
}

int ruli_dname_decode(char *dst_buf, int dst_buf_size, int *dst_name_len,
		      const char *src_dname, int src_dname_len)
{
  *dst_name_len = 0;

  return dname_decode(dst_buf, dst_buf_size, dst_name_len,
		      src_dname, src_dname_len);
}

/*
 * '*total_len' must be zero before calling this function!
 *
 * Returns -1 on failure, 0 on success.
 */
static int dname_extract(const ruli_uint8_t *msg, 
			 const ruli_uint8_t *msg_pastend,
			 ruli_uint8_t *buf,
			 const ruli_uint8_t *buf_pastend, 
			 const ruli_uint8_t *label, 
			 int *total_len, 
			 int remaining_depth)
{
  int len, len_plus_1;

  assert(msg <= msg_pastend);
  assert(msg <= label);
  assert(buf <= buf_pastend);
  assert(remaining_depth >= 0);

  if (label >= msg_pastend)
    return RULI_TXT_BAD;

  /*
   * Iterate over labels
   */
  for (;;) {

    /* Is the label still inside the message? */
    if (label >= msg_pastend)
      return RULI_TXT_BAD;

    len = *label;

    /* Last label? */
    if (!len) {

      if (buf >= buf_pastend)
	return RULI_TXT_BAD;

      /* end of string */
      *buf = '\0';

      return RULI_TXT_OK;
    }

    /* Name compression? 0xC0 = 192 = 11000000 */
    if ((len & 0xC0) == 0xC0) {

      if (!remaining_depth)
        return RULI_TXT_BAD;

      return dname_extract(msg, msg_pastend,
			   buf, buf_pastend, 
			   msg + (ruli_pack2(label) & ~0xC000),
			   total_len, --remaining_depth);
    }

    len_plus_1 = len + 1;

    /* Will name fit into buffer? */
    if (buf + len_plus_1 >= buf_pastend)
      return RULI_TXT_BAD;

    *total_len += len_plus_1;
    
    ++label; /* skip label length */

    /* Copy label into buffer */
    memcpy(buf, label, len);
    buf += len;
    *buf = '.';
    ++buf;

    label += len;
  }

  /* NOT REACHED */
  assert(0);
}

int ruli_dname_extract(const ruli_uint8_t *msg, 
		       const ruli_uint8_t *msg_pastend,
		       ruli_uint8_t *buf,
		       const ruli_uint8_t *buf_pastend, 
		       const ruli_uint8_t *label, 
		       int *total_len)
{
  *total_len = 0;

  return dname_extract(msg, msg_pastend, 
		       buf, buf_pastend, 
		       label, total_len,
		       RULI_LIMIT_COMPRESSION_DEPTH);
}

/* Handle dname in text format */
int ruli_dname_match(const char *name1, int len1, 
		     const char *name2, int len2)
{
  if (name1[len1 - 1] == '.')
    --len1;

  if (name2[len2 - 1] == '.')
    --len2;

  if (len1 != len2)
    return 0;

  return !strncasecmp(name1, name2, len1);
}

/*
 * '*expanded_len' must be zero before calling this function
 *
 * Returns -1 on failure, 0 on success
 */
static int dname_expand(const ruli_uint8_t *src_msg, 
			const ruli_uint8_t *src_msg_pastend,
			ruli_uint8_t       *dst_buf,
			const ruli_uint8_t *dst_buf_pastend, 
			const ruli_uint8_t *src_label, 
			size_t             *expanded_len,
                        int                remaining_depth)
{
  int len, len_plus_1;

  assert(src_msg <= src_msg_pastend);
  assert(src_msg <= src_label);
  assert(dst_buf <= dst_buf_pastend);
  assert(remaining_depth >= 0);

  if (src_label >= src_msg_pastend)
    return RULI_TXT_BAD;

  /*
   * Iterate over labels
   */
  for (;;) {

    /* Is the label still inside the message? */
    if (src_label >= src_msg_pastend)
      return RULI_TXT_BAD;

    len = *src_label;

    assert(0 <= len);
    assert(len <= 255);

    /* Last label? */
    if (!len) {

      if (dst_buf >= dst_buf_pastend)
	return RULI_TXT_BAD;

      /* write empty label */
      *dst_buf = '\0'; 

      ++*expanded_len;

      return RULI_TXT_OK;
    }

    /* Name compression? 0xC0 = 192 = 11000000 */
    if ((len & 0xC0) == 0xC0) {

      if (!remaining_depth)
        return RULI_TXT_BAD;

      return dname_expand(src_msg, src_msg_pastend,
			  dst_buf, dst_buf_pastend, 
			  src_msg + (ruli_pack2(src_label) & ~0xC000),
			  expanded_len, --remaining_depth);
    }

    len_plus_1 = len + 1; /* label_len + len_octet (1) */

    assert(1 <= len_plus_1);
    assert(len_plus_1 <= 256);

    /* Will (len-octet + label) fit into buffer? */
    if (dst_buf + len_plus_1 >= dst_buf_pastend)
      return RULI_TXT_BAD;
    
    /* Copy label into buffer */
    memcpy(dst_buf, src_label, len_plus_1);
    dst_buf   += len_plus_1;
    src_label += len_plus_1;

    assert((*expanded_len + len_plus_1) > *expanded_len);

    *expanded_len += len_plus_1;
  }

  /* NOT REACHED */
  assert(0);
}

int ruli_dname_expand(const ruli_uint8_t *src_msg, 
		      const ruli_uint8_t *src_msg_pastend,
		      ruli_uint8_t       *dst_buf,
		      const ruli_uint8_t *dst_buf_pastend, 
		      const ruli_uint8_t *src_label, 
		      size_t             *expanded_len)
{
  *expanded_len = 0;

  return dname_expand(src_msg, src_msg_pastend, 
		      dst_buf, dst_buf_pastend, 
		      src_label, expanded_len,
                      RULI_LIMIT_COMPRESSION_DEPTH);
}

#ifdef RULI_TXT_COMPARE_DEBUG
static void dump_dname(FILE *out, const ruli_uint8_t *dname, int dname_len)
{
  char txt[RULI_LIMIT_DNAME_TEXT_BUFSZ];
  int  txt_len;
  int  result;

  assert(sizeof(ruli_uint8_t) == sizeof(char));

  result = ruli_dname_decode(txt, RULI_LIMIT_DNAME_TEXT_BUFSZ, &txt_len,
                             (const char *) dname, dname_len);
  assert(!result);

  fprintf(out, "(txt_len=%d)%s", txt_len, txt);
}
#endif

/*
  Compares a compressed name against an uncompressed name
*/
int ruli_dname_compare(const ruli_uint8_t *comp_name,
                       const ruli_uint8_t *comp_name_msg, size_t comp_name_msg_len,
                       const ruli_uint8_t *uncomp_name, size_t uncomp_name_len)
{
  ruli_uint8_t name_buf[RULI_LIMIT_DNAME_ENCODED];
  size_t       name_len;
  int          result;

  result = ruli_dname_expand(comp_name_msg,
                             comp_name_msg + comp_name_msg_len,
                             name_buf,
                             name_buf + RULI_LIMIT_DNAME_ENCODED,
                             comp_name,
                             &name_len);
  assert(!result);

#ifdef RULI_TXT_COMPARE_DEBUG
  {
    fprintf   (stderr, "DEBUG: ruli_dname_compare(): comp_name="); 
    dump_dname(stderr, name_buf, name_len);
    fprintf   (stderr, " uncomp_name="); 
    dump_dname(stderr, uncomp_name, uncomp_name_len);
    fprintf   (stderr, "\n"); 
  }
#endif

  if (name_len != uncomp_name_len)
    return RULI_TXT_BAD;

  if (memcmp(name_buf, uncomp_name, name_len))
    return RULI_TXT_BAD;

  return RULI_TXT_OK;
}

/*
  Concatenate a pair of uncompressed, encoded domain names.
*/
int ruli_dname_concat(ruli_uint8_t *dst_buf, int dst_buf_len, int *len,
                      const ruli_uint8_t *src1, int len1,
                      const ruli_uint8_t *src2, int len2)
{
  assert(len1 > 0);
  assert(len2 > 0);
  assert(len1 < RULI_LIMIT_DNAME_ENCODED);
  assert(len2 < RULI_LIMIT_DNAME_ENCODED);

  assert(src1[len1 - 1] == '\0');
  assert(src2[len2 - 1] == '\0');

  --len1;

  if ((len1 + len2) > RULI_LIMIT_DNAME_ENCODED)
    return RULI_TXT_BAD;

  if (len1 >= dst_buf_len)
    return RULI_TXT_BAD;

  if (len2 >= dst_buf_len)
    return RULI_TXT_BAD;

  if ((len1 + len2) > dst_buf_len)
    return RULI_TXT_BAD;

  assert((len1 + len2) <= RULI_LIMIT_DNAME_ENCODED);  
  assert(len1 < dst_buf_len);
  assert(len2 < dst_buf_len);
  assert((len1 + len2) <= dst_buf_len);  

  memcpy(dst_buf, src1, len1);
  memcpy(dst_buf + len1, src2, len2);

  *len = len1 + len2;

  return RULI_TXT_OK;
}


