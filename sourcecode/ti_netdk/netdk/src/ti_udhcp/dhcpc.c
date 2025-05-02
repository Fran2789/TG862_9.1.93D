/* dhcpc.c
 *
 * udhcp DHCP client
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
Includes Intel Corporation's changes/modifications dated: 2006-2014. 
Changed/modified portions - Copyright © 2006-2014, Intel Corporation.
The changes are:

1. The server_config structure is extended to an array to define dhcp server 
configuration on a per interface basis. NSP supports multiple lan groups 
and requires dhcp server configuration per lan groups. These configurations 
are saved in the server_config array. udhcp server supports configuration for
upto 6 interfaces.
2. Modified the main() function accordingly to listen on upto 6 sockets. 
lease_file is therefore defined on a per interface basis. auto_time variable 
(timeout_end) is extended to an array to hold 6 entries. 
3. Added ability to save an offer that was valid till timeout. This will allow
waiting for a better offer till timeout. 
4. fixed timing issue and confirmed listening socket is open before sending 
renew request
5. Do not exit in SIGTERM signal handler, since this may cause ICC lock. Instead,
streamline the SIGTERM signal into the main "select" by registring it using signalfd()
-------------------------------------------------------------------------------------*/

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/signalfd.h>

#include "dhcpc.h"
#include "options.h"
#include "clientpacket.h"
#include "udhcp_packet.h"
#include "script.h"
#include "socket.h"
#include "udhcp_debug.h"
#include "udhcp_alloc.h"
#include "arpping.h"
#include "stdarg.h"

//ARRIS ADD START
#ifdef CONFIG_DMALLOC_ARRIS
#include "dmalloc/dmalloc.h"
#endif
// ARRIS ADD END

static udhcpc_client_state state;
static unsigned long requested_ip; /* = 0 */
static unsigned long server_addr;
static unsigned long timeout;
static unsigned int packet_num; /* = 0 */
static int fd;
static int failurecnt = 0;
static unsigned int retransmit_time;
static unsigned int send_final_packet;
static int exiting; // ARRIS
// UNIHAN ADD START
// For follow RFC2131 4.4.5 section
#define MIN_REMAINING_LEASE_TIME (unsigned long)(60)
// UNIHAN ADD EDN

#define GENERATE_RANDOM_NUM  -1 + (int) (3.0 * rand()/(RAND_MAX + 1.0))


// Following added to support plugin to handle vertical specific options
UDHCPC_MCB udhcpc_mcb;
UDHCPC_PACKET_STATS udhcpc_stats;

extern void default_plugin_init(void);
extern UDHCP_ERROR get_packet(struct dhcpMessage *packet, int fd, int *len);
extern UDHCP_ERROR get_raw_packet(struct dhcpMessage *payload, int fd, int *len);
static int handle_dhcp_info(udhcpc_client_state state, char msg_type,UDHCP_ERROR error, 
                            struct dhcpMessage *packet);
#define LISTEN_NONE 0
#define LISTEN_KERNEL 1
#define LISTEN_RAW 2
static int listen_mode;
static void* ptr_plugin = NULL;

struct udhcp_client_config_t udhcp_client_config = {
	/* Default options. */
	abort_if_no_lease: 0,
	foreground: 0,
	quit_after_lease: 0,
	interface: "eth0",
	clientid: NULL,
	hostname: NULL,
	ifindex: 0,
	arp: "\0\0\0\0\0\0",		/* appease gcc-3.0 */
	backoff_time: 4,		
};

unsigned long get_curr_server(void)
{
    return server_addr;
}

int get_sysUpTime(unsigned int *sysUpTimeVal)
{
    FILE *fp;
    char line[MAX_LINE_SIZE];
    char *ret_val;
    unsigned int upTime = 0;
    
   /* This file contains two numbers:
    * the uptime of the system (seconds), and the amount of time spent in idle process (seconds). 
    * We care only for the first one */
    fp = fopen( UPTIME_FILE_PATH, "r");
    if (fp == NULL)
    {
        return -1;
    }
   
    ret_val = fgets(line,MAX_LINE_SIZE,fp);
    fclose(fp);

    if (ret_val == NULL)
    {
        return -1;
    }

    /* Extracting the first token (number of up-time in seconds). */
    ret_val = strtok (line," .");

    /* we need only the number of seconds */
    upTime += atoi(ret_val);

    *sysUpTimeVal = upTime;

    return 0;
}

udhcpc_client_state udhcp_get_client_state(void)
{
	return state;
}

static void print_usage(void)
{
	printf(
"Usage: ti_udhcpc [OPTIONS]\n\n"
"  -H, --hostname=HOSTNAME         Client hostname\n"
"  -i, --interface=INTERFACE       Interface to use (default: eth0)\n"
"  -n, --now                       Exit with failure if lease cannot be\n"
"                                  immediately negotiated.\n"
"  -q, --quit                      Quit after obtaining lease\n"
"  -r, --request=IP                IP address to request (default: none)\n"
"  -b, --backoff_time=BACKOFF_SEC  Initial time in sec to wait for before retransmitting DHCP DISCOVER/REQUEST/RENEW message(default: 4 sec)\n"
"  -plugin <plugin.so>             plugin to customize ti_dhcpc\n"
"  -v, --version                   Display version\n"
	);
}


