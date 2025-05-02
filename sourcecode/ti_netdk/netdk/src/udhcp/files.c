/* 
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */

//-------------------------------------------------------------------------------------
// Copyright 2006, Texas Instruments Incorporated
//
// This program has been modified from its original operation by Texas Instruments
// to do the following:
// 
// 1. NSP Policy Routing Framework
// 2. Added a new text file udhcpd.host to allow consistency in host IP. 
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
//------------------------------------------------------------------------------------- 

/* Change Description:07112006
 * 1. k_arr is modified to accomodate changes for DHCP class
 * 2. Modified functions to include classindex to indicate correct server_config
 * 3. Modified create_config for special handling of option 43 and read the class info
 */

/* Change Description:09252006
 * 1. k_arr is removed and read_config modified 
 */

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#if 0
#include <linux/pr.h>
#endif

#include "debug.h"
#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "leases.h"

// UNIHAN ADD START, to support static lease
#include "static_leases.h"
#include <netinet/ether.h>
// UNIHAN ADD END

/* ARRIS ADD START */
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
char *arris_tr69_runtime_conf_file = NULL; // ARRIS ADD
struct tr69_config_t arris_tr69_conf;// ARRIS ADD
#endif
/* ARRIS ADD END */

#define LEASE_ADD		1
#define LEASE_DEL 	2

#define LEASEFILE_CLASS "/landhcps00.leases"
#define LEASEFILE_NO_CLASS "/landhcps0.leases"
#define IFID_LOCATION 9
#define CLASSID_LOCATION 10

#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
#define STATIC_ADDRESS_BUFFER_LENGTH    64 //UNIHAN MODIFY for TR069
#define STATIC_ADDRESS_ITEM_COUNT       2 //UNIHAN MODIFY for TR069
#endif //INCLUDE_ARRIS_GW_TR69
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
extern void write_to_delta(u_int8_t *chaddr, u_int32_t yiaddr, u_int8_t *hname,unsigned long leasetime,u_int8_t action,int classindex, char *devicename, int dynamic, char *optionfile, struct dhcp_option125_info *opt125);
#else
extern void write_to_delta(u_int8_t *chaddr, u_int32_t yiaddr, u_int8_t *hname,unsigned long leasetime,u_int8_t action,int classindex, char *devicename, int dynamic, char *optionfile);
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY

/* on these functions, make sure you datatype matches */
static int read_ip(char *line, void *arg)
{
	struct in_addr *addr = arg;
	struct hostent *host;
	int retval = 1;
#if 0
    unsigned int pr_mark = UDHCP_PR_MARK;
#endif

	if (!inet_aton(line, addr))
	{
    /* Adding an option which allows to pass a mark value for a particular
	 * socket. 
	 */
#if 1
		if ((host = gethostbyname(line))) 
#else
		if ((host = ti_gethostbyname(line, pr_mark))) 
#endif
			addr->s_addr = *((unsigned long *) host->h_addr_list[0]);
		else retval = 0;
	}
	return retval;
}

// UNIHAN ADD START, to support static lease
static int read_mac(const char *line, void *arg)
{
    if( NULL == ether_aton_r(line, (struct ether_addr *)arg) )
    {
        return 0;
    }
    return 1;
}
// UNIHAN ADD END

static int read_str(char *line, void *arg)
{
	char **dest = arg;
	
	if (*dest) free(*dest);
	*dest = strdup(line);
	
	return 1;
}


static int read_u32(char *line, void *arg)
{
	u_int32_t *dest = arg;
	char *endptr;
	*dest = strtoul(line, &endptr, 0);
	return endptr[0] == '\0';
}


static int read_yn(char *line, void *arg)
{
	char *dest = arg;
	int retval = 1;

	if (!strcasecmp("yes", line))
		*dest = 1;
	else if (!strcasecmp("no", line))
		*dest = 0;
	else retval = 0;
	
	return retval;
}


