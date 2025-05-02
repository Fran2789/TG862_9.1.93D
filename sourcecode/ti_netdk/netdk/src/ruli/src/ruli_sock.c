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
  $Id: ruli_sock.c,v 1.14 2005/06/08 22:38:28 evertonm Exp $
  */


#include <stdio.h>      /* FIXME: remove me [used for fprintf() debug] */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <ruli_sock.h>
#include "stdlib.h"
//#include "netutils.h"

#ifdef RULI_SOCK_DUMP_DEBUG
static void dump_buf(FILE *out, const ruli_uint8_t *buf, int len)
{
    int i;
    fprintf(out, " dump=%d @%u", len, (unsigned int) buf);
    for(i = 0; i < len; ++i)
      fprintf(out, " %02x", (unsigned char) buf[i]);
}
#endif

static int solve_protocol(const char *proto_name)
{
  struct protoent *pe;

  pe = getprotobyname(proto_name);
  if (!pe)
    return -1;

  return pe->p_proto;
}

/** ARRIS ADD: BEGIN                      **/
//TODO: This function needs to be in a common place so it is not defined all over the place
static int fill_ip6addr(struct in6_addr *in6addr, char *ip6str)
{
        int i=0;
        char temp[8];

        for(i=0 ;i<16; i++)
        {
                sprintf(temp,"%c%c", *(ip6str+(2*i)), *(ip6str+(2*i)+1));
                in6addr->s6_addr[i] = strtol(temp, NULL, 16);
        }
        return 0;
}
/** ARRIS ADD: END                      **/

/** ARRIS ADD: BEGIN                      **/
#ifdef INCLUDE_ARRIS_GW_TR69
int ruli_sock_create(int family, int type, const char *proto_name, char* interface)
#else
int ruli_sock_create(int family, int type, const char *proto_name)
#endif
/** ARRIS ADD: END                      **/
{
  union {
    struct sockaddr_in inet;
    struct sockaddr_in6 inet6;
  } sa;
  unsigned int sa_len = 0; /* picky compilers */
  int proto;
  int sd;
  long flags;

/** ARRIS ADD: BEGIN                      **/
#ifdef INCLUDE_ARRIS_GW_TR69
#define LINE_LENGTH 80
        char line[LINE_LENGTH];
        FILE *fp;
        char ip6addr[LINE_LENGTH];
        char if_name[16];
        char if_index[8], scope[8];
#endif
/** ARRIS ADD: END                      **/

  /*
   * Solve protocol
   */
  proto = solve_protocol(proto_name);
  if (proto == -1)
    return -1;

  /*
   * Create socket descriptor
   */
  sd = socket(family, type, proto);
  if (sd == -1)
    return -1;

  /*
   * Specify non-blocking behavior
   */
  flags = fcntl(sd, F_GETFL, 0);
  if (flags == -1) {
    close(sd);
    return -1;
  }

  if (fcntl(sd, F_SETFL, flags | O_NONBLOCK)) {
    close(sd);
    return -1;
  }

  /*
   * Bind the socket to local addresses
   */

  switch (family) {
  case PF_INET:

    sa_len = sizeof(sa.inet);
    sa.inet.sin_family      = family;
    sa.inet.sin_port        = htons(0);   /* request a local port */
    sa.inet.sin_addr.s_addr = INADDR_ANY; /* all local addresses */
    memset((char *) sa.inet.sin_zero, 0, sizeof(sa.inet.sin_zero));

    break;

  case PF_INET6:

    sa_len = sizeof(sa.inet6);
    memset(&sa.inet6, 0, sizeof(sa.inet6));

    sa.inet6.sin6_family = family;
    sa.inet6.sin6_port   = htons(0); /* request a local port */

    /* all local addresses */
    assert(sizeof(sa.inet6.sin6_addr.s6_addr) == sizeof(in6addr_any));

    memcpy(sa.inet6.sin6_addr.s6_addr, &in6addr_any, sizeof(in6addr_any));
/** ARRIS ADD: BEGIN                      **/
#ifdef INCLUDE_ARRIS_GW_TR69
    if (interface)
    {
        fp = fopen("/proc/net/if_inet6", "r");
        if (fp != NULL)
        {
            while (fgets(line, LINE_LENGTH, fp) != NULL) 
            {
                sscanf(line,"%s %s %*s %s %*s %s",ip6addr, if_index, scope, if_name);
                if(memcmp(if_name, interface, strlen(interface))!=0)
                        continue;

                if( scope[0] != '0')    // Only bind to Global IP
                        continue;

                sa.inet6.sin6_scope_id = strtol(if_index, NULL, 16);
                //NETUTILS_parse_hex(ip6addr, &(sa.inet6.sin6_addr));
                fill_ip6addr(&(sa.inet6.sin6_addr), ip6addr);
#ifdef RULI_RES_DEBUG
                fprintf(stderr, "DEBUG: %s: srcAddr=%s if_name=%s\n", __FUNCTION__, ip6addr, if_name);
#endif
            }
            fclose(fp);
        }

    }
#endif
/** ARRIS ADD: END                      **/
    break;
    
  default:
    assert(0);
  }

  if (bind(sd, (struct sockaddr *) &sa, sa_len)) {
    close(sd);
    return -1;
  }

  return sd;
}