/* just a little helper */
static void change_mode(int new_mode)
{
	LOG(LOG_DEBUG,"entering %s listen mode",new_mode ? (new_mode == 1 ? "kernel" : "raw") : "none");
	if(fd)
		close(fd);
	fd = -1;
	listen_mode = new_mode;
}


/* SIGUSR1 handler (renew) */
static void renew_requested(int sig)
{
	sig = 0;
 
	if (state == BOUND || state == RENEWING || state == REBINDING)
  	{
	  	change_mode(LISTEN_KERNEL);
		packet_num = 0;
		udhcpc_stats.state=UDHCPC_DOWN;
		state = RENEW_REQUESTED;
		udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
	}
	else if (state == RELEASED) {
    	packet_num = 0;
		change_mode(LISTEN_RAW);
		state = INIT_SELECTING;
		udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
	}
	LOG(LOG_INFO, "Received SIGUSR1");

	/* Kill any timeouts because the user wants this to hurry along */
	timeout = 0;
}


/* SIGUSR2 handler (release) */
static void release_requested(int sig)
{
	sig = 0;
	/* send release packet */

	if (state == BOUND || state == RENEWING || state == REBINDING) 
	{
	
		send_release(server_addr, requested_ip); /* unicast */
		udhcpc_mcb.plugin.event_logger(state,RELEASED,"Released Lease");
	}
	LOG(LOG_INFO, "Received SIGUSR2");
    // ARRIS ADD - fix race condition exception
    if( exiting == 1 )
    {
        LOG(LOG_INFO, "Ignoring SIGUSR2 since we are exiting");
        return;
    }
    // END ARRIS
    change_mode(LISTEN_NONE);
	state = RELEASED;
    udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
	timeout = 0x7fffffff;
}

/* Exit and cleanup */
static void exit_client(int retval)
{
    // ARRIS ADD
    unsigned int now = 0;

    // ARRIS set flag indicating we are in the process of existing dhcp
    exiting = 1;

    /* send release packet */
   if (state == BOUND || state == RENEWING || state == REBINDING) 
   {
		send_release(server_addr, requested_ip); /* unicast */
		udhcpc_mcb.plugin.event_logger(state,RELEASED,"Released Lease");
    }
	
    get_sysUpTime( &now );  // ARRIS MODIFY
    udhcpc_mcb.plugin.report_lease_times(now, 0, 0, 0);
    // END ARRIS ADD

	udhcpc_mcb.plugin.exit();
	CLOSE_LOG();
	if( ptr_plugin )
		dlclose(ptr_plugin);
	exit(retval);
}

void print_config(void)
{
	printf ("quit_after_lease = %d\n",(int)udhcp_client_config.quit_after_lease);
	printf ("abort_if_no_lease= %d\n",(int)udhcp_client_config.abort_if_no_lease);
	printf ("backoff_time= %d\n",(int)udhcp_client_config.backoff_time);
	if (udhcp_client_config.interface)
		printf ("interface= %s\n",udhcp_client_config.interface);
	if (udhcp_client_config.clientid)
		printf ("clientid= %s\n",udhcp_client_config.clientid+3);
	if (udhcp_client_config.hostname)
		printf ("hostname= %s\n",udhcp_client_config.hostname);
	return;
}