/* read a dhcp option and add it to opt_list */
static void read_opt(char *line, void *arg, int ifid, int classindex)
{
	struct option_set **opt_list = arg;
	char *opt, *val, *endptr;
	struct dhcp_option *option = NULL;
	int retval = 0, length = 0;
    /* Fix for alignment issues seen on ARM board. ARM has stricter
       alignment rules for int. The char buffer here is passed to
       read_ip function to process options which hold IP address values 
       (eg router, dns, subnet etc), where it is cast to int and if its not 
       aligned on word boundary, will result in Bus error on ARM. Fix is
       to force the buffer to start on word boundary.
    */
	char buffer[255] __attribute__ ((aligned));
	u_int16_t result_u16;
	u_int32_t result_u32;
	int i;

	sprintf(buffer,"%s",line);
	opt = strtok(buffer," ");
	if(!opt){
		return ;
	}
	if(!strcmp(opt, "vendorid")){
        /* ARRIS ADD START */
        if (server_config[ifid][classindex].classifier.id)
            free(server_config[ifid][classindex].classifier.id);
        /* ARRIS ADD END */
		server_config[ifid][classindex].classifier.id = strdup("vendorid");
		val = strtok(NULL,"\"");
        /* ARRIS ADD START */
        if (server_config[ifid][classindex].classifier.value)
            free(server_config[ifid][classindex].classifier.value);
        /* ARRIS ADD END */
		server_config[ifid][classindex].classifier.value = strdup(val);
		return ;
	}
	if(!strcmp(opt, "userclass")){
        /* ARRIS ADD START */
        if (server_config[ifid][classindex].classifier.id)
            free(server_config[ifid][classindex].classifier.id);
        /* ARRIS ADD END */
		server_config[ifid][classindex].classifier.id = strdup("userclass");
		val = strtok(NULL,"\"");
        /* ARRIS ADD START */
        if (server_config[ifid][classindex].classifier.value)
            free(server_config[ifid][classindex].classifier.value);
        /* ARRIS ADD END */
		server_config[ifid][classindex].classifier.value = strdup(val);
		return ;
	}
	if(!strcmp(opt, "vendorinfo")){
		val = strtok(NULL," ");
        /* ARRIS ADD START */
        if (server_config[ifid][classindex].vendorinfo)
            free(server_config[ifid][classindex].vendorinfo);
        /* ARRIS ADD END */
		server_config[ifid][classindex].vendorinfo = strdup(val);
		return ;
	}
	if (!(opt = strtok(line, " \t=")))
		return ;
	
	for (i = 0; options[i].code; i++)
		if (!strcmp(options[i].name, opt))
			option = &(options[i]);
		
	if (!option)
		return ;
	
	do {
		val = strtok(NULL, ", \t");
		if (val) {
			length = option_lengths[option->flags & TYPE_MASK];
			retval = 0;
			switch (option->flags & TYPE_MASK) {
			case OPTION_IP:
				retval = read_ip(val, buffer);
				break;
			case OPTION_IP_PAIR:
				retval = read_ip(val, buffer);
				if (!(val = strtok(NULL, ", \t/-"))) retval = 0;
				if (retval) retval = read_ip(val, buffer + 4);
				break;
			case OPTION_STRING:
				length = strlen(val);
				if (length > 0) {
					if (length > 254) length = 254;
					memcpy(buffer, val, length);
					retval = 1;
				}
				break;
			case OPTION_BOOLEAN:
				retval = read_yn(val, buffer);
				break;
			case OPTION_U8:
				buffer[0] = strtoul(val, &endptr, 0);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_U16:
				result_u16 = htons(strtoul(val, &endptr, 0));
				memcpy(buffer, &result_u16, 2);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_S16:
				result_u16 = htons(strtol(val, &endptr, 0));
				memcpy(buffer, &result_u16, 2);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_U32:
				result_u32 = htonl(strtoul(val, &endptr, 0));
				memcpy(buffer, &result_u32, 4);
				retval = (endptr[0] == '\0');
				break;
			case OPTION_S32:
				result_u32 = htonl(strtol(val, &endptr, 0));	
				memcpy(buffer, &result_u32, 4);
				retval = (endptr[0] == '\0');
				break;
			default:
				break;
			}
			if (retval) 
				attach_option(opt_list, option, buffer, length);
		};
	} while (val && retval && option->flags & OPTION_LIST);
	//return retval;
	return ;
}

// UNIHAN ADD START, to support static lease
static int read_staticlease(char *file, void *arg)
{
    FILE *fp;
    char ipaddress[17];
    char macaddr[18];
    struct in_addr ipaddr;
    struct ether_addr mac_bytes;
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    char buf[STATIC_ADDRESS_BUFFER_LENGTH] = {0};//UNIHAN MODIFY for TR069
    int ncols;//UNIHAN MODIFY for TR069
#endif //INCLUDE_ARRIS_GW_TR69
    int errorno, ret; // ARRIS ADD
    fp = fopen(file, "r");
    if( NULL == fp )
    {
        return 0;
    }

    /* ARRIS ADD START */
    do
    {
        ret = flock(fileno(fp), LOCK_SH);
        errorno = errno;
    }while (ret == -1 && errorno == EINTR);
    
    // Read it anyway if lock failed
    /* ARRIS ADD END */
    for (;;)
    {

#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
//UNIHAN MODIFY for TR069
        //each line might include 2 or 3 items, so we cannot use fscanf directly
        if (!fgets(buf, sizeof(buf), fp))
        {
            break;
        }
        
        ncols = sscanf(buf, "%s %s\n", macaddr, ipaddress);
                
        if ( ncols < STATIC_ADDRESS_ITEM_COUNT )
        {
            continue;
        }
//UNIHAN MODIFY END for TR069
#else
        if( EOF == fscanf(fp, "%s %s\n", macaddr, ipaddress) )
        {
            break;
        }
#endif //INCLUDE_ARRIS_GW_TR69
        /* Read ip */
        if( 0 == inet_aton(ipaddress, &ipaddr) )
        {
            continue;
        }

        /* Read mac */
        if( 0 == read_mac(macaddr, &mac_bytes) )
        {
            continue;
        }

        addStaticLease(arg, (uint8_t*) &mac_bytes, ipaddr.s_addr);
    }
    fclose(fp);

#if 1
    printStaticLeases(arg);
#endif

    return 1;
}
// UNIHAN ADD END

/* ARRIS ADD START */
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)

