/*
 * udhcpc_plugin.h
 *
 * The file contains the definitions of all the PLUGIN API that is 
 * exposed by the core udhcp client .
 *
 * Copyright (C) 2008 Texas Instruments Incorporated - http://www.ti.com/
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed “as is” WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _UDHCPC_PLUGIN_H
#define _UDHCPC_PLUGIN_H

/* max len of plugin name including NULL should be 30 */
#define UDHCPC_MAX_PLUGIN_NAME 30

#include "udhcp_packet.h"

typedef enum udhcpc_client_state 
{
	INIT_SELECTING,
	REQUESTING,
	BOUND,
	RENEWING,
	REBINDING,
	INIT_REBOOT,
	RENEW_REQUESTED,
	RELEASED
} udhcpc_client_state;


typedef struct UDHCPC_PARAMS
{
	struct dhcpMessage* message;
	char msg_type;
	char **options; /* null terminated array of pointers holding strings of the form
					   option_name=option_val*/
}UDHCPC_PARAMS;

typedef enum UDHCPC_STATE
{
	UDHCPC_DOWN, /* = 0 */
	UDHCPC_UP
}UDHCPC_STATE;

typedef struct UDHCPC_PACKET_STATS
{
	UDHCPC_STATE state;
	u_int32_t discover;
	u_int32_t request;
	u_int32_t offer;
	u_int32_t ack;
	u_int32_t nack;
	u_int32_t release; /* sent */
	u_int32_t renew; /* sent */
	u_int32_t config_attempts; /* how many times dhcpc config process attempted */
} UDHCPC_PACKET_STATS;

/***********************************************************************************
 * Structure name : UDHCPC_PLUGIN_MCB
 ***********************************************************************************/

typedef struct UDHCPC_PLUGIN_MCB 
{
	/* version information about plugin */
	unsigned short major_version;
	unsigned short minor_version;

	/* name of the plugin */
	char plugin_name[UDHCPC_MAX_PLUGIN_NAME]; 

	/* List of callback function to be populated by the plugin */

	/* parse the command line option specific to a plugin. update *argc by the number
       arguments it consumes + 1 (for the option itself)
	*/  
	int  	(*cmd_option_parser)(int *argc, char *argv[]);

	/* add parameter request list option - 55. return length of option area (TLV) or 
	   -1 if error  
    */
	short	(*add_param_request_list)(udhcpc_client_state state,char msg_type,char *option);

	/* add plugin specific options, code + len + data and return the length of option area (TLV) 
	   or -1 if error 
	*/
	short	(*add_options)(udhcpc_client_state state,char msg_type, char *option); 
    
	/* parse option from server, return status  */
	UDHCP_ERROR	(*parse_option)(udhcpc_client_state state,char msg_type, char* option);	

	/* Core pass DHCP configuration received from server to plugin using this API.
	   Return 0 - Success, -1 - failure.
	*/
	int (*report_config)(udhcpc_client_state state, UDHCPC_PARAMS *params, UDHCP_ERROR error );  

	// ARRIS ADD - to get lease times from DHCP offer
	void (*report_lease_times)(unsigned long now, unsigned long lease_time, unsigned long renew_time, unsigned long rebind_time);

    // ARRIS ADD - to report DHCP state changes
    void (*report_state_change)( udhcpc_client_state state );

	/* DHCP Event logger. Handle General DHCP events (discovery, ack, nack, or error events)*/
	void (*event_logger)(udhcpc_client_state curr_state, udhcpc_client_state new_state, char* event);

	// ARRIS ADD - to convert packet to log
	int (*convert_packet_to_log)(struct dhcpMessage *dhcpPkt);
    
	// ARRIS ADD - to delay nak process
    void (*delay_nak)( udhcpc_client_state state, unsigned long expireTime,  
                                                unsigned long rebindTime, void (*stateChangeFunc)(udhcpc_client_state),
                                                int (*gtSysUpTime)(unsigned int *));
    // ARRIS ADD - to print tr069 debug info
    void (*tr069_v4_debug)(const char *str , ...);

    // ARRIS ADD - to parse tr069 option43
    int (*tr069_parser_opt43)(unsigned char *opt);

	/* handle exit */
	void 	(*exit)(void);

}UDHCPC_PLUGIN_MCB;

/* Registration & Unregistration of Plugins with the core TI UDHCPC. */
extern int udhcpc_register_plugin (UDHCPC_PLUGIN_MCB *ptr_plugin);
//extern int udhcpc_unregister_plugin (char* ptr_name);
/* get stats from dhcp client core */
extern int udhcpc_get_stats(struct UDHCPC_PACKET_STATS *udhcpc_stats);
/* get interface name from dhcp client core */
extern char *udhcpc_get_interface(void);
/* get the current server ip */
extern unsigned long get_curr_server(void);


#endif /* _UDHCPC_PLUGIN_H */