int udhcp_option_parse (int argc, char* argv[])
{
    int len, index = 0;

    /* Extract the version information */
    sscanf(
		VERSION, 
		"%hu.%hu", (unsigned short *)&udhcpc_mcb.major_version, 
		(unsigned short *)&udhcpc_mcb.minor_version
		);

    /* Verify if some options have been passed or not? */
    if (argc == 1)
    {
        /* No options passed cannot proceed. */
        print_usage();
        return -1;
    }

    /* Skip the executable name and start with the arguments. */
    index = 1;

    /* Parse all the options. */
    while (index < argc)
    {
        if (strcmp(argv[index], "-H") == 0)
        {
			len = strlen(argv[index+1]) > 255 ? 255 : strlen(argv[index+1]);
			if (udhcp_client_config.hostname) udhcp_free(udhcp_client_config.hostname);
			udhcp_client_config.hostname = udhcp_alloc(len + 2);
			udhcp_client_config.hostname[OPT_CODE] = DHCP_HOST_NAME;
			udhcp_client_config.hostname[OPT_LEN] = len;
			strncpy(udhcp_client_config.hostname + 2,argv[index+1] , len);
            index = index + 2;
        }
        else if (strcmp(argv[index], "-h") == 0)
		{
			print_usage();
            index = index + 1;
		}
        else if (strcmp(argv[index], "-i") == 0)
		{
			udhcp_client_config.interface =  argv[index+1];	
            index = index + 2;
		}
        else if (strcmp(argv[index], "-n") == 0)
		{
			udhcp_client_config.abort_if_no_lease = 1;
            index = index + 1;
		}
        else if (strcmp(argv[index], "-p") == 0)
		{
			udhcp_client_config.interface =  argv[index+1];	
            index = index + 1;
		}
        else if (strcmp(argv[index], "-q") == 0)
		{
			udhcp_client_config.quit_after_lease = 1;
            index = index + 1;
		}
        else if (strcmp(argv[index], "-r") == 0)
		{
			requested_ip = inet_addr(argv[index+1]);
            		index = index + 2;
		}	
        else if (strcmp(argv[index], "-v") == 0)
		{
			printf("ti_udhcpc, version %s\n\n", VERSION);
			exit_client(0);
		}
        else if (strcmp(argv[index], "-b") == 0)
        {
            if(atoi(argv[index+1]) < 1 || atoi(argv[index+1]) > CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME)
            {
                printf("ti_udhcpc: DHCPDISCOVER / DHCPREQUEST retransmission backoff time range: [1,64]sec  \n");
                exit(0);
            }
            udhcp_client_config.backoff_time =  atoi(argv[index+1]);
           	index = index + 2;
        }
        else if (strcmp(argv[index], "-plugin") == 0)
        {            
            void (*ptr_plugin_init)(void); 

            /* Load the library. */
            ptr_plugin = dlopen(argv[index+1], RTLD_GLOBAL | RTLD_NOW);
            if (ptr_plugin == NULL)
            {
                LOG(LOG_ERR, "Unable to load plugin %s Error: %s.\n", argv[index+1], dlerror());
                return -1;
            }

            /* Initialize the plugin */
            ptr_plugin_init = dlsym (ptr_plugin, "udhcpc_plugin_init");
            if (ptr_plugin_init == NULL)
            {
                LOG(LOG_ERR, "Unable to initialize plugin %s Error: %s.\n", argv[index+1], dlerror());
                dlclose(ptr_plugin);
                return -1;
            }

            /* Initialize the Plugin. */
            ptr_plugin_init();

            index = index + 2;            
        }
        else
        {
            /* This could be a plugin specific option. */
			UDHCPC_PLUGIN_MCB* plugin_ptr = &udhcpc_mcb.plugin;
            int               option_processed = -1;

            /* Does this plugin expect options?  Call the option handler for the plugin! */
            option_processed = plugin_ptr->cmd_option_parser(&index, argv);

            /* Is this an invalid option for the plugin? */
            if (option_processed < 0)
            {
                LOG(LOG_ERR, "ti_udhcpc Error: Plugin %s rejected the option\n", 
                       plugin_ptr->plugin_name);
				index++;
                return -1;
            }
		}
	}
	return 0;
}



/* SIGTERM handler */
static void terminate(int sig)
{
	sig = 0;
	LOG(LOG_ERR,"Received SIGTERM");
	exit_client(0);
}


static int handle_plugin_options(char msg_type, struct dhcpMessage *packet)
{
	int i, length, j;
	unsigned char *optionptr;
	int over = 0, done = 0, curr = OPTION_FIELD;
	unsigned char option_code, option_len; 
	int found, res;
	
	optionptr = packet->options;
	i = 0;
	length = CONFIG_TI_TIUDHCPC_MAX_OPTION_BUFSIZE;
	while (!done) {
		found =0;
		if (i >= length) {
			LOG(LOG_ERR,"bogus packet, option fields too long.");
			return UDHCP_RFC_VIOLATION;
		}
		option_code = optionptr[i + OPT_CODE];
		option_len = optionptr[i + OPT_LEN]; 
		
		if (i + 1 + option_len  >= length) {
				LOG(LOG_ERR, "bogus packet, option fields too long.");
				break;
		}

		switch (option_code) {

		case DHCP_PADDING:
			i++;
			break;
		case DHCP_OPTION_OVER:
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				LOG(LOG_ERR,"bogus packet, option fields too long.");
				return UDHCP_RFC_VIOLATION;
			}
			over = optionptr[i + 3];
			i += optionptr[OPT_LEN] + 2;
                        printf("\n**********************over = %d DHCP_OPTION_OVER= %d optionptr =%s = %u ********** \n",over,option_code,optionptr,optionptr);			
			break;
		case DHCP_END:
			if (curr == OPTION_FIELD && over & FILE_FIELD) {
				optionptr = packet->file;
				i = 0;
				length = 128;
				curr = FILE_FIELD;
                                printf("\n**********************over = %d DHCP_END1= %d optionptr =%s = %u ********** \n",over,option_code,optionptr,optionptr);				
			} else if (curr == FILE_FIELD && over & SNAME_FIELD) {
				optionptr = packet->sname;
				i = 0;
				length = 64;
				curr = SNAME_FIELD;
                                printf("\n**********************over = %d DHCP_END2= %d optionptr =%s = %u ********** \n",over,option_code,optionptr,optionptr);				
			} else done = 1;
			break;
		default:
			{
				found=0;
	    		for (j = 0; options[j].code; j++) {
#ifdef INCLUDE_ARRIS_GW_TR69
//ARRIS ADD BEGIN
					if(option_code == 125)
					{
						break;
					}
//ARRIS ADD END
#endif
					if (option_code == options[j].code)
					{
						found=1;
						break;
					}
				}
				if (!found)
				{
						LOG(LOG_DEBUG,"found a plugin option\n");
						// pass the option to the plugin
						res = udhcpc_mcb.plugin.parse_option(state, msg_type,(optionptr + i));
						if (res)
							return res;
					}

					i += optionptr[OPT_LEN + i] + 2;
				}
			}
		}
		return 0;
	}