int read_tr69_runtime_config()
{
    FILE *fp;
    int errcode, ret, i;
    char buf[1024];
    char *token, *line;
    
    if (!arris_tr69_runtime_conf_file || !*arris_tr69_runtime_conf_file)
    {
        return -1;
    }

    fp = fopen(arris_tr69_runtime_conf_file, "r");
    if (!fp)
    {
        return -1;
    }

    // We might be interrupted by signal at any time
    do{
        ret = flock(fileno(fp), LOCK_SH);
        errcode = errno;
    }while (ret == -1 && errcode == EINTR);

    while (fgets(buf, sizeof(buf), fp))
    {
        if (strchr(buf, '\n')) *(strchr(buf, '\n')) = '\0';
        //strncpy(orig, buffer, 200);
		if (strchr(buf, '#')) *(strchr(buf, '#')) = '\0';
		token = buf + strspn(buf, " \t");
		if (*token == '\0') continue;
		line = token + strcspn(token, " \t=");
		if (*line == '\0') continue;
		*line = '\0';
		line++;
		/* eat leading whitespace */
		line = line + strspn(line, " \t=");
		/* eat trailing whitespace */
		for (i = strlen(line) ; i > 0 && isspace(line[i-1]); i--);
		line[i] = '\0';

        if (!strcasecmp(token, "tr69_enable"))
        {
            read_u32(line, &arris_tr69_conf.enable);
            LOG(LOG_DEBUG, "tr69_enable: %d\n", arris_tr69_conf.enable);
            continue;
        }

        if (!strcasecmp(token, "tr69_provision_code"))
        {
            read_str(line, &arris_tr69_conf.provision_code);
            LOG(LOG_DEBUG, "tr69_provision_code:%s\n", arris_tr69_conf.provision_code);
            continue;
        }

        if (!strcasecmp(token, "tr69_retry_wait_interval"))
        {
            if (read_u32(line, &i))
            {
                read_str(line, &arris_tr69_conf.retry_wait_interval);
                LOG(LOG_DEBUG, "tr69_retry_wait_interval:%s\n", arris_tr69_conf.retry_wait_interval);
            }
            continue;
        }

        if (!strcasecmp(token, "tr69_retry_interval_multi"))
        {
            if (read_u32(line, &i))
            {
                read_str(line, &arris_tr69_conf.retry_interval_multi);
                LOG(LOG_DEBUG, "tr69_retry_interval_multi:%s\n", arris_tr69_conf.retry_interval_multi);
            }
            continue;
        }

        if (!strcasecmp(token, "tr69_acs_url"))
        {
            read_str(line, &arris_tr69_conf.acs_url);
            LOG(LOG_DEBUG, "tr69_acs_url:%s\n", arris_tr69_conf.acs_url);
            continue;
        }
    }
    
    fclose(fp);

    return 0;
}
#endif
/* ARRIS ADD END */

