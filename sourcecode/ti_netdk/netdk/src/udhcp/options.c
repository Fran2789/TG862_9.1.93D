/* 
 * options.c -- DHCP server option packet tools 
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */
 /*-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
//
// 1. Added a new function, get_option_length() to get the length of the option value 
//    in the received dhcp packet. This is used in serverpacket.c.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "leases.h"


//ARRIS ADD
#define MAX_STRING_SIZE 256
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
extern struct tr69_config_t arris_tr69_conf;
#endif
//END ARRIS ADD

/* supported options are easily added here */
struct dhcp_option options[] = {
	/* name[10]	flags					code */
	{"subnet",	OPTION_IP | OPTION_REQ,			0x01},
	{"timezone",	OPTION_S32,				0x02},
	{"router",	OPTION_IP | OPTION_LIST | OPTION_REQ,	0x03},
	{"timesvr",	OPTION_IP | OPTION_LIST,		0x04},
	{"namesvr",	OPTION_IP | OPTION_LIST,		0x05},
	{"dns",		OPTION_IP | OPTION_LIST | OPTION_REQ,	0x06},
	{"logsvr",	OPTION_IP | OPTION_LIST,		0x07},
	{"cookiesvr",	OPTION_IP | OPTION_LIST,		0x08},
	{"lprsvr",	OPTION_IP | OPTION_LIST,		0x09},
	{"hostname",	OPTION_STRING | OPTION_REQ,		0x0c},
	{"bootsize",	OPTION_U16,				0x0d},
	{"domain",	OPTION_STRING | OPTION_REQ,		0x0f},
	{"swapsvr",	OPTION_IP,				0x10},
	{"rootpath",	OPTION_STRING,				0x11},
	{"ipttl",	OPTION_U8,				0x17},
	{"mtu",		OPTION_U16,				0x1a},
	{"broadcast",	OPTION_IP | OPTION_REQ,			0x1c},
	{"ntpsrv",	OPTION_IP | OPTION_LIST,		0x2a},
	{"vendorinfo",	OPTION_STRING,				0x2b},
	{"wins",	OPTION_IP | OPTION_LIST,		0x2c},
	{"requestip",	OPTION_IP,				0x32},
	{"lease",	OPTION_U32,				0x33},
	{"dhcptype",	OPTION_U8,				0x35},
	{"serverid",	OPTION_IP,				0x36},
	{"vendorid",	OPTION_STRING,				0x3c},
	{"message",	OPTION_STRING,				0x38},
	{"tftp",	OPTION_STRING,				0x42},
	{"bootfile",	OPTION_STRING,				0x43},
	{"userclass",	OPTION_STRING,				0x4d},
	{"vendorspecific",	OPTION_STRING | OPTION_LIST, 0x7d},

	{"",		0x00,				0x00}
};

/* Lengths of the different option types */
int option_lengths[] = {
	[OPTION_IP] =		4,
	[OPTION_IP_PAIR] =	8,
	[OPTION_BOOLEAN] =	1,
	[OPTION_STRING] =	1,
	[OPTION_U8] =		1,
	[OPTION_U16] =		2,
	[OPTION_S16] =		2,
	[OPTION_U32] =		4,
	[OPTION_S32] =		4,
    [OPTION_VARIABLE] =	1  /* added for Option 125 */
};


/* get an option with bounds checking (warning, not aligned). */
int get_option_length(struct dhcpMessage *packet, int code)
{
	int i, length;
	unsigned char *optionptr;
	int over = 0, done = 0, curr = OPTION_FIELD;
	
	optionptr = packet->options;
	i = 0;
	length = 308;
	while (!done) {
		if (i >= length) {
			LOG(LOG_WARNING, "bogus packet, option fields too long.");
			return (int)NULL;
		}
		if (optionptr[i + OPT_CODE] == code) {
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				LOG(LOG_WARNING, "bogus packet, option fields too long.");
				return 0;
			}
			return optionptr[i + OPT_LEN];
		}			
		switch (optionptr[i + OPT_CODE]) {
		case DHCP_PADDING:
			i++;
			break;
		case DHCP_OPTION_OVER:
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				LOG(LOG_WARNING, "bogus packet, option fields too long.");
				return 0;
			}
			over = optionptr[i + 3];
			i += optionptr[OPT_LEN] + 2;
			break;
		case DHCP_END:
			if (curr == OPTION_FIELD && over & FILE_FIELD) {
				optionptr = packet->file;
				i = 0;
				length = 128;
				curr = FILE_FIELD;
			} else if (curr == FILE_FIELD && over & SNAME_FIELD) {
				optionptr = packet->sname;
				i = 0;
				length = 64;
				curr = SNAME_FIELD;
			} else done = 1;
			break;
		default:
			i += optionptr[OPT_LEN + i] + 2;
		}
	}
	return 0;
}


