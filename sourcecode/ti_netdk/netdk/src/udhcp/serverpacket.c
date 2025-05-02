/* serverpacket.c
 *
 * Constuct and send DHCP server packets
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
// 1. Added utility function copy_till() to copy a string till the delimiter 
//    specified
// 2. Changes to support multiple interfaces sendOffer() and sendAck() functions 
//    modified to extract DHCP_HOST_NAME option field from the request and copy the 
//    same to the offer message in the received dhcp packet. This is used in serverpacket.c.
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


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <malloc.h>

#include "packet.h"
#include "debug.h"
#include "dhcpd.h"
#include "options.h"
#include "leases.h"
#include "files.h"



#include "static_leases.h" // UNIHAN ADD, to support static lease

#define LEASE_ADD     1
#define LEASE_DEL     2


/* Change Description:07112006
 * 1. Option 43 handling added in init_packet
 * 2. Modified functions to include classindex to indicate correct server_config
 */
static int copy_till(char *inp , char *dest, char sep ,unsigned int length)
{
	unsigned int i = 0;
  while( (inp[i] != 0x00) && (inp[i] != sep) && (i < length ))
  {
	 *(dest + i) = inp[i];
	 i++;
  }
  *(dest + i) = 0x00;
  return i; 	
}

/* send a packet to giaddr using the kernel ip stack */
static int send_packet_to_relay(struct dhcpMessage *payload, int ifid,int classindex)
{
	DEBUG(LOG_INFO, "Forwarding packet to relay");

	return kernel_packet(payload, server_config[ifid][classindex].server, SERVER_PORT,
			payload->giaddr, SERVER_PORT);
}

/* This function analyses the Vendor Specific Option 125, received from the Client                   */
/* Currently it looks for the Enterprise code 4491 - Cable labs                                      */
/* Inside the Enterprise code the function looks for the Suboption 1 -  DHCPv4 Option Request Option */
/* Inside Suboption 1 it looks for the Value 3 - eRouter Container                                   */
static int checkIfOption125IsRequested(char *receivedOption125)
{

//#define GW_OPT_ENTERPRISE_NUMBER 4491
//#define GW_OPT_SUB_OPTION_3 3

    int ret = 0;
    int enterpriseCodeFound;
    Uint8 *vendorOption125 = receivedOption125;
    int i,j;
    Uint8 *optionPtr = receivedOption125;  /* pointer to the received from client Option 125 */
    Uint8 optionLen = vendorOption125[1];  /* length of the received from client Option 125 */

    /* look for CableLabs enterprise code 4491 */
    /* skip opcode and oplength - point to the enterprise code */
    vendorOption125 += OPT_SUBOPTION_LEN + OPT_SUBOPTION_CODE; 

    /* look for enterprise number of cable labs 4491 */
    /* optionLen-5 is the length of the rest of the message without Enterprise code and Enterprise block length */
    /* *(vendorOption125 + i + 4) + 5 is the length of the Enterprise block plus 5 bytes of enterprise and length */
    for (i=0; i < (optionLen - (OPT_SUBOPTION_LEN + OPT_ENTERPRISE_NUMBER_LEN)); i += *(vendorOption125 + i + OPT_ENTERPRISE_NUMBER_LEN) + OPT_SUBOPTION_LEN + OPT_ENTERPRISE_NUMBER_LEN )
    {
        if (((Uint32 *)vendorOption125)[i] == OPT_ENTERPRISE_NUMBER)
        {
          enterpriseCodeFound = 1;
          vendorOption125 += i;       /* points to the enterprise number */ 
          break;
        }
    }

    if (enterpriseCodeFound == 0)  /* no CableLabs enterprise code found */
        return ret;

      vendorOption125 += OPT_ENTERPRISE_NUMBER_LEN; /* points to the length of enterprise block */

      /* get length of enterprise block of options */
      int enterpriseBlockLen = *vendorOption125;
      
      /* point to the suboption code */
      vendorOption125 += OPT_SUBOPTION_LEN;

      /* go through the block and look for the requested sub-options */
      /* enterpriseBlockLen - 2 is the length of the suboption without its type and length */
      /* *(vendorOption125 + i + 1) + 2 is the length of subooption plus its type and length */
      for (i=0; i < (enterpriseBlockLen - (OPT_SUBOPTION_LEN + OPT_SUBOPTION_CODE)); i += *(vendorOption125 + i + OPT_SUBOPTION_LEN) + OPT_SUBOPTION_LEN + OPT_SUBOPTION_CODE)
      {
        /* switch on sub-option */
        switch (*(vendorOption125 + i))
        {
        case 1: /* Suboption - Requested options */

            for (j=0; j < (*(vendorOption125 + i + OPT_SUBOPTION_LEN) ); j++)
            {
                if ( *(vendorOption125 + i + OPT_SUBOPTION_LEN + OPT_SUBOPTION_CODE + j) == CL_V4EROUTER_CONTAINER_OPTION)
                {
                    return 1;
                }
            }
            break;
        }

        return ret;
      }
}

