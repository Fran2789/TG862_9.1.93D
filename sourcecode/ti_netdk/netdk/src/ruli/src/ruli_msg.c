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
  $Id: ruli_msg.c,v 1.8 2003/02/13 05:57:46 evertonm Exp $
 */


#include <stdio.h>    /* FIXME: remove me [used for fprintf() debug] */

#include <assert.h>
#include <string.h>

#include <ruli_msg.h>
#include <ruli_txt.h>


#ifdef RULI_MSG_DUMP_DEBUG
static void dump_buf(FILE *out, ruli_uint8_t *buf, int len)
{
    int i;
    fprintf(out, " dump=%d @%u", len, (unsigned int) buf);
    for(i = 0; i < len; ++i)
      fprintf(out, " %02x", (unsigned char) buf[i]);
}
#endif

static ruli_uint16_t build_options(ruli_uint16_t option_qr, 
				   ruli_uint16_t option_opcode, 
				   ruli_uint16_t option_aa, 
				   ruli_uint16_t option_tc, 
				   ruli_uint16_t option_rd, 
				   ruli_uint16_t option_ra, 
				   ruli_uint16_t option_z,
				   ruli_uint16_t option_rcode)
{
  return \
    option_qr     << RULI_MSG_OFFSET_QR     | \
    option_opcode << RULI_MSG_OFFSET_OPCODE | \
    option_aa     << RULI_MSG_OFFSET_AA     | \
    option_tc     << RULI_MSG_OFFSET_TC     | \
    option_rd     << RULI_MSG_OFFSET_RD     | \
    option_ra     << RULI_MSG_OFFSET_RA     | \
    option_z      << RULI_MSG_OFFSET_Z      | \
    option_rcode  << RULI_MSG_OFFSET_RCODE;
}

int ruli_msg_build(ruli_uint8_t *buf, int buf_size,
		   int *msg_len, ruli_uint16_t query_id, 
		   const ruli_uint8_t *dname, int dname_len,
		   ruli_uint16_t qclass, ruli_uint16_t qtype)
{
  ruli_uint8_t  *i;
  ruli_uint16_t options;
  
#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: %s: %s(): buf_size=%d query_id=%d "
	  "dname_from=%u dname_len=%d "
	  "qclass=%d qtype=%d\n", 
	  __FILE__, __PRETTY_FUNCTION__,
	  buf_size, query_id, (unsigned int) dname, dname_len, qclass, qtype);

  {
    char txt[RULI_LIMIT_DNAME_TEXT_BUFSZ];
    int  txt_len;
    int  result;
    
    result = ruli_dname_decode(txt, RULI_LIMIT_DNAME_TEXT_BUFSZ,
			       &txt_len,
			       (const char *) dname,
			       dname_len);
    assert(!result);
    
    fprintf(stderr,
	    "DEBUG: %s: %s(): building name=%s len=%d\n",
	    __FILE__, __PRETTY_FUNCTION__,
	    txt, txt_len);
  }
#endif /* RULI_RES_DEBUG */

  /*
    Buffer must support largest possible message
   */
  if (buf_size < RULI_LIMIT_MSG_HIGH)
    return RULI_MSG_SHORT_BUF;

  options = build_options(/* qr     = query      */ 0, \
			  /* opcode = standard   */ RULI_OPCODE_QUERY, \
			  /* aa     = don't care */ 0, \
			  /* tc     = false      */ 0, \
			  /* rd     = true       */ 1, \
			  /* ra     = don't care */ 0, \
			  /* z      = 0          */ 0, \
			  /* rcode  = don't care */ 0);
  
  i = ruli_unpack2(buf, query_id);

  i = ruli_unpack2(i, options);

  i = ruli_unpack2(i, /* qdcount */ 1);
  i = ruli_unpack2(i, /* ancount */ 0);
  i = ruli_unpack2(i, /* nscount */ 0);
  i = ruli_unpack2(i, /* arcount */ 0);

  memcpy(i, dname, dname_len);
  i += dname_len;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, "DEBUG: ruli_msg_build(): wrote_dname_len=%d at %u\n", 
	  dname_len, (unsigned int) (i - dname_len));
#endif

  i = ruli_unpack2(i, qtype);
  i = ruli_unpack2(i, qclass);

  /*
   * Message is finished, let's calculate its length
   */

  {
    int len = i - buf;

#ifdef RULI_RES_DEBUG
    fprintf(stderr, "DEBUG: ruli_msg_build(): wrote_msg_len=%d at %u\n", 
	    len, (unsigned int) buf);
#endif
    
    assert(len >= RULI_LIMIT_MSG_LOW);
    assert(len <= RULI_LIMIT_MSG_HIGH);
    assert(len <= buf_size);
    
    if (msg_len)
      *msg_len = len;

#ifdef RULI_MSG_DUMP_DEBUG
      fprintf(stderr, "DEBUG: ruli_msg_build():");
      dump_buf(stderr, buf, len);
      fprintf(stderr, "\n");
#endif
  }

  return RULI_MSG_OK;
}

int ruli_msg_parse_header(ruli_msg_header_t *msg_hdr, 
			  const ruli_uint8_t *msg, size_t msg_len)
{
  /* Message too short? */
  if (msg_len < RULI_LIMIT_MSG_HEADER)
    return RULI_MSG_PARSE_SHORT;

  msg_hdr->id      = ruli_pack2(msg);
  msg_hdr->flags   = ruli_pack2(msg + 2);
  msg_hdr->rcode   = msg[3] & 0xF;
  msg_hdr->qdcount = ruli_pack2(msg + 4);
  msg_hdr->ancount = ruli_pack2(msg + 6);
  msg_hdr->nscount = ruli_pack2(msg + 8);
  msg_hdr->arcount = ruli_pack2(msg + 10);

  return RULI_MSG_OK;
}

