#include <stdio.h>
#include <arpa/inet.h>
#include "udns_api.h"

int main(int argc, char **argv) 
{
    struct in6_addr* addrs=NULL;
    struct in6_addr* addrs_list=NULL;
    char str_ipv6_addr[INET6_ADDRSTRLEN];
    int num=0;
    int i=0;
    // UNIHAN ADD START FOR PROD00221726
    unsigned ttl=0;
    int with_ttl=0;
    // UNIHAN ADD END FOR PROD00221726

    if(argc<1) {
        return 0;
    }  
    
    // UNIHAN MOD START FOR PROD00221726
    if(getopt(argc, argv, "t") == 't')
    {
        with_ttl = 1;
    }

    addrs_list = udns_resolve_a6_multi_with_ttl(argv[1], &num, &ttl);
    //addrs_list = udns_resolve_a6_multi(argv[1], &num);
    //printf("domaina=[%s], num=[%d]\n", argv[1], num);
    // UNIHAN MOD END FOR PROD00221726

    for(i=0; i<num; i++) {
        addrs = addrs_list + i;
        if( inet_ntop(AF_INET6, addrs, str_ipv6_addr, INET6_ADDRSTRLEN) == NULL) {
            printf("bad IPv6\n");
        }
        else {
            // UNIHAN MOD START FOR PROD00221726
            if(with_ttl == 1)
            {
                printf("%s. AAAA %u %s\n", argv[1], ttl, str_ipv6_addr);
                return 0;
            }
            else
            {
            printf("IPv6 : [%s]\n", str_ipv6_addr);
        }
            // UNIHAN MOD END FOR PROD00221726
        }
    }

    return 1;
}

