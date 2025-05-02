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
  $Id: ipv6.c,v 1.5 2004/06/16 23:05:58 evertonm Exp $
 */


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <ruli.h>

static void perform(int expected_good, const char *str)
{
  struct in6_addr in;
  int good;

  printf("%4s %-39s = ", expected_good ? "good" : "bad", str);

  good = ruli_inet6_aton(str, &in);

  if (good) {
    char buf[40];
    int r = ruli_inet6_snprintf(buf, 40, "%04X", &in);
    assert(r > 0);
    assert(r < 40);
    printf(buf);
  }
   
  if (!good == !expected_good) 
    printf(" ok\n");
  else
    printf(" ugh\n");
}

static void go()
{
  perform(0, "0");
  perform(0, ":");
  perform(0, "0:");
  perform(0, ":0");
  perform(0, "0:0");
  perform(0, "0:0:");
  perform(0, ":0:0");
  perform(0, "1:1:");
  perform(0, ":1:1");
  perform(0, "1:1:1:");
  perform(0, ":1:1:1");
  perform(0, "1:2:3:4:5:6:7");
  perform(0, "1:2:3:4:5:6:7:");
  perform(0, ":1:2:3:4:5:6:7");
  perform(0, "1:2:3:4:5:6:7-8");
  perform(0, "1:2:3:4:5:6:7 8");

  perform(1, "0:1:2:3:4:5:6:7");
  perform(1, "F:E:D:C:B:A:9:8");

  perform(1, "00:10:20:30:40:50:60:70");
  perform(1, "01:11:21:31:41:51:61:71");

  perform(1, "00FF:10EE:20DD:30CC:40BB:50AA:6099:7088");
  perform(1, "01FF:11EE:21DD:31CC:41BB:51AA:6199:7188");
  perform(1, "00FF:11EE:22DD:33CC:44BB:55AA:6699:7788");
  perform(1, "0FFF:1EEE:2DDD:3CCC:4BBB:5AAA:6999:7888");

  perform(1, "::");
  perform(1, "0::");
  perform(1, "::0");
  perform(1, "0::0");
  perform(1, "1::");
  perform(1, "::1");
  perform(1, "1::1");
  perform(1, "1:0::0:2");
  perform(1, "0:1::2:0");
  perform(1, "2:0:1::2:0:1");
  perform(1, "1:0::");
  perform(1, "1:2::");
  perform(1, "::1:0");
  perform(1, "::1:2");
  perform(0, "1::1:");
  perform(0, ":1::1");
  perform(0, "1:1::1:");
  perform(0, ":1::1:1");
  perform(0, "::1:");
  perform(0, ":1:");
  perform(0, ":::");
  perform(0, "1::1::1");
  perform(0, "1::1::");
  perform(0, "::1::1");
  perform(0, "::1::");
}

int main(int argc, const char **argv) 
{
  go();
  exit(0);
}