/* get an option with bounds checking (warning, not aligned). */
unsigned char *get_option(struct dhcpMessage *packet, int code)
{
	int i, length;
	unsigned char *optionptr;
	int over = 0, done = 0, curr = OPTION_FIELD;
	
	optionptr = packet->options;
	i = 0;
	length = 308;
	while (!done) {
		if (i >= length) {
			LOG(LOG_WARNING, "bogus packet, option fields too long.");
			return NULL;
		}
		if (optionptr[i + OPT_CODE] == code) {
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				LOG(LOG_WARNING, "bogus packet, option fields too long.");
				return NULL;
			}
			return optionptr + i + 2;
		}			
		switch (optionptr[i + OPT_CODE]) {
		case DHCP_PADDING:
			i++;
			break;
		case DHCP_OPTION_OVER:
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				LOG(LOG_WARNING, "bogus packet, option fields too long.");
				return NULL;
			}
			over = optionptr[i + 3];
			i += optionptr[OPT_LEN] + 2;
			break;
		case DHCP_END:
			if (curr == OPTION_FIELD && over & FILE_FIELD) {
				optionptr = packet->file;
				i = 0;
				length = 128;
				curr = FILE_FIELD;
			} else if (curr == FILE_FIELD && over & SNAME_FIELD) {
				optionptr = packet->sname;
				i = 0;
				length = 64;
				curr = SNAME_FIELD;
			} else done = 1;
			break;
		default:
			i += optionptr[OPT_LEN + i] + 2;
		}
	}
	return NULL;
}


/* get a multiple occurancies of option with bounds checking (warning, not aligned). */
/* counter idicates - which occurance of option to get */
unsigned char *get_option_multiple(struct dhcpMessage *packet, int code, int counter)
{
	int i, length;
	unsigned char *optionptr;
	int over = 0, done = 0, curr = OPTION_FIELD;
    int optionCount = 0;
	
	optionptr = packet->options;
	i = 0;
	length = 308;
    
	while (!done) {
		if (i >= length) {
			LOG(LOG_WARNING, "bogus packet, option fields too long.");
			return NULL;
		}
		if (optionptr[i + OPT_CODE] == code) {
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				LOG(LOG_WARNING, "bogus packet, option fields too long.");
				return NULL;
			}
            if (optionCount == counter)
            {
                return optionptr + i + 2;
            }
            else 
            {
                optionCount++;
            }
		}			
		switch (optionptr[i + OPT_CODE]) {
		case DHCP_PADDING:
			i++;
			break;
		case DHCP_OPTION_OVER:
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				LOG(LOG_WARNING, "bogus packet, option fields too long.");
				return NULL;
			}
			over = optionptr[i + 3];
			i += optionptr[OPT_LEN] + 2;
			break;
		case DHCP_END:
			if (curr == OPTION_FIELD && over & FILE_FIELD) {
				optionptr = packet->file;
				i = 0;
				length = 128;
				curr = FILE_FIELD;
			} else if (curr == FILE_FIELD && over & SNAME_FIELD) {
				optionptr = packet->sname;
				i = 0;
				length = 64;
				curr = SNAME_FIELD;
			} else done = 1;
			break;
		default:
			i += optionptr[OPT_LEN + i] + 2;
		}
	}
	return NULL;
}


/* return the position of the 'end' option (no bounds checking) */
int end_option(unsigned char *optionptr) 
{
	int i = 0;
	
	while (optionptr[i] != DHCP_END) {
		if (optionptr[i] == DHCP_PADDING) i++;
		else i += optionptr[i + OPT_LEN] + 2;
	}
	return i;
}


/* add an option string to the options (an option string contains an option code,
 * length, then data) */