/* Check whether the Vendor Specific Option 125 received and contains the Requested Options suboption 1 */
/* with the eRouter Container suboption code - 3. Only in this case the Container suboption in the */
/* option 125 will be added to the Server message */
static int check_and_send_vendor_specific_option(struct dhcpMessage *oldpacket, u_int8_t *packetOptions)
{
  u_int8_t *vendorSpecific;

  int occurance = 0;
  int done = 0;  

  /* check if option 125 vendor specific is requested - get multiple occurancies */
  while(!done)
  {
      vendorSpecific = get_option_multiple(oldpacket, DHCP_VENDOR_SPECIFIC, occurance);
      if (vendorSpecific)
      {
          /* check if Container suboption is requested in the Client message */
          if (checkIfOption125IsRequested(vendorSpecific - 2) == 1)
          {
              done = 1;

              /* check if vendor specific option value is already read from file */
              if (vendor125_OptionInfo == NULL)
              {
                /* if not read - read from file */
                // UNIHAN MOD START, for PROD00214575
                read_vendor_options();
                // UNIHAN MOD END
              }

              /* vendor specific option value exists */
              if (vendor125_OptionInfo != NULL)
              {
                  add_option_string(packetOptions, vendor125_OptionInfo);
                  return 1;
              }
          }
          else
          {
              occurance++;
          }
      }
      else
      {
          done = 1;
          return 0;
      }
  }

  return 0;
}