int read_config(char *file)
{
	FILE *fp;
    char buffer[200], orig[200], *token, *line;
    int i, index,classindex = 0,ifid = 0;
    unsigned int len = 0;
    char def[50];
    char * leasefile;
    int prev;
    int fd;
    int flock_status;


    for (index = 0; index < MAX_INTERFACES; index++){
        for(classindex = 0 ; classindex < MAX_CLASSES; classindex++){
            switch(index){
                case 0:
                    sprintf(def,"eth0");
                break;
                case 1:
                    sprintf(def,"eth1");
                break;
                case 2:
                case 3:
                case 4:
                case 5:
                    sprintf(def,"usbrndis");
                break;
            }
            read_str(def, &server_config[index][classindex].interface);
            switch(classindex){
                case 0:
                    sprintf(def,"192.168.%d.20",index);
                break;
                case 1:
                    sprintf(def,"192.168.%d.115",index);
                break;
                case 2:
                    sprintf(def,"192.168.%d.156",index);
                break;
                case 3:
                    sprintf(def,"192.168.%d.197",index);
                break;
            }
            read_ip(def, &server_config[index][classindex].start);
            switch(classindex){
                case 0:
                    sprintf(def,"192.168.%d.114",index);
                break;
                case 1:
                    sprintf(def,"192.168.%d.155",index);
                break;
                case 2:
                    sprintf(def,"192.168.%d.196",index);
                break;
                case 3:
                    sprintf(def,"192.168.%d.254",index);
                break;
            }
            read_ip(def, &server_config[index][classindex].end);

            sprintf(def,"/var/lib/misc/udhcpd%d%d.leases",ifid,classindex);
            read_str(def, &server_config[index][classindex].lease_file);

            sprintf(def,"604800");
            read_u32(def, &server_config[index][classindex].inflease_time);
            
            sprintf(def,"yes");
            read_yn(def, &server_config[index][classindex].remaining);

            sprintf(def,"512");
            read_u32(def, &server_config[index][classindex].max_leases);

            sprintf(def,"1");
            read_u32(def, &server_config[index][classindex].auto_time);

            sprintf(def,"3600");
            read_u32(def, &server_config[index][classindex].decline_time);

            sprintf(def,"3600");
            read_u32(def, &server_config[index][classindex].conflict_time);

            sprintf(def,"60");
            read_u32(def, &server_config[index][classindex].offer_time);

            sprintf(def,"60");
            read_u32(def, &server_config[index][classindex].min_lease);

            sprintf(def,"/var/run/udhcpd.pid");
            read_str(def, &server_config[index][classindex].pidfile);

            sprintf(def,"0.0.0.0");
            read_ip(def, &server_config[index][classindex].siaddr);

            sprintf(def,"/var/lib/misc/udhcpd%d%d.host",ifid,classindex);
            read_str(def, &server_config[index][classindex].host_file);

            // UNIHAN ADD START, to support static lease
            sprintf(def, "/var/lib/misc/udhcpd%d%d.static_leases", ifid, classindex);
            read_str(def, &server_config[index][classindex].static_lease_file);
            // UNIHAN ADD END

            //UNIHAN ADD START
        #ifdef MIB_arrisRouterLanCustom	
            //To support LanCustom
            sprintf(def,"/var/lib/misc/udhcpd%d%d.custom",ifid,classindex);
            read_str(def, &server_config[index][classindex].Custom_file);
        #endif
            //UNIHAN ADD END

            // UNIHAN ADD START, to support CPE aging
            sprintf(def, "0");
            read_u32(def, &server_config[index][classindex].cpe_aging_base);
            server_config[index][classindex].cpe_aging_time = 0;
            // UNIHAN ADD END
            // UNIHAN ADD START, give default value to vendor_option_file for PROD00214575
            sprintf(def,"/var/tmp/vendor_option.125");
            read_str(def, &vendor_option_file);
            // UNIHAN ADD END
        }
    }

	if (!(fp = fopen(file, "r"))) {
		LOG(LOG_ERR, "unable to open config file: %s", file);
		goto exitUpdate;
	}

	/* Lock file for read */
	fd = fileno(fp);
	if (fd == -1)
	{
		LOG(LOG_ERR, "Cannot get fd (from fp), not reading file: %s", file);
		goto closeFile;
	}

	/* Lock, ignore interrupting signals */
	while ((flock_status = flock(fd, LOCK_SH)) == EINTR);
	if (flock_status != 0)
	{
		LOG(LOG_ERR, "Cannot get lock on file: %s, not reading", file);
		goto closeFile;
	}

	index = -1;	
    prev = -1;
    ifid = -1;
    while (fgets(buffer, 200, fp)) {
		if (strchr(buffer, '\n')) *(strchr(buffer, '\n')) = '\0';
        strncpy(orig, buffer, 200);
		if (strchr(buffer, '#')) *(strchr(buffer, '#')) = '\0';
		token = buffer + strspn(buffer, " \t");
		if (*token == '\0') continue;
		line = token + strcspn(token, " \t=");
		if (*line == '\0') continue;
		*line = '\0';
		line++;
		/* eat leading whitespace */
		line = line + strspn(line, " \t=");
		/* eat trailing whitespace */
		for (i = strlen(line) ; i > 0 && isspace(line[i-1]); i--);
		line[i] = '\0';
	
        if (!strcasecmp(token, "lease_file")) {
            sprintf(def,"%s",line);
            leasefile = strrchr(def,'/');
            if(!leasefile)
			{
				LOG(LOG_ERR, "Error entry in %s.\n\tLine: %s. \n\tAbort reading config file.", file, line);
				goto unlockFile;
			}
            len = strlen(leasefile);
            ifid = leasefile[IFID_LOCATION] - '0';
            // ARRIS MOD - all index use 0, distinguish from ip str
            /*if(ifid != prev)
            {
                prev = ifid;
				index++;
            }*/
            index++;
            // END ARRIS MOD
            
            if(len == strlen(LEASEFILE_CLASS))
            {
                classindex = leasefile[CLASSID_LOCATION] - '0';
            }else classindex = 0;
            sprintf(def,"/var/lib/misc/udhcpd%d%d.leases",ifid,classindex);
            if(!read_str(line, &server_config[index][classindex].lease_file))
                read_str(def, &server_config[index][classindex].lease_file);
            continue;
        }
        if (!strcasecmp(token, "start")) {
            switch(classindex){
                case 0:
                    sprintf(def,"192.168.%d.20",index);
                break;
                case 1:
                    sprintf(def,"192.168.%d.115",index);
                break;
                case 2:
                    sprintf(def,"192.168.%d.156",index);
                break;
                case 3:
                    sprintf(def,"192.168.%d.197",index);
                break;
            }
            if(!read_ip(line, &server_config[index][classindex].start))
                read_ip(def, &server_config[index][classindex].start);
            continue;
        }
        if (!strcasecmp(token, "end")) {
            switch(classindex){
                case 0:
                    sprintf(def,"192.168.%d.114",index);
                break;
                case 1:
                    sprintf(def,"192.168.%d.155",index);
                break;
                case 2:
                    sprintf(def,"192.168.%d.196",index);
                break;
                case 3:
                    sprintf(def,"192.168.%d.254",index);
                break;
            }
            if(!read_ip(line, &server_config[index][classindex].end))
                read_ip(def, &server_config[index][classindex].end);
            continue;
        }
        if (!strcasecmp(token, "interface")) {
            switch(classindex){
                case 0:
                    sprintf(def,"eth0");
                break;
                case 1:
                    sprintf(def,"eth1");
                break;
                case 2:
                case 3:
                case 4:
                case 5:
                    sprintf(def,"usbrndis");
                break;
            }
            if(!read_str(line, &server_config[index][classindex].interface))
                read_str(def, &server_config[index][classindex].interface);
			server_config[index][classindex].active=TRUE;
            continue;
        }
        if ((!strcasecmp(token, "opt")) || (!strcasecmp(token, "option"))){
            read_opt(line,&server_config[index][classindex].options,index,classindex);
            continue;
        }
        if (!strcasecmp(token, "inflease_time")) {
            sprintf(def,"604800");
            if(!read_u32(line,&server_config[index][classindex].inflease_time))
                read_u32(def,&server_config[index][classindex].inflease_time);
            continue;
        }
        if (!strcasecmp(token, "max_leases")) {
            sprintf(def,"254");
            if(!read_u32(line,&server_config[index][classindex].max_leases))
                read_u32(def,&server_config[index][classindex].max_leases);
            continue;
        }
        if (!strcasecmp(token, "remaining")) {
            sprintf(def,"yes");
            if(!read_yn(line,&server_config[index][classindex].remaining))
                read_yn(def,&server_config[index][classindex].remaining);
            continue;
        }
        if (!strcasecmp(token, "auto_time")) {
            sprintf(def,"1");
            if(!read_u32(line,&server_config[index][classindex].auto_time))
                read_u32(def,&server_config[index][classindex].auto_time);
            continue;
        }
        if (!strcasecmp(token, "decline_time")) {
            sprintf(def,"3600");
            if(!read_u32(line,&server_config[index][classindex].decline_time))
                read_u32(def,&server_config[index][classindex].decline_time);
            continue;
        }
        if (!strcasecmp(token, "conflict_time")) {
            sprintf(def,"3600");
            if(!read_u32(line,&server_config[index][classindex].conflict_time))
                read_u32(def,&server_config[index][classindex].conflict_time);
            continue;
        }
        if (!strcasecmp(token, "offer_time")) {
            sprintf(def,"60");
            if(!read_u32(line,&server_config[index][classindex].offer_time))
                read_u32(def,&server_config[index][classindex].offer_time);
            continue;
        }
        if (!strcasecmp(token, "min_lease")) {
            sprintf(def,"60");
            if(!read_u32(line,&server_config[index][classindex].min_lease))
                read_u32(def,&server_config[index][classindex].min_lease);
            continue;
        }
        if (!strcasecmp(token, "pidfile")) {
            sprintf(def,"/var/run/udhcpd.pid");
            if(!read_str(line,&server_config[index][classindex].pidfile))
                read_str(def,&server_config[index][classindex].pidfile);
            continue;
        }
        if (!strcasecmp(token, "notify_file")) {
            read_str(line,&server_config[index][classindex].notify_file);
            continue;
        }
        if (!strcasecmp(token, "siaddr")) {
            sprintf(def,"0.0.0.0");
            if(!read_ip(line,&server_config[index][classindex].siaddr))
                read_ip(def,&server_config[index][classindex].siaddr);
            continue;
        }
        if (!strcasecmp(token, "sname")) {
            read_str(line,&server_config[index][classindex].sname);
            continue;
        }
        if (!strcasecmp(token, "boot_file")) {
            read_str(line,&server_config[index][classindex].boot_file);
            continue;
        }
        if (!strcasecmp(token, "host_file")) {
            sprintf(def,"/var/lib/misc/udhcpd%d%d.host",ifid,classindex);
            if(!read_str(line,&server_config[index][classindex].host_file))
                read_str(def,&server_config[index][classindex].host_file);
            continue;
        }
        
        //UNIHAN ADD START
        //To support LanCustom
    #ifdef MIB_arrisRouterLanCustom	
        if (!strcasecmp(token, "Custom_file")) {
            sprintf(def,"/var/lib/misc/udhcpd%d%d.custom",ifid,classindex);
            if(!read_str(line,&server_config[index][classindex].Custom_file))
                read_str(def,&server_config[index][classindex].Custom_file);
            continue;
        }
    #endif
        // UNIHAN ADD END


        // UNIHAN ADD START, to support static lease
        if( 0 == strcasecmp(token, "static_lease_file") )
        {
            sprintf(def, "/var/lib/misc/udhcpd%d%d.static_leases", ifid, classindex);
            if( 0 == read_str(line, &server_config[index][classindex].static_lease_file))
            {
                read_str(def, &server_config[index][classindex].static_lease_file);
            }

            read_staticlease(server_config[index][classindex].static_lease_file, &server_config[index][classindex].static_leases);
            continue;
        }
        // UNIHAN ADD END

        // UNIHAN ADD START, to support CPE aging
        if( 0 == strcasecmp(token, "cpe_aging") )
        {
            sprintf(def, "0");
            if( 0 == read_u32(line, &server_config[index][classindex].cpe_aging_base))
            {
                read_u32(def, &server_config[index][classindex].cpe_aging_base);
            }
            server_config[index][classindex].cpe_aging_time = 0;
            continue;
        }
        // UNIHAN ADD END
        // UNIHAN ADD START, read vendor_option_file value and set it to vendor_option_file for PROD00214575
        if (!strcasecmp(token, "vendor_option_file"))
        {
            sprintf(def,"/var/tmp/vendor_option.125");
            if(!read_str(line,&vendor_option_file))
            {
                read_str(def,&vendor_option_file);
            }
            continue;
        }
        // UNIHAN ADD END
        /* ARRIS ADD START */        
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
        if (!strcasecmp(token, "vendor_option_125_gwsn"))
        {
            read_str(line, &server_config[index][classindex].option125_gwsn);
            LOG(LOG_DEBUG, "vendor_option_125_sn: %s\n", server_config[index][classindex].option125_gwsn);
            continue;
        }

        if (!strcasecmp(token, "vendor_option_125_gwpclass"))
        {
            read_str(line, &server_config[index][classindex].option125_gwpclass);
            LOG(LOG_DEBUG, "vendor_option_125_pclass: %s\n", server_config[index][classindex].option125_gwpclass);
            continue;
        }

        if (!strcasecmp(token, "tr69_runtime_config_file"))
        {
            read_str(line, &arris_tr69_runtime_conf_file);
            LOG(LOG_DEBUG, "arris_tr69_runtime_conf_file:%s\n", arris_tr69_runtime_conf_file);
            continue;
        }
#endif
        /* ARRIS ADD END */
    }
    no_of_ifaces = index + 1 ;

	/* Done reading - Unlock and close */ 
	flock(fd, LOCK_UN);
	fclose(fp);
	return 1;

	/* Error Exit */
	unlockFile:
		flock(fd, LOCK_UN);
	
	closeFile:
		fclose(fp);
	
	exitUpdate:
		return 0;
}

