#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

static const struct prefixlist {
  struct in6_addr prefix;
  unsigned int bits;
  int val;
} default_label[] = {
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,1     }, 128, 0 },
  { { 0x20,0x02,0,0, 0,0,0,0, 0,0,0,0,       0,0,0,0     }, 16,  2 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,0     }, 96,  3 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0xff,0xff, 0,0,0,0     }, 96,  4 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,0     }, 0,   1 },
  { { 0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff }, 0, 0 }
};

static const struct prefixlist default_precedence[] = {
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,1     }, 128, 50 },
  { { 0x20,0x02,0,0, 0,0,0,0, 0,0,0,0,       0,0,0,0     }, 16,  30 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,0     }, 96,  20 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0xff,0xff, 0,0,0,0     }, 96,  10 },
  { { 0,0,0,0,       0,0,0,0, 0,0,0,0,       0,0,0,0     }, 0,   40 },
  { { 0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff }, 0, 0 }
};

static void show(const char *str, const struct prefixlist *prefix)
{
  char buf[40];

  if (!inet_ntop(AF_INET6, &prefix->prefix, buf, sizeof buf)) {
    fprintf(stderr, "inet_ntop: %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  printf("%10s %40s/%3d %3d\n", str, buf, prefix->bits, prefix->val);
}

int main()
{
  int i;

  for (i = 0; i < 6; ++i)
    show("label", &default_label[i]);

  for (i = 0; i < 6; ++i)
    show("precedence", &default_precedence[i]);

  exit(0);
}