/* send a packet to a specific arp address and ip address by creating our own ip packet */
static int send_packet_to_client(struct dhcpMessage *payload, int force_broadcast, int ifid,int classindex)
{
	unsigned char *chaddr;
	u_int32_t ciaddr;
	
	if (force_broadcast) {
		DEBUG(LOG_INFO, "broadcasting packet to client (NAK)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else if (payload->ciaddr) {
		DEBUG(LOG_INFO, "unicasting packet to client ciaddr");
		ciaddr = payload->ciaddr;
		chaddr = payload->chaddr;
	} else if (ntohs(payload->flags) & BROADCAST_FLAG) {
		DEBUG(LOG_INFO, "broadcasting packet to client (requested)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else {
		DEBUG(LOG_INFO, "unicasting packet to client yiaddr");
		ciaddr = payload->yiaddr;
		chaddr = payload->chaddr;
	}
	return raw_packet(payload, server_config[ifid][classindex].siaddr, SERVER_PORT, 
			ciaddr, CLIENT_PORT, chaddr, server_config[ifid][classindex].ifindex);
}


/* send a dhcp packet, if force broadcast is set, the packet will be broadcast to the client */
static int send_packet(struct dhcpMessage *payload, int force_broadcast, int ifid,int classindex)
{
	int ret;

	if (payload->giaddr)
		ret = send_packet_to_relay(payload, ifid,classindex);
	else ret = send_packet_to_client(payload, force_broadcast, ifid,classindex);
	return ret;
}

/* ARRIS CHANGE START */
/* Option 43 must be added at the end of the list to work around a Microsoft XP problem (http://support.microsoft.com/kb/953761).
   Moved the option 43 code to a new function that will be called after all other options have been added */
static void init_packet(struct dhcpMessage *packet, struct dhcpMessage *oldpacket, char type, int ifid,int classindex)
{
	//struct option_set *new;
    //int length;
	init_header(packet, type);
	packet->xid = oldpacket->xid;
	memcpy(packet->chaddr, oldpacket->chaddr, 16);
	packet->flags = oldpacket->flags;
	packet->giaddr = oldpacket->giaddr;
	packet->ciaddr = oldpacket->ciaddr;
	add_simple_option(packet->options, DHCP_SERVER_ID, server_config[ifid][classindex].siaddr);
#if 0
        if(flag43 == 2) 
        {
            if( server_config[ifid][classindex].vendorinfo != NULL ) 
            {
                 length = strlen(server_config[ifid][classindex].vendorinfo);
                 new = malloc(sizeof(struct option_set));
                 if(new)
                 {
		              new->data = malloc(length + 2);
                      if(new->data)
                      {
		                   new->data[OPT_CODE] = DHCP_VENDOR_INFO;
		                   new->data[OPT_LEN] = length;
		                   memcpy(new->data + 2, server_config[ifid][classindex].vendorinfo, length);
                           add_option_string(packet->options, new->data);
                           
		                   free(new->data);
                       }
	                   free(new);
                  }
             }
             else
                 add_simple_option(packet->options, DHCP_VENDOR_INFO, (int)NULL);
        } 
        if(flag43 == 1)
        {
            add_simple_option(packet->options, DHCP_VENDOR_INFO, (int)NULL);
        } 
#endif
/* ARRIS CHANGE END */
}

/* ARRIS ADD START */
/* Add option 43 to the option list.  It is called after all other options are added so that option 43 is always the last one */
static void packet_option43(struct dhcpMessage *packet, int ifid,int classindex,int flag43)
{
    struct option_set *new;
    int length;

    if (flag43 == 2)
    {
        if ( server_config[ifid][classindex].vendorinfo != NULL )
        {
            length = strlen(server_config[ifid][classindex].vendorinfo);
            new = malloc(sizeof(struct option_set));
            if (new)
            {
                new->data = malloc(length + 2);
                if (new->data)
                {
                    new->data[OPT_CODE] = DHCP_VENDOR_INFO;
                    new->data[OPT_LEN] = length;
                    memcpy(new->data + 2, server_config[ifid][classindex].vendorinfo, length);
                    add_option_string(packet->options, new->data);

                    free(new->data);
                }
                free(new);
            }
        }
        /* Original code from init_packet() that sent option 43 with a value of NULL is now commented out.
           No point of sending NULL data, send only if valid vendor info is present */
        // else
            // add_simple_option(packet->options, DHCP_VENDOR_INFO, (int)NULL);
    }
    /* same as above, do not send if no valid vendor info is present */
//  if (flag43 == 1)
//  {
//      add_simple_option(packet->options, DHCP_VENDOR_INFO, (int)NULL);
//  }
}
/* Parse dhcp's request parameter list from option 55 in DHCP packet */
void packet_option55_parse(struct dhcpMessage *packet, char *device_name, int nlen) 
{
    int len = 0, found;
    char *option55 = get_option(packet, DHCP_PARAM_REQ);
    int option55_len = get_option_length(packet, DHCP_PARAM_REQ); /* ARRIS ADD PROD00223383 */
    FILE *fp = NULL;

    strncpy(device_name, "unknown device", nlen - 1);
    device_name[nlen - 1] = 0;
    
    fp = fopen(DHCP_FINGERPRINTS_CONF_FILE,"r");
    if (fp == NULL)
        return;
    if (!option55)
    {
        fclose(fp);
        return;
    }
    char *req_list = (char *)malloc(4 * option55_len); /* ARRIS ADD PROD00223383 */
    if (req_list == NULL)
    {
        fclose(fp);
        return;
    }
    memset(req_list,0,4 * option55_len);
    while (len < option55_len) /* ARRIS ADD PROD00223383 */
    {
        if (option55[len] != 255)
        {
            char itostr[4];
            sprintf(itostr,"%d,",option55[len]);
            strcat(req_list,itostr);
        }
        len++;
    }
    req_list[strlen(req_list) - 1] = '\0';

    char *comp = NULL;
    size_t num = 0;
    char *device_name_pre = NULL;
    found = 0;
    while ((len = getline(&comp,&num,fp)) > 0)
    {
        comp[strlen(comp) - 1] = '\0';
        if (len >= 3 && comp[0] == 'd' && comp[1] == 'e' && comp[2] == 's')
        {
            device_name_pre = strstr(comp,"=");
            strncpy(device_name, device_name_pre + 1, nlen - 1);
            device_name[nlen - 1] = 0;
        }
        else if (strcmp(req_list,comp) == 0)
        {
            found = 1;
            break;
        }
    }

    if (!found) 
    {
        strncpy(device_name, "unknown device", nlen - 1);
        device_name[nlen - 1] = 0;
    }
    /* ARRIS ADD START */
    if (comp) free(comp);
    /* ARRIS ADD END */
    free(req_list);
    fclose(fp);
}
/* ARRIS ADD END */

/* add in the bootp options */
static void add_bootp_options(struct dhcpMessage *packet, int ifid,int classindex)
{
	packet->siaddr = server_config[ifid][classindex].siaddr;
	if (server_config[ifid][classindex].sname)
		strncpy(packet->sname, server_config[ifid][classindex].sname, sizeof(packet->sname) - 1);
	if (server_config[ifid][classindex].boot_file)
		strncpy(packet->file, server_config[ifid][classindex].boot_file, sizeof(packet->file) - 1);
}
	

/* send a DHCP OFFER to a DHCP DISCOVER */
int sendOffer(struct dhcpMessage *oldpacket, int ifid, int classindex, int flag43)
{
	struct dhcpMessage packet;
	struct dhcpOfferedAddr *lease = NULL;
        u_int32_t req_align, lease_time_align = server_config[ifid][classindex].lease,leasetime = 0;
	unsigned char *req, *lease_time;
	struct option_set *curr;
	struct in_addr addr;
  u_int8_t hostname[50] = "unknown" , *hname;
  int length = 0;
    u_int8_t *vendorSpecific;
    u_int32_t addr32; // UNIHAN ADD

    uint32_t static_lease_ip = 0; // UNIHAN ADD
//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    bool isGetDeviceIdentity = false;
    bool isGetDslForumString = false;
    struct dhcp_option125_info broadband_specOpt;
    init_option125_info(&broadband_specOpt);
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

    init_packet(&packet, oldpacket, DHCPOFFER, ifid, classindex); /* ARRIS CHANGE - delete flag43 */

    static_lease_ip = getIpByMac(server_config[ifid][classindex].static_leases, oldpacket->chaddr); // UNIHAN ADD
	
	/* ADDME: if static, short circuit */
    // UNIHAN ADD START, to support static lease
    if( 0 == static_lease_ip )
    {
    // UNIHAN ADD END

        // UNIHAN ADD START, to support CPE aging
        /* if CPE aging is set, and time is expired, clean all expired leases. */
        if( (0 != server_config[ifid][classindex].cpe_aging_base) &&
            (server_config[ifid][classindex].cpe_aging_time < time(0)) )
        {
            addr32 = ntohl(server_config[ifid][classindex].start);
            for( ;addr32 <= ntohl(server_config[ifid][classindex].end); addr32++ )
            {
            /* ie, 192.168.55.0 */
            if( !(addr32 & 0xFF) )
                {      
                    continue;
                }

            /* ie, 192.168.55.255 */
            if( (addr32 & 0xFF) == 0xFF )
                {      
                    break;
                }

            /* lease exist and is expired */
            if( (lease = find_lease_by_yiaddr(htonl(addr32), ifid, classindex)) &&
                    lease_expired(lease, ifid, classindex) )
                {
                    clear_lease(lease->chaddr, lease->yiaddr, ifid, classindex);
                }
            }

            /* start/renew aging */
            server_config[ifid][classindex].cpe_aging_time = server_config[ifid][classindex].cpe_aging_base + time(0);
        }
        // UNIHAN ADD END

	/* the client is in our lease/offered table */
        if ((lease = find_lease_by_chaddr(oldpacket->chaddr, ifid, classindex)))
  {
                if (!lease_expired(lease, ifid, classindex))
		{
                if ((lease->expires == server_config[ifid][classindex].inflease_time) 
                    && (server_config[ifid][classindex].lease == server_config[ifid][classindex].inflease_time))
                                lease_time_align = server_config[ifid][classindex].inflease_time;
      else
				lease_time_align = lease->expires - time(0);
		}
		packet.yiaddr = lease->yiaddr;
		
	/* Or the client has a requested ip */
	} else if ((req = get_option(oldpacket, DHCP_REQUESTED_IP)) &&

		   /* Don't look here (ugly hackish thing to do) */
		   memcpy(&req_align, req, 4) &&

		   /* and the ip is in the lease range */
		   ntohl(req_align) >= ntohl(server_config[ifid][classindex].start) &&
		   ntohl(req_align) <= ntohl(server_config[ifid][classindex].end) &&
		   
            /* and its not already taken/offered */
		   ((!(lease = find_lease_by_yiaddr(req_align, ifid, classindex)) ||
		   
		   /* or its taken, but expired */ /* ADDME: or maybe in here */
		   lease_expired(lease,ifid,classindex)))) 
        {
		   /* check id addr is not taken by a static ip */
                if(!check_ip(req_align, ifid, classindex))
				packet.yiaddr = req_align; /* FIXME: oh my, is there a host using this IP? */
                else 
                { 
                        packet.yiaddr = find_address(0, ifid, classindex);

                        /* try for an expired lease */
                        if (!packet.yiaddr) packet.yiaddr = find_address(1, ifid, classindex);
                }

        /* otherwise, find a free IP */
	}
  else
  {
            /* Is it a static lease? (No, because find_address skips static lease) */
		packet.yiaddr = find_address(0, ifid,classindex);

		/* try for an expired lease */
		if (!packet.yiaddr) packet.yiaddr = find_address(1, ifid,classindex);
	}
	
	if(!packet.yiaddr) 
        {
		LOG(LOG_WARNING, "no IP addresses to give -- OFFER abandoned");
		return -1;
	}

  hname = get_option(oldpacket,DHCP_HOST_NAME);
  if(hname)
  {
    add_option_string(packet.options, hname - 2);
		memset(hostname,0x00,50);
    length = get_option_length(oldpacket,DHCP_HOST_NAME);
    copy_till(hname , hostname, '.',length);
		LOG(LOG_INFO, "SENDING OFFER to %s\n",hostname);
  }	

  /* check if option 125 vendor specific is requested - get multiple occurancies */  
      if (check_and_send_vendor_specific_option(oldpacket, packet.options) == 1)
      {
          LOG(LOG_INFO, "SENDING OFFER with option 125\n");
      }

//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
// For COMCAST, don't save device identity.
#ifndef CONFIG_VENDOR_COMCAST
        isGetDeviceIdentity = get_option125_info(oldpacket, &broadband_specOpt);
        isGetDslForumString = is_option124_dslforum_exist(oldpacket);
#endif // CONFIG_VENDOR_COMCAST
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

  /*Check for infinite lease */
	if ((lease = find_lease_by_chaddr(oldpacket->chaddr, ifid, classindex)))
	{
            if ((lease->expires == server_config[ifid][classindex].inflease_time)
                    && (server_config[ifid][classindex].lease == server_config[ifid][classindex].inflease_time))
			leasetime = server_config[ifid][classindex].inflease_time;
		else
				leasetime = server_config[ifid][classindex].offer_time;
	}
  else
		leasetime = server_config[ifid][classindex].offer_time;

//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
        if (!add_lease(packet.chaddr, packet.yiaddr, leasetime /*server_config[ifid].offer_time*/, ifid, classindex, hostname, DHCP_LAN_CLIENT_UNKNOWN, &broadband_specOpt))
        {
#else
        if (!add_lease(packet.chaddr, packet.yiaddr, leasetime /*server_config[ifid].offer_time*/, ifid, classindex, hostname)) {/*ARRIS MOD*/
#endif //INCLUDE_ARRIS_GW_TR69
//UNIHAN MODIFY
		LOG(LOG_WARNING, "lease pool is full -- OFFER abandoned");
		return -1;
	}		

	if ((lease_time = get_option(oldpacket, DHCP_LEASE_TIME))) {
		memcpy(&lease_time_align, lease_time, 4);
		lease_time_align = ntohl(lease_time_align);
		if (lease_time_align > server_config[ifid][classindex].lease) 
			lease_time_align = server_config[ifid][classindex].lease;
	}

	/* Make sure we aren't just using the lease time from the previous offer */
	if (lease_time_align < server_config[ifid][classindex].min_lease) 
		lease_time_align = server_config[ifid][classindex].lease;

  /* For inifinite leases change the lease time */
  if( leasetime == server_config[ifid][classindex].inflease_time)
    lease_time_align = leasetime;

	/* ADDME: end of short circuit */		
    // UNIHAN ADD START, to support static lease
    }
    else
    {
        /* It is a static lease... use it */
        packet.yiaddr = static_lease_ip;
    }
    // UNIHAN ADD END

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_align));

	curr = server_config[ifid][classindex].options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet, ifid, classindex);
	
	addr.s_addr = packet.yiaddr;
        packet_option43(&packet, ifid, classindex, flag43); /* ARRIS ADD - add option 43 at the end */

//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
// For COMCAST, there is NO TR69 options in DHCP messages, server side or LAN.
#ifndef CONFIG_VENDOR_COMCAST
    packet_option125(&packet, isGetDeviceIdentity, isGetDslForumString, ifid, classindex); /* ARRIS CHANGE */
#endif // CONFIG_VENDOR_COMCAST
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

	LOG(LOG_INFO, "sending OFFER of %s", inet_ntoa(addr));
	return send_packet(&packet, 0, ifid,classindex);
}


