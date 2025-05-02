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
  $Id: ruli-getaddrinfo.c,v 1.8 2005/08/30 13:40:28 evertonm Exp $
 */


#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ruli.h>

#include "parse_options.h"


const int INBUFSZ = 1024;

const char *prog_name;


static void solve(const char *fullname)
{
  int  name_len = strlen(fullname);
  char name[name_len + 1];

  char *txt_service;
  int  txt_service_len;
  char *txt_domain;
  int  txt_domain_len;

  memcpy(name, fullname, name_len + 1);

  /*
   * Split full domain name in service + domain
   * Example: _http._tcp.domain => _http._tcp + domain
   */

  {
    int  name_len  = strlen(name);
    char *past_end = name + name_len;
    char *i        = name;

    assert(name_len > 0);
    assert(name_len < INBUFSZ);

    if (*i != '_') {
      fprintf(stderr, 
	      "%s: solve(): could not match _service\n",
	      prog_name);
      
      return;
    }

    /*
     * Find domain
     */
    for (; i < past_end; ++i) {
      if (*i == '.') {
	++i;
	if (i < past_end) {
	  if (*i != '_')
	    break;
	}
      }
    }

    if (i >= past_end) {
      fprintf(stderr, 
	      "%s: solve(): could not split service/domain\n",
	      prog_name);
      
      return;
    }

    txt_service     = name;
    txt_service_len = i - name - 1;
    txt_domain      = i;
    txt_domain_len  = past_end - i;

    txt_service[txt_service_len] = '\0';
  }

  /*
   * Submit query
   */
  {
    const int BUFSZ = RULI_LIMIT_LABEL_HIGH + 1; /* = 64 */
    struct addrinfo hints;
    struct addrinfo *ai_res;
    char service[BUFSZ];
    struct protoent *pe;
    char *i, *j;
    int result;

    /*
     * from: txt_service = "_smtp._tcp"
     * make: service = "smtp"
     */
    i = txt_service;
    assert(*i == '_');
    ++i;
    assert(i < (txt_service + txt_service_len));
    j = (char *) memchr(i, '.', txt_service_len - (i - txt_service));
    assert(j);
    assert(*j == '.');
    memcpy(service, i, j - i);
    service[j - i] = '\0';

    /*
     * j = "._tcp";
     */
    if (!strcasecmp(j, "._tcp"))
      hints.ai_socktype = SOCK_STREAM;
    else if (!strcasecmp(j, "._udp"))
      hints.ai_socktype = SOCK_DGRAM;
    else {
      printf("%s bad-socket-type: %s\n", fullname, j);
      return;
    }

    j += 2;
    assert(j < (txt_service + txt_service_len));
    /*
     * j = "tcp";
     */

    pe = getprotobyname(j);
    if (!pe) {
      printf("%s bad-protocol: %s\n", fullname, j);
      return;
    }

    hints.ai_protocol = pe->p_proto;
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = PF_UNSPEC;
    hints.ai_addrlen = 0;
    hints.ai_addr = 0;
    hints.ai_canonname = 0;

    result = ruli_getaddrinfo(txt_domain, service, &hints, &ai_res);
    if (result) {
      printf("%s getaddrinfo-failed: %s\n", fullname, gai_strerror(result));
      return;
    }

    /* show addresses */
    {
      struct addrinfo *ai;

      for (ai = ai_res; ai; ai = ai->ai_next) {

	printf(fullname);

	switch (ai->ai_family) {
	case PF_INET:
	  {
	    struct sockaddr_in *sa = (struct sockaddr_in *) ai->ai_addr;

	    assert(sizeof(*sa) <= ai->ai_addrlen);

	    printf(" canon=%s port=%d IPv4/%s\n",
		   ai->ai_canonname, ntohs(sa->sin_port),
		   inet_ntoa(sa->sin_addr));
	  }
	  break;

	case PF_INET6:
	  {
	    struct sockaddr_in6 *sa = (struct sockaddr_in6 *) ai->ai_addr;

	    assert(sizeof(*sa) <= ai->ai_addrlen);

	    printf(" canon=%s port=%d IPv6/", 
		   ai->ai_canonname, ntohs(sa->sin6_port));
	    ruli_inet6_print(stdout, &sa->sin6_addr);
	    printf("\n");
	  }
	  break;

	default:
	  assert(0);
	}

      } /* scan list */

    } /* show addresses */

    ruli_freeaddrinfo(ai_res);

  } /* submit query */

}

static void go()
{
  char inbuf[INBUFSZ];

  /*
   * Scan stdin
   */
  for (;;) {
    if (!fgets(inbuf, INBUFSZ, stdin)) {
      if (feof(stdin))
	break;

      fprintf(stderr, 
	      "%s: reading from stdin: %s\n", 
	      prog_name, strerror(errno));

      continue;
    }

    /*
     * Scan tokens
     */
    {
      const char *SEP = "\r\n\t ";
      char       *ptr;
      char       *tok;

      tok = strtok_r(inbuf, SEP, &ptr);
      if (!tok)
	continue;

      for (;;) {

	/*
	 * Make SRV query for token
	 */
	solve(tok);

	tok = strtok_r(0, SEP, &ptr);
	if (!tok)
	  break;
      } /* for */

    } /* Scan tokens */

  }
}


int main(int argc, const char *argv[]) 
{
  prog_name = argv[0];

  ruli_getaddrinfo_setoptions(parse_options(argc, argv, 1));

  go();

  exit(0);
}

