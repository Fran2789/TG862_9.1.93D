/*
 * udhcpc_default_plugin.c
 *
 * The file contains the default plugin implementation that can be 
 * overridden by custom plugins functions
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/param.h>
#include <errno.h>
#include <alloca.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include "udhcp_debug.h"
#include "dhcpc.h"
#include "options.h"
#include "udhcp_alloc.h"


static int cmd_option_parser_default(int *argc, char *argv[]);
static short add_param_request_list_default(udhcpc_client_state state,char msg_type,char *option);
static short add_options_default(udhcpc_client_state state,char msg_type,char *option);
static UDHCP_ERROR parse_option_default(udhcpc_client_state state,char msg_type,char *option);
static int report_config_default(udhcpc_client_state state,UDHCPC_PARAMS *params, UDHCP_ERROR error);
static void event_logger_default(udhcpc_client_state curr_state,udhcpc_client_state new_state,char *event);

UDHCPC_PLUGIN_MCB   default_udhcpc_plugin;

static int cmd_option_parser_default(int *argc, char *argv[])
{
	// return error since we don't have any real plugin and hence no
	LOG(LOG_DEBUG,"cmd_option_parser_default:argc:%d,argv:%p",*argc,argv);
	return -1;
}

static short add_param_request_list_default(udhcpc_client_state state,char msg_type, char *option_area)
{
	int i, len = 0;
	LOG(LOG_DEBUG,"add_param_request_list_default:state - %d,msgtype - %d",state,msg_type);
	option_area[OPT_CODE] = DHCP_PARAM_REQ;
	for (i = 0; options[i].code; i++)
		if (options[i].flags & OPTION_REQ)
			option_area[OPT_DATA + len++] = options[i].code;
	option_area[OPT_LEN] = len;
	return (len + OPT_DATA); /* len + 1 char opt code and + 1 for len */
}


static UDHCP_ERROR parse_option_default(udhcpc_client_state state,char msg_type, char *option)
{
	LOG(
		LOG_DEBUG,"parse_option_default:state - %d,msg_type - %d,option ptr - %p",
		state,
	    msg_type, 
		option
		);
	return UDHCP_SUCCESS;
} 

static int report_config_default(udhcpc_client_state state, UDHCPC_PARAMS *params, UDHCP_ERROR error)
{
	LOG(
	  LOG_DEBUG,
	  "report_config_default: state - %d, params ptr - %p, error - %d",
	  state, 
	  params,
	  error
      );
	return 0;	
}

static void event_logger_default(udhcpc_client_state curr_state,udhcpc_client_state new_state, char *event )
{
	char *temp = (char *)udhcp_alloc(strlen(event)+50);
	if (temp)
	{
		sprintf(
			temp,
			"EVENT:%s:current state= %d,new state = %d\n",
			event,
			(unsigned int)curr_state,
			(unsigned int)new_state
			);
		LOG(LOG_INFO,temp);
		udhcp_free(temp);
	}
	return;
}

static short add_options_default(udhcpc_client_state state,char msg_type, char *option_data)
{
	LOG(
	  LOG_DEBUG,
	  "add_options_default: state - %d,msg_type - %d, option data ptr - %p",
	  state,
	  msg_type,
	  option_data
	  );
	/* This add a default vendor id option info to the DHCP messages */
	struct vendor  {
		char vendor, length;
		char str[sizeof("udhcp "VERSION)];
	} vendor_id = { DHCP_VENDOR,  sizeof("udhcp "VERSION) - 1, "udhcp "VERSION};

	*option_data = vendor_id.vendor;
	*(option_data+OPT_LEN) = vendor_id.length;
	strcpy(option_data+OPT_DATA,vendor_id.str);
	return vendor_id.length+2;
}

static void report_lease_times_default(unsigned long now, unsigned long lease_time, unsigned long renew_time, unsigned long rebind_time)
{    
	LOG(
	  LOG_DEBUG,
	  "report_lease_times: now  - %d, lease_time - %d, renew_time - %d, rebind_time - %d", now, lease_time, renew_time, rebind_time);
    return;
}

static void report_state_change_default(udhcpc_client_state state)
{
	LOG(
	  LOG_DEBUG,
	  "report_state_change: state  - %d", state);
    return;
}
static int convert_packet_to_log_default(struct dhcpMessage *dhcpPkt)
{
	LOG(
	  LOG_DEBUG,
	  "convert_packet_to_log_default: dhcpPkt ptr - %p", dhcpPkt);
	return 0;	
}

static void delay_nak_default( udhcpc_client_state state, unsigned long expireTime,  
                                                   unsigned long rebindTime, void(*stateChangeFunc)(udhcpc_client_state),
                                                   int(*gtSysUpTime)(unsigned int *))
{
	LOG(
		LOG_DEBUG,"delay_nak_default:state - %d,expireTime - %d,rebindTime - %d, stateChangeFunc ptr -p%, gtSysUpTime ptr - p%",
		state,
	    expireTime,
	    rebindTime,
		stateChangeFunc,
		gtSysUpTime
		);
    return;
}

static void tr069_v4_debug_default(const char *str , ...)
{
	LOG(
	  LOG_DEBUG,
	  "convert_packet_to_log_default: %s", str);
    return;
}

static int tr069_parser_opt43_default(unsigned char *opt)
{
	LOG(
	  LOG_DEBUG,
	  "tr069_parser_opt43_default: %p", opt);
    return 0;
}


static void exit_default(void)
{
	return;
}
void default_plugin_init(void)
{
	LOG(LOG_DEBUG,"default_plugin_init\n");
	    /* Extract the version information */
    sscanf(
		VERSION, 
		"%hu.%hu", (unsigned short *)&default_udhcpc_plugin.major_version, 
		(unsigned short *)&default_udhcpc_plugin.minor_version
		);

	bzero((void *)&default_udhcpc_plugin, sizeof(UDHCPC_PLUGIN_MCB));
	default_udhcpc_plugin.major_version=1;
	default_udhcpc_plugin.minor_version=0;
	strcpy(default_udhcpc_plugin.plugin_name,"UDHCPC Default");
	default_udhcpc_plugin.cmd_option_parser=cmd_option_parser_default;
	default_udhcpc_plugin.add_param_request_list=add_param_request_list_default;
	default_udhcpc_plugin.add_options=add_options_default;
	default_udhcpc_plugin.parse_option=parse_option_default;
	default_udhcpc_plugin.report_config=report_config_default;
	default_udhcpc_plugin.event_logger=event_logger_default;
	default_udhcpc_plugin.report_lease_times=report_lease_times_default;
	default_udhcpc_plugin.report_state_change=report_state_change_default;
	default_udhcpc_plugin.convert_packet_to_log=convert_packet_to_log_default;
    default_udhcpc_plugin.delay_nak=delay_nak_default;
    default_udhcpc_plugin.tr069_v4_debug=tr069_v4_debug_default;
    default_udhcpc_plugin.tr069_parser_opt43=tr069_parser_opt43_default;
	default_udhcpc_plugin.exit=exit_default;
	udhcpc_register_plugin(&default_udhcpc_plugin);
	return;
}