static int handle_dhcp_info(
			udhcpc_client_state state, 
			char msg_type,UDHCP_ERROR error, 
			struct dhcpMessage *packet
			)
{
	char **options=NULL, **temp1, **temp2;
	int ret;
	UDHCPC_PARAMS* udhcpc_params = NULL;

	if (packet)
	{
		udhcpc_params = (UDHCPC_PARAMS *) udhcp_alloc(sizeof (UDHCPC_PARAMS));

		if (!udhcpc_params)
		{
			LOG(LOG_ERR,"Error in udhcp_alloc for recvd_message \n");
			return -1;
		}
		options = fill_envp(packet);

		udhcpc_params->message= packet;
		udhcpc_params->msg_type = msg_type;
		udhcpc_params->options = options;
	}
	ret=udhcpc_mcb.plugin.report_config(state, udhcpc_params, error);	

	if (options)
	{
		temp1 = options;
		while(*temp1)
		{
			temp2=temp1;
			temp1++;
			udhcp_free(*temp2);
		}
		udhcp_free (options);
	}

    if(udhcpc_params)
        {
/* ARRIS MODIFY */
           printf("\nMessage type = %d\n",msg_type);
           udhcpc_mcb.plugin.convert_packet_to_log(udhcpc_params->message);
/* END ARRIS MODIFY */
        udhcp_free(udhcpc_params);
        }

	return ret;
}

/* END ARRIS MODIFY */

