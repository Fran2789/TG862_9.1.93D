/**************************************************************************
 * FILE PURPOSE	:  	udhcpc sample plugin
 **************************************************************************
 * FILE NAME	:   udhcpc_sample_plugin.c
 *
 * DESCRIPTION	:
 *  The file contains the default plugin implementation that can be 
 *  overridden by custom plugins functions
 *
 *	CALL-INs:
 *
 *	CALL-OUTs:
 *
 *	User-Configurable Items:
 *
 *	(C) Copyright 2006, Texas Instruments, Inc.
 *************************************************************************/

#include "udhcpc_plugin.h"
#include "udhcp_debug.h"

UDHCPC_PLUGIN_MCB   sample_udhcpc_plugin;

struct vendorid_option 
{
	int vendor_id_present;
	char vendor_id[40];
	char additional_vendor_info[40];
};

struct vendorid_option vendor_info;

static int cmd_option_parser(int *argc, char *argv[]);
static short add_param_request_list(udhcpc_client_state state,char msg_type, char *option_data);
static UDHCP_ERROR parse_option(udhcpc_client_state state,char msg_type, char *option);
static short add_options(udhcpc_client_state state,char msg_type, char *option_data);
static int report_config(udhcpc_client_state state,UDHCPC_PARAMS *params, UDHCP_ERROR error);
static void event_logger(udhcpc_client_state curr_state,udhcpc_client_state new_state,char *event);

static int cmd_option_parser(int *argc, char *argv[])
{
	// return error since we don't have any real plugin and hence no
	// option should come here
	LOG(LOG_DEBUG,"Sample cmd_option_parser");

    if (strcmp(argv[*argc], "-vendorid") == 0)
    {
		strcpy(vendor_info.vendor_id, argv[*argc+1]);
		*argc = *argc + 2;
		printf("vendor_id = %s\n",vendor_info.vendor_id);
		vendor_info.vendor_id_present=1;
	}
	else
		*argc = *argc + 1;

	return 0;
}

static short add_param_request_list(udhcpc_client_state state,char msg_type, char *option_data)
{
	LOG(LOG_DEBUG,"Sample add_param_request_list,state = %d, msg_type = %d",(int)state,(int)msg_type);
	*option_data=55;
	*(option_data+1)=4;
	*(option_data+2)= 1;
	*(option_data+3)= 3;
	*(option_data+4)= 6;
	*(option_data+5)=0x3c;
	return 6;
}


static UDHCP_ERROR parse_option(udhcpc_client_state state,char msg_type, char *option_data)
{
	int i;
	char opt_code;
	char opt_len;
	opt_code = *option_data;
	opt_len  = *(option_data+1);
	LOG(LOG_DEBUG,"Sample: parse_plugin_option state = %d,opt code = %d, len = %d, data = ",state, opt_code, opt_len);
	for (i=0; i < opt_len; i++)
		printf("%x,",*(option_data+2+i));	
	LOG(LOG_DEBUG,"\n");
	return 0;
} 

static int report_config(udhcpc_client_state state,UDHCPC_PARAMS *params, UDHCP_ERROR error)
{
	
	struct in_addr temp_addr,temp_inet;
	char **temp;
	UDHCPC_PACKET_STATS stats;
	char tempstr[40];
	
	LOG(LOG_DEBUG,"Sample: report_config, state = %d", (unsigned int)state);
	if (!error)
	{
 		temp = params->options;
		LOG(LOG_DEBUG,"message_type=%d",params->msg_type);
		LOG(LOG_DEBUG,"yiaddr=%s",inet_ntoa(params->message->yiaddr));
		while(*temp)
		{
			LOG(LOG_DEBUG,"%s",*temp);
			temp++;
		}
		sprintf(tempstr,"/sbin/ifconfig nas0 %s",inet_ntoa(params->message->yiaddr)); 
		system(tempstr);
	}
	else
	{
		LOG(LOG_DEBUG,"Sample: Error reporting : error = %d",error);
	}
	
	udhcpc_get_stats(&stats);
	
	LOG(LOG_DEBUG,"stats.state=%d",stats.state);
	LOG(LOG_DEBUG,"stats.discover=%d",stats.discover);
	LOG(LOG_DEBUG,"stats.request=%d",stats.request);
	LOG(LOG_DEBUG,"stats.offer=%d",stats.offer);
	LOG(LOG_DEBUG,"stats.ack=%d",stats.ack);
	LOG(LOG_DEBUG,"stats.nack=%d",stats.nack);
	LOG(LOG_DEBUG,"stats.release=%d",stats.release);
	LOG(LOG_DEBUG,"stats.renew=%d",stats.renew);
	LOG(LOG_DEBUG,"stats.config_attempts=%d",stats.config_attempts);

	return 0;
}

static void event_logger(udhcpc_client_state curr_state,udhcpc_client_state new_state,char *event)
{
		char temp[100];
		sprintf(temp,"event_logger:%s:current state= %d,new state = %d",event,(unsigned int)curr_state,(unsigned int)new_state);
		LOG(LOG_DEBUG,temp);
}

static short add_options(udhcpc_client_state state,char msg_type, char *option_data)
{
	int len;
	LOG(LOG_DEBUG,"Sample : add_plugin_options");
	if (vendor_info.vendor_id_present)
	{
 		len = strlen(vendor_info.vendor_id);
		*option_data = 0x3c;
		strcpy(option_data+2,vendor_info.vendor_id);
		*(option_data+1) = len;
		return len+2;
	}
	return 0;
}

void udhcpc_plugin_init()
{
	
	LOG(LOG_DEBUG,"Sample plugin_init");
	bzero((void *)&sample_udhcpc_plugin,sizeof(UDHCPC_PLUGIN_MCB));
	sample_udhcpc_plugin.major_version=1;
	sample_udhcpc_plugin.minor_version=0;
	strcpy(sample_udhcpc_plugin.plugin_name,"Sample");
	vendor_info.vendor_id_present=0;
	sample_udhcpc_plugin.cmd_option_parser=cmd_option_parser;
	sample_udhcpc_plugin.add_param_request_list=add_param_request_list;
	sample_udhcpc_plugin.add_options=add_options;
	sample_udhcpc_plugin.parse_option=parse_option;
	sample_udhcpc_plugin.report_config=report_config;
	//sample_udhcpc_plugin.event_logger=event_logger;
	if (udhcpc_register_plugin(&sample_udhcpc_plugin)<0)
	{
		LOG(LOG_DEBUG,"error in intialising Sample plugin");
	}
}