int add_option_string(unsigned char *optionptr, unsigned char *string)
{
	int end = end_option(optionptr);
	
	/* end position + string length + option code/length + end option */
	if (end + string[OPT_LEN] + 2 + 1 >= 308) {
		LOG(LOG_ERR, "Option 0x%02x did not fit into the packet!", string[OPT_CODE]);
		return 0;
	}
	DEBUG(LOG_INFO, "adding option 0x%02x", string[OPT_CODE]);
	memcpy(optionptr + end, string, string[OPT_LEN] + 2);
	optionptr[end + string[OPT_LEN] + 2] = DHCP_END;
	return string[OPT_LEN] + 2;
}


/* add a one to four byte option to a packet */
int add_simple_option(unsigned char *optionptr, unsigned char code, u_int32_t data)
{
	char length = 0;
	int i;
	unsigned char option[2 + 4];

    union {
	unsigned char u8;
	u_int16_t u16;
	u_int32_t u32;
	} aligned;
	
/*
	unsigned char *u8;
	u_int16_t *u16;
	u_int32_t *u32;
	u_int32_t aligned;
	u8 = (unsigned char *) &aligned;
	u16 = (u_int16_t *) &aligned;
	u32 = &aligned;
*/

	for (i = 0; options[i].code; i++)
		if (options[i].code == code) {
			length = option_lengths[options[i].flags & TYPE_MASK];
		}
		
	if (!length) {
		DEBUG(LOG_ERR, "Could not add option 0x%02x", code);
		return 0;
	}
	
	option[OPT_CODE] = code;
	option[OPT_LEN] = length;

	switch (length) {
		case 1: 
		{
			/*
			*u8 =  data; 
			break;
			*/
			aligned.u8=data;
			break;
		}
		case 2: 
		{
			/*
			*u16 = data; 
			break;
			*/
			aligned.u16=data;
			break;
		}
		case 4: 
		{
			/*
			*u32 = data; 
			break;
			*/
			aligned.u32=data;
			break;
		}
	}
	memcpy(option + 2, &aligned.u32, length); 
/*
	memcpy(option + 2, &aligned, length); 
*/
	return add_option_string(optionptr, option);
}


/* find option 'code' in opt_list */
struct option_set *find_option(struct option_set *opt_list, char code)
{
	while (opt_list && opt_list->data[OPT_CODE] < code)
		opt_list = opt_list->next;

	if (opt_list && opt_list->data[OPT_CODE] == code) return opt_list;
	else return NULL;
}


/* add an option to the opt_list */
void attach_option(struct option_set **opt_list, struct dhcp_option *option, char *buffer, int length)
{
	struct option_set *existing, *new, **curr;

	/* add it to an existing option */
	if ((existing = find_option(*opt_list, option->code))) {
		DEBUG(LOG_INFO, "Attaching option %s to existing member of list", option->name);
		if (option->flags & OPTION_LIST) {
			if (existing->data[OPT_LEN] + length <= 255) {
				existing->data = realloc(existing->data, 
						existing->data[OPT_LEN] + length + 2);
				memcpy(existing->data + existing->data[OPT_LEN] + 2, buffer, length);
				existing->data[OPT_LEN] += length;
			} /* else, ignore the data, we could put this in a second option in the future */
		} /* else, ignore the new data */
	} else {
		DEBUG(LOG_INFO, "Attaching option %s to list", option->name);
		
		/* make a new option */
		new = malloc(sizeof(struct option_set));
		new->data = malloc(length + 2);
		new->data[OPT_CODE] = option->code;
		new->data[OPT_LEN] = length;
		memcpy(new->data + 2, buffer, length);
		
		curr = opt_list;
		while (*curr && (*curr)->data[OPT_CODE] < option->code)
			curr = &(*curr)->next;
			
		new->next = *curr;
		*curr = new;		
	}
}

void delete_all_options(struct option_set **opt_list)
{
	struct option_set *next;
	/* check opt_list for null */
	struct option_set *curr = *opt_list;

	while (curr)
	{
		next = curr->next;
		free(curr->data);
		free(curr);
		curr = next;
	}

	*opt_list = NULL;
}

//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
void init_option125_info(struct dhcp_option125_info *retInfo)
{
    memset( retInfo->deviceManufacturerOUI, 0, sizeof(retInfo->deviceManufacturerOUI) );
    memset( retInfo->deviceSerialNumber, 0, sizeof(retInfo->deviceSerialNumber) );
    memset( retInfo->deviceProductClass, 0, sizeof(retInfo->deviceProductClass) );
}