int update_options(char *file)
{
	FILE *fp;
    int fd;
    int flock_status;
    char buffer[200], orig[200], *token, *line;
    int i, j, index,classindex = 0,ifid = 0;
    unsigned int len = 0;
    char def[50];
    char * leasefile;
    int prev;
	struct option_set *option;

	for (i = 0; i < no_of_ifaces; i++)
	{
		for (j = 0; j < MAX_CLASSES; j++)
		{
			/* Re-Initialise server_config options to all zeros */ 
			delete_all_options(&server_config[i][j].options);
		}
	}

	if (!(fp = fopen(file, "r"))) 
	{
		LOG(LOG_ERR, "unable to open config file %s : %s (%d)", file, strerror(errno), errno);
		goto exitUpdate;
	}

	/* Lock file for read */
	fd = fileno(fp);
	if (fd == -1)
	{
		LOG(LOG_ERR, "Cannot get fd (from fp), not reading file: %s", file);
		goto closeFile;
	}

	/* Lock, ignore interrupting signals */
	while ((flock_status = flock(fd, LOCK_SH)) == EINTR);
	if (flock_status != 0)
	{
		LOG(LOG_ERR, "Cannot get lock on file: %s, not reading", file);
		goto closeFile;
	}

	index = -1;	
    prev = -1;
    ifid = -1;

	/* read from file */
	while (fgets(buffer, 200, fp)) 
	{
		if (strchr(buffer, '\n')) *(strchr(buffer, '\n')) = '\0';
        strncpy(orig, buffer, 200);
		if (strchr(buffer, '#')) *(strchr(buffer, '#')) = '\0';
		token = buffer + strspn(buffer, " \t");
		if (*token == '\0') continue;
		line = token + strcspn(token, " \t=");
		if (*line == '\0') continue;
		*line = '\0';
		line++;
		/* eat leading whitespace */
		line = line + strspn(line, " \t=");
		/* eat trailing whitespace */
		for (i = strlen(line) ; i > 0 && isspace(line[i-1]); i--);
		line[i] = '\0';

		if (!strcasecmp(token, "lease_file")) 
		{
            sprintf(def,"%s",line);
            leasefile = strrchr(def,'/');
            if(!leasefile)
			{
				LOG(LOG_ERR, "Error entry in %s. Line: %s. Abort reading config file.", file, line);
				goto unlockFile;
			}
            len = strlen(leasefile);
            ifid = leasefile[IFID_LOCATION] - '0';
            // ARRIS MOD - all index use 0, distinguish from ip str
            /*if(ifid != prev)
            {
                prev = ifid;
				index++;
            }*/
            index++;
            // END ARRIS MOD
            if(len == strlen(LEASEFILE_CLASS))
            {
                classindex = leasefile[CLASSID_LOCATION] - '0';
            } else classindex = 0;
            sprintf(def,"/var/lib/misc/udhcpd%d%d.leases",ifid,classindex);
            if(!read_str(line, &server_config[index][classindex].lease_file))
                read_str(def, &server_config[index][classindex].lease_file);
            continue;
        }

		/* if current line is an option - add it to server_config[][].options db */
		if ((!strcasecmp(token, "opt")) || (!strcasecmp(token, "option")))
		{
			read_opt(line,&server_config[index][classindex].options,index,classindex);
		}
    }

	/* Done reading - Unlock and close */ 
	flock(fd, LOCK_UN);
	fclose(fp);

	/* Update server_config[][].lease param with the lease time calaulated from the lease option*/
	for (i = 0; i < no_of_ifaces; i++)
	{
		for (j = 0; j < MAX_CLASSES; j++)
		{
			if ((option = find_option(server_config[i][j].options, DHCP_LEASE_TIME)))
			{
				memcpy(&server_config[i][j].lease, option->data + 2, 4);
				server_config[i][j].lease = ntohl(server_config[i][j].lease);
			}
			else 
			{
				server_config[i][j].lease = LEASE_TIME;
			}
		}
	}

	return 1;


	/* Error Exit */
	unlockFile:
		flock(fd, LOCK_UN);
	
	closeFile:
		fclose(fp);
	
	exitUpdate:
		return 0;
}

