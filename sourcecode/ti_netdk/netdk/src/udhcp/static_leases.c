/*
 * static_leases.c -- Couple of functions to assist with storing and
 * retrieving data for static leases
 *
 * Wade Berrier <wberrier@myrealbox.com> September 2004
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "static_leases.h"
#include "dhcpd.h"

/* Takes the address of the pointer to the static_leases linked list,
 *   Address to a 6 byte mac address
 *   Address to a 4 byte ip address */
void addStaticLease(struct static_lease **lease_struct, uint8_t *mac, uint32_t ip)
{
	struct static_lease *new_static_lease;

	/* Build new node */
    new_static_lease = malloc(sizeof(struct static_lease));
    memcpy(new_static_lease->mac, mac, 6);
	new_static_lease->ip = ip;
	new_static_lease->next = NULL;

	/* If it's the first node to be added... */
	if(*lease_struct == NULL)
	{
		*lease_struct = new_static_lease;
	}
	else
	{
        struct static_lease *cur = *lease_struct;
        while (cur->next)
		{
			cur = cur->next;
		}

		cur->next = new_static_lease;
	}
}

/* Check to see if a mac has an associated static lease */
uint32_t getIpByMac(struct static_lease *lease_struct, void *mac)
{
    while (lease_struct) {
        if (memcmp(lease_struct->mac, mac, 6) == 0)
            return lease_struct->ip;
        lease_struct = lease_struct->next;
    }

	return 0;
}

/* Check to see if an ip is reserved as a static ip */
int reservedIp(struct static_lease *lease_struct, uint32_t ip)
{
    while (lease_struct) {
        if (lease_struct->ip == ip)
            return 1;
        lease_struct = lease_struct->next;
    }

    return 0;
}

#if 1
/* Print out static leases just to check what's going on */
/* Takes the address of the pointer to the static_leases linked list */
void printStaticLeases(struct static_lease **arg)
{
	struct static_lease *cur = *arg;

	while(cur != NULL)
	{
        printf("PrintStaticLeases: Lease mac Value: %02x:%02x:%02x:%02x:%02x:%02x\n",
            cur->mac[0], cur->mac[1], cur->mac[2],
            cur->mac[3], cur->mac[4], cur->mac[5]
        );
		printf("PrintStaticLeases: Lease ip Value: %x\n", cur->ip);

		cur = cur->next;
	}


}
#endif