int ruli_sock_create_udp(int family, char* interface)
{
/** ARRIS ADD: BEGIN                      **/
#ifdef INCLUDE_ARRIS_GW_TR69
  int sd = ruli_sock_create(family, SOCK_DGRAM, "udp", interface);
#else
  int sd = ruli_sock_create(family, SOCK_DGRAM, "udp");
#endif
/** ARRIS ADD: END                      **/

    if (sd == -1)
        return -1;

    if(interface && strlen(interface) > 0) 
    {
        /*
         * Bind the socket to interface specified. requirement
         * for multiple logical IP stacks.
        */
        {
            if (setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface) + 1) < 0) {
                int result = close(sd);
                assert(!result);
                return -1;
            }
        }
#ifdef RULI_RES_DEBUG
        fprintf(stderr, "DEBUG: %s: src interface: %s\n", __FUNCTION__, interface);
#endif
    }
  return sd;
}

int ruli_sock_create_tcp(int family, char* interface)
{
    int sd = -1;

/** ARRIS ADD: BEGIN                      **/
#ifdef INCLUDE_ARRIS_GW_TR69
    sd = ruli_sock_create(family, SOCK_STREAM, "tcp", interface);
#else
    sd = ruli_sock_create(family, SOCK_STREAM, "tcp");
#endif
/** ARRIS ADD: END                      **/

    if(interface && strlen(interface) > 0) 
    {
        if (sd == -1)
            return -1;

        /*
         * Bind the socket to interface specified. requirement
         * for multiple logical IP stacks.
        */
        {
            if (setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface) + 1) < 0) {
                int result = close(sd);
                assert(!result);
                return -1;
            }
        }
   }

   return sd;
}

/*
  Return true if socket has successfully connected
 */
int ruli_sock_has_connected(int tcp_sd)
{
  int       optval;
  socklen_t optlen = sizeof(optval);
  int       result = getsockopt(tcp_sd, SOL_SOCKET, SO_ERROR, 
				&optval, &optlen);

  assert(!result);
  assert(optlen == sizeof(optval));

  return !optval;
}

/*
  BEWARE: 
  - *sa must be large enough to hold either PF_INET or PF_INET6
  - *sa_len is IN for *sa size and OUT for *addr size
 */
void ruli_sock_set_sockaddr(struct sockaddr *sa, int *sa_len, 
			    const _ruli_addr *inet, int family,
			    int port)
{
  assert(sa_len);

  switch (family) {
  case PF_INET:
    {
      struct sockaddr_in *ad = (struct sockaddr_in *) sa;

      assert(*sa_len >= sizeof(*ad));

      ad->sin_family = family;
      ad->sin_port   = htons(port);
      ad->sin_addr   = inet->ipv4;
      memset((char *) ad->sin_zero, 0, sizeof(ad->sin_zero));

      *sa_len = sizeof(*ad);
    }
    break;

  case PF_INET6:
    {
      struct sockaddr_in6 *ad = (struct sockaddr_in6 *) sa;

      assert(*sa_len >= sizeof(*ad));

      *sa_len = sizeof(*ad);
      memset(ad, 0, *sa_len);

      ad->sin6_family = family;
      ad->sin6_port   = htons(port);
      ad->sin6_addr   = inet->ipv6;
    }
    break;
    
  default:
    assert(0);
  }
}

int ruli_sock_connect(int sd, const ruli_addr_t *remote_addr, int remote_port)
{
  union {
    struct sockaddr_in  inet;
    struct sockaddr_in6 inet6;
  } sa;
  int sa_len = sizeof(sa);
  int result;

  assert(sizeof(sa) > sizeof(struct sockaddr));

  ruli_sock_set_sockaddr((struct sockaddr *) &sa, &sa_len,
			 &remote_addr->addr, remote_addr->addr_family,
			 remote_port);

  assert((size_t) sa_len >= sizeof(sa.inet));
  assert((size_t) sa_len <= sizeof(sa.inet6));

  result = connect(sd, (struct sockaddr *) &sa, sa_len);

#ifdef RULI_RES_DEBUG
  fprintf(stderr, "DEBUG: connect(): result=%d errno=%d\n", result, errno);
#endif

  if (!result)
    return RULI_SOCK_OK;
  
  if (errno == EINPROGRESS)
    return RULI_SOCK_WOULD_BLOCK;

  return RULI_SOCK_CONNECT_FAIL;
}