Bool file_updated(char *filename, struct stat *curr_stat)
{
    struct stat updated_stat = {0};

    /* The status of stat is irrelevant, since when it fails, we want to zero the stat*/
    /* struct, and compare against a zero prev                                        */
	if (NULL == filename)
	{
		LOG(LOG_ERR, "Null pointer refference for file name.");
		return False;
	}

	if (NULL == curr_stat)
	{
		LOG(LOG_ERR, "Null pointer refference for current file status.");
		return False;
	}

    stat(filename, &updated_stat);
	if( curr_stat->st_ctime != updated_stat.st_ctime)
    {
		LOG(LOG_DEBUG, "New %s file found.\n", filename);
        /* Update current stat */
        if (curr_stat != NULL)
        {
            *curr_stat = updated_stat;
        }
        return True;
    }

    return False;
}

/* Vendor Specific Option 125 is received from the WAN DHCP Client and is passed in a file to this Server  */
/* The Option contains the whole Enterprise Block for Cable Labs - 4491, starting from 4 bytes code - 4491 */
void read_vendor_options(void)
{
    int i;
    unsigned int len = 0;
    int fd;
    int flock_status;

    u_int8_t vendorBuffer[OPT_MAX_LEN]; 
    u_int8_t *vendorOption125 = vendorBuffer;
    Uint32 enterpriseNumber = OPT_ENTERPRISE_NUMBER;

    char *file = vendor_option_file; 

	if ((fd = open(file, O_RDONLY, 0644)) < 0) 
    {
		LOG(LOG_ERR, "Unable to open vendor option125 file: %s", file);
		goto exitUpdate;
	}

    /* get the length of the file */
    len = lseek(fd, 0, SEEK_END);

    /* max length is 255 bytes */
    if ((len <= 0) || (len >= OPT_MAX_LEN))
    {
		LOG(LOG_ERR, "Invalid length of Vendor Option 125 file: %s, not reading", file);
		goto closeFile;
    }

	/* Lock, ignore interrupting signals */
	while ((flock_status = flock(fd, LOCK_SH)) == EINTR);
	if (flock_status != 0)
	{
		LOG(LOG_ERR, "Cannot get lock on file: %s, not reading", file);
		goto closeFile;
	}

    /* set to the beginning of file */
    lseek(fd, 0, SEEK_SET);  

    /* ARRIS ADD START */
    if (vendor125_OptionInfo)
    {
        free(vendor125_OptionInfo);
    }
    /* ARRIS ADD END */
    vendor125_OptionInfo = malloc(len + 2);

	read(fd, (void *)vendorBuffer, len);

	/* Done reading - Unlock and close */ 
	flock(fd, LOCK_UN);
	close(fd);

    /* look for suboption 3 - eRouterContainer in the read buffer */
    /* vendorOption125 points to the enterprise code 4491 */
    if (memcmp(vendorOption125, &enterpriseNumber, sizeof(enterpriseNumber)) != 0)
        return 0;

    /* point to the length of enterprise block */
    vendorOption125 += OPT_ENTERPRISE_NUMBER_LEN; 

    /* get length of enterprise block of sub-options */
    int enterpriseBlockLen = *vendorOption125;

    /* point to the start of the enterprise block */
    vendorOption125 += 1;

    /* go through the block and look for the requested sub-option */
    /* 2 = suboption type + suboption length bytes */
    for (i=0; i < (enterpriseBlockLen - (OPT_SUBOPTION_CODE + OPT_SUBOPTION_LEN)); i += *(vendorOption125 + OPT_SUBOPTION_LEN + i) + (OPT_SUBOPTION_CODE + OPT_SUBOPTION_LEN))
    {
      /* search for erouter Container sub-option - opcode = 3 inside enterprise block */
      switch (*(vendorOption125 + i))
      {
      case CL_V4EROUTER_CONTAINER_OPTION:

          /* build in buffP the complete Vendor Specific Option 125 with just one Suboption - 3 eRouter Container */
          /* length includes: suboption length plus suboption code byte and suboption length byte */
          memcpy(&vendor125_OptionInfo[7], (vendorOption125 + i), *(vendorOption125 + i + OPT_SUBOPTION_CODE) + OPT_SUBOPTION_CODE + OPT_SUBOPTION_LEN ); 
          vendor125_OptionInfo[6] = vendor125_OptionInfo[8] + OPT_SUBOPTION_CODE + OPT_SUBOPTION_LEN;                             /* enterprise block lenght */

          memcpy(&vendor125_OptionInfo[2], &enterpriseNumber, sizeof(enterpriseNumber));     /* enterprise code - 4 bytes */
          vendor125_OptionInfo[1] = vendor125_OptionInfo[6] + OPT_ENTERPRISE_NUMBER_LEN + OPT_SUBOPTION_LEN;                             /* option length */
          vendor125_OptionInfo[0] = OPT_VENDOR_SPECIFIC;                                     /* option code */

          return 1;

       } /* switch */
     }  /* for */
   
    return 0;

	/* Error Exit */
	unlockFile:
		flock(fd, LOCK_UN);
	
	closeFile:
		close(fd);
	
	exitUpdate:
		return 0;

}

