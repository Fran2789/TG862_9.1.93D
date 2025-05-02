#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "udns.h"

int main(int argc,char **argv)
{
	struct dns_rr_a4 *rr;
	struct dns_ctx *nctx = NULL;
	struct dns_rr_a6 *rr6;
	char *str;
	int j;	
	
	if(argc < 2){
		printf("wrong params!!!\n");
		return -1;
	}
		
	
	if (dns_init(NULL, 0, 0) < 0 || !(nctx = dns_new(NULL))){
        	printf("unable to initialize dns init\n");
		return -1;
	}
	
	if (dns_open(nctx) < 0){
        	printf("unable to initialize dns context\n");
		return -1;
	}	    

	
#if 1
	rr6 = dns_resolve_a6(nctx, argv[1], 0);
	if (!rr6){
		printf("unable to resolve nameserver %s: %s\n",argv[1], dns_strerror(dns_status(nctx)));
	  return -1;
	}
  inet_ntop(AF_INET6, rr6->dnsa6_addr, str, INET6_ADDRSTRLEN);
  printf("resolved v6 ip : %s\n", str);

  free(rr6);  

#else

	rr = dns_resolve_a4(nctx, argv[1], 0);
        if (!rr){
          printf("unable to resolve nameserver %s: %s\n",argv[1], dns_strerror(dns_status(nctx)));
	  return -1;
	}

        for(j = 0; j < rr->dnsa4_nrr; ++j) {
     printf("resolved ip : %s\n", inet_ntoa(rr->dnsa4_addr[j]));
        }
        free(rr);
#endif
   
	dns_free(nctx);
	return 0;
}