/* get option125(Vendor-Identifying Vendor-Specific) information. */
bool get_option125_info(struct dhcpMessage *packet, struct dhcp_option125_info *retInfo)
{
    bool retValue = false;
    int option125_count = 0;
    int option125_length = 0;
    u_int8_t *option125 = NULL;
    u_int8_t data_len = 0;
    u_int8_t enterprise_count = 0;
    u_int8_t subopt_code = 0;
    u_int8_t subopt_len = 0;
    u_int32_t enterprise_number = 0;

    option125 = get_option(packet, DHCP_VENDORID_VENDORSPECIFIC);
    if (option125)
    {
        option125_length = get_option_length(packet,DHCP_VENDORID_VENDORSPECIFIC);

        while (option125_count < option125_length)
        {
            enterprise_number = 0;
            data_len = 0;

            /* Get 32-bit enterprise number */
            enterprise_number = (*(option125 + option125_count++) << 24);
            enterprise_number += (*(option125 + option125_count++) << 16);
            enterprise_number += (*(option125 + option125_count++) << 8);
            enterprise_number += (*(option125 + option125_count++));

            /* Get 8-bit data length */
            data_len = *(option125 + option125_count++);

            if (enterprise_number == IANA_ENTERPRISE_BROADBAND_FORUM)
            {
                enterprise_count = 0;

                while (enterprise_count < data_len)
                {
                    subopt_code = 0;
                    subopt_len = 0;

                    subopt_code = *(option125 + option125_count + enterprise_count++);
                    subopt_len = *(option125 + option125_count + enterprise_count++);

                    switch (subopt_code)
                    {
                        case BROADBAND_SUBOPT_DEVICE_MANUFACTUREROUI:
                            if (subopt_len >= OPTION125_MAX_OUI_LEN)
                            {
                                subopt_len = (OPTION125_MAX_OUI_LEN - 1);
                            }
                            if (subopt_len != 0)
                            {
                                memcpy(retInfo->deviceManufacturerOUI, (option125 + option125_count + enterprise_count), subopt_len);
                            }
                            retInfo->deviceManufacturerOUI[subopt_len] = '\0';
                            break;
                            
                        case BROADBAND_SUBOPT_DEVICE_SERIALNUMBER:
                            if (subopt_len >= OPTION125_MAX_SUBOPT_LEN)
                            {
                                subopt_len = (OPTION125_MAX_SUBOPT_LEN - 1);
                            }
                            if (subopt_len != 0)
                            {
                                memcpy(retInfo->deviceSerialNumber, (option125 + option125_count + enterprise_count), subopt_len);
                            }
                            retInfo->deviceSerialNumber[subopt_len] = '\0';
                            break;
                            
                        case BROADBAND_SUBOPT_DEVICE_PRODUCTCLASS:
                            if (subopt_len >= OPTION125_MAX_SUBOPT_LEN)
                            {
                                subopt_len = (OPTION125_MAX_SUBOPT_LEN - 1);
                            }
                            if (subopt_len != 0)
                            {
                                memcpy(retInfo->deviceProductClass, (option125 + option125_count + enterprise_count), subopt_len);
                            }
                            retInfo->deviceProductClass[subopt_len] = '\0';
                            break; 
                            
                        default:
                            break;
                    }
                    enterprise_count += subopt_len;
                }
            }
            option125_count += data_len;
        }
    }

     /*  
     * 'The Device MUST support Device-Gateway association 
     * as defined in [TR-069a4] Annex F.'
     * For a DHCP request from the Device that contains the 
     * Device Identity, the DHCP Option MUST contain the 
     * following Encapsulated Vendor-Specific Option-Data fields:
     * DeviceManufacturerOUI
     * DeviceSerialNumber
     * DeviceProductClass (this MAY be left out if the corresponding
     * source Parameter is not present)
     */
    if ( (strlen(retInfo->deviceManufacturerOUI) != 0) && 
         (strlen(retInfo->deviceSerialNumber) != 0) )
    {
         retValue = true;
    }

    return retValue;
}