/* the dummy var is here so this can be a signal handler */
void write_leases(int ifid, int classindex)
{
	FILE *fp;
	unsigned int i;
	char buf[255];
	time_t curr = time(0);
	unsigned long lease_time;
  int j;
    //UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    unsigned char line[ARRIS_ROUTER_LAN_DHCP_LEASE_OPT125_MAX_SIZE];
#else
    unsigned char line[80];
#endif
  //END UNIHAN MODIFY
  struct in_addr in;
	
	if (!(fp = fopen(server_config[ifid][classindex].lease_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", server_config[ifid][classindex].lease_file);
		return;
	}

    flock(fileno(fp), LOCK_EX);
	
	for (i = 0; i < server_config[ifid][classindex].max_leases; i++) {
		if (server_config[ifid][classindex].leases[i].yiaddr != 0) {
			if (server_config[ifid][classindex].remaining) {
				if (lease_expired(&(server_config[ifid][classindex].leases[i]),ifid,classindex))
				{
					lease_time = 0;
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
                    // Delete doesn't need opt125
                    if( server_config[ifid][classindex].leases[i].expires != 0)
                    {
                        write_to_delta(server_config[ifid][classindex].leases[i].chaddr,
                                       server_config[ifid][classindex].leases[i].yiaddr,
                                       server_config[ifid][classindex].leases[i].hostname,
                                       0,
                                       LEASE_DEL,
                                       classindex,
                                       NULL,
                                       0,
                                       NULL,
                                       NULL);
                    }
#else
					if( server_config[ifid][classindex].leases[i].expires != 0)
						write_to_delta(server_config[ifid][classindex].leases[i].chaddr,
								server_config[ifid][classindex].leases[i].yiaddr,
								server_config[ifid][classindex].leases[i].hostname,
								0,LEASE_DEL,classindex, NULL, 0, NULL);
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
					server_config[ifid][classindex].leases[i].expires = 0;
				}
				else
				{
					if( server_config[ifid][classindex].leases[i].expires != server_config[ifid][classindex].inflease_time)
        		lease_time = server_config[ifid][classindex].leases[i].expires - curr;
					else
						lease_time = server_config[ifid][classindex].inflease_time;
				}
			}
			else
				lease_time = server_config[ifid][classindex].leases[i].expires;

			j = sprintf(line,"%02x:%02x:%02x:%02x:%02x:%02x ",server_config[ifid][classindex].leases[i].chaddr[0],
									server_config[ifid][classindex].leases[i].chaddr[1],
									server_config[ifid][classindex].leases[i].chaddr[2],
									server_config[ifid][classindex].leases[i].chaddr[3],
									server_config[ifid][classindex].leases[i].chaddr[4],
									server_config[ifid][classindex].leases[i].chaddr[5]);

			in.s_addr = server_config[ifid][classindex].leases[i].yiaddr;
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)   
            j = snprintf(line + j, sizeof(line), "%s %ld %s\n",
                        inet_ntoa(in),
                        lease_time,
                        server_config[ifid][classindex].leases[i].hostname); 
#else
            //ARRIS MOD START
			j = sprintf(line + j,"%s %ld %s\n",inet_ntoa(in),lease_time,server_config[ifid][classindex].leases[i].hostname);
            //ARRIS MOD END
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
      fwrite( line, strlen(line), 1, fp);
		}
	}
	fclose(fp);
	
	if (server_config[ifid][classindex].notify_file) {
		sprintf(buf, "%s %s", server_config[ifid][classindex].notify_file, server_config[ifid][classindex].lease_file);
		system(buf);
	}
}


void read_hosts(char *file, int ifid,int classindex)
{
	FILE *fp;
//UNIHAN MODIFY
	int ncols;
//END UNIHAN MODIFY
	struct dhcpOfferedAddr lease;
	
  char ipaddress[17],macaddr[18];
  char hname[50];
  unsigned int leasetime;
  struct in_addr ipaddr;
    //ARRIS ADD START
    unsigned int adptype;
    char buf[512];
    char tmpfile[] = "/var/tmp/dhcp_hosts_temp";
    FILE *fp1;
    //ARRIS ADD END

  /* Added as a part of Alignment problem fixes for ARM */
  int j = 0;
  char mac_str[18] __attribute__ ((aligned));
  char* mac_byte __attribute__ ((aligned));
  u_int8_t mac_hex __attribute__ ((aligned));
  char *endptr;

//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    struct dhcp_option125_info opt125Info;
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

/*  mac ipaddress leasetime hostname */
//ARRIS MOD START
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
#define	readentry(fp) \
	fscanf((fp), "%s %s %u %s %u %s %s %s\n", \
		 macaddr, ipaddress, &leasetime, hname, &adptype, opt125Info.deviceManufacturerOUI, opt125Info.deviceSerialNumber, opt125Info.deviceProductClass)
#else
#define	readentry(fp) \
	fscanf((fp), "%s %s %u %s %u\n", \
		 macaddr, ipaddress, &leasetime, hname, &adptype)
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY

    //we have to copy hostfile to a temp file first
    //because it is possible that the hostfile will be updated in the scope of this function
    //exactly, function add_lease() might change hostfile and function add_lease() is called
    //by oursleves for a couple of times.
    fp1 = fopen(file, "r");
    if (!fp1)
    {
        return;
    }
    fp = fopen(tmpfile, "w+");
    if (!fp)
    {
        printf("dhcp error: failed to create temp host file %s, %s\n", tmpfile, strerror(errno));
        fclose(fp1);
        return;
    }
//UNIHAN MODIFY
    while ((ncols = fread(buf, 1, sizeof(buf), fp1)))
    {
        fwrite(buf, 1, ncols, fp);
    }
//END UNIHAN MODIFY

    //close hostfile for update;
    fclose(fp1);
    fseek(fp, 0, SEEK_SET);
//ARRIS MOD END
	/*if (!(fp = fopen(file, "r"))) {
//		LOG(LOG_ERR, "Unable to open %s for reading", file); ARRIS remove unneeded print
		return;
	}*/

	for (;;)
    {

        #if 0
        if ((ncols = readentry(fp)) == EOF)//UNIHAN MODIFY
            break;
        #endif
        //each line might include 4 or 5 items, so we cannot use fscanf directly
        if (!fgets(buf, sizeof(buf), fp))
            break;

//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
        ncols = sscanf(buf, "%s %s %u %s %u %s %s %s\n", macaddr, ipaddress, &leasetime, hname, &adptype, opt125Info.deviceManufacturerOUI, opt125Info.deviceSerialNumber, opt125Info.deviceProductClass);
#else
        ncols = sscanf(buf, "%s %s %u %s %u\n", macaddr, ipaddress, &leasetime, hname, &adptype);
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
        if (!ncols)//UNIHAN MODIFY
            continue;
        //ARRIS MOD END
        
		LOG(LOG_ERR, "Unable to open %s for reading", file);
		if (!inet_aton(ipaddress, &ipaddr))
			continue;
    lease.yiaddr = ipaddr.s_addr;
    memset(lease.chaddr,0x00,16);

    /* Commented out as this was causing an alignment problems on ARM.
       All the array elements lease.chaddr[x] neednt be word aligned 
       and thus when type-casted to int will lead to Bus error on ARM
       as ARM requires word alignment on int.
    */
    /*
    sscanf(macaddr,"%x:%x:%x:%x:%x:%x", 
				(int *)&lease.chaddr[0],
				(int *)&lease.chaddr[1],
				(int *)&lease.chaddr[2],
				(int *)&lease.chaddr[3],
				(int *)&lease.chaddr[4],
				(int *)&lease.chaddr[5]);
    */
    /* Fix for ARM Alignment problem. Since not all elements of the array
       lease.chaddr are going to be word aligned, typecasting as shown above
       is not possible. So transfer byte-by-byte of the MAC address to the lease.chaddr
       array in hex int format.
    */
    j = 0;
    mac_byte = strtok(macaddr, ":");
    while(mac_byte != NULL)
    {
        /* mac_byte need not be byte aligned, so copy to a buffer which is aligned first */
        strcpy(mac_str, mac_byte);
        /* convert the word aligned string buffer to hex int */
        mac_hex = strtoul(mac_str, &endptr, 16);
        /* finally, copy the hex int to dest array */
        memcpy(&lease.chaddr[j], &mac_hex, sizeof(u_int8_t));
        /* Repeat the same for the next byte of MAC address till done */
        mac_byte = strtok(NULL, ":");
        j++;
    }

    lease.expires = leasetime;
    strcpy(lease.hostname,hname);
//UNIHAN ADD
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
    if (ncols < DHCP_HOSTFILE_PARAMETERCOUNT_INCLUDE_DEVICEID)
    {
        /* Doesn't include device manufacturer OUI, device serial number and device product class. */
        memset(&opt125Info, 0, sizeof(opt125Info));
    }
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD
		if (lease.yiaddr >= server_config[ifid][classindex].start && lease.yiaddr <= server_config[ifid][classindex].end)
		{
			if (!server_config[ifid][classindex].remaining) lease.expires -= time(0);
//UNIHAN MODIFY
#if defined(INCLUDE_ARRIS_GW_TR69) && !defined(CONFIG_VENDOR_ARRIS_CM_TR69)
			if (!(add_lease(lease.chaddr, lease.yiaddr, lease.expires, ifid, classindex,lease.hostname, 0, &opt125Info)))
#else
			if (!(add_lease(lease.chaddr, lease.yiaddr, lease.expires, ifid, classindex,lease.hostname))) //ARRIS MOD
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN MODIFY
            {
				LOG(LOG_WARNING, "Too many leases while loading %s\n", file);
				break;
			}
        }				
	}
	fclose(fp);
    unlink(tmpfile);//ARRIS ADD
}