#ifdef COMBINED_BINARY
int udhcpc(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	unsigned char *temp, *message;
	unsigned long t1 = 0, t2 = 0, xid = 0, remaining_t1 = 0, remaining_t2 = 0;
	unsigned long start = 0;
	fd_set rfds;
	int retval;
	struct timeval tv;
	int len;
	struct dhcpMessage packet;
	UDHCP_ERROR error;
	char text[30];
	unsigned char hwaddr[6];
	unsigned int sysUpTime = 0;
	int sysRetVal = -1;
	unsigned int now = 0;
	int sigterm_fd;
	sigset_t sigterm_mask;
	int fds;

    /* Following params are used to save a valid offer*/
    unsigned long last_xid;
    unsigned long last_server_addr;
    unsigned long last_requested_ip;
#ifdef INCLUDE_PCABLE   // ARRIS ADD    
    unsigned long expireTime=0, rebindTime=0;      
#endif     // ARRIS ADD END

	OPEN_LOG("udhcpc");

    last_xid = last_server_addr = last_requested_ip = 0;

	default_plugin_init();
	bzero((void *)&udhcpc_stats, sizeof(udhcpc_stats));

	if (udhcp_option_parse(argc,argv) < 0)
	{ 
		LOG(LOG_ERR, "udhcp client exit due to parser error\n");
		exit_client(1);
	}
	LOG(LOG_INFO," TI udhcp client (v%s) started", VERSION);

	if (read_interface(udhcp_client_config.interface, &udhcp_client_config.ifindex,
			   NULL, udhcp_client_config.arp) < 0)
		exit_client(1);

    //LOG(LOG_INFO,"TI udhcp client read_interface, udhcp_client_config.ifindex = %d", udhcp_client_config.ifindex);
    LOG(LOG_INFO," TI udhcp client read_interface, udhcp_client_config.ifindex = %d", udhcp_client_config.ifindex);

	if (!udhcp_client_config.clientid) {
		udhcp_client_config.clientid = udhcp_alloc(6 + 3);
		udhcp_client_config.clientid[OPT_CODE] = DHCP_CLIENT_ID;
		udhcp_client_config.clientid[OPT_LEN] = 7;
		udhcp_client_config.clientid[OPT_DATA] = 1;
		memcpy(udhcp_client_config.clientid + 3, udhcp_client_config.arp, 6);
	}

	/* setup signal handlers */
	signal(SIGUSR1, renew_requested);
	signal(SIGUSR2, release_requested);
	//signal(SIGTERM, terminate);
	/* Init sigmask */
	if (sigemptyset(&sigterm_mask) != 0)
	{
		int err = errno;
		LOG(LOG_ERR, "FATAL: failed sigemptyset() - %s\n", strerror(err));
		exit_client(1);
	}
	/* Add sigterm to mask */
	if (sigaddset(&sigterm_mask, SIGTERM) != 0)
	{
		int err = errno;
		LOG(LOG_ERR, "FATAL: failed sigaddset() - %s\n", strerror(err));
		exit_client(1);
	}
	/* Block sigterm */
	if (sigprocmask(SIG_BLOCK, &sigterm_mask, NULL) != 0)
	{
		int err = errno;
		LOG(LOG_ERR, "FATAL: failed sigprocmask() - %s\n", strerror(err));
		exit_client(1);
	}
	/* Create fd for sigterm */
	if ((sigterm_fd = signalfd(-1, &sigterm_mask, 0)) < 0)
	{
		int err = errno;
		LOG(LOG_ERR, "FATAL: failed signalfd() - %s - %d\n", strerror(err), err);
		exit_client(1);
	}

	state = INIT_SELECTING;
    udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
	udhcpc_mcb.plugin.event_logger(state,state,"startup");
	change_mode(LISTEN_RAW);



	for (;;) {
		if((sysRetVal = get_sysUpTime(&sysUpTime)) == -1) {
			LOG(LOG_ERR, "FATAL: couldnt retrieve system up time from kernel \n");
			exit_client(0);
		}
		
		tv.tv_sec = timeout - sysUpTime;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);

		if (listen_mode != LISTEN_NONE && fd < 0) {
			if (listen_mode == LISTEN_KERNEL)
				fd = listen_socket(INADDR_ANY, CLIENT_PORT, udhcp_client_config.interface);
			else
				fd = raw_socket(udhcp_client_config.ifindex);
			if (fd < 0) {
				LOG(LOG_ERR,"FATAL: couldn't listen on socket\n");
				exit_client(0);
			}
		}
		if (fd >= 0) FD_SET(fd, &rfds);
		fds = fd;
		if (sigterm_fd >= 0) FD_SET(sigterm_fd, &rfds);
		if (sigterm_fd > fd)
		{
			fds = sigterm_fd;
		}

		if (tv.tv_sec > 0) {
      
			retval = select(fds + 1, &rfds, NULL, NULL, &tv);
		} else retval = 0; /* If we already timed out, fall through */

		if((sysRetVal = get_sysUpTime(&sysUpTime)) == -1) {
			LOG(LOG_ERR, "FATAL: couldnt retrieve system up time from kernel \n");
			exit_client(0);
		}

		now = sysUpTime;
	
		if (retval == 0) {
			/* timeout dropped to zero */
			switch (state) {
			case INIT_SELECTING:

                /* check if we have a saved offer and use it to send select*/
                if ( 0 != last_xid)
                {
                    server_addr = last_server_addr;
                    xid = last_xid;
                    requested_ip = last_requested_ip;
                    last_xid = last_server_addr = last_requested_ip = 0;

                    /* enter requesting state */
                    state = REQUESTING;
                    timeout = now;
                    packet_num = 0;
                    break;
                }
                /* Do exponential back off for retransmitting DHCPDISCOVER message  */
                if (packet_num == 0) 
                    retransmit_time = udhcp_client_config.backoff_time; 
                else
                    retransmit_time <<= 1; 

                /* We just backed off the last time. we need to send a packet before we quit */
                if(send_final_packet == 1) {
                    /* send the last discover packet */
                    // ARRIS MOD - move counter increment above send_discover()
                    udhcpc_stats.discover++;
                    send_discover(xid, requested_ip); /* broadcast */
                    //udhcpc_stats.discover++;
                    send_final_packet = 0;
                }

				if (retransmit_time <= CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME) 
                {
					if (packet_num == 0)
						xid = random_xid();

					/* send discover packet */
                    // ARRIS MOD - move counter increment above send_discover()
                    udhcpc_stats.discover++;
					send_discover(xid, requested_ip); /* broadcast */
					//udhcpc_stats.discover++;

					timeout = now + retransmit_time + GENERATE_RANDOM_NUM;

                    /* send_final_packet flag is set only when we are backing off for the last time.
                       The next time when we wake up the retransmit_time is doubled and is more
                       than the CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME and thus we will not re-enter 
                       this if block to send the last packet. When this flag is set, it is treated 
                       as a special case and the renew/request packet is sent out for the last time.
                    */
                    if(retransmit_time <= CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME && 
                        (2 * retransmit_time > CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME))
                        send_final_packet = 1;
					packet_num++;
				}
        		else
        		{
				    if (++udhcpc_stats.config_attempts == CONFIG_TI_TIUDHCPC_MAX_CONFIG_ATTEMPTS_THRESHOLD)
					{
						handle_dhcp_info(state,0 ,UDHCP_MAX_CONFIG_ATTEMPTS_REACHED,NULL);
					}
					if (udhcp_client_config.abort_if_no_lease)
          				{
						LOG(LOG_ERR,"No lease, failing.\n");
						exit_client(1);
				  	}
					/* wait to try again */
          			failurecnt++;
					packet_num = 0;
					timeout = now + CONFIG_TI_TIUDHCPC_RESTART_DELAY;
					udhcpc_mcb.plugin.event_logger(state,state,"No lease from Server: Restarting");
				}
				break;
			case RENEW_REQUESTED:
			case REQUESTING:
                /* Do exponential back off for retransmitting DHCPREQUEST/DHCPRENEW messages */
                if (packet_num == 0) 
                    retransmit_time = udhcp_client_config.backoff_time; 
                else 
                    retransmit_time <<= 1; 

                /* We just backed off the last time. we need to send a packet before we quit */
                if(send_final_packet == 1) {
					/* send the last renew/request packet */
					if (state == RENEW_REQUESTED)
					{
						udhcpc_stats.renew++;
						send_renew(xid, server_addr, requested_ip); /* unicast */
					}
					else 
					{
						udhcpc_stats.request++;
						send_selecting(xid, server_addr, requested_ip); /* broadcast */
					}
                    send_final_packet = 0;
                }

				if (retransmit_time <= CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME) 
        		{
					/* send request packet */
					if (state == RENEW_REQUESTED)
					{
						udhcpc_stats.renew++;
						send_renew(xid, server_addr, requested_ip); /* unicast */
					}
					else 
					{
						udhcpc_stats.request++;
						send_selecting(xid, server_addr, requested_ip); /* broadcast */
					}

					timeout = now + retransmit_time + GENERATE_RANDOM_NUM;
                    /* send_final_packet flag is set only when we are backing off for the last time.
                       The next time when we wake up the retransmit_time is doubled and is more
                       than the CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME and thus we will not re-enter 
                       this if block to send the last packet. When this flag is set, it is treated 
                       as a special case and the renew/request packet is sent out for the last time.
                    */
                    if(retransmit_time <= CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME && 
                        (2 * retransmit_time > CONFIG_TI_TIUDHCPC_MAX_BACKOFF_TIME))
                        send_final_packet = 1;
					packet_num++;
				}
				else
        			{
					if (++udhcpc_stats.config_attempts == CONFIG_TI_TIUDHCPC_MAX_CONFIG_ATTEMPTS_THRESHOLD)
					{
						handle_dhcp_info(state,0 ,UDHCP_MAX_CONFIG_ATTEMPTS_REACHED,NULL);
					}
					/* timed out, go back to init state */
					udhcpc_mcb.plugin.event_logger(state,INIT_SELECTING,"No lease from Server: Restarting");
					state = INIT_SELECTING;
                    udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
					timeout = now + CONFIG_TI_TIUDHCPC_RESTART_DELAY;
					packet_num = 0;
					change_mode(LISTEN_RAW);
				}
				break;
			case BOUND:
				udhcpc_mcb.plugin.event_logger(state,RENEWING,"T1 Expiry - Lease starting to expire");
				/* Lease is starting to run out, time to enter renewing state */
				state = RENEWING;
                udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
				change_mode(LISTEN_KERNEL);
				/* fall right through */
			case RENEWING:
				/* Either set a new T1, or enter REBINDING state */
				if ((t2 - t1) <= (udhcp_client_config.lease/ 14400 + 1)) {
					/* timed out, enter rebinding state */
					udhcpc_mcb.plugin.event_logger(state,REBINDING,"T2 Expiry - Lease Server failed to renew lease");
					state = REBINDING;
                    udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
					timeout = now + (t2 - t1);
					change_mode(LISTEN_RAW);		// SERCOMM ADD : T2 expire sending raw BC, need to enter raw listening mode
				} else {
					/* send a request packet */
					udhcpc_stats.renew++;
					send_renew(xid, server_addr, requested_ip); /* unicast */

                    remaining_t1 = (t2 - t1) / 2;
                    // UNIHAN MOD START
                    // For follow RFC2131 4.4.5 section
                    if (remaining_t1 < MIN_REMAINING_LEASE_TIME)
                    {
                        remaining_t1 = MIN_REMAINING_LEASE_TIME;
                    }

                    if ( (remaining_t1 + t1) > t2)
                    {
                        timeout = (t2 - t1) + now;
                        t1 = t2;
                    }
                    else
                    {
                        timeout = remaining_t1 + now;
                        t1 = remaining_t1 + t1;
                    }
                    // UNIHAN MOD END
				}
				break;
			case REBINDING:
				/* Either set a new T2, or enter INIT state */
				if ((udhcp_client_config.lease - t2) <= (udhcp_client_config.lease / 14400 + 1)) {
					/* timed out, enter init state */
					handle_dhcp_info(state,0 ,UDHCP_LEASE_LOST ,NULL);
					udhcpc_mcb.plugin.event_logger(state,INIT_SELECTING,"Lease lost, entering init state");
					state = INIT_SELECTING;
                    udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
					udhcpc_stats.state = UDHCPC_DOWN;
					timeout = now;
					packet_num = 0;
					change_mode(LISTEN_RAW);
				} else {
					/* send a request packet */
					udhcpc_stats.renew++;
					send_renew(xid, 0, requested_ip); /* broadcast */

                    remaining_t2 = (udhcp_client_config.lease - t2) / 2;
                    // UNIHAN MOD START
                    // For follow RFC2131 4.4.5 section
                    if (remaining_t2 < MIN_REMAINING_LEASE_TIME)
                    {
                        remaining_t2 = MIN_REMAINING_LEASE_TIME;
                    }
                    
                    if ( (t2 + remaining_t2) > udhcp_client_config.lease)
                    {
                        timeout = (udhcp_client_config.lease - t2) + now;
                        t2 = udhcp_client_config.lease;
                    }
                    else
                    {
                        timeout = remaining_t2 + now;
                        t2 = remaining_t2 + t2;
                    }
                    // UNIHAN MOD END
				}
				break;
			case RELEASED:
				/* yah, I know, *you* say it would never happen */
				timeout = 0x7fffffff;
				break;
			default:
				break;
			}
		} else if (retval == -1 && errno == EINTR) {
			/* a signal was caught */

		} else {
			if (retval > 0 && FD_ISSET(sigterm_fd, &rfds)) {
				/* Got SIGTERM */
				terminate(SIGTERM);
		} else if (retval > 0 && listen_mode != LISTEN_NONE && FD_ISSET(fd, &rfds)) {
			/* a packet is ready, read it */

			if (listen_mode == LISTEN_KERNEL)
				error = get_packet(&packet, fd, &len);
			else error = get_raw_packet(&packet, fd, &len);

			if (error == UDHCP_SOCKET_ERROR && errno != EINTR) {
				strerror_r(errno, text, 50);
				LOG(LOG_ERR, "error on read, reopening socket:");
				change_mode(listen_mode); /* just close and reopen */
			}
			if (error) 
			{
				handle_dhcp_info(state,0 ,error,NULL);
				continue;
			}

			if (packet.xid != xid) {
				continue;
			}

			if ((message = get_option(&packet, DHCP_MESSAGE_TYPE,&len)) == NULL) {
				LOG(LOG_ERR,"couldnt get option from packet -- ignoring\n");
				continue;
			}
                                                                                                   
			switch (state) {
			case INIT_SELECTING:
				/* Must be a DHCPOFFER to one of our xid's */
				if (*message == DHCPOFFER) 
				{
                    int ret;
					udhcpc_stats.offer++;
					if ((temp=get_option(&packet, DHCP_SERVER_ID,&len))) 
					{
						if (!(error = handle_plugin_options(DHCPOFFER,&packet)))
						{
					 		if ((ret = handle_dhcp_info(state,DHCPOFFER,UDHCP_SUCCESS,&packet)) == UDHCP_SUCCESS)
							{
								memcpy(&server_addr,temp,4);
								xid = packet.xid;
								requested_ip = packet.yiaddr;
                                last_xid = last_server_addr = last_requested_ip = 0;

								/* enter requesting state */
								udhcpc_mcb.plugin.event_logger(state,RENEWING,"Got Offer from Server");
								state = REQUESTING;
                                udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
								timeout = now;
								packet_num = 0;
							}
							else if ( ret == UDHCP_INTERNAL_SAVE_AND_WAIT)
                            {
                                memcpy(&last_server_addr,temp,4);
								last_xid = packet.xid;
								last_requested_ip = packet.yiaddr;
                            }
							else
							{
								LOG(LOG_ERR,"Plugin rejected Config\n");
							}
						} 
						else 
						{
					 		handle_dhcp_info(state,DHCPOFFER,error,NULL);
							LOG(LOG_ERR,"Plugin rejected Option\n");
						}
					}
			   		else 
                    {
						LOG(LOG_ERR,"No server ID in message\n");
			    	}
					// ARRIS ADD START
                    if ((temp=get_option(&packet, DHCP_VENDOR_SPEC,&len))) 
                    {
                        udhcpc_mcb.plugin.tr069_v4_debug( "Got DHCP Option 43");
                        udhcpc_mcb.plugin.tr069_parser_opt43(temp);
                    }
                    else
                    {
                        udhcpc_mcb.plugin.tr069_v4_debug( "No DHCP Option 43");
                    }
					// ARRIS ADD END

		    	}
				break;
			case RENEW_REQUESTED:
			case REQUESTING:
			case RENEWING:
			case REBINDING:
				if (*message == DHCPACK) 
				{
					udhcpc_stats.ack++;
					if (!(temp = get_option(&packet, DHCP_LEASE_TIME,&len))) 
					{
						LOG(LOG_WARNING,"No lease time with ACK, using 1 hour lease\n");
						udhcp_client_config.lease = 60 * 60;
					} else {
						memcpy(&udhcp_client_config.lease, temp, 4);
						udhcp_client_config.lease = ntohl(udhcp_client_config.lease);
                    } // ARRIS MODIFY
                                                /* RFC 2131 - Section 3.1.5 - Duplicate address detection */
                                                if(arpping(packet.yiaddr, 0, udhcp_client_config.arp,
                                                           udhcp_client_config.interface, hwaddr) == 0)
                                                {
                                                        /* IP address collision detected on network */
                                                        /* Resetting State machine */
                                                        send_decline(packet.xid, packet.yiaddr, packet.siaddr);
                                                        udhcpc_mcb.plugin.event_logger(state,INIT_SELECTING,"IP address collision detected, entering init state"); 
                                                        // ARRIS ADD : Add the hw address we collided with to the event string
                                                        LOG(LOG_INFO,"IP address collision (with %02X:%02X:%02X:%02X:%02X:%02X)", 
                                                                hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
                                                        // END ARRIS
                                                        change_mode(LISTEN_RAW);
                                                        state = INIT_SELECTING;
                                                        udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
                                                        timeout = now;
                                                        packet_num = 0;
                                                        sleep(10);
							continue;
                                                }
					//} ARRIS MODIFY
	
					if (!(error = handle_plugin_options(DHCPACK,&packet)))
					{
						 udhcpc_stats.state=UDHCPC_UP;
						 if (!handle_dhcp_info(state,DHCPACK,UDHCP_SUCCESS,&packet))
						 {
                                                     /* ARRIS ADD START */
                                                     unsigned char *current_server;

                                                     if ((current_server=get_option(&packet, DHCP_SERVER_ID,&len))) 
                                                     {
                                                         /* It is possible the ACK came from a different DHCP server 
                                                            update the server address if it has changed */
                                                         if( memcmp(&server_addr,current_server,4) != 0 )
                                                         {
                                                             memcpy(&server_addr,current_server,4);
                                                             LOG(LOG_INFO,"DHCP server has changed, updating address\n");
                                                         }
                                                     }
                                                     /* ARRIS ADD END */

                         #ifdef CONFIG_TI_TIUDHCPC_T1_T2_OVERRIDE         

                            /* Read DHCP_T2 (59) from packet. If not present/has invalid value,
                             * fallback to T2 defaults, i.e. T2 = 0.875 * lease.
                             */
					        if (!(temp = get_option(&packet, DHCP_T2,&len))) 
					        {
						        LOG(LOG_INFO,"No DHCP_T2 option, by default T2=lease * 0.875\n");
							    /* little fixed point for n * .875 */
							    t2 = (udhcp_client_config.lease * 0x7) >> 3;
					        } else {
						        memcpy(&t2, temp, 4);
						        t2 = ntohl(t2);
                                if((t2 <= 0) || (t2 > udhcp_client_config.lease)) {
						            LOG(LOG_INFO,"Invalid DHCP_T2 option, falling back to default T2=lease * 0.875\n");
							        /* little fixed point for n * .875 */
							        t2 = (udhcp_client_config.lease * 0x7) >> 3;
                                }
                            } /* if (!(temp = get_option(&packet, DHCP_T2,&len))) */
                            LOG(LOG_DEBUG,"DHCP_T2 selected=%d\n",t2);
                            /* Read DHCP_T1 (58) from packet. If not present/has invalid value,
                             * fallback to T1 defaults, i.e. T1 = 0.5 * lease.
                             */
					        if (!(temp = get_option(&packet, DHCP_T1,&len))) 
					        {
						        LOG(LOG_INFO,"No DHCP_T1 option, by default T1=lease/2\n");
							    t1 = udhcp_client_config.lease / 2;
					        } else {
						        memcpy(&t1, temp, 4);
						        t1 = ntohl(t1);
                                if((t1 <= 0) || (t1 >= t2)) {
						            LOG(LOG_INFO,"Invalid DHCP_T1 option, falling back to default T1=lease/2\n");
                                    t1 = udhcp_client_config.lease / 2;
                                    if(t1 >= t2) {
						                LOG(LOG_INFO,"Invalid DHCP_T2/DHCP_T1 combination, falling back to default for T2=lease * 0.875\n");
							            /* little fixed point for n * .875 */
							            t2 = (udhcp_client_config.lease * 0x7) >> 3;
                                    }
                                }
                            } /* if (!(temp = get_option(&packet, DHCP_T1,&len))) */

                            #else
                            
                                /* Configure the T1, T2 by RFC defaults */
							    t1 = udhcp_client_config.lease / 2;

							    /* little fixed point for n * .875 */
							    t2 = (udhcp_client_config.lease * 0x7) >> 3;
                                
                            #endif

							start = now;
#ifdef INCLUDE_PCABLE  // ARRIS ADD
                            expireTime = start + (2 * t1);
							rebindTime = t2 + start;
#endif  // ARRIS ADD END
							timeout = t1 + start;
							requested_ip = packet.yiaddr;
							udhcpc_mcb.plugin.event_logger(state,BOUND,"Got ACK from Server");
                            udhcpc_mcb.plugin.report_lease_times(now, udhcp_client_config.lease, t1, t2);    // ARRIS ADD
							state = BOUND;
                            udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
							packet_num = 0;
							udhcpc_stats.config_attempts=0;
							change_mode(LISTEN_NONE);
						 }
						else
						{
							LOG(LOG_ERR,"Plugin rejected DHCP Config \n");
							udhcpc_stats.state=UDHCPC_DOWN;
						}
					}
					else
					{
						handle_dhcp_info(state,DHCPACK,error,NULL);
						LOG(LOG_ERR,"Plugin rejected option \n");
					}

				} 
				else if (*message == DHCPNAK) 
				{
					udhcpc_stats.nack++;
					LOG(LOG_INFO,"Received DHCP NAK\n");

#ifdef INCLUDE_PCABLE  // ARRIS ADD
                    udhcpc_mcb.plugin.delay_nak(state, expireTime, rebindTime, 
                                                       udhcpc_mcb.plugin.report_state_change, get_sysUpTime);
#endif // ARRIS ADD END

					if ((++udhcpc_stats.config_attempts == CONFIG_TI_TIUDHCPC_MAX_CONFIG_ATTEMPTS_THRESHOLD) && 
						((state == REQUESTING)  ||
						 (state == RENEW_REQUESTED)))
					{
						handle_dhcp_info(state,(char) 0 ,UDHCP_MAX_CONFIG_ATTEMPTS_REACHED, NULL);
					}
			                else 
                                        {
                                                if (!handle_dhcp_info(state,DHCPNAK,UDHCP_SUCCESS,&packet))
						{
					        udhcpc_mcb.plugin.event_logger(state,INIT_SELECTING,"Got NACK from server");
					        state = INIT_SELECTING;
                                                udhcpc_mcb.plugin.report_state_change( state );   // ARRIS ADD
					        timeout = now;
					        requested_ip = 0;
					        packet_num = 0;
					        change_mode(LISTEN_RAW);
					        sleep(3); /* avoid excessive network traffic */
				        }
						else
						{
							LOG(LOG_ERR,"Plugin rejected DHCP NAK \n");
						}
                    }
				}
				break;
			/* case BOUND, RELEASED: - ignore all packets */
				default:
				break;
			}
			} else {
			/* An error occured */
			LOG(LOG_ERR,"Error on select\n");
			}
		}
	}
	return 0;
}

