#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
//#include "udns.h"
#include "udns_api.h"
extern char udns_conf_path[];
//extern char interface[];

void udns_set_interface(char *name)
{
//	strcpy(interface,name);
}
void udns_set_conf_path(char *path)
{
	strcpy(udns_conf_path,path);
}

int udns_resolve_a6(char *name, const struct in6_addr *addr)
{
	struct dns_rr_a6 *rr6;
	struct dns_ctx *nctx = NULL;
	
	if(!name)
		return 0;
	
	if (dns_init(NULL, 0, 0) < 0 || !(nctx = dns_new(NULL))){
		return 0;
	}
	
	if (dns_open(nctx) < 0){
		return 0;
	}	  
	
	rr6 = dns_resolve_a6(nctx, name, 0);
	
	if (!rr6){
	  dns_free(nctx);
	  return 0;
	}

	memcpy(addr, &rr6->dnsa6_addr[0], sizeof(rr6->dnsa6_addr[0]));

	free(rr6);
	dns_free(nctx);
	
	return 1;
}

// UNIHAN ADD START FOR PROD00221726
struct in6_addr* udns_resolve_a6_multi_with_ttl(char *name, int *num, unsigned *ttl)
{
    struct dns_rr_a6 *rr6;
    struct dns_ctx *nctx = NULL;
    int i=0;
    struct in6_addr *addr_list=NULL;

    if(!name)
        return NULL;

    if (dns_init(NULL, 0, 0) < 0 || !(nctx = dns_new(NULL))){
        return NULL;
    }

    if (dns_open(nctx) < 0){
        return NULL;
    }

    rr6 = dns_resolve_a6(nctx, name, 0);

    if (!rr6){
        dns_free(nctx);
        return NULL;
    }

    *num = rr6->dnsa6_nrr;
    *ttl = rr6->dnsa6_ttl;
    addr_list = malloc(sizeof(struct in6_addr) * (*num));
    if( addr_list == NULL ) {
        return NULL;
    }
    memset(addr_list, 0, sizeof(addr_list));

    for(i=0; i<*num; i++){
        memcpy( (addr_list+i), &(rr6->dnsa6_addr[i]), sizeof(rr6->dnsa6_addr[i])); 
    }

    free(rr6);
    dns_free(nctx);

    return addr_list;
}
// UNIHAN ADD END FOR PROD00221726

struct in6_addr* udns_resolve_a6_multi(char *name, int *num)
{
	struct dns_rr_a6 *rr6;
	struct dns_ctx *nctx = NULL;
	int i=0;
	struct in6_addr *addr_list=NULL;

	if(!name)
		return NULL;

	if (dns_init(NULL, 0, 0) < 0 || !(nctx = dns_new(NULL))){
		return NULL;
	}

	if (dns_open(nctx) < 0){
		return NULL;
	}

	rr6 = dns_resolve_a6(nctx, name, 0);

	if (!rr6){
		dns_free(nctx);
		return NULL;
	}

	*num = rr6->dnsa6_nrr;
	addr_list = malloc(sizeof(struct in6_addr) * (*num));
	if( addr_list == NULL ) {
		return NULL;
	}
	memset(addr_list, 0, sizeof(addr_list));

	for(i=0; i<*num; i++){
		memcpy( (addr_list+i), &(rr6->dnsa6_addr[i]), sizeof(rr6->dnsa6_addr[i])); 
	}

	free(rr6);
	dns_free(nctx);

	return addr_list;
}

unsigned long int udns_resolve_a4(char *name)
{
	struct dns_rr_a4 *rr;
	struct dns_ctx *nctx = NULL;
	unsigned long int s_addr=0;
	if(!name)
		return 0;
	if (dns_init(NULL, 0, 1) < 0 || !(nctx = dns_new(NULL))){
		return 0;
	}
	if (dns_open(nctx) < 0){
		return 0;
	}	    
	rr = dns_resolve_a4(nctx, name, 0);
        if (!rr){
	  dns_free(nctx);
	  return 0;
	}
	s_addr=rr->dnsa4_addr[0].s_addr;
	free(rr);
	dns_free(nctx);
	return s_addr;
}

//void udns_resolve_a4_multi(char *name, unsigned long int *s_addr, int *num)
unsigned long int * udns_resolve_a4_multi(char *name, int *num)
{
	struct dns_rr_a4 *rr;
	struct dns_ctx *nctx = NULL;
	int i;
	unsigned long int *s_addr;
	
	if(!name)
		return NULL;
	
	if (dns_init(NULL, 0, 1) < 0 || !(nctx = dns_new(NULL))){
		return NULL;
	}
	
	if (dns_open(nctx) < 0){
		return NULL;
	}	    
	
	rr = dns_resolve_a4(nctx, name, 0);
        if (!rr){
	  dns_free(nctx);
	  return NULL;
	}
	
	*num = rr->dnsa4_nrr;
	s_addr = malloc(sizeof(unsigned long int) * (*num));
	if (s_addr == NULL) {
		return NULL; 
	}
	memset(s_addr, 0, sizeof(s_addr));
	
	for(i=0; i<*num; i++){
		*(s_addr+i) = rr->dnsa4_addr[i].s_addr;
	}
	free(rr);
	dns_free(nctx);
	
	return s_addr;
}

int bind_if(int sockfd,char *ifname)
{
	struct sockaddr_in sa_recv;
	struct sockaddr_in *saddr;
    	int sock=-1;
	struct ifreq ifr;

	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1){
	        return -1;
	}
    	strcpy(ifr.ifr_name, ifname);
    	ifr.ifr_addr.sa_family = AF_INET;

	if (ioctl(sock, SIOCGIFADDR, &ifr)<0) {
       		close(sock);
        	return -1;
	}
    	close(sock);
	bzero((char *) &sa_recv, sizeof(sa_recv));
	sa_recv.sin_family=AF_INET;
        saddr = (struct sockaddr_in *)&ifr.ifr_addr;
        sa_recv.sin_addr.s_addr= saddr->sin_addr.s_addr;
	
	if(bind(sockfd,(struct sockaddr *) &sa_recv,sizeof(sa_recv)) == -1) {
		return -1;
	}
	return 0;
}