/* Get option124(Vendor Class) enterprise number. */
#define DSL_FORUM_STRING                     "dslforum.org"
#define MAX_DHCP_VENDORCLASS_DATALEN         256
bool is_option124_dslforum_exist(struct dhcpMessage *packet)
{
    bool retValue = false;
    u_int8_t *option124 = NULL;

    option124 = get_option(packet, DHCP_VENDORID_VENDORCLASS);
    if (option124)
    {
        int option124_count = 0;
        int option124_length = 0;

        option124_length = get_option_length(packet,DHCP_VENDORID_VENDORCLASS);

        while (option124_count < option124_length)
        {
            u_int32_t enterprise_number = 0;
            u_int8_t data_len = 0;
            char vendorClassData[MAX_DHCP_VENDORCLASS_DATALEN] = {0};

            /* Get 32-bit enterprise number */
            enterprise_number = (*(option124 + option124_count++) << 24);
            enterprise_number += (*(option124 + option124_count++) << 16);
            enterprise_number += (*(option124 + option124_count++) << 8);
            enterprise_number += (*(option124 + option124_count++));

            /* Get 8-bit data length */
            data_len = *(option124 + option124_count++);

            if (data_len != 0)
            {
                memcpy(vendorClassData, option124 + option124_count, data_len);
            }
            vendorClassData[data_len] = '\0';

            if (strstr(vendorClassData, DSL_FORUM_STRING) != NULL)
            {
                retValue = true;
            }
            option124_count += data_len;
        }
    }

    return retValue;
}

