/*
 * udhcpc_plugin.c
 *
 * The file contains the implementation of all the PLUGIN API that is 
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
#include <dlfcn.h>
#include "udhcp_debug.h"
#include "dhcpc.h"

/**************************************************************************
 * FUNCTION NAME : udhcpc_get_stats()
 **************************************************************************
 * DESCRIPTION   :
 *  This function is called by plugin to get the dhcp client statistics
 * 
 * ARGS          :
 *  Ptr to user provided UDHCPC_PACKET_STATS structure           
 * 
 * RETURNS       :
 *  0 - Success, -1 Error 
 *************************************************************************/

int udhcpc_get_stats(struct UDHCPC_PACKET_STATS *stats)
{
// ARRIS temporarily remove print so basic regression can run and generate reports
//	LOG(LOG_INFO,"udhcpc_get_stats");
	if (!stats)
	{
		LOG(LOG_ERR,"udhcpc_get_stats api called with NULL pointer\n");
		return -1;	
	}
	stats->state=udhcpc_stats.state;
	stats->discover=udhcpc_stats.discover;
	stats->request=udhcpc_stats.request;
	stats->offer=udhcpc_stats.offer;
	stats->ack=udhcpc_stats.ack;
	stats->release=udhcpc_stats.release;
	stats->nack=udhcpc_stats.nack;
	stats->renew=udhcpc_stats.renew;
	stats->config_attempts=udhcpc_stats.config_attempts;
	return 0;
}

/**************************************************************************
 * FUNCTION NAME : udhcpc_register_plugin
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the API that is invoked by the plugin to place itself
 *  into the udhcpc Core application. It is assumed that the PLUGIN
 *  structure has been filled.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 *************************************************************************/
int udhcpc_register_plugin (UDHCPC_PLUGIN_MCB* ptr_plugin)
{
	int len;
    UDHCPC_PLUGIN_MCB*  ptr_udhcpc_plugin = &udhcpc_mcb.plugin;

    /* Validate the version of the plugin. */
    if (ptr_plugin->major_version < udhcpc_mcb.major_version)
    {
        LOG(LOG_ERR, "plugin register failed due to version mismatch\n");
        return -1;
    }
	ptr_udhcpc_plugin->major_version = ptr_plugin->major_version;
	ptr_udhcpc_plugin->minor_version = ptr_plugin->minor_version;
	len = strlen(ptr_plugin->plugin_name);
	if (len >= UDHCPC_MAX_PLUGIN_NAME)
	{
        LOG(LOG_ERR, "plugin register failed due to plugin name exceeding size\n");
        return -1;
    }
		
	strncpy(ptr_udhcpc_plugin->plugin_name,ptr_plugin->plugin_name,len);
	ptr_udhcpc_plugin->plugin_name[len] = (char)NULL;

	if (ptr_plugin->cmd_option_parser)
		ptr_udhcpc_plugin->cmd_option_parser = ptr_plugin->cmd_option_parser;
	if (ptr_plugin->add_param_request_list)
		ptr_udhcpc_plugin->add_param_request_list= ptr_plugin->add_param_request_list;
	if (ptr_plugin->parse_option)
		ptr_udhcpc_plugin->parse_option = ptr_plugin->parse_option;
	if (ptr_plugin->add_options)
		ptr_udhcpc_plugin->add_options= ptr_plugin->add_options;
	if (ptr_plugin->report_config)
		ptr_udhcpc_plugin->report_config= ptr_plugin->report_config;
	if (ptr_plugin->event_logger)
		ptr_udhcpc_plugin->event_logger= ptr_plugin->event_logger;
    // ARRIS ADD
	if (ptr_plugin->report_lease_times)
		ptr_udhcpc_plugin->report_lease_times= ptr_plugin->report_lease_times;
	if (ptr_plugin->report_state_change)
		ptr_udhcpc_plugin->report_state_change= ptr_plugin->report_state_change;
	if (ptr_plugin->convert_packet_to_log)
		ptr_udhcpc_plugin->convert_packet_to_log= ptr_plugin->convert_packet_to_log;
	if (ptr_plugin->delay_nak)
		ptr_udhcpc_plugin->delay_nak= ptr_plugin->delay_nak;
	if (ptr_plugin->tr069_v4_debug)
		ptr_udhcpc_plugin->tr069_v4_debug= ptr_plugin->tr069_v4_debug;
	if (ptr_plugin->delay_nak)
		ptr_udhcpc_plugin->tr069_parser_opt43= ptr_plugin->tr069_parser_opt43;
    // END ARRIS ADD
	if (ptr_plugin->exit)
        ptr_udhcpc_plugin->exit= ptr_plugin->exit;

    /* Return Success. */
    LOG(LOG_INFO, "Plugin %s registered successfully\n", ptr_plugin->plugin_name);
    return 0;
}

/**************************************************************************
 * FUNCTION NAME : udhcpc_unregister_plugin
 **************************************************************************
 * DESCRIPTION   :
 *  This function is the API that is invoked by the plugin to unregister
 *  itself from the core udhcpc application.
 *
 * RETURNS       :
 *  0   -   Success
 *  <0  -   Error
 *************************************************************************/
#if 0
int udhcp_unregister_plugin (char* ptr_name)
{
	UDHCPC_PLUGIN_MCB*   ptr_udhcpc_plugin = &udhcpc_mcb.plugin;

    /* Basic Validations: Validate the name of the plugin that is to be unregistered? */
    if (ptr_name == NULL)
    {
        LOG(LOG_ERR, "udhcpc plugin Error: No name passed for unregistering.\n");
        return -1;
    }

	bzero((void *)ptr_udhcpc_plugin, sizeof(UDHCPC_PLUGIN_MCB));

    /* The plugin has been unregistered. */
    LOG(LOG_INFO, "Plugin %s unregistered succesfully\n", ptr_name);
    return 0;
}
#endif

/**************************************************************************
 * FUNCTION NAME : udhcpc_get_interface
 **************************************************************************
 * DESCRIPTION   :
 *  This function is called by plugin to get the interface name
 *
 * RETURNS       :
 *      char *   ptr to interface name 
 *************************************************************************/
char * udhcpc_get_interface()
{
	return (udhcp_client_config.interface);
}