int sendNAK(struct dhcpMessage *oldpacket, int ifid,int classindex,int flag43)
{
	struct dhcpMessage packet;

	init_packet(&packet, oldpacket, DHCPNAK, ifid,classindex); /* ARRIS CHANGE - delete flag43 */
        packet_option43(&packet, ifid, classindex, flag43); /* ARRIS ADD - add option 43 at the end */
	
	DEBUG(LOG_INFO, "sending NAK");
	return send_packet(&packet, 1, ifid,classindex);
}


int sendACK(struct dhcpMessage *oldpacket, u_int32_t yiaddr, int ifid,int classindex,int flag43)
{
	struct dhcpMessage packet;
	struct option_set *curr;
	unsigned char *lease_time;
	u_int32_t lease_time_align = server_config[ifid][classindex].lease;
	struct in_addr addr;
    u_int8_t hostname[50]="unknown" , *hname;
    char device_name[64] = "unknown device";
    int length = 0;
    struct dhcpOfferedAddr *lease = NULL;
//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    bool isGetDeviceIdentity = false;
    bool isGetDslForumString = false;
    struct dhcp_option125_info broadband_specOpt;
    init_option125_info(&broadband_specOpt);
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

    /* ARRIS ADD BEGIN : For updating client database */
#ifdef INCLUDE_ARRIS_GW	
    uint32_t static_lease_ip = 0;
    bool     is_dynamic_lease = True;
#endif
    /* ARRIS ADD END */
    
	init_packet(&packet, oldpacket, DHCPACK, ifid,classindex); /* ARRIS CHANGE - delete flag43 */
    u_int8_t *vendorSpecific;
	packet.yiaddr = yiaddr;
	
	if ((lease_time = get_option(oldpacket, DHCP_LEASE_TIME))) {
		memcpy(&lease_time_align, lease_time, 4);
		lease_time_align = ntohl(lease_time_align);
		if (lease_time_align > server_config[ifid][classindex].lease) 
			lease_time_align = server_config[ifid][classindex].lease;
		else if (lease_time_align < server_config[ifid][classindex].min_lease) 
			lease_time_align = server_config[ifid][classindex].lease;
	}
	
#if 0
  /* If the existing lease entry has infinite entry give it infinite time */
	if ( (lease = find_lease_by_chaddr(oldpacket->chaddr, ifid,classindex)) )
	{
		if(lease->expires == server_config[ifid][classindex].inflease_time)
			lease_time_align = server_config[ifid][classindex].inflease_time;
	}
#endif
	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_align));

  hname = get_option(oldpacket,DHCP_HOST_NAME);
  if(hname)
  {
    add_option_string(packet.options, hname - 2 );
    memset(hostname,0x00,50);
    length = get_option_length(oldpacket,DHCP_HOST_NAME);
    length = length >= sizeof(hostname) ? sizeof(hostname) - 1 : length; // fix bug of size overflow
    copy_till(hname , hostname, '.',length);
//		LOG(LOG_INFO, "SENDING ACK to %s\n",hostname); ARRIS remove worthless print
  }	
  packet_option55_parse(oldpacket,device_name, sizeof(device_name)); // FIX BUG OF SIZE OVERFLOW
	
