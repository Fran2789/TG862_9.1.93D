#include "udns.h"
void udns_set_interface(char *name);
void udns_set_conf_path(char *path);
unsigned long int udns_resolve_a4(char *name);
int udns_resolve_a6(char *name, const struct in6_addr *addr);
struct in6_addr* udns_resolve_a6_multi(char *name, int *num);
struct in6_addr* udns_resolve_a6_multi_with_ttl(char *name, int *num, unsigned *ttl);    // UNIHAN ADD FOR PROD00221726
int bind_if(int sockfd,char *ifname);
unsigned long int * udns_resolve_a4_multi(char *name, int *num);