int ruli_sock_sendto(int sd, const ruli_addr_t *rem_addr, int rem_port,
		     const ruli_uint8_t *buf, int msg_len)
{
  union {
    struct sockaddr_in  inet;
    struct sockaddr_in6 inet6;
  } sa;
  int sa_len = sizeof(sa);
  int wr;

  assert(sizeof(sa) > sizeof(struct sockaddr));

  ruli_sock_set_sockaddr((struct sockaddr *) &sa, &sa_len,
			 &rem_addr->addr, rem_addr->addr_family,
			 rem_port);

  assert((size_t) sa_len >= sizeof(sa.inet));
  assert((size_t) sa_len <= sizeof(sa.inet6));

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: ruli_sock_sendto(): fd=%d len=%d dst=",
	  sd, msg_len);
  ruli_addr_print(stderr, rem_addr);
  fprintf(stderr, ":%d", rem_port);
#ifdef RULI_SOCK_DUMP_DEBUG
  dump_buf(stderr, buf, msg_len);
#endif
  fprintf(stderr, "\n");
#endif

  wr = sendto(sd, buf, msg_len, 0, (struct sockaddr *) &sa, sa_len);
  if (wr != msg_len) {

    assert(wr == -1);

    if (errno == EWOULDBLOCK)
      return RULI_SOCK_WOULD_BLOCK;

    if (errno == EAGAIN)
      return RULI_SOCK_WOULD_BLOCK;

    return RULI_SOCK_SEND_FAIL;
  }

  return RULI_SOCK_OK;
}

int ruli_sock_send(int sd, const ruli_uint8_t *buf, int msg_len)
{
  int wr;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, "DEBUG: ruli_sock_send(): len=%d", msg_len);
#ifdef RULI_SOCK_DUMP_DEBUG
  dump_buf(stderr, buf, msg_len);
#endif
  fprintf(stderr, "\n");
#endif

  wr = send(sd, buf, msg_len, 0);
  if (wr != msg_len) {

    assert(wr == -1);

    if (errno == EWOULDBLOCK)
      return RULI_SOCK_WOULD_BLOCK;

    if (errno == EAGAIN)
      return RULI_SOCK_WOULD_BLOCK;

    return RULI_SOCK_SEND_FAIL;
  }

  return RULI_SOCK_OK;
}

int ruli_sock_recvfrom(int sd, ruli_uint8_t *buf, int buf_size, int *msg_len, 
		       struct sockaddr *sa, socklen_t *sa_len)
{
  int rd;

  assert(buf_size > 0);

  rd = recvfrom(sd, buf, buf_size, 0, sa, sa_len);
  if (rd == -1) {
    if (errno == EWOULDBLOCK)
      return RULI_SOCK_WOULD_BLOCK;

    if (errno == EAGAIN)
      return RULI_SOCK_WOULD_BLOCK;

    return RULI_SOCK_RECV_FAIL;
  }

  assert(rd >= 0);
  assert(rd <= buf_size);

  if (msg_len)
    *msg_len = rd;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: ruli_sock_recvfrom(): recv_len=%d buf_size=%d", 
	  rd, buf_size);
#ifdef RULI_SOCK_DUMP_DEBUG
  dump_buf(stderr, buf, rd);
#endif
  fprintf(stderr, "\n");
#endif

  return RULI_SOCK_OK;
}

int ruli_sock_recv(int sd, ruli_uint8_t *buf, int buf_size, int *msg_len)
{
  int rd;

  assert(buf_size > 0);

  rd = recv(sd, buf, buf_size, 0);
  if (rd == -1) {
    if (errno == EWOULDBLOCK)
      return RULI_SOCK_WOULD_BLOCK;

    if (errno == EAGAIN)
      return RULI_SOCK_WOULD_BLOCK;

    return RULI_SOCK_RECV_FAIL;
  }

  assert(rd >= 0);
  assert(rd <= buf_size);

  if (msg_len)
    *msg_len = rd;

#ifdef RULI_RES_DEBUG
  fprintf(stderr, 
	  "DEBUG: ruli_sock_recv(): recv_len=%d buf_size=%d", 
	  rd, buf_size);
#ifdef RULI_SOCK_DUMP_DEBUG
  dump_buf(stderr, buf, rd);
#endif
  fprintf(stderr, "\n");
#endif

  if (!rd)
    return RULI_SOCK_CLOSED;

  return RULI_SOCK_OK;
}