//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
// For COMCAST, don't save device identity.
#ifndef CONFIG_VENDOR_COMCAST
    isGetDeviceIdentity = get_option125_info(oldpacket, &broadband_specOpt);
    isGetDslForumString = is_option124_dslforum_exist(oldpacket);
#endif // CONFIG_VENDOR_COMCAST
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

	curr = server_config[ifid][classindex].options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet, ifid, classindex);

	addr.s_addr = packet.yiaddr;
//	LOG(LOG_INFO, "sending ACK to %s", inet_ntoa(addr)); ARRIS remove worthless print

    packet_option43(&packet, ifid, classindex, flag43); /* ARRIS ADD - add option 43 at the end */

//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
// For COMCAST, there is NO TR69 options in DHCP messages, server side or LAN.
#ifndef CONFIG_VENDOR_COMCAST
    packet_option125(&packet, isGetDeviceIdentity, isGetDslForumString, ifid, classindex); /* ARRIS CHANGE */
#endif // CONFIG_VENDOR_COMCAST
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

  /* check if option 125 vendor specific is requested - get multiple occurancies */  
    if (check_and_send_vendor_specific_option(oldpacket, packet.options) == 1)
    {
        LOG(LOG_INFO, "SENDING ACK with option 125\n");
    }


    if (send_packet(&packet, 0, ifid,classindex) < 0) 
        return -1;

    /* ARRIS MOD BEGIN */