#define OPT_VENDOR_SPEC_ENTERPRISE_LEN 4 
#define OPT_SUB_TYPE 1
#define MAX_HWMAC_SIZE 6
void packet_option125(struct dhcpMessage *packet, bool isGetDeviceIdentity, bool isGetDslForumString, int ifid, int classindex)
{
    int offsetCnt = 0;
    int tempVal = 0;
    int subOptOffset = 0;
    char buffer[MAX_STRING_SIZE + 1];
    int end = end_option(packet->options);
    int len; // ARRIS ADD

    if (!arris_tr69_conf.enable)
    {
        /* Don't packet option125, if CWMP is disabled.*/
        return;
    }
    
    if ((isGetDeviceIdentity == false) &&
        (isGetDslForumString == false))
    {
        /* Don't packet option125, if device identity and 'dslforum.org' are not received.*/
        return;
    }

    packet->options[end + OPT_CODE] = DHCP_VENDORID_VENDORSPECIFIC;
    tempVal = IANA_ENTERPRISE_BROADBAND_FORUM;

    for ( offsetCnt = 0 ; offsetCnt < OPT_VENDOR_SPEC_ENTERPRISE_LEN ; offsetCnt++ )
    {
        packet->options[end + OPT_DATA + OPT_VENDOR_SPEC_ENTERPRISE_LEN - offsetCnt - 1]  = ( (tempVal & ( 0xff << (8 * offsetCnt))) >> (8 * offsetCnt) );
    }

    subOptOffset = end + OPT_DATA + OPT_VENDOR_SPEC_ENTERPRISE_LEN;

    if (isGetDeviceIdentity == true)
    {
        /* Send out gateway identidy. */
        buffer[0] = '\0'; // Clean buffer - empty string
        sprintf(buffer,"%06X",CONFIG_VENDOR_ID);
        packet->options[subOptOffset += OPT_LEN] = BROADBAND_SUBOPT_GW_MANUFACTUREROUI;
        packet->options[subOptOffset += OPT_SUB_TYPE] = strlen(buffer);
        if (strlen(buffer) != 0)
        {
            memcpy(packet->options + subOptOffset + OPT_LEN, buffer, strlen(buffer));
        }
        subOptOffset += strlen(buffer);

        /* ARRIS CHANGE START */
        if (server_config[ifid][classindex].option125_gwsn)
        {
            len = strlen(server_config[ifid][classindex].option125_gwsn);
            len = len >= sizeof(buffer) ? sizeof(buffer) - 1: len;
            strncpy(buffer, server_config[ifid][classindex].option125_gwsn, len);
            buffer[len] = 0;
        /* ARRIS CHANGE END */
        }
        else
        {
            buffer[0] = '\0'; // Clean buffer - empty string
        }
        packet->options[subOptOffset += OPT_LEN] = BROADBAND_SUBOPT_GW_SERIALNUMBER;
        packet->options[subOptOffset += OPT_SUB_TYPE] = strlen(buffer) ;
        if (strlen(buffer) != 0)
        {
            memcpy(packet->options + subOptOffset + OPT_LEN, buffer, strlen(buffer));
        }
        subOptOffset += strlen(buffer);

        /* ARRIS CHANGE START */
        if (server_config[ifid][classindex].option125_gwpclass)
        {
            len = strlen(server_config[ifid][classindex].option125_gwpclass);
            len = len >= sizeof(buffer) ? sizeof(buffer) - 1 : len;
            strncpy(buffer, server_config[ifid][classindex].option125_gwpclass, len);
            buffer[len] = 0;
        }
        else
        /* ARRIS CHANGE END */
        {
            buffer[0] = '\0'; // Clean buffer - empty string
        }
        packet->options[subOptOffset += OPT_LEN] = BROADBAND_SUBOPT_GW_PRODUCTCLASS;
        packet->options[subOptOffset += OPT_SUB_TYPE] = strlen(buffer);
        if (strlen(buffer) != 0)
        {
            memcpy(packet->options + subOptOffset + OPT_LEN, buffer, strlen(buffer));
        }
        subOptOffset += strlen(buffer);
    }

    if (isGetDslForumString == true)
    {
        /* Reply ACS discovery parameter. */
        if (arris_tr69_conf.acs_url && *arris_tr69_conf.acs_url)
        {
            strncpy(buffer, arris_tr69_conf.acs_url, sizeof(buffer) - 1);   
            buffer[sizeof(buffer) - 1] = 0;
        }
        else
        {
            buffer[0] = '\0'; // Clean buffer - empty string
        }
        packet->options[subOptOffset += OPT_LEN] = BROADBAND_SUBOPT_ACS_URL;
        packet->options[subOptOffset += OPT_SUB_TYPE] = strlen(buffer);
        if (strlen(buffer) != 0)
        {
            memcpy(packet->options + subOptOffset + OPT_LEN, buffer, strlen(buffer));
        }
        subOptOffset += strlen(buffer);

        if (arris_tr69_conf.provision_code && *arris_tr69_conf.provision_code)
        {
            strncpy(buffer, arris_tr69_conf.provision_code, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = 0;
        }
        else
        {
            buffer[0] = '\0'; // Clean buffer - empty string
        }
        packet->options[subOptOffset += OPT_LEN] = BROADBAND_SUBOPT_PROVISIONING_CODE;
        packet->options[subOptOffset += OPT_SUB_TYPE] = strlen(buffer);
        if (strlen(buffer) != 0)
        {
            memcpy(packet->options + subOptOffset + OPT_LEN, buffer, strlen(buffer));
        }
        subOptOffset += strlen(buffer);

        if (arris_tr69_conf.retry_wait_interval && *arris_tr69_conf.retry_wait_interval)
        {
            sprintf(buffer, "%s", arris_tr69_conf.retry_wait_interval);
        }
        else
        {
            buffer[0] = '\0'; // Clean buffer - empty string
        }
        packet->options[subOptOffset += OPT_LEN] = BROADBAND_SUBOPT_CWMP_RETRY_MINIMUM_WAIT_INTERVAL;
        packet->options[subOptOffset += OPT_SUB_TYPE] = strlen(buffer);
        if (strlen(buffer) != 0)
        {
            memcpy(packet->options + subOptOffset + OPT_LEN, buffer, strlen(buffer));
        }
        subOptOffset += strlen(buffer);

        if (arris_tr69_conf.retry_interval_multi && *arris_tr69_conf.retry_interval_multi)
        {
            sprintf(buffer, "%s", arris_tr69_conf.retry_interval_multi);
        }
        else
        {
            buffer[0] = '\0'; // Clean buffer - empty string
        }
        packet->options[subOptOffset += OPT_LEN] = BROADBAND_SUBOPT_CWMP_RETRY_INTERVAL_MULTIPLIER;
        packet->options[subOptOffset += OPT_SUB_TYPE] = strlen(buffer);
        if (strlen(buffer) != 0)
        {
            memcpy(packet->options + subOptOffset + OPT_LEN, buffer, strlen(buffer));
        }
        subOptOffset += strlen(buffer);
    }

    packet->options[end + OPT_DATA + OPT_VENDOR_SPEC_ENTERPRISE_LEN] = subOptOffset - OPT_DATA - OPT_VENDOR_SPEC_ENTERPRISE_LEN - end;
    packet->options[end + OPT_LEN] = subOptOffset - end - OPT_LEN;
    packet->options[subOptOffset + OPT_LEN] = DHCP_END;
}
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

// UNIHAN ADD START FOR PROD00207932
void write_option_to_file(struct dhcpMessage *packet)
{
	int optionPtrOffSet, optionMaxLength;
	unsigned char *optionPtr;
	int over = 0, done = 0, curr = OPTION_FIELD;
	optionPtr = packet->options;
	optionPtrOffSet = 0;
	optionMaxLength = 308; // refer to *get_option()

    FILE *fp;
    char fileName[MAX_STRING_SIZE] = {0};
    char line[MAX_OPTION_BUFFER] = {0};
    char ip[MAX_STRING_SIZE] = {0};

	while (!done) {
		if (optionPtrOffSet >= optionMaxLength) {
			LOG(LOG_WARNING, "bogus packet, option fields too long.");
			return;
		}
		if (optionPtrOffSet + 1 + optionPtr[optionPtrOffSet + OPT_LEN] >= optionMaxLength) 
        {
			LOG(LOG_WARNING, "bogus packet, option fields too long.");
			return;
		}

       	if (optionPtr[optionPtrOffSet + OPT_CODE] == DHCP_REQUESTED_IP) 
        {
            snprintf(ip,MAX_STRING_SIZE,"%d.%d.%d.%d",optionPtr[optionPtrOffSet + OPT_DATA],   optionPtr[optionPtrOffSet + OPT_DATA + 1],
													  optionPtr[optionPtrOffSet + OPT_DATA +2],optionPtr[optionPtrOffSet + OPT_DATA + 3]);
        }

        int offSet = 0;
        int optionLength = optionPtr[optionPtrOffSet + OPT_LEN];
        char optionData[MAX_STRING_SIZE]= {0};
        char strtmp[MAX_STRING_SIZE]= {0};
        
        if (optionLength > 0)
        {
            for(offSet = 0;optionLength > offSet;offSet++)
            {
                snprintf(&optionData[offSet*2],MAX_STRING_SIZE,"%02X",optionPtr[optionPtrOffSet + OPT_DATA + offSet]);
            }
            snprintf(strtmp,MAX_STRING_SIZE,"option %d:%s\n",optionPtr[optionPtrOffSet + OPT_CODE],optionData);
        }
        strncat(line, strtmp, MAX_OPTION_BUFFER - strlen(line)-1); 

		switch (optionPtr[optionPtrOffSet + OPT_CODE]) {
		case DHCP_PADDING:
			optionPtrOffSet++;
			break;
		case DHCP_OPTION_OVER:
			if (optionPtrOffSet + 1 + optionPtr[optionPtrOffSet + OPT_LEN] >= optionMaxLength) {
				LOG(LOG_WARNING, "bogus packet, option fields too long.");
				return;
			}
			over = optionPtr[optionPtrOffSet + 3];
			optionPtrOffSet += optionPtr[OPT_LEN] + 2;
			break;
        case DHCP_END:
			if (curr == OPTION_FIELD && over & FILE_FIELD) {
				optionPtr = packet->file;
				optionPtrOffSet = 0;
				optionMaxLength = 128;
				curr = FILE_FIELD;
			} else if (curr == FILE_FIELD && over & SNAME_FIELD) {
				optionPtr = packet->sname;
				optionPtrOffSet = 0;
				optionMaxLength = 64;
				curr = SNAME_FIELD;
			} else done = 1;
			break;
		default:
			optionPtrOffSet += optionPtr[OPT_LEN + optionPtrOffSet] + 2;
		}
	}

    snprintf(fileName,MAX_STRING_SIZE,"%s%s%s",DHCP_OPTION_FILE_PATH,DHCP_OPTIOM_FILE_NAME,ip);
    if (!(fp = fopen(fileName, "w"))) 
    {
		LOG(LOG_ERR, "Unable to open %s for writing", DHCP_OPTION_FILE_PATH);
		return;
	}

    fwrite(line, sizeof(char), strlen(line), fp);
    
    fclose(fp);

	return;
}
// UNIHAN ADD END

