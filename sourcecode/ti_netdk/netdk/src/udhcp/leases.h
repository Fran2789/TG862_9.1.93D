/* leases.h */
/*-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
// 1. Added hostname to dhcpOfferedAddr
// 2. ARRIS Added adptype to dhcpOfferedAddr 
// 3. Added deviceManufacturerOUI, deviceSerialNumber and deviceProductClass to dhcpOfferedAddr
//
// THIS MODIFIED SOFTWARE AND DOCUMENTATION ARE PROVIDED
// "AS IS," AND TEXAS INSTRUMENTS MAKES NO REPRESENTATIONS
// OR WARRENTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO, WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
// DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS,
// COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
//
// These changes are covered as per original license.
//-------------------------------------------------------------------------------------*/

#ifndef _LEASES_H
#define _LEASES_H

//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
#include "options.h"
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

//#define INF_LEASETIME  	604800
/* Change Description:07112006
 * 1. Modified functions to include classindex to indicate correct server_config
 */
//ARRIS ADD START
#define DHCP_LAN_CLIENT_UNKNOWN     (0)
#define DHCP_LAN_CLIENT_EPHY        (1)
#define DHCP_LAN_CLIENT_MOCA        (2)
#define DHCP_LAN_CLIENT_WIFI        (3)
//ARRIS ADD END

//UNIHAN ADD
/* macaddr, ip address, lease time, host name, adptype */
#define DHCP_HOSTFILE_PARAMETERCOUNT_INCLUDE_ADPTYPE    5

#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
/* macaddr, ip address, lease time, host name, adptype
 * device manufacturer OUI, device serial number and device product class. 
 */
#define DHCP_HOSTFILE_PARAMETERCOUNT_INCLUDE_DEVICEID    8
#define ARRIS_NULL_STRING "null"
#define ARRIS_EMPTY_STRING ""
#define ARRIS_ROUTER_LAN_DHCP_LEASE_OPT125_MAX_SIZE 256
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

struct dhcpOfferedAddr {
    /* Make sure that the array start is word aligned - required for ARM. */
    u_int8_t chaddr[16]; __attribute__ ((aligned));
    u_int32_t yiaddr;	/* network order */
    u_int32_t expires;	/* host order */
    u_int8_t hostname[50];
};

extern unsigned char blank_chaddr[];

void clear_lease(u_int8_t *chaddr, u_int32_t yiaddr, int ifid,int classindex);
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
struct dhcpOfferedAddr *add_lease(u_int8_t *chaddr, u_int32_t yiaddr, unsigned long lease, int ifid,int classindex,u_int8_t *hname, unsigned int adptype, struct dhcp_option125_info *opt125);
#else
struct dhcpOfferedAddr *add_lease(u_int8_t *chaddr, u_int32_t yiaddr, unsigned long lease, int ifid,int classindex,u_int8_t *hname);
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
int lease_expired(struct dhcpOfferedAddr *lease,int ifid,int classindex);
struct dhcpOfferedAddr *oldest_expired_lease(int ifid,int classindex);
struct dhcpOfferedAddr *find_lease_by_chaddr(u_int8_t *chaddr, int ifid,int classindex);
struct dhcpOfferedAddr *find_lease_by_yiaddr(u_int32_t yiaddr, int ifid,int classindex);
u_int32_t find_address(int check_expired, int ifid,int classindex);
int check_ip(u_int32_t addr, int ifid, int classindex);

#endif
