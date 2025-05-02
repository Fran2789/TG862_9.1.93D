/* static_leases.h */
#ifndef _STATIC_LEASES_H
#define _STATIC_LEASES_H

#include "dhcpd.h"

/* Config file will pass static lease info to this function which will add it
 * to a data structure that can be searched later */
void addStaticLease(struct static_lease **lease_struct, uint8_t *mac, uint32_t ip);

/* Check to see if a mac has an associated static lease */
uint32_t getIpByMac(struct static_lease *lease_struct, void *mac);

/* Check to see if an ip is reserved as a static ip */
int reservedIp(struct static_lease *lease_struct, uint32_t ip);

#if 1
/* Print out static leases just to check what's going on */
void printStaticLeases(struct static_lease **lease_struct);
#endif

#endif