#ifdef INCLUDE_ARRIS_GW
    static_lease_ip = getIpByMac(server_config[ifid][classindex].static_leases, packet.chaddr);
    if ( static_lease_ip == packet.yiaddr )
    {
        is_dynamic_lease = False;
    }
#endif

// UNIHAN MOD START FOR PROD00207932
    char ip[20] = {0};
    char opt_var_file[256] = {0};
    if(inet_ntop(AF_INET, (void *)&packet.yiaddr, ip, 16)!=NULL);
    {
        snprintf(opt_var_file, sizeof(opt_var_file), "%s%s", DHCP_OPTIOM_FILE_NAME, ip);
    }
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    add_lease(packet.chaddr, packet.yiaddr, lease_time_align, ifid, classindex, hostname, 0, &broadband_specOpt);

#ifdef INCLUDE_ARRIS_GW
    write_to_delta(packet.chaddr,packet.yiaddr,hostname,lease_time_align,LEASE_ADD,classindex, device_name, is_dynamic_lease, opt_var_file, &broadband_specOpt);
#endif
    
#else
    add_lease(packet.chaddr, packet.yiaddr, lease_time_align, ifid, classindex, hostname);

#ifdef INCLUDE_ARRIS_GW
    write_to_delta(packet.chaddr,packet.yiaddr,hostname,lease_time_align,LEASE_ADD,classindex, device_name, is_dynamic_lease, opt_var_file);
#endif
    
#endif //INCLUDE_ARRIS_GW_TR69
// UNIHAN MOD END

    /* ARRIS MOD END */

	return 0;
}


int send_inform(struct dhcpMessage *oldpacket, int ifid, int classindex, int flag43)
{
	struct dhcpMessage packet;
	struct option_set *curr;

    //ARRIS ADD for PROD00199540  
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    bool isGetDeviceIdentity = false;
    bool isGetDslForumString = false;
    struct dhcp_option125_info broadband_specOpt;
    init_option125_info(&broadband_specOpt);
#endif // INCLUDE_ARRIS_GW_TR69
    //ARRIS ADD END
    
	init_packet(&packet, oldpacket, DHCPACK, ifid, classindex); /* ARRIS CHANGE - delete flag43 */
	
	curr = server_config[ifid][classindex].options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet, ifid,classindex);

    //ARRIS ADD for PROD00199540  
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
/* For COMCAST, there is NO TR69 options in DHCP messages, server side or LAN.
 * Don't save device identity.
 */
#ifndef CONFIG_VENDOR_COMCAST
    isGetDeviceIdentity = get_option125_info(oldpacket, &broadband_specOpt);
    isGetDslForumString = is_option124_dslforum_exist(oldpacket);
    packet_option125(&packet, isGetDeviceIdentity, isGetDslForumString, ifid, classindex); /* ARRIS CHANGE */
#endif // CONFIG_VENDOR_COMCAST
#endif // INCLUDE_ARRIS_GW_TR69
    //ARRIS ADD END

        packet_option43(&packet, ifid, classindex, flag43); /* ARRIS ADD - add option 43 at the end */

	return send_packet(&packet, 0, ifid,classindex);
}

